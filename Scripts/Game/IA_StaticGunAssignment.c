// One-person assignment, ticked by the owning site (never a per-gun call queue).
// Terminal release is idempotent. An accepted GetIn is not another retry.
// After a successful mount, an unexpected hop-off remounts while the site and
// gun remain usable. Capture, death, a player in the seat, a wrecked gun and
// exhausted ammunition still release once.
class IA_StaticGunAssignment
{
	protected static const int INITIAL_ATTEMPTS = 3;
	protected static const int INITIAL_WINDOW_MS = 5000;
	protected static const int REMOUNT_ATTEMPTS = 8;
	protected static const int REMOUNT_RETRY_MS = 2000;
	protected static const int ACCEPTED_WAIT_MS = 4000;

	protected IA_AiGroup m_Group;
	protected IA_StaticGunComponent m_Gun;
	protected AIAgent m_Agent;
	protected IEntity m_Pawn;
	protected TurretCompartmentSlot m_Seat;
	protected int m_iSerial;
	protected int m_iState; // 0 spawn, 1 boarding, 2 mounted, 3 exit, 4 done
	protected int m_iReadyMs;
	protected int m_iNextAttemptMs;
	protected int m_iAttempts;
	protected int m_iEmptySinceMs;
	protected int m_iExitDeadlineMs;
	protected bool m_bAccepted;
	protected bool m_bPinned;
	protected bool m_bWasMounted;
	protected bool m_bRestoreDefense = true;
	protected bool m_bExitRequested;
	protected bool m_bBoardingCancelled;
	protected vector m_vDefend;
	protected float m_fRadius;

	void Setup(IA_AiGroup group, IA_StaticGunComponent gun, int serial, vector defend, float radius)
	{
		m_Group = group;
		m_Gun = gun;
		m_iSerial = serial;
		m_vDefend = defend;
		m_fRadius = radius;
		if (gun)
			m_Seat = gun.GetSeat();
	}

	bool BlocksOrders() { return m_iState < 4; }
	bool InitialPending() { return m_iState < 2 && !m_bWasMounted; }
	bool IsMounted() { return m_iState == 2; }

	void OnSpawnReady()
	{
		if (!m_iReadyMs)
			m_iReadyMs = System.GetTickCount();
	}

