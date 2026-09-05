//------------------------------------------------------------------------------------------------
//! Candidate shortlist, footprint validation, staged spawn, and rollback.
//! Never announces tasks or advances the AO.
//------------------------------------------------------------------------------------------------
class IA_DynamicSitePlacer
{
	static const float AO_MARGIN_M = 350;
	static const float PLAYER_CLEAR_M = 250;
	static const float GRID_M = 10;
	static const int MAX_SAMPLE_CENTERS = 96;
	static const int MAX_SHORTLIST = 12;
	static const int PLACE_DEADLINE_MS = 45000;
	static const int MAX_ROOTS = 96;
	static const int MAX_EXPANDED = 600;
	static const int NAV_RECHECK_MS = 15000;
	static const float LANE_SAMPLE_M = 3;

	protected int m_iSerial;
	protected int m_iGroupId;
	protected ref IA_BaseObjectiveSettings m_Settings;
	protected ref RandomGenerator m_Rng;
	protected int m_iSeed;
	protected ref array<ref IA_DynamicSiteCandidate> m_aShortlist;
	protected ref IA_DynamicSiteResult m_Result;
	protected ref IA_DynamicSiteInstance m_Building;
	protected ref IA_DynamicSiteLayout m_ActiveLayout;
	protected int m_iWorkStep;
	protected int m_iCandidateIndex;
	protected int m_iModuleIndex;
	protected int m_iGarrisonLeft;
	protected int m_iGuardSlot;
	protected int m_iPlaceStartMs;
	protected int m_iNavWaitStartMs;
	protected bool m_bComplete;
	protected bool m_bWorkQueued;
	protected bool m_bPrecomputeOnly;

	//------------------------------------------------------------------------------------------------
	void IA_DynamicSitePlacer()
	{
		m_aShortlist = new array<ref IA_DynamicSiteCandidate>();
		m_Rng = new RandomGenerator();
	}

	//------------------------------------------------------------------------------------------------
	void BeginPrecompute(int serial, int groupId)
	{
		m_iSerial = serial;
		m_iGroupId = groupId;
		m_iSeed = serial * 7919 + groupId * 104729 + System.GetUnixTime();
		m_Rng.SetSeed(m_iSeed);
		m_aShortlist.Clear();
		m_bComplete = false;
		m_bPrecomputeOnly = true;
		m_Result = null;
		m_Building = null;
		BuildShortlist();
	}

	//------------------------------------------------------------------------------------------------
	void BeginPlacement(int serial, IA_BaseObjectiveSettings settings)
	{
		if (serial != m_iSerial)
			return;

		m_Settings = settings;
		m_bPrecomputeOnly = false;
		m_bComplete = false;
		m_Result = null;
		m_Building = null;
		m_iWorkStep = 1;
		m_iCandidateIndex = 0;
		m_iModuleIndex = 0;
		m_iGarrisonLeft = 0;
		m_iGuardSlot = 0;
		m_iPlaceStartMs = System.GetTickCount();
		m_iNavWaitStartMs = 0;
		QueueWork();
	}

	//------------------------------------------------------------------------------------------------
	void Cancel(int serial)
	{
		if (serial != m_iSerial && serial != 0)
			return;
		m_bComplete = true;
		m_bWorkQueued = false;
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(this.ProcessWork);
		if (m_Building)
			m_Building.ImmediateRollback();
		m_Building = null;
	}

	//------------------------------------------------------------------------------------------------
	bool IsComplete()
	{
		return m_bComplete;
	}

