#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Run after tools/author_heli_paint_channels.py or after adding a skin.
//! Checks every paint channel: its materials and prefabs are registered under
//! the names IA_HeliPaintChannels builds, a channel airframe inherits its stock
//! airframe, names only its own channel's materials on slots its meshes have,
//! overrides seats without adding slots and spawns with them. Checks every
//! skin: its colour materials are registered and inherit the surface they
//! colour, and every colour the stock material sets can be read both from it
//! and through the skin, so a skin can be shown and stock paint put back.
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter skin asset check", wbModules: {"ResourceManager"})]
class IA_HeliSkinAssetCheck : WorkbenchPlugin
{
	protected int m_iFailures;
	// Containers read from a prefab are only valid while its Resource is held.
	protected ref array<ref Resource> m_aLoaded = {};

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAHeliSkinAssetCheck");
		BaseWorld world = preview.GetRef();

		int channel;
		int surface;
		for (surface = 0; surface < IA_HeliPaintChannels.SURFACE_COUNT; surface++)
		{
			CheckStockMaterial(IA_HeliPaintChannels.GetStockMaterial(surface));
			for (channel = 1; channel <= IA_HeliPaintChannels.CHANNEL_COUNT; channel++)
			{
				CheckInherits("channel " + channel.ToString(), IA_HeliPaintChannels.GetMaterial(surface, channel), IA_HeliPaintChannels.GetStockMaterial(surface));
			}
		}

		array<ResourceName> airframes = IA_HeliPaintChannels.GetStockPrefabs();
		Check(!airframes.IsEmpty(), "at least one airframe has paint channels");
		foreach (ResourceName stock : airframes)
		{
			for (channel = 1; channel <= IA_HeliPaintChannels.CHANNEL_COUNT; channel++)
			{
				CheckAirframe(stock, channel, world);
			}
		}

		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		Check(!defs.IsEmpty(), "the catalogue lists at least one skin");
		int paints;
		ResourceName paint;
		foreach (IA_HeliSkinDef def : defs)
		{
			paints = 0;
			for (surface = 0; surface < IA_HeliPaintChannels.SURFACE_COUNT; surface++)
			{
				paint = def.GetPaint(surface);
				if (paint.IsEmpty())
					continue;

				paints = paints + 1;
				// Inheriting the stock material is what makes a parameter the skin leaves out read as stock.
				CheckInherits(def.m_sKey, paint, IA_HeliPaintChannels.GetStockMaterial(surface));
				CheckColours(def.m_sKey, paint, IA_HeliPaintChannels.GetStockMaterial(surface));
			}
			Check(paints > 0, def.m_sKey + " colours at least one surface");
			Check(IA_HeliSkinPaint.Apply(1, def.m_iId) == IA_HeliPaintChannels.SURFACE_COUNT, def.m_sKey + " paints every surface of a channel");
		}
		Check(IA_HeliSkinPaint.Apply(1, IA_HeliSkinCatalog.SKIN_NONE) == IA_HeliPaintChannels.SURFACE_COUNT, "stock paint is put back on every surface");

