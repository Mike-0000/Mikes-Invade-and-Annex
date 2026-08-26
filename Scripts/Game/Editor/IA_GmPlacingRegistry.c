//------------------------------------------------------------------------------------------------
//! Puts I&A capture-site wrappers into the Game Master content browser.
//! Prefabs under PrefabsEditable/IA are not collected unless they sit in a
//! SCR_PlaceableEntitiesRegistry on SCR_PlacingEditorComponentClass.
//! Load the conf (do not `new` the registry): Attribute m_bExposed is private and
//! stays false on script `new`, so GetPrefabs(onlyExposed=true) would insert empty
//! names and the cards would never appear.
//------------------------------------------------------------------------------------------------
modded class SCR_PlacingEditorComponentClass
{
	protected static const ResourceName IA_SITES_REGISTRY = "{1A6D47B0E6C35B01}Configs/Editor/PlaceableEntities/IA_Sites.conf";

	protected bool m_bIAGmPlaceables;
	protected ref SCR_PlaceableEntitiesRegistry m_IARegistry;

	//------------------------------------------------------------------------------------------------
	protected void IA_EnsurePlaceables()
	{
		if (m_bIAGmPlaceables)
			return;
		if (!m_Registries)
			return;

		Resource configContainer = BaseContainerTools.LoadContainer(IA_SITES_REGISTRY);
		if (!configContainer || !configContainer.IsValid())
		{
			Print("[IA] Placeable registry conf failed to load", LogLevel.ERROR);
			return;
		}

		m_IARegistry = SCR_PlaceableEntitiesRegistry.Cast(BaseContainerTools.CreateInstanceFromContainer(configContainer.GetResource().ToBaseContainer()));
		if (!m_IARegistry)
		{
			Print("[IA] Placeable registry conf is not SCR_PlaceableEntitiesRegistry", LogLevel.ERROR);
			return;
		}

		m_bIAGmPlaceables = true;
		m_aIndexes.Insert(m_iPrefabCount);
		m_iPrefabCount = m_iPrefabCount + m_IARegistry.GetPrefabs().Count();
		m_Registries.Insert(m_IARegistry);
		Print(string.Format("[IA] Registered %1 I&A sites in the Game Master content browser", m_IARegistry.GetPrefabs().Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override ResourceName GetPrefab(int index)
	{
		IA_EnsurePlaceables();
		return super.GetPrefab(index);
	}

	//------------------------------------------------------------------------------------------------
	override int CountPrefabs()
	{
		IA_EnsurePlaceables();
		return super.CountPrefabs();
	}

	//------------------------------------------------------------------------------------------------
	override int GetPrefabs(out notnull array<ResourceName> outPrefabs, bool onlyExposed = false)
	{
		IA_EnsurePlaceables();
		return super.GetPrefabs(outPrefabs, onlyExposed);
	}

	//------------------------------------------------------------------------------------------------
	override int GetPrefabID(ResourceName prefab)
	{
		IA_EnsurePlaceables();
		return super.GetPrefabID(prefab);
	}
}
