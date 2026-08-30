//------------------------------------------------------------------------------------------------
//! One optional Enhanced Defense mini-objective. Server map markers are placed
//! immediately (not queued behind the main hold) so players can react during PREPARE / PROBE.
//------------------------------------------------------------------------------------------------
enum IA_DefendEventType
{
	CommanderFob,
	ElitePatrol,
	ScoutMortar,
	MortarTeam,
	Convoy,
	Relay,
	Sniper
}

enum IA_DefendEventState
{
	Pending,
	Active,
	Succeeded,
	Ignored,
	Cancelled
}

class IA_DefendEvent
{
	protected static const int SCOUT_DWELL_MS = 180000;
	protected static const int SCOUT_FALLBACK_MS = 900000;
	protected static const int MORTAR_REVEAL_MS = 180000;
	protected static const int MORTAR_EXACT_MS = 360000;
	protected static const int MORTAR_FIRST_BARRAGE_MS = 240000;
	protected static const int RELAY_TIMEOUT_MS = 480000;
	protected static const int PRIORITY_WARN2_MS = 240000;
	protected static const int PRIORITY_WARN1_MS = 120000;
	protected static const ResourceName FOB_TENT_HQ = "{1D2887BB9A7D4670}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et";
	protected static const ResourceName FOB_TENT_BARRACKS = "{607E00F1C367D129}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et";
	protected static const ResourceName FOB_ANTENNA = "{55B73CF1EE914E07}Prefabs/Props/Military/Compositions/USSR/Antenna_02_USSR.et";
	protected static const ResourceName FOB_SANDBAGS = "{8E1DF47DD56E69E6}Prefabs/Compositions/Slotted/SlotFlatSmall/SandbagPosition_S_USSR_01.et";

	protected IA_EnhancedDefendDirector m_Director;
	protected IA_DefendEventType m_eType;
	protected IA_DefendEventState m_eState;
	protected string m_sTitle;
	protected string m_sDesc;
	protected vector m_vSite;
	protected vector m_vApprox;
	protected int m_iSpawnTick;
	protected int m_iReconMs;
	protected bool m_bReconDone;
	protected bool m_bMortarRevealed;
	protected bool m_bPriority;
	protected bool m_bDemoted;
	protected bool m_bWarned2;
	protected bool m_bWarned1;
	protected int m_iDeadlineMs;
	protected int m_iLastMarkerTick;
	protected bool m_bExactMarker;
	protected bool m_bEngaged;
	protected bool m_bUnitsSeen;
	protected int m_iUnitsSeenTick;
	protected int m_iObsIndex;
	protected int m_iDwellStart;
	protected bool m_bDwelling;
	protected bool m_bScoutOrdered;
	protected ref array<vector> m_ObsPoints;
	protected ref array<ref IA_AiGroup> m_Groups;
	protected ref array<IEntity> m_FobEntities;
	protected ref SCR_MapMarkerBase m_MapMarker;
	protected vector m_vMarkerPos;
	protected IA_AiGroup m_Primary;

	//------------------------------------------------------------------------------------------------
	void IA_DefendEvent(IA_DefendEventType eventType)
	{
		m_eType = eventType;
		m_eState = IA_DefendEventState.Pending;
		m_Groups = new array<ref IA_AiGroup>();
		m_ObsPoints = new array<vector>();
		m_FobEntities = new array<IEntity>();
		m_sTitle = ResolveTitle();
		m_sDesc = ResolveDesc();
		m_iReconMs = SCOUT_FALLBACK_MS;
	}

	//------------------------------------------------------------------------------------------------
	static IA_DefendEvent Create(IA_DefendEventType eventType)
	{
		return new IA_DefendEvent(eventType);
	}

