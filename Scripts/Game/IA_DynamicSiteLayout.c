//------------------------------------------------------------------------------------------------
//! Authored Full (180x140) and Compact (120x100) base manifests.
//! Coordinates are local metres. +Z is the rear; the front gate is on -Z.
//! Visual modules use audited USSR tent/composition resources already registered
//! by the base game (same starting set as IA_DefendEvent). Workbench-owned IA
//! wrappers can replace these ResourceNames later without changing sockets.
//------------------------------------------------------------------------------------------------
class IA_DynamicSiteModule
{
	string m_sId;
	ResourceName m_Prefab;
	vector m_vLocalPosition;
	float m_fLocalYawDeg;
	float m_fPrefabYawCorrectionDeg;
	float m_fHalfWidthM;
	float m_fHalfDepthM;
	ref array<vector> m_aSupportPoints;
	float m_fMaxSupportDeltaM = 0.35;
	int m_iGroundingPolicy;
	bool m_bRequired = true;
	int m_iEstimatedExpandedEntities = 8;
	int m_iRole;
}

class IA_DynamicSiteLayout
{
	static const int LAYOUT_FULL = 0;
	static const int LAYOUT_COMPACT = 1;

	static const ResourceName PREFAB_HQ = "{1D2887BB9A7D4670}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et";
	static const ResourceName PREFAB_BARRACKS = "{607E00F1C367D129}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et";
	static const ResourceName PREFAB_MEDICAL = "{5A25DAC561B3BFF8}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Medical_USSR_01.et";
	static const ResourceName PREFAB_SUPPLY = "{DC82213D62A2F621}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Supply_Large_USSR_01.et";
	static const ResourceName PREFAB_RADIO = "{55B73CF1EE914E07}Prefabs/Props/Military/Compositions/USSR/Antenna_02_USSR.et";
	static const ResourceName PREFAB_COVER = "{8E1DF47DD56E69E6}Prefabs/Compositions/Slotted/SlotFlatSmall/SandbagPosition_S_USSR_01.et";
	static const ResourceName PREFAB_TOWER = "{DFBF655559915333}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_USSR_01.et";
	static const ResourceName PREFAB_WORKSHOP = "{4510C273B794F3A9}Prefabs/Compositions/Slotted/SlotFlatMedium/VehicleMaintenance_M_USSR_01.et";
	static const ResourceName PREFAB_FUEL = "{73604060A04CBB85}Prefabs/Compositions/Slotted/SlotFlatSmall/FuelStorage_S_USSR_01.et";

	int m_iLayoutId;
	string m_sName;
	float m_fHalfWidthM;
	float m_fHalfDepthM;
	float m_fCaptureRadiusM;
	vector m_vCaptureLocal;
	vector m_vAssemblyLocal;
	ref array<ref IA_DynamicSiteModule> m_aModules;
	ref array<vector> m_aEntries;
	ref array<vector> m_aGuardPosts;
	ref array<vector> m_aPerimeterStations;

