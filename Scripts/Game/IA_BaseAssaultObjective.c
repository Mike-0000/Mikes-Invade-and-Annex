//------------------------------------------------------------------------------------------------
//! Seize → Regroup → Warning → Defend at one physical site.
//------------------------------------------------------------------------------------------------
class IA_BaseAssaultObjective
{
	static const int WARNING_MS = 30000;
	static const int TICK_CLAMP_MS = 2000;

	protected int m_iSerial;
	protected int m_iGroupId;
	protected ref IA_BaseObjectiveSettings m_Settings;
	protected ref IA_DynamicSiteInstance m_Site;
	protected ref IA_BasePlayerSampler m_Sampler;
	protected int m_ePhase;
	protected int m_iCaptureAccMs;
	protected int m_iRegroupStartMs;
	protected int m_iWarningStartMs;
	protected int m_iLastTickMs;
	protected int m_iInitialTarget;
	protected bool m_bCaptureAwarded;
	protected bool m_bTaskPublished;
	protected bool m_bDefenseStarted;
	protected bool m_bResultSent;
	protected ref map<string, int> m_CaptureLedger;
	protected IA_DefendMission m_Defend;
	protected IA_DynamicObjectiveDirector m_Director;

	//------------------------------------------------------------------------------------------------
	void IA_BaseAssaultObjective()
	{
		m_Sampler = new IA_BasePlayerSampler();
		m_CaptureLedger = new map<string, int>();
		m_ePhase = IA_BaseObjectivePhase.None;
	}

	//------------------------------------------------------------------------------------------------
	void SetDirector(IA_DynamicObjectiveDirector director)
	{
		m_Director = director;
	}

	//------------------------------------------------------------------------------------------------
	void Begin(int serial, int groupId, IA_BaseObjectiveSettings settings, IA_BasePlayerSampler sampler)
	{
		m_iSerial = serial;
		m_iGroupId = groupId;
		m_Settings = settings;
		m_ePhase = IA_BaseObjectivePhase.Placing;
		m_iCaptureAccMs = 0;
		m_iRegroupStartMs = 0;
		m_iWarningStartMs = 0;
		m_iLastTickMs = System.GetTickCount();
		m_bCaptureAwarded = false;
		m_bTaskPublished = false;
		m_bDefenseStarted = false;
		m_bResultSent = false;
		m_Sampler = sampler;
	}

	//------------------------------------------------------------------------------------------------
	int GetPhase()
	{
		return m_ePhase;
	}

	//------------------------------------------------------------------------------------------------
	int GetSerial()
	{
		return m_iSerial;
	}

	//------------------------------------------------------------------------------------------------
	IA_DynamicSiteInstance GetSite()
	{
		return m_Site;
	}

	//------------------------------------------------------------------------------------------------
	IA_BasePlayerSampler GetSampler()
	{
		return m_Sampler;
	}

	//------------------------------------------------------------------------------------------------
	void Tick(int nowMs)
	{
		int dt = nowMs - m_iLastTickMs;
		if (dt < 0)
			dt = 0;
		if (dt > TICK_CLAMP_MS)
			dt = TICK_CLAMP_MS;
		m_iLastTickMs = nowMs;

		vector assembly = vector.Zero;
		float assemblyRadius = 0;
		if (m_Site)
		{
			assembly = m_Site.GetAssemblyPoint();
			assemblyRadius = IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M;
		}
		m_Sampler.TickRecord(System.GetUnixTime(), assembly, assemblyRadius);

		if (m_ePhase == IA_BaseObjectivePhase.Seize)
			TickSeize(dt);
		else if (m_ePhase == IA_BaseObjectivePhase.Regroup)
			TickRegroup(nowMs);
		else if (m_ePhase == IA_BaseObjectivePhase.Warning)
			TickWarning(nowMs);
	}

	//------------------------------------------------------------------------------------------------
	void OnSiteReady(int serial, IA_DynamicSiteInstance site)
	{
		if (serial != m_iSerial)
			return;
		if (!site)
		{
			OnSiteFailed(serial, "null_site");
			return;
		}

		m_Site = site;
		m_ePhase = IA_BaseObjectivePhase.Seize;
		m_iCaptureAccMs = 0;
		m_Site.SpawnMapMarker();
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (init)
			init.RetireOrdinaryObjectivesForBase(serial, site.GetHost());
		PublishSeizeTask();
		PublishStatus(true);
	}