	IA_DefendEventType GetType() { return m_eType; }
	IA_DefendEventState GetState() { return m_eState; }
	string GetTitle() { return m_sTitle; }
	vector GetSite() { return m_vSite; }
	bool IsPriority() { return m_bPriority && !m_bDemoted; }
	bool IsOpen()
	{
		if (m_eState == IA_DefendEventState.Pending)
			return true;
		if (m_eState == IA_DefendEventState.Active)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void SetPriorityDeadline(int deadlineMs)
	{
		m_bPriority = true;
		m_iDeadlineMs = deadlineMs;
		m_sTitle = "HIGH PRIORITY: " + ResolveTitle();
		m_sDesc = ResolveDesc() + " Complete before the deadline to shorten the hold.";
	}

	//------------------------------------------------------------------------------------------------
	void Activate(notnull IA_EnhancedDefendDirector director)
	{
		if (m_eState != IA_DefendEventState.Pending)
			return;

		m_Director = director;
		m_iSpawnTick = System.GetTickCount();
		m_eState = IA_DefendEventState.Active;

		vector center = director.GetDefendPoint();
		m_vSite = PickSite(center);
		if (m_vSite == vector.Zero)
			m_vSite = IA_SpawnPlacement.FindEventSite(center, 350, 650);
		if (m_vSite == vector.Zero)
			m_vSite = center + Vector(400, 0, 0);

		m_vApprox = Jitter(m_vSite, 80, 150);
		if (m_eType == IA_DefendEventType.ScoutMortar)
			BuildScoutOps(center);

		SpawnContent();

		vector pin = m_vApprox;
		if (m_eType == IA_DefendEventType.ScoutMortar)
			pin = m_vSite;
		else if (m_eType == IA_DefendEventType.CommanderFob)
			pin = m_vSite;
		else if (m_eType == IA_DefendEventType.Relay)
			pin = m_vSite;
		else if (m_eType == IA_DefendEventType.Convoy)
			pin = m_vSite;
		SpawnMapMarker(pin);
		director.Notify("TaskCreated", m_sTitle);
	}

	//------------------------------------------------------------------------------------------------
	void Update()
	{
		if (m_eState != IA_DefendEventState.Active)
			return;
		if (!m_Director)
			return;

		int now = System.GetTickCount();
		RefreshEngaged();
		TickType(now);

		if (m_eState != IA_DefendEventState.Active)
			return;

		if (m_bPriority && !m_bDemoted)
			TickPriority(now);

		UpdateMarker(now);
	}

	//------------------------------------------------------------------------------------------------
	void Cancel()
	{
		if (!IsOpen())
			return;
		m_eState = IA_DefendEventState.Cancelled;
		CompleteTask(false);
	}

	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		RemoveMapMarker();
		DespawnFob();

		int count = m_Groups.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup grp = m_Groups[i];
			if (!grp)
				continue;
			if (grp.GetAliveCount() <= 0)
				continue;
			grp.RemoveAllOrders();
		}
		m_Groups.Clear();
		m_Primary = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Succeed(string note)
	{
		if (m_eState != IA_DefendEventState.Active)
			return;
		m_eState = IA_DefendEventState.Succeeded;
		CompleteTask(true);
		if (m_Director)
		{
			m_Director.Notify("TaskCompleted", m_sTitle);
			m_Director.OnEventResolved(this, true, note);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Ignore(string note)
	{
		if (m_eState != IA_DefendEventState.Active)
			return;
		m_eState = IA_DefendEventState.Ignored;
		RemoveMapMarker();
		if (m_Director)
		{
			m_Director.Notify("ReinforcementsCalled", note);
			m_Director.OnEventResolved(this, false, note);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickPriority(int now)
	{
		int elapsed = now - m_iSpawnTick;
		int remain = m_iDeadlineMs - elapsed;
		if (remain <= PRIORITY_WARN2_MS && !m_bWarned2)
		{
			m_bWarned2 = true;
			if (m_Director)
				m_Director.Notify("ReinforcementsCalled", "HIGH PRIORITY 4:00 remaining — " + ResolveTitle());
		}
		if (remain <= PRIORITY_WARN1_MS && !m_bWarned1)
		{
			m_bWarned1 = true;
			if (m_Director)
				m_Director.Notify("ReinforcementsCalled", "HIGH PRIORITY 2:00 remaining — " + ResolveTitle());
		}
		if (remain > 0)
			return;

		m_bDemoted = true;
		m_bPriority = false;
		m_sTitle = ResolveTitle();
		if (m_vMarkerPos != vector.Zero)
			SpawnMapMarker(m_vMarkerPos);
		if (m_Director)
		{
			m_Director.Notify("ReinforcementsCalled", "HIGH PRIORITY expired — objective remains optional.");
			m_Director.OnPriorityTimeout(this);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickType(int now)
	{
		if (m_eType == IA_DefendEventType.CommanderFob)
		{
			if (IsPrimaryDead())
				Succeed("Enemy commander eliminated. Coordinated assault cancelled.");
			return;
		}

		if (m_eType == IA_DefendEventType.ElitePatrol)
		{
			if (AreAllDead())
				Succeed("Elite patrol destroyed.");
			else if (m_bEngaged && m_Primary)
				m_Primary.SetDefendMode(true, m_Director.GetDefendPoint());
			return;
		}

		if (m_eType == IA_DefendEventType.ScoutMortar)
		{
			NoteUnitsSeen();
			if (IsPrimaryDead() && !m_bReconDone)
			{
				Succeed("Scout destroyed. Extra-squad call-in aborted.");
				return;
			}
			if (!m_bReconDone)
				TickScoutRecon(now);
			if (m_bReconDone && m_eState == IA_DefendEventState.Active)
			{
				Ignore("Scout completed recon. Spotting team deploying.");
				if (m_Director)
					m_Director.QueueMortarFollowup();
			}
			return;
		}

		if (m_eType == IA_DefendEventType.MortarTeam)
		{
			if (AreAllDead())
			{
				Succeed("Spotting crew eliminated. Later reinforcement pulses cancelled.");
				return;
			}
			if ((now - m_iSpawnTick) > MORTAR_REVEAL_MS && !m_bMortarRevealed)
			{
				m_bMortarRevealed = true;
				m_vApprox = Jitter(m_vSite, 40, 80);
				SpawnMapMarker(m_vApprox);
			}
			if ((now - m_iSpawnTick) > MORTAR_EXACT_MS)
				m_bExactMarker = true;
			if ((now - m_iSpawnTick) > MORTAR_FIRST_BARRAGE_MS)
				m_Director.PulseMortarBarrage();
			return;
		}

		if (m_eType == IA_DefendEventType.Convoy)
		{
			if (AreAllDead())
			{
				Succeed("Convoy destroyed. Next assault reduced.");
				return;
			}
			if (HasConvoyReachedLine())
				Ignore("Convoy reached the line. Extra troops joining the assault.");
			return;
		}

		if (m_eType == IA_DefendEventType.Relay)
		{
			if (AreAllDead())
			{
				Succeed("Signal relay destroyed. Inbound cues restored.");
				return;
			}
			if ((now - m_iSpawnTick) > RELAY_TIMEOUT_MS)
				Ignore("Relay still transmitting. Inbound wave direction hidden.");
			return;
		}

		if (m_eType == IA_DefendEventType.Sniper)
		{
			if (AreAllDead())
				Succeed("Sniper pair eliminated.");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnContent()
	{
		if (m_eType == IA_DefendEventType.CommanderFob)
		{
			SpawnFob();
			m_Primary = m_Director.SpawnEventGroup(OffsetFlat(m_vSite, 2, 1), 1, false, true, true);
			Track(m_Primary);
			Track(m_Director.SpawnEventGroup(OffsetFlat(m_vSite, 10, 2), 4, true, false, true));
			Track(m_Director.SpawnEventGroup(OffsetFlat(m_vSite, -8, 9), 4, false, false, true));
			Track(m_Director.SpawnEventGroup(OffsetFlat(m_vSite, -8, -9), 4, false, false, true));
			return;
		}

		if (m_eType == IA_DefendEventType.ElitePatrol)
		{
			m_Primary = m_Director.SpawnEventGroup(m_vSite, 8, true, false, false);
			Track(m_Primary);
			if (m_Primary)
			{
				m_Primary.RemoveAllOrders();
				m_Primary.AddOrder(Jitter(m_Director.GetDefendPoint(), 180, 260), IA_AiOrder.Patrol, true);
			}
			return;
		}

		if (m_eType == IA_DefendEventType.ScoutMortar)
		{
			m_Primary = m_Director.SpawnEventGroup(m_vSite, 3, false, false, false);
			Track(m_Primary);
			IssueScoutOp(0);
			return;
		}

		if (m_eType == IA_DefendEventType.MortarTeam)
		{
			m_Primary = m_Director.SpawnEventGroup(m_vSite, 3, false, false, true);
			Track(m_Primary);
			return;
		}

		if (m_eType == IA_DefendEventType.Convoy)
		{
			m_Primary = m_Director.SpawnEventConvoyVehicle(m_vSite);
			Track(m_Primary);
			if (m_Primary)
				Track(m_Primary.GetLinkedPassengerGroup());

			vector secondSite = IA_VehicleManager.FindRoadInAnnulus(m_vSite, 15, 40, -1);
			if (secondSite == vector.Zero)
				secondSite = m_vSite + Vector(14, 0, 8);
			IA_AiGroup second = m_Director.SpawnEventConvoyVehicle(secondSite);
			Track(second);
			if (second)
				Track(second.GetLinkedPassengerGroup());
			return;
		}

		if (m_eType == IA_DefendEventType.Relay)
		{
			m_Primary = m_Director.SpawnEventGroup(m_vSite, 3, false, false, true);
			Track(m_Primary);
			m_Director.SetHideInboundCues(true);
			return;
		}

		if (m_eType == IA_DefendEventType.Sniper)
		{
			m_Primary = m_Director.SpawnEventGroup(m_vSite, 2, true, false, true);
			Track(m_Primary);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Track(IA_AiGroup grp)
	{
		if (!grp)
			return;
		m_Groups.Insert(grp);
		if (!m_Primary)
			m_Primary = grp;
	}

	//------------------------------------------------------------------------------------------------
	protected vector PickSite(vector center)
	{
		if (m_eType == IA_DefendEventType.CommanderFob)
			return IA_SpawnPlacement.FindEventSite(center, 450, 700);
		if (m_eType == IA_DefendEventType.ScoutMortar)
			return IA_SpawnPlacement.FindEventSite(center, 280, 420);
		if (m_eType == IA_DefendEventType.Sniper)
			return IA_SpawnPlacement.FindEventSite(center, 300, 450);
		if (m_eType == IA_DefendEventType.MortarTeam)
			return IA_SpawnPlacement.FindEventSite(center, 500, 800);
		if (m_eType == IA_DefendEventType.Convoy)
		{
			vector road = IA_VehicleManager.FindRoadInAnnulus(center, 450, 850, -1);
			if (road != vector.Zero)
				return road;
		}
		return IA_SpawnPlacement.FindEventSite(center, 350, 600);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshEngaged()
	{
		if (m_bEngaged)
			return;
		int count = m_Groups.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup grp = m_Groups[i];
			if (!grp)
				continue;
			int danger = grp.GetLastDangerEventTime();
			if (danger > 0 && (System.GetUnixTime() - danger) < 20)
			{
				m_bEngaged = true;
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsPrimaryDead()
	{
		NoteUnitsSeen();
		if (!m_bUnitsSeen)
			return false;
		if (!m_Primary)
			return true;
		if (!m_Primary.IsSpawned())
			return false;
		return m_Primary.GetAliveCount() <= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasConvoyReachedLine()
	{
		if (!m_Director)
			return false;

		vector line = m_Director.GetDefendPoint();
		int count = m_Groups.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup grp = m_Groups[i];
			if (!grp)
				continue;
			if (grp.HasDumpedPassengers())
				return true;

			IA_AiGroup pax = grp.GetLinkedPassengerGroup();
			if (pax && pax.HasDumpedPassengers())
				return true;

			vector origin = grp.GetOrigin();
			if (origin == vector.Zero)
				continue;
			if (vector.Distance(origin, line) < 120)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AreAllDead()
	{
		NoteUnitsSeen();
		if (!m_bUnitsSeen)
			return false;
		int count = m_Groups.Count();
		if (count <= 0)
			return false;
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup grp = m_Groups[i];
			if (!grp)
				continue;
			if (!grp.IsSpawned())
				return false;
			if (grp.GetAliveCount() > 0)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void NoteUnitsSeen()
	{
		if (m_bUnitsSeen)
			return;

		int count = m_Groups.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AiGroup grp = m_Groups[i];
			if (!grp)
				continue;
			if (!grp.IsSpawned())
				continue;
			if (grp.GetAliveCount() <= 0)
				continue;

			m_bUnitsSeen = true;
			m_iUnitsSeenTick = System.GetTickCount();
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateMarker(int now)
	{
		bool exact = m_bExactMarker;
		if (m_eType == IA_DefendEventType.ScoutMortar)
			exact = true;
		if (m_eType == IA_DefendEventType.ElitePatrol && m_bEngaged)
			exact = true;
		if (m_eType == IA_DefendEventType.Sniper && m_bEngaged)
			exact = true;
		if (m_eType == IA_DefendEventType.MortarTeam && m_bMortarRevealed && (now - m_iSpawnTick) > MORTAR_EXACT_MS)
			exact = true;
		if (m_eType == IA_DefendEventType.Convoy && m_Primary)
			exact = true;

		int interval = 10000;
		if (exact)
			interval = 8000;
		if (m_iLastMarkerTick != 0 && (now - m_iLastMarkerTick) < interval)
			return;
		m_iLastMarkerTick = now;

		vector pos = m_vApprox;
		if (exact && m_Primary)
		{
			vector live = m_Primary.GetOrigin();
			if (live != vector.Zero)
				pos = live;
			else
				pos = m_vSite;
		}
		else if (exact)
			pos = m_vSite;
		else if (m_eType == IA_DefendEventType.CommanderFob)
			pos = m_vSite;
		else if (m_eType == IA_DefendEventType.Relay)
			pos = m_vSite;

		if (pos == vector.Zero)
			return;
		if (m_MapMarker && vector.Distance(pos, m_vMarkerPos) < 25)
			return;

		SpawnMapMarker(pos);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnMapMarker(vector origin)
	{
		if (origin == vector.Zero)
			return;

		RemoveMapMarker();

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr)
			return;

		ref SCR_MapMarkerBase marker = mgr.PrepareMilitaryMarker(EMilitarySymbolIdentity.OPFOR, EMilitarySymbolDimension.LAND, ResolveMarkerIcon());
		if (!marker)
		{
			marker = new SCR_MapMarkerBase();
			marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		}

		int worldX = origin[0];
		int worldZ = origin[2];
		marker.SetWorldPos(worldX, worldZ);
		marker.SetCustomText(m_sTitle);
		marker.SetCanBeRemovedByOwner(false);
		mgr.InsertStaticMarker(marker, false, true);
		m_MapMarker = marker;
		m_vMarkerPos = origin;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMapMarker()
	{
		if (!m_MapMarker)
			return;

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (mgr)
			mgr.RemoveStaticMarker(m_MapMarker);

		m_MapMarker = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CompleteTask(bool success)
	{
		RemoveMapMarker();
	}

	//------------------------------------------------------------------------------------------------
	protected EMilitarySymbolIcon ResolveMarkerIcon()
	{
		if (m_eType == IA_DefendEventType.ScoutMortar)
			return EMilitarySymbolIcon.RECON;
		if (m_eType == IA_DefendEventType.MortarTeam)
			return EMilitarySymbolIcon.MORTAR;
		if (m_eType == IA_DefendEventType.Convoy)
			return EMilitarySymbolIcon.MOTORIZED;
		if (m_eType == IA_DefendEventType.Relay)
			return EMilitarySymbolIcon.RELAY;
		if (m_eType == IA_DefendEventType.Sniper)
			return EMilitarySymbolIcon.SNIPER;
		if (m_eType == IA_DefendEventType.CommanderFob)
			return EMilitarySymbolIcon.MOBILEHQ;
		return EMilitarySymbolIcon.INFANTRY;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildScoutOps(vector center)
	{
		m_ObsPoints.Clear();
		m_iObsIndex = 0;
		m_bDwelling = false;
		m_iDwellStart = 0;

		vector toward = center - m_vSite;
		toward[1] = 0;
		float len = toward.Length();
		vector firstOp = m_vSite;
		if (len > 1)
		{
			toward = toward.Normalized();
			firstOp = m_vSite + toward * 40;
			firstOp[1] = GetGame().GetWorld().GetSurfaceY(firstOp[0], firstOp[2]);
		}
		m_ObsPoints.Insert(firstOp);

		float baseAng = Math.Atan2(m_vSite[2] - center[2], m_vSite[0] - center[0]);
		m_ObsPoints.Insert(RingPoint(center, baseAng + 2.1, 280, 360));
		m_ObsPoints.Insert(RingPoint(center, baseAng + 4.2, 280, 360));
	}

	//------------------------------------------------------------------------------------------------
	protected vector RingPoint(vector center, float angle, float minR, float maxR)
	{
		float dist = IA_Game.rng.RandFloatXY(minR, maxR);
		vector outPos;
		outPos[0] = center[0] + Math.Cos(angle) * dist;
		outPos[2] = center[2] + Math.Sin(angle) * dist;
		outPos[1] = GetGame().GetWorld().GetSurfaceY(outPos[0], outPos[2]);
		return outPos;
	}

	//------------------------------------------------------------------------------------------------
	protected void IssueScoutOp(int index)
	{
		if (!m_Primary)
			return;
		if (!m_ObsPoints)
			return;
		if (index < 0 || index >= m_ObsPoints.Count())
			return;

		vector op = m_ObsPoints[index];
		if (op == vector.Zero)
			return;

		m_Primary.RemoveAllOrders();
		m_Primary.AddOrder(op, IA_AiOrder.Move, true);
		m_bScoutOrdered = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickScoutRecon(int now)
	{
		if (!m_bUnitsSeen)
			return;

		if ((now - m_iUnitsSeenTick) >= m_iReconMs)
		{
			m_bReconDone = true;
			return;
		}

		if (!m_Primary)
			return;

		if (!m_bScoutOrdered)
			IssueScoutOp(m_iObsIndex);

		if (!m_ObsPoints || m_iObsIndex >= m_ObsPoints.Count())
		{
			m_bReconDone = true;
			return;
		}

		vector origin = m_Primary.GetOrigin();
		if (origin == vector.Zero)
			return;

		vector op = m_ObsPoints[m_iObsIndex];
		float dist = vector.Distance(origin, op);
		if (dist > 50)
		{
			m_bDwelling = false;
			m_iDwellStart = 0;
			return;
		}

		if (!m_bDwelling)
		{
			m_bDwelling = true;
			m_iDwellStart = now;
			m_Primary.RemoveAllOrders();
			m_Primary.AddOrder(op, IA_AiOrder.Hold, true);
			return;
		}

		if ((now - m_iDwellStart) < SCOUT_DWELL_MS)
			return;

		m_iObsIndex = m_iObsIndex + 1;
		m_bDwelling = false;
		m_iDwellStart = 0;
		if (m_iObsIndex >= m_ObsPoints.Count())
		{
			m_bReconDone = true;
			return;
		}

		IssueScoutOp(m_iObsIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnFob()
	{
		if (!m_FobEntities)
			m_FobEntities = new array<IEntity>();

		float faceAo = 0;
		if (m_Director)
		{
			vector dir = m_Director.GetDefendPoint() - m_vSite;
			dir[1] = 0;
			if (dir.Length() > 1)
			{
				vector angles = dir.VectorToAngles();
				faceAo = angles[0];
			}
		}

		SpawnFobPiece(FOB_TENT_HQ, m_vSite, faceAo);
		SpawnFobPiece(FOB_TENT_BARRACKS, OffsetFlat(m_vSite, 7, -5), faceAo + 90);
		SpawnFobPiece(FOB_ANTENNA, OffsetFlat(m_vSite, -5, -6), faceAo);
		SpawnFobPiece(FOB_SANDBAGS, OffsetFlat(m_vSite, 11, 2), faceAo + 90);
		SpawnFobPiece(FOB_SANDBAGS, OffsetFlat(m_vSite, -9, 10), faceAo + 210);
		SpawnFobPiece(FOB_SANDBAGS, OffsetFlat(m_vSite, -9, -10), faceAo + 330);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnFobPiece(ResourceName prefab, vector origin, float yawDeg)
	{
		if (prefab == "")
			return;
		if (origin == vector.Zero)
			return;

		Resource res = Resource.Load(prefab);
		if (!res)
			return;

		vector pos = origin;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);

		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yawDeg, 0, 0), mat);
		mat[3] = pos;
		SCR_TerrainHelper.SnapToTerrain(mat, GetGame().GetWorld());

		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(mat, params.Transform);

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
			return;

		m_FobEntities.Insert(ent);
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
			aiWorld.RequestNavmeshRebuildEntity(ent);
	}

	//------------------------------------------------------------------------------------------------
	protected void DespawnFob()
	{
		if (!m_FobEntities)
			return;

		int count = m_FobEntities.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IEntity ent = m_FobEntities[i];
			if (ent)
				IA_Game.AddEntityToGc(ent);
		}
		m_FobEntities.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected vector OffsetFlat(vector center, float east, float north)
	{
		vector outPos = center;
		outPos[0] = center[0] + east;
		outPos[2] = center[2] + north;
		outPos[1] = GetGame().GetWorld().GetSurfaceY(outPos[0], outPos[2]);
		return outPos;
	}

	//------------------------------------------------------------------------------------------------
	protected vector Jitter(vector center, float minR, float maxR)
	{
		float angle = IA_Game.rng.RandFloat01() * Math.PI2;
		float dist = IA_Game.rng.RandFloatXY(minR, maxR);
		vector outPos;
		outPos[0] = center[0] + Math.Cos(angle) * dist;
		outPos[2] = center[2] + Math.Sin(angle) * dist;
		outPos[1] = GetGame().GetWorld().GetSurfaceY(outPos[0], outPos[2]);
		return outPos;
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveTitle()
	{
		if (m_eType == IA_DefendEventType.CommanderFob)
			return "Eliminate the enemy commander";
		if (m_eType == IA_DefendEventType.ElitePatrol)
			return "Hunt the elite patrol";
		if (m_eType == IA_DefendEventType.ScoutMortar)
			return "Destroy the scout element";
		if (m_eType == IA_DefendEventType.MortarTeam)
			return "Eliminate the spotting crew";
		if (m_eType == IA_DefendEventType.Convoy)
			return "Destroy the reinforcement convoy";
		if (m_eType == IA_DefendEventType.Relay)
			return "Destroy the signal relay";
		return "Hunt the sniper pair";
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveDesc()
	{
		if (m_eType == IA_DefendEventType.CommanderFob)
			return "Kill the officer at the rushed field FOB (command tent and sandbags). Success cancels the next major assault.";
		if (m_eType == IA_DefendEventType.ElitePatrol)
			return "Find and destroy the special-forces squad before it hunts gunfire.";
		if (m_eType == IA_DefendEventType.ScoutMortar)
			return "Find the scout team at the recon marker. They will occupy three observation posts for several minutes before calling extra squads onto the hold.";
		if (m_eType == IA_DefendEventType.MortarTeam)
			return "Find and kill the spotting crew. While they live, extra infantry squads are called onto the hold.";
		if (m_eType == IA_DefendEventType.Convoy)
			return "Destroy the truck convoy on the road before it reaches the line and dismounts.";
		if (m_eType == IA_DefendEventType.Relay)
			return "Destroy the mobile relay to keep inbound attack cues readable.";
		return "Hunt the sniper pair after they fire to remove overwatch.";
	}
}
