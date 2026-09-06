//------------------------------------------------------------------------------------------------
//! Owns spawned base roots, the transient host, exclusion geometry, and
//! proximity-aware deferred cleanup.
//------------------------------------------------------------------------------------------------
class IA_DynamicSiteInstance
{
	static const float ASSEMBLY_RADIUS_M = 150;
	static const float CLEANUP_PLAYER_M = 600;
	static const int CLEANUP_TICK_SEC = 8;

	protected int m_iSerial;
	protected int m_iGroupId;
	protected int m_iSiteId;
	protected vector m_vOrigin;
	protected float m_fYawDeg;
	protected ref IA_DynamicSiteLayout m_Layout;
	protected ref IA_AreaInstance m_Host;
	protected ref array<IEntity> m_aRoots;
	protected ref array<ref IA_AiGroup> m_aGarrison;
	protected int m_iGarrisonBudget;
	protected int m_iGarrisonSpawned;
	protected bool m_bCleanupArmed;
	protected int m_iLastCleanupUnix;
	protected ref array<ref Tuple2<vector, vector>> m_aNavAreas;
	protected ref array<bool> m_aNavRedoRoads;
	protected ref SCR_MapMarkerBase m_MapMarker;
	protected Faction m_EnemyFaction;
	protected ref map<string, IEntity> m_Panels = new map<string, IEntity>();
	protected ref array<ref IA_StaticGunRecord> m_Emplacements = {};
	protected bool m_bEmplacementAssignmentsStopped;
	protected bool m_bEmplacementsRevealed;
	protected int m_iEmplacementTickMs;
	protected string m_sEmplacementBuildSummary;
	protected bool m_bEmplacementSummaryReported;

	//------------------------------------------------------------------------------------------------
	void IA_DynamicSiteInstance()
	{
		m_aRoots = new array<IEntity>();
		m_aGarrison = new array<ref IA_AiGroup>();
	}

	//------------------------------------------------------------------------------------------------
	static IA_DynamicSiteInstance Create(int serial, int groupId, vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, Faction enemyFaction)
	{
		ref IA_DynamicSiteInstance site = new IA_DynamicSiteInstance();
		site.m_iSerial = serial;
		site.m_iGroupId = groupId;
		site.m_iSiteId = serial * 100 + groupId;
		site.m_vOrigin = origin;
		site.m_fYawDeg = yawDeg;
		site.m_Layout = layout;
		site.m_EnemyFaction = enemyFaction;
		return site;
	}

	//------------------------------------------------------------------------------------------------
	IA_AreaInstance GetHost()
	{
		return m_Host;
	}

	//------------------------------------------------------------------------------------------------
	vector GetOrigin()
	{
		return m_vOrigin;
	}

	//------------------------------------------------------------------------------------------------
	float GetYawDeg()
	{
		return m_fYawDeg;
	}

	//------------------------------------------------------------------------------------------------
	IA_DynamicSiteLayout GetLayout()
	{
		return m_Layout;
	}

	//------------------------------------------------------------------------------------------------
	int GetSiteId()
	{
		return m_iSiteId;
	}

	//------------------------------------------------------------------------------------------------
	int GetSerial()
	{
		return m_iSerial;
	}

	//------------------------------------------------------------------------------------------------
	vector GetCapturePoint()
	{
		vector rootMat[4];
		m_Layout.BuildRootTransform(m_vOrigin, m_fYawDeg, rootMat);
		vector world = m_Layout.LocalOffsetToWorld(rootMat, m_Layout.m_vCaptureLocal);
		world[1] = IA_BasePlayerSampler.SampleSupportY(world);
		return world;
	}

	//------------------------------------------------------------------------------------------------
	vector GetAssemblyPoint()
	{
		vector world = m_vOrigin;
		world[1] = IA_BasePlayerSampler.SampleSupportY(world);
		return world;
	}

	//------------------------------------------------------------------------------------------------
	float GetCaptureRadius()
	{
		return m_Layout.m_fCaptureRadiusM;
	}

	//------------------------------------------------------------------------------------------------
	float GetHalfWidth()
	{
		return m_Layout.m_fHalfWidthM;
	}

