//------------------------------------------------------------------------------------------------
//! One unlockable helicopter recolor. A skin swaps material slot families on
//! one airframe family; it never changes the prefab, so seats, catalog labels
//! and pilot-role checks keep working.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinDef
{
	int m_iId;					// replicated id, never reuse
	string m_sKey;				// key shared with the backend threshold table
	string m_sDisplayName;
	string m_sPrefabToken;		// substring of the vehicle prefab path
	int m_iRequiredPoints;		// global transport rating

	// Material slot name prefixes on the airframe's meshes, and the material each one gets.
	ref array<string> m_aSlotPrefixes = {};
	ref array<ResourceName> m_aMaterials = {};

	//------------------------------------------------------------------------------------------------
	//! \return replacement for a mesh material slot, empty when this skin leaves the slot alone
	ResourceName FindMaterial(string slotName)
	{
		int count = m_aSlotPrefixes.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (slotName.StartsWith(m_aSlotPrefixes[i]))
				return m_aMaterials[i];
		}
		return ResourceName.Empty;
	}
}
