//------------------------------------------------------------------------------------------------
//! Server-side tracker that credits helicopter pilots for combat insertions and
//! puts an unlocked skin on the airframe they fly.
//!
//! An insertion is one living player who rode as a passenger in a player-piloted
//! helicopter, left it while it was still intact, and reached the ground alive
//! near an objective of the active AO. Rules live in IA_TransportScoring.
//!
//! The pilot is told through one card (IA_PilotHud), not one message per
//! passenger: credits are batched per pilot and reported at most once a tick as
//! an IA_PilotDropoffPayload, which the card merges while it is open. Taking a
//! pilot seat shows the same card with the rating alone.
//------------------------------------------------------------------------------------------------
class IA_TransportPilotTracker
{
	protected static const int TICK_MS = 1000;

	protected static ref IA_TransportPilotTracker s_Instance;

	protected ref map<int, ref IA_TransportRide> m_mRides;
	// Tick of each passenger's last credited insertion, keyed by identity so a reconnect cannot reset it.
	protected ref map<string, int> m_mLastCreditMs;
	// Credits not yet shown to their pilot, keyed by pilot identity.
	protected ref map<string, ref IA_TransportDropoff> m_mDropoffs;
	// Points a pilot earned whose total they have not seen: the rating was unknown or they had no HUD.
	protected ref map<string, int> m_mUnreported;
	// Pilots owed a rating card, identity to player id.
	protected ref map<string, int> m_mStatusOwed;
	protected ref map<string, int> m_mPilotSeenMs;
	protected ref map<string, int> m_mLastCardMs;

