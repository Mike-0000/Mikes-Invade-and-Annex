//------------------------------------------------------------------------------------------------
//! I&A fills the HALO planner drop-site list. Vanilla HALO Collect stays empty
//! until a consumer Inserts on GetOnCollect.
//!
//! Consumer: loaded with I&A. Call EnsureRegistered before the planner opens.
//! Do not instantiate from UI code.
//------------------------------------------------------------------------------------------------
class IA_HaloDropCatalog
{
	protected static ref IA_HaloDropCatalog s_Instance;

	//------------------------------------------------------------------------------------------------
	static void EnsureRegistered()
	{
		if (s_Instance)
			return;

		s_Instance = new IA_HaloDropCatalog();
		MHJ_DropSiteCatalog.GetOnCollect().Insert(s_Instance.OnCollect);
	}

	//------------------------------------------------------------------------------------------------
	void OnCollect(array<ref MHJ_DropSite> outSites)
	{
		if (!outSites)
			return;

		IA_HaloDropSites.Fill(outSites);
	}
}
