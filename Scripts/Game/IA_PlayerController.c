//------------------------------------------------------------------------------------------------
//! Client-to-server channel for the admin config panel.
//! RPCs on IA_MissionInitializer fail on dedicated clients when that world entity is not
//! streamed; the local player controller is always owned by the client.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void IA_AskUpdateAdminConfig(string packed)
	{
		if (Replication.IsServer())
		{
			IA_ApplyAdminConfigIfAdmin(packed);
			return;
		}

		Rpc(RpcAsk_IA_UpdateAdminConfig, packed);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskPersistAdminConfig(string packed)
	{
		if (Replication.IsServer())
		{
			IA_PersistAdminConfigIfAdmin(packed);
			return;
		}

		Rpc(RpcAsk_IA_PersistAdminConfig, packed);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskClearAdminOverrides()
	{
		if (Replication.IsServer())
		{
			IA_ClearAdminOverridesIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_ClearAdminOverrides);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskForceCompleteZone()
	{
		if (Replication.IsServer())
		{
			IA_ForceCompleteZoneIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_ForceCompleteZone);
	}

	void IA_AskForceCompleteZoneAndDefend()
	{
		if (Replication.IsServer())
		{
			IA_ForceCompleteZoneAndDefendIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_ForceCompleteZoneAndDefend);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskPromoteSelf()
	{
		if (Replication.IsServer())
		{
			IA_PromoteSelfIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_PromoteSelf);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskForceQRF(int type)
	{
		if (Replication.IsServer())
		{
			IA_ForceQRFIfAdmin(type);
			return;
		}

		Rpc(RpcAsk_IA_ForceQRF, type);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_UpdateAdminConfig(string packed)
	{
		IA_ApplyAdminConfigIfAdmin(packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_PersistAdminConfig(string packed)
	{
		IA_PersistAdminConfigIfAdmin(packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ClearAdminOverrides()
	{
		IA_ClearAdminOverridesIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceCompleteZone()
	{
		IA_ForceCompleteZoneIfAdmin();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceCompleteZoneAndDefend()
	{
		IA_ForceCompleteZoneAndDefendIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_PromoteSelf()
	{
		IA_PromoteSelfIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceQRF(int type)
	{
		IA_ForceQRFIfAdmin(type);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ApplyAdminConfigIfAdmin(string packed)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Admin config update rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
		{
			Print("[IA] Admin config update rejected: mission initializer missing", LogLevel.ERROR);
			return;
		}

		init.ServerApplyAdminConfig(packed);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_PersistAdminConfigIfAdmin(string packed)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Admin persist rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
		{
			Print("[IA] Admin persist rejected: mission initializer missing", LogLevel.ERROR);
			return;
		}

		init.ServerPersistAdminConfig(packed);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ClearAdminOverridesIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Admin override clear rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerClearAdminOverrides();
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ForceCompleteZoneIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force complete zone rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerForceCompleteZone();
	}

	protected void IA_ForceCompleteZoneAndDefendIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force complete + defend rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerForceCompleteZoneAndDefend();
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_PromoteSelfIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Promote self rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (!session)
		{
			Print("[IA] Promote self rejected: session rank manager missing", LogLevel.ERROR);
			return;
		}

		session.PromotePlayer(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmPlaceSite(int type, float x, float z, int bucket, string name, float radius)
	{
		if (Replication.IsServer())
		{
			IA_GmPlaceSiteIfAdmin(type, x, z, bucket, name, radius);
			return;
		}

		Rpc(RpcAsk_IA_GmPlaceSite, type, x, z, bucket, name, radius);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmPlaceHoldPost(float x, float z, float radius)
	{
		if (Replication.IsServer())
		{
			IA_GmPlaceHoldPostIfAdmin(x, z, radius);
			return;
		}

		Rpc(RpcAsk_IA_GmPlaceHoldPost, x, z, radius);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmActivateStaging()
	{
		if (Replication.IsServer())
		{
			IA_GmActivateStagingIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_GmActivateStaging);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskForceQRFAt(int type, float x, float z)
	{
		if (Replication.IsServer())
		{
			IA_ForceQRFAtIfAdmin(type, x, z);
			return;
		}

		Rpc(RpcAsk_IA_ForceQRFAt, type, x, z);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmStartSideAt(float x, float z)
	{
		if (Replication.IsServer())
		{
			IA_GmStartSideIfAdmin(x, z);
			return;
		}

		Rpc(RpcAsk_IA_GmStartSideAt, x, z);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmHotAdd(float x, float z)
	{
		if (Replication.IsServer())
		{
			IA_GmHotAddIfAdmin(x, z);
			return;
		}

		Rpc(RpcAsk_IA_GmHotAdd, x, z);
	}

	//------------------------------------------------------------------------------------------------
	void IA_BroadcastGmBuckets()
	{
		IA_GmDirector dir = IA_GmDirector.GetInstance();
		Rpc(RpcDo_IA_GmSetBuckets, dir.GetLiveGroupId(), dir.GetStagingGroupId());
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmRenameSite(float x, float z, string name)
	{
		if (Replication.IsServer())
		{
			IA_GmRenameSiteIfAdmin(x, z, name);
			return;
		}

		Rpc(RpcAsk_IA_GmRenameSite, x, z, name);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskGmDeleteStaging(float x, float z)
	{
		if (Replication.IsServer())
		{
			IA_GmDeleteStagingIfAdmin(x, z);
			return;
		}

		Rpc(RpcAsk_IA_GmDeleteStaging, x, z);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmPlaceSite(int type, float x, float z, int bucket, string name, float radius)
	{
		IA_GmPlaceSiteIfAdmin(type, x, z, bucket, name, radius);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmPlaceHoldPost(float x, float z, float radius)
	{
		IA_GmPlaceHoldPostIfAdmin(x, z, radius);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmActivateStaging()
	{
		IA_GmActivateStagingIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceQRFAt(int type, float x, float z)
	{
		IA_ForceQRFAtIfAdmin(type, x, z);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmStartSideAt(float x, float z)
	{
		IA_GmStartSideIfAdmin(x, z);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmHotAdd(float x, float z)
	{
		IA_GmHotAddIfAdmin(x, z);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmRenameSite(float x, float z, string name)
	{
		IA_GmRenameSiteIfAdmin(x, z, name);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_GmDeleteStaging(float x, float z)
	{
		IA_GmDeleteStagingIfAdmin(x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmPlaceSiteIfAdmin(int type, float x, float z, int bucket, string name, float radius)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] GM place rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_GmBucket gmBucket = IA_GmBucket.Staging;
		if (bucket == IA_GmBucket.Live)
			gmBucket = IA_GmBucket.Live;

		vector pos = Vector(x, 0, z);
		IA_AreaType areaType = type;
		IA_GmDirector dir = IA_GmDirector.GetInstance();
		int liveBefore = dir.GetLiveGroupId();
		IA_AreaMarker marker = dir.PlaceSite(areaType, pos, gmBucket, name, radius);
		if (gmBucket == IA_GmBucket.Live && marker && marker.GetAreaType() != IA_AreaType.DefendObjective)
			dir.HotAdd(marker);

		if (!marker)
			return;

		if (dir.GetLiveGroupId() != liveBefore)
			Rpc(RpcDo_IA_GmSetBuckets, dir.GetLiveGroupId(), dir.GetStagingGroupId());

		vector origin = marker.GetOrigin();
		int groupId = marker.m_areaGroup;
		float placedRadius = marker.GetRadius();
		string placedName = marker.GetAreaName();
		dir.RememberPlacedSite(marker.GetAreaType(), origin[0], origin[2], groupId, placedRadius, placedName);
		Rpc(RpcDo_IA_GmSitePlaced, marker.GetAreaType(), origin[0], origin[2], groupId, placedRadius, placedName);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmPlaceHoldPostIfAdmin(float x, float z, float radius)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] GM building hold rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_GmDirector.GetInstance().PlaceHoldPost(Vector(x, 0, z), radius);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmActivateStagingIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Activate Staging rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerActivateStaging();
		IA_GmDirector dir = IA_GmDirector.GetInstance();
		Rpc(RpcDo_IA_GmSetBuckets, dir.GetLiveGroupId(), dir.GetStagingGroupId());
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ForceQRFAtIfAdmin(int type, float x, float z)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force QRF at point rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		IA_AreaGroupManager mgr = init.GetCurrentAreaGroupManager();
		if (!mgr)
		{
			Print("[IA][Admin] Force QRF at point rejected: no area group manager", LogLevel.WARNING);
			return;
		}

		mgr.ForceSpawnQRFAt(type, Vector(x, 0, z));
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmStartSideIfAdmin(float x, float z)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Start side mission rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_GmDirector.GetInstance().StartSideAt(Vector(x, 0, z));
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmHotAddIfAdmin(float x, float z)
	{
		if (!IA_IsAdminCaller())
			return;

		ref array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		if (!markers)
			return;

		vector pos = Vector(x, 0, z);
		IA_AreaMarker best = null;
		float bestDist = 80;
		int i;
		int count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			vector origin = marker.GetOrigin();
			origin[1] = 0;
			float dist = vector.Distance(origin, pos);
			if (dist > bestDist)
				continue;
			bestDist = dist;
			best = marker;
		}

		if (best)
		{
			IA_GmDirector dir = IA_GmDirector.GetInstance();
			int liveBefore = dir.GetLiveGroupId();
			dir.HotAdd(best);
			if (dir.GetLiveGroupId() != liveBefore)
				Rpc(RpcDo_IA_GmSetBuckets, dir.GetLiveGroupId(), dir.GetStagingGroupId());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmRenameSiteIfAdmin(float x, float z, string name)
	{
		if (!IA_IsAdminCaller())
			return;
		if (name.IsEmpty())
			return;

		IA_GmDirector dir = IA_GmDirector.GetInstance();
		dir.RenameSiteAt(x, z, name);
		IA_AreaMarker marker = dir.FindMarkerNear(x, z, 80);
		if (!marker)
			return;

		vector origin = marker.GetOrigin();
		Rpc(RpcDo_IA_GmSitePlaced, marker.GetAreaType(), origin[0], origin[2], marker.m_areaGroup, marker.GetRadius(), marker.GetAreaName());
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_GmDeleteStagingIfAdmin(float x, float z)
	{
		if (!IA_IsAdminCaller())
			return;

		IA_GmDirector dir = IA_GmDirector.GetInstance();
		if (!dir.DeleteStagingAt(x, z))
			return;

		Rpc(RpcDo_IA_GmSiteRemoved, x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ForceQRFIfAdmin(int type)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force QRF rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
		{
			Print("[IA][Admin] Force QRF rejected: mission initializer missing", LogLevel.ERROR);
			return;
		}

		IA_AreaGroupManager mgr = init.GetCurrentAreaGroupManager();
		if (!mgr)
		{
			Print("[IA][Admin] Force QRF rejected: no area group manager", LogLevel.WARNING);
			return;
		}

		mgr.ForceSpawnQRF(type);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IA_GmSitePlaced(int type, float x, float z, int groupId, float radius, string name)
	{
		if (Replication.IsServer())
			return;

		IA_GmDirector.GetInstance().RememberPlacedSite(type, x, z, groupId, radius, name);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IA_GmSetBuckets(int liveGroup, int stagingGroup)
	{
		if (Replication.IsServer())
			return;

		IA_GmDirector.GetInstance().SetBuckets(liveGroup, stagingGroup);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_IA_GmSiteRemoved(float x, float z)
	{
		if (Replication.IsServer())
			return;

		IA_GmDirector.GetInstance().ForgetKnownSite(x, z);
	}

	protected bool IA_IsAdminCaller()
	{
		if (!Replication.IsRunning())
			return true;

		return SCR_Global.IsAdmin(GetPlayerId());
	}
}
