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
	protected IA_AreaInstance m_Host;
	protected ref array<IEntity> m_aRoots;
	protected ref array<ref IA_AiGroup> m_aGarrison;
	protected int m_iGarrisonBudget;
	protected int m_iGarrisonSpawned;
	protected bool m_bCleanupArmed;
	protected int m_iLastCleanupUnix;
	protected ref array<ref Tuple2<vector, vector>> m_aNavAreas;
	protected ref array<bool> m_aNavRedoRoads;
	protected SCR_MapMarkerBase m_MapMarker;
	protected Faction m_EnemyFaction;

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
		float yaw = m_fYawDeg * Math.DEG2RAD;
		float c = Math.Cos(yaw);
		float s = Math.Sin(yaw);
		float lx = (delta[0] * c) + (delta[2] * s);
		float lz = (-delta[0] * s) + (delta[2] * c);
		return Vector(lx, 0, lz);
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
		if (!ent)
			return 0;
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
	void AddGarrisonGroup(IA_AiGroup group)
	{
		if (!group)
			return;
		m_aGarrison.Insert(group);
		if (m_Host)
			m_Host.AddMilitaryGroup(group);
	}

	//------------------------------------------------------------------------------------------------
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
		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (mgr)
			mgr.RemoveStaticMarker(m_MapMarker);
		m_MapMarker = null;
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
				group.Despawn();
		}
		if (m_Host)
			m_Host.CancelPendingSpawns();
	}

	//------------------------------------------------------------------------------------------------
	void BeginDeferredCleanup()
	{
		m_bCleanupArmed = true;
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

		DeleteRoots();
		RequestSavedNavRebuild();
		m_aRoots.Clear();
		m_aGarrison.Clear();
		m_Host = null;
		m_bCleanupArmed = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool PlayersNearbyOrOccupying()
	{
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
	void DeleteRoots()
	{
		int count = m_aRoots.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity root = m_aRoots[i];
			if (root)
				IA_Game.AddEntityToGc(root);
		}
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
	void ImmediateRollback()
	{
		CancelOwnedSpawns();
		DeleteRoots();
		m_aRoots.Clear();
		if (m_Host && !m_Host.IsShutDown())
			m_Host.ForceFinish();
		IA_Game game = IA_Game.Instantiate();
		if (game && m_Host)
			game.RemoveTransientArea(m_Host);
		m_Host = null;
		RemoveMapMarker();
	}
}