	//------------------------------------------------------------------------------------------------
	static IA_DynamicSiteLayout CreateFull()
	{
		ref IA_DynamicSiteLayout layout = new IA_DynamicSiteLayout();
		layout.m_iLayoutId = LAYOUT_FULL;
		layout.m_sName = "Full";
		layout.m_fHalfWidthM = 90;
		layout.m_fHalfDepthM = 70;
		layout.m_fCaptureRadiusM = 35;
		layout.m_vCaptureLocal = Vector(0, 0, 24);
		layout.m_vAssemblyLocal = Vector(0, 0, 0);
		layout.m_aModules = new array<ref IA_DynamicSiteModule>();
		layout.m_aEntries = new array<vector>();
		layout.m_aGuardPosts = new array<vector>();
		layout.m_aPerimeterStations = new array<vector>();

		layout.m_aEntries.Insert(Vector(0, 0, -70));
		layout.m_aEntries.Insert(Vector(-90, 0, -8));
		layout.m_aEntries.Insert(Vector(90, 0, -8));

		layout.m_aGuardPosts.Insert(Vector(0, 0, 22));
		layout.m_aGuardPosts.Insert(Vector(-14, 0, -54));
		layout.m_aGuardPosts.Insert(Vector(-73, 0, -24));
		layout.m_aGuardPosts.Insert(Vector(73, 0, -24));
		layout.m_aGuardPosts.Insert(Vector(0, 0, 54));
		layout.m_aGuardPosts.Insert(Vector(19, 0, 1));

		layout.AddModule("hq", PREFAB_HQ, 0, 35, 180, 12, 10, IA_DynamicSiteModuleRole.Hq, 0.35, 16);
		layout.AddModule("radio", PREFAB_RADIO, 23, 46, 0, 5, 5, IA_DynamicSiteModuleRole.Radio, 0.50, 4);
		layout.AddModule("barracksA", PREFAB_BARRACKS, -62, 36, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("barracksB", PREFAB_BARRACKS, -62, 12, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("barracksC", PREFAB_BARRACKS, -35, 36, 270, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("barracksD", PREFAB_BARRACKS, -35, 12, 270, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("medical", PREFAB_MEDICAL, 59, 31, 270, 14, 14, IA_DynamicSiteModuleRole.Medical, 0.35, 12);
		layout.AddModule("motor", PREFAB_WORKSHOP, -49, -41, 90, 21, 15, IA_DynamicSiteModuleRole.MotorPool, 0.50, 14);
		layout.AddModule("supply", PREFAB_SUPPLY, 30, -31, 0, 11, 11, IA_DynamicSiteModuleRole.Supply, 0.35, 10);
		layout.AddModule("fuel", PREFAB_FUEL, 65, -49, 0, 11, 11, IA_DynamicSiteModuleRole.Fuel, 0.50, 8);

		layout.AddCover("coverRearW2", -76, 62, 0);
		layout.AddCover("coverRearW1", -46, 62, 0);
		layout.AddCover("coverRearC", 0, 62, 0);
		layout.AddCover("coverRearE1", 46, 62, 0);
		layout.AddCover("coverRearE2", 76, 62, 0);
		layout.AddCover("coverWestN", -82, 38, 270);
		layout.AddCover("coverWestC", -82, 10, 270);
		layout.AddCover("coverWestS", -82, -30, 270);
		layout.AddCover("coverWestF", -82, -56, 270);
		layout.AddCover("coverEastN", 82, 38, 90);
		layout.AddCover("coverEastC", 82, 10, 90);
		layout.AddCover("coverEastS", 82, -30, 90);
		layout.AddCover("coverEastF", 82, -56, 90);
		layout.AddCover("coverFrontW2", -65, -62, 180);
		layout.AddCover("coverFrontW1", -38, -62, 180);
		layout.AddCover("coverGateW", -17, -61, 180);
		layout.AddCover("coverGateE", 17, -61, 180);
		layout.AddCover("coverFrontE1", 38, -62, 180);
		layout.AddCover("coverFrontE2", 65, -67, 180);
		return layout;
	}

	//------------------------------------------------------------------------------------------------
	static IA_DynamicSiteLayout CreateCompact()
	{
		ref IA_DynamicSiteLayout layout = new IA_DynamicSiteLayout();
		layout.m_iLayoutId = LAYOUT_COMPACT;
		layout.m_sName = "Compact";
		layout.m_fHalfWidthM = 60;
		layout.m_fHalfDepthM = 50;
		layout.m_fCaptureRadiusM = 30;
		layout.m_vCaptureLocal = Vector(0, 0, 17);
		layout.m_vAssemblyLocal = Vector(0, 0, 0);
		layout.m_aModules = new array<ref IA_DynamicSiteModule>();
		layout.m_aEntries = new array<vector>();
		layout.m_aGuardPosts = new array<vector>();
		layout.m_aPerimeterStations = new array<vector>();

		layout.m_aEntries.Insert(Vector(0, 0, -50));
		layout.m_aEntries.Insert(Vector(-60, 0, -5));
		layout.m_aEntries.Insert(Vector(60, 0, -5));

		layout.m_aGuardPosts.Insert(Vector(0, 0, 16));
		layout.m_aGuardPosts.Insert(Vector(-12, 0, -36));
		layout.m_aGuardPosts.Insert(Vector(-46, 0, -18));
		layout.m_aGuardPosts.Insert(Vector(46, 0, -18));
		layout.m_aGuardPosts.Insert(Vector(0, 0, 39));
		layout.m_aGuardPosts.Insert(Vector(14, 0, 5));

		layout.AddModule("hq", PREFAB_HQ, 0, 27, 180, 10, 9, IA_DynamicSiteModuleRole.Hq, 0.35, 16);
		layout.AddModule("barracksA", PREFAB_BARRACKS, -37, 28, 90, 9, 8, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("barracksB", PREFAB_BARRACKS, -37, 9, 90, 9, 8, IA_DynamicSiteModuleRole.Barracks, 0.35, 10);
		layout.AddModule("medical", PREFAB_MEDICAL, 37, 24, 270, 11, 11, IA_DynamicSiteModuleRole.Medical, 0.35, 12);
		layout.AddModule("motor", PREFAB_WORKSHOP, -33, -30, 90, 14, 11, IA_DynamicSiteModuleRole.MotorPool, 0.50, 14);
		layout.AddModule("supply", PREFAB_SUPPLY, 25, -24, 0, 9, 9, IA_DynamicSiteModuleRole.Supply, 0.35, 10);
		layout.AddModule("fuel", PREFAB_FUEL, 47, -37, 0, 6, 7, IA_DynamicSiteModuleRole.Fuel, 0.50, 8);

		layout.AddCover("coverRearW2", -49, 43, 0);
		layout.AddCover("coverRearW1", -24, 43, 0);
		layout.AddCover("coverRearE1", 24, 43, 0);
		layout.AddCover("coverRearE2", 49, 43, 0);
		layout.AddCover("coverWestN", -53, 24, 270);
		layout.AddCover("coverWestS", -53, -25, 270);
		layout.AddCover("coverEastN", 53, 24, 90);
		layout.AddCover("coverEastS", 53, -25, 90);
		layout.AddCover("coverFrontW", -40, -43, 180);
		layout.AddCover("coverGateW", -17, -43, 180);
		layout.AddCover("coverGateE", 17, -43, 180);
		return layout;
	}

	//------------------------------------------------------------------------------------------------
	static IA_DynamicSiteLayout CreateById(int layoutId)
	{
		if (layoutId == LAYOUT_COMPACT)
			return CreateCompact();
		return CreateFull();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddModule(string id, ResourceName prefab, float x, float z, float yawDeg, float halfW, float halfD, int role, float maxDelta, int expanded)
	{
		ref IA_DynamicSiteModule mod = new IA_DynamicSiteModule();
		mod.m_sId = id;
		mod.m_Prefab = prefab;
		mod.m_vLocalPosition = Vector(x, 0, z);
		mod.m_fLocalYawDeg = yawDeg;
		mod.m_fPrefabYawCorrectionDeg = 0;
		mod.m_fHalfWidthM = halfW;
		mod.m_fHalfDepthM = halfD;
		mod.m_aSupportPoints = new array<vector>();
		mod.m_aSupportPoints.Insert(Vector(0, 0, 0));
		mod.m_aSupportPoints.Insert(Vector(-halfW * 0.6, 0, -halfD * 0.6));
		mod.m_aSupportPoints.Insert(Vector(halfW * 0.6, 0, -halfD * 0.6));
		mod.m_aSupportPoints.Insert(Vector(-halfW * 0.6, 0, halfD * 0.6));
		mod.m_aSupportPoints.Insert(Vector(halfW * 0.6, 0, halfD * 0.6));
		mod.m_fMaxSupportDeltaM = maxDelta;
		mod.m_iGroundingPolicy = IA_DynamicSiteGrounding.UprightPad;
		mod.m_bRequired = true;
		mod.m_iEstimatedExpandedEntities = expanded;
		mod.m_iRole = role;
		m_aModules.Insert(mod);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCover(string id, float x, float z, float yawDeg)
	{
		AddModule(id, PREFAB_COVER, x, z, yawDeg, 4, 3, IA_DynamicSiteModuleRole.Cover, 0.50, 3);
		m_aModules[m_aModules.Count() - 1].m_iGroundingPolicy = IA_DynamicSiteGrounding.TerrainSegment;
		m_aPerimeterStations.Insert(Vector(x, 0, z));
	}

	//------------------------------------------------------------------------------------------------
	void BuildRootTransform(vector origin, float yawDeg, out vector rootMat[4])
	{
		Math3D.AnglesToMatrix(Vector(yawDeg, 0, 0), rootMat);
		rootMat[3] = origin;
	}

	//------------------------------------------------------------------------------------------------
	void LocalToWorld(vector rootMat[4], vector localPos, float localYawDeg, float yawCorrection, out vector worldMat[4])
	{
		vector localMat[4];
		Math3D.AnglesToMatrix(Vector(localYawDeg + yawCorrection, 0, 0), localMat);
		localMat[3] = localPos;
		Math3D.MatrixMultiply4(rootMat, localMat, worldMat);
	}

	//------------------------------------------------------------------------------------------------
	vector LocalOffsetToWorld(vector rootMat[4], vector localPos)
	{
		return rootMat[3] + (rootMat[0] * localPos[0]) + (rootMat[2] * localPos[2]);
	}

	//------------------------------------------------------------------------------------------------
	int CountRequiredModules()
	{
		int count = 0;
		int n = m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			if (m_aModules[i] && m_aModules[i].m_bRequired)
				count = count + 1;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	int EstimateExpandedEntities()
	{
		int total = 0;
		int n = m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			if (m_aModules[i])
				total = total + m_aModules[i].m_iEstimatedExpandedEntities;
		}
		return total;
	}
}
