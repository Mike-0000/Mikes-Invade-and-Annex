// One-person assignment, ticked by the owning site (never a per-gun call queue).
// Terminal release is idempotent. An accepted GetIn is not another retry.
class IA_StaticGunAssignment
{
	protected IA_AiGroup m_Group;
	protected IA_StaticGunComponent m_Gun;
	protected AIAgent m_Agent;
	protected IEntity m_Pawn;
	protected TurretCompartmentSlot m_Seat;
	protected int m_iSerial;
	protected int m_iState; // 0 spawn, 1 initial boarding, 2 mounted, 3 exit, 4 done
	protected int m_iReadyMs;
	protected int m_iNextAttemptMs;
	protected int m_iAttempts;
	protected int m_iEmptySinceMs;
	protected int m_iExitDeadlineMs;
	protected bool m_bAccepted;
	protected bool m_bPinned;
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
	bool InitialPending() { return m_iState < 2; }
	bool IsMounted() { return m_iState == 2; }

	void OnSpawnReady()
	{
		if (!m_iReadyMs)
			m_iReadyMs = System.GetTickCount();
	}

	bool ShouldDeferGroupDespawn()
	{
		if (m_iState == 4)
			return false;
		if (IsPlayer())
			return true;
		return IsPawnCompartmentBound();
	}

	void Tick(bool permitInitialMount, bool siteLive)
	{
		if (!Replication.IsServer() || m_iState == 4)
			return;
		int now = System.GetTickCount();
		if (m_iState == 3)
		{
			FinishExit(now);
			return;
		}
		if (!m_Group || !m_Group.IsSpawned())
		{
			Release(false);
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
			if (m_Seat.GetOccupant() != m_Pawn || !access.IsInCompartment())
			{
				Release(true);
				return;
			}
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
		if (!permitInitialMount || now - m_iReadyMs >= 5000)
		{
			Release(true);
			return;
		}
		if (m_Seat.GetOccupant() == m_Pawn && access.IsInCompartment())
		{
			m_iState = 2;
			ClearReservation();
			return;
		}
		if (m_bAccepted || now < m_iNextAttemptMs)
			return;
		if (m_iAttempts >= 3 || access.IsInCompartment() || access.IsGettingOut())
		{
			Release(true);
			return;
		}
		m_iAttempts++;
		m_iNextAttemptMs = now + 1000;
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

	protected bool IsCompartmentBound(CompartmentAccessComponent access)
	{
		if (!access)
			return false;
		if (access.IsInCompartment())
			return true;
		if (access.IsGettingIn())
			return true;
		if (access.IsGettingOut())
			return true;
		return false;
	}

	protected bool IsPawnCompartmentBound()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Pawn);
		if (!character)
			return false;
		return IsCompartmentBound(character.GetCompartmentAccessComponent());
	}

	// Same sequence as IA_AiGroup.ForceEjectSeatedMembers: ANIMATED GetOut from a
	// sandbag nest often never leaves the turret.
	protected bool RequestAiEject(CompartmentAccessComponent access, ChimeraCharacter character)
	{
		if (!access || !character)
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
			if (IsCompartmentBound(access))
			{
				if (access.IsGettingIn() || access.IsGettingOut())
					return;
				if (now < m_iExitDeadlineMs)
				{
					if (!m_bExitRequested)
						m_bExitRequested = access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.INVALID, false);
					if (IsCompartmentBound(access))
						return;
				}
				else
				{
					RequestAiEject(access, character);
					if (IsCompartmentBound(access))
						return;
				}
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
