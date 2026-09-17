// One compartment host (vehicle hull, static gun, or mortar) whose occupants
// cache together. Driven as one logical transaction; each engine call is one
// worker operation. Abort remounts ejected survivors. Never delete a seated pawn.
class IA_DynamicAIOccupancyHost
{
	static const float STATIONARY_SPEED_M_S = 0.5;
	static const int STATIONARY_HOLD_MS = 2000;
	static const int QUIESCE_MS = 400;

	protected static ref map<string, ref IA_DynamicAIOccupancyHost> s_Hosts;

	protected RplId m_HostId;
	protected IEntity m_Host;
	protected IA_DynamicAIOccupancyState m_eState;
	protected int m_iStateSinceMs;
	protected int m_iStationarySinceMs;
	protected int m_iEjectIndex;
	protected int m_iCommitIndex;
	protected int m_iAbortIndex;
	protected bool m_bOrdersStripped;
	protected bool m_bPostEjectQuiesce;
	protected ref array<IA_DynamicAIGroupCache> m_aCaches = {};
	protected ref array<ref IA_DynamicAIOccupancySeat> m_aSeats = {};

	static void Reset()
	{
		if (s_Hosts)
			s_Hosts.Clear();
	}

	static string HostKey(RplId hostId)
	{
		if (!hostId.IsValid())
			return "";
		return hostId.AsString();
	}

	static IA_DynamicAIOccupancyHost Find(RplId hostId)
	{
		string key = HostKey(hostId);
		if (key == "" || !s_Hosts)
			return null;
		return s_Hosts.Get(key);
	}

	static bool IsCacheParticipating(IA_DynamicAIGroupCache cache)
	{
		if (!cache || !s_Hosts)
			return false;
		foreach (string key, IA_DynamicAIOccupancyHost host : s_Hosts)
		{
			if (host && host.ContainsCache(cache) && host.m_eState != IA_DynamicAIOccupancyState.Idle)
				return true;
		}
		return false;
	}

	static void AbortForCache(IA_DynamicAIGroupCache cache)
	{
		if (!cache || !s_Hosts)
			return;
		foreach (string key, IA_DynamicAIOccupancyHost host : s_Hosts)
		{
			if (host && host.ContainsCache(cache))
				host.BeginAbort();
		}
	}

	static bool LinkedGroupsAllowEvict(IA_AiGroup owner)
	{
		if (!owner)
			return false;
		if (!CacheAllowsEvict(owner.GetDynamicAICache()))
			return false;
		IA_AiGroup linkedPassenger = owner.GetLinkedPassengerGroup();
		if (linkedPassenger && !CacheAllowsEvict(linkedPassenger.GetDynamicAICache()))
			return false;
		IA_AiGroup linkedCrew = owner.GetLinkedCrewGroup();
		if (linkedCrew && !CacheAllowsEvict(linkedCrew.GetDynamicAICache()))
			return false;
		return true;
	}

	protected static bool CacheAllowsEvict(IA_DynamicAIGroupCache cache)
	{
		if (!cache)
			return true;
		IA_DynamicAIBudgetCache budget = IA_DynamicAIBudgetCache.Cast(cache);
		if (!budget || !budget.IsBudgetActive())
			return true;
		return budget.GetDesired() <= 0;
	}

