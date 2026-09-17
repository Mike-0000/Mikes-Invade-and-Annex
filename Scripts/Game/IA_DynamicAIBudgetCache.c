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
	protected int m_iCaptureSeedMs = -1;
	protected float m_fNearest = 10000000;
	protected vector m_vNearestPlayer;

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
		if (IsCivilianCache() || IsVehicleCache() || m_bBudgetActive || m_bFinished)
			return;
		m_bBudgetActive = true;
		m_bBudgetPaused = m_bCached;
		// A legacy wake does not become a force-full restore; the allocator
		// decides how many of these reserves fit under the shared target.
		m_bForceFull = false;
		m_bExitBudget = false;
		m_iPlanAt = 0;
	}

	// Reserve-first spawn hands over soldiers the group never created. They enter
	// the ledger exactly like evicted survivors, so one restore path serves both.
	void AdoptReserveSeeds(notnull array<ref IA_DynamicAIUnit> seeds)
	{
		if (m_bFinished || IsCivilianCache() || IsVehicleCache())
			return;
		EnableBudget();
		if (!m_bBudgetActive)
			return;
		int adopted;
		foreach (IA_DynamicAIUnit seed : seeds)
		{
			if (!seed || seed.m_sPrefab.IsEmpty() || seed.m_bRestored || seed.m_bDead)
				continue;
			seed.m_bBudgetAdmitted = false;
			seed.m_bBudgetProtected = false;
			seed.m_bBudgetLeader = false;
			m_aUnits.Insert(seed);
			adopted++;
		}
		if (adopted == 0)
			return;
		m_bCached = true;
		m_iPlanAt = 0;
		RefreshRoster(System.GetTickCount());
		UpdateState();
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
		m_bWaking = true;
		m_bUrgentWake = m_bUrgentWake || urgent;
		m_iEvictSince = -1;
	}

	// OFF and budget-0 drains restore every survivor. Approach, combat, capture,
	// and defend only mark the group waking so the allocator can fill nearest first.
	void RequestMandatoryRestore(bool urgent = true)
	{
		RequestWake(urgent);
		if (!m_bBudgetActive || !m_bCached || m_bFinished)
			return;
		m_bForceFull = true;
		m_iDesired = GetLogicalAliveCount();
	}

	// Capture progress needs at least one physical defender per contested group.
	// This seeds exactly one soldier over the target and ranks the group first;
	// the rest of its roster still fills through nearest-first optional restore.
	void RequestCaptureSeed()
	{
		if (!m_bBudgetActive || !m_bCached || m_bFinished)
			return;
		m_iCaptureSeedMs = System.GetTickCount();
		RequestWake();
	}

	bool HasCapturePriority()
	{
		if (m_iCaptureSeedMs < 0 || !m_bBudgetActive || !m_bCached || m_bFinished)
			return false;
		return System.GetTickCount() - m_iCaptureSeedMs < CaptureSeedWindowMs();
	}

	bool HasCaptureSeedWork()
	{
		if (!HasCapturePriority() || !IsOwnerLive())
			return false;
		return PhysicalAliveCount() == 0;
	}

	protected int CaptureSeedWindowMs()
	{
		int seconds = IA_DynamicAISpawning.GetTuning().m_iDynamicAICaptureSeedSec;
		if (seconds < 1)
			seconds = 1;
		return seconds * 1000;
	}

	protected int PhysicalAliveCount()
	{
		if (!m_Owner)
			return 0;
		return m_Owner.GetDynamicAIPhysicalAliveCount();
	}

	override void CheckWake(array<vector> players, bool enabled)
	{
		if (!m_bBudgetActive)
		{
			super.CheckWake(players, enabled);
			return;
		}
		if (!enabled || m_bExitBudget)
			RequestMandatoryRestore(false);
		CheckProtection(players);
	}

	protected float NearestDistance(vector position, array<vector> players)
	{
		vector unused;
		return NearestPlayer(position, players, unused);
	}

	protected float NearestPlayer(vector position, array<vector> players, out vector nearestPlayer)
	{
		float nearestSq = 100000000000000;
		nearestPlayer = vector.Zero;
		foreach (vector player : players)
		{
			float dx = position[0] - player[0];
			float dz = position[2] - player[2];
			float distanceSq = dx * dx + dz * dz;
			if (distanceSq >= nearestSq)
				continue;
			nearestSq = distanceSq;
			nearestPlayer = player;
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

	// Nearest-first, except a hidden reserve that is almost as close is preferred
	// so the soldier does not appear in a player's face. Never vetoes restore.
	protected bool PreferHiddenReserve(IA_DynamicAIUnit candidate, IA_DynamicAIUnit current)
	{
		if (!candidate || !current)
			return false;
		if (candidate.m_fNearestPlayerM + 80 < current.m_fNearestPlayerM)
			return true;
		if (current.m_fNearestPlayerM + 80 < candidate.m_fNearestPlayerM)
			return false;
		bool candidateHidden = IsHiddenFromNearestPlayer(candidate);
		bool currentHidden = IsHiddenFromNearestPlayer(current);
		if (candidateHidden != currentHidden)
			return candidateHidden;
		return candidate.m_fNearestPlayerM < current.m_fNearestPlayerM;
	}

	protected bool IsHiddenFromNearestPlayer(IA_DynamicAIUnit unit)
	{
		if (!unit || m_vNearestPlayer == vector.Zero)
			return false;
		BaseWorld world;
		if (GetGame())
			world = GetGame().GetWorld();
		if (!world)
			return false;
		ref TraceParam trace = new TraceParam();
		trace.Start = m_vNearestPlayer + Vector(0, 1.6, 0);
		trace.End = CurrentPosition(unit) + Vector(0, 1.0, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		return world.TraceMove(trace, null) < 1.0;
	}

	void CheckProtection(array<vector> players)
	{
		if (!IsOwnerLive() || m_bFinished)
			return;
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		m_fNearest = 10000000;
		m_vNearestPlayer = vector.Zero;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (!unit.IsLogicallyAlive())
				continue;
			vector position = CurrentPosition(unit);
			vector nearestPlayer;
			unit.m_fNearestPlayerM = NearestPlayer(position, players, nearestPlayer);
			if (unit.m_fNearestPlayerM < m_fNearest)
			{
				m_fNearest = unit.m_fNearestPlayerM;
				m_vNearestPlayer = nearestPlayer;
			}
		}
		if (m_fNearest <= tuning.m_iDynamicAICloseDistanceM)
			m_bClose = true;
		else if (m_fNearest > tuning.m_iDynamicAIReleaseDistanceM)
			m_bClose = false;
		// Close and combat keep live soldiers; they must not force-full a cached
		// town over the shared target. Nearest optional restore fills the budget.
	}

	bool HasRecentCombat()
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

	// Close-range keep and min-live still hold a soldier. Combat only holds a
	// squad together while the shared target has room, unless the admin hard cap
	// is on — then distance alone decides who stays.
	bool CombatBlocksEviction()
	{
		if (IA_DynamicAISpawning.GetTuning().m_bDynamicAIHardCap)
			return false;
		if (HasRecentCombat() && !IA_DynamicAISpawning.IsOverTarget())
			return true;
		return false;
	}

	// New native members enter the ledger; transferred, deleted or dead live
	// members leave it permanently. An absent saved reserve is never re-seeded.
	void RefreshRoster(int now)
	{
		if (!m_bBudgetActive || !IsOwnerLive() || m_bFinished)
			return;
		SCR_AIGroup group = m_Owner.GetSCR_AIGroup();
		if (!group)
			return;
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		ref array<IEntity> members = {};
		PlayerManager manager;
		if (GetGame())
			manager = GetGame().GetPlayerManager();
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

	// Seated healthy occupants are occupancy work, not protected budget squatters.
	// CanRemoveUnit is false for them so the per-soldier evict path will not delete a seated pawn.
	protected bool IsSeatedOccupancyCandidate(IA_DynamicAIUnit unit, PlayerManager manager)
	{
		if (!unit || !unit.m_bRestored || unit.m_bDead || !unit.m_Entity || !manager)
			return false;
		SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(unit.m_Entity);
		if (!pawn)
			return false;
		if (manager.GetPlayerIdFromControlledEntity(pawn) > 0)
			return false;
		if (!pawn.GetParent() && !pawn.IsInVehicle())
			return false;
		return IsPawnSafeToVirtualize(pawn, manager, true);
	}

	protected bool OccupancySeatIsBudgetProtected(bool close, bool minLive, bool full, string reason)
	{
		if (full)
			return true;
		if (close)
			return true;
		if (minLive)
			return true;
		if (reason == "initialization")
			return true;
		return false;
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
		// A squad already trading fire outranks an equidistant quiet one, so the
		// shared target funds the engagement the players are actually in.
		if (m_fNearest <= tuning.m_iDynamicAIWakeDistanceM && HasRecentCombat())
			entry.m_iRetentionBiasM = entry.m_iRetentionBiasM + tuning.m_iDynamicAIRetentionBiasM;
		if (HasCapturePriority())
		{
			// Contested objective defenders outrank every other optional squad.
			entry.m_fDistance = 0;
			entry.m_bInRange = true;
		}
		string reason = m_Owner.GetDynamicAIRoleBlockReason();
		bool full = m_bForceFull || (reason != "" && reason != "initialization");
		PlayerManager manager = GetGame().GetPlayerManager();
		int minLiveMs = tuning.m_iDynamicAIMinLiveSec * 1000;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			unit.m_bBudgetProtected = false;
			if (!unit.IsLogicallyAlive())
				continue;
			if (unit.m_bRestored)
				entry.m_iPhysical++;
			bool close = false;
			bool minLive = false;
			if (unit.m_bRestored)
			{
				close = NearestDistance(CurrentPosition(unit), players) <= tuning.m_iDynamicAIReleaseDistanceM;
				minLive = now - unit.m_iBudgetLiveSinceMs < minLiveMs;
			}
			if (IsSeatedOccupancyCandidate(unit, manager))
			{
				unit.m_bBudgetProtected = OccupancySeatIsBudgetProtected(close, minLive, full, reason);
			}
			else
			{
				unit.m_bBudgetProtected = full || unit.m_bBudgetAdmitted;
				if (unit.m_bRestored && (reason == "initialization" || close || !CanRemoveUnit(unit, now, manager)))
					unit.m_bBudgetProtected = true;
			}
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
		if (HasCaptureSeedWork())
			return true;
		return HasAdmittedRetryWork();
	}

	bool HasAdmittedRetryWork()
	{
		if (!m_bCached || m_bFinished)
			return false;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bBudgetAdmitted && !unit.m_bRestored && !unit.m_bDead)
				return true;
		}
		return false;
	}

	bool IsExitingBudget()
	{
		return m_bExitBudget;
	}

	int GetBudgetOrder()
	{
		return m_iOrder;
	}

	float GetNearestPlayerDistance()
	{
		return m_fNearest;
	}

	bool WantsOptionalRestore(int now)
	{
		if (!m_bBudgetActive || !m_bCached || m_bFinished || !IsOwnerLive())
			return false;
		if (m_iPlanAt == 0 || now - m_iPlanAt > 5000 || GetBudgetCost() >= m_iDesired)
			return false;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bRestored || unit.m_bDead || now < unit.m_iNextAttemptMs)
				continue;
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
		bool anyUnit = m_bForceFull || m_bExitBudget || HasCaptureSeedWork();
		// The original leader returns first when nothing is physical; otherwise
		// the reserve closest to a player fills the fight where it is happening.
		bool preferLeader = PhysicalAliveCount() == 0;
		ref IA_DynamicAIUnit chosen;
		ref IA_DynamicAIUnit inView;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bRestored || unit.m_bDead || now < unit.m_iNextAttemptMs)
				continue;
			if (!optional && !anyUnit && !unit.m_bBudgetAdmitted)
				continue;
			if (preferLeader)
			{
				if (!chosen || unit.m_bBudgetLeader)
					chosen = unit;
				if (unit.m_bBudgetLeader)
					break;
				continue;
			}
			// Nearest-first, except that a saved position almost on top of a
			// player is used only when the squad has nothing farther to send.
			if (unit.m_fNearestPlayerM < IA_DynamicAISpawning.POPIN_MIN_M)
			{
				if (!inView || unit.m_fNearestPlayerM < inView.m_fNearestPlayerM)
					inView = unit;
				continue;
			}
			if (!chosen)
			{
				chosen = unit;
				continue;
			}
			if (PreferHiddenReserve(unit, chosen))
				chosen = unit;
		}
		if (!chosen)
			chosen = inView;
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
		// Group-level close no longer vetoes eviction: soldiers inside the release
		// distance are skipped below so farther teammates can free budget slots.
		CheckProtection(players);
		if (m_bForceFull || m_Owner.GetDynamicAIRoleBlockReason() != "")
		{
			m_iEvictSince = -1;
			return false;
		}
		// Recent combat holds a squad together only while the shared target has
		// room. Once it is full, distance decides who stays: soldiers inside the
		// release distance are still skipped below, so only far fighters release.
		if (CombatBlocksEviction())
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
		if (m_Owner.ShouldKeepVehicleOccupantsPhysical())
			return false;
		if (m_bBudgetActive)
		{
			if (m_bClose || m_bForceFull || CombatBlocksEviction() || m_iDesired > 0)
				return false;
			if (!IA_DynamicAIOccupancyHost.LinkedGroupsAllowEvict(m_Owner))
				return false;
			IA_Config tuning = IA_DynamicAISpawning.GetTuning();
			int now = System.GetTickCount();
			if (m_iEvictSince < 0 || now - m_iEvictSince < tuning.m_iDynamicAIEvictDelaySec * 1000)
				return false;
			return true;
		}
		return true;
	}

#ifdef WORKBENCH
	bool OccupancySeatIsBudgetProtectedForTest(bool close, bool minLive, bool full, string reason)
	{
		return OccupancySeatIsBudgetProtected(close, minLive, full, reason);
	}
#endif

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
		m_iCaptureSeedMs = -1;
	}
}
