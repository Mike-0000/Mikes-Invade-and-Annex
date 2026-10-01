//------------------------------------------------------------------------------------------------
//! Rules of the paint bay, the skin menu a helicopter pilot opens from the
//! seat. The client uses GetPilotedHelicopter to decide whether the menu may
//! open; the server decides everything else and never trusts what the client
//! says it has unlocked.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintService
{
	// Answers to a paint bay request.
	static const int RESULT_STATE = 0;			// nothing was changed, the rating is the answer
	static const int RESULT_APPLIED = 1;
	static const int RESULT_NOT_PILOT = 2;
	static const int RESULT_NO_CHANNEL = 3;		// this helicopter cannot be repainted
	static const int RESULT_LOCKED = 4;
	static const int RESULT_SYNCING = 5;		// the rating is not known yet
	static const int RESULT_UNAVAILABLE = 6;

	//------------------------------------------------------------------------------------------------
	//! \return the helicopter this character flies from the pilot's seat, null for a co-pilot, crew or passenger
	static IEntity GetPilotedHelicopter(IEntity pawn)
	{
		if (!pawn)
			return null;

		IEntity entity = CompartmentAccessComponent.GetVehicleIn(pawn);
		if (!entity || !entity.FindComponent(VehicleHelicopterSimulation))
			return null;

		Vehicle vehicle = Vehicle.Cast(entity);
		if (!vehicle || vehicle.GetPilot() != pawn)
			return null;
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Server. \return the player's global transport rating, negative while it is unknown (it is then asked for)
	static int ReadRating(int playerId)
	{
		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (guid.IsEmpty())
			return IA_TransportPilotRecord.RATING_UNKNOWN;

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		int rating = store.GetRating(guid);
		if (rating < 0)
			store.RequestRating(guid, GetGame().GetPlayerManager().GetPlayerName(playerId), System.GetTickCount());
		return rating;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a skin on the helicopter this player pilots, if they may wear it.
	//! \param admin an admin may wear any skin, which is how a skin is checked without earning it
	//! \return one of the RESULT_ values
	static int TrySetSkin(IEntity pawn, int skinId, int rating, bool admin)
	{
		if (!Replication.IsServer())
			return RESULT_UNAVAILABLE;

		IEntity vehicle = GetPilotedHelicopter(pawn);
		if (!vehicle)
			return RESULT_NOT_PILOT;

		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		if (!skins)
			return RESULT_UNAVAILABLE;
		if (IA_HeliSkinManagerComponent.GetVehicleChannel(vehicle) == IA_HeliPaintChannels.CHANNEL_NONE)
			return RESULT_NO_CHANNEL;

		if (skinId != IA_HeliSkinCatalog.SKIN_NONE)
		{
			IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(skinId);
			if (!def)
				return RESULT_UNAVAILABLE;
			if (!admin)
			{
				// An unknown rating unlocks nothing.
				if (rating < 0)
					return RESULT_SYNCING;
				if (!IA_HeliSkinCatalog.IsUnlocked(def, rating))
					return RESULT_LOCKED;
			}
		}

		if (!skins.SetVehicleSkin(vehicle, skinId, true))
			return RESULT_UNAVAILABLE;
		return RESULT_APPLIED;
	}
}
