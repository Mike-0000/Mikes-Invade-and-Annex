//------------------------------------------------------------------------------------------------
//! Authored bases from a large operating base down to a small command post.
//! Coordinates are local metres. +Z is the rear; the front gate is on -Z.
//! IA scenery roots preserve stock USSR layouts without campaign construction
//! actions or service controllers. Perimeters use individually grounded stock tall sandbag walls.
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
	vector m_vSupportMins;
	vector m_vSupportMaxs;
	float m_fMaxSupportDeltaM = 0.35;
	float m_fMaxFoundationLiftM;
	float m_fFloorAboveOriginM;
	int m_iGroundingPolicy;
	bool m_bFollowTerrainPlane;
	float m_fClearanceHeightM = 6;
	bool m_bRequired = true;
	int m_iEstimatedExpandedEntities = 8;
	int m_iRole;
	int m_iPerimeterSide = -1;

	// Physical bearing area is distinct from the larger reserved clearance pad.
	void SetSupportFootprint(vector mins, vector maxs)
	{
		m_vSupportMins = mins;
		m_vSupportMaxs = maxs;
		m_aSupportPoints.Clear();
		m_aSupportPoints.Insert(vector.Zero);
		int nx = Math.Ceil((maxs[0] - mins[0]) / 2);
		int nz = Math.Ceil((maxs[2] - mins[2]) / 2);
		for (int ix = 0; ix <= nx; ix++)
		{
			for (int iz = 0; iz <= nz; iz++)
				m_aSupportPoints.Insert(Vector(Math.Min(maxs[0], mins[0] + ix * 2), 0, Math.Min(maxs[2], mins[2] + iz * 2)));
		}
	}

	// Returns an upright composition height; furnishings stay on its stock floor.
	bool ResolveSupportHeight(float centerY, float minY, float maxY, out float originY)
	{
		originY = centerY;
		if (maxY - minY > m_fMaxSupportDeltaM)
			return false;
		if (m_fMaxFoundationLiftM > 0)
		{
			originY = Math.Max(centerY, maxY - m_fFloorAboveOriginM);
			if (originY - centerY > m_fMaxFoundationLiftM)
				return false;
		}
		return true;
	}
}

class IA_DynamicSiteLayout
{
	static const int LAYOUT_FULL = 0;
	static const int LAYOUT_COMPACT = 1;
	static const int LAYOUT_COURTYARD = 2;
	static const int LAYOUT_ROADSIDE = 3;
	static const int LAYOUT_COMMAND_POST = 4;
	static const int LAYOUT_RALLY_POST = 5;

	static const ResourceName PREFAB_HQ = "{1BAF0417D04A57BB}Prefabs/DynamicBase/IA_Tent_CommandPost_USSR_01.et";
	static const ResourceName PREFAB_BARRACKS = "{958824648DCA51E3}Prefabs/DynamicBase/IA_Tent_Barracks_USSR_01.et";
	static const ResourceName PREFAB_MEDICAL = "{B4D06456A1D95855}Prefabs/DynamicBase/IA_Tent_Medical_USSR_01.et";
	static const ResourceName PREFAB_SUPPLY = "{03C725A3AAE35D1C}Prefabs/DynamicBase/IA_Tent_Supply_Large_USSR_01.et";
	static const ResourceName PREFAB_RADIO = "{55B73CF1EE914E07}Prefabs/Props/Military/Compositions/USSR/Antenna_02_USSR.et";
	static const ResourceName PREFAB_COVER = "{9C9C4BED9E19C374}Prefabs/Props/Military/Sandbags/Sandbag_01_wall_solid_burlap.et";
	static const ResourceName PREFAB_COVER_WINDOW = "{CD67070EFAFC28C7}Prefabs/Props/Military/Sandbags/Sandbag_01_wall_burlap.et";
	static const ResourceName PREFAB_COVER_HIGH = "{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et";
	static const ResourceName PREFAB_COVER_ROUND = "{7AF4B627D5C90235}Prefabs/Props/Military/Sandbags/Sandbag_01_round_high_burlap.et";
	static const ResourceName PREFAB_TOWER = "{DFBF655559915333}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_USSR_01.et";
	static const ResourceName PREFAB_WORKSHOP = "{EE2DA99D3A9B5F46}Prefabs/DynamicBase/IA_VehicleMaintenance_M_USSR_01.et";
	static const ResourceName PREFAB_FUEL = "{490B2E7D9EB95EC7}Prefabs/DynamicBase/IA_FuelStorage_S_USSR_01.et";

	static const ResourceName PREFAB_DRESSING_BRIEFING = "{C1DC3475A54F5B8B}Prefabs/DynamicBase/IA_Dressing_Briefing.et";
	static const ResourceName PREFAB_DRESSING_MESS = "{C5FCEDF014885D44}Prefabs/DynamicBase/IA_Dressing_Mess.et";
	static const ResourceName PREFAB_DRESSING_STORES = "{EBACA024C71C5CAF}Prefabs/DynamicBase/IA_Dressing_Stores.et";
	static const ResourceName PREFAB_DRESSING_WATER = "{B3E06835E2455923}Prefabs/DynamicBase/IA_Dressing_Water.et";
	static const ResourceName PREFAB_DRESSING_WORKSHOP = "{3A2537AB39405511}Prefabs/DynamicBase/IA_Dressing_Workshop.et";
	static const ResourceName PREFAB_DRESSING_UTILITY = "{9F0EFC4CC9CD571D}Prefabs/DynamicBase/IA_Dressing_Utility.et";
	static const ResourceName PREFAB_DRESSING_WATERWASH = "{AB8A3CB589625084}Prefabs/DynamicBase/IA_Dressing_WaterWash.et";
	static const ResourceName PREFAB_DRESSING_BULKWATER = "{CE0933F9935A5FD4}Prefabs/DynamicBase/IA_Dressing_BulkWater.et";
	static const ResourceName PREFAB_DRESSING_SANITATION = "{6F3672FEE4B857A1}Prefabs/DynamicBase/IA_Dressing_Sanitation.et";
	static const ResourceName PREFAB_DRESSING_KITCHEN = "{0138CE87F30058BE}Prefabs/DynamicBase/IA_Dressing_Kitchen.et";
	static const ResourceName PREFAB_DRESSING_STORESCOVERED = "{674112EA1BA85649}Prefabs/DynamicBase/IA_Dressing_StoresCovered.et";
	static const ResourceName PREFAB_DRESSING_POWER = "{3F22F8B9E0EB5442}Prefabs/DynamicBase/IA_Dressing_Power.et";
	static const ResourceName PREFAB_DRESSING_MEDICAL = "{AC27F856D7A750A7}Prefabs/DynamicBase/IA_Dressing_Medical.et";
	static const ResourceName PREFAB_DRESSING_REST = "{754823CC537E5D85}Prefabs/DynamicBase/IA_Dressing_Rest.et";
	static const ResourceName PREFAB_DRESSING_COMMS = "{BB26A698BEF55499}Prefabs/DynamicBase/IA_Dressing_Comms.et";
	static const ResourceName PREFAB_DRESSING_WASTE = "{A70917953C1D58AB}Prefabs/DynamicBase/IA_Dressing_Waste.et";
	static const ResourceName PREFAB_DRESSING_ENTRANCELIGHT = "{8726E742FEE65661}Prefabs/DynamicBase/IA_Dressing_EntranceLight.et";
	static const ResourceName PREFAB_DRESSING_BRIEFINGLIT = "{986F4B80B0BE52E0}Prefabs/DynamicBase/IA_Dressing_BriefingLit.et";
	static const ResourceName PREFAB_DRESSING_MESSLIT = "{06111FFC070A5834}Prefabs/DynamicBase/IA_Dressing_MessLit.et";
	static const ResourceName PREFAB_DRESSING_KITCHENLIT = "{7381F407AD8A54B8}Prefabs/DynamicBase/IA_Dressing_KitchenLit.et";
	static const ResourceName PREFAB_DRESSING_WORKSHOPLIT = "{965FEDAFCD3F54D5}Prefabs/DynamicBase/IA_Dressing_WorkshopLit.et";
	static const ResourceName PREFAB_DRESSING_MEDICALLIT = "{A9D76F51AE7B5D37}Prefabs/DynamicBase/IA_Dressing_MedicalLit.et";
	static const ResourceName PREFAB_DRESSING_COMMSLIT = "{D7D10DFBACA05AD0}Prefabs/DynamicBase/IA_Dressing_CommsLit.et";