	static void Request(IA_AiGroup owner, array<vector> players, int now)
	{
		if (!owner || !Replication.IsServer())
			return;
		if (owner.ShouldKeepVehicleOccupantsPhysical())
			return;
		IEntity host = owner.GetOccupancyHostEntity();
		if (!host)
			return;
		RplId hostId = Replication.FindItemId(host);
		if (!hostId.IsValid())
			return;
		string key = HostKey(hostId);
		if (key == "")
			return;
		if (!s_Hosts)
			s_Hosts = new map<string, ref IA_DynamicAIOccupancyHost>();
		ref IA_DynamicAIOccupancyHost existing = s_Hosts.Get(key);
		if (existing)
		{
			existing.AddCache(owner.GetDynamicAICache());
			return;
		}
		ref IA_DynamicAIOccupancyHost created = new IA_DynamicAIOccupancyHost();
		created.m_HostId = hostId;
		created.m_Host = host;
		created.m_eState = IA_DynamicAIOccupancyState.Qualify;
		created.m_iStateSinceMs = now;
		created.m_iStationarySinceMs = now;
		created.AddCache(owner.GetDynamicAICache());
		IA_AiGroup passenger = owner.GetLinkedPassengerGroup();
		if (passenger && passenger.GetDynamicAICache())
			created.AddCache(passenger.GetDynamicAICache());
		IA_AiGroup crew = owner.GetLinkedCrewGroup();
		if (crew && crew.GetDynamicAICache())
			created.AddCache(crew.GetDynamicAICache());
		s_Hosts.Set(key, created);
	}

