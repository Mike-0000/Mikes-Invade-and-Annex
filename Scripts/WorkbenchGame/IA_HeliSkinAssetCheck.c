#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Run after adding or changing a helicopter skin. Registers skin prefabs,
//! parts and materials Workbench has not imported yet, prints the ResourceName
//! to reference, and checks every variant: it inherits its stock airframe,
//! repaints only slots its meshes have, overrides seats without adding slots,
//! and spawns with its painted parts.
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
		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		Check(!defs.IsEmpty(), "the catalogue lists at least one skin");

		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAHeliSkinAssetCheck");
		BaseWorld world = preview.GetRef();
		int count;
		int i;
		foreach (IA_HeliSkinDef def : defs)
		{
			count = def.m_aStockPrefabs.Count();
			Check(count > 0 && count == def.m_aSkinPrefabs.Count(), def.m_sKey + ": every stock airframe is paired with a variant");
			for (i = 0; i < count; i++)
			{
				CheckVariant(def.m_sKey, def.m_aStockPrefabs[i], def.m_aSkinPrefabs[i], world);
			}
		}

		if (m_iFailures == 0)
			Print("[IA][HeliSkinAssetCheck] PASS", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVariant(string key, ResourceName stock, ResourceName skin, BaseWorld world)
	{
		string label = key + ": " + skin.GetPath();
		Check(Workbench.GetResourceName(stock.GetPath()) == stock, label + " is paired with a registered stock airframe");

		BaseContainer source = LoadRegistered(label, skin);
		if (!source)
			return;

		BaseContainer ancestor = source.GetAncestor();
		Check(ancestor && ancestor.GetResourceName().GetPath() == stock.GetPath(), label + " inherits " + stock.GetPath());
		Check(CheckPaint(label, source) > 0, label + " assigns a skin material");

		ref array<ResourceName> parts = {};
		int slotCount = OwnParts(source, parts);
		if (ancestor)
			Check(slotCount == OwnParts(ancestor, null), label + " overrides slots without adding any");

		BaseContainer partSource;
		foreach (ResourceName part : parts)
		{
			partSource = LoadRegistered(label, part);
			if (partSource)
				Check(CheckPaint(part.GetPath(), partSource) > 0, part.GetPath() + " assigns a skin material");
		}

		CheckSpawn(label, skin, parts, world);
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
	//! Checks the materials a prefab's mesh takes from this addon.
	//! \return how many it assigns
	protected int CheckPaint(string label, notnull BaseContainer source)
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
				Check(Registered(assigned) == assigned, label + " must reference " + Registered(assigned));
				Check(Loads(assigned), label + ": " + assigned.GetPath() + " loads");
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
	protected void CheckSpawn(string label, ResourceName skin, notnull array<ResourceName> parts, BaseWorld world)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(skin), world, params);
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
	protected bool Loads(ResourceName name)
	{
		Resource resource = Resource.Load(name);
		return resource && resource.IsValid();
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