	void Tick(bool permitInitialMount, bool siteLive)
	{
		if (!Replication.IsServer() || m_iState == 4)
			return;
		int now = System.GetTickCount();
		if (!m_Group || !m_Group.IsSpawned())
		{
			Release(false);
			return;
		}
		if (m_iState == 3)
		{
			FinishExit(now);
			return;
		}
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!siteLive || !init || init.GetAoActivationSerial() != m_iSerial || !m_Gun || m_Gun.GetSiteSerial() != m_iSerial || !m_Gun.IsUsable())
		{
			Release(siteLive);
			return;
		}
		if (!m_Pawn)
		{
			if (m_Group.GetPendingUnitCount() > 0 || !m_iReadyMs)
				return; // FinalizeStaggeredSpawn owns readiness, including delayed AddAI.
			OnSpawnReady();
			array<AIAgent> agents = {};
			SCR_AIGroup nativeGroup = m_Group.GetSCR_AIGroup();
			if (nativeGroup)
				nativeGroup.GetAgents(agents);
			if (!agents.IsEmpty())
			{
				m_Agent = agents[0];
				if (m_Agent)
					m_Pawn = m_Agent.GetControlledEntity();
			}
		}
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Pawn);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().GetLifeState() != ECharacterLifeState.ALIVE || IsPlayer())
		{
			Release(false);
			return;
		}
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !m_Seat)
		{
			Release(true);
			return;
		}
		if (m_iState == 2)
		{
			if (m_Seat.GetOccupant() == m_Pawn && access.IsInCompartment())
			{
				if (m_Gun.GetLoadedRounds() > 0)
				{
					m_iEmptySinceMs = 0;
					return;
				}
				if (!m_iEmptySinceMs)
					m_iEmptySinceMs = now;
				int allowance = 10000;
				if (m_Gun.GetUsableRounds() > 0 && m_Gun.GetController())
					allowance = Math.Max(10000, Math.Ceil(m_Gun.GetController().GetReloadDuration() * 1000) + 5000);
				if (now - m_iEmptySinceMs >= allowance)
					Release(true);
				return;
			}
			if (access.IsGettingIn() || access.IsGettingOut())
				return;
			if (m_Seat.GetOccupant() && m_Seat.GetOccupant() != m_Pawn)
			{
				Release(true);
				return;
			}
			if (m_Gun.GetUsableRounds() <= 0)
			{
				Release(true);
				return;
			}
			BeginRemount(now);
			return;
		}
		if (!m_bWasMounted)
		{
			if (!permitInitialMount || now - m_iReadyMs >= INITIAL_WINDOW_MS)
			{
				Release(true);
				return;
			}
		}
		if (m_Seat.GetOccupant() == m_Pawn && access.IsInCompartment())
		{
			m_iState = 2;
			m_bWasMounted = true;
			m_iAttempts = 0;
			m_bAccepted = false;
			ClearReservation();
			return;
		}
		if (access.IsGettingOut())
			return;
		if (m_bAccepted)
		{
			if (now < m_iNextAttemptMs + ACCEPTED_WAIT_MS)
				return;
			m_bAccepted = false;
		}
		if (now < m_iNextAttemptMs)
			return;
		int attemptCap = INITIAL_ATTEMPTS;
		if (m_bWasMounted)
			attemptCap = REMOUNT_ATTEMPTS;
		if (m_iAttempts >= attemptCap || access.IsInCompartment())
		{
			Release(true);
			return;
		}
		m_iAttempts++;
		int retryMs = 1000;
		if (m_bWasMounted)
			retryMs = REMOUNT_RETRY_MS;
		m_iNextAttemptMs = now + retryMs;
		if (m_Seat.GetOccupant() || (m_Seat.IsReserved() && !m_Seat.IsReservedBy(m_Pawn)))
			return;
		m_iState = 1;
		m_Seat.SetReserved(m_Pawn);
		if (!m_bPinned)
		{
			if (m_Agent.GetLOD() == AIAgent.GetMaxLOD())
				m_Agent.SetLOD(Math.Max(0, AIAgent.GetMaxLOD() - 1));
			m_Agent.PreventMaxLOD();
			m_bPinned = true;
		}
		m_bAccepted = access.GetInVehicle(m_Seat.GetOwner(), m_Seat, true, -1, ECloseDoorAfterActions.INVALID, false);
		if (!m_bAccepted)
			ClearReservation();
	}

	protected void BeginRemount(int now)
	{
		m_iState = 1;
		m_bAccepted = false;
		m_bBoardingCancelled = false;
		m_iAttempts = 0;
		m_iEmptySinceMs = 0;
		m_iNextAttemptMs = now + REMOUNT_RETRY_MS;
		ClearReservation();
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Emplacements] remount serial=%1", m_iSerial), LogLevel.NORMAL);
		}
	}

	void Release(bool restoreDefense)
	{
		if (!Replication.IsServer() || m_iState == 4)
			return;
		if (!restoreDefense)
			m_bRestoreDefense = false;
		ClearReservation();
		if (m_bPinned && m_Agent)
			m_Agent.AllowMaxLOD();
		m_bPinned = false;
		if (m_iState != 3)
		{
			m_iState = 3;
			m_iExitDeadlineMs = System.GetTickCount() + 5000;
		}
		FinishExit(System.GetTickCount());
	}

	protected bool IsPlayer()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		return m_Pawn && (!pm || pm.GetPlayerIdFromControlledEntity(m_Pawn) > 0);
	}

	protected void ClearReservation()
	{
		if (m_Seat && m_Pawn && m_Seat.IsReservedBy(m_Pawn))
			m_Seat.SetReserved(null);
	}

	protected void FinishExit(int now)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Pawn);
		if (character && !IsPlayer())
		{
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (access && access.GetCompartment() && access.GetCompartment() != m_Seat)
			{
				// A different assignment now owns this soldier. Do not eject it
				// from an unrelated vehicle or issue conflicting infantry orders.
				m_bRestoreDefense = false;
				m_iState = 4;
				return;
			}
			if (access && m_bAccepted && !m_bBoardingCancelled && !access.IsInCompartment() && !access.IsGettingOut())
			{
				// Cancel a stale INITIAL entry, never interrupt an exit. Otherwise
				// a previously accepted native request could mount after capture.
				access.InterruptVehicleActionQueue(true, true, true);
				m_bBoardingCancelled = true;
			}
			if (access && (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()))
			{
				if (!m_bExitRequested && !access.IsGettingIn() && !access.IsGettingOut())
					m_bExitRequested = access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.INVALID, false);
				if (now < m_iExitDeadlineMs)
					return;
				// Never force a pawn pose or repeatedly teleport a failed exit.
				// Normal host cleanup owns the remaining AI; do not remount it.
				m_bRestoreDefense = false;
			}
		}
		m_iState = 4;
		if (m_bRestoreDefense && m_Group && m_Group.IsSpawned() && character && !IsPlayer() && character.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE)
			m_Group.RestoreStaticGunInfantry(m_vDefend, m_fRadius);
		m_Gun = null;
		m_Agent = null;
		m_Pawn = null;
		m_Seat = null;
	}
}
