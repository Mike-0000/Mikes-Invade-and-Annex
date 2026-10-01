#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Run after adding or changing a helicopter skin. Registers skin materials
//! Workbench has not imported yet, prints the ResourceName IA_HeliSkinCatalog
//! must reference, and checks every skin against its airframe's meshes.
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter skin asset check", wbModules: {"ResourceManager"})]
class IA_HeliSkinAssetCheck : WorkbenchPlugin
{
	protected int m_iFailures;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		Check(!defs.IsEmpty(), "the catalogue lists at least one skin");
		foreach (IA_HeliSkinDef def : defs)
		{
			CheckDef(def);
		}

		if (m_iFailures == 0)
			Print("[IA][HeliSkinAssetCheck] PASS", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDef(notnull IA_HeliSkinDef def)
	{
		ref array<ResourceName> meshes = {};
		AirframeMeshes(def.m_sPrefabToken, meshes);
		Check(!meshes.IsEmpty(), def.m_sKey + ": its airframe has meshes listed in AirframeMeshes");

		int count = def.m_aSlotPrefixes.Count();
		Check(count > 0 && count == def.m_aMaterials.Count(), def.m_sKey + ": every slot has a material");

		int i;
		for (i = 0; i < count; i++)
		{
			CheckMaterial(def.m_sKey, def.m_aMaterials[i]);
			Check(AnyMeshHasSlot(meshes, def.m_aSlotPrefixes[i]), def.m_sKey + ": slot " + def.m_aSlotPrefixes[i] + " exists on the airframe");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Meshes that carry an airframe's paint: the hull and the parts slotted into it.
	protected void AirframeMeshes(string prefabToken, notnull array<ResourceName> meshes)
	{
		if (prefabToken == "/UH1H/")
		{
			meshes.Insert("{7FF449DB9ED11DB4}Assets/Vehicles/Helicopters/UH1H/UH1H_base.xob");
			meshes.Insert("{B0A46CDFE35A7B66}Assets/Vehicles/Helicopters/UH1H/VehParts/Seats/UH1H_Seats_cargo.xob");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A material Workbench never imported has no GUID and is left out of a published build.
	protected void CheckMaterial(string key, ResourceName material)
	{
		string path = material.GetPath();
		ResourceName registered = Workbench.GetResourceName(path);
		if (!registered.StartsWith("{"))
		{
			registered = Register(path);
			if (!registered.IsEmpty())
				Print("[IA][HeliSkinAssetCheck] Registered " + registered, LogLevel.NORMAL);
		}

		Check(registered.StartsWith("{"), key + ": " + path + " is a registered resource");
		Check(registered == material, key + ": the catalogue must reference " + registered);

		Resource resource = Resource.Load(registered);
		Check(resource && resource.IsValid(), key + ": " + path + " loads");
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
	//! A remap whose source name is not on the mesh fails at runtime, so check the names here.
	protected bool AnyMeshHasSlot(notnull array<ResourceName> meshes, string slotPrefix)
	{
		string materials[256];
		int count;
		int i;
		foreach (ResourceName mesh : meshes)
		{
			Resource resource = Resource.Load(mesh);
			if (!resource || !resource.IsValid())
				continue;

			VObject vobj = resource.GetResource().ToVObject();
			if (!vobj)
				continue;

			count = vobj.GetMaterials(materials);
			for (i = 0; i < count; i++)
			{
				if (materials[i].StartsWith(slotPrefix))
					return true;
			}
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
