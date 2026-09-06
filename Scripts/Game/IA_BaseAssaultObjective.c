//------------------------------------------------------------------------------------------------
//! Seize -> standard defense at one physical site. Capture completes assembly;
//! no separate regroup gate or counterattack countdown precedes the defense.
//------------------------------------------------------------------------------------------------
class IA_BaseAssaultObjective
{
	static const int TICK_CLAMP_MS = 2000;

	protected int m_iSerial;
	protected int m_iGroupId;
	protected ref IA_BaseObjectiveSettings m_Settings;
	protected ref IA_DynamicSiteInstance m_Site;
	protected ref IA_BasePlayerSampler m_Sampler;
	protected int m_ePhase;
	protected int m_iCaptureAccMs;
	protected int m_iLastTickMs;
	protected bool m_bCaptureAwarded;
	protected bool m_bDefenseStarted;
	protected bool m_bResultSent;
	protected ref map<string, int> m_CaptureLedger;
	protected ref IA_DefendMission m_Defend;
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
		m_iLastTickMs = System.GetTickCount();
		m_bCaptureAwarded = false;
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

		if (m_ePhase == IA_BaseObjectivePhase.Seize)
			TickSeize(dt);
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
		if (m_ePhase != IA_BaseObjectivePhase.Seize || !m_Site)
			return false;
		IA_Log.Info("[IA][Base] Admin bypass into standard defense at the live base.");
		StartBaseDefense();
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
		{
			AwardCaptureOnce();
			StartBaseDefense();
		}

		PublishStatus(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartBaseDefense()
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
		m_Site.GetHost().QueueTask("Seize the enemy operating base", "Clear and secure the command area, then defend the captured base.", m_Site.GetCapturePoint());
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
		int capturePermille = 0;
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
			if (m_ePhase == IA_BaseObjectivePhase.Seize && m_Sampler.HasHostileInZone(capPos, capR))
				reason = IA_BaseStatusReason.ClearCommand;
		}

		int need = 90000;
		if (m_Settings)
			need = m_Settings.GetCaptureMs();
		if (need > 0)
			capturePermille = Math.Round((1000.0 * m_iCaptureAccMs) / need);
		if (capturePermille > 1000)
			capturePermille = 1000;
		if (m_ePhase == IA_BaseObjectivePhase.Failed)
			reason = IA_BaseStatusReason.Failed;

		// Preserve the packed protocol; retired roster/countdown fields stay zero.
		init.PublishBaseObjectiveStatus(force, m_iSerial, m_iGroupId, m_ePhase, siteId, sitePos, capPos, capR, capturePermille, 0, 0, 0, 0, reason);
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