	//------------------------------------------------------------------------------------------------
	static void EnsureStarted()
	{
		if (!Replication.IsServer() || s_Instance)
			return;

		s_Instance = new IA_TransportPilotTracker();
		GetGame().GetCallqueue().CallLater(s_Instance.Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void IA_TransportPilotTracker()
	{
		m_mRides = new map<int, ref IA_TransportRide>();
		m_mLastCreditMs = new map<string, int>();
		m_mDropoffs = new map<string, ref IA_TransportDropoff>();
		m_mUnreported = new map<string, int>();
		m_mStatusOwed = new map<string, int>();
		m_mPilotSeenMs = new map<string, int>();
		m_mLastCardMs = new map<string, int>();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		int now = System.GetTickCount();
		ref array<int> players = {};
		pm.GetPlayers(players);

		foreach (int playerId : players)
		{
			TickPlayer(pm, playerId, now);
		}
		IA_TransportPilotStore.GetInstance().SendQueuedRequests();
		FlushDropoffs(pm, now);
		FlushStatus(pm, players, now);

		// Riders who disconnected never reach the per-player pass above.
		ref array<int> stale = {};
		foreach (int riderId, IA_TransportRide ride : m_mRides)
		{
			if (!players.Contains(riderId))
				stale.Insert(riderId);
		}
		foreach (int staleId : stale)
		{
			m_mRides.Remove(staleId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickPlayer(PlayerManager pm, int playerId, int now)
	{
		IEntity pawn = pm.GetPlayerControlledEntity(playerId);
		if (!IA_BasePlayerSampler.IsLivingConsciousPawn(pawn) || !IA_BasePlayerSampler.IsFriendlyPlayerPawn(pawn))
		{
			m_mRides.Remove(playerId);
			return;
		}

		// Seats live on slotted parts, so resolve the root vehicle rather than the compartment owner.
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(pawn);
		if (vehicle && IsHelicopter(vehicle))
		{
			TickAboard(pm, playerId, pawn, vehicle, now);
			return;
		}

		IA_TransportRide ride = m_mRides.Get(playerId);
		if (!ride)
			return;

		// Boarding anything else ends the ride: a truck from the LZ is not the pilot's delivery.
		if (vehicle)
		{
			m_mRides.Remove(playerId);
			return;
		}

		if (!ride.m_bExited)
		{
			// Survivors of a shot-down helicopter were not inserted.
			if (!IsIntact(ride.m_Vehicle))
			{
				m_mRides.Remove(playerId);
				return;
			}
			ride.m_bExited = true;
			ride.m_iExitMs = now;
		}

		if (now - ride.m_iExitMs > IA_TransportScoring.SETTLE_TIMEOUT_MS)
		{
			m_mRides.Remove(playerId);
			return;
		}

		// Still dropping from a hover or under a canopy; judge where they land.
		if (!IA_BasePlayerSampler.IsStandingForZone(pawn))
			return;

		CreditRide(playerId, pawn, ride, now);
		m_mRides.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickAboard(PlayerManager pm, int playerId, IEntity pawn, IEntity vehicle, int now)
	{
		int pilotId = 0;
		Vehicle veh = Vehicle.Cast(vehicle);
		if (veh)
		{
			IEntity pilot = veh.GetPilot();
			if (pilot)
				pilotId = pm.GetPlayerIdFromControlledEntity(pilot);
		}

		if (pilotId == playerId)
		{
			m_mRides.Remove(playerId);
			ApplyPilotSkin(pm, playerId, vehicle);
			NotePilotSeat(playerId, now);
			return;
		}

		IA_TransportRide ride = m_mRides.Get(playerId);
		if (!ride || ride.m_bExited || ride.m_Vehicle != vehicle)
		{
			ref IA_TransportRide started = new IA_TransportRide();
			started.m_Vehicle = vehicle;
			IA_AreaMarker.TryGetPawnWorldPos(vehicle, started.m_vBoardPos);
			m_mRides.Set(playerId, started);
			ride = started;
		}

		if (pilotId > 0 && pilotId != ride.m_iPilotPlayerId)
		{
			ride.m_iPilotPlayerId = pilotId;
			ride.m_sPilotGuid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId);
			ride.m_sPilotName = pm.GetPlayerName(pilotId);
			// Fetch early so the pilot's total is known by the time passengers step out.
			IA_TransportPilotStore.GetInstance().RequestRating(ride.m_sPilotGuid, ride.m_sPilotName, System.GetTickCount());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreditRide(int passengerId, IEntity pawn, IA_TransportRide ride, int now)
	{
		if (ride.m_sPilotGuid.IsEmpty())
			return;

		vector pos;
		if (!IA_AreaMarker.TryGetPawnWorldPos(pawn, pos))
			return;

		string passengerGuid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(passengerId);
		if (passengerGuid.IsEmpty() || passengerGuid == ride.m_sPilotGuid)
			return;

		int lastCredit = -1;
		if (m_mLastCreditMs.Contains(passengerGuid))
			lastCredit = m_mLastCreditMs.Get(passengerGuid);

		float travel = vector.DistanceXZ(ride.m_vBoardPos, pos);
		if (!IA_TransportScoring.IsCreditableRide(travel, now, lastCredit))
			return;

		float edge = NearestObjectiveEdge(pos);
		if (edge < 0)
			return;

		int points = IA_TransportScoring.InsertionPoints(edge);
		if (points <= 0)
			return;

		m_mLastCreditMs.Set(passengerGuid, now);
		Award(ride, points, edge, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void Award(IA_TransportRide ride, int points, float edge, int now)
	{
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		IA_TransportPilotRecord record = store.AddInsertion(ride.m_sPilotGuid, ride.m_sPilotName, points);
		if (!record)
			return;

		IA_StatsManager.GetInstance().QueueTransportInsertion(ride.m_sPilotGuid, ride.m_sPilotName, points);

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][TransportPilot] Insertion credited: +%1 at %2 m from the objective.", points, Math.Round(edge)), LogLevel.NORMAL);
		}

		// Passengers of one landing settle over several ticks; FlushDropoffs reports them together.
		IA_TransportDropoff dropoff = m_mDropoffs.Get(ride.m_sPilotGuid);
		if (!dropoff)
		{
			ref IA_TransportDropoff started = new IA_TransportDropoff();
			started.m_sPilotGuid = ride.m_sPilotGuid;
			started.m_iFirstMs = now;
			m_mDropoffs.Set(ride.m_sPilotGuid, started);
			dropoff = started;
		}
		dropoff.m_iPilotPlayerId = ride.m_iPilotPlayerId;
		dropoff.Add(points, edge);
	}

	//------------------------------------------------------------------------------------------------
	//! Report each pilot's batched credits as one card update.
	protected void FlushDropoffs(PlayerManager pm, int now)
	{
		if (m_mDropoffs.IsEmpty())
			return;

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		ref array<string> done = {};
		foreach (string guid, IA_TransportDropoff dropoff : m_mDropoffs)
		{
			int pilotId = dropoff.m_iPilotPlayerId;
			// Player ids are reused after a disconnect, so confirm it is still the same pilot.
			if (SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId) != guid)
			{
				done.Insert(guid);
				continue;
			}

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pm.GetPlayerControlledEntity(pilotId));
			if (!character)
			{
				// A dead pilot has no HUD. The points are banked either way; the total follows on a rating card.
				if (now - dropoff.m_iFirstMs >= IA_TransportScoring.DROPOFF_EXPIRE_MS)
				{
					OweStatus(guid, pilotId, dropoff.m_iPoints);
					done.Insert(guid);
				}
				continue;
			}

			ref IA_PilotDropoffPayload payload = new IA_PilotDropoffPayload();
			payload.m_iKind = IA_PilotDropoffPayload.KIND_DROP;
			payload.m_iTroops = dropoff.m_iTroops;
			payload.m_iPoints = dropoff.m_iPoints;
			payload.m_iEdgeM = dropoff.AverageEdge();

			int rating = store.GetRating(guid);
			if (rating < 0)
			{
				// Until the global total arrives the points are banked but no total can be shown.
				OweStatus(guid, pilotId, dropoff.m_iPoints);
			}
			else
			{
				FillProgress(payload, rating, dropoff.m_iPoints + TakeUnreported(guid));
				m_mStatusOwed.Remove(guid);
			}

			character.SetUIOne("PilotProgress", payload.Pack(), pilotId);
			m_mLastCardMs.Set(guid, now);
			done.Insert(guid);
		}

		foreach (string doneGuid : done)
		{
			m_mDropoffs.Remove(doneGuid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Send the rating card to pilots who took a seat, or whose total arrived after their card.
	protected void FlushStatus(PlayerManager pm, notnull array<int> players, int now)
	{
		if (m_mStatusOwed.IsEmpty())
			return;

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		ref array<string> done = {};
		foreach (string guid, int pilotId : m_mStatusOwed)
		{
			if (!players.Contains(pilotId) || SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId) != guid)
			{
				m_mUnreported.Remove(guid);
				done.Insert(guid);
				continue;
			}

			// A dropoff about to be reported carries the total itself.
			if (m_mDropoffs.Contains(guid))
				continue;

			int rating = store.GetRating(guid);
			if (rating < 0)
			{
				store.RequestRating(guid, pm.GetPlayerName(pilotId), now);
				continue;
			}

			int earned = 0;
			if (m_mUnreported.Contains(guid))
				earned = m_mUnreported.Get(guid);
			int lastCard = -1;
			if (m_mLastCardMs.Contains(guid))
				lastCard = m_mLastCardMs.Get(guid);

			// Unreported points always get their total; a seat change alone waits out the cooldown.
			if (earned <= 0 && !IA_TransportScoring.IsStatusDue(now, lastCard))
			{
				done.Insert(guid);
				continue;
			}

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pm.GetPlayerControlledEntity(pilotId));
			if (!character)
			{
				if (earned <= 0)
					done.Insert(guid);
				continue;
			}

			ref IA_PilotDropoffPayload payload = new IA_PilotDropoffPayload();
			payload.m_iKind = IA_PilotDropoffPayload.KIND_STATUS;
			FillProgress(payload, rating, earned);
			m_mUnreported.Remove(guid);

			character.SetUIOne("PilotProgress", payload.Pack(), pilotId);
			m_mLastCardMs.Set(guid, now);
			done.Insert(guid);
		}

		foreach (string doneGuid : done)
		{
			m_mStatusOwed.Remove(doneGuid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param earned points credited since the pilot last saw a total; crossing a threshold with them announces the unlock
	protected void FillProgress(notnull IA_PilotDropoffPayload payload, int rating, int earned)
	{
		IA_HeliSkinDef unlocked = payload.SetProgress(rating, earned);
		if (unlocked)
			IA_Log.Info(string.Format("[IA][TransportPilot] Skin %1 unlocked at rating %2.", unlocked.m_sKey, rating));
	}

	//------------------------------------------------------------------------------------------------
	protected void OweStatus(string guid, int pilotId, int points)
	{
		int total = points;
		if (m_mUnreported.Contains(guid))
			total = total + m_mUnreported.Get(guid);
		if (total > 0)
			m_mUnreported.Set(guid, total);
		m_mStatusOwed.Set(guid, pilotId);
	}

	//------------------------------------------------------------------------------------------------
	protected int TakeUnreported(string guid)
	{
		if (!m_mUnreported.Contains(guid))
			return 0;

		int points = m_mUnreported.Get(guid);
		m_mUnreported.Remove(guid);
		return points;
	}

	//------------------------------------------------------------------------------------------------
	//! A pilot who has just sat down is owed a look at their rating.
	protected void NotePilotSeat(int pilotId, int now)
	{
		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId);
		if (guid.IsEmpty())
			return;

		int lastSeen = -1;
		if (m_mPilotSeenMs.Contains(guid))
			lastSeen = m_mPilotSeenMs.Get(guid);
		m_mPilotSeenMs.Set(guid, now);

		if (IA_TransportScoring.IsNewPilotSeat(now, lastSeen))
			m_mStatusOwed.Set(guid, pilotId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPilotSkin(PlayerManager pm, int pilotId, IEntity vehicle)
	{
		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId);
		if (guid.IsEmpty())
			return;

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		int rating = store.GetRating(guid);
		if (rating < 0)
		{
			store.RequestRating(guid, pm.GetPlayerName(pilotId), System.GetTickCount());
			return;
		}

		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		EntityPrefabData prefab = vehicle.GetPrefabData();
		if (!skins || !prefab)
			return;

		IA_HeliSkinDef def = IA_HeliSkinCatalog.ResolveForPilot(rating, prefab.GetPrefabName());
		if (def)
			skins.SetVehicleSkin(vehicle, def.m_iId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return metres to the nearest objective circle of the active AO, negative when there is none
	protected float NearestObjectiveEdge(vector pos)
	{
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		if (!markers)
			return -1;

		int activeGroup = IA_VehicleManager.GetActiveGroup();
		float best = -1;
		foreach (IA_AreaMarker marker : markers)
		{
			if (!marker || marker.m_areaGroup != activeGroup)
				continue;

			float edge = IA_TransportScoring.EdgeDistance(pos, marker.GetOrigin(), marker.GetRadius());
			if (best < 0 || edge < best)
				best = edge;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHelicopter(IEntity vehicle)
	{
		return vehicle.FindComponent(VehicleHelicopterSimulation) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIntact(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return false;
		return true;
	}
}
