//------------------------------------------------------------------------------------------------
//! Server: gives a pilot their livery before they board. When the nearest
//! player walking up to a helicopter parked on its pad has earned one,
//! IA_HeliSkinManagerComponent recolours the helicopter where it stands. It is
//! the livery they last chose in the paint bay this session, stock included,
//! and the highest one they have unlocked when they have not chosen yet.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPadService
{
	protected static const float APPROACH_RADIUS_M = 10;

	// Identity id -> the livery that pilot last put on from the paint bay; lasts the session.
	protected static ref map<string, int> s_mChoices = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! Server: the pilot put this livery on from the paint bay, stock paint included.
	static void RememberChoice(int playerId, int skinId)
	{
		if (!Replication.IsServer())
			return;

		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (guid.IsEmpty())
			return;
		s_mChoices.Set(guid, skinId);
	}

	//------------------------------------------------------------------------------------------------
	//! \param choice the livery the pilot last chose, negative when they have not chosen
	//! \return the livery a pad airframe takes for this pilot, null to leave it as it is
	static IA_HeliSkinDef PickLivery(int rating, int choice)
	{
		if (choice == IA_HeliSkinCatalog.SKIN_NONE)
			return null;

		// A choice made as an admin, or before a threshold was raised, may not be unlocked.
		IA_HeliSkinDef chosen = IA_HeliSkinCatalog.FindDef(choice);
		if (chosen && IA_HeliSkinCatalog.IsUnlocked(chosen, rating))
			return chosen;
		return IA_HeliSkinCatalog.FindBestUnlocked(rating);
	}

	//------------------------------------------------------------------------------------------------
	//! Called once per tracker tick with the connected players.
	static void Tick(PlayerManager pm, notnull array<int> players, int now)
	{
		if (!Replication.IsServer() || !pm)
			return;

		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		array<IA_VehicleRespawner> pads = IA_VehicleRespawner.GetHeliPads();
		if (!skins || !pads)
			return;

		foreach (IA_VehicleRespawner pad : pads)
		{
			if (pad)
				TickPad(pm, players, now, pad, skins);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickPad(PlayerManager pm, notnull array<int> players, int now, notnull IA_VehicleRespawner pad, notnull IA_HeliSkinManagerComponent skins)
	{
		IEntity vehicle = pad.GetParkedVehicle();
		if (!vehicle)
			return;

		// Only an airframe on a paint channel can be recoloured.
		if (IA_HeliSkinManagerComponent.GetVehicleChannel(vehicle) == IA_HeliPaintChannels.CHANNEL_NONE)
			return;

		// A livery put on from the seat stays until the airframe is replaced, stock included.
		if (skins.IsPilotChoice(vehicle))
			return;

		int pilotId = NearestPlayerOnFoot(pm, players, vehicle);
		if (pilotId <= 0)
			return;

		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(pilotId);
		if (guid.IsEmpty())
			return;

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		int rating = store.GetRating(guid);
		if (rating < 0)
		{
			// An unknown rating unlocks nothing; ask and decide on a later tick.
			store.RequestRating(guid, pm.GetPlayerName(pilotId), now);
			return;
		}

		int choice = -1;
		if (!s_mChoices.Find(guid, choice))
			choice = -1;

		IA_HeliSkinDef best = PickLivery(rating, choice);
		if (!best)
			return;

		// Never trade a livery down: a better pilot's airframe stays as it is.
		IA_HeliSkinDef worn = IA_HeliSkinCatalog.FindDef(skins.GetVehicleSkin(vehicle));
		if (worn && worn.m_iRequiredPoints >= best.m_iRequiredPoints)
			return;

		if (!skins.SetVehicleSkin(vehicle, best.m_iId))
			return;

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][HeliSkin] Pad helicopter repainted to %1 for player %2.", best.m_sKey, pilotId), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return id of the nearest living friendly player on foot within reach of the vehicle, 0 when none
	protected static int NearestPlayerOnFoot(PlayerManager pm, notnull array<int> players, IEntity vehicle)
	{
		vector vehiclePos;
		if (!IA_AreaMarker.TryGetPawnWorldPos(vehicle, vehiclePos))
			return 0;

		int nearestId = 0;
		float nearestSq = APPROACH_RADIUS_M * APPROACH_RADIUS_M;
		vector pawnPos;
		float distSq;
		IEntity pawn;
		foreach (int playerId : players)
		{
			pawn = pm.GetPlayerControlledEntity(playerId);
			if (!IA_BasePlayerSampler.IsLivingConsciousPawn(pawn) || !IA_BasePlayerSampler.IsFriendlyPlayerPawn(pawn))
				continue;
			if (CompartmentAccessComponent.GetVehicleIn(pawn))
				continue;
			if (!IA_AreaMarker.TryGetPawnWorldPos(pawn, pawnPos))
				continue;

			distSq = vector.DistanceSq(pawnPos, vehiclePos);
			if (distSq < nearestSq)
			{
				nearestSq = distSq;
				nearestId = playerId;
			}
		}
		return nearestId;
	}
}
