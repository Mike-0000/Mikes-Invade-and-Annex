// Permanent-headquarters recipes: concrete and camo sandbag wall runs,
// casemates, mesh-only buildings, an obstacle belt and planned dressing
// (gates, checkpoint, roads, dirt decals, lived-in vignettes). Shares the composed-site survey, ownership,
// emplacement and nav lifecycle; only asset lookup and grounding differ.
class IA_HeadquartersSiteLayout : IA_ComposedSiteLayout
{
	// Panels carry 1 m of buried foundation (measured y -1.002). Lift at most
	// 0.9 m so the downhill end still sits in the ground.
	static const float WALL_MAX_DELTA_M = 0.9;
	static const float WALL_MAX_LIFT_M = 0.9;
	static const float WALL_HALF_DEPTH_M = 0.6;
	// Obstacles snap to the terrain per piece; only reject cliffs.
	static const float OBSTACLE_MAX_DELTA_M = 1.2;
	// Buildings with a buried plinth stand upright instead of tilting with the grade.
	static const float UPRIGHT_MIN_FOUNDATION_M = 0.6;
	static const float UPRIGHT_MAX_LIFT_M = 1.0;
	// Game Master camo sandbag runs have no buried foundation (measured y
	// -0.17 with the authored 0.1 m sink), so they follow the grade instead.
	static const float SANDBAG_MAX_FOUNDATION_M = 0.5;
	static const float SANDBAG_MAX_RESIDUAL_M = 0.5;

	override protected IA_BaseCompositionAsset ResolveAsset(string key)
	{
		ref IA_BaseCompositionAsset asset = IA_HeadquartersCatalog.Get(key);
		if (asset)
			return asset;
		return IA_BaseCompositionCatalog.Get(key);
	}

	override void AddComposition(string key, vector position, float yaw, int role, int side, bool required)
	{
		int before = m_aModules.Count();
		super.AddComposition(key, position, yaw, role, side, required);
		if (side >= 0 || m_aModules.Count() == before)
			return;
		ref IA_BaseCompositionAsset asset = IA_HeadquartersCatalog.Get(key);
		if (!asset)
			return;
		float foundation = -asset.m_vMins[1];
		if (foundation < UPRIGHT_MIN_FOUNDATION_M)
			return;
		// A tilted guard tower or barracks reads as broken; its plinth hides lift.
		IA_DynamicSiteModule module = m_aModules[before];
		module.m_bFollowTerrainPlane = false;
		module.m_fMaxFoundationLiftM = Math.Min(UPRIGHT_MAX_LIFT_M, foundation - 0.3);
		module.m_fMaxSupportDeltaM = Math.Min(module.m_fMaxSupportDeltaM, foundation - 0.2);
	}

	// Concrete panels stand upright on 1 m of foundation; camo sandbag runs
	// have none and follow the terrain plane.
	static bool IsSandbagRun(notnull IA_BaseCompositionAsset asset)
	{
		return -asset.m_vMins[1] < SANDBAG_MAX_FOUNDATION_M;
	}

