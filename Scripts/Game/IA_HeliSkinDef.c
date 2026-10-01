//------------------------------------------------------------------------------------------------
//! One unlockable helicopter skin. Paint is authored into prefab variants, one
//! per stock airframe: remapping materials on a live vehicle crashes the engine,
//! so a skinned helicopter is a different prefab, never a repainted entity.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinDef
{
	int m_iId;					// never reuse
	string m_sKey;				// key shared with the backend threshold table
	string m_sDisplayName;
	int m_iRequiredPoints;		// global transport rating

	// Stock airframes and, at the same index, the variant wearing this skin.
	ref array<ResourceName> m_aStockPrefabs = {};
	ref array<ResourceName> m_aSkinPrefabs = {};

	//------------------------------------------------------------------------------------------------
	//! \return this skin's variant of a stock airframe, empty when it has none
	ResourceName FindVariant(ResourceName stockPrefab)
	{
		int index = IndexOf(m_aStockPrefabs, stockPrefab);
		if (index < 0)
			return ResourceName.Empty;
		return m_aSkinPrefabs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the stock airframe behind one of this skin's variants, empty when it is not one
	ResourceName FindStock(ResourceName skinPrefab)
	{
		int index = IndexOf(m_aSkinPrefabs, skinPrefab);
		if (index < 0)
			return ResourceName.Empty;
		return m_aStockPrefabs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Compares paths, so a prefab name with or without its GUID matches.
	protected int IndexOf(notnull array<ResourceName> prefabs, ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return -1;

		string path = prefab.GetPath();
		int count = prefabs.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (prefabs[i].GetPath() == path)
				return i;
		}
		return -1;
	}
}