	//------------------------------------------------------------------------------------------------
	float GetHalfDepth()
	{
		return m_Layout.m_fHalfDepthM;
	}

	//------------------------------------------------------------------------------------------------
	bool ContainsExpandedFootprint(vector position, float marginM)
	{
		return DistanceToFootprint(position) <= marginM;
	}

	//------------------------------------------------------------------------------------------------
	float DistanceToFootprint(vector position)
	{
		vector local = WorldToLocalFlat(position);
		float dx = 0;
		if (local[0] > m_Layout.m_fHalfWidthM)
			dx = local[0] - m_Layout.m_fHalfWidthM;
		else if (local[0] < -m_Layout.m_fHalfWidthM)
			dx = -m_Layout.m_fHalfWidthM - local[0];

		float dz = 0;
		if (local[2] > m_Layout.m_fHalfDepthM)
			dz = local[2] - m_Layout.m_fHalfDepthM;
		else if (local[2] < -m_Layout.m_fHalfDepthM)
			dz = -m_Layout.m_fHalfDepthM - local[2];

		if (dx == 0 && dz == 0)
			return 0;
		return Math.Sqrt((dx * dx) + (dz * dz));
	}

	//------------------------------------------------------------------------------------------------
	vector WorldToLocalFlat(vector world)
	{
		vector delta = world - m_vOrigin;
		vector rootMat[4];
		m_Layout.BuildRootTransform(m_vOrigin, m_fYawDeg, rootMat);
		return Vector(vector.Dot(delta, rootMat[0]), 0, vector.Dot(delta, rootMat[2]));
	}

