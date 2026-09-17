// Budget mode retains the complete CURRENT roster, including live squad members.
// Budget 0 drains this ledger, then delegates to the original distance-only cache.
class IA_DynamicAIBudgetCache : IA_DynamicAIGroupCache
{
	static const float CLOSE_M = 300;
	static const float RELEASE_M = 400;
	static const int MIN_LIVE_MS = 30000;
	static const int EVICT_DELAY_MS = 10000;
	protected static int s_iNextOrder;
	protected int m_iOrder;
	protected bool m_bBudgetActive;
	protected bool m_bBudgetPaused;
	protected bool m_bBudgetDeleting;
	protected bool m_bConvertingDowned;
	protected bool m_bExitBudget;
	protected bool m_bForceFull;
	protected bool m_bClose;
	protected int m_iDesired;
	protected int m_iEvictSince = -1;
	protected int m_iPlanAt;
	protected int m_iRetryEvictAt;
	protected int m_iLastCasualtyMs = -1;
	protected float m_fNearest = 10000000;

	override void Init(IA_AiGroup owner)
	{
		super.Init(owner);
		m_iOrder = s_iNextOrder++;
	}

	bool IsBudgetActive()
	{
		return m_bBudgetActive;
	}

	override bool IsPaused()
	{
		if (m_bBudgetActive)
			return m_bBudgetPaused;
		return super.IsPaused();
	}

	override bool ShouldPreserveGroup()
	{
		if (m_bFinished)
			return false;
		if (m_bBudgetActive)
			return m_bBudgetDeleting || GetUnrestoredCount() > 0;
		return super.ShouldPreserveGroup();
	}

	void EnableBudget()
	{
		if (IsCivilianCache() || m_bBudgetActive || m_bFinished)
			return;
		m_bBudgetActive = true;
		m_bBudgetPaused = m_bCached;
		m_bForceFull = m_bWaking;
		m_bExitBudget = false;
		m_iPlanAt = 0;
	}

	void DisableBudget()
	{
		if (!m_bBudgetActive || m_bFinished)
			return;
		m_bExitBudget = true;
		m_bForceFull = true;
		m_iDesired = GetLogicalAliveCount();
		UpdateState();
	}

	override void ResetQuietPeriod()
	{
		super.ResetQuietPeriod();
		m_bClose = false;
		m_iEvictSince = -1;
		m_iPlanAt = 0;
	}

	override void RequestWake(bool urgent = true)
	{
		if (!m_bBudgetActive)
		{
			super.RequestWake(urgent);
			return;
		}
		if (!m_bCached || m_bFinished)
			return;
		if (!m_bWaking)
			m_iWakeRequestedMs = System.GetTickCount();
		m_bForceFull = true;
		m_bWaking = true;
		m_bUrgentWake = m_bUrgentWake || urgent;
		m_iDesired = GetLogicalAliveCount();
		m_iEvictSince = -1;
	}

	override void CheckWake(array<vector> players, bool enabled)
	{
		if (!m_bBudgetActive)
		{
			super.CheckWake(players, enabled);
			return;
		}
		if (!enabled || m_bExitBudget)
			RequestWake(false);
		CheckProtection(players);
	}

	protected float NearestDistance(vector position, array<vector> players)
	{
		float nearestSq = 100000000000000;
		foreach (vector player : players)
		{
			float dx = position[0] - player[0];
			float dz = position[2] - player[2];
			nearestSq = Math.Min(nearestSq, dx * dx + dz * dz);
		}
		return Math.Sqrt(nearestSq);
	}

	protected vector CurrentPosition(IA_DynamicAIUnit unit)
	{
		if (!unit.m_Entity)
			return unit.GetPosition();
		vector transform[4];
		unit.m_Entity.GetWorldTransform(transform);
		return transform[3];
	}

