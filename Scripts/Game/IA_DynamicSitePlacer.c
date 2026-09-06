//------------------------------------------------------------------------------------------------
//! Candidate shortlist, footprint validation, staged spawn, and rollback.
//! Never announces tasks or advances the AO.
//------------------------------------------------------------------------------------------------
class IA_DynamicSitePlacer
{
	static const float AO_MARGIN_M = 600;
	static const float PLAYER_CLEAR_M = 350;
	static const float GRID_M = 10;
	static const int MAX_SAMPLE_CENTERS = 2048;
	static const int MAX_REFINEMENT_CENTERS = 32;
	static const float REFINEMENT_STEP_M = 7.5;
	static const int REFINEMENT_OFFSETS = 9;
	static const int FINE_HEADINGS = 24;
	static const int MAX_SHORTLIST = 64;
	static const int SURVEY_POSES_PER_TICK = 64;
	static const int SURVEY_SLICE_MS = 4;
	static const int PLACE_DEADLINE_MS = 60000;
	static const int SURVEY_DEADLINE_MS = 180000;
	static const int MAX_ROOTS = 256;
	static const int MAX_EXPANDED = 2200;
	static const int NAV_RECHECK_MS = 15000;
	static const float LANE_SAMPLE_M = 3;
	// Whole-base bowl. 4 m rejected RallyPosts on ordinary Everon rolls.
	static const float FOOTPRINT_HEIGHT_SPAN_M = 6;
	// cos(10 deg). The old 5 deg (0.9961947) vetoed almost every Montignac
	// field once HQ and LivingLarge both followed the terrain plane.
	static const float COMPOSITION_MIN_UP_Y = 0.9848078;

	protected int m_iSerial;
	protected int m_iGroupId;
	protected ref IA_BaseObjectiveSettings m_Settings;
	protected ref RandomGenerator m_Rng;
	protected int m_iSeed;
	protected int m_iDesignVariant = -1;
	protected ref array<ref IA_DynamicSiteCandidate> m_aShortlist;
	protected ref IA_DynamicSiteResult m_Result;
	protected ref IA_DynamicSiteInstance m_Building;
	protected IA_DynamicObjectiveDirector m_Director;
	protected ref IA_DynamicSiteLayout m_ActiveLayout;
	protected int m_iWorkStep;
	protected int m_iCandidateIndex;
	protected ref array<ref IA_DynamicSiteLayout> m_aLayouts;
	protected ref map<string, int> m_Rejections;
	protected string m_sTerrainRejection;
	protected string m_sTerrainDetail;
	protected int m_iHeadingIndex;
	protected bool m_bCandidateSearchPending;
	protected int m_iModuleIndex;
	protected ref array<int> m_aPlacedPerimeter;
	protected int m_iGarrisonLeft;
	protected int m_iGuardSlot;
	protected int m_iPlaceStartMs;
	protected int m_iConstructionStartMs;
	protected int m_iNavWaitStartMs;
	protected bool m_bComplete;
	protected bool m_bWorkQueued;
	protected bool m_bPrecomputeOnly;
	protected ref array<vector> m_aSearchCenters;
	protected ref array<float> m_aSearchRadii;
	protected int m_iSearchSizeMode;
	protected int m_iSampleCount;
	protected int m_iSurveyLayout;
	protected int m_iSurveyHeading;
	protected vector m_vSurveyAnchor;
	protected bool m_bSurveyAnchorActive;
	protected bool m_bSurveyDone;
	protected ref array<vector> m_aRefinementCenters = {};
	protected ref array<int> m_aRefinementScores = {};
	protected int m_iRefinementSample;
	protected bool m_bSurveyRefining;
	protected int m_iTerrainModulesPassed;
	protected ref IA_EmplacementBuilder m_EmplacementBuilder;
	protected bool m_bEmplacementPhaseDone;
	protected int m_iCrewLeft;
	protected int m_iCrewIndex;

	//------------------------------------------------------------------------------------------------
	void IA_DynamicSitePlacer()
	{
		m_aShortlist = new array<ref IA_DynamicSiteCandidate>();
		m_Rng = new RandomGenerator();
	}

#ifdef WORKBENCH
	protected BaseWorld m_AuditWorld;
	void SetAuditWorld(BaseWorld world)
	{
		m_AuditWorld = world;
	}
#endif

	protected BaseWorld GetPlacementWorld()
	{
#ifdef WORKBENCH
		if (m_AuditWorld)
			return m_AuditWorld;
#endif
		return GetGame().GetWorld();
	}

	protected bool IsOcean(vector point)
	{
		return point[1] <= GetPlacementWorld().GetOceanHeight(point[0], point[2]);
	}

	void SetDirector(IA_DynamicObjectiveDirector director)
	{
		m_Director = director;
	}

	//------------------------------------------------------------------------------------------------
	void BeginPrecompute(int serial, int groupId)
	{
		m_iSerial = serial;
		m_iGroupId = groupId;
		m_iSeed = serial * 7919 + groupId * 104729 + System.GetUnixTime();
		m_iDesignVariant = IA_BaseDesignLibrary.Select(m_iSeed);
		m_Rng.SetSeed(m_iSeed);
		m_aShortlist.Clear();
		m_bComplete = false;
		m_bPrecomputeOnly = true;
		m_Result = null;
		m_Building = null;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		int sizeMode = IA_DynamicSiteSizeMode.Auto;
		if (cfg)
			sizeMode = cfg.m_iDynamicBaseSizeMode;
		InitializeSurvey(sizeMode);
		QueueWork();
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
		m_iHeadingIndex = 0;
		// Admin settings can change while the ordinary objectives are running.
		if (settings.m_iSizeMode != m_iSearchSizeMode)
			InitializeSurvey(settings.m_iSizeMode);
		m_iModuleIndex = 0;
		m_iGarrisonLeft = 0;
		m_iGuardSlot = 0;
		m_iPlaceStartMs = System.GetTickCount();
		m_iConstructionStartMs = 0;
		m_iNavWaitStartMs = 0;
		RefreshShortlistScores();
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
		if (m_Building && m_Director)
			m_Director.RetireSite(m_Building);
		m_Building = null;
		// Success can be queued for delivery when an admin replaces the AO.
		if (m_Result && m_Result.m_Site && m_Director)
			m_Director.RetireSite(m_Result.m_Site);
		m_Result = null;
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
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init || init.GetAoActivationSerial() != m_iSerial)
		{
			Cancel(m_iSerial);
			return;
		}
		if (m_bPrecomputeOnly)
		{
			StepSurvey();
			if (!m_bSurveyDone && m_aShortlist.Count() < MAX_SHORTLIST && (HasSurveyWork() || m_aShortlist.IsEmpty()))
				QueueWork();
			return;
		}

