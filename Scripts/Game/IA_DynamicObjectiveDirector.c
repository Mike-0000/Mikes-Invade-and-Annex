//------------------------------------------------------------------------------------------------
//! One terminal BaseAssault decision per AO activation.
//------------------------------------------------------------------------------------------------
class IA_DynamicObjectiveDirector
{
	protected int m_iSerial;
	protected int m_iGroupId;
	protected bool m_bDecided;
	protected bool m_bAccepted;
	protected bool m_bFallbackDispatched;
	protected bool m_bTickQueued;
	protected ref IA_BaseObjectiveSettings m_Settings;
	protected ref IA_BaseAssaultObjective m_Objective;
	protected ref IA_DynamicSitePlacer m_Placer;
	protected ref array<ref IA_DynamicSiteInstance> m_Retired;
	protected int m_eLastResult = -1;

	//------------------------------------------------------------------------------------------------
	void IA_DynamicObjectiveDirector()
	{
		m_Placer = new IA_DynamicSitePlacer();
		m_Retired = new array<ref IA_DynamicSiteInstance>();
	}

	//------------------------------------------------------------------------------------------------
	void BeginAo(int serial, int groupId)
	{
		if (m_Objective && m_Objective.GetSerial() != serial)
			m_Objective.Cancel(IA_BaseCancelReason.AoReplaced);

		m_iSerial = serial;
		m_iGroupId = groupId;
		m_bDecided = false;
		m_bAccepted = false;
		m_bFallbackDispatched = false;
		m_Settings = null;
		m_Objective = null;
		m_eLastResult = -1;
		m_Placer.BeginPrecompute(serial, groupId);
		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	bool OwnsActivation(int serial)
	{
		return serial == m_iSerial;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBlockingAoCompletion()
	{
		if (!m_bAccepted)
			return false;
		if (!m_Objective)
			return true;
		int phase = m_Objective.GetPhase();
		if (phase == IA_BaseObjectivePhase.Completed)
			return false;
		if (phase == IA_BaseObjectivePhase.Cancelled)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool BlocksAutomaticPressure()
	{
		if (!m_Objective)
			return m_bAccepted;
		int phase = m_Objective.GetPhase();
		if (phase == IA_BaseObjectivePhase.Placing)
			return true;
		if (phase == IA_BaseObjectivePhase.Seize)
			return true;
		if (phase == IA_BaseObjectivePhase.Regroup)
			return true;
		if (phase == IA_BaseObjectivePhase.Warning)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	int CountRetiredSites()
	{
		return m_Retired.Count();
	}

	//------------------------------------------------------------------------------------------------
	bool TryBeginTerminalObjective(bool forceBase = false)
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return false;
		if (init.GetAoActivationSerial() != m_iSerial)
			return false;
		if (m_bDecided)
			return m_bAccepted;
		if (m_Retired.Count() >= 2)
		{
			Print("[IA][Base] Two retired sites still retained — automatic base selection falls back.", LogLevel.WARNING);
			m_bDecided = true;
			m_bAccepted = false;
			return false;
		}

		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!forceBase)
		{
			if (!cfg || !cfg.m_bDynamicBaseEnabled)
			{
				m_bDecided = true;
				return false;
			}
			if (cfg.m_bGameMasterMode && !cfg.m_bDynamicBaseInGm)
			{
				m_bDecided = true;
				return false;
			}
			int chance = cfg.m_iDynamicBaseChancePct;
			if (chance <= 0)
			{
				m_bDecided = true;
				return false;
			}
			if (chance < 100)
			{
				if (Math.RandomFloat01() >= (chance / 100.0))
				{
					m_bDecided = true;
					return false;
				}
			}
		}

		Faction enemy = init.GetRandomEnemyFaction();
		m_Settings = IA_BaseObjectiveSettings.SnapshotFrom(cfg, enemy);
		LogArtMismatch(enemy);
		m_bDecided = true;
		m_bAccepted = true;

		ref IA_BaseAssaultObjective objective = new IA_BaseAssaultObjective();
		objective.SetDirector(this);
		objective.Begin(m_iSerial, m_iGroupId, m_Settings);
		m_Objective = objective;
		m_Placer.BeginPlacement(m_iSerial, m_Settings);
		EnsureTick();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Tick()
	{
		if (m_Placer && m_Placer.IsComplete() && m_Objective && m_Objective.GetPhase() == IA_BaseObjectivePhase.Placing)
		{
			IA_DynamicSiteResult result = m_Placer.TakeResult();
			if (result && result.m_iSerial == m_iSerial)
			{
				if (result.m_bSuccess && result.m_Site)
					m_Objective.OnSiteReady(m_iSerial, result.m_Site);
				else
				{
					string reason = "placement";
					if (result)
						reason = result.m_sReason;
					m_Objective.OnSiteFailed(m_iSerial, reason);
				}
			}
		}

		if (m_Objective)
			m_Objective.Tick(System.GetTickCount());

		int i;
		for (i = m_Retired.Count() - 1; i >= 0; i--)
		{
			IA_DynamicSiteInstance site = m_Retired[i];
			if (!site || site.TickCleanup())
				m_Retired.Remove(i);
		}

		EnsureTick();
	}

	//------------------------------------------------------------------------------------------------
	void Cancel(int cancelReason)
	{
		if (m_Placer)
			m_Placer.Cancel(m_iSerial);
		if (m_Objective)
			m_Objective.Cancel(cancelReason);
		m_bAccepted = false;
	}

	//------------------------------------------------------------------------------------------------
	void OnObjectiveResult(int serial, int result, string reason)
	{
		if (serial != m_iSerial)
			return;
		m_eLastResult = result;
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (init)
			init.HandleDynamicObjectiveResult(serial, result, reason);
	}

	//------------------------------------------------------------------------------------------------
	void RetireSite(IA_DynamicSiteInstance site)
	{
		if (!site)
			return;
		site.BeginDeferredCleanup();
		m_Retired.Insert(site);
	}

	//------------------------------------------------------------------------------------------------
	bool AdminBypassToDefend()
	{
		if (!m_Objective)
			return false;
		return m_Objective.AdminBypassToDefend();
	}

	//------------------------------------------------------------------------------------------------
	IA_BaseAssaultObjective GetObjective()
	{
		return m_Objective;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureTick()
	{
		if (m_bTickQueued)
			return;
		m_bTickQueued = true;
		GetGame().GetCallqueue().Remove(this.OnQueuedTick);
		GetGame().GetCallqueue().CallLater(this.OnQueuedTick, 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQueuedTick()
	{
		m_bTickQueued = false;
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	protected void LogArtMismatch(Faction enemy)
	{
		if (!enemy)
			return;
		string key = enemy.GetFactionKey();
		if (key == "USSR")
			return;
		Print(string.Format("[IA][Base] USSR field-base art is used; enemy infantry/vehicles remain %1.", key), LogLevel.WARNING);
	}
}
