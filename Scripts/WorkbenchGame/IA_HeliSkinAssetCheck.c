#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Run after tools/author_heli_paint_channels.py, after adding a helicopter
//! family or after adding a livery. Checks every family of
//! IA_HeliPaintManifest: its stock materials are registered and hold the stock
//! values the manifest says they hold, every channel material is registered
//! and inherits its stock material, a channel airframe inherits its stock
//! airframe, names only its own channel's materials on slots its meshes have,
//! overrides parts without adding slots and spawns with them. Checks every
//! livery paints every surface of every family and that stock paint goes back.
//!   -iaSkinFamily <key>  check one family only
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter skin asset check", wbModules: {"ResourceManager"})]
class IA_HeliSkinAssetCheck : WorkbenchPlugin
{
	// The manifest holds three decimals.
	protected static const float TOLERANCE = 0.002;

	protected int m_iFailures;
	// Containers read from a prefab are only valid while its Resource is held.
	protected ref array<ref Resource> m_aLoaded = {};
	// The channel materials some twin of the family being checked names.
	protected ref array<ResourceName> m_aShown = {};

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAHeliSkinAssetCheck");
		BaseWorld world = preview.GetRef();

		string only;
		System.GetCLIParam("iaSkinFamily", only);

		array<ref IA_HeliPaintFamily> families = IA_HeliPaintChannels.GetFamilies();
		Check(!families.IsEmpty(), "at least one helicopter family has paint channels");
		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		Check(!defs.IsEmpty(), "the catalogue lists at least one livery");

		int checked;
		foreach (IA_HeliPaintFamily family : families)
		{
			if (!only.IsEmpty() && family.m_sKey != only)
				continue;

			checked = checked + 1;
			CheckFamily(family, defs, world);
		}
		Check(checked > 0, "a family was checked");

