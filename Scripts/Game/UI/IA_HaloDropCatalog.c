//------------------------------------------------------------------------------------------------
//! I&A fills the HALO planner drop-site list. Vanilla HALO Collect stays empty
//! when this addon is not loaded.
//!
//! Consumer: loaded with I&A. Do not instantiate.
//------------------------------------------------------------------------------------------------
modded class MHJ_DropSiteCatalog
{
	//------------------------------------------------------------------------------------------------
	static override void Collect(notnull array<ref MHJ_DropSite> outSites)
	{
		IA_HaloDropSites.Fill(outSites);
		super.Collect(outSites);
	}
}