	void CheckProtection(array<vector> players)
	{
		if (!IsOwnerLive() || m_bFinished)
			return;
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		m_fNearest = 10000000;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.IsLogicallyAlive())
				m_fNearest = Math.Min(m_fNearest, NearestDistance(CurrentPosition(unit), players));
		}
		if (m_fNearest <= tuning.m_iDynamicAICloseDistanceM)
			m_bClose = true;
		else if (m_fNearest > tuning.m_iDynamicAIReleaseDistanceM)
			m_bClose = false;
		string reason = m_Owner.GetDynamicAIRoleBlockReason();
		if (m_bClose || HasRecentCombat() || (reason != "" && reason != "initialization"))
			RequestWake();
	}

	protected bool HasRecentCombat()
	{
		int combatQuietSec = IA_DynamicAISpawning.GetTuning().m_iDynamicAICombatQuietSec;
		if (m_iLastCasualtyMs >= 0 && System.GetTickCount() - m_iLastCasualtyMs < combatQuietSec * 1000)
			return true;
		int danger = m_Owner.GetLastDangerEventTime();
		if (danger > 0 && System.GetUnixTime() - danger < combatQuietSec)
			return true;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (!unit.m_Entity || unit.m_bDead)
				continue;
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(unit.m_Entity.FindComponent(SCR_AICombatComponent));
			if (combat && combat.GetCurrentTarget() && combat.GetCurrentTarget().GetTimeSinceSeen() < combatQuietSec)
				return true;
		}
		return false;
	}

	// New native members enter the ledger; transferred, deleted or dead live
	// members leave it permanently. An absent saved reserve is never re-seeded.
	void RefreshRoster(int now)
	{
		if (!m_bBudgetActive || !IsOwnerLive() || m_bFinished)
			return;
		SCR_AIGroup group = m_Owner.GetSCR_AIGroup();
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		ref array<IEntity> members = {};
		PlayerManager manager = GetGame().GetPlayerManager();
		foreach (AIAgent agent : agents)
		{
			if (!agent || !agent.GetControlledEntity())
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (manager && manager.GetPlayerIdFromControlledEntity(entity) > 0)
				continue;
			ChimeraCharacter pawn = ChimeraCharacter.Cast(entity);
			if (pawn && pawn.GetCharacterController() && pawn.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
				continue;
			members.Insert(entity);
			bool known = false;
			foreach (IA_DynamicAIUnit existing : m_aUnits)
			{
				if (existing.m_Entity == entity)
				{
					known = true;
					break;
				}
			}
			if (known)
				continue;
			ref IA_DynamicAIUnit added = new IA_DynamicAIUnit();
			added.m_Entity = entity;
			added.m_bRestored = true;
			added.m_iBudgetLiveSinceMs = now;
			entity.GetWorldTransform(added.m_aTransform);
			m_aUnits.Insert(added);
		}
		foreach (IA_DynamicAIUnit record : m_aUnits)
		{
			if (!record.m_bRestored)
				continue;
			if (!record.IsLogicallyAlive() || !members.Contains(record.m_Entity))
				record.m_bDead = true;
			if (!record.m_bDead)
				record.m_bBudgetLeader = record.m_Entity == group.GetLeaderEntity();
		}
		// Corpses/transfers are no longer roster slots. Removing their identity
		// also lets an externally transferred survivor be adopted if it rejoins.
		for (int index = m_aUnits.Count() - 1; index >= 0; index--)
		{
			if (m_aUnits[index].m_bDead && m_aUnits[index].m_bRestored)
				m_aUnits.RemoveOrdered(index);
		}
		UpdateState();
	}

	protected bool CanRemoveUnit(IA_DynamicAIUnit unit, int now, PlayerManager manager)
	{
		if (!unit.m_bRestored || unit.m_bDead || !unit.m_Entity || !manager || now - unit.m_iBudgetLiveSinceMs < IA_DynamicAISpawning.GetTuning().m_iDynamicAIMinLiveSec * 1000)
			return false;
		SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(unit.m_Entity);
		if (!pawn || pawn.GetParent() || pawn.IsInVehicle() || manager.GetPlayerIdFromControlledEntity(pawn) > 0)
			return false;
		if (IA_DynamicAIOccupancyHost.IsCacheParticipating(this))
			return false;
		AIAgent agent = GetRestoreAgent(unit);
		if (!agent || agent.GetParentGroup() != m_Owner.GetSCR_AIGroup())
			return false;
		CharacterControllerComponent controller = pawn.GetCharacterController();
		if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return false;
		if (controller.IsUnconscious())
			return true;
		return CanRecreateHealthyInfantry(pawn, manager);
	}

	void Describe(IA_DynamicAIBudgetEntry entry, array<vector> players, int now)
	{
		RefreshRoster(now);
		if (!IsOwnerLive() || m_bFinished)
			return;
		CheckProtection(players);
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		entry.m_iAlive = GetLogicalAliveCount();
		entry.m_iPreviousDesired = m_iDesired;
		entry.m_fDistance = m_fNearest;
		entry.m_iOrder = m_iOrder;
		entry.m_iRetentionBiasM = tuning.m_iDynamicAIRetentionBiasM;
		entry.m_bInRange = m_fNearest <= tuning.m_iDynamicAIWakeDistanceM;
		if (m_iDesired > 0 && m_fNearest <= tuning.m_iDynamicAICacheDistanceM)
			entry.m_bInRange = true;
		string reason = m_Owner.GetDynamicAIRoleBlockReason();
		bool full = m_bForceFull || m_bClose || HasRecentCombat() || (reason != "" && reason != "initialization");
		PlayerManager manager = GetGame().GetPlayerManager();
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			unit.m_bBudgetProtected = false;
			if (!unit.IsLogicallyAlive())
				continue;
			if (unit.m_bRestored)
				entry.m_iPhysical++;
			unit.m_bBudgetProtected = full || unit.m_bBudgetAdmitted;
			if (unit.m_bRestored && (reason == "initialization" || NearestDistance(CurrentPosition(unit), players) <= tuning.m_iDynamicAIReleaseDistanceM || !CanRemoveUnit(unit, now, manager)))
				unit.m_bBudgetProtected = true;
			if (unit.m_bBudgetProtected)
				entry.m_iProtected++;
		}
	}

	void SetAllocation(int desired, int now)
	{
		m_iDesired = Math.Clamp(desired, 0, GetLogicalAliveCount());
		m_iPlanAt = now;
		if (GetBudgetCost() > m_iDesired)
		{
			if (m_iEvictSince < 0)
				m_iEvictSince = now;
		}
		else
			m_iEvictSince = -1;
	}

	int GetDesired()
	{
		return m_iDesired;
	}

	// Living AI members plus pending creations/admitted failures not yet in it.
	// Reserved failed slots prevent optional work from overcommitting the target.
	int GetBudgetCost()
	{
		if (!IsOwnerLive() || m_bFinished)
			return 0;
		int count = m_Owner.GetDynamicAIPhysicalAliveCount();
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bDead || unit.m_bRestored || !unit.m_bBudgetAdmitted)
				continue;
			AIAgent agent = GetRestoreAgent(unit);
			if (!agent || agent.GetParentGroup() != m_Owner.GetSCR_AIGroup())
				count++;
		}
		return count;
	}

	bool HasMandatoryWork()
	{
		if (!m_bCached)
			return false;
		if (m_bForceFull || m_bExitBudget)
			return true;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bBudgetAdmitted && !unit.m_bRestored && !unit.m_bDead)
				return true;
		}
		return false;
	}

	bool RestoreBudgetUnit(int now, bool optional)
	{
		if (!m_bBudgetActive || !m_bCached || m_bFinished || !IsOwnerLive())
			return false;
		if (!optional && !HasMandatoryWork())
			return false;
		if (optional && (m_iPlanAt == 0 || now - m_iPlanAt > 5000 || GetBudgetCost() >= m_iDesired))
			return false;
		ref IA_DynamicAIUnit chosen;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bRestored || unit.m_bDead || now < unit.m_iNextAttemptMs)
				continue;
			if (!optional && !m_bForceFull && !m_bExitBudget && !unit.m_bBudgetAdmitted)
				continue;
			if (!chosen || unit.m_bBudgetLeader)
				chosen = unit;
			if (unit.m_bBudgetLeader)
				break;
		}
		if (!chosen)
			return false;
		chosen.m_bBudgetAdmitted = true;
		m_bCached = true;
		m_bWaking = true;
		RestoreUnit(chosen, now);
		if (!IsOwnerLive() || m_bFinished)
			return true;
		if (chosen.m_bRestored)
		{
			chosen.m_bBudgetAdmitted = false;
			chosen.m_iBudgetLiveSinceMs = now;
		}
		UpdateState();
		return true;
	}

	bool EvictBudgetUnit(array<vector> players, int now)
	{
		if (!m_bBudgetActive || m_bExitBudget || !IsOwnerLive() || m_bFinished || m_bForceFull || m_iPlanAt == 0 || now - m_iPlanAt > 5000)
			return false;
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		if (now < m_iRetryEvictAt || m_iEvictSince < 0 || now - m_iEvictSince < tuning.m_iDynamicAIEvictDelaySec * 1000 || GetBudgetCost() <= m_iDesired)
			return false;
		// Revalidate current players, combat, role, membership and health at commit.
		CheckProtection(players);
		if (m_bClose || m_bForceFull || HasRecentCombat() || m_Owner.GetDynamicAIRoleBlockReason() != "")
		{
			m_iEvictSince = -1;
			return false;
		}
		PlayerManager manager = GetGame().GetPlayerManager();
		ref IA_DynamicAIUnit chosen;
		float farthest = -1;
		IEntity leader = m_Owner.GetSCR_AIGroup().GetLeaderEntity();
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (!CanRemoveUnit(unit, now, manager))
				continue;
			float distance = NearestDistance(CurrentPosition(unit), players);
			if (distance <= tuning.m_iDynamicAIReleaseDistanceM)
				continue;
			// Prefer retaining the leader, but never let it hold an extra slot
			// when another protected member already keeps the group alive.
			if (m_iDesired > 0 && chosen && chosen.m_Entity != leader && unit.m_Entity == leader)
				continue;
			if (m_iDesired > 0 && chosen && chosen.m_Entity == leader && unit.m_Entity != leader)
				farthest = -1;
			if (distance > farthest)
			{
				farthest = distance;
				chosen = unit;
			}
		}
		if (!chosen)
			return false;
		SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(chosen.m_Entity);
		if (pawn.GetCharacterController().IsUnconscious())
		{
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(pawn.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!damage)
				return false;
			m_bConvertingDowned = true;
			damage.Kill(damage.GetInstigator());
			m_bConvertingDowned = false;
			if (IsOwnerLive() && !m_bFinished)
			{
				if (pawn && pawn.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD)
					m_iRetryEvictAt = now + 30000;
				UpdateState();
			}
			return true;
		}
		if (!chosen.Capture(pawn))
		{
			m_iRetryEvictAt = now + 30000;
			return false;
		}
		chosen.m_bBudgetLeader = pawn == leader;
		chosen.m_bRestored = false;
		chosen.m_bBudgetAdmitted = false;
		chosen.m_iFailures = 0;
		chosen.m_iNextAttemptMs = 0;
		m_bCached = true;
		m_bBudgetDeleting = true;
		m_Owner.PrepareDynamicAIUnitRemoval(pawn);
		if (IsOwnerLive() && !m_bFinished && m_Owner.GetDynamicAIPhysicalAliveCount() <= 1)
		{
			m_bBudgetPaused = true;
			m_Owner.SuspendForDynamicAI();
		}
		if (!IsOwnerLive() || m_bFinished)
			return true;
		chosen.m_Entity = null;
		RplComponent.DeleteRplEntity(pawn, false);
		m_bBudgetDeleting = false;
		if (IsOwnerLive() && !m_bFinished)
			UpdateState();
		return true;
	}

	override bool CacheOccupancyPawn(IA_DynamicAIOccupancySeat seat)
	{
		bool cached = super.CacheOccupancyPawn(seat);
		if (cached && IsOwnerLive() && !m_bFinished)
			UpdateState();
		return cached;
	}

	bool WantsOccupancyEviction()
	{
		if (!IsOwnerLive() || m_bFinished)
			return false;
		if (IA_DynamicAIOccupancyHost.IsCacheParticipating(this))
			return true;
		if (!m_Owner || !m_Owner.HasOccupancyMembers())
			return false;
		if (m_bBudgetActive)
		{
			if (m_bClose || m_bForceFull || HasRecentCombat() || m_iDesired > 0)
				return false;
			return IA_DynamicAIOccupancyHost.LinkedGroupsAllowEvict(m_Owner);
		}
		return true;
	}

	protected void UpdateState()
	{
		if (!m_bBudgetActive || !IsOwnerLive() || m_bFinished || m_bBudgetDeleting)
			return;
		bool hadMissing = m_bCached;
		m_bCached = GetUnrestoredCount() > 0;
		bool pause = m_bCached && m_Owner.GetDynamicAIPhysicalAliveCount() == 0;
		bool wasPaused = m_bBudgetPaused;
		m_bBudgetPaused = pause;
		if (pause && !wasPaused)
			m_Owner.SuspendForDynamicAI();
		if (!IsOwnerLive() || m_bFinished)
			return;
		if (!m_bCached)
		{
			m_bForceFull = false;
			m_bWaking = false;
			m_bUrgentWake = false;
			if (m_bExitBudget)
			{
				m_bBudgetActive = false;
				m_bExitBudget = false;
				m_aUnits.Clear();
				ResetQuietPeriod();
			}
		}
		if (!pause && (wasPaused || (hadMissing && !m_bCached)))
			m_Owner.ResumeAfterDynamicAI();
	}

	override void OnUnitKilled(IEntity entity)
	{
		super.OnUnitKilled(entity);
		if (m_bBudgetActive)
		{
			// The first long-range shot may kill the last physical member before
			// its combat component can report a target. Wake its surviving reserves.
			if (entity && !m_bConvertingDowned && !m_bBudgetDeleting)
			{
				m_iLastCasualtyMs = System.GetTickCount();
				RequestWake();
			}
			UpdateState();
		}
	}

	override string GetDiagnosticStatus()
	{
		if (!m_bBudgetActive)
			return super.GetDiagnosticStatus();
		if (m_bBudgetPaused)
			return "budget dormant";
		if (m_bCached)
			return "budget partial";
		return "budget live";
	}

	override void Retire()
	{
		super.Retire();
		m_bBudgetActive = false;
		m_bBudgetPaused = false;
		m_bBudgetDeleting = false;
		m_bForceFull = false;
	}
}