	//------------------------------------------------------------------------------------------------
	void OnSiteFailed(int serial, string reason)
	{
		if (serial != m_iSerial || m_ePhase != IA_BaseObjectivePhase.Placing)
			return;
		m_ePhase = IA_BaseObjectivePhase.Cancelled;
		PublishStatus(true);
		EmitResult(IA_DynamicObjectiveResult.Fallback, reason);
	}

	//------------------------------------------------------------------------------------------------
	void OnDefenseEnded(int serial, bool completed)
	{
		if (serial != m_iSerial || m_ePhase != IA_BaseObjectivePhase.Defend || m_bResultSent)
			return;
		if (completed)
		{
			m_ePhase = IA_BaseObjectivePhase.Completed;
			PublishStatus(true);
			if (m_Site)
			{
				m_Site.RemoveMapMarker();
				if (m_Director)
					m_Director.RetireSite(m_Site);
			}
			m_Site = null;
			EmitResult(IA_DynamicObjectiveResult.Completed, "defense");
			return;
		}

		m_ePhase = IA_BaseObjectivePhase.Failed;
		PublishStatus(true);
		EmitResult(IA_DynamicObjectiveResult.Failed, "defense_abort");
	}

	//------------------------------------------------------------------------------------------------
	void Cancel(int reason)
	{
		if (m_ePhase == IA_BaseObjectivePhase.Completed)
			return;
		if (m_ePhase == IA_BaseObjectivePhase.Cancelled)
			return;

		m_ePhase = IA_BaseObjectivePhase.Cancelled;
		DismissTask();
		if (m_Defend)
		{
			m_Defend.AbortDefendMission();
			m_Defend = null;
		}
		if (m_Site)
		{
			m_Site.RemoveMapMarker();
			if (m_Director)
				m_Director.RetireSite(m_Site);
			m_Site = null;
		}
		PublishStatus(true);
		EmitResult(IA_DynamicObjectiveResult.Aborted, reason.ToString());
	}