		if (m_iFailures == 0)
			Print(string.Format("[IA][HeliSkinAssetCheck] PASS families=%1 liveries=%2", checked, defs.Count()), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFamily(notnull IA_HeliPaintFamily family, notnull array<ref IA_HeliSkinDef> defs, BaseWorld world)
	{
		string label = family.m_sKey;
		int surfaceCount = family.m_aSurfaces.Count();
		Check(surfaceCount > 0, label + " has a paint surface");
		Check(!family.m_aStockPrefabs.IsEmpty(), label + " has an airframe");
		Check(!family.m_sDisplayName.IsEmpty() && !family.m_sStockPaint.IsEmpty(), label + " is named for the paint bay");

		int local;
		int primaries;
		foreach (IA_HeliPaintSurface surface : family.m_aSurfaces)
		{
			primaries = primaries + CheckStockMaterial(label, surface);
			for (local = 1; local <= IA_HeliPaintChannels.CHANNEL_COUNT; local++)
			{
				CheckInherits(label + " channel " + local.ToString(), surface.GetMaterial(local), surface.m_sStockMaterial);
			}
		}
		Check(primaries > 0, label + " has a layer that takes the livery colour");

		m_aShown.Clear();
		int airframeCount = family.m_aStockPrefabs.Count();
		int airframe;
		for (airframe = 0; airframe < airframeCount; airframe++)
		{
			for (local = 1; local <= IA_HeliPaintChannels.CHANNEL_COUNT; local++)
			{
				CheckAirframe(family, family.m_aStockPrefabs[airframe], local, world);
			}
		}

		// Airframes of a family differ (one mesh for the ambulance, pylons on the gunship), so a
		// surface need not be on each of them; one that no twin names would be painted and never seen.
		foreach (IA_HeliPaintSurface shown : family.m_aSurfaces)
		{
			for (local = 1; local <= IA_HeliPaintChannels.CHANNEL_COUNT; local++)
			{
				Check(m_aShown.Contains(shown.GetMaterial(local)), label + " channel " + local.ToString() + " shows " + shown.m_sStockMaterial.GetPath());
			}
		}

		int channel = IA_HeliPaintChannels.ToChannel(family, 1);
		foreach (IA_HeliSkinDef def : defs)
		{
			Check(IA_HeliSkinPaint.Apply(channel, def.m_iId) == surfaceCount, label + ": " + def.m_sKey + " paints every surface of a channel");
		}
		Check(IA_HeliSkinPaint.Apply(channel, IA_HeliSkinCatalog.SKIN_NONE) == surfaceCount, label + ": stock paint is put back on every surface");
	}

	//------------------------------------------------------------------------------------------------
	//! Stock paint is put back from the manifest's values, so they must be what the stock file holds.
	//! \return how many of the surface's parameters take the livery colour
	protected int CheckStockMaterial(string label, notnull IA_HeliPaintSurface surface)
	{
		ResourceName stock = surface.m_sStockMaterial;
		label = label + ": " + stock.GetPath();
		Check(Workbench.GetResourceName(stock.GetPath()) == stock, label + " is the registered stock material");

		ref Resource resource = Resource.Load(stock);
		if (!resource || !resource.IsValid())
		{
			Check(false, label + " loads");
			return 0;
		}

		m_aLoaded.Insert(resource);
		BaseContainer source = resource.GetResource().ToBaseContainer();
		Check(!surface.m_aParams.IsEmpty(), label + " has parameters to paint");

		int primaries;
		BaseContainer holder;
		string text;
		float number;
		vector colour;
		foreach (IA_HeliPaintParam param : surface.m_aParams)
		{
			if (param.m_iKind == IA_HeliPaintParam.KIND_PRIMARY || param.m_iKind == IA_HeliPaintParam.KIND_TINT)
				primaries = primaries + 1;

			Check(source.GetVarIndex(param.m_sParam) >= 0, label + " has a parameter " + param.m_sParam);
			holder = FindSetter(source, param.m_sParam);
			if (!param.m_bStockSet)
			{
				Check(holder == null, label + " leaves " + param.m_sParam + " unset, as the manifest says");
				continue;
			}
			if (!holder)
			{
				Check(false, label + " sets " + param.m_sParam + ", as the manifest says");
				continue;
			}

			if (param.m_iKind == IA_HeliPaintParam.KIND_SCALAR)
			{
				holder.Get(param.m_sParam, number);
				Check(Math.AbsFloat(number - param.m_fStock) <= TOLERANCE, string.Format("%1: %2 is %3 in the file and %4 in the manifest", label, param.m_sParam, number, param.m_fStock));
				continue;
			}

			// A colour reads only as text, "r g b a".
			holder.Get(param.m_sParam, text);
			if (!ParseColour(text, colour))
			{
				Check(false, label + ": " + param.m_sParam + " reads as a colour: " + text);
				continue;
			}
			Check(vector.Distance(colour, param.m_vStock) <= TOLERANCE * 2, string.Format("%1: %2 is %3 in the file and %4 in the manifest", label, param.m_sParam, text, param.m_vStock));
		}
		return primaries;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the file in the material's inheritance chain that sets a parameter, null when none does
	protected BaseContainer FindSetter(BaseContainer source, string param)
	{
		BaseContainer container = source;
		while (container)
		{
			if (container.IsVariableSetDirectly(param))
				return container;
			container = container.GetAncestor();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ParseColour(string text, out vector colour)
	{
		ref array<string> parts = {};
		text.Split(" ", parts, true);
		if (parts.Count() < 3)
			return false;

		colour = Vector(parts[0].ToFloat(), parts[1].ToFloat(), parts[2].ToFloat());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInherits(string label, ResourceName material, ResourceName stock)
	{
		Check(!material.IsEmpty(), label + " has a material for " + stock.GetPath());
		if (material.IsEmpty())
			return;

		BaseContainer source = LoadRegistered(label, material);
		if (!source)
			return;

		// Inheriting the stock material is what makes a channel look stock until a livery is set.
		BaseContainer ancestor = source.GetAncestor();
		Check(ancestor && ancestor.GetResourceName().GetPath() == stock.GetPath(), label + ": " + material.GetPath() + " inherits " + stock.GetPath());
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAirframe(notnull IA_HeliPaintFamily family, ResourceName stock, int local, BaseWorld world)
	{
		int channel = IA_HeliPaintChannels.ToChannel(family, local);
		ResourceName prefab = IA_HeliPaintChannels.FindChannelPrefab(stock, channel);
		string label = prefab.GetPath();
		Check(!prefab.IsEmpty(), stock.GetPath() + " has a twin on channel " + local.ToString());
		Check(Workbench.GetResourceName(stock.GetPath()) == stock, stock.GetPath() + " is a registered stock airframe");
		if (prefab.IsEmpty())
			return;

		Check(IA_HeliPaintChannels.FindChannel(prefab) == channel, label + " is recognised as its channel");

		BaseContainer source = LoadRegistered(label, prefab);
		if (!source)
			return;

		BaseContainer ancestor = source.GetAncestor();
		Check(ancestor && ancestor.GetResourceName().GetPath() == stock.GetPath(), label + " inherits " + stock.GetPath());
		Check(HasRig(source), label + " carries the paint rig component");

		ref array<ResourceName> named = {};
		Check(CheckPaint(label, source, channel, named) > 0, label + " names its channel's materials");

		ref array<ResourceName> parts = {};
		int slotCount = OwnParts(source, parts);
		if (ancestor)
			Check(slotCount == OwnParts(ancestor, null), label + " overrides slots without adding any");

		BaseContainer partSource;
		foreach (ResourceName part : parts)
		{
			partSource = LoadRegistered(label, part);
			if (partSource)
				Check(CheckPaint(part.GetPath(), partSource, channel, named) > 0, part.GetPath() + " names its channel's materials");
		}

		foreach (ResourceName shown : named)
		{
			m_aShown.Insert(shown);
		}

		CheckSpawn(label, prefab, parts, world);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasRig(notnull BaseContainer source)
	{
		BaseContainerList components = source.GetObjectArray("components");
		if (!components)
			return false;

		int componentCount = components.Count();
		int c;
		for (c = 0; c < componentCount; c++)
		{
			if (components.Get(c).GetClassName() == "IA_HeliPaintRigComponent")
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A file Workbench never imported has no GUID and is left out of a published build.
	//! \return the prefab's source, null when it does not load
	protected BaseContainer LoadRegistered(string label, ResourceName name)
	{
		ResourceName registered = Registered(name);
		Check(registered.StartsWith("{"), label + ": " + name.GetPath() + " is a registered resource");
		Check(registered == name, label + " must reference " + registered);

		ref Resource resource = Resource.Load(registered);
		if (!resource || !resource.IsValid())
		{
			Check(false, label + ": " + name.GetPath() + " loads");
			return null;
		}

		m_aLoaded.Insert(resource);
		return resource.GetResource().ToBaseContainer();
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName Registered(ResourceName name)
	{
		string path = name.GetPath();
		ResourceName registered = Workbench.GetResourceName(path);
		if (registered.StartsWith("{"))
			return registered;

		registered = Register(path);
		if (!registered.IsEmpty())
			Print("[IA][HeliSkinAssetCheck] Registered " + registered, LogLevel.NORMAL);
		return registered;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName Register(string path)
	{
		string absPath;
		if (!Workbench.GetAbsolutePath(path, absPath, true))
			return ResourceName.Empty;

		ResourceManager manager = Workbench.GetModule(ResourceManager);
		if (!manager || !manager.RegisterResourceFile(absPath, false))
			return ResourceName.Empty;

		MetaFile meta = manager.GetMetaFile(absPath);
		if (!meta)
			return ResourceName.Empty;
		return meta.GetResourceID();
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the paint materials a prefab's mesh names: all must belong to one paint channel.
	//! \param[out] named the materials it names are added
	//! \return how many it assigns
	protected int CheckPaint(string label, notnull BaseContainer source, int channel, notnull array<ResourceName> named)
	{
		BaseContainerList components = source.GetObjectArray("components");
		if (!components)
			return 0;

		int painted;
		ResourceName mesh;
		ResourceName assigned;
		string slot;
		BaseContainer component;
		BaseContainer material;
		BaseContainerList materials;
		int materialCount;
		int m;
		int componentCount = components.Count();
		int c;
		for (c = 0; c < componentCount; c++)
		{
			component = components.Get(c);
			if (component.GetClassName() != "MeshObject")
				continue;

			component.Get("Object", mesh);
			materials = component.GetObjectArray("Materials");
			if (!materials)
				continue;

			materialCount = materials.Count();
			for (m = 0; m < materialCount; m++)
			{
				material = materials.Get(m);
				material.Get("SourceMaterial", slot);
				material.Get("AssignedMaterial", assigned);
				if (!IsOwnAsset(assigned))
					continue;

				painted = painted + 1;
				named.Insert(assigned);
				Check(MeshHasSlot(mesh, slot), label + ": slot " + slot + " exists on " + mesh.GetPath());
				Check(IsChannelMaterial(assigned, channel), label + ": " + assigned.GetPath() + " is a material of its channel");
			}
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] parts paint channel prefabs slotted into the source; may be null
	//! \return number of slots on the source
	protected int OwnParts(notnull BaseContainer source, array<ResourceName> parts)
	{
		BaseContainerList components = source.GetObjectArray("components");
		if (!components)
			return 0;

		int total;
		ResourceName part;
		BaseContainer component;
		BaseContainerList slots;
		int slotCount;
		int s;
		int componentCount = components.Count();
		int c;
		for (c = 0; c < componentCount; c++)
		{
			component = components.Get(c);
			if (component.GetClassName() != "SlotManagerComponent")
				continue;

			slots = component.GetObjectArray("Slots");
			if (!slots)
				continue;

			slotCount = slots.Count();
			total = total + slotCount;
			for (s = 0; s < slotCount; s++)
			{
				part = ResourceName.Empty;
				slots.Get(s).Get("Prefab", part);
				if (parts && IsOwnAsset(part))
					parts.Insert(part);
			}
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab source can be right and the slot override still not take; spawn it to see.
	protected void CheckSpawn(string label, ResourceName prefab, notnull array<ResourceName> parts, BaseWorld world)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params);
		Check(entity != null, label + " spawns");
		if (!entity)
			return;

		foreach (ResourceName part : parts)
		{
			Check(HasPart(entity, part.GetPath(), 0), label + " spawns with " + part.GetPath());
		}
		// Not SCR_EntityHelper: its replicated delete crashes on a vehicle without a running session.
		delete entity;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPart(IEntity ent, string path, int depth)
	{
		if (!ent || depth > 4)
			return false;

		EntityPrefabData prefab = ent.GetPrefabData();
		if (prefab && prefab.GetPrefabName().GetPath() == path)
			return true;

		IEntity child = ent.GetChildren();
		while (child)
		{
			if (HasPart(child, path, depth + 1))
				return true;
			child = child.GetSibling();
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! What tools/author_heli_paint_channels.py writes: IA_<name>_Paint<n>.
	protected bool IsOwnAsset(ResourceName name)
	{
		string path = name.GetPath();
		return path.Contains("/IA_") && path.Contains("_Paint");
	}

	//------------------------------------------------------------------------------------------------
	//! A mesh naming another channel's material would be recoloured with that channel's helicopter.
	protected bool IsChannelMaterial(ResourceName name, int channel)
	{
		IA_HeliPaintFamily family = IA_HeliPaintChannels.GetFamily(channel);
		if (!family)
			return false;

		int local = IA_HeliPaintChannels.GetLocalChannel(channel);
		foreach (IA_HeliPaintSurface surface : family.m_aSurfaces)
		{
			if (surface.GetMaterial(local) == name)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A material assigned to a slot the mesh does not have paints nothing.
	protected bool MeshHasSlot(ResourceName mesh, string slot)
	{
		Resource resource = Resource.Load(mesh);
		if (!resource || !resource.IsValid())
			return false;

		VObject vobj = resource.GetResource().ToVObject();
		if (!vobj)
			return false;

		string materials[256];
		int count = vobj.GetMaterials(materials);
		int i;
		for (i = 0; i < count; i++)
		{
			if (materials[i] == slot)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(bool condition, string message)
	{
		if (condition)
			return;

		m_iFailures = m_iFailures + 1;
		Print("[IA][HeliSkinAssetCheck] FAIL: " + message, LogLevel.ERROR);
	}
}
#endif