	//------------------------------------------------------------------------------------------------
	bool OwnsEntity(IEntity ent)
	{
		if (!ent)
			return false;
		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity root = m_aRoots[i];
			if (!root)
				continue;
			if (root == ent)
				return true;
			IEntity parent = ent.GetParent();
			while (parent)
			{
				if (parent == root)
					return true;
				parent = parent.GetParent();
			}
		}
		return false;
	}

	// Trace exclusions must include children: compositions have most colliders there.
	void CollectOwnedEntities(notnull array<IEntity> entities)
	{
		foreach (IEntity root : m_aRoots)
			CollectEntityTree(root, entities);
	}

	protected void CollectEntityTree(IEntity entity, notnull array<IEntity> entities)
	{
		if (!entity)
			return;
		entities.Insert(entity);
		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectEntityTree(child, entities);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	void AddRoot(IEntity root)
	{
		if (!root)
			return;
		if (m_aRoots.Find(root) == -1)
			m_aRoots.Insert(root);
	}

	//------------------------------------------------------------------------------------------------
	int GetRootCount()
	{
		return m_aRoots.Count();
	}

	//------------------------------------------------------------------------------------------------
	int CountExpandedEntities()
	{
		int total = 0;
		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
			total = total + CountDescendants(m_aRoots[i]);
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountDescendants(IEntity ent)
	{
		if (!ent || ChimeraCharacter.Cast(ent))
			return 0; // Hardware ceiling: existing crew/players are not new hardware.
		int total = 1;
		IEntity child = ent.GetChildren();
		while (child)
		{
			total = total + CountDescendants(child);
			child = child.GetSibling();
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	bool CreateHost()
	{
		if (m_Host)
			return true;

		string areaName = "IA_Base_" + m_iSerial.ToString() + "_" + m_iGroupId.ToString();
		IA_Area area = IA_Area.CreateTransient(areaName, IA_AreaType.DynamicBase, GetAssemblyPoint(), ASSEMBLY_RADIUS_M);
		if (!area)
			return false;

		m_Host = IA_AreaInstance.Create(area, IA_Faction.USSR, m_EnemyFaction, 0, m_iGroupId, true);
		if (!m_Host)
			return false;

		IA_Game game = IA_Game.Instantiate();
		if (game)
			game.AddTransientArea(m_Host);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsHostLive()
	{
		if (!m_Host)
			return false;
		return !m_Host.IsShutDown();
	}

	//------------------------------------------------------------------------------------------------
	void RegisterPanel(string id, IEntity panel)
	{
		if (panel)
			m_Panels.Set(id, panel);
	}

	IEntity GetPanel(string id) { return m_Panels.Get(id); }
	Faction GetEnemyFaction() { return m_EnemyFaction; }
	array<ref IA_StaticGunRecord> GetEmplacements() { return m_Emplacements; }

	void AddEmplacement(IA_StaticGunRecord record)
	{
		if (!record || !record.m_Root)
			return;
		AddRoot(record.m_Root); // Register before component/inventory verification.
		m_Emplacements.Insert(record);
	}

	bool RemoveLastEmplacement()
	{
		if (m_bEmplacementsRevealed || m_Emplacements.IsEmpty())
			return false;
		int index = m_Emplacements.Count() - 1;
		ref IA_StaticGunRecord record = m_Emplacements[index];
		if (record.HasPlayerOccupantOrTransition())
			return false;
		if (record.m_Crew)
			record.m_Crew.ReleaseStaticGunAssignment(true);
		IEntity root = record.m_Root;
		m_Emplacements.Remove(index);
		m_aRoots.RemoveItem(root);
		if (root)
			SCR_EntityHelper.DeleteEntityAndChildren(root);
		return true;
	}

	bool HasEmplacementPlayerOccupant()
	{
		foreach (IA_StaticGunRecord record : m_Emplacements)
		{
			if (record && record.HasPlayerOccupantOrTransition())
				return true;
		}
		return false;
	}

	void TickEmplacements(bool constructing = false)
	{
		if (!Replication.IsServer() || m_Emplacements.IsEmpty())
			return;
		int now = System.GetTickCount();
		if (m_iEmplacementTickMs && now - m_iEmplacementTickMs < 1000)
			return;
		m_iEmplacementTickMs = now;
		bool live = IsHostLive() && !m_bCleanupArmed;
		if (!live)
			StopEmplacementAssignments(false);
		foreach (IA_StaticGunRecord record : m_Emplacements)
		{
			if (record && record.m_Crew)
				record.m_Crew.TickStaticGunAssignment(constructing && !m_bEmplacementAssignmentsStopped, live);
		}
	}

	bool EmplacementMountsPending()
	{
		foreach (IA_StaticGunRecord record : m_Emplacements)
		{
			if (record && record.m_Crew && record.m_Crew.IsStaticGunMountPending())
				return true;
		}
		return false;
	}

	void SetEmplacementBuildSummary(string summary)
	{
		m_sEmplacementBuildSummary = summary;
	}

	void ReportEmplacements()
	{
		if (m_bEmplacementSummaryReported)
			return;
		if (m_sEmplacementBuildSummary.IsEmpty() && m_Emplacements.IsEmpty())
			return;
		if (IA_Log.IsDebugEnabled())
		{
			m_bEmplacementSummaryReported = true;
			int pkm, nsv, scoped, aa, assigned, fallback;
			foreach (IA_StaticGunRecord record : m_Emplacements)
			{
				if (record.m_Profile.m_iKind == 3)
					aa++;
				else if (record.m_Profile.m_iKind == 2)
					scoped++;
				else if (record.m_Profile.m_iBeltSize == 50)
					nsv++;
				else
					pkm++;
				if (record.m_Crew)
				{
					if (record.m_Crew.IsStaticGunMounted())
						assigned++;
					else
						fallback++;
				}
			}
			Print(string.Format("[IA][Emplacements] site=%1 PKM=%2 NSV=%3 scopedNSV=%4 AA=%5 crew=%6 fallback=%7 %8", m_iSiteId, pkm, nsv, scoped, aa, assigned, fallback, m_sEmplacementBuildSummary), LogLevel.NORMAL);
		}
	}

	void RevealEmplacements()
	{
		if (!m_bEmplacementsRevealed && m_Layout && m_Layout.m_bComposed)
			IA_BaseDesignLibrary.Committed(m_Layout.m_iDesignVariant);
		m_bEmplacementsRevealed = true;
		ReportEmplacements();
	}

	void StopEmplacementAssignments(bool restoreDefense)
	{
		m_bEmplacementAssignmentsStopped = true;
		foreach (IA_StaticGunRecord record : m_Emplacements)
		{
			if (record && record.m_Crew)
				record.m_Crew.ReleaseStaticGunAssignment(restoreDefense);
		}
	}

	void AddGarrisonGroup(IA_AiGroup group)
	{
		if (!group)
			return;
		m_aGarrison.Insert(group);
		if (m_Host)
			m_Host.AddMilitaryGroup(group);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveFailedEmplacementCrew(IA_AiGroup group)
	{
		if (!group || group.GetSpawnedUnitCount() > 0)
			return;
		group.ReleaseStaticGunAssignment(false);
		group.CancelPendingUnitSpawns();
		group.Despawn();
		m_aGarrison.RemoveItem(group);
		if (m_Host)
			m_Host.RemoveMilitaryGroup(group);
	}

	int GetGarrisonBudget()
	{
		return m_iGarrisonBudget;
	}

	//------------------------------------------------------------------------------------------------
	void SetGarrisonBudget(int budget)
	{
		m_iGarrisonBudget = budget;
	}

	//------------------------------------------------------------------------------------------------
	int CountLivingGarrison()
	{
		int alive = 0;
		int count = m_aGarrison.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup group = m_aGarrison[i];
			if (group)
				alive = alive + group.GetAliveCount();
		}
		return alive;
	}

	//------------------------------------------------------------------------------------------------
	int CountPendingGarrison()
	{
		int pending = 0;
		int count = m_aGarrison.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup group = m_aGarrison[i];
			if (group)
				pending = pending + group.GetPendingUnitCount();
		}
		return pending;
	}

	//------------------------------------------------------------------------------------------------
	bool IsGarrisonReady()
	{
		if (CountPendingGarrison() > 0)
			return false;
		if (CountLivingGarrison() < m_iGarrisonBudget)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	vector GetGuardPostWorld(int index)
	{
		if (!m_Layout || !m_Layout.m_aGuardPosts)
			return m_vOrigin;
		if (index < 0 || index >= m_Layout.m_aGuardPosts.Count())
			return m_vOrigin;

		vector rootMat[4];
		m_Layout.BuildRootTransform(m_vOrigin, m_fYawDeg, rootMat);
		vector world = m_Layout.LocalOffsetToWorld(rootMat, m_Layout.m_aGuardPosts[index]);
		world[1] = IA_BasePlayerSampler.SampleSupportY(world);
		return world;
	}

	//------------------------------------------------------------------------------------------------
	vector GetPerimeterStationWorld(int index)
	{
		if (!m_Layout || !m_Layout.m_aPerimeterStations)
			return m_vOrigin;
		if (index < 0 || index >= m_Layout.m_aPerimeterStations.Count())
			return m_vOrigin;

		vector rootMat[4];
		m_Layout.BuildRootTransform(m_vOrigin, m_fYawDeg, rootMat);
		vector world = m_Layout.LocalOffsetToWorld(rootMat, m_Layout.m_aPerimeterStations[index]);
		world[1] = IA_BasePlayerSampler.SampleSupportY(world);
		return world;
	}

	//------------------------------------------------------------------------------------------------
	int GetGuardPostCount()
	{
		if (!m_Layout || !m_Layout.m_aGuardPosts)
			return 0;
		return m_Layout.m_aGuardPosts.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetPerimeterStationCount()
	{
		if (!m_Layout || !m_Layout.m_aPerimeterStations)
			return 0;
		return m_Layout.m_aPerimeterStations.Count();
	}

	//------------------------------------------------------------------------------------------------
	void SpawnMapMarker()
	{
		RemoveMapMarker();
		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr)
			return;

		ref SCR_MapMarkerBase marker = mgr.PrepareMilitaryMarker(EMilitarySymbolIdentity.OPFOR, EMilitarySymbolDimension.LAND, EMilitarySymbolIcon.MOBILEHQ);
		if (!marker)
			return;

		vector pos = GetCapturePoint();
		marker.SetWorldPos(pos[0], pos[2]);
		marker.SetCustomText("Enemy operating base");
		marker.SetCanBeRemovedByOwner(false);
		mgr.InsertStaticMarker(marker, false, true);
		m_MapMarker = marker;
	}

	//------------------------------------------------------------------------------------------------
	void RemoveMapMarker()
	{
		if (!m_MapMarker)
			return;

		// Keep a local ref: RemoveStaticMarker drops the manager's ref, then
		// calls GetMarkerID() again. A non-ref member is collected in between.
		ref SCR_MapMarkerBase marker = m_MapMarker;
		m_MapMarker = null;

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr)
			return;

		int markerId = marker.GetMarkerID();
		if (markerId != -1)
		{
			if (!mgr.GetStaticMarkerByID(markerId) && !mgr.GetDisabledMarkerByID(markerId))
				return;
		}

		mgr.RemoveStaticMarker(marker);
	}

	//------------------------------------------------------------------------------------------------
	void SnapshotNavBounds()
	{
		m_aNavAreas = new array<ref Tuple2<vector, vector>>();
		m_aNavRedoRoads = new array<bool>();
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity root = m_aRoots[i];
			if (root)
				aiWorld.GetNavmeshRebuildAreas(root, m_aNavAreas, m_aNavRedoRoads);
		}
	}

	//------------------------------------------------------------------------------------------------
	void RequestNavRebuild()
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity root = m_aRoots[i];
			if (root)
				aiWorld.RequestNavmeshRebuildEntity(root);
		}
	}

	//------------------------------------------------------------------------------------------------
	void CancelOwnedSpawns()
	{
		int count = m_aGarrison.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup group = m_aGarrison[i];
			if (group)
				group.CancelPendingUnitSpawns();
		}
		if (m_Host)
			m_Host.CancelPendingSpawns();
	}

	//------------------------------------------------------------------------------------------------
	void BeginDeferredCleanup()
	{
		if (m_bCleanupArmed)
			return;
		m_bCleanupArmed = true;
		ReportEmplacements();
		StopEmplacementAssignments(false);
		m_iLastCleanupUnix = 0;
		RemoveMapMarker();
		CancelOwnedSpawns();
		if (m_Host && !m_Host.IsShutDown())
			m_Host.ForceFinish();
		SnapshotNavBounds();
	}

	//------------------------------------------------------------------------------------------------
	bool TickCleanup()
	{
		if (!m_bCleanupArmed)
			return false;

		int now = System.GetUnixTime();
		if (m_iLastCleanupUnix != 0 && (now - m_iLastCleanupUnix) < CLEANUP_TICK_SEC)
			return false;
		m_iLastCleanupUnix = now;

		if (PlayersNearbyOrOccupying())
			return false;

		if (!DeleteRoots())
			return false;
		RequestSavedNavRebuild();
		m_aRoots.Clear();
		m_Panels.Clear();
		m_Emplacements.Clear();
		m_aGarrison.Clear();
		m_Host = null;
		m_bCleanupArmed = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool PlayersNearbyOrOccupying()
	{
		if (HasEmplacementPlayerOccupant())
			return true;
		ref array<vector> players = new array<vector>();
		IA_SpawnPlacement.CollectPlayerPositions(players);
		int count = players.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (DistanceToFootprint(players[i]) <= CLEANUP_PLAYER_M)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool DeleteRoots()
	{
		// Independent final guard protects direct/abort callers as well as TickCleanup.
		if (HasEmplacementPlayerOccupant())
			return false;
		StopEmplacementAssignments(false);
		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity root = m_aRoots[i];
			if (root)
				SCR_EntityHelper.DeleteEntityAndChildren(root);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestSavedNavRebuild()
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		if (!m_aNavAreas || m_aNavAreas.IsEmpty())
			return;
		if (!m_aNavRedoRoads)
			m_aNavRedoRoads = new array<bool>();
		aiWorld.RequestNavmeshRebuildAreas(m_aNavAreas, m_aNavRedoRoads);
	}

	//------------------------------------------------------------------------------------------------
}