	//------------------------------------------------------------------------------------------------
	bool AdminBypassToDefend()
	{
		if (m_ePhase != IA_BaseObjectivePhase.Seize && m_ePhase != IA_BaseObjectivePhase.Regroup && m_ePhase != IA_BaseObjectivePhase.Warning)
			return false;
		if (!m_Site)
			return false;
		Print("[IA][Base] Admin bypass into prepared defense at the live base.", LogLevel.WARNING);
		StartPreparedDefense();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSeize(int dtMs)
	{
		if (!m_Site || !m_Site.IsHostLive())
		{
			m_ePhase = IA_BaseObjectivePhase.Failed;
			PublishStatus(true);
			EmitResult(IA_DynamicObjectiveResult.Failed, "site_lost");
			return;
		}

		vector cap = m_Site.GetCapturePoint();
		float radius = m_Site.GetCaptureRadius();
		bool friendly = m_Sampler.HasLivingFriendlyInZone(cap, radius);
		bool hostile = m_Sampler.HasHostileInZone(cap, radius);
		if (friendly && !hostile)
		{
			m_iCaptureAccMs = m_iCaptureAccMs + dtMs;
			AccrueCapture(dtMs);
		}

		int need = 90000;
		if (m_Settings)
			need = m_Settings.GetCaptureMs();
		if (m_iCaptureAccMs >= need)
			EnterRegroup();

		PublishStatus(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnterRegroup()
	{
		if (m_ePhase != IA_BaseObjectivePhase.Seize)
			return;

		AwardCaptureOnce();
		m_ePhase = IA_BaseObjectivePhase.Regroup;
		m_iRegroupStartMs = System.GetTickCount();
		m_Sampler.FreezeAtCapture(m_Site.GetAssemblyPoint(), IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M, m_Settings.m_fRegroupFraction);
		m_iInitialTarget = m_Sampler.GetInitialTarget();
		PublishRegroupTask();
		PublishStatus(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickRegroup(int nowMs)
	{
		if (!m_Site || !m_Site.IsHostLive())
		{
			m_ePhase = IA_BaseObjectivePhase.Failed;
			PublishStatus(true);
			EmitResult(IA_DynamicObjectiveResult.Failed, "site_lost");
			return;
		}

		vector cap = m_Site.GetCapturePoint();
		if (m_Sampler.HasHostileInZone(cap, m_Site.GetCaptureRadius()))
		{
			PublishStatus(false);
			return;
		}

		int elapsed = nowMs - m_iRegroupStartMs;
		int minMs = 90000;
		int maxMs = 240000;
		if (m_Settings)
		{
			minMs = m_Settings.GetRegroupMinMs();
			maxMs = m_Settings.GetRegroupMaxMs();
		}

		int eligiblePresent = m_Sampler.CountEligiblePresent(m_Site.GetAssemblyPoint(), IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M);
		int allPresent = m_Sampler.CountAllGroundedPresent(m_Site.GetAssemblyPoint(), IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M);
		int target = m_Sampler.GetCurrentTarget(System.GetUnixTime());
		if (target < 1)
			target = 1;

		bool ready = false;
		if (elapsed >= minMs && eligiblePresent >= target)
			ready = true;
		else if (elapsed >= maxMs && allPresent >= 1)
			ready = true;

		if (ready)
			EnterWarning(nowMs);

		PublishStatus(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnterWarning(int nowMs)
	{
		m_ePhase = IA_BaseObjectivePhase.Warning;
		m_iWarningStartMs = nowMs;
		PublishStatus(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickWarning(int nowMs)
	{
		if (!m_Site || !m_Site.IsHostLive())
		{
			m_ePhase = IA_BaseObjectivePhase.Failed;
			PublishStatus(true);
			EmitResult(IA_DynamicObjectiveResult.Failed, "site_lost");
			return;
		}

		vector cap = m_Site.GetCapturePoint();
		if (m_Sampler.HasHostileInZone(cap, m_Site.GetCaptureRadius()))
		{
			m_ePhase = IA_BaseObjectivePhase.Regroup;
			m_iWarningStartMs = 0;
			PublishStatus(true);
			return;
		}

		int allPresent = m_Sampler.CountAllGroundedPresent(m_Site.GetAssemblyPoint(), IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M);
		if (allPresent < 1)
		{
			m_ePhase = IA_BaseObjectivePhase.Regroup;
			m_iWarningStartMs = 0;
			PublishStatus(true);
			return;
		}

		if ((nowMs - m_iWarningStartMs) >= WARNING_MS)
			StartPreparedDefense();

		PublishStatus(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartPreparedDefense()
	{
		if (m_bDefenseStarted)
			return;
		if (!m_Site || !m_Site.GetHost())
			return;

		m_bDefenseStarted = true;
		DismissTask();
		IA_Config defenseCfg = null;
		if (m_Settings)
			defenseCfg = m_Settings.m_DefenseSnapshot;
		m_Defend = IA_DefendMission.CreateForDynamicBase(m_Site.GetAssemblyPoint(), m_iGroupId, "operating base", m_Site.GetHost(), ResolveEnemy(), m_iSerial, defenseCfg);
		if (!m_Defend)
		{
			m_ePhase = IA_BaseObjectivePhase.Failed;
			PublishStatus(true);
			EmitResult(IA_DynamicObjectiveResult.Failed, "defense_create");
			return;
		}

		m_ePhase = IA_BaseObjectivePhase.Defend;
		IA_Game game = IA_Game.Instantiate();
		if (game)
			game.SetActiveDefendMission(m_Defend);
		m_Defend.StartDefendMission();
		PublishStatus(true);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction ResolveEnemy()
	{
		if (m_Settings)
			return m_Settings.m_EnemyFaction;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void AccrueCapture(int dtMs)
	{
		int add = Math.Round(dtMs / 1000.0);
		if (add < 1)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm || !m_Site)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		vector cap = m_Site.GetCapturePoint();
		float radius = m_Site.GetCaptureRadius();
		int count = ids.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (!m_Sampler.IsPlayerGroundedPresent(ids[i], cap, radius))
				continue;
			string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(ids[i]);
			if (guid.IsEmpty())
				continue;
			int prev = 0;
			if (m_CaptureLedger.Contains(guid))
				prev = m_CaptureLedger.Get(guid);
			m_CaptureLedger.Set(guid, prev + add);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardCaptureOnce()
	{
		if (m_bCaptureAwarded)
			return;
		m_bCaptureAwarded = true;

		IA_StatsManager stats = IA_StatsManager.GetInstance();
		foreach (string guid, int score : m_CaptureLedger)
		{
			if (!stats || score <= 0)
				continue;
			string name = IA_AreaMarker.GetPlayerNameFromGuid(guid);
			stats.QueueCaptureContribution(guid, name, score);
		}
		m_CaptureLedger.Clear();

		if (m_Site && m_Site.GetHost())
			m_Site.GetHost().CompleteTaskByTitle("Seize the enemy operating base");
	}

	//------------------------------------------------------------------------------------------------
	protected void PublishSeizeTask()
	{
		if (!m_Site || !m_Site.GetHost())
			return;
		m_Site.GetHost().DismissOpenTasks();
		m_Site.GetHost().QueueTask("Seize the enemy operating base", "Clear and secure the command area. Counterattack preparations begin after capture.", m_Site.GetCapturePoint());
		m_bTaskPublished = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void PublishRegroupTask()
	{
		if (!m_Site || !m_Site.GetHost())
			return;
		m_Site.GetHost().DismissOpenTasks();
		m_Site.GetHost().QueueTask("Regroup at the captured base", "Assemble and prepare for the counterattack.", m_Site.GetAssemblyPoint());
	}

	//------------------------------------------------------------------------------------------------
	protected void DismissTask()
	{
		if (m_Site && m_Site.GetHost())
			m_Site.GetHost().DismissOpenTasks();
	}

	//------------------------------------------------------------------------------------------------
	void PublishStatus(bool force)
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		int reason = IA_BaseStatusReason.None;
		int remain = 0;
		int capturePermille = 0;
		int eligiblePresent = 0;
		int allPresent = 0;
		int target = m_Sampler.GetCurrentTarget(System.GetUnixTime());
		vector sitePos = vector.Zero;
		vector capPos = vector.Zero;
		float capR = 0;
		string siteId = "0";

		if (m_Site)
		{
			sitePos = m_Site.GetAssemblyPoint();
			capPos = m_Site.GetCapturePoint();
			capR = m_Site.GetCaptureRadius();
			siteId = m_Site.GetSiteId().ToString();
			eligiblePresent = m_Sampler.CountEligiblePresent(sitePos, IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M);
			allPresent = m_Sampler.CountAllGroundedPresent(sitePos, IA_DynamicSiteInstance.ASSEMBLY_RADIUS_M);
			if (m_ePhase == IA_BaseObjectivePhase.Seize || m_ePhase == IA_BaseObjectivePhase.Regroup || m_ePhase == IA_BaseObjectivePhase.Warning)
			{
				if (m_Sampler.HasHostileInZone(capPos, capR))
					reason = IA_BaseStatusReason.ClearCommand;
			}
		}

		int need = 90000;
		if (m_Settings)
			need = m_Settings.GetCaptureMs();
		if (need > 0)
			capturePermille = Math.Round((1000.0 * m_iCaptureAccMs) / need);
		if (capturePermille > 1000)
			capturePermille = 1000;

		if (m_ePhase == IA_BaseObjectivePhase.Regroup)
		{
			int minMs = 90000;
			int maxMs = 240000;
			if (m_Settings)
			{
				minMs = m_Settings.GetRegroupMinMs();
				maxMs = m_Settings.GetRegroupMaxMs();
			}
			int elapsed = System.GetTickCount() - m_iRegroupStartMs;
			int waitMs = minMs - elapsed;
			if (waitMs < 0)
				waitMs = maxMs - elapsed;
			if (waitMs < 0)
				waitMs = 0;
			remain = Math.Ceil(waitMs / 1000.0);
			if (allPresent < 1)
				reason = IA_BaseStatusReason.AwaitingForces;
		}
		else if (m_ePhase == IA_BaseObjectivePhase.Warning)
		{
			int left = WARNING_MS - (System.GetTickCount() - m_iWarningStartMs);
			if (left < 0)
				left = 0;
			remain = Math.Ceil(left / 1000.0);
		}
		else if (m_ePhase == IA_BaseObjectivePhase.Failed)
		{
			reason = IA_BaseStatusReason.Failed;
		}

		init.PublishBaseObjectiveStatus(force, m_iSerial, m_iGroupId, m_ePhase, siteId, sitePos, capPos, capR, capturePermille, eligiblePresent, target, allPresent, remain, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void EmitResult(int result, string reason)
	{
		if (m_bResultSent)
			return;
		m_bResultSent = true;
		if (m_Director)
			m_Director.OnObjectiveResult(m_iSerial, result, reason);
	}
}