	// One replicated wall run on the wall line. Coverage is required
	// collectively (HasPerimeterCoverage), like scrappy sandbag infill.
	void AddWallRun(string key, vector position, float yaw, int side)
	{
		ref IA_BaseCompositionAsset asset = IA_HeadquartersCatalog.Get(key);
		if (!asset)
			return;
		float halfW = Math.Max(Math.AbsFloat(asset.m_vMins[0]), Math.AbsFloat(asset.m_vMaxs[0]));
		string id = "wall_" + m_aModules.Count().ToString();
		AddModule(id, asset.m_Prefab, position[0], position[2], yaw, halfW, WALL_HALF_DEPTH_M, IA_DynamicSiteModuleRole.Cover, WALL_MAX_DELTA_M, asset.m_iExpanded);
		IA_DynamicSiteModule module = m_aModules[m_aModules.Count() - 1];
		module.m_iGroundingPolicy = IA_DynamicSiteGrounding.UprightPad;
		module.m_iPerimeterSide = side;
		module.m_bRequired = false;
		module.m_bFollowTerrainPlane = false;
		module.m_fMaxFoundationLiftM = WALL_MAX_LIFT_M;
		module.m_fFloorAboveOriginM = 0.05;
		module.m_fClearanceHeightM = asset.m_vMaxs[1] + 1;
		module.SetSupportFootprint(Vector(asset.m_vMins[0], 0, asset.m_vMins[2]), Vector(asset.m_vMaxs[0], 0, asset.m_vMaxs[2]));
		if (IsSandbagRun(asset))
		{
			module.m_iGroundingPolicy = IA_DynamicSiteGrounding.TerrainSegment;
			module.m_bFollowTerrainPlane = true;
			module.m_fMaxFoundationLiftM = 0;
			module.m_fFloorAboveOriginM = 0;
			module.m_fMaxSupportDeltaM = SANDBAG_MAX_RESIDUAL_M;
			// Soft camo nets overhang the bags; only the bags bear.
			module.SetSupportFootprint(Vector(asset.m_vMins[0], 0, -WALL_HALF_DEPTH_M), Vector(asset.m_vMaxs[0], 0, WALL_HALF_DEPTH_M));
		}
		m_aPerimeterStations.Insert(position);
	}

	// Outer obstacle belt. Dressing never vetoes a site and spawns after guns.
	void AddObstacle(string key, vector position, float yaw)
	{
		ref IA_BaseCompositionAsset asset = IA_HeadquartersCatalog.Get(key);
		if (!asset)
			return;
		float halfW = Math.Max(Math.AbsFloat(asset.m_vMins[0]), Math.AbsFloat(asset.m_vMaxs[0]));
		float halfD = Math.Max(Math.AbsFloat(asset.m_vMins[2]), Math.AbsFloat(asset.m_vMaxs[2]));
		string id = "belt_" + m_aModules.Count().ToString();
		AddDressing(id, asset.m_Prefab, position[0], position[2], yaw, halfW, halfD, asset.m_iExpanded);
		IA_DynamicSiteModule module = m_aModules[m_aModules.Count() - 1];
		module.m_iGroundingPolicy = IA_DynamicSiteGrounding.TerrainSegment;
		module.m_fMaxSupportDeltaM = OBSTACLE_MAX_DELTA_M;
		module.m_fClearanceHeightM = asset.m_vMaxs[1] + 1;
		module.SetSupportFootprint(Vector(asset.m_vMins[0], 0, asset.m_vMins[2]), Vector(asset.m_vMaxs[0], 0, asset.m_vMaxs[2]));
	}

	// Generated gates, checkpoint, roads, decals and vignettes. The recipe
	// planned each mesh box clear of guns, doors and the site's own props;
	// the spawn trace still rejects foreign obstructions and cliffs.
	void AddDressingItem(string key, vector position, float yaw, float maxResidualM)
	{
		ref IA_BaseCompositionAsset asset = IA_HeadquartersCatalog.Get(key);
		if (!asset)
			return;
		float halfW = Math.Max(Math.AbsFloat(asset.m_vMins[0]), Math.AbsFloat(asset.m_vMaxs[0]));
		float halfD = Math.Max(Math.AbsFloat(asset.m_vMins[2]), Math.AbsFloat(asset.m_vMaxs[2]));
		string id = "dress_" + m_aModules.Count().ToString();
		AddDressing(id, asset.m_Prefab, position[0], position[2], yaw, halfW, halfD, asset.m_iExpanded);
		IA_DynamicSiteModule module = m_aModules[m_aModules.Count() - 1];
		module.m_iGroundingPolicy = IA_DynamicSiteGrounding.TerrainSegment;
		module.m_bFollowTerrainPlane = true;
		module.m_bPlannedClearance = true;
		module.m_fMaxSupportDeltaM = maxResidualM;
		module.m_fClearanceHeightM = Math.Max(0.2, asset.m_vMaxs[1]) + 1;
		module.SetSupportFootprint(Vector(asset.m_vMins[0], 0, asset.m_vMins[2]), Vector(asset.m_vMaxs[0], 0, asset.m_vMaxs[2]));
	}
}