	//------------------------------------------------------------------------------------------------
	IA_DynamicSiteResult TakeResult()
	{
		IA_DynamicSiteResult result = m_Result;
		m_Result = null;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	void ProcessWork()
	{
		m_bWorkQueued = false;
		if (m_bComplete)
			return;
		if (m_bPrecomputeOnly)
			return;

		int now = System.GetTickCount();
		if ((now - m_iPlaceStartMs) > PLACE_DEADLINE_MS)
		{
			FailCurrent("timeout");
			FinishFailure("timeout");
			return;
		}

		if (m_iWorkStep == 1)
			StepPickCandidate();
		else if (m_iWorkStep == 2)
			StepSpawnModules();
		else if (m_iWorkStep == 3)
			StepNavRecheck();
		else if (m_iWorkStep == 4)
			StepGarrison();
		else if (m_iWorkStep == 5)
			StepRevealAssert();

		if (!m_bComplete)
			QueueWork();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueWork()
	{
		if (m_bWorkQueued || m_bComplete)
			return;
		m_bWorkQueued = true;
		GetGame().GetCallqueue().CallLater(this.ProcessWork, 200, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StepPickCandidate()
	{
		RefreshShortlistScores();
		if (m_iCandidateIndex >= m_aShortlist.Count())
		{
			FinishFailure("no_legal_site");
			return;
		}

		IA_DynamicSiteCandidate cand = m_aShortlist[m_iCandidateIndex];
		m_iCandidateIndex = m_iCandidateIndex + 1;
		if (!cand)
			return;
		if (!ValidateCandidate(cand))
			return;

		m_ActiveLayout = IA_DynamicSiteLayout.CreateById(cand.m_iLayoutId);
		if (!PreflightResources(m_ActiveLayout))
			return;

		m_Building = IA_DynamicSiteInstance.Create(m_iSerial, m_iGroupId, cand.m_vCenter, cand.m_fYawDeg, m_ActiveLayout, ResolveEnemyFaction());
		m_iModuleIndex = 0;
		m_iWorkStep = 2;
	}

	//------------------------------------------------------------------------------------------------
	protected void StepSpawnModules()
	{
		if (!m_Building || !m_ActiveLayout)
		{
			m_iWorkStep = 1;
			return;
		}

		if (!ValidatePlayerClearance(m_Building.GetOrigin(), m_Building.GetYawDeg(), m_ActiveLayout, m_Building))
		{
			FailCurrent("player_approached");
			return;
		}

		int spawned = 0;
		int total = m_ActiveLayout.m_aModules.Count();
		while (m_iModuleIndex < total && spawned < 2)
		{
			IA_DynamicSiteModule mod = m_ActiveLayout.m_aModules[m_iModuleIndex];
			m_iModuleIndex = m_iModuleIndex + 1;
			if (!mod)
				continue;
			if (!SpawnModule(m_Building, mod))
			{
				if (mod.m_bRequired)
				{
					FailCurrent("spawn_failed");
					return;
				}
				continue;
			}
			spawned = spawned + 1;
			if (m_Building.GetRootCount() > MAX_ROOTS)
			{
				FailCurrent("root_ceiling");
				return;
			}
			if (m_Building.CountExpandedEntities() > MAX_EXPANDED)
			{
				FailCurrent("entity_ceiling");
				return;
			}
		}

		if (m_iModuleIndex >= total)
		{
			m_Building.RequestNavRebuild();
			m_iNavWaitStartMs = System.GetTickCount();
			m_iWorkStep = 3;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StepNavRecheck()
	{
		if (!m_Building)
		{
			m_iWorkStep = 1;
			return;
		}

		if (ValidateInteriorRoutes(m_Building.GetOrigin(), m_Building.GetYawDeg(), m_ActiveLayout, true))
		{
			m_iWorkStep = 4;
			m_iGarrisonLeft = 0;
			if (m_Settings)
				m_iGarrisonLeft = m_Settings.ComputeGarrisonBudget();
			m_Building.SetGarrisonBudget(m_iGarrisonLeft);
			m_iGuardSlot = 0;
			return;
		}

		if ((System.GetTickCount() - m_iNavWaitStartMs) > NAV_RECHECK_MS)
			FailCurrent("nav_invalid");
	}

	//------------------------------------------------------------------------------------------------
	protected void StepGarrison()
	{
		if (!m_Building)
		{
			m_iWorkStep = 1;
			return;
		}

		if (!m_Building.CreateHost())
		{
			FailCurrent("host_failed");
			return;
		}

		if (m_iGarrisonLeft <= 0)
		{
			m_iWorkStep = 5;
			return;
		}

		int size = 4;
		if (m_iGuardSlot < 2)
			size = 2;
		if (size > m_iGarrisonLeft)
			size = m_iGarrisonLeft;

		vector post = ResolveNextGuardPost();
		IA_AiGroup group = IA_AiGroup.CreateMilitaryGroupFromUnits(post, IA_Faction.USSR, size, ResolveEnemyFaction(), false, true, false, false);
		if (!group)
		{
			FailCurrent("garrison_failed");
			return;
		}

		IA_AreaInstance host = m_Building.GetHost();
		if (host && host.GetArea())
			group.SetAssignedArea(host.GetArea());
		group.SetHoldPost(post, 5);
		group.Spawn(IA_AiOrder.Hold, post);
		m_Building.AddGarrisonGroup(group);
		m_iGarrisonLeft = m_iGarrisonLeft - size;
		m_iGuardSlot = m_iGuardSlot + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void StepRevealAssert()
	{
		if (!m_Building || !m_Building.IsHostLive())
		{
			FailCurrent("host_lost");
			return;
		}
		if (!m_Building.IsGarrisonReady())
		{
			if (m_Building.CountLivingGarrison() < m_Building.GetGarrisonBudget() && m_Building.CountPendingGarrison() == 0)
			{
				FailCurrent("garrison_dead");
				return;
			}
			return;
		}
		if (!ValidatePlayerClearance(m_Building.GetOrigin(), m_Building.GetYawDeg(), m_ActiveLayout, m_Building))
		{
			FailCurrent("reveal_visible");
			return;
		}

		FinishSuccess();
	}

	//------------------------------------------------------------------------------------------------
	protected vector ResolveNextGuardPost()
	{
		int posts = m_Building.GetGuardPostCount();
		if (m_iGuardSlot < posts)
			return m_Building.GetGuardPostWorld(m_iGuardSlot);

		int extra = m_iGuardSlot - posts;
		int stations = m_Building.GetPerimeterStationCount();
		if (stations > 0)
			return m_Building.GetPerimeterStationWorld(extra % stations);
		return m_Building.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected void FailCurrent(string reason)
	{
		Print(string.Format("[IA][Base] Placement candidate failed: %1", reason), LogLevel.WARNING);
		if (m_Building)
		{
			if (m_Building.PlayersNearbyOrOccupying())
				m_Building.BeginDeferredCleanup();
			else
				m_Building.ImmediateRollback();
		}
		m_Building = null;
		m_ActiveLayout = null;
		m_iWorkStep = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishSuccess()
	{
		ref IA_DynamicSiteResult result = new IA_DynamicSiteResult();
		result.m_iSerial = m_iSerial;
		result.m_bSuccess = true;
		result.m_sReason = "ok";
		result.m_Site = m_Building;
		m_Result = result;
		m_Building = null;
		m_bComplete = true;
		Print(string.Format("[IA][Base] Placement committed serial=%1 site=%2", m_iSerial, result.m_Site.GetSiteId()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishFailure(string reason)
	{
		ref IA_DynamicSiteResult result = new IA_DynamicSiteResult();
		result.m_iSerial = m_iSerial;
		result.m_bSuccess = false;
		result.m_sReason = reason;
		m_Result = result;
		m_bComplete = true;
		Print(string.Format("[IA][Base] Placement failed serial=%1 reason=%2", m_iSerial, reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction ResolveEnemyFaction()
	{
		if (m_Settings && m_Settings.m_EnemyFaction)
			return m_Settings.m_EnemyFaction;
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (init)
			return init.GetRandomEnemyFaction();
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildShortlist()
	{
		m_aShortlist.Clear();
		ref array<vector> centers = new array<vector>();
		ref array<float> radii = new array<float>();
		CollectPrimaryCircles(centers, radii);
		if (centers.IsEmpty())
			return;

		int attempts = 0;
		while (attempts < MAX_SAMPLE_CENTERS && m_aShortlist.Count() < MAX_SHORTLIST)
		{
			attempts = attempts + 1;
			int pick = m_Rng.RandInt(0, centers.Count());
			if (pick >= centers.Count())
				pick = 0;
			vector c = centers[pick];
			float r = radii[pick] + AO_MARGIN_M;
			float ang = m_Rng.RandFloat01() * 6.283185;
			float dist = m_Rng.RandFloat01() * r;
			vector sample = Vector(c[0] + (Math.Cos(ang) * dist), 0, c[2] + (Math.Sin(ang) * dist));
			sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);
			if (!IsInsideUnion(sample, centers, radii))
				continue;
			if (IA_SpawnPlacement.IsInOcean(sample))
				continue;
			if (IA_SpawnPlacement.GetSlopeTangent(sample, 10) > 0.25)
				continue;

			ref IA_DynamicSiteCandidate cand = new IA_DynamicSiteCandidate();
			cand.m_vCenter = sample;
			cand.m_fYawDeg = 0;
			cand.m_iLayoutId = IA_DynamicSiteLayout.LAYOUT_FULL;
			cand.m_iSeed = m_iSeed + attempts;
			cand.m_fScore = 999999;
			m_aShortlist.Insert(cand);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectPrimaryCircles(notnull array<vector> centers, notnull array<float> radii)
	{
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		if (!markers)
			return;

		int count = markers.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			if (marker.m_areaGroup != m_iGroupId)
				continue;
			IA_AreaType t = marker.GetAreaType();
			if (t == IA_AreaType.DefendObjective)
				continue;
			if (t == IA_AreaType.MortarPit)
				continue;
			if (t == IA_AreaType.RadioTower)
				continue;
			if (t == IA_AreaType.Assassination)
				continue;
			centers.Insert(marker.GetOrigin());
			radii.Insert(marker.GetRadius());
		}

		if (!centers.IsEmpty())
			return;

		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			if (marker.m_areaGroup != m_iGroupId)
				continue;
			IA_AreaType t = marker.GetAreaType();
			if (t == IA_AreaType.DefendObjective)
				continue;
			if (t == IA_AreaType.MortarPit)
				continue;
			centers.Insert(marker.GetOrigin());
			radii.Insert(marker.GetRadius());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInsideUnion(vector pos, notnull array<vector> centers, notnull array<float> radii)
	{
		int count = centers.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			float r = radii[i] + AO_MARGIN_M;
			if (IA_AreaMarker.IsWorldPosInsideCaptureRadius(pos, centers[i], r))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshShortlistScores()
	{
		ref array<vector> players = new array<vector>();
		CollectDeployedPlayerPositions(players);
		int count = m_aShortlist.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_DynamicSiteCandidate cand = m_aShortlist[i];
			if (!cand)
				continue;
			cand.m_fScore = ScoreCenter(cand.m_vCenter, players);
		}
		SortShortlist();
	}

	//------------------------------------------------------------------------------------------------
	protected float ScoreCenter(vector center, notnull array<vector> players)
	{
		if (players.IsEmpty())
			return 0;

		ref array<float> dists = new array<float>();
		int count = players.Count();
		int i;
		for (i = 0; i < count; i++)
			dists.Insert(vector.Distance(center, players[i]));
		dists.Sort();
		float median = dists[dists.Count() / 2];
		int p90i = Math.Round((dists.Count() - 1) * 0.90);
		if (p90i < 0)
			p90i = 0;
		if (p90i >= dists.Count())
			p90i = dists.Count() - 1;
		float spread = IA_SpawnPlacement.GetFootprintHeightDelta(center, 12);
		return median + (0.35 * dists[p90i]) + (100 * spread);
	}

	//------------------------------------------------------------------------------------------------
	protected void SortShortlist()
	{
		int n = m_aShortlist.Count();
		int i;
		int j;
		for (i = 0; i < n; i++)
		{
			for (j = i + 1; j < n; j++)
			{
				if (m_aShortlist[j].m_fScore < m_aShortlist[i].m_fScore)
				{
					IA_DynamicSiteCandidate tmp = m_aShortlist[i];
					m_aShortlist[i] = m_aShortlist[j];
					m_aShortlist[j] = tmp;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectDeployedPlayerPositions(notnull array<vector> outPos)
	{
		outPos.Clear();
		IA_SpawnPlacement.CollectPlayerPositions(outPos);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateCandidate(notnull IA_DynamicSiteCandidate cand)
	{
		int sizeMode = 0;
		if (m_Settings)
			sizeMode = m_Settings.m_iSizeMode;

		ref array<int> layouts = new array<int>();
		if (sizeMode == IA_DynamicSiteSizeMode.Compact)
			layouts.Insert(IA_DynamicSiteLayout.LAYOUT_COMPACT);
		else if (sizeMode == IA_DynamicSiteSizeMode.Full)
			layouts.Insert(IA_DynamicSiteLayout.LAYOUT_FULL);
		else
		{
			layouts.Insert(IA_DynamicSiteLayout.LAYOUT_FULL);
			layouts.Insert(IA_DynamicSiteLayout.LAYOUT_COMPACT);
		}

		ref array<float> headings = new array<float>();
		headings.Insert(0);
		headings.Insert(90);
		headings.Insert(180);
		headings.Insert(270);

		int li;
		int hi;
		for (li = 0; li < layouts.Count(); li++)
		{
			IA_DynamicSiteLayout layout = IA_DynamicSiteLayout.CreateById(layouts[li]);
			for (hi = 0; hi < headings.Count(); hi++)
			{
				float yaw = headings[hi];
				if (!FootprintInsideUnion(cand.m_vCenter, yaw, layout))
					continue;
				if (!ValidateTerrain(cand.m_vCenter, yaw, layout))
					continue;
				if (!ValidatePlayerClearance(cand.m_vCenter, yaw, layout, null))
					continue;
				if (!ValidateInteriorRoutes(cand.m_vCenter, yaw, layout, false))
					continue;

				cand.m_fYawDeg = yaw;
				cand.m_iLayoutId = layouts[li];
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FootprintInsideUnion(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout)
	{
		ref array<vector> centers = new array<vector>();
		ref array<float> radii = new array<float>();
		CollectPrimaryCircles(centers, radii);
		if (centers.IsEmpty())
			return false;

		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		ref array<vector> corners = new array<vector>();
		corners.Insert(layout.LocalOffsetToWorld(rootMat, Vector(-layout.m_fHalfWidthM, 0, -layout.m_fHalfDepthM)));
		corners.Insert(layout.LocalOffsetToWorld(rootMat, Vector(layout.m_fHalfWidthM, 0, -layout.m_fHalfDepthM)));
		corners.Insert(layout.LocalOffsetToWorld(rootMat, Vector(-layout.m_fHalfWidthM, 0, layout.m_fHalfDepthM)));
		corners.Insert(layout.LocalOffsetToWorld(rootMat, Vector(layout.m_fHalfWidthM, 0, layout.m_fHalfDepthM)));
		int i;
		for (i = 0; i < corners.Count(); i++)
		{
			if (!IsInsideUnion(corners[i], centers, radii))
				return false;
		}

		float x = -layout.m_fHalfWidthM;
		while (x <= layout.m_fHalfWidthM)
		{
			float z = -layout.m_fHalfDepthM;
			while (z <= layout.m_fHalfDepthM)
			{
				vector p = layout.LocalOffsetToWorld(rootMat, Vector(x, 0, z));
				if (!IsInsideUnion(p, centers, radii))
					return false;
				z = z + GRID_M;
			}
			x = x + GRID_M;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateTerrain(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return false;

		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		float minY = 99999;
		float maxY = -99999;
		float x = -layout.m_fHalfWidthM;
		while (x <= layout.m_fHalfWidthM)
		{
			float z = -layout.m_fHalfDepthM;
			while (z <= layout.m_fHalfDepthM)
			{
				vector p = layout.LocalOffsetToWorld(rootMat, Vector(x, 0, z));
				p[1] = world.GetSurfaceY(p[0], p[2]);
				if (IA_SpawnPlacement.IsInOcean(p))
					return false;
				if (p[1] < minY)
					minY = p[1];
				if (p[1] > maxY)
					maxY = p[1];
				z = z + GRID_M;
			}
			x = x + GRID_M;
		}
		if ((maxY - minY) > 4)
			return false;

		int n = layout.m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			IA_DynamicSiteModule mod = layout.m_aModules[i];
			if (!mod)
				continue;
			if (!ValidateModulePad(rootMat, mod))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateModulePad(vector rootMat[4], notnull IA_DynamicSiteModule mod)
	{
		vector worldMat[4];
		IA_DynamicSiteLayout dummy = IA_DynamicSiteLayout.CreateFull();
		dummy.LocalToWorld(rootMat, mod.m_vLocalPosition, mod.m_fLocalYawDeg, mod.m_fPrefabYawCorrectionDeg, worldMat);
		float minY = 99999;
		float maxY = -99999;
		int n = mod.m_aSupportPoints.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			vector local = mod.m_aSupportPoints[i];
			vector p = worldMat[3] + (worldMat[0] * local[0]) + (worldMat[2] * local[2]);
			p[1] = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
			if (IA_SpawnPlacement.IsInOcean(p))
				return false;
			if (p[1] < minY)
				minY = p[1];
			if (p[1] > maxY)
				maxY = p[1];
		}
		if ((maxY - minY) > mod.m_fMaxSupportDeltaM)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidatePlayerClearance(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, IA_DynamicSiteInstance ignoreSite)
	{
		ref array<vector> players = new array<vector>();
		IA_SpawnPlacement.CollectPlayerPositions(players);
		int count = players.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (DistanceToOrientedRect(players[i], origin, yawDeg, layout) < PLAYER_CLEAR_M)
				return false;
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return true;

		array<int> ids = {};
		pm.GetPlayers(ids);
		int idCount = ids.Count();
		for (i = 0; i < idCount; i++)
		{
			IEntity pawn = pm.GetPlayerControlledEntity(ids[i]);
			if (!pawn)
				continue;
			if (HasClearLosToBase(pawn, origin, yawDeg, layout, ignoreSite))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected float DistanceToOrientedRect(vector world, vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout)
	{
		ref IA_DynamicSiteInstance tmp = IA_DynamicSiteInstance.Create(0, 0, origin, yawDeg, layout, null);
		return tmp.DistanceToFootprint(world);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasClearLosToBase(IEntity observer, vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, IA_DynamicSiteInstance ignoreSite)
	{
		vector eye;
		if (!IA_AreaMarker.TryGetPawnWorldPos(observer, eye))
			return false;
		eye[1] = eye[1] + 1.6;

		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		ref array<vector> samples = new array<vector>();
		samples.Insert(origin);
		samples.Insert(layout.LocalOffsetToWorld(rootMat, Vector(-layout.m_fHalfWidthM, 0, -layout.m_fHalfDepthM)));
		samples.Insert(layout.LocalOffsetToWorld(rootMat, Vector(layout.m_fHalfWidthM, 0, -layout.m_fHalfDepthM)));
		samples.Insert(layout.LocalOffsetToWorld(rootMat, Vector(-layout.m_fHalfWidthM, 0, layout.m_fHalfDepthM)));
		samples.Insert(layout.LocalOffsetToWorld(rootMat, Vector(layout.m_fHalfWidthM, 0, layout.m_fHalfDepthM)));
		samples.Insert(layout.LocalOffsetToWorld(rootMat, layout.m_vCaptureLocal) + Vector(0, 6, 0));

		World world = GetGame().GetWorld();
		int i;
		for (i = 0; i < samples.Count(); i++)
		{
			vector target = samples[i];
			target[1] = target[1] + 2;
			ref TraceParam p = new TraceParam();
			p.Start = eye;
			p.End = target;
			p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			p.Exclude = observer;
			float hit = world.TraceMove(p, null);
			if (hit >= 0.99)
				return true;
			if (ignoreSite && p.TraceEnt && ignoreSite.OwnsEntity(p.TraceEnt))
				continue;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateInteriorRoutes(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, bool includeProps)
	{
		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		vector capture = layout.LocalOffsetToWorld(rootMat, layout.m_vCaptureLocal);
		int reachable = 0;
		int i;
		int n = layout.m_aEntries.Count();
		for (i = 0; i < n; i++)
		{
			vector entry = layout.LocalOffsetToWorld(rootMat, layout.m_aEntries[i]);
			if (LatticeConnected(entry, capture, origin, yawDeg, layout))
				reachable = reachable + 1;
		}
		return reachable >= 2;
	}

	//------------------------------------------------------------------------------------------------
	protected bool LatticeConnected(vector from, vector to, vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout)
	{
		vector delta = to - from;
		delta[1] = 0;
		float len = delta.Length();
		if (len < 1)
			return true;
		vector dir = delta / len;
		int steps = Math.Ceil(len / LANE_SAMPLE_M);
		int i;
		for (i = 0; i <= steps; i++)
		{
			vector p = from + (dir * (i * LANE_SAMPLE_M));
			p[1] = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
			if (IA_SpawnPlacement.IsInOcean(p))
				return false;
			if (!IA_SpawnPlacement.HasStandRoom(p))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PreflightResources(notnull IA_DynamicSiteLayout layout)
	{
		int n = layout.m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			IA_DynamicSiteModule mod = layout.m_aModules[i];
			if (!mod || !mod.m_bRequired)
				continue;
			Resource res = Resource.Load(mod.m_Prefab);
			if (!res)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnModule(notnull IA_DynamicSiteInstance site, notnull IA_DynamicSiteModule mod)
	{
		Resource res = Resource.Load(mod.m_Prefab);
		if (!res)
			return false;

		vector rootMat[4];
		site.GetLayout().BuildRootTransform(site.GetOrigin(), site.GetYawDeg(), rootMat);
		vector worldMat[4];
		site.GetLayout().LocalToWorld(rootMat, mod.m_vLocalPosition, mod.m_fLocalYawDeg, mod.m_fPrefabYawCorrectionDeg, worldMat);
		vector spawnPos = worldMat[3];
		spawnPos[1] = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
		worldMat[3] = spawnPos;
		if (mod.m_iGroundingPolicy == IA_DynamicSiteGrounding.UprightPad)
			SCR_TerrainHelper.SnapToTerrain(worldMat, GetGame().GetWorld());

		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(worldMat, params.Transform);
		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
			return false;
		site.AddRoot(ent);
		return true;
	}
}
