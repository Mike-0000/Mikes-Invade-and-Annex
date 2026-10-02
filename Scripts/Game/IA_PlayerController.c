//------------------------------------------------------------------------------------------------
//! Client-to-server channel for the admin config panel.
//! RPCs on IA_MissionInitializer fail on dedicated clients when that world entity is not
//! streamed; the local player controller is always owned by the client.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected static const int IA_HELI_PAINT_MIN_GAP_MS = 120;
	protected int m_iIA_LastHeliPaintMs = -IA_HELI_PAINT_MIN_GAP_MS;
	protected int m_iIA_LastHeliStateMs = -IA_HELI_PAINT_MIN_GAP_MS;
	protected static const int IA_BOARD_MIN_GAP_MS = 100;
	protected static const int IA_STANDING_MIN_GAP_MS = 1000;
	protected int m_iIA_LastBoardAskMs = -IA_BOARD_MIN_GAP_MS;
	protected int m_iIA_LastStandingAskMs = -IA_STANDING_MIN_GAP_MS;

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

	void IA_AskForceCompleteObjectivesAndSeizeBase()
	{
		if (Replication.IsServer())
		{
			IA_ForceCompleteObjectivesAndSeizeBaseIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_ForceCompleteObjectivesAndSeizeBase);
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
	//! Paint bay: ask the server for this player's rating and the thresholds in force.
	void IA_AskHeliPaintState()
	{
		if (Replication.IsServer())
		{
			IA_AnswerHeliPaint(false, IA_HeliSkinCatalog.SKIN_NONE);
			return;
		}

		Rpc(RpcAsk_IA_HeliPaintState);
	}

	//------------------------------------------------------------------------------------------------
	//! Paint bay: ask the server to put a skin on the helicopter this player pilots.
	void IA_AskSetHeliSkin(int skinId)
	{
		if (Replication.IsServer())
		{
			IA_AnswerHeliPaint(true, skinId);
			return;
		}

		Rpc(RpcAsk_IA_SetHeliSkin, skinId);
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

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceCompleteObjectivesAndSeizeBase()
	{
		IA_ForceCompleteObjectivesAndSeizeBaseIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_PromoteSelf()
	{
		IA_PromoteSelfIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_HeliPaintState()
	{
		IA_AnswerHeliPaint(false, IA_HeliSkinCatalog.SKIN_NONE);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_SetHeliSkin(int skinId)
	{
		IA_AnswerHeliPaint(true, skinId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_IA_HeliPaintReply(int result, int rating, int skinId, string thresholds)
	{
		IA_HeliPaintMenu.OnServerReply(result, rating, skinId, thresholds);
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

	protected void IA_ForceCompleteObjectivesAndSeizeBaseIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force seize base rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerForceCompleteObjectivesAndSeizeBase();
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
	//! Server: answer a paint bay request. The client only names a skin; the seat, the paint
	//! channel and the unlock are all checked here. Nobody is let past the unlock: an admin
	//! earns a livery like any other pilot.
	protected void IA_AnswerHeliPaint(bool apply, int skinId)
	{
		// A paint bay waits for its answer before it asks again; anything faster is not one.
		int now = System.GetTickCount();
		if (apply)
		{
			if (now - m_iIA_LastHeliPaintMs < IA_HELI_PAINT_MIN_GAP_MS)
				return;
			m_iIA_LastHeliPaintMs = now;
		}
		else
		{
			if (now - m_iIA_LastHeliStateMs < IA_HELI_PAINT_MIN_GAP_MS)
				return;
			m_iIA_LastHeliStateMs = now;
		}

		int rating = IA_HeliPaintService.ReadRating(GetPlayerId());
		int result = IA_HeliPaintService.RESULT_STATE;
		if (apply)
		{
			result = IA_HeliPaintService.TrySetSkin(GetControlledEntity(), skinId, rating);
			if (result == IA_HeliPaintService.RESULT_APPLIED)
				IA_HeliSkinPadService.RememberChoice(GetPlayerId(), skinId);
			if (IA_Log.IsDebugEnabled())
			{
				Print(string.Format("[IA][HeliPaint] Player %1 asked for skin %2: result %3.", GetPlayerId(), skinId, result), LogLevel.NORMAL);
			}
		}

		string thresholds = IA_HeliSkinCatalog.PackThresholds();
		if (GetPlayerId() == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_IA_HeliPaintReply(result, rating, skinId, thresholds);
			return;
		}

		Rpc(RpcDo_IA_HeliPaintReply, result, rating, skinId, thresholds);
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

	//------------------------------------------------------------------------------------------------
	//! Client: ask for the page of a leaderboard that holds one row. The answer reaches
	//! IA_StatisticsMenu as a head and one or more row chunks.
	//! \param viewId the menu's tag for this board and sort order, sent back with the answer
	//! \param board an IA_BoardProtocol.BOARD_ value
	//! \param sortKey an IA_BoardProtocol.SORT_ value
	//! \param offset index of a row on the wanted page
	void IA_AskLeaderboardPage(int viewId, int board, int sortKey, bool descending, int offset)
	{
		if (Replication.IsServer())
		{
			IA_AnswerLeaderboardPage(viewId, board, sortKey, descending, offset);
			return;
		}

		Rpc(RpcAsk_IA_LeaderboardPage, viewId, board, sortKey, descending, offset);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_LeaderboardPage(int viewId, int board, int sortKey, bool descending, int offset)
	{
		IA_AnswerLeaderboardPage(viewId, board, sortKey, descending, offset);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_AnswerLeaderboardPage(int viewId, int board, int sortKey, bool descending, int offset)
	{
		ref array<string> none = {};
		int now = System.GetTickCount();
		if (now - m_iIA_LastBoardAskMs < IA_BOARD_MIN_GAP_MS)
		{
			IA_SendLeaderboardPage(viewId, IA_BoardProtocol.STATUS_BUSY, 0, offset, none, "");
			return;
		}
		m_iIA_LastBoardAskMs = now;

		IA_LeaderboardManagerComponent boards = IA_LeaderboardManagerComponent.GetInstance();
		if (!boards)
		{
			IA_SendLeaderboardPage(viewId, IA_BoardProtocol.STATUS_OFFLINE, 0, offset, none, "");
			return;
		}

		boards.Request(GetPlayerId(), viewId, board, sortKey, descending, offset);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player one page. The rows go out in chunks short enough for an RPC
	//! string, so a page's size on the wire never depends on how long the names are.
	//! \param total rows on the board; on IA_BoardProtocol.STATUS_LIMITED the seconds to wait
	//! \param rows packed IA_BoardRow lines, the first being row number offset
	//! \param mine the player's own packed line, empty when they are not on the board
	void IA_SendLeaderboardPage(int viewId, int status, int total, int offset, notnull array<string> rows, string mine)
	{
		bool local = GetPlayerId() == SCR_PlayerController.GetLocalPlayerId();
		if (local)
			RpcDo_IA_LeaderboardHead(viewId, status, total, offset, mine);
		else
			Rpc(RpcDo_IA_LeaderboardHead, viewId, status, total, offset, mine);

		// A refusal has no rows; its head ends the answer.
		if (status != IA_BoardProtocol.STATUS_OK)
			return;

		ref array<string> chunks = {};
		ref array<int> firsts = {};
		IA_BoardProtocol.SplitRows(rows, offset, chunks, firsts);
		int last = chunks.Count() - 1;
		for (int i = 0; i <= last; i++)
		{
			IA_SendLeaderboardRows(local, viewId, firsts[i], i == last, chunks[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_SendLeaderboardRows(bool local, int viewId, int offset, bool last, string rows)
	{
		if (local)
		{
			RpcDo_IA_LeaderboardRows(viewId, offset, last, rows);
			return;
		}
		Rpc(RpcDo_IA_LeaderboardRows, viewId, offset, last, rows);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_IA_LeaderboardHead(int viewId, int status, int total, int offset, string mine)
	{
		IA_StatisticsMenu.OnBoardHead(viewId, status, total, offset, mine);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_IA_LeaderboardRows(int viewId, int offset, bool last, string rows)
	{
		IA_StatisticsMenu.OnBoardRows(viewId, offset, last, rows);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask where this player stands on the session board. A hosting player reads it live.
	void IA_AskSessionStanding()
	{
		if (Replication.IsServer())
			return;

		Rpc(RpcAsk_IA_SessionStanding);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_SessionStanding()
	{
		int now = System.GetTickCount();
		if (now - m_iIA_LastStandingAskMs < IA_STANDING_MIN_GAP_MS)
			return;
		m_iIA_LastStandingAskMs = now;

		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (session)
			session.SendStanding(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell this player their own place on the session board.
	void IA_SendSessionStanding(int place, int total, int rankId, int kills, int deaths, int score)
	{
		if (GetPlayerId() == SCR_PlayerController.GetLocalPlayerId())
			return;

		Rpc(RpcDo_IA_SessionStanding, place, total, rankId, kills, deaths, score);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_IA_SessionStanding(int place, int total, int rankId, int kills, int deaths, int score)
	{
		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (session)
			session.OnLocalStanding(place, total, rankId, kills, deaths, score);
	}

	protected bool IA_IsAdminCaller()
	{
		if (!Replication.IsRunning())
			return true;

		return SCR_Global.IsAdmin(GetPlayerId());
	}
}
