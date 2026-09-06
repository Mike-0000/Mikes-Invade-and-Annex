// Composition-based layouts use the existing survey/ownership/nav lifecycle.
// Catalog dimensions are measured meshes, not campaign interaction volumes.
class IA_ComposedSiteLayout : IA_DynamicSiteLayout
{
	void Initialize(int id, int variant, string name, float halfWidth, float halfDepth, int garrison)
	{
		m_bComposed = true;
		m_iDesignVariant = variant;
		m_iLayoutId = id;
		m_sName = name;
		m_fHalfWidthM = halfWidth;
		m_fHalfDepthM = halfDepth;
		m_iMaxGarrison = garrison;
		m_fCaptureRadiusM = Math.Min(30, halfWidth * 0.4);
		m_vAssemblyLocal = vector.Zero;
		m_aModules = {};
		m_aEntries = {Vector(0, 0, -halfDepth), Vector(-halfWidth, 0, -8), Vector(halfWidth, 0, -8)};
		m_aGuardPosts = {};
		m_aPerimeterStations = {};
	}

	override float GetDefendPostRadius(vector localPost, float preferredRadius = 15)
	{
		// Discrete positions are not four continuous wall faces. A deep bunker
		// on one flank must not invalidate every outdoor post along that side.
		float clearance = Math.Min(m_fHalfWidthM - Math.AbsFloat(localPost[0]), m_fHalfDepthM - Math.AbsFloat(localPost[2])) - 1;
		return Math.Max(0, Math.Min(preferredRadius, clearance));
	}

	void AddComposition(string key, vector position, float yaw, int role, int side, bool required)
	{
		ref IA_BaseCompositionAsset asset = IA_BaseCompositionCatalog.Get(key);
		if (!asset)
			return;
		string id = key + "_" + m_aModules.Count().ToString();
		float halfW = Math.Max(Math.AbsFloat(asset.m_vMins[0]), Math.AbsFloat(asset.m_vMaxs[0])) + 1;
		float halfD = Math.Max(Math.AbsFloat(asset.m_vMins[2]), Math.AbsFloat(asset.m_vMaxs[2])) + 1;
		AddModule(id, asset.m_Prefab, position[0], position[2], yaw, halfW, halfD, role, 0.15, asset.m_iExpanded);
		IA_DynamicSiteModule module = m_aModules[m_aModules.Count() - 1];
		module.m_bRequired = required;
		module.m_iPerimeterSide = side;
		module.m_bFollowTerrainPlane = true;
		module.m_fClearanceHeightM = Math.Max(6, asset.m_vMaxs[1] + 1);
		module.SetSupportFootprint(Vector(asset.m_vMins[0], 0, asset.m_vMins[2]), Vector(asset.m_vMaxs[0], 0, asset.m_vMaxs[2]));
		if (side >= 0)
			m_aPerimeterStations.Insert(position);
		foreach (IA_BaseGunSocket socket : asset.m_aSockets)
		{
			ref IA_DynamicBaseEmplacementSpec spec = IA_DynamicBaseEmplacementSpec.Create(id + "_gun", id, position, yaw, side, socket.m_iKind > 0);
			spec.m_Socket = socket;
			m_aEmplacements.Insert(spec);
		}
	}
}