		if (m_iFailures == 0)
			Print("[IA][HeliSkinAssetCheck] PASS", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	//! Stock paint is restored by setting the stock material's colours back, so they must be readable.
	protected void CheckStockMaterial(ResourceName stock)
	{
		string label = stock.GetPath();
		Check(Workbench.GetResourceName(label) == stock, label + " is the registered stock material");

		ref Resource resource = Resource.Load(stock);
		if (!resource || !resource.IsValid())
		{
			Check(false, label + " loads");
			return;
		}

		m_aLoaded.Insert(resource);
		float color[4];
		Check(IA_HeliSkinPaint.ReadColor(resource.GetResource().ToBaseContainer(), "Color_1", color), label + " gives its first colour");
		Check(color[0] > 0 && color[0] < 1 && color[3] == 1, label + " gives its first colour as numbers");
	}

	//------------------------------------------------------------------------------------------------
	//! A colour the stock material sets and the skin's material cannot give would be reset to white.
	protected void CheckColours(string label, ResourceName paint, ResourceName stock)
	{
		ref Resource paintResource = Resource.Load(paint);
		ref Resource stockResource = Resource.Load(stock);
		if (!paintResource || !paintResource.IsValid() || !stockResource || !stockResource.IsValid())
			return;

		BaseContainer paintSource = paintResource.GetResource().ToBaseContainer();
		BaseContainer stockSource = stockResource.GetResource().ToBaseContainer();
		float color[4];
		foreach (string colorName : IA_HeliSkinPaint.GetColorParams())
		{
			if (IA_HeliSkinPaint.ReadColor(stockSource, colorName, color))
				Check(IA_HeliSkinPaint.ReadColor(paintSource, colorName, color), label + ": " + paint.GetPath() + " gives " + colorName);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInherits(string label, ResourceName material, ResourceName stock)
	{
		BaseContainer source = LoadRegistered(label, material);
		if (!source)
			return;

		BaseContainer ancestor = source.GetAncestor();
		Check(ancestor && ancestor.GetResourceName().GetPath() == stock.GetPath(), label + ": " + material.GetPath() + " inherits " + stock.GetPath());
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAirframe(ResourceName stock, int channel, BaseWorld world)
	{
		ResourceName prefab = IA_HeliPaintChannels.FindChannelPrefab(stock, channel);
		string label = prefab.GetPath();
		Check(!prefab.IsEmpty(), stock.GetPath() + " has a twin on channel " + channel.ToString());
		Check(Workbench.GetResourceName(stock.GetPath()) == stock, label + " is paired with a registered stock airframe");
		if (prefab.IsEmpty())
			return;

		BaseContainer source = LoadRegistered(label, prefab);
		if (!source)
			return;

		BaseContainer ancestor = source.GetAncestor();
		Check(ancestor && ancestor.GetResourceName().GetPath() == stock.GetPath(), label + " inherits " + stock.GetPath());
		Check(CheckPaint(label, source, channel) == IA_HeliPaintChannels.SURFACE_COUNT, label + " names its channel's material on every surface");

		ref array<ResourceName> parts = {};
		int slotCount = OwnParts(source, parts);
		if (ancestor)
			Check(slotCount == OwnParts(ancestor, null), label + " overrides slots without adding any");
		Check(!parts.IsEmpty(), label + " carries its own seats");

		BaseContainer partSource;
		foreach (ResourceName part : parts)
		{
			partSource = LoadRegistered(label, part);
			if (partSource)
				Check(CheckPaint(part.GetPath(), partSource, channel) > 0, part.GetPath() + " names its channel's materials");
		}

		CheckSpawn(label, prefab, parts, world);
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
	//! Checks the materials a prefab's mesh takes from this addon: all must belong to one paint channel.
	//! \return how many it assigns
	protected int CheckPaint(string label, notnull BaseContainer source, int channel)
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
				Check(MeshHasSlot(mesh, slot), label + ": slot " + slot + " exists on " + mesh.GetPath());
				Check(IsChannelMaterial(assigned, channel), label + ": " + assigned.GetPath() + " is a material of channel " + channel.ToString());
			}
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] parts prefabs from this addon slotted into the source; may be null
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
	protected bool IsOwnAsset(ResourceName name)
	{
		return name.GetPath().Contains("/IA_");
	}

	//------------------------------------------------------------------------------------------------
	//! A mesh naming another channel's material would be recoloured with that channel's helicopter.
	protected bool IsChannelMaterial(ResourceName name, int channel)
	{
		int surface;
		for (surface = 0; surface < IA_HeliPaintChannels.SURFACE_COUNT; surface++)
		{
			if (IA_HeliPaintChannels.GetMaterial(surface, channel) == name)
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