	int m_iLayoutId;
	bool m_bComposed;
	int m_iDesignVariant = -1;
	string m_sName;
	float m_fHalfWidthM;
	float m_fHalfDepthM;
	float m_fCaptureRadiusM;
	int m_iMaxGarrison = 36;
	vector m_vCaptureLocal;
	vector m_vAssemblyLocal;
	ref array<ref IA_DynamicSiteModule> m_aModules;
	// Kept outside survey modules. Populate only after the exact site is chosen.
	ref array<ref IA_DynamicBaseEmplacementSpec> m_aEmplacements = {};
	protected bool m_bEmplacementsPrepared;

	void PrepareEmplacements()
	{
		if (m_bEmplacementsPrepared || m_bComposed)
			return;
		m_bEmplacementsPrepared = true;
		IA_BaseEmplacementManifest.Populate(this);
	}
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

		layout.AddCover("wall_rear_00", -87.300, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -84.341, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -81.381, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -78.422, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -75.463, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -72.503, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", -69.544, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", -66.585, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", -63.625, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", -60.666, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", -57.707, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", -54.747, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_12", -51.788, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_13", -48.829, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_14", -45.869, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_15", -42.910, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_16", -39.951, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_17", -36.992, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_18", -34.032, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_19", -31.073, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_20", -28.114, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_21", -25.154, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_22", -22.195, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_23", -19.236, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_24", -16.276, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_25", -13.317, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_26", -10.358, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_27", -7.398, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_28", -4.439, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_29", -1.480, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_30", 1.480, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_31", 4.439, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_32", 7.398, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_33", 10.358, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_34", 13.317, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_35", 16.276, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_36", 19.236, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_37", 22.195, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_38", 25.154, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_39", 28.114, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_40", 31.073, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_41", 34.032, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_42", 36.992, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_43", 39.951, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_44", 42.910, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_45", 45.869, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_46", 48.829, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_47", 51.788, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_48", 54.747, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_49", 57.707, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_50", 60.666, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_51", 63.625, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_52", 66.585, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_53", 69.544, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_54", 72.503, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_55", 75.463, 69.100, 0, 2, 0);
		layout.AddCover("wall_rear_56", 78.422, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_57", 81.381, 69.100, 0, 1, 0);
		layout.AddCover("wall_rear_58", 84.341, 69.100, 0, 0, 0);
		layout.AddCover("wall_rear_59", 87.300, 69.100, 0, 2, 0);
		layout.AddCover("wall_frontW_00", -87.300, -69.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -84.356, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -81.411, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -78.467, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -75.522, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_05", -72.578, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_06", -69.633, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_07", -66.689, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_08", -63.744, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_09", -60.800, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_10", -57.856, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_11", -54.911, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_12", -51.967, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_13", -49.022, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_14", -46.078, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_15", -43.133, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_16", -40.189, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_17", -37.244, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_18", -34.300, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_19", -31.356, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_20", -28.411, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_21", -25.467, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_22", -22.522, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_23", -19.578, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontW_24", -16.633, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_25", -13.689, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontW_26", -10.744, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontW_27", -7.800, -69.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 7.800, -69.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 10.744, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 13.689, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 16.633, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 19.578, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_05", 22.522, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_06", 25.467, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_07", 28.411, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_08", 31.356, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_09", 34.300, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_10", 37.244, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_11", 40.189, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_12", 43.133, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_13", 46.078, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_14", 49.022, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_15", 51.967, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_16", 54.911, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_17", 57.856, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_18", 60.800, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_19", 63.744, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_20", 66.689, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_21", 69.633, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_22", 72.578, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_23", 75.522, -69.100, 180, 2, 2);
		layout.AddCover("wall_frontE_24", 78.467, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_25", 81.411, -69.100, 180, 1, 2);
		layout.AddCover("wall_frontE_26", 84.356, -69.100, 180, 0, 2);
		layout.AddCover("wall_frontE_27", 87.300, -69.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -89.100, -67.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -89.100, -64.328, 270, 1, 3);
		layout.AddCover("wall_westS_02", -89.100, -61.356, 270, 0, 3);
		layout.AddCover("wall_westS_03", -89.100, -58.383, 270, 2, 3);
		layout.AddCover("wall_westS_04", -89.100, -55.411, 270, 0, 3);
		layout.AddCover("wall_westS_05", -89.100, -52.439, 270, 1, 3);
		layout.AddCover("wall_westS_06", -89.100, -49.467, 270, 0, 3);
		layout.AddCover("wall_westS_07", -89.100, -46.494, 270, 2, 3);
		layout.AddCover("wall_westS_08", -89.100, -43.522, 270, 0, 3);
		layout.AddCover("wall_westS_09", -89.100, -40.550, 270, 1, 3);
		layout.AddCover("wall_westS_10", -89.100, -37.578, 270, 0, 3);
		layout.AddCover("wall_westS_11", -89.100, -34.606, 270, 2, 3);
		layout.AddCover("wall_westS_12", -89.100, -31.633, 270, 0, 3);
		layout.AddCover("wall_westS_13", -89.100, -28.661, 270, 1, 3);
		layout.AddCover("wall_westS_14", -89.100, -25.689, 270, 0, 3);
		layout.AddCover("wall_westS_15", -89.100, -22.717, 270, 2, 3);
		layout.AddCover("wall_westS_16", -89.100, -19.744, 270, 0, 3);
		layout.AddCover("wall_westS_17", -89.100, -16.772, 270, 1, 3);
		layout.AddCover("wall_westS_18", -89.100, -13.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -89.100, -2.200, 270, 3, 3);
		layout.AddCover("wall_westN_01", -89.100, 0.696, 270, 1, 3);
		layout.AddCover("wall_westN_02", -89.100, 3.592, 270, 0, 3);
		layout.AddCover("wall_westN_03", -89.100, 6.487, 270, 2, 3);
		layout.AddCover("wall_westN_04", -89.100, 9.383, 270, 0, 3);
		layout.AddCover("wall_westN_05", -89.100, 12.279, 270, 1, 3);
		layout.AddCover("wall_westN_06", -89.100, 15.175, 270, 0, 3);
		layout.AddCover("wall_westN_07", -89.100, 18.071, 270, 2, 3);
		layout.AddCover("wall_westN_08", -89.100, 20.967, 270, 0, 3);
		layout.AddCover("wall_westN_09", -89.100, 23.863, 270, 1, 3);
		layout.AddCover("wall_westN_10", -89.100, 26.758, 270, 0, 3);
		layout.AddCover("wall_westN_11", -89.100, 29.654, 270, 2, 3);
		layout.AddCover("wall_westN_12", -89.100, 32.550, 270, 0, 3);
		layout.AddCover("wall_westN_13", -89.100, 35.446, 270, 1, 3);
		layout.AddCover("wall_westN_14", -89.100, 38.342, 270, 0, 3);
		layout.AddCover("wall_westN_15", -89.100, 41.237, 270, 2, 3);
		layout.AddCover("wall_westN_16", -89.100, 44.133, 270, 0, 3);
		layout.AddCover("wall_westN_17", -89.100, 47.029, 270, 1, 3);
		layout.AddCover("wall_westN_18", -89.100, 49.925, 270, 0, 3);
		layout.AddCover("wall_westN_19", -89.100, 52.821, 270, 2, 3);
		layout.AddCover("wall_westN_20", -89.100, 55.717, 270, 0, 3);
		layout.AddCover("wall_westN_21", -89.100, 58.612, 270, 1, 3);
		layout.AddCover("wall_westN_22", -89.100, 61.508, 270, 0, 3);
		layout.AddCover("wall_westN_23", -89.100, 64.404, 270, 2, 3);
		layout.AddCover("wall_westN_24", -89.100, 67.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 89.100, -67.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 89.100, -64.328, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 89.100, -61.356, 90, 0, 1);
		layout.AddCover("wall_eastS_03", 89.100, -58.383, 90, 2, 1);
		layout.AddCover("wall_eastS_04", 89.100, -55.411, 90, 0, 1);
		layout.AddCover("wall_eastS_05", 89.100, -52.439, 90, 1, 1);
		layout.AddCover("wall_eastS_06", 89.100, -49.467, 90, 0, 1);
		layout.AddCover("wall_eastS_07", 89.100, -46.494, 90, 2, 1);
		layout.AddCover("wall_eastS_08", 89.100, -43.522, 90, 0, 1);
		layout.AddCover("wall_eastS_09", 89.100, -40.550, 90, 1, 1);
		layout.AddCover("wall_eastS_10", 89.100, -37.578, 90, 0, 1);
		layout.AddCover("wall_eastS_11", 89.100, -34.606, 90, 2, 1);
		layout.AddCover("wall_eastS_12", 89.100, -31.633, 90, 0, 1);
		layout.AddCover("wall_eastS_13", 89.100, -28.661, 90, 1, 1);
		layout.AddCover("wall_eastS_14", 89.100, -25.689, 90, 0, 1);
		layout.AddCover("wall_eastS_15", 89.100, -22.717, 90, 2, 1);
		layout.AddCover("wall_eastS_16", 89.100, -19.744, 90, 0, 1);
		layout.AddCover("wall_eastS_17", 89.100, -16.772, 90, 1, 1);
		layout.AddCover("wall_eastS_18", 89.100, -13.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 89.100, -2.200, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 89.100, 0.696, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 89.100, 3.592, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 89.100, 6.487, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 89.100, 9.383, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 89.100, 12.279, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 89.100, 15.175, 90, 0, 1);
		layout.AddCover("wall_eastN_07", 89.100, 18.071, 90, 2, 1);
		layout.AddCover("wall_eastN_08", 89.100, 20.967, 90, 0, 1);
		layout.AddCover("wall_eastN_09", 89.100, 23.863, 90, 1, 1);
		layout.AddCover("wall_eastN_10", 89.100, 26.758, 90, 0, 1);
		layout.AddCover("wall_eastN_11", 89.100, 29.654, 90, 2, 1);
		layout.AddCover("wall_eastN_12", 89.100, 32.550, 90, 0, 1);
		layout.AddCover("wall_eastN_13", 89.100, 35.446, 90, 1, 1);
		layout.AddCover("wall_eastN_14", 89.100, 38.342, 90, 0, 1);
		layout.AddCover("wall_eastN_15", 89.100, 41.237, 90, 2, 1);
		layout.AddCover("wall_eastN_16", 89.100, 44.133, 90, 0, 1);
		layout.AddCover("wall_eastN_17", 89.100, 47.029, 90, 1, 1);
		layout.AddCover("wall_eastN_18", 89.100, 49.925, 90, 0, 1);
		layout.AddCover("wall_eastN_19", 89.100, 52.821, 90, 2, 1);
		layout.AddCover("wall_eastN_20", 89.100, 55.717, 90, 0, 1);
		layout.AddCover("wall_eastN_21", 89.100, 58.612, 90, 1, 1);
		layout.AddCover("wall_eastN_22", 89.100, 61.508, 90, 0, 1);
		layout.AddCover("wall_eastN_23", 89.100, 64.404, 90, 2, 1);
		layout.AddCover("wall_eastN_24", 89.100, 67.300, 90, 3, 1);

		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_briefinglit_0", PREFAB_DRESSING_BRIEFINGLIT, 14, 49, 0, 2.3, 2.6, 9);
		layout.AddDressing("dressing_kitchenlit_1", PREFAB_DRESSING_KITCHENLIT, -47, 53, 0, 3.5, 3.2, 6);
		layout.AddDressing("dressing_mess_2", PREFAB_DRESSING_MESS, -46, -2, 0, 1.7, 1.5, 7);
		layout.AddDressing("dressing_bulkwater_3", PREFAB_DRESSING_BULKWATER, -78, 32, 0, 2.2, 3.4, 4);
		layout.AddDressing("dressing_storescovered_4", PREFAB_DRESSING_STORESCOVERED, 45, -26, 0, 1.9, 1.6, 6);
		layout.AddDressing("dressing_stores_5", PREFAB_DRESSING_STORES, 30, -48, 0, 1.8, 2.9, 6);
		layout.AddDressing("dressing_workshoplit_6", PREFAB_DRESSING_WORKSHOPLIT, -67, -22, 0, 2, 1.3, 7);
		layout.AddDressing("dressing_commslit_7", PREFAB_DRESSING_COMMSLIT, 28, 57, 0, 2, 1.3, 5);
		layout.AddDressing("dressing_medicallit_8", PREFAB_DRESSING_MEDICALLIT, 78, 33, 0, 2, 2, 6);
		layout.AddDressing("dressing_power_9", PREFAB_DRESSING_POWER, 72, -32, 0, 2, 2.5, 7);
		layout.AddDressing("dressing_storescovered_10", PREFAB_DRESSING_STORESCOVERED, -30, -58, 0, 1.9, 1.6, 6);
		layout.AddDressing("dressing_rest_11", PREFAB_DRESSING_REST, -15, 43, 90, 2, 1.8, 4);
		layout.AddDressing("dressing_sanitation_12", PREFAB_DRESSING_SANITATION, -78, -3, 90, 2.4, 2.8, 5);
		layout.AddDressing("dressing_sanitation_13", PREFAB_DRESSING_SANITATION, 78, 3, 0, 2.4, 2.8, 5);
		layout.AddDressing("dressing_waste_14", PREFAB_DRESSING_WASTE, 76, -62, 0, 1, 0.7, 3);
		layout.AddDressing("dressing_entrancelight_15", PREFAB_DRESSING_ENTRANCELIGHT, 9, -60, 0, 2, 1.5, 2);
		// INTERIOR DRESSING END
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

		layout.AddCover("wall_rear_00", -57.300, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -54.362, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -51.423, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -48.485, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -45.546, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -42.608, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", -39.669, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", -36.731, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", -33.792, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", -30.854, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", -27.915, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", -24.977, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_12", -22.038, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_13", -19.100, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_14", -16.162, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_15", -13.223, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_16", -10.285, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_17", -7.346, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_18", -4.408, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_19", -1.469, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_20", 1.469, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_21", 4.408, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_22", 7.346, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_23", 10.285, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_24", 13.223, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_25", 16.162, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_26", 19.100, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_27", 22.038, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_28", 24.977, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_29", 27.915, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_30", 30.854, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_31", 33.792, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_32", 36.731, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_33", 39.669, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_34", 42.608, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_35", 45.546, 49.100, 0, 2, 0);
		layout.AddCover("wall_rear_36", 48.485, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_37", 51.423, 49.100, 0, 1, 0);
		layout.AddCover("wall_rear_38", 54.362, 49.100, 0, 0, 0);
		layout.AddCover("wall_rear_39", 57.300, 49.100, 0, 2, 0);
		layout.AddCover("wall_frontW_00", -57.300, -49.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -54.388, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -51.476, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -48.565, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -45.653, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_05", -42.741, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontW_06", -39.829, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_07", -36.918, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontW_08", -34.006, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_09", -31.094, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontW_10", -28.182, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_11", -25.271, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontW_12", -22.359, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_13", -19.447, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontW_14", -16.535, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_15", -13.624, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontW_16", -10.712, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontW_17", -7.800, -49.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 7.800, -49.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 10.712, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 13.624, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 16.535, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 19.447, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_05", 22.359, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontE_06", 25.271, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_07", 28.182, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontE_08", 31.094, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_09", 34.006, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontE_10", 36.918, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_11", 39.829, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontE_12", 42.741, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_13", 45.653, -49.100, 180, 1, 2);
		layout.AddCover("wall_frontE_14", 48.565, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_15", 51.476, -49.100, 180, 2, 2);
		layout.AddCover("wall_frontE_16", 54.388, -49.100, 180, 0, 2);
		layout.AddCover("wall_frontE_17", 57.300, -49.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -59.100, -47.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -59.100, -44.492, 270, 1, 3);
		layout.AddCover("wall_westS_02", -59.100, -41.685, 270, 0, 3);
		layout.AddCover("wall_westS_03", -59.100, -38.877, 270, 2, 3);
		layout.AddCover("wall_westS_04", -59.100, -36.069, 270, 0, 3);
		layout.AddCover("wall_westS_05", -59.100, -33.262, 270, 1, 3);
		layout.AddCover("wall_westS_06", -59.100, -30.454, 270, 0, 3);
		layout.AddCover("wall_westS_07", -59.100, -27.646, 270, 2, 3);
		layout.AddCover("wall_westS_08", -59.100, -24.838, 270, 0, 3);
		layout.AddCover("wall_westS_09", -59.100, -22.031, 270, 1, 3);
		layout.AddCover("wall_westS_10", -59.100, -19.223, 270, 0, 3);
		layout.AddCover("wall_westS_11", -59.100, -16.415, 270, 2, 3);
		layout.AddCover("wall_westS_12", -59.100, -13.608, 270, 0, 3);
		layout.AddCover("wall_westS_13", -59.100, -10.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -59.100, 0.800, 270, 3, 3);
		layout.AddCover("wall_westN_01", -59.100, 3.706, 270, 1, 3);
		layout.AddCover("wall_westN_02", -59.100, 6.613, 270, 0, 3);
		layout.AddCover("wall_westN_03", -59.100, 9.519, 270, 2, 3);
		layout.AddCover("wall_westN_04", -59.100, 12.425, 270, 0, 3);
		layout.AddCover("wall_westN_05", -59.100, 15.331, 270, 1, 3);
		layout.AddCover("wall_westN_06", -59.100, 18.238, 270, 0, 3);
		layout.AddCover("wall_westN_07", -59.100, 21.144, 270, 2, 3);
		layout.AddCover("wall_westN_08", -59.100, 24.050, 270, 0, 3);
		layout.AddCover("wall_westN_09", -59.100, 26.956, 270, 1, 3);
		layout.AddCover("wall_westN_10", -59.100, 29.863, 270, 0, 3);
		layout.AddCover("wall_westN_11", -59.100, 32.769, 270, 2, 3);
		layout.AddCover("wall_westN_12", -59.100, 35.675, 270, 0, 3);
		layout.AddCover("wall_westN_13", -59.100, 38.581, 270, 1, 3);
		layout.AddCover("wall_westN_14", -59.100, 41.488, 270, 0, 3);
		layout.AddCover("wall_westN_15", -59.100, 44.394, 270, 2, 3);
		layout.AddCover("wall_westN_16", -59.100, 47.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 59.100, -47.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 59.100, -44.492, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 59.100, -41.685, 90, 0, 1);
		layout.AddCover("wall_eastS_03", 59.100, -38.877, 90, 2, 1);
		layout.AddCover("wall_eastS_04", 59.100, -36.069, 90, 0, 1);
		layout.AddCover("wall_eastS_05", 59.100, -33.262, 90, 1, 1);
		layout.AddCover("wall_eastS_06", 59.100, -30.454, 90, 0, 1);
		layout.AddCover("wall_eastS_07", 59.100, -27.646, 90, 2, 1);
		layout.AddCover("wall_eastS_08", 59.100, -24.838, 90, 0, 1);
		layout.AddCover("wall_eastS_09", 59.100, -22.031, 90, 1, 1);
		layout.AddCover("wall_eastS_10", 59.100, -19.223, 90, 0, 1);
		layout.AddCover("wall_eastS_11", 59.100, -16.415, 90, 2, 1);
		layout.AddCover("wall_eastS_12", 59.100, -13.608, 90, 0, 1);
		layout.AddCover("wall_eastS_13", 59.100, -10.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 59.100, 0.800, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 59.100, 3.706, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 59.100, 6.613, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 59.100, 9.519, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 59.100, 12.425, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 59.100, 15.331, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 59.100, 18.238, 90, 0, 1);
		layout.AddCover("wall_eastN_07", 59.100, 21.144, 90, 2, 1);
		layout.AddCover("wall_eastN_08", 59.100, 24.050, 90, 0, 1);
		layout.AddCover("wall_eastN_09", 59.100, 26.956, 90, 1, 1);
		layout.AddCover("wall_eastN_10", 59.100, 29.863, 90, 0, 1);
		layout.AddCover("wall_eastN_11", 59.100, 32.769, 90, 2, 1);
		layout.AddCover("wall_eastN_12", 59.100, 35.675, 90, 0, 1);
		layout.AddCover("wall_eastN_13", 59.100, 38.581, 90, 1, 1);
		layout.AddCover("wall_eastN_14", 59.100, 41.488, 90, 0, 1);
		layout.AddCover("wall_eastN_15", 59.100, 44.394, 90, 2, 1);
		layout.AddCover("wall_eastN_16", 59.100, 47.300, 90, 3, 1);

		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_briefinglit_0", PREFAB_DRESSING_BRIEFINGLIT, 16, 30, 0, 2.3, 2.6, 9);
		layout.AddDressing("dressing_kitchenlit_1", PREFAB_DRESSING_KITCHENLIT, -21, 27, 0, 3.5, 3.2, 6);
		layout.AddDressing("dressing_bulkwater_2", PREFAB_DRESSING_BULKWATER, -49, 10, 0, 2.2, 3.4, 4);
		layout.AddDressing("dressing_storescovered_3", PREFAB_DRESSING_STORESCOVERED, 24, -39, 0, 1.9, 1.6, 6);
		layout.AddDressing("dressing_workshoplit_4", PREFAB_DRESSING_WORKSHOPLIT, -15, -27, 0, 2, 1.3, 7);
		layout.AddDressing("dressing_power_5", PREFAB_DRESSING_POWER, 42, -21, 0, 2, 2.5, 7);
		layout.AddDressing("dressing_rest_6", PREFAB_DRESSING_REST, -34, 43, 0, 2, 1.8, 4);
		layout.AddDressing("dressing_medical_7", PREFAB_DRESSING_MEDICAL, 42, 8, 0, 2, 2, 5);
		layout.AddDressing("dressing_comms_8", PREFAB_DRESSING_COMMS, 29, 42, 0, 2, 1.3, 4);
		layout.AddDressing("dressing_sanitation_9", PREFAB_DRESSING_SANITATION, -49, -13, 0, 2.4, 2.8, 5);
		layout.AddDressing("dressing_waste_10", PREFAB_DRESSING_WASTE, 39, -40, 0, 1, 0.7, 3);
		layout.AddDressing("dressing_entrancelight_11", PREFAB_DRESSING_ENTRANCELIGHT, 9, -42, 0, 2, 1.5, 2);
		// INTERIOR DRESSING END
		return layout;
	}

	//------------------------------------------------------------------------------------------------
	static IA_DynamicSiteLayout CreateCourtyard()
	{
		ref IA_DynamicSiteLayout layout = CreateSmallLayout(LAYOUT_COURTYARD, "Courtyard", 44, 38, 22, Vector(0, 0, 10), -2, 24);
		// Command tent at the rear; sleeping quarters flank its forecourt.
		// Medical and supply tents face each other across the main approach.
		layout.AddModule("hq", PREFAB_HQ, 0, 24, 180, 12, 10, IA_DynamicSiteModuleRole.Hq, 0.35, 45);
		layout.AddModule("barracksW", PREFAB_BARRACKS, -27, 17, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 55);
		layout.AddModule("barracksE", PREFAB_BARRACKS, 27, 17, 270, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 55);
		layout.AddModule("medical", PREFAB_MEDICAL, -24, -22, 90, 14, 14, IA_DynamicSiteModuleRole.Medical, 0.35, 34);
		layout.AddModule("supply", PREFAB_SUPPLY, 25, -22, 270, 11, 11, IA_DynamicSiteModuleRole.Supply, 0.35, 24);

		layout.AddCover("wall_rear_00", -41.300, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -38.350, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -35.400, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -32.450, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -29.500, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -26.550, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", -23.600, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", -20.650, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", -17.700, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", -14.750, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", -11.800, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", -8.850, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_12", -5.900, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_13", -2.950, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_14", 0.000, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_15", 2.950, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_16", 5.900, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_17", 8.850, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_18", 11.800, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_19", 14.750, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_20", 17.700, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_21", 20.650, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_22", 23.600, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_23", 26.550, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_24", 29.500, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_25", 32.450, 37.100, 0, 1, 0);
		layout.AddCover("wall_rear_26", 35.400, 37.100, 0, 0, 0);
		layout.AddCover("wall_rear_27", 38.350, 37.100, 0, 2, 0);
		layout.AddCover("wall_rear_28", 41.300, 37.100, 0, 0, 0);
		layout.AddCover("wall_frontW_00", -41.300, -37.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -38.342, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -35.383, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -32.425, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -29.467, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontW_05", -26.508, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontW_06", -23.550, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontW_07", -20.592, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontW_08", -17.633, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontW_09", -14.675, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontW_10", -11.717, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontW_11", -8.758, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontW_12", -5.800, -37.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 5.800, -37.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 8.758, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 11.717, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 14.675, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 17.633, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontE_05", 20.592, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontE_06", 23.550, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontE_07", 26.508, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontE_08", 29.467, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontE_09", 32.425, -37.100, 180, 1, 2);
		layout.AddCover("wall_frontE_10", 35.383, -37.100, 180, 0, 2);
		layout.AddCover("wall_frontE_11", 38.342, -37.100, 180, 2, 2);
		layout.AddCover("wall_frontE_12", 41.300, -37.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -43.100, -35.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -43.100, -32.550, 270, 1, 3);
		layout.AddCover("wall_westS_02", -43.100, -29.800, 270, 0, 3);
		layout.AddCover("wall_westS_03", -43.100, -27.050, 270, 2, 3);
		layout.AddCover("wall_westS_04", -43.100, -24.300, 270, 0, 3);
		layout.AddCover("wall_westS_05", -43.100, -21.550, 270, 1, 3);
		layout.AddCover("wall_westS_06", -43.100, -18.800, 270, 0, 3);
		layout.AddCover("wall_westS_07", -43.100, -16.050, 270, 2, 3);
		layout.AddCover("wall_westS_08", -43.100, -13.300, 270, 0, 3);
		layout.AddCover("wall_westS_09", -43.100, -10.550, 270, 1, 3);
		layout.AddCover("wall_westS_10", -43.100, -7.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -43.100, 3.800, 270, 3, 3);
		layout.AddCover("wall_westN_01", -43.100, 6.664, 270, 1, 3);
		layout.AddCover("wall_westN_02", -43.100, 9.527, 270, 0, 3);
		layout.AddCover("wall_westN_03", -43.100, 12.391, 270, 2, 3);
		layout.AddCover("wall_westN_04", -43.100, 15.255, 270, 0, 3);
		layout.AddCover("wall_westN_05", -43.100, 18.118, 270, 1, 3);
		layout.AddCover("wall_westN_06", -43.100, 20.982, 270, 0, 3);
		layout.AddCover("wall_westN_07", -43.100, 23.845, 270, 2, 3);
		layout.AddCover("wall_westN_08", -43.100, 26.709, 270, 0, 3);
		layout.AddCover("wall_westN_09", -43.100, 29.573, 270, 1, 3);
		layout.AddCover("wall_westN_10", -43.100, 32.436, 270, 0, 3);
		layout.AddCover("wall_westN_11", -43.100, 35.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 43.100, -35.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 43.100, -32.550, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 43.100, -29.800, 90, 0, 1);
		layout.AddCover("wall_eastS_03", 43.100, -27.050, 90, 2, 1);
		layout.AddCover("wall_eastS_04", 43.100, -24.300, 90, 0, 1);
		layout.AddCover("wall_eastS_05", 43.100, -21.550, 90, 1, 1);
		layout.AddCover("wall_eastS_06", 43.100, -18.800, 90, 0, 1);
		layout.AddCover("wall_eastS_07", 43.100, -16.050, 90, 2, 1);
		layout.AddCover("wall_eastS_08", 43.100, -13.300, 90, 0, 1);
		layout.AddCover("wall_eastS_09", 43.100, -10.550, 90, 1, 1);
		layout.AddCover("wall_eastS_10", 43.100, -7.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 43.100, 3.800, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 43.100, 6.664, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 43.100, 9.527, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 43.100, 12.391, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 43.100, 15.255, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 43.100, 18.118, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 43.100, 20.982, 90, 0, 1);
		layout.AddCover("wall_eastN_07", 43.100, 23.845, 90, 2, 1);
		layout.AddCover("wall_eastN_08", 43.100, 26.709, 90, 0, 1);
		layout.AddCover("wall_eastN_09", 43.100, 29.573, 90, 1, 1);
		layout.AddCover("wall_eastN_10", 43.100, 32.436, 90, 0, 1);
		layout.AddCover("wall_eastN_11", 43.100, 35.300, 90, 3, 1);

		layout.m_aGuardPosts.Insert(Vector(0, 0, 10));
		layout.m_aGuardPosts.Insert(Vector(0, 0, -30));
		layout.m_aGuardPosts.Insert(Vector(-36, 0, -2));
		layout.m_aGuardPosts.Insert(Vector(36, 0, -2));
		layout.m_aGuardPosts.Insert(Vector(-15, 0, 33));
		layout.m_aGuardPosts.Insert(Vector(-7, 0, -20));
		layout.m_aGuardPosts.Insert(Vector(10, 0, -20));
		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_briefinglit_0", PREFAB_DRESSING_BRIEFINGLIT, 16, 30, 0, 2.3, 2.6, 9);
		layout.AddDressing("dressing_kitchenlit_1", PREFAB_DRESSING_KITCHENLIT, -25, 31, 0, 3.5, 3.2, 6);
		layout.AddDressing("dressing_waterwash_2", PREFAB_DRESSING_WATERWASH, -6, -25, 0, 1.4, 1.2, 5);
		layout.AddDressing("dressing_storescovered_3", PREFAB_DRESSING_STORESCOVERED, 26, -7, 0, 1.9, 1.6, 6);
		layout.AddDressing("dressing_power_4", PREFAB_DRESSING_POWER, 39, -25, 0, 2, 2.5, 7);
		layout.AddDressing("dressing_medical_5", PREFAB_DRESSING_MEDICAL, -7, -16, 0, 2, 2, 5);
		layout.AddDressing("dressing_sanitation_6", PREFAB_DRESSING_SANITATION, -37, 31, 0, 2.4, 2.8, 5);
		layout.AddDressing("dressing_waste_7", PREFAB_DRESSING_WASTE, 37, 30, 0, 1, 0.7, 3);
		layout.AddDressing("dressing_entrancelight_8", PREFAB_DRESSING_ENTRANCELIGHT, 9, -29, 0, 2, 1.5, 2);
		// INTERIOR DRESSING END
		return layout;
	}

	static IA_DynamicSiteLayout CreateRoadside()
	{
		ref IA_DynamicSiteLayout layout = CreateSmallLayout(LAYOUT_ROADSIDE, "Roadside", 30, 48, 20, Vector(0, 0, 19), -10, 20);
		// Two rows along a clear central lane fit a long, narrow clearing.
		// "Roadside" describes the shape; placement still requires safe terrain.
		layout.AddModule("hq", PREFAB_HQ, 0, 32, 180, 12, 10, IA_DynamicSiteModuleRole.Hq, 0.35, 45);
		layout.AddModule("barracksN", PREFAB_BARRACKS, -17, 7, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 55);
		layout.AddModule("supply", PREFAB_SUPPLY, 17, 7, 270, 11, 11, IA_DynamicSiteModuleRole.Supply, 0.35, 24);
		layout.AddModule("barracksS", PREFAB_BARRACKS, -17, -26, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 55);
		layout.AddModule("fuel", PREFAB_FUEL, 17, -26, 0, 11, 11, IA_DynamicSiteModuleRole.Fuel, 0.50, 14);

		layout.AddCover("wall_rear_00", -27.300, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -24.426, 47.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -21.553, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -18.679, 47.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -15.805, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -12.932, 47.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", -10.058, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", -7.184, 47.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", -4.311, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", -1.437, 47.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", 1.437, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", 4.311, 47.100, 0, 2, 0);
		layout.AddCover("wall_rear_12", 7.184, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_13", 10.058, 47.100, 0, 1, 0);
		layout.AddCover("wall_rear_14", 12.932, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_15", 15.805, 47.100, 0, 2, 0);
		layout.AddCover("wall_rear_16", 18.679, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_17", 21.553, 47.100, 0, 1, 0);
		layout.AddCover("wall_rear_18", 24.426, 47.100, 0, 0, 0);
		layout.AddCover("wall_rear_19", 27.300, 47.100, 0, 2, 0);
		layout.AddCover("wall_frontW_00", -27.300, -47.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -24.613, -47.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -21.925, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -19.238, -47.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -16.550, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontW_05", -13.863, -47.100, 180, 1, 2);
		layout.AddCover("wall_frontW_06", -11.175, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontW_07", -8.488, -47.100, 180, 2, 2);
		layout.AddCover("wall_frontW_08", -5.800, -47.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 5.800, -47.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 8.488, -47.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 11.175, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 13.863, -47.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 16.550, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontE_05", 19.238, -47.100, 180, 1, 2);
		layout.AddCover("wall_frontE_06", 21.925, -47.100, 180, 0, 2);
		layout.AddCover("wall_frontE_07", 24.613, -47.100, 180, 2, 2);
		layout.AddCover("wall_frontE_08", 27.300, -47.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -29.100, -45.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -29.100, -42.350, 270, 1, 3);
		layout.AddCover("wall_westS_02", -29.100, -39.400, 270, 0, 3);
		layout.AddCover("wall_westS_03", -29.100, -36.450, 270, 2, 3);
		layout.AddCover("wall_westS_04", -29.100, -33.500, 270, 0, 3);
		layout.AddCover("wall_westS_05", -29.100, -30.550, 270, 1, 3);
		layout.AddCover("wall_westS_06", -29.100, -27.600, 270, 0, 3);
		layout.AddCover("wall_westS_07", -29.100, -24.650, 270, 2, 3);
		layout.AddCover("wall_westS_08", -29.100, -21.700, 270, 0, 3);
		layout.AddCover("wall_westS_09", -29.100, -18.750, 270, 1, 3);
		layout.AddCover("wall_westS_10", -29.100, -15.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -29.100, -4.200, 270, 3, 3);
		layout.AddCover("wall_westN_01", -29.100, -1.288, 270, 1, 3);
		layout.AddCover("wall_westN_02", -29.100, 1.624, 270, 0, 3);
		layout.AddCover("wall_westN_03", -29.100, 4.535, 270, 2, 3);
		layout.AddCover("wall_westN_04", -29.100, 7.447, 270, 0, 3);
		layout.AddCover("wall_westN_05", -29.100, 10.359, 270, 1, 3);
		layout.AddCover("wall_westN_06", -29.100, 13.271, 270, 0, 3);
		layout.AddCover("wall_westN_07", -29.100, 16.182, 270, 2, 3);
		layout.AddCover("wall_westN_08", -29.100, 19.094, 270, 0, 3);
		layout.AddCover("wall_westN_09", -29.100, 22.006, 270, 1, 3);
		layout.AddCover("wall_westN_10", -29.100, 24.918, 270, 0, 3);
		layout.AddCover("wall_westN_11", -29.100, 27.829, 270, 2, 3);
		layout.AddCover("wall_westN_12", -29.100, 30.741, 270, 0, 3);
		layout.AddCover("wall_westN_13", -29.100, 33.653, 270, 1, 3);
		layout.AddCover("wall_westN_14", -29.100, 36.565, 270, 0, 3);
		layout.AddCover("wall_westN_15", -29.100, 39.476, 270, 2, 3);
		layout.AddCover("wall_westN_16", -29.100, 42.388, 270, 0, 3);
		layout.AddCover("wall_westN_17", -29.100, 45.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 29.100, -45.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 29.100, -42.350, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 29.100, -39.400, 90, 0, 1);
		layout.AddCover("wall_eastS_03", 29.100, -36.450, 90, 2, 1);
		layout.AddCover("wall_eastS_04", 29.100, -33.500, 90, 0, 1);
		layout.AddCover("wall_eastS_05", 29.100, -30.550, 90, 1, 1);
		layout.AddCover("wall_eastS_06", 29.100, -27.600, 90, 0, 1);
		layout.AddCover("wall_eastS_07", 29.100, -24.650, 90, 2, 1);
		layout.AddCover("wall_eastS_08", 29.100, -21.700, 90, 0, 1);
		layout.AddCover("wall_eastS_09", 29.100, -18.750, 90, 1, 1);
		layout.AddCover("wall_eastS_10", 29.100, -15.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 29.100, -4.200, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 29.100, -1.288, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 29.100, 1.624, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 29.100, 4.535, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 29.100, 7.447, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 29.100, 10.359, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 29.100, 13.271, 90, 0, 1);
		layout.AddCover("wall_eastN_07", 29.100, 16.182, 90, 2, 1);
		layout.AddCover("wall_eastN_08", 29.100, 19.094, 90, 0, 1);
		layout.AddCover("wall_eastN_09", 29.100, 22.006, 90, 1, 1);
		layout.AddCover("wall_eastN_10", 29.100, 24.918, 90, 0, 1);
		layout.AddCover("wall_eastN_11", 29.100, 27.829, 90, 2, 1);
		layout.AddCover("wall_eastN_12", 29.100, 30.741, 90, 0, 1);
		layout.AddCover("wall_eastN_13", 29.100, 33.653, 90, 1, 1);
		layout.AddCover("wall_eastN_14", 29.100, 36.565, 90, 0, 1);
		layout.AddCover("wall_eastN_15", 29.100, 39.476, 90, 2, 1);
		layout.AddCover("wall_eastN_16", 29.100, 42.388, 90, 0, 1);
		layout.AddCover("wall_eastN_17", 29.100, 45.300, 90, 3, 1);

		layout.m_aGuardPosts.Insert(Vector(0, 0, 19));
		layout.m_aGuardPosts.Insert(Vector(-7, 0, -41));
		layout.m_aGuardPosts.Insert(Vector(7, 0, -41));
		layout.m_aGuardPosts.Insert(Vector(0, 0, -24));
		layout.m_aGuardPosts.Insert(Vector(0, 0, 6));
		layout.m_aGuardPosts.Insert(Vector(0, 0, 44.7));
		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_briefinglit_0", PREFAB_DRESSING_BRIEFINGLIT, 17, 33, 0, 2.3, 2.6, 9);
		layout.AddDressing("dressing_mess_1", PREFAB_DRESSING_MESS, -17, 23, 0, 1.7, 1.5, 7);
		layout.AddDressing("dressing_waterwash_2", PREFAB_DRESSING_WATERWASH, -17, -6, 0, 1.4, 1.2, 5);
		layout.AddDressing("dressing_storescovered_3", PREFAB_DRESSING_STORESCOVERED, 18, 22, 0, 1.9, 1.6, 6);
		layout.AddDressing("dressing_power_4", PREFAB_DRESSING_POWER, 17, -40, 90, 2, 2.5, 7);
		layout.AddDressing("dressing_workshoplit_5", PREFAB_DRESSING_WORKSHOPLIT, -17, -38, 0, 2, 1.3, 7);
		layout.AddDressing("dressing_sanitation_6", PREFAB_DRESSING_SANITATION, -22, 38, 0, 2.4, 2.8, 5);
		layout.AddDressing("dressing_waste_7", PREFAB_DRESSING_WASTE, 20, -6, 0, 1, 0.7, 3);
		layout.AddDressing("dressing_entrancelight_8", PREFAB_DRESSING_ENTRANCELIGHT, 11, -40, 0, 2, 1.5, 2);
		// INTERIOR DRESSING END
		return layout;
	}

	static IA_DynamicSiteLayout CreateCommandPost()
	{
		ref IA_DynamicSiteLayout layout = CreateSmallLayout(LAYOUT_COMMAND_POST, "Command post", 32, 28, 18, Vector(0, 0, 0), 2, 16);
		// Three full-size tents form a U around the command yard. Omit the
		// workshop and fuel compound instead of shrinking or overlapping buildings.
		layout.AddModule("hq", PREFAB_HQ, 0, 15, 180, 12, 10, IA_DynamicSiteModuleRole.Hq, 0.35, 45);
		layout.AddModule("barracks", PREFAB_BARRACKS, -19, -12, 90, 9, 9, IA_DynamicSiteModuleRole.Barracks, 0.35, 55);
		layout.AddModule("supply", PREFAB_SUPPLY, 19, -12, 270, 11, 11, IA_DynamicSiteModuleRole.Supply, 0.35, 24);

		layout.AddCover("wall_rear_00", -29.300, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -26.370, 27.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -23.440, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -20.510, 27.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -17.580, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -14.650, 27.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", -11.720, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", -8.790, 27.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", -5.860, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", -2.930, 27.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", 0.000, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", 2.930, 27.100, 0, 2, 0);
		layout.AddCover("wall_rear_12", 5.860, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_13", 8.790, 27.100, 0, 1, 0);
		layout.AddCover("wall_rear_14", 11.720, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_15", 14.650, 27.100, 0, 2, 0);
		layout.AddCover("wall_rear_16", 17.580, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_17", 20.510, 27.100, 0, 1, 0);
		layout.AddCover("wall_rear_18", 23.440, 27.100, 0, 0, 0);
		layout.AddCover("wall_rear_19", 26.370, 27.100, 0, 2, 0);
		layout.AddCover("wall_rear_20", 29.300, 27.100, 0, 0, 0);
		layout.AddCover("wall_frontW_00", -29.300, -27.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -26.363, -27.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -23.425, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -20.488, -27.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -17.550, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontW_05", -14.613, -27.100, 180, 1, 2);
		layout.AddCover("wall_frontW_06", -11.675, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontW_07", -8.738, -27.100, 180, 2, 2);
		layout.AddCover("wall_frontW_08", -5.800, -27.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 5.800, -27.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 8.738, -27.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 11.675, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 14.613, -27.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 17.550, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontE_05", 20.488, -27.100, 180, 1, 2);
		layout.AddCover("wall_frontE_06", 23.425, -27.100, 180, 0, 2);
		layout.AddCover("wall_frontE_07", 26.363, -27.100, 180, 2, 2);
		layout.AddCover("wall_frontE_08", 29.300, -27.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -31.100, -25.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -31.100, -22.613, 270, 1, 3);
		layout.AddCover("wall_westS_02", -31.100, -19.925, 270, 0, 3);
		layout.AddCover("wall_westS_03", -31.100, -17.238, 270, 2, 3);
		layout.AddCover("wall_westS_04", -31.100, -14.550, 270, 0, 3);
		layout.AddCover("wall_westS_05", -31.100, -11.863, 270, 1, 3);
		layout.AddCover("wall_westS_06", -31.100, -9.175, 270, 0, 3);
		layout.AddCover("wall_westS_07", -31.100, -6.488, 270, 2, 3);
		layout.AddCover("wall_westS_08", -31.100, -3.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -31.100, 7.800, 270, 3, 3);
		layout.AddCover("wall_westN_01", -31.100, 10.717, 270, 1, 3);
		layout.AddCover("wall_westN_02", -31.100, 13.633, 270, 0, 3);
		layout.AddCover("wall_westN_03", -31.100, 16.550, 270, 2, 3);
		layout.AddCover("wall_westN_04", -31.100, 19.467, 270, 0, 3);
		layout.AddCover("wall_westN_05", -31.100, 22.383, 270, 1, 3);
		layout.AddCover("wall_westN_06", -31.100, 25.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 31.100, -25.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 31.100, -22.613, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 31.100, -19.925, 90, 0, 1);
		layout.AddCover("wall_eastS_03", 31.100, -17.238, 90, 2, 1);
		layout.AddCover("wall_eastS_04", 31.100, -14.550, 90, 0, 1);
		layout.AddCover("wall_eastS_05", 31.100, -11.863, 90, 1, 1);
		layout.AddCover("wall_eastS_06", 31.100, -9.175, 90, 0, 1);
		layout.AddCover("wall_eastS_07", 31.100, -6.488, 90, 2, 1);
		layout.AddCover("wall_eastS_08", 31.100, -3.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 31.100, 7.800, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 31.100, 10.717, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 31.100, 13.633, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 31.100, 16.550, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 31.100, 19.467, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 31.100, 22.383, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 31.100, 25.300, 90, 3, 1);

		layout.m_aGuardPosts.Insert(Vector(0, 0, 0));
		layout.m_aGuardPosts.Insert(Vector(-6, 0, -23));
		layout.m_aGuardPosts.Insert(Vector(6, 0, -23));
		layout.m_aGuardPosts.Insert(Vector(-20, 0, 14));
		layout.m_aGuardPosts.Insert(Vector(20, 0, 14));
		layout.m_aGuardPosts.Insert(Vector(-15, 0, 24));
		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_briefinglit_0", PREFAB_DRESSING_BRIEFINGLIT, 18, 19, 0, 2.3, 2.6, 9);
		layout.AddDressing("dressing_mess_1", PREFAB_DRESSING_MESS, -19, 8, 0, 1.7, 1.5, 7);
		layout.AddDressing("dressing_waterwash_2", PREFAB_DRESSING_WATERWASH, -7, -20, 0, 1.4, 1.2, 5);
		layout.AddDressing("dressing_stores_3", PREFAB_DRESSING_STORES, 19, 7, 90, 1.8, 2.9, 6);
		layout.AddDressing("dressing_power_4", PREFAB_DRESSING_POWER, 25, 7, 0, 2, 2.5, 7);
		layout.AddDressing("dressing_comms_5", PREFAB_DRESSING_COMMS, 15, 13, 90, 2, 1.3, 4);
		layout.AddDressing("dressing_sanitation_6", PREFAB_DRESSING_SANITATION, -23, 20, 0, 2.4, 2.8, 5);
		layout.AddDressing("dressing_waste_7", PREFAB_DRESSING_WASTE, 6, -20, 90, 1, 0.7, 3);
		layout.AddDressing("dressing_entrancelight_8", PREFAB_DRESSING_ENTRANCELIGHT, -5, -16, 0, 2, 1.5, 2);
		// INTERIOR DRESSING END
		return layout;
	}

	static IA_DynamicSiteLayout CreateRallyPost()
	{
		ref IA_DynamicSiteLayout layout = CreateSmallLayout(LAYOUT_RALLY_POST, "Rally post", 18, 24, 9, Vector(0, 0, -11), -10, 12);
		// Last-resort authored fortification for narrow clearings. The stock HQ
		// remains full size; barracks and the supply shelter are omitted.
		layout.AddModule("hq", PREFAB_HQ, 0, 9, 180, 12, 10, IA_DynamicSiteModuleRole.Hq, 0.35, 41);

		layout.AddCover("wall_rear_00", -15.300, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_01", -12.518, 23.100, 0, 1, 0);
		layout.AddCover("wall_rear_02", -9.736, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_03", -6.955, 23.100, 0, 2, 0);
		layout.AddCover("wall_rear_04", -4.173, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_05", -1.391, 23.100, 0, 1, 0);
		layout.AddCover("wall_rear_06", 1.391, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_07", 4.173, 23.100, 0, 2, 0);
		layout.AddCover("wall_rear_08", 6.955, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_09", 9.736, 23.100, 0, 1, 0);
		layout.AddCover("wall_rear_10", 12.518, 23.100, 0, 0, 0);
		layout.AddCover("wall_rear_11", 15.300, 23.100, 0, 2, 0);
		layout.AddCover("wall_frontW_00", -15.300, -23.100, 180, 3, 2);
		layout.AddCover("wall_frontW_01", -12.925, -23.100, 180, 1, 2);
		layout.AddCover("wall_frontW_02", -10.550, -23.100, 180, 0, 2);
		layout.AddCover("wall_frontW_03", -8.175, -23.100, 180, 2, 2);
		layout.AddCover("wall_frontW_04", -5.800, -23.100, 180, 3, 2);
		layout.AddCover("wall_frontE_00", 5.800, -23.100, 180, 3, 2);
		layout.AddCover("wall_frontE_01", 8.175, -23.100, 180, 1, 2);
		layout.AddCover("wall_frontE_02", 10.550, -23.100, 180, 0, 2);
		layout.AddCover("wall_frontE_03", 12.925, -23.100, 180, 2, 2);
		layout.AddCover("wall_frontE_04", 15.300, -23.100, 180, 3, 2);
		layout.AddCover("wall_westS_00", -17.100, -21.300, 270, 3, 3);
		layout.AddCover("wall_westS_01", -17.100, -18.550, 270, 1, 3);
		layout.AddCover("wall_westS_02", -17.100, -15.800, 270, 3, 3);
		layout.AddCover("wall_westN_00", -17.100, -4.200, 270, 3, 3);
		layout.AddCover("wall_westN_01", -17.100, -1.367, 270, 1, 3);
		layout.AddCover("wall_westN_02", -17.100, 1.467, 270, 0, 3);
		layout.AddCover("wall_westN_03", -17.100, 4.300, 270, 2, 3);
		layout.AddCover("wall_westN_04", -17.100, 7.133, 270, 0, 3);
		layout.AddCover("wall_westN_05", -17.100, 9.967, 270, 1, 3);
		layout.AddCover("wall_westN_06", -17.100, 12.800, 270, 0, 3);
		layout.AddCover("wall_westN_07", -17.100, 15.633, 270, 2, 3);
		layout.AddCover("wall_westN_08", -17.100, 18.467, 270, 0, 3);
		layout.AddCover("wall_westN_09", -17.100, 21.300, 270, 3, 3);
		layout.AddCover("wall_eastS_00", 17.100, -21.300, 90, 3, 1);
		layout.AddCover("wall_eastS_01", 17.100, -18.550, 90, 1, 1);
		layout.AddCover("wall_eastS_02", 17.100, -15.800, 90, 3, 1);
		layout.AddCover("wall_eastN_00", 17.100, -4.200, 90, 3, 1);
		layout.AddCover("wall_eastN_01", 17.100, -1.367, 90, 1, 1);
		layout.AddCover("wall_eastN_02", 17.100, 1.467, 90, 0, 1);
		layout.AddCover("wall_eastN_03", 17.100, 4.300, 90, 2, 1);
		layout.AddCover("wall_eastN_04", 17.100, 7.133, 90, 0, 1);
		layout.AddCover("wall_eastN_05", 17.100, 9.967, 90, 1, 1);
		layout.AddCover("wall_eastN_06", 17.100, 12.800, 90, 0, 1);
		layout.AddCover("wall_eastN_07", 17.100, 15.633, 90, 2, 1);
		layout.AddCover("wall_eastN_08", 17.100, 18.467, 90, 0, 1);
		layout.AddCover("wall_eastN_09", 17.100, 21.300, 90, 3, 1);

		layout.m_aGuardPosts.Insert(Vector(0, 0, -7));
		layout.m_aGuardPosts.Insert(Vector(-4, 0, -18));
		layout.m_aGuardPosts.Insert(Vector(4, 0, -18));
		layout.m_aGuardPosts.Insert(Vector(-13.5, 0, 8));
		layout.m_aGuardPosts.Insert(Vector(13.5, 0, 8));
		// INTERIOR DRESSING BEGIN
		layout.AddDressing("dressing_messlit_0", PREFAB_DRESSING_MESSLIT, -9, -6, 0, 1.7, 1.5, 7);
		layout.AddDressing("dressing_stores_1", PREFAB_DRESSING_STORES, 9, -15, 90, 1.8, 2.9, 6);
		layout.AddDressing("dressing_waterwash_2", PREFAB_DRESSING_WATERWASH, -10, -17, 0, 1.4, 1.2, 5);
		layout.AddDressing("dressing_power_3", PREFAB_DRESSING_POWER, 9, -5, 0, 2, 2.5, 7);
		// INTERIOR DRESSING END
		return layout;
	}

	protected static IA_DynamicSiteLayout CreateSmallLayout(int id, string name, float halfW, float halfD, float captureRadius, vector capture, float crossLaneZ, int maxGarrison)
	{
		ref IA_DynamicSiteLayout layout = new IA_DynamicSiteLayout();
		layout.m_iLayoutId = id;
		layout.m_sName = name;
		layout.m_fHalfWidthM = halfW;
		layout.m_fHalfDepthM = halfD;
		layout.m_fCaptureRadiusM = captureRadius;
		layout.m_iMaxGarrison = maxGarrison;
		layout.m_vCaptureLocal = capture;
		layout.m_vAssemblyLocal = vector.Zero;
		layout.m_aModules = new array<ref IA_DynamicSiteModule>();
		layout.m_aEntries = new array<vector>();
		layout.m_aGuardPosts = new array<vector>();
		layout.m_aPerimeterStations = new array<vector>();
		layout.m_aEntries.Insert(Vector(0, 0, -halfD));
		layout.m_aEntries.Insert(Vector(-halfW, 0, crossLaneZ));
		layout.m_aEntries.Insert(Vector(halfW, 0, crossLaneZ));
		return layout;
	}

	// Ordered largest to smallest in Auto. Existing admin values stay unchanged.
	static void GetAllowedLayoutIds(int sizeMode, notnull array<int> ids)
	{
		ids.Clear();
		if (sizeMode >= IA_DynamicSiteSizeMode.Full && sizeMode <= IA_DynamicSiteSizeMode.RallyPost)
		{
			ids.Insert(sizeMode - 1);
			return;
		}
		ids.Insert(LAYOUT_FULL);
		ids.Insert(LAYOUT_COMPACT);
		ids.Insert(LAYOUT_COURTYARD);
		ids.Insert(LAYOUT_ROADSIDE);
		ids.Insert(LAYOUT_COMMAND_POST);
		ids.Insert(LAYOUT_RALLY_POST);
	}

	static IA_DynamicSiteLayout CreateById(int layoutId)
	{
		if (layoutId == LAYOUT_RALLY_POST)
			return CreateRallyPost();
		if (layoutId == LAYOUT_COMMAND_POST)
			return CreateCommandPost();
		if (layoutId == LAYOUT_ROADSIDE)
			return CreateRoadside();
		if (layoutId == LAYOUT_COURTYARD)
			return CreateCourtyard();
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
		mod.m_vSupportMins = Vector(-halfW, 0, -halfD);
		mod.m_vSupportMaxs = Vector(halfW, 0, halfD);
		mod.m_aSupportPoints.Insert(Vector(0, 0, 0));
		mod.m_aSupportPoints.Insert(Vector(-halfW, 0, -halfD));
		mod.m_aSupportPoints.Insert(Vector(halfW, 0, -halfD));
		mod.m_aSupportPoints.Insert(Vector(-halfW, 0, halfD));
		mod.m_aSupportPoints.Insert(Vector(halfW, 0, halfD));
		mod.m_fMaxSupportDeltaM = maxDelta;
		// Measured stock USSR tent: x=-3.951..3.972, z=-3.917..4.699.
		// Its earth foundation extends to y=-2.746; composition adds +0.1 m.
		// Keep a conservative 0.8 m bearing range and at most 0.35 m lift.
		if (prefab == PREFAB_HQ || prefab == PREFAB_BARRACKS || prefab == PREFAB_MEDICAL)
		{
			mod.SetSupportFootprint(Vector(-4, 0, -4), Vector(4, 0, 4.75));
			mod.m_fMaxSupportDeltaM = 0.8;
			mod.m_fMaxFoundationLiftM = 0.35;
			mod.m_fFloorAboveOriginM = 0.1;
		}
		else if (prefab == PREFAB_SUPPLY)
		{
			// Measured aggregate shelter/crate bounds, rounded outward. This
			// module has no earth foundation, so retain its 0.35 m height limit.
			mod.SetSupportFootprint(Vector(-4.5, 0, -5.5), Vector(4.5, 0, 5.5));
		}
		mod.m_iGroundingPolicy = IA_DynamicSiteGrounding.UprightPad;
		mod.m_bRequired = true;
		mod.m_iEstimatedExpandedEntities = expanded;
		mod.m_iRole = role;
		m_aModules.Insert(mod);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCover(string id, float x, float z, float yawDeg, int style = 0, int side = 0)
	{
		ResourceName prefab = PREFAB_COVER;
		if (style == 1)
			prefab = PREFAB_COVER_WINDOW;
		else if (style == 2)
			prefab = PREFAB_COVER_HIGH;
		else if (style == 3)
			prefab = PREFAB_COVER_ROUND;
		// One replicated stock panel per section, independently terrain-aligned.
		AddModule(id, prefab, x, z, yawDeg, 1.8, 0.8, IA_DynamicSiteModuleRole.Cover, 0.50, 1);
		IA_DynamicSiteModule mod = m_aModules[m_aModules.Count() - 1];
		mod.m_iGroundingPolicy = IA_DynamicSiteGrounding.TerrainSegment;
		mod.m_iPerimeterSide = side;
		mod.m_bRequired = false; // Coverage is required collectively, below.
		m_aPerimeterStations.Insert(Vector(x, 0, z));
	}

	protected void AddDressing(string id, ResourceName prefab, float x, float z, float yawDeg, float halfW, float halfD, int expanded)
	{
		AddModule(id, prefab, x, z, yawDeg, halfW, halfD, IA_DynamicSiteModuleRole.Dressing, 0.15, expanded);
		IA_DynamicSiteModule mod = m_aModules[m_aModules.Count() - 1];
		mod.m_bRequired = false;
		mod.SetSupportFootprint(Vector(-halfW, 0, -halfD), Vector(halfW, 0, halfD));
	}

	// Defend circles stay inside the authored wall faces, with a one-metre
	// movement margin. Side IDs: rear=0, east=1, front=2, west=3.
	float GetDefendPostRadius(vector localPost, float preferredRadius = 15)
	{
		float clearance = preferredRadius;
		array<bool> found = {false, false, false, false};
		foreach (IA_DynamicSiteModule mod : m_aModules)
		{
			int side = mod.m_iPerimeterSide;
			if (side < 0 || side > 3)
				continue;
			found[side] = true;
			// Perimeter panels use cardinal headings; pad depth is wall thickness.
			float distance;
			if (side == 0)
				distance = mod.m_vLocalPosition[2] - localPost[2];
			else if (side == 1)
				distance = mod.m_vLocalPosition[0] - localPost[0];
			else if (side == 2)
				distance = localPost[2] - mod.m_vLocalPosition[2];
			else
				distance = localPost[0] - mod.m_vLocalPosition[0];
			clearance = Math.Min(clearance, distance - mod.m_fHalfDepthM - 1);
		}
		foreach (bool sideFound : found)
		{
			if (!sideFound)
				return 0;
		}
		return Math.Max(0, clearance);
	}

	bool HasPerimeterCoverage(notnull array<int> placed)
	{
		array<int> total = {0, 0, 0, 0};
		foreach (IA_DynamicSiteModule mod : m_aModules)
		{
			if (mod.m_iPerimeterSide >= 0)
				total[mod.m_iPerimeterSide] = total[mod.m_iPerimeterSide] + 1;
		}
		int all = 0;
		int built = 0;
		for (int side = 0; side < 4; side++)
		{
			if (placed[side] < Math.Ceil(total[side] * 0.60))
				return false;
			all += total[side];
			built += placed[side];
		}
		return built >= Math.Ceil(all * 0.75);
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
