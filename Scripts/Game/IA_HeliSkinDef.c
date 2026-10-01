//------------------------------------------------------------------------------------------------
//! One unlockable helicopter recolor. A skin swaps one material slot family on
//! one airframe family; it never changes the prefab, so seats, catalog labels
//! and pilot-role checks keep working.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinDef
{
	int m_iId;					// replicated id, never reuse
	string m_sKey;				// key shared with the backend threshold table
	string m_sDisplayName;
	string m_sPrefabToken;		// substring of the vehicle prefab path
	string m_sSlotPrefix;		// material slot name prefix on the xob
	ResourceName m_sMaterial;
	int m_iRequiredPoints;		// global transport rating
}
