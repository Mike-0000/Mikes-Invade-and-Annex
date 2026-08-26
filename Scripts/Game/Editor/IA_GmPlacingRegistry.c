//------------------------------------------------------------------------------------------------
//! Puts I&A capture-site wrappers into the vanilla Game Master content browser.
//! Prefabs under PrefabsEditable/IA are not collected unless they sit in a
//! SCR_PlaceableEntitiesRegistry on SCR_PlacingEditorComponentClass.
//------------------------------------------------------------------------------------------------
modded class SCR_PlacingEditorComponentClass
{
	protected bool m_bIAGmPlaceables;
	protected ref SCR_PlaceableEntitiesRegistry m_IARegistry;
	protected ref array<ResourceName> m_IAPrefabs;

	//------------------------------------------------------------------------------------------------
	protected void IA_EnsurePlaceables()
	{
		if (m_bIAGmPlaceables)
			return;
		m_bIAGmPlaceables = true;

		m_IAPrefabs = new array<ResourceName>();
		m_IAPrefabs.Insert("{1A6D47B0E6C35924}PrefabsEditable/IA/E_IA_Town.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35914}PrefabsEditable/IA/E_IA_City.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35934}PrefabsEditable/IA/E_IA_Property.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35944}PrefabsEditable/IA/E_IA_Military.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35954}PrefabsEditable/IA/E_IA_SmallMilitary.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35964}PrefabsEditable/IA/E_IA_Docks.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35974}PrefabsEditable/IA/E_IA_RadioTower.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35984}PrefabsEditable/IA/E_IA_MortarPit.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35994}PrefabsEditable/IA/E_IA_Defend.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35A03}PrefabsEditable/IA/E_IA_SideObjective.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35A13}PrefabsEditable/IA/E_IA_HVT.et");
		m_IAPrefabs.Insert("{1A6D47B0E6C35A23}PrefabsEditable/IA/E_IA_AISpawnPoint.et");

		m_IARegistry = new SCR_PlaceableEntitiesRegistry();
		m_IARegistry.SetPrefabs(m_IAPrefabs);

		if (!m_Registries)
			return;

		m_aIndexes.Insert(m_iPrefabCount);
		m_iPrefabCount = m_iPrefabCount + m_IAPrefabs.Count();
		m_Registries.Insert(m_IARegistry);
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
