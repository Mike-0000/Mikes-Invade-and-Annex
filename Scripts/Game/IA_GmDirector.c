//------------------------------------------------------------------------------------------------
//! Shared Game Master director. Places and registers the same IA_AreaMarker pipeline
//! used by classic I&A. Capture, QRF, and waves stay on MissionInitializer / AreaGroupManager.
//------------------------------------------------------------------------------------------------
enum IA_GmBucket
{
	Staging,
	Live
}

class IA_GmDirector
{
	protected static const int GROUP_BASE = 1000;
	protected static ref IA_GmDirector s_Instance;

	protected int m_iLiveGroup;
	protected int m_iStagingGroup;

	//------------------------------------------------------------------------------------------------
	static IA_GmDirector GetInstance()
	{
		if (!s_Instance)
			s_Instance = new IA_GmDirector();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void IA_GmDirector()
	{
		m_iLiveGroup = -1;
		m_iStagingGroup = GROUP_BASE;
	}

	//------------------------------------------------------------------------------------------------
	void EnsureStarted()
	{
		if (m_iStagingGroup < GROUP_BASE)
			m_iStagingGroup = GROUP_BASE;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsGameMasterMode()
	{
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return false;
		return cfg.m_bGameMasterMode;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAutoQrfOff()
	{
		if (!IsGameMasterMode())
			return false;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return false;
		if (cfg.m_bGmAutoQrf)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAutoArtyOff()
	{
		if (!IsGameMasterMode())
			return false;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return false;
		if (cfg.m_bGmAutoArty)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAutoSideOff()
	{
		if (!IsGameMasterMode())
			return false;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return true;
		if (cfg.m_bGmAutoSideMissions)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAutoSupportOn()
	{
		if (!IsGameMasterMode())
			return true;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return false;
		return cfg.m_bGmAutoPlaceSupport;
	}

	//------------------------------------------------------------------------------------------------
	static bool ShouldAutoActivateStaging()
	{
		if (!IsGameMasterMode())
			return true;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return false;
		return cfg.m_bGmAutoActivateStaging;
	}

	//------------------------------------------------------------------------------------------------
	int GetLiveGroupId()
	{
		return m_iLiveGroup;
	}

	//------------------------------------------------------------------------------------------------
	int GetStagingGroupId()
	{
		EnsureStarted();
		return m_iStagingGroup;
	}

	//------------------------------------------------------------------------------------------------
	int GetGroupIdForBucket(IA_GmBucket bucket)
	{
		EnsureStarted();
		if (bucket == IA_GmBucket.Live && m_iLiveGroup >= 0)
			return m_iLiveGroup;
		return m_iStagingGroup;
	}

	//------------------------------------------------------------------------------------------------
	void PromoteStagingToLive()
	{
		EnsureStarted();
		m_iLiveGroup = m_iStagingGroup;
		m_iStagingGroup = m_iLiveGroup + 1;
		Print(string.Format("[IA_GmDirector] Live group is now %1, staging %2", m_iLiveGroup, m_iStagingGroup), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void ClearLive()
	{
		m_iLiveGroup = -1;
	}

	//------------------------------------------------------------------------------------------------
	static string AreaTypeToString(IA_AreaType type)
	{
		if (type == IA_AreaType.Town)
			return "Town";
		if (type == IA_AreaType.City)
			return "City";
		if (type == IA_AreaType.Property)
			return "Property";
		if (type == IA_AreaType.Airport)
			return "Airport";
		if (type == IA_AreaType.Docks)
			return "Docks";
		if (type == IA_AreaType.Military)
			return "Military";
		if (type == IA_AreaType.SmallMilitary)
			return "SmallMilitary";
		if (type == IA_AreaType.RadioTower)
			return "RadioTower";
		if (type == IA_AreaType.DefendObjective)
			return "DefendObjective";
		if (type == IA_AreaType.MortarPit)
			return "MortarPit";
		if (type == IA_AreaType.Assassination)
			return "Assassination";
		return "Property";
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName PrefabForAreaType(IA_AreaType type)
	{
		if (type == IA_AreaType.Town)
			return "{808EDC7240341FEE}Prefabs/IA_AreaMarkers/IA_Town.et";
		if (type == IA_AreaType.City)
			return "{EB1E27CA063A3403}Prefabs/IA_AreaMarkers/IA_City.et";
		if (type == IA_AreaType.Property)
			return "{4C3443FA0E441870}Prefabs/IA_AreaMarkers/IA_Property.et";
		if (type == IA_AreaType.Docks)
			return "{B3CEBC9A9C5872A6}Prefabs/IA_AreaMarkers/IA_Docks.et";
		if (type == IA_AreaType.Military)
			return "{0C868FB4A69F9BE8}Prefabs/IA_AreaMarkers/IA_Military.et";
		if (type == IA_AreaType.SmallMilitary)
			return "{7ADD348E46715524}Prefabs/IA_AreaMarkers/IA_SmallMilitary.et";
		if (type == IA_AreaType.RadioTower)
			return "{4CE72B528FB4853C}Prefabs/RadioTower.et";
		if (type == IA_AreaType.MortarPit)
			return "Prefabs/IA_AreaMarkers/IA_MortarPit.et";
		if (type == IA_AreaType.DefendObjective)
			return "{B66EE4ECE753F16C}Prefabs/IA_AreaMarkers/IA_DefendObjective.et";
		if (type == IA_AreaType.Assassination)
			return "{CC469EDE5BF275E5}Prefabs/IA_AreaMarkers/IA_SideObjectiveMarker.et";
		return "{4C3443FA0E441870}Prefabs/IA_AreaMarkers/IA_Property.et";
	}

	//------------------------------------------------------------------------------------------------
	static float DefaultRadiusForType(IA_AreaType type)
	{
		if (type == IA_AreaType.City)
			return 115;
		if (type == IA_AreaType.Town)
			return 90;
		if (type == IA_AreaType.Military)
			return 150;
		if (type == IA_AreaType.Docks)
			return 100;
		if (type == IA_AreaType.RadioTower)
			return 30;
		if (type == IA_AreaType.MortarPit)
			return 40;
		if (type == IA_AreaType.DefendObjective)
			return 50;
		return 80;
	}

	//------------------------------------------------------------------------------------------------
	string MakeUniqueName(IA_AreaType type)
	{
		string prefix = AreaTypeToString(type);
		int n = 1;
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		int i;
		int count = 0;
		if (markers)
			count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			n++;
		}
		return prefix + " " + n.ToString();
	}

	//------------------------------------------------------------------------------------------------
	void RegisterMarker(notnull IA_AreaMarker marker, IA_GmBucket bucket)
	{
		if (!Replication.IsServer())
			return;

		EnsureStarted();
		int groupId = GetGroupIdForBucket(bucket);
		marker.SetAreaGroup(groupId);

		string name = marker.GetAreaName();
		if (name.IsEmpty() || name == "CaptureZone")
		{
			name = MakeUniqueName(marker.GetAreaType());
			marker.SetAreaName(name);
		}

		Print(string.Format("[IA_GmDirector] Registered '%1' (%2) group %3", name, AreaTypeToString(marker.GetAreaType()), groupId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static IA_AreaMarker MarkerFromEditorItem(Managed item)
	{
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(item);
		if (!editable)
			return null;
		return IA_AreaMarker.Cast(editable.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	IA_AreaMarker PlaceSite(IA_AreaType type, vector pos, IA_GmBucket bucket, string name, float radius)
	{
		if (!Replication.IsServer())
			return null;

		if (type == IA_AreaType.Assassination)
		{
			PlaceSideObjective(pos);
			return null;
		}

		ResourceName prefab = PrefabForAreaType(type);
		Resource res = Resource.Load(prefab);
		if (!res)
			res = Resource.Load("{61B9AD559D2CE12D}Components/Area_Marker.et");
		if (!res)
		{
			Print("[IA_GmDirector] Failed to load area marker prefab", LogLevel.ERROR);
			return null;
		}

		IEntity ent = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(pos));
		IA_AreaMarker marker = IA_AreaMarker.Cast(ent);
		if (!marker)
		{
			Print("[IA_GmDirector] Spawned entity is not IA_AreaMarker", LogLevel.ERROR);
			if (ent)
				IA_Game.AddEntityToGc(ent);
			return null;
		}

		if (radius <= 0)
			radius = DefaultRadiusForType(type);
		if (name.IsEmpty())
			name = MakeUniqueName(type);

		int groupId = GetGroupIdForBucket(bucket);
		marker.ConfigureRuntime(groupId, name, radius, AreaTypeToString(type));
		RegisterMarker(marker, bucket);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	void HotAdd(notnull IA_AreaMarker marker)
	{
		if (!Replication.IsServer())
			return;

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		if (m_iLiveGroup >= 0)
		{
			marker.SetAreaGroup(m_iLiveGroup);
			init.SpawnAreaFromMarker(marker, init.GetRandomEnemyFaction(), m_iLiveGroup);
			return;
		}

		RegisterMarker(marker, IA_GmBucket.Staging);
		init.ServerActivateStaging();
	}

	//------------------------------------------------------------------------------------------------
	array<IA_AreaMarker> CollectGroupMarkers(int groupId)
	{
		ref array<IA_AreaMarker> result = new array<IA_AreaMarker>();
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		if (!markers)
			return result;

		int i;
		int count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			if (marker.m_areaGroup != groupId)
				continue;
			result.Insert(marker);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	IA_SideObjectiveMarker PlaceSideObjective(vector pos)
	{
		if (!Replication.IsServer())
			return null;

		Resource res = Resource.Load("{CC469EDE5BF275E5}Prefabs/IA_AreaMarkers/IA_SideObjectiveMarker.et");
		if (!res)
		{
			Print("[IA_GmDirector] Failed to load side-objective prefab", LogLevel.ERROR);
			return null;
		}

		IEntity ent = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(pos));
		IA_SideObjectiveMarker marker = IA_SideObjectiveMarker.Cast(ent);
		if (!marker)
		{
			Print("[IA_GmDirector] Spawned entity is not IA_SideObjectiveMarker", LogLevel.ERROR);
			if (ent)
				IA_Game.AddEntityToGc(ent);
			return null;
		}

		Print("[IA_GmDirector] Placed side-objective marker", LogLevel.NORMAL);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	IA_SideObjectiveMarker FindNearestSideMarker(vector pos, float maxDist)
	{
		array<IA_SideObjectiveMarker> markers = IA_SideObjectiveMarker.GetAllMarkers();
		if (!markers)
			return null;

		IA_SideObjectiveMarker best = null;
		float bestDist = maxDist;
		int i;
		int count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_SideObjectiveMarker marker = markers[i];
			if (!marker)
				continue;
			float dist = vector.Distance(marker.GetOrigin(), pos);
			if (dist > bestDist)
				continue;
			bestDist = dist;
			best = marker;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	bool StartSideAt(vector pos)
	{
		if (!Replication.IsServer())
			return false;

		IA_SideObjectiveMarker marker = FindNearestSideMarker(pos, 250);
		if (!marker)
			marker = PlaceSideObjective(pos);
		if (!marker)
			return false;

		return IA_SideObjectiveManager.GetInstance().StartAt(marker);
	}
}