	static int TickAll(array<vector> players, int now, int maxOps, int startedMs)
	{
		if (!s_Hosts || s_Hosts.IsEmpty() || maxOps <= 0)
			return 0;
		int operations;
		ref array<string> keys = {};
		foreach (string key, IA_DynamicAIOccupancyHost listed : s_Hosts)
		{
			if (key != "")
				keys.Insert(key);
		}
		foreach (string tickKey : keys)
		{
			if (operations >= maxOps)
				break;
			if (System.GetTickCount() - startedMs >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			ref IA_DynamicAIOccupancyHost ticking = s_Hosts.Get(tickKey);
			if (!ticking)
				continue;
			if (ticking.m_eState == IA_DynamicAIOccupancyState.Idle)
			{
				s_Hosts.Remove(tickKey);
				continue;
			}
			int used = ticking.Step(players, now);
			if (used > 0)
				operations = operations + used;
			if (ticking.m_eState == IA_DynamicAIOccupancyState.Idle)
				s_Hosts.Remove(tickKey);
		}
		return operations;
	}

	static int RemountUnit(IA_DynamicAIUnit unit)
	{
		if (!unit || !unit.HasOccupancy() || !unit.m_Entity)
			return 0;
		Managed found = Replication.FindItem(unit.m_HostId);
		IEntity host = IEntity.Cast(found);
		if (!host)
			return 0;
		BaseCompartmentSlot slot = IA_VehicleManager.FindCompartmentSlot(host, unit.m_iOccupancyMgrId, unit.m_iOccupancySlotId);
		if (!slot)
			return 0;
		IEntity occupant = slot.GetOccupant();
		if (occupant && occupant != unit.m_Entity)
		{
			PlayerManager manager = GetGame().GetPlayerManager();
			if (manager && manager.GetPlayerIdFromControlledEntity(occupant) > 0)
				return 0;
			return 0;
		}
		if (occupant == unit.m_Entity)
			return 2;
		IEntity vehicleEnt = slot.GetVehicle();
		if (!vehicleEnt)
			vehicleEnt = host;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(unit.m_Entity.FindComponent(CompartmentAccessComponent));
		if (!access)
			return 0;
		bool boarded = access.GetInVehicle(vehicleEnt, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
		if (!boarded)
			return 1;
		return 2;
	}

	bool ContainsCache(IA_DynamicAIGroupCache cache)
	{
		return cache && m_aCaches.Contains(cache);
	}

	protected void AddCache(IA_DynamicAIGroupCache cache)
	{
		if (!cache || m_aCaches.Contains(cache))
			return;
		m_aCaches.Insert(cache);
	}

	protected int Step(array<vector> players, int now)
	{
		if (m_eState == IA_DynamicAIOccupancyState.Qualify)
			return StepQualify(players, now);
		if (m_eState == IA_DynamicAIOccupancyState.Quiesce)
			return StepQuiesce(players, now);
		if (m_eState == IA_DynamicAIOccupancyState.Eject)
			return StepEject(players, now);
		if (m_eState == IA_DynamicAIOccupancyState.Commit)
			return StepCommit(players, now);
		if (m_eState == IA_DynamicAIOccupancyState.Abort)
			return StepAbort(now);
		return 0;
	}

	protected int StepQualify(array<vector> players, int now)
	{
		if (!RefreshHost())
		{
			FinishIdle();
			return 0;
		}
		if (!IsHostStationary())
		{
			m_iStationarySinceMs = now;
			return 0;
		}
		if (now - m_iStationarySinceMs < STATIONARY_HOLD_MS)
			return 0;
		string veto = CollectSeats(players, now);
		if (veto != "")
		{
			FinishIdle();
			return 0;
		}
		if (m_aSeats.IsEmpty())
		{
			FinishIdle();
			return 0;
		}
		m_eState = IA_DynamicAIOccupancyState.Quiesce;
		m_iStateSinceMs = now;
		m_bOrdersStripped = false;
		return 0;
	}

	protected int StepQuiesce(array<vector> players, int now)
	{
		if (!m_bOrdersStripped)
		{
			foreach (IA_DynamicAIGroupCache cache : m_aCaches)
			{
				if (cache && cache.GetOwner())
					cache.GetOwner().RemoveAllOrders(false);
			}
			m_bOrdersStripped = true;
			m_iStateSinceMs = now;
			return 1;
		}
		if (now - m_iStateSinceMs < GetQuiesceMs())
			return 0;
		string veto = CollectSeats(players, now);
		if (veto != "")
		{
			BeginAbort();
			return 0;
		}
		foreach (IA_DynamicAIGroupCache cache : m_aCaches)
		{
			if (cache && cache.GetOwner() && cache.GetOwner().HasPendingCompartmentTree())
			{
				BeginAbort();
				return 0;
			}
		}
		m_eState = IA_DynamicAIOccupancyState.Eject;
		m_iStateSinceMs = now;
		m_iEjectIndex = 0;
		m_bPostEjectQuiesce = false;
		SuspendAssignments();
		return 0;
	}

	protected int StepEject(array<vector> players, int now)
	{
		string veto = QualifyLive(players, now);
		if (veto != "")
		{
			BeginAbort();
			return 0;
		}
		if (m_iEjectIndex >= m_aSeats.Count())
		{
			if (!m_bPostEjectQuiesce)
			{
				m_bPostEjectQuiesce = true;
				m_iStateSinceMs = now;
				return 0;
			}
			if (now - m_iStateSinceMs < GetQuiesceMs())
				return 0;
			foreach (IA_DynamicAIGroupCache cache : m_aCaches)
			{
				if (cache && cache.GetOwner() && cache.GetOwner().HasPendingCompartmentTree())
				{
					BeginAbort();
					return 0;
				}
			}
			m_eState = IA_DynamicAIOccupancyState.Commit;
			m_iCommitIndex = 0;
			m_iStateSinceMs = now;
			return 0;
		}
		ref IA_DynamicAIOccupancySeat seat = m_aSeats[m_iEjectIndex];
		if (!seat || !seat.m_Pawn)
		{
			m_iEjectIndex = m_iEjectIndex + 1;
			return 0;
		}
		if (seat.m_bEjected)
		{
			if (IsPawnSeated(seat.m_Pawn))
			{
				EjectPawn(seat.m_Pawn);
				return 1;
			}
			seat.m_Pawn.GetWorldTransform(seat.m_aTransform);
			m_iEjectIndex = m_iEjectIndex + 1;
			return 0;
		}
		EjectPawn(seat.m_Pawn);
		seat.m_bEjected = true;
		return 1;
	}

	protected int StepCommit(array<vector> players, int now)
	{
		string veto = QualifyLive(players, now);
		if (veto != "")
		{
			BeginAbort();
			return 0;
		}
		if (m_iCommitIndex >= m_aSeats.Count())
		{
			FinishIdle();
			return 0;
		}
		ref IA_DynamicAIOccupancySeat seat = m_aSeats[m_iCommitIndex];
		m_iCommitIndex = m_iCommitIndex + 1;
		if (!seat || !seat.m_Pawn || !seat.m_Cache)
			return 0;
		if (IsPawnSeated(seat.m_Pawn))
		{
			BeginAbort();
			return 0;
		}
		if (!seat.m_Cache.CacheOccupancyPawn(seat))
		{
			BeginAbort();
			return 1;
		}
		seat.m_bDeleted = true;
		seat.m_Pawn = null;
		return 1;
	}

	protected int StepAbort(int now)
	{
		if (m_iAbortIndex >= m_aSeats.Count())
		{
			ResumeAssignments();
			FinishIdle();
			return 0;
		}
		ref IA_DynamicAIOccupancySeat seat = m_aSeats[m_iAbortIndex];
		m_iAbortIndex = m_iAbortIndex + 1;
		if (!seat || seat.m_bDeleted || !seat.m_Pawn || seat.m_bRemounted)
			return 0;
		if (!seat.m_bEjected)
			return 0;
		if (RemountPawn(seat))
		{
			seat.m_bRemounted = true;
			return 1;
		}
		return 1;
	}

	protected void BeginAbort()
	{
		if (m_eState == IA_DynamicAIOccupancyState.Abort || m_eState == IA_DynamicAIOccupancyState.Idle)
			return;
		m_eState = IA_DynamicAIOccupancyState.Abort;
		m_iAbortIndex = 0;
		m_iStateSinceMs = System.GetTickCount();
	}

	protected void FinishIdle()
	{
		m_eState = IA_DynamicAIOccupancyState.Idle;
		m_bPostEjectQuiesce = false;
		m_bOrdersStripped = false;
		m_aSeats.Clear();
	}

	protected bool RefreshHost()
	{
		if (m_Host && !m_Host.IsDeleted())
			return true;
		Managed found = Replication.FindItem(m_HostId);
		m_Host = IEntity.Cast(found);
		return m_Host != null;
	}

	protected bool IsHostStationary()
	{
		if (!m_Host)
			return false;
		Physics physics = m_Host.GetPhysics();
		if (!physics)
			return true;
		vector velocity = physics.GetVelocity();
		return velocity.Length() < STATIONARY_SPEED_M_S;
	}

	protected string CollectSeats(array<vector> players, int now)
	{
		m_aSeats.Clear();
		if (!RefreshHost())
			return "missing host";
		if (!IsHostStationary())
			return "moving";
		ref array<BaseCompartmentSlot> occupied = new array<BaseCompartmentSlot>();
		IA_VehicleManager.CollectOccupiedCompartmentSlots(m_Host, occupied);
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return "missing players";
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		foreach (BaseCompartmentSlot slot : occupied)
		{
			if (!slot)
				continue;
			IEntity pawnEnt = slot.GetOccupant();
			if (!pawnEnt)
				continue;
			if (manager.GetPlayerIdFromControlledEntity(pawnEnt) > 0)
				return "player occupant";
			IA_DynamicAIGroupCache cache = IA_DynamicAISpawning.FindCacheForPawn(pawnEnt);
			if (!cache || !cache.IsOwnerLive() || cache.IsFinished())
				return "unregistered occupant";
			IA_AiGroup owner = cache.GetOwner();
			if (!owner)
				return "unregistered occupant";
			string role = owner.GetDynamicAIRoleBlockReason();
			if (role != "")
				return role;
			if (owner.HasPendingCompartmentTree())
				return "boarding";
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pawnEnt);
			if (!character || !cache.IsPawnSafeToVirtualize(character, manager, true))
				return "injured occupant";
			vector transform[4];
			pawnEnt.GetWorldTransform(transform);
			if (IA_SpawnPlacement.IsNearAnyPlayer(transform[3], players, tuning.m_iDynamicAICacheDistanceM))
				return "player nearby";
			if (!m_aCaches.Contains(cache))
				m_aCaches.Insert(cache);
			ref IA_DynamicAIOccupancySeat seat = new IA_DynamicAIOccupancySeat();
			seat.m_Pawn = pawnEnt;
			seat.m_Cache = cache;
			seat.m_HostId = m_HostId;
			seat.m_iMgrId = slot.GetCompartmentMgrID();
			seat.m_iSlotId = slot.GetCompartmentSlotID();
			seat.m_eKind = ResolveKind(owner, slot);
			pawnEnt.GetWorldTransform(seat.m_aTransform);
			m_aSeats.Insert(seat);
		}
		foreach (IA_DynamicAIGroupCache cache : m_aCaches)
		{
			if (!cache || !cache.IsOwnerLive())
				return "retired participant";
			if (cache.GetOwner() && cache.GetOwner().HasPendingCompartmentTree())
				return "boarding";
			IA_DynamicAIBudgetCache budget = IA_DynamicAIBudgetCache.Cast(cache);
			if (budget && budget.IsBudgetActive() && budget.GetDesired() > 0)
				return "allocation";
		}
		return "";
	}

	protected string QualifyLive(array<vector> players, int now)
	{
		if (!RefreshHost() || !IsHostStationary())
			return "moving";
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return "missing players";
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		foreach (IA_DynamicAIOccupancySeat seat : m_aSeats)
		{
			if (!seat || seat.m_bDeleted)
				continue;
			if (!seat.m_Cache || !seat.m_Cache.IsOwnerLive() || seat.m_Cache.IsFinished())
				return "retired participant";
			if (seat.m_Pawn && manager.GetPlayerIdFromControlledEntity(seat.m_Pawn) > 0)
				return "player occupant";
			if (seat.m_Pawn && IA_SpawnPlacement.IsNearAnyPlayer(seat.m_aTransform[3], players, tuning.m_iDynamicAIReleaseDistanceM))
				return "player nearby";
			IA_DynamicAIBudgetCache budget = IA_DynamicAIBudgetCache.Cast(seat.m_Cache);
			if (budget && budget.IsBudgetActive() && budget.GetDesired() > 0)
				return "allocation";
		}
		return "";
	}

	protected IA_DynamicAIOccupancyKind ResolveKind(IA_AiGroup owner, BaseCompartmentSlot slot)
	{
		if (owner && owner.IsMortarCrew())
			return IA_DynamicAIOccupancyKind.Mortar;
		if (owner && owner.HasStaticGunAssignment())
			return IA_DynamicAIOccupancyKind.Gun;
		if (slot && TurretCompartmentSlot.Cast(slot) && owner && owner.IsMortarCrew())
			return IA_DynamicAIOccupancyKind.Mortar;
		return IA_DynamicAIOccupancyKind.Vehicle;
	}

	protected void SuspendAssignments()
	{
		foreach (IA_DynamicAIGroupCache cache : m_aCaches)
		{
			if (cache && cache.GetOwner())
				cache.GetOwner().SuspendOccupancyAssignment();
		}
	}

	protected void ResumeAssignments()
	{
		foreach (IA_DynamicAIGroupCache cache : m_aCaches)
		{
			if (cache && cache.GetOwner())
				cache.GetOwner().ResumeOccupancyAssignment();
		}
	}

	protected bool IsPawnSeated(IEntity pawn)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(pawn);
		if (!character)
			return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return character.IsInVehicle();
		if (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
			return true;
		return character.IsInVehicle();
	}

	protected bool EjectPawn(IEntity pawn)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(pawn);
		if (!character)
			return false;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;
		bool left = false;
		if (access.CanGetOutVehicleViaDoor(-1))
			left = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);
		if (!left)
		{
			vector mat[4];
			character.GetWorldTransform(mat);
			left = access.GetOutVehicle_NoDoor(mat, false, false);
		}
		return left;
	}

	protected bool RemountPawn(IA_DynamicAIOccupancySeat seat)
	{
		if (!seat || !seat.m_Pawn)
			return false;
		if (!RefreshHost())
			return false;
		BaseCompartmentSlot slot = IA_VehicleManager.FindCompartmentSlot(m_Host, seat.m_iMgrId, seat.m_iSlotId);
		if (!slot)
			return false;
		IEntity occupant = slot.GetOccupant();
		if (occupant == seat.m_Pawn)
			return true;
		if (occupant)
			return false;
		IEntity vehicleEnt = slot.GetVehicle();
		if (!vehicleEnt)
			vehicleEnt = m_Host;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(seat.m_Pawn.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;
		return access.GetInVehicle(vehicleEnt, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
	}

	protected int GetQuiesceMs()
	{
		return QUIESCE_MS;
	}

#ifdef WORKBENCH
	static bool QualifyAllowedForTest(bool moving, bool playerOccupant, bool injured, bool close, bool ineligible, bool boarding)
	{
		if (moving || playerOccupant || injured || close || ineligible || boarding)
			return false;
		return true;
	}

	static bool LinkedAllocationAllowsEvictForTest(int crewDesired, int passengerDesired)
	{
		if (crewDesired > 0)
			return false;
		if (passengerDesired > 0)
			return false;
		return true;
	}

	static int RemainingOccupancyOpsForTest(int lastOperations, int cap)
	{
		int remaining = cap - lastOperations;
		if (remaining < 0)
			return 0;
		return remaining;
	}

	IA_DynamicAIOccupancyState GetStateForTest()
	{
		return m_eState;
	}

	int GetSeatCountForTest()
	{
		return m_aSeats.Count();
	}

	int GetDeletedCountForTest()
	{
		int count;
		foreach (IA_DynamicAIOccupancySeat seat : m_aSeats)
		{
			if (seat && seat.m_bDeleted)
				count++;
		}
		return count;
	}

	int GetRemountedCountForTest()
	{
		int count;
		foreach (IA_DynamicAIOccupancySeat seat : m_aSeats)
		{
			if (seat && seat.m_bRemounted)
				count++;
		}
		return count;
	}

	void SeedCachesForTest(IA_DynamicAIGroupCache first, IA_DynamicAIGroupCache second)
	{
		m_aCaches.Clear();
		if (first)
			m_aCaches.Insert(first);
		if (second)
			m_aCaches.Insert(second);
	}

	void SeedSeatForTest(IA_DynamicAIGroupCache cache, int mgrId, int slotId, bool ejected, bool deleted)
	{
		ref IA_DynamicAIOccupancySeat seat = new IA_DynamicAIOccupancySeat();
		seat.m_Cache = cache;
		seat.m_iMgrId = mgrId;
		seat.m_iSlotId = slotId;
		seat.m_eKind = IA_DynamicAIOccupancyKind.Vehicle;
		seat.m_bEjected = ejected;
		seat.m_bDeleted = deleted;
		seat.m_aTransform[3] = "10 0 10";
		m_aSeats.Insert(seat);
	}

	void SetStateForTest(IA_DynamicAIOccupancyState state)
	{
		m_eState = state;
		if (state == IA_DynamicAIOccupancyState.Abort)
			m_iAbortIndex = 0;
		if (state == IA_DynamicAIOccupancyState.Commit)
			m_iCommitIndex = 0;
		if (state == IA_DynamicAIOccupancyState.Eject)
			m_iEjectIndex = 0;
	}

	void BeginAbortForTest()
	{
		BeginAbort();
	}

	bool StepAbortBookkeepingForTest()
	{
		int before = m_iAbortIndex;
		foreach (IA_DynamicAIOccupancySeat seat : m_aSeats)
		{
			if (!seat || seat.m_bDeleted)
				continue;
			if (seat.m_bEjected && !seat.m_bRemounted)
				seat.m_bRemounted = true;
		}
		m_eState = IA_DynamicAIOccupancyState.Idle;
		return m_iAbortIndex == before;
	}

	void MarkDeletedForTest(int index)
	{
		if (index < 0 || index >= m_aSeats.Count())
			return;
		if (m_aSeats[index])
			m_aSeats[index].m_bDeleted = true;
	}
#endif
}
