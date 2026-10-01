//------------------------------------------------------------------------------------------------
//! Server: gives a pilot their unlocked skin before they board. A helicopter
//! still parked and empty on its pad is replaced with the skinned prefab when
//! the nearest player walking up to it has earned one. Paint cannot change on a
//! live vehicle, so nothing happens once a helicopter has left its pad.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPadService
{
	protected static const float APPROACH_RADIUS_M = 10;

	//------------------------------------------------------------------------------------------------
	//! Called once per tracker tick with the connected players.
	static void Tick(PlayerManager pm, notnull array<int> players, int now)
	{
		if (!Replication.IsServer() || !pm)
			return;

		array<IA_VehicleRespawner> pads = IA_VehicleRespawner.GetHeliPads();
		if (!pads)
			return;

		// A swap takes the vehicle off its pad, so iterate a copy.
		ref array<IA_VehicleRespawner> snapshot = {};
		snapshot.Copy(pads);
		foreach (IA_VehicleRespawner pad : snapshot)
		{
			if (pad)
				TickPad(pm, players, now, pad);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickPad(PlayerManager pm, notnull array<int> players, int now, notnull IA_VehicleRespawner pad)
	{
		IEntity vehicle = pad.GetParkedVehicle();
		if (!vehicle)
			return;

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (!prefabData)
			return;

		ResourceName prefab = prefabData.GetPrefabName();
		ResourceName stock = IA_HeliSkinCatalog.FindStockPrefab(prefab);
		if (stock.IsEmpty())
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

		IA_HeliSkinDef best = IA_HeliSkinCatalog.ResolveForPilot(rating, stock);
		if (!best)
			return;

		// Never trade a skin down: a better pilot's airframe stays as it is.
		IA_HeliSkinDef worn = IA_HeliSkinCatalog.FindDefBySkinPrefab(prefab);
		if (worn && worn.m_iRequiredPoints >= best.m_iRequiredPoints)
			return;

		if (!IA_HeliSkinSwap.Begin(vehicle, best.FindVariant(stock), pad))
			return;

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][HeliSkin] Pad helicopter swapped to %1 for player %2.", best.m_sKey, pilotId), LogLevel.NORMAL);
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
