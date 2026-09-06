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

	void AddPerimeterWall(vector position, float yaw, int side, int style = 0)
	{
		AddCover("infill_" + m_aModules.Count().ToString(), position[0], position[2], yaw, style, side);
		// Do not reveal a nominally closed perimeter with randomly missing walls.
		m_aModules[m_aModules.Count() - 1].m_bRequired = true;
	}

	protected float InteriorSupportDelta(float meshW, float meshD)
	{
		float spanM = Math.Max(meshW, meshD) * 2;
		float maxDelta = 0.8;
		if (spanM > 16)
			maxDelta = 0.8 + (spanM - 16) * 0.015;
		if (maxDelta > 1.6)
			maxDelta = 1.6;
		return maxDelta;
	}

	void AddComposition(string key, vector position, float yaw, int role, int side, bool required)
	{
		ref IA_BaseCompositionAsset asset = IA_BaseCompositionCatalog.Get(key);
		if (!asset)
			return;
		string id = key + "_" + m_aModules.Count().ToString();
		float meshW = Math.Max(Math.AbsFloat(asset.m_vMins[0]), Math.AbsFloat(asset.m_vMaxs[0]));
		float meshD = Math.Max(Math.AbsFloat(asset.m_vMins[2]), Math.AbsFloat(asset.m_vMaxs[2]));
		float halfW = meshW;
		float halfD = meshD;
		float maxDelta = 0.8;
		if (side >= 0)
		{
			// Wall-line poses sit on the grade. The old 0.15 m absolute span
			// rejected every nest once FollowTerrainPlane was cleared. Do not
			// add the interior +1 m pad or the volume samples land outside
			// the wall and fail on edge terrain.
			maxDelta = 0.8;
			required = true;
		}
		else
		{
			halfW = meshW + 1;
			halfD = meshD + 1;
			// Interiors follow the grade. 15 cm over a 50 m LivingLarge pad
			// rejected ordinary Everon farmland once every recipe required
			// that cluster. Scale residual with the measured mesh span.
			maxDelta = InteriorSupportDelta(meshW, meshD);
		}
		AddModule(id, asset.m_Prefab, position[0], position[2], yaw, halfW, halfD, role, maxDelta, asset.m_iExpanded);
		IA_DynamicSiteModule module = m_aModules[m_aModules.Count() - 1];
		module.m_bRequired = required;
		module.m_iPerimeterSide = side;
		// Perimeter fighting positions stay on the authored heading so they
		// sit in the wall line. Interior tents may still follow the grade.
		module.m_bFollowTerrainPlane = side < 0;
		if (side < 0)
		{
			module.m_fMaxFoundationLiftM = 0.35;
			module.m_fFloorAboveOriginM = 0.1;
		}
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