		int now = System.GetTickCount();
		bool timedOut = now - m_iPlaceStartMs > SURVEY_DEADLINE_MS;
		if (m_iConstructionStartMs != 0)
			timedOut = now - m_iConstructionStartMs > PLACE_DEADLINE_MS;
		if (timedOut)
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
		else if (m_iWorkStep == 6)
			StepEmplacements();

		if (!m_bComplete)
			QueueWork();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueWork()
	{
		if (m_bWorkQueued || m_bComplete)
			return;
		m_bWorkQueued = true;
		int delay = 200;
		if (m_bPrecomputeOnly || m_iWorkStep == 1 || m_iWorkStep == 2 || m_iWorkStep == 6)
			delay = 16; // Survey and construction yield under their work budgets.
		GetGame().GetCallqueue().CallLater(this.ProcessWork, delay, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StepPickCandidate()
	{
		if (m_Director && m_Director.CountRetiredSites() >= 2)
		{
			FinishFailure("retired_site_limit");
			return;
		}
		if (m_iCandidateIndex >= m_aShortlist.Count())
		{
			// Keep looking beyond this batch before allowing a smaller layout.
			m_aShortlist.Clear();
			m_iCandidateIndex = 0;
			if (m_bSurveyDone)
				FinishFailure("no_legal_site");
			else
			{
				StepSurvey();
				RefreshShortlistScores();
			}
			return;
		}

		IA_DynamicSiteCandidate cand = m_aShortlist[m_iCandidateIndex];
		if (!ValidateCandidate(cand))
		{
			if (!m_bCandidateSearchPending)
			{
				m_iCandidateIndex++;
				m_iHeadingIndex = 0;
			}
			return;
		}

		m_ActiveLayout = ResolveLayout(cand.m_iLayoutId);
		if (!PreflightResources(m_ActiveLayout))
			return;

		m_Building = IA_DynamicSiteInstance.Create(m_iSerial, m_iGroupId, cand.m_vCenter, cand.m_fYawDeg, m_ActiveLayout, ResolveEnemyFaction());
		if (m_iConstructionStartMs == 0)
			m_iConstructionStartMs = System.GetTickCount();
		m_iModuleIndex = 0;
		m_aPlacedPerimeter = {0, 0, 0, 0};
		m_EmplacementBuilder = null;
		m_bEmplacementPhaseDone = !m_Settings || !m_Settings.m_bEmplacementsEnabled;
		m_iCrewLeft = 0;
		m_iCrewIndex = 0;
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
		int started = System.GetTickCount();
		int total = m_ActiveLayout.m_aModules.Count();
		while (m_iModuleIndex < total && spawned < 2)
		{
			if (System.GetTickCount() - started >= SURVEY_SLICE_MS)
				return;
			IA_DynamicSiteModule mod = m_ActiveLayout.m_aModules[m_iModuleIndex];
			if (mod && mod.m_iRole == IA_DynamicSiteModuleRole.Dressing && !m_bEmplacementPhaseDone)
			{
				BeginEmplacementPhase();
				return;
			}
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
			if (mod.m_iPerimeterSide >= 0)
				m_aPlacedPerimeter[mod.m_iPerimeterSide] = m_aPlacedPerimeter[mod.m_iPerimeterSide] + 1;
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
			if (!m_ActiveLayout.HasPerimeterCoverage(m_aPlacedPerimeter))
			{
				FailCurrent("perimeter_coverage_changed");
				return;
			}
			if (!m_bEmplacementPhaseDone)
			{
				BeginEmplacementPhase();
				return;
			}
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
				m_iGarrisonLeft = m_Settings.ComputeInitialGarrison(m_ActiveLayout.m_iMaxGarrison);
			else
				m_iGarrisonLeft = Math.Round(Math.Min(36, m_ActiveLayout.m_iMaxGarrison) * 1.75);
			// Optional access must be navigable after the normal nav rebuild.
			if (!m_ActiveLayout.m_bComposed && !ValidateEmplacementAccess())
			{
				m_Building.RemoveLastEmplacement();
				m_Building.RequestNavRebuild();
				m_iWorkStep = 3;
				return;
			}
			m_Building.SetGarrisonBudget(m_iGarrisonLeft);
			m_iCrewLeft = IA_EmplacementProfile.CrewBudget(m_Building.GetEmplacements().Count(), m_iGarrisonLeft);
			m_iCrewIndex = 0;
			m_iGuardSlot = 0;
			if (IA_Log.IsDebugEnabled())
			{
				Print(string.Format("[IA][Base] Initial garrison budget=%1 layout=%2 max=%3", m_iGarrisonLeft, m_ActiveLayout.m_sName, m_ActiveLayout.m_iMaxGarrison), LogLevel.NORMAL);
			}
			return;
		}

		if (System.GetTickCount() - m_iNavWaitStartMs > 1000 && m_Building.RemoveLastEmplacement())
		{
			m_Building.RequestNavRebuild();
			return; // rollback weapons on this exact site before the ordinary fail path
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

		m_Building.TickEmplacements(true);
		if (SettleEmplacementCrewSpawns())
			return;
		if (m_iCrewLeft > 0)
		{
			SpawnEmplacementCrew();
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
		if (!IA_SpawnPlacement.IsOutdoorStandPose(post))
		{
			// Crew spawn locations were independently validated; do not move an
			// original guard post to make room for a new emplacement.
			FailCurrent("guard_post_blocked");
			return;
		}
		vector localDefendCenter;
		float defendRadius;
		if (!IA_BaseGarrisonArea.Resolve(m_ActiveLayout, m_Building.WorldToLocalFlat(post), localDefendCenter, defendRadius))
		{
			FailCurrent("guard_post_no_defend_room");
			return;
		}
		vector rootMat[4];
		m_ActiveLayout.BuildRootTransform(m_Building.GetOrigin(), m_Building.GetYawDeg(), rootMat);
		vector defendCenter = m_ActiveLayout.LocalOffsetToWorld(rootMat, localDefendCenter);
		defendCenter[1] = IA_BasePlayerSampler.SampleSupportY(defendCenter);
		// Each group gets its own waypoint over the same whole-base area.
		// Preserve dispersed spawn posts; do not spawn everybody at the centre.
		IA_AiGroup group = IA_AiGroup.CreateMilitaryGroupFromUnits(post, IA_Faction.USSR, size, ResolveEnemyFaction(), false, true, true, false);
		if (!group)
		{
			FailCurrent("garrison_failed");
			return;
		}

		IA_AreaInstance host = m_Building.GetHost();
		if (host && host.GetArea())
			group.SetAssignedArea(host.GetArea());
		group.SetDefendPost(defendCenter, defendRadius);
		group.Spawn(IA_AiOrder.Defend, defendCenter);
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Base] Garrison spawn=%1 defendCenter=%2 radius=%3 priority=%4", post, defendCenter, defendRadius, IA_AiGroup.WP_PRIORITY_DEFEND_POST), LogLevel.NORMAL);
		}
		m_Building.AddGarrisonGroup(group);
		group.SpawnNextUnit();
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
		m_Building.TickEmplacements(true);
		if (m_Building.EmplacementMountsPending())
			return;
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

		m_Building.RevealEmplacements();
		FinishSuccess();
	}

	protected void BeginEmplacementPhase()
	{
		if (m_ActiveLayout.m_bComposed)
			m_EmplacementBuilder = new IA_CompositionGunBuilder();
		else
			m_EmplacementBuilder = new IA_EmplacementBuilder();
		m_EmplacementBuilder.Begin(m_Building);
		m_iWorkStep = 6;
	}

	protected void StepEmplacements()
	{
		if (!m_Building || !m_EmplacementBuilder)
		{
			m_bEmplacementPhaseDone = true;
			m_iWorkStep = 2;
			return;
		}
		if (!ValidatePlayerClearance(m_Building.GetOrigin(), m_Building.GetYawDeg(), m_ActiveLayout, m_Building))
		{
			FailCurrent("player_approached");
			return;
		}
		// Keep the optional phase away from the site's global deadline.
		if (System.GetTickCount() - m_iConstructionStartMs > PLACE_DEADLINE_MS - 25000)
			m_EmplacementBuilder.Finish();
		else
			m_EmplacementBuilder.Step();
		if (m_EmplacementBuilder.IsDone())
		{
			m_bEmplacementPhaseDone = true;
			m_EmplacementBuilder = null;
			m_iWorkStep = 2;
		}
	}

	protected bool ValidateEmplacementAccess()
	{
		if (m_Building.GetEmplacements().IsEmpty())
			return true;
		foreach (IA_StaticGunRecord record : m_Building.GetEmplacements())
		{
			if (record.m_bAuthoredAccess)
			{
				if (!HasLocalNavmesh(record.m_vAccess) || !IA_CompositionGunBuilder.StandingClear(record.m_vAccess))
					return false;
				continue; // an elevated seat does not need a ground navmesh polygon
			}
			vector mat[4];
			Math3D.AnglesToMatrix(Vector(record.m_fYaw, 0, 0), mat);
			vector inside = record.m_vOrigin - mat[2] * 3.5;
			vector operator = record.m_vOrigin + mat[0] * record.m_Profile.m_vSeat[0] + mat[2] * record.m_Profile.m_vSeat[2];
			if (!HasLocalNavmesh(inside) || !HasLocalNavmesh(operator))
				return false;
		}
		int guardGroups = 2 + Math.Ceil(Math.Max(0, m_iGarrisonLeft - 4) / 4.0);
		for (int i = 0; i < guardGroups; i++)
		{
			vector guard = ResolveGuardPostIndex(i);
			foreach (IA_StaticGunRecord record : m_Building.GetEmplacements())
			{
				if (record.ContainsReservedPoint(guard, 1.3))
					return false;
			}
			if (!IA_SpawnPlacement.IsOutdoorStandPose(guard))
				return false;
		}
		return true;
	}

	protected bool SettleEmplacementCrewSpawns()
	{
		foreach (IA_StaticGunRecord record : m_Building.GetEmplacements())
		{
			if (!record.m_Crew || record.m_bCrewSpawnSettled)
				continue;
			if (record.m_Crew.GetSpawnedUnitCount() > 0)
			{
				record.m_bCrewSpawnSettled = true;
				continue;
			}
			if (System.GetTickCount() - record.m_iCrewSpawnMs < 1000)
				return true;
			// Refund only an initial spawn that never produced an assigned unit.
			// A soldier who spawned and died is NEVER replaced by this path.
			m_Building.RemoveFailedEmplacementCrew(record.m_Crew);
			record.m_Crew = null;
			record.m_bCrewSpawnSettled = true;
			m_iGarrisonLeft++;
		}
		return false;
	}

	protected void SpawnEmplacementCrew()
	{
		array<ref IA_StaticGunRecord> guns = m_Building.GetEmplacements();
		if (m_iCrewIndex >= guns.Count())
		{
			m_iCrewLeft = 0;
			return;
		}
		IA_StaticGunRecord record = guns[m_iCrewIndex];
		m_iCrewIndex++;
		m_iCrewLeft--;
		if (!record.m_Gun || !record.m_Gun.IsUsable())
			return; // no budget consumed: ordinary infantry receives the slot
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(record.m_fYaw, 0, 0), mat);
		vector post = record.m_vOrigin - mat[2] * 3;
		if (record.m_bAuthoredAccess)
		{
			post = record.m_vAccess;
			if (!IA_CompositionGunBuilder.StandingClear(post))
				return;
		}
		else
		{
			post[1] = IA_BasePlayerSampler.SampleSupportY(post);
			if (!IA_SpawnPlacement.IsOutdoorStandPose(post))
				return;
		}
		vector localCenter;
		float radius;
		if (!IA_BaseGarrisonArea.Resolve(m_ActiveLayout, m_Building.WorldToLocalFlat(post), localCenter, radius))
			return;
		vector root[4];
		m_ActiveLayout.BuildRootTransform(m_Building.GetOrigin(), m_Building.GetYawDeg(), root);
		vector center = m_ActiveLayout.LocalOffsetToWorld(root, localCenter);
		center[1] = IA_BasePlayerSampler.SampleSupportY(center);
		IA_AiGroup group = IA_AiGroup.CreateMilitaryGroupFromUnits(post, IA_Faction.USSR, 1, ResolveEnemyFaction(), false, true, true, false);
		if (!group)
			return;
		group.SetAssignedArea(m_Building.GetHost().GetArea());
		group.AssignStaticGun(record.m_Gun, m_iSerial, center, radius);
		group.Spawn(IA_AiOrder.Defend, center); // intent suppresses Defend while assigned
		m_Building.AddGarrisonGroup(group);
		record.m_Crew = group;
		record.m_iCrewSpawnMs = System.GetTickCount();
		group.SpawnNextUnit();
		m_iGarrisonLeft--;
	}

	//------------------------------------------------------------------------------------------------
	protected vector ResolveNextGuardPost()
	{
		return ResolveGuardPostIndex(m_iGuardSlot);
	}

	protected vector ResolveGuardPostIndex(int slotIndex)
	{
		int posts = m_Building.GetGuardPostCount();
		if (slotIndex < posts)
			return m_Building.GetGuardPostWorld(slotIndex);

		int extra = slotIndex - posts;
		int stations = m_Building.GetPerimeterStationCount();
		if (stations > 0)
		{
			vector station = m_Building.GetPerimeterStationWorld(extra % stations);
			vector inward = m_Building.GetOrigin() - station;
			inward[1] = 0;
			inward.Normalize();
			station = station + inward * 6;
			station[1] = IA_BasePlayerSampler.SampleSupportY(station);
			return station;
		}
		return m_Building.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected void FailCurrent(string reason)
	{
		RecordRejection(reason);
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Base] Placement candidate failed: %1", reason), LogLevel.NORMAL);
		}
		if (m_Building && m_Director)
			m_Director.RetireSite(m_Building);
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
		IA_Log.Info(string.Format("[IA][Base] Placement committed serial=%1 site=%2 layout=%3 yaw=%4", m_iSerial, result.m_Site.GetSiteId(), m_ActiveLayout.m_sName, result.m_Site.GetYawDeg()));
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
		if (m_Rejections)
		{
			foreach (string stage, int count : m_Rejections)
				Print(string.Format("[IA][Base] Rejections: %1=%2", stage, count), LogLevel.WARNING);
		}
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
	protected void InitializeSurvey(int sizeMode)
	{
		m_aShortlist.Clear();
		m_Rng.SetSeed(m_iSeed);
		m_iSearchSizeMode = sizeMode;
		m_iSampleCount = 0;
		m_iSurveyLayout = 0;
		m_iSurveyHeading = 0;
		m_bSurveyAnchorActive = false;
		m_bSurveyDone = false;
		ResetRefinement();
		m_Rejections = new map<string, int>();
		m_aSearchCenters = new array<vector>();
		m_aSearchRadii = new array<float>();
		CollectPrimaryCircles(m_aSearchCenters, m_aSearchRadii);
		m_aLayouts = new array<ref IA_DynamicSiteLayout>();
		array<int> layoutIds = {};
		IA_DynamicSiteLayout.GetAllowedLayoutIds(sizeMode, layoutIds);
		foreach (int layoutId : layoutIds)
		{
			if (m_iDesignVariant < 0)
				m_iDesignVariant = IA_BaseDesignLibrary.Select(m_iSeed);
			ref IA_DynamicSiteLayout layout = IA_BaseDesignRecipes.Create(layoutId, m_iDesignVariant);
			if (PreflightResources(layout))
				m_aLayouts.Insert(layout);
			else
				RecordRejection("missing_resource");
		}
	}

	// Progressive disk coverage avoids random clusters. Circle-local sequence
	// indices keep coverage stable when multiple objective circles are interleaved.
	protected float RadicalInverse(int index, int radix)
	{
		float value = 0;
		float fraction = 1;
		while (index > 0)
		{
			fraction = fraction / radix;
			int digit = index % radix;
			value += digit * fraction;
			index = index / radix;
		}
		return value;
	}

	protected vector CoarseAnchor(int sample)
	{
		int count = m_aSearchCenters.Count();
		int pick = sample % count;
		int sequence = sample / count + 1;
		vector center = m_aSearchCenters[pick];
		float radius = Math.Max(0, m_aSearchRadii[pick] + AO_MARGIN_M);
		int rotation = m_iSeed % 360;
		float angle = (RadicalInverse(sequence, 3) + rotation / 360.0) * 6.283185;
		float distance = Math.Sqrt(RadicalInverse(sequence, 2)) * radius;
		return Vector(center[0] + Math.Cos(angle) * distance, 0, center[2] + Math.Sin(angle) * distance);
	}

	protected vector RefinementOffset(int index)
	{
		if (index == 0)
			return vector.Zero;
		int cell = index - 1;
		if (cell >= 4)
			cell++;
		int row = cell / 3;
		int column = cell % 3;
		return Vector((column - 1) * REFINEMENT_STEP_M, 0, (row - 1) * REFINEMENT_STEP_M);
	}

	protected void ResetRefinement()
	{
		m_aRefinementCenters.Clear();
		m_aRefinementScores.Clear();
		m_iRefinementSample = 0;
		m_bSurveyRefining = false;
	}

	protected bool HasSurveyWork()
	{
		return m_bSurveyAnchorActive || m_iSampleCount < MAX_SAMPLE_CENTERS || m_iRefinementSample < m_aRefinementCenters.Count() * REFINEMENT_OFFSETS;
	}

	protected void RememberNearFit(vector anchor)
	{
		if (m_bSurveyRefining)
			return;
		int worst = -1;
		for (int i = 0; i < m_aRefinementCenters.Count(); i++)
		{
			if (vector.DistanceXZ(anchor, m_aRefinementCenters[i]) < REFINEMENT_STEP_M * 2)
			{
				if (m_iTerrainModulesPassed > m_aRefinementScores[i])
				{
					m_aRefinementCenters[i] = anchor;
					m_aRefinementScores[i] = m_iTerrainModulesPassed;
				}
				return;
			}
			if (worst < 0 || m_aRefinementScores[i] < m_aRefinementScores[worst])
				worst = i;
		}
		if (m_aRefinementCenters.Count() < MAX_REFINEMENT_CENTERS)
		{
			m_aRefinementCenters.Insert(anchor);
			m_aRefinementScores.Insert(m_iTerrainModulesPassed);
		}
		else if (worst >= 0 && m_iTerrainModulesPassed > m_aRefinementScores[worst])
		{
			m_aRefinementCenters[worst] = anchor;
			m_aRefinementScores[worst] = m_iTerrainModulesPassed;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Survey the entire anchor budget for each size before allowing a downgrade.
	//! Batches contain one layout only; player distance ranks sites within that size.
	//! Only complete terrain/obstruction fits enter the shortlist. Survey during
	//! the ordinary AO; an immediate admin start resumes the same bounded work.
	protected void StepSurvey()
	{
		if (m_bSurveyDone || m_aShortlist.Count() >= MAX_SHORTLIST)
			return;
		if (m_aSearchCenters.IsEmpty() || m_aLayouts.IsEmpty())
		{
			m_bSurveyDone = true;
			return;
		}
		int started = System.GetTickCount();
		int poses = 0;
		while (poses < SURVEY_POSES_PER_TICK && m_aShortlist.Count() < MAX_SHORTLIST)
		{
			if (poses > 0 && System.GetTickCount() - started >= SURVEY_SLICE_MS)
				return;
			poses++;
			if (!m_bSurveyAnchorActive)
			{
				if (!HasSurveyWork())
				{
					// Consume all qualified sites of this size before surveying smaller ones.
					if (!m_aShortlist.IsEmpty())
						return;
					if (IA_Log.IsDebugEnabled())
					{
						Print(string.Format("[IA][Base] Layout search exhausted: %1 anchors=%2 refinement_positions=%3", m_aLayouts[m_iSurveyLayout].m_sName, m_iSampleCount, m_iRefinementSample), LogLevel.NORMAL);
					}
					m_iSurveyLayout++;
					if (m_iSurveyLayout >= m_aLayouts.Count())
					{
						m_bSurveyDone = true;
						return;
					}
					m_iSampleCount = 0;
					ResetRefinement();
					// Every size receives the same spatial samples, independent of earlier fits.
					m_Rng.SetSeed(m_iSeed);
				}
				m_bSurveyRefining = m_iSampleCount >= MAX_SAMPLE_CENTERS;
				if (m_bSurveyRefining)
				{
					int refineIndex = m_iRefinementSample / REFINEMENT_OFFSETS;
					m_vSurveyAnchor = m_aRefinementCenters[refineIndex] + RefinementOffset(m_iRefinementSample % REFINEMENT_OFFSETS);
					m_iRefinementSample++;
				}
				else
				{
					m_vSurveyAnchor = CoarseAnchor(m_iSampleCount);
					m_iSampleCount++;
				}
				m_vSurveyAnchor[1] = GetPlacementWorld().GetSurfaceY(m_vSurveyAnchor[0], m_vSurveyAnchor[2]);
				if (IsOcean(m_vSurveyAnchor))
				{
					RecordRejection("anchor_water");
					continue;
				}
				bool duplicate = false;
				foreach (IA_DynamicSiteCandidate existing : m_aShortlist)
				{
					if (vector.DistanceXZ(existing.m_vHqAnchor, m_vSurveyAnchor) < 1)
						duplicate = true;
				}
				if (duplicate)
					continue;
				m_iSurveyHeading = 0;
				m_bSurveyAnchorActive = true;
			}
			IA_DynamicSiteLayout layout = m_aLayouts[m_iSurveyLayout];
			int headings = 8;
			if (m_bSurveyRefining)
				headings = FINE_HEADINGS;
			float yaw = m_iSurveyHeading * (360.0 / headings);
			m_iSurveyHeading++;
			if (m_iSurveyHeading >= headings)
			{
				m_iSurveyHeading = 0;
				m_bSurveyAnchorActive = false;
			}
			vector origin = OriginFromHq(m_vSurveyAnchor, yaw, layout);
			vector rootMat[4];
			layout.BuildRootTransform(origin, yaw, rootMat);
			// The first module is the HQ in every manifest. This cheap check
			// avoids scanning a whole base around an unusable foundation.
			if (!ValidateModulePad(rootMat, layout.m_aModules[0]))
			{
				RecordTerrainRejection(origin, yaw, layout);
				continue;
			}
			if (!FootprintInsideUnion(origin, yaw, layout))
			{
				RecordRejection("ao_boundary");
				continue;
			}
			if (!ValidateTerrain(origin, yaw, layout, true))
			{
				RememberNearFit(m_vSurveyAnchor);
				RecordTerrainRejection(origin, yaw, layout);
				continue;
			}
			ref IA_DynamicSiteCandidate cand = new IA_DynamicSiteCandidate();
			cand.m_vCenter = origin;
			cand.m_vHqAnchor = m_vSurveyAnchor;
			cand.m_fYawDeg = yaw;
			cand.m_fSurveyYawDeg = yaw;
			cand.m_iLayoutId = layout.m_iLayoutId;
			cand.m_iSeed = m_iSeed + m_iSampleCount;
			m_aShortlist.Insert(cand);
			m_bSurveyAnchorActive = false;
			if (IA_Log.IsDebugEnabled())
			{
				Print(string.Format("[IA][Base] Terrain-qualified site: layout=%1 center=%2 yaw=%3 anchors_sampled=%4", layout.m_sName, origin, yaw, m_iSampleCount), LogLevel.NORMAL);
			}
		}
	}

	protected vector OriginFromHq(vector anchor, float yaw, notnull IA_DynamicSiteLayout layout)
	{
		vector rootMat[4];
		layout.BuildRootTransform(vector.Zero, yaw, rootMat);
		vector offset = layout.LocalOffsetToWorld(rootMat, layout.m_aModules[0].m_vLocalPosition);
		vector origin = anchor - offset;
		origin[1] = GetPlacementWorld().GetSurfaceY(origin[0], origin[2]);
		return origin;
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
		// Every retained site already fits a whole design; a tiny center-height
		// sample must not displace that evidence with a misleading terrain rank.
		return median + (0.35 * dists[p90i]);
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
	protected IA_DynamicSiteLayout ResolveLayout(int layoutId)
	{
		foreach (IA_DynamicSiteLayout layout : m_aLayouts)
		{
			if (layout.m_iLayoutId == layoutId)
				return layout;
		}
		return IA_BaseDesignRecipes.Create(layoutId, m_iDesignVariant);
	}

	protected bool ValidateCandidate(notnull IA_DynamicSiteCandidate cand)
	{
		m_bCandidateSearchPending = false;
		int tested = 0;
		int started = System.GetTickCount();
		IA_DynamicSiteLayout layout = ResolveLayout(cand.m_iLayoutId);
		while (m_iHeadingIndex < FINE_HEADINGS)
		{
			if (tested > 0 && System.GetTickCount() - started >= SURVEY_SLICE_MS)
				break;
			// Revalidate the exact surveyed pose first, including refined headings.
			// Keep its reference yaw stable across failed construction attempts.
			float yaw = cand.m_fSurveyYawDeg + m_iHeadingIndex * (360.0 / FINE_HEADINGS);
			if (yaw >= 360)
				yaw -= 360;
			m_iHeadingIndex++;
			tested++;
			vector origin = OriginFromHq(cand.m_vHqAnchor, yaw, layout);
			if (!FootprintInsideUnion(origin, yaw, layout))
			{
				RecordRejection("ao_boundary");
				continue;
			}
			if (!ValidateTerrain(origin, yaw, layout))
			{
				RecordTerrainRejection(origin, yaw, layout);
				continue;
			}
			if (!ValidatePlayerClearance(origin, yaw, layout, null))
			{
				RecordRejection("players_or_visibility");
				continue;
			}
			if (!ValidateInteriorRoutes(origin, yaw, layout, false))
			{
				RecordRejection("access_routes");
				continue;
			}

			cand.m_vCenter = origin;
			cand.m_fYawDeg = yaw;
			cand.m_iLayoutId = layout.m_iLayoutId;
			// A failed build resumes remaining headings, then other sites of this size.
			return true;
		}
		m_bCandidateSearchPending = m_iHeadingIndex < FINE_HEADINGS;
		return false;
	}

	protected void RecordRejection(string reason)
	{
		if (m_Rejections)
			m_Rejections.Set(reason, m_Rejections.Get(reason) + 1);
	}

	protected void RecordTerrainRejection(vector origin, float yaw, notnull IA_DynamicSiteLayout layout)
	{
		string reason = m_sTerrainRejection + "/" + layout.m_sName;
		// Distinguish the compulsory HQ screening from later module failures.
		if (m_sTerrainDetail.StartsWith("module="))
		{
			int end = m_sTerrainDetail.IndexOf(" ");
			if (end > 7)
				reason += "/" + m_sTerrainDetail.Substring(7, end - 7);
		}
		if (IA_Log.IsDebugEnabled())
		{
			if (!m_Rejections.Contains(reason))
				Print(string.Format("[IA][Base] Rejection example: %1 center=%2 yaw=%3 %4", reason, origin, yaw, m_sTerrainDetail), LogLevel.NORMAL);
		}
		RecordRejection(reason);
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
		// A disk is convex: if all rectangle corners are in one disk, every
		// interior sample is too. Overlapping-circle cases still use the full grid.
		for (int circle = 0; circle < centers.Count(); circle++)
		{
			bool contained = true;
			foreach (vector corner : corners)
			{
				if (!IA_AreaMarker.IsWorldPosInsideCaptureRadius(corner, centers[circle], radii[circle] + AO_MARGIN_M))
				{
					contained = false;
					break;
				}
			}
			if (contained)
				return true;
		}
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
	protected bool ValidateTerrain(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, bool hqAlreadyValidated = false)
	{
		m_iTerrainModulesPassed = 0;
		m_sTerrainRejection = "world_unavailable";
		m_sTerrainDetail = "";
		BaseWorld world = GetPlacementWorld();
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
				if (IsOcean(p))
				{
					m_sTerrainRejection = "footprint_water";
					m_sTerrainDetail = string.Format("sample=%1 ocean_y=%2", p, world.GetOceanHeight(p[0], p[2]));
					return false;
				}
				if (p[1] < minY)
					minY = p[1];
				if (p[1] > maxY)
					maxY = p[1];
				// Fail as soon as the sampled range exceeds the limit. Surveying
				// more anchors must not require full scans of obviously steep sites.
				if ((maxY - minY) > FOOTPRINT_HEIGHT_SPAN_M)
				{
					m_sTerrainRejection = "footprint_height_span";
					m_sTerrainDetail = string.Format("height_delta=%1 limit=%2", maxY - minY, FOOTPRINT_HEIGHT_SPAN_M);
					return false;
				}
				z = z + GRID_M;
			}
			x = x + GRID_M;
		}
		ref array<int> perimeter = {0, 0, 0, 0};
		int n = layout.m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			IA_DynamicSiteModule mod = layout.m_aModules[i];
			if (!mod)
				continue;
			// Decorative vignettes never veto a site or consume terrain survey
			// traces. SpawnModule still checks their support and obstructions.
			if (mod.m_iRole == IA_DynamicSiteModuleRole.Dressing)
				continue;
			// Survey screened this exact HQ pose immediately before this call.
			// Live candidate validation uses the default and checks every module.
			bool padValid = i == 0 && hqAlreadyValidated;
			if (!padValid)
				padValid = ValidateModulePad(rootMat, mod);
			if (!padValid)
			{
				if (mod.m_bRequired)
					return false;
				continue;
			}
			if (mod.m_iPerimeterSide >= 0)
				perimeter[mod.m_iPerimeterSide] = perimeter[mod.m_iPerimeterSide] + 1;
			if (mod.m_bRequired)
				m_iTerrainModulesPassed++;
		}
		if (!layout.HasPerimeterCoverage(perimeter))
		{
			m_sTerrainRejection = "perimeter_coverage";
			m_sTerrainDetail = string.Format("sides=%1,%2,%3,%4", perimeter[0], perimeter[1], perimeter[2], perimeter[3]);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateModulePad(vector rootMat[4], notnull IA_DynamicSiteModule mod)
	{
		vector worldMat[4];
		vector localMat[4];
		Math3D.AnglesToMatrix(Vector(mod.m_fLocalYawDeg + mod.m_fPrefabYawCorrectionDeg, 0, 0), localMat);
		localMat[3] = mod.m_vLocalPosition;
		Math3D.MatrixMultiply4(rootMat, localMat, worldMat);
		AlignComposition(worldMat, mod);
		float originY;
		if (!SampleModuleSupport(worldMat, mod, originY))
			return false;
		return IsModuleVolumeClear(worldMat, mod);
	}

	protected void AlignComposition(inout vector worldMat[4], IA_DynamicSiteModule mod)
	{
		if (!mod.m_bFollowTerrainPlane)
			return;
		ref TraceParam terrain = new TraceParam();
		terrain.Flags = TraceFlags.WORLD;
		SCR_TerrainHelper.SnapAndOrientToTerrain(worldMat, GetPlacementWorld(), false, terrain);
	}

	protected bool SampleModuleSupport(vector worldMat[4], notnull IA_DynamicSiteModule mod, out float originY)
	{
		if (mod.m_bFollowTerrainPlane && worldMat[1][1] < COMPOSITION_MIN_UP_Y)
		{
			m_sTerrainRejection = "composition_slope";
			m_sTerrainDetail = "module=" + mod.m_sId;
			return false;
		}
		float centerY = GetPlacementWorld().GetSurfaceY(worldMat[3][0], worldMat[3][2]);
		originY = centerY;
		float minY = 99999;
		float maxY = -99999;
		int n = mod.m_aSupportPoints.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			vector local = mod.m_aSupportPoints[i];
			vector p = worldMat[3] + (worldMat[0] * local[0]) + (worldMat[2] * local[2]);
			p[1] = GetPlacementWorld().GetSurfaceY(p[0], p[2]);
			if (IsOcean(p))
			{
				m_sTerrainRejection = "pad_water";
				m_sTerrainDetail = string.Format("module=%1 sample=%2", mod.m_sId, p);
				return false;
			}
			if (mod.m_bFollowTerrainPlane)
			{
				vector expected = worldMat[0]*local[0] + worldMat[2]*local[2];
				p[1] = p[1] - expected[1];
			}
			if (p[1] < minY)
				minY = p[1];
			if (p[1] > maxY)
				maxY = p[1];
		}
		if (!mod.ResolveSupportHeight(centerY, minY, maxY, originY))
		{
			m_sTerrainRejection = "pad_height_span";
			if (maxY - minY <= mod.m_fMaxSupportDeltaM)
				m_sTerrainRejection = "foundation_lift";
			m_sTerrainDetail = string.Format("module=%1 height_delta=%2 limit=%3 bearing=%4..%5 lift=%6 lift_limit=%7", mod.m_sId, maxY - minY, mod.m_fMaxSupportDeltaM, mod.m_vSupportMins, mod.m_vSupportMaxs, originY - centerY, mod.m_fMaxFoundationLiftM);
			return false;
		}
		return true;
	}

	protected bool IsModuleVolumeClear(vector worldMat[4], notnull IA_DynamicSiteModule mod, IA_DynamicSiteInstance ignoreSite = null)
	{
		BaseWorld world = GetPlacementWorld();
		vector center = worldMat[3];
		center[1] = world.GetSurfaceY(center[0], center[2]);
		float minY = center[1];
		float maxY = center[1];
		for (int ix = -1; ix <= 1; ix++)
		{
			for (int iz = -1; iz <= 1; iz++)
			{
				vector p = center + worldMat[0] * (ix * mod.m_fHalfWidthM) + worldMat[2] * (iz * mod.m_fHalfDepthM);
				p[1] = world.GetSurfaceY(p[0], p[2]);
				if (IsOcean(p))
				{
					m_sTerrainRejection = "pad_water";
					m_sTerrainDetail = string.Format("module=%1 sample=%2", mod.m_sId, p);
					return false;
				}
				minY = Math.Min(minY, p[1]);
				maxY = Math.Max(maxY, p[1]);
			}
		}
		// Preserve the actual reserved rectangle at diagonal headings. Its
		// world-axis bounding box includes unrelated ground outside the pad.
		ref TraceOBB clearance = new TraceOBB();
		clearance.Mat[0] = worldMat[0];
		clearance.Mat[1] = worldMat[1];
		clearance.Mat[2] = worldMat[2];
		clearance.Start = Vector(center[0], minY + 0.1, center[2]);
		clearance.End = clearance.Start + Vector(0, 0.05, 0);
		clearance.Mins = Vector(-mod.m_fHalfWidthM, 0, -mod.m_fHalfDepthM);
		clearance.Maxs = Vector(mod.m_fHalfWidthM, maxY - minY + mod.m_fClearanceHeightM, mod.m_fHalfDepthM);
		// Terrain is checked separately. Starting above a height-span allowance
		// misses low obstructions; including WORLD here rejects the slope itself.
		clearance.Flags = TraceFlags.ENTS;
		// Authored wall ends meet; ignore only this base during construction.
		ref array<IEntity> exclusions = {};
		if (ignoreSite)
		{
			ignoreSite.CollectOwnedEntities(exclusions);
			clearance.ExcludeArray = exclusions;
		}
		if (world.TraceMove(clearance, null) < 1)
		{
			m_sTerrainRejection = "module_obstruction";
			string hit = "world_geometry";
			if (clearance.TraceEnt)
			{
				hit = clearance.TraceEnt.ClassName();
				EntityPrefabData prefabData = clearance.TraceEnt.GetPrefabData();
				if (prefabData)
					hit = prefabData.GetPrefabName();
			}
			m_sTerrainDetail = string.Format("module=%1 hit=%2 pad=%3x%4 center=%5", mod.m_sId, hit, mod.m_fHalfWidthM * 2, mod.m_fHalfDepthM * 2, center);
			return false;
		}
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
		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		vector delta = world - origin;
		float dx = Math.Max(0, Math.AbsFloat(vector.Dot(delta, rootMat[0])) - layout.m_fHalfWidthM);
		float dz = Math.Max(0, Math.AbsFloat(vector.Dot(delta, rootMat[2])) - layout.m_fHalfDepthM);
		return Math.Sqrt(dx * dx + dz * dz);
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

		BaseWorld world = GetPlacementWorld();
		int i;
		for (i = 0; i < samples.Count(); i++)
		{
			vector target = samples[i];
			target[1] = world.GetSurfaceY(target[0], target[2]) + 2;
			if (i == samples.Count() - 1)
				target[1] = target[1] + 6;
			ref TraceParam p = new TraceParam();
			p.Start = eye;
			p.End = target;
			p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			array<IEntity> exclusions = {observer};
			if (ignoreSite)
				ignoreSite.CollectOwnedEntities(exclusions);
			p.ExcludeArray = exclusions;
			float hit = world.TraceMove(p, null);
			if (hit >= 0.99)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateInteriorRoutes(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, bool includeProps)
	{
		vector rootMat[4];
		layout.BuildRootTransform(origin, yawDeg, rootMat);
		vector capture = layout.LocalOffsetToWorld(rootMat, layout.m_vCaptureLocal);
		// Composition interiors are allowed to occupy the old empty plaza.
		// Survey still proves the terrain can host the gates; walking through
		// authored tents after spawn is not a straight-line trace promise.
		if (includeProps && layout.m_bComposed)
			return true;
		// The authored side gates meet the main lane at the cross-lane junction.
		vector junctionLocal = Vector(0, 0, layout.m_aEntries[1][2]);
		vector junction = layout.LocalOffsetToWorld(rootMat, junctionLocal);
		if (!IsRouteSegmentClear(junction, capture, includeProps))
			return false;
		int reachable = 0;
		int i;
		int n = layout.m_aEntries.Count();
		for (i = 0; i < n; i++)
		{
			vector entry = layout.LocalOffsetToWorld(rootMat, layout.m_aEntries[i]);
			if (IsRouteSegmentClear(entry, junction, includeProps))
				reachable = reachable + 1;
		}
		return reachable >= 2;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsRouteSegmentClear(vector from, vector to, bool checkNavmesh)
	{
		vector delta = to - from;
		delta[1] = 0;
		float len = delta.Length();
		if (len < 1)
			return true;
		vector dir = delta / len;
		int steps = Math.Ceil(len / LANE_SAMPLE_M);
		vector previous = from;
		previous[1] = IA_BasePlayerSampler.SampleSupportY(previous);
		int i;
		for (i = 0; i <= steps; i++)
		{
			vector p = from + (dir * Math.Min(len, i * LANE_SAMPLE_M));
			p[1] = GetPlacementWorld().GetSurfaceY(p[0], p[2]);
			if (IsOcean(p))
				return false;
			if (!IA_SpawnPlacement.HasStandRoom(p))
				return false;
			// A swept standing body catches walls between samples and low obstacles.
			ref TraceBox body = new TraceBox();
			body.Start = previous + Vector(0, 0.2, 0);
			body.End = p + Vector(0, 0.2, 0);
			body.Mins = Vector(-0.4, 0, -0.4);
			body.Maxs = Vector(0.4, 1.6, 0.4);
			body.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			if (GetPlacementWorld().TraceMove(body, null) < 1)
				return false;
			if (checkNavmesh)
			{
				if (!HasLocalNavmesh(p))
					return false;
			}
			previous = p;
		}
		return true;
	}

	protected bool HasLocalNavmesh(vector point)
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return false;
		aiWorld.RequestNavmeshLoad(point);
		NavmeshWorldComponent navmesh = aiWorld.GetNavmeshWorldComponent("Soldiers");
		if (!navmesh)
			return false;
		if (!navmesh.IsTileLoaded(point))
		{
			if (!navmesh.IsTileRequested(point))
				navmesh.LoadTileIn(point);
			return false;
		}
		if (!navmesh.IsTileValid(point))
			return false;
		// The ordinary spawn helper searches 16 metres away. That is unsuitable
		// for validating a lane sample: a valid result can be far from the lane.
		vector reachable = point;
		return navmesh.GetReachablePoint(point, 1, reachable) && vector.Distance(point, reachable) <= 1.5;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PreflightResources(notnull IA_DynamicSiteLayout layout)
	{
		int n = layout.m_aModules.Count();
		int i;
		for (i = 0; i < n; i++)
		{
			IA_DynamicSiteModule mod = layout.m_aModules[i];
			if (!mod)
				continue;
			Resource res = Resource.Load(mod.m_Prefab);
			if (!res && mod.m_iRole != IA_DynamicSiteModuleRole.Dressing)
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

		if (mod.m_iRole == IA_DynamicSiteModuleRole.Dressing)
		{
			vector siteMat[4];
			site.GetLayout().BuildRootTransform(site.GetOrigin(), site.GetYawDeg(), siteMat);
			vector dressingCenter = site.GetLayout().LocalOffsetToWorld(siteMat, mod.m_vLocalPosition);
			foreach (IA_StaticGunRecord gun : site.GetEmplacements())
			{
				if (gun.ContainsReservedPoint(dressingCenter, Math.Sqrt(mod.m_fHalfWidthM * mod.m_fHalfWidthM + mod.m_fHalfDepthM * mod.m_fHalfDepthM)))
					return false;
			}
		}

		vector rootMat[4];
		site.GetLayout().BuildRootTransform(site.GetOrigin(), site.GetYawDeg(), rootMat);
		vector worldMat[4];
		site.GetLayout().LocalToWorld(rootMat, mod.m_vLocalPosition, mod.m_fLocalYawDeg, mod.m_fPrefabYawCorrectionDeg, worldMat);
		AlignComposition(worldMat, mod);
		float supportY;
		if (!SampleModuleSupport(worldMat, mod, supportY))
			return false;
		IA_DynamicSiteInstance ignoreSite;
		if (mod.m_iPerimeterSide >= 0)
			ignoreSite = site;
		if (!IsModuleVolumeClear(worldMat, mod, ignoreSite))
			return false;
		vector spawnPos = worldMat[3];
		spawnPos[1] = supportY;
		worldMat[3] = spawnPos;
		if (mod.m_iGroundingPolicy == IA_DynamicSiteGrounding.TerrainSegment)
		{
			ref TraceParam groundTrace = new TraceParam();
			groundTrace.Flags = TraceFlags.WORLD;
			SCR_TerrainHelper.SnapAndOrientToTerrain(worldMat, GetPlacementWorld(), false, groundTrace);
		}

		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(worldMat, params.Transform);
		IEntity ent = GetGame().SpawnEntityPrefab(res, GetPlacementWorld(), params);
		if (!ent)
			return false;
		site.AddRoot(ent);
		if (mod.m_iPerimeterSide >= 0)
			site.RegisterPanel(mod.m_sId, ent);
		return true;
	}

}
