//------------------------------------------------------------------------------------------------
//! One unlockable helicopter skin. A skin is a set of colours: for each surface
//! of the airframe it names a material that holds them. That material is never
//! put on a mesh; IA_HeliSkinPaint copies its colours onto a paint channel, so
//! a skin changes on a live helicopter without touching the entity.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinDef
{
	int m_iId;					// never reuse
	string m_sKey;				// key shared with the backend threshold table
	string m_sDisplayName;
	int m_iRequiredPoints;		// global transport rating

	// By IA_HeliPaintChannels surface; an empty entry keeps that surface stock.
	ref array<ResourceName> m_aPaints = {};

	//------------------------------------------------------------------------------------------------
	void IA_HeliSkinDef()
	{
		int surface;
		for (surface = 0; surface < IA_HeliPaintChannels.SURFACE_COUNT; surface++)
		{
			m_aPaints.Insert(ResourceName.Empty);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return the material holding this skin's colours for a surface, empty when it keeps stock
	ResourceName GetPaint(int surface)
	{
		if (surface < 0 || surface >= m_aPaints.Count())
			return ResourceName.Empty;
		return m_aPaints[surface];
	}
}
