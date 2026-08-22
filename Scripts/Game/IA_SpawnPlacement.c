class IA_SpawnPlacement
{
	static const float PLAYER_MIN_M = 280.0;
	static const float PLAYER_MAX_M = 550.0;
	static const float FIGHT_NEAR_AO_M = 700.0;
	static const float CENTER_MIN_M = 220.0;
	static const float CENTER_MAX_M = 550.0;
	static const float HARD_CAP_FROM_CENTER_M = 600.0;
	static const float ARRIVE_UNPIN_M = 120.0;
	static const int SAME_RADIUS_TRIES = 8;
	static const float EMPTY_CYLINDER_R = 0.6;
	static const float EMPTY_SEARCH_R = 18.0;
	static const float NAVMESH_REACH_M = 12.0;
	static const string NAVMESH_PROJECT = "Soldiers";

	static void CollectPlayerPositions(array<vector> positions)
	{
		if (!positions)
			return;

		positions.Clear();

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		int idCount = playerIds.Count();
		int i;
		for (i = 0; i < idCount; i++)
		{
			IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerIds[i]);
			if (playerEntity)
				positions.Insert(playerEntity.GetOrigin());
		}
	}

	static bool IsLegalInbound(vector pos, vector center, array<vector> players, float centerMax, bool applyPlayerMax)
	{
		if (pos == vector.Zero)
			return false;

		float fromCenter = vector.Distance(pos, center);
		if (fromCenter > centerMax)
			return false;

		if (!players || players.IsEmpty())
			return true;

		float playerMinSq = PLAYER_MIN_M * PLAYER_MIN_M;
		float playerMaxSq = PLAYER_MAX_M * PLAYER_MAX_M;
		float nearestSq = -1.0;

		int playerCount = players.Count();
		int i;
		for (i = 0; i < playerCount; i++)
		{
			float dsq = vector.DistanceSq(pos, players[i]);
			if (dsq < playerMinSq)
				return false;

			if (nearestSq < 0.0 || dsq < nearestSq)
				nearestSq = dsq;
		}

		if (applyPlayerMax && nearestSq > playerMaxSq)
			return false;

		return true;
	}

	static bool IsFightNearAo(vector center, array<vector> players)
	{
		if (!players || players.IsEmpty())
			return false;

		float nearSq = FIGHT_NEAR_AO_M * FIGHT_NEAR_AO_M;
		int playerCount = players.Count();
		int i;
		for (i = 0; i < playerCount; i++)
		{
			if (vector.DistanceSq(players[i], center) <= nearSq)
				return true;
		}
		return false;
	}

	static bool IsInSector(vector pos, vector center, int sectorIndex)
	{
		if (sectorIndex < 0)
			return true;

		float dx = pos[0] - center[0];
		float dz = pos[2] - center[2];
		float angle = Math.Atan2(dz, dx);
		if (angle < 0)
			angle = angle + Math.PI2;

		float sectorStart = sectorIndex * (Math.PI2 * 0.25);
		float sectorEnd = sectorStart + (Math.PI2 * 0.25);
		if (angle >= sectorStart && angle < sectorEnd)
			return true;
		return false;
	}

	static vector SamplePolar(vector center, float minR, float maxR, int sectorIndex)
	{
		float angle;
		if (sectorIndex >= 0)
		{
			float sectorStart = sectorIndex * (Math.PI2 * 0.25);
			float sectorEnd = sectorStart + (Math.PI2 * 0.25);
			angle = sectorStart + (IA_Game.rng.RandFloat01() * (sectorEnd - sectorStart));
		}
		else
		{
			angle = IA_Game.rng.RandFloat01() * Math.PI2;
		}

		float dist = IA_Game.rng.RandFloatXY(minR, maxR);
		vector pos;
		pos[0] = center[0] + Math.Cos(angle) * dist;
		pos[2] = center[2] + Math.Sin(angle) * dist;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		return pos;
	}

	static bool IsInOcean(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float oceanY = world.GetOceanHeight(pos[0], pos[2]);
		if (pos[1] <= oceanY)
			return true;
		return false;
	}

	static bool TrySnapInfantryPoint(vector sample, vector center, array<vector> players, float centerMax, bool applyPlayerMax, out vector outPos)
	{
		outPos = vector.Zero;
		if (IsInOcean(sample))
			return false;

		vector emptyPos;
		bool foundEmpty = SCR_WorldTools.FindEmptyTerrainPosition(emptyPos, sample, EMPTY_SEARCH_R, EMPTY_CYLINDER_R, 2.0, TraceFlags.ENTS | TraceFlags.OCEAN);
		if (!foundEmpty)
			return false;

		emptyPos[1] = GetGame().GetWorld().GetSurfaceY(emptyPos[0], emptyPos[2]);
		if (IsInOcean(emptyPos))
			return false;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
			aiWorld.RequestNavmeshLoad(emptyPos);

		vector candidate = emptyPos;
		if (aiWorld)
		{
			NavmeshWorldComponent navmesh = aiWorld.GetNavmeshWorldComponent(NAVMESH_PROJECT);
			if (navmesh)
			{
				if (!navmesh.IsTileLoaded(emptyPos) && !navmesh.IsTileRequested(emptyPos))
					navmesh.LoadTileIn(emptyPos);

				if (navmesh.IsTileLoaded(emptyPos) || navmesh.IsTileValid(emptyPos))
				{
					vector reachable = emptyPos;
					if (navmesh.GetReachablePoint(emptyPos, NAVMESH_REACH_M, reachable))
					{
						if (reachable != vector.Zero)
						{
							reachable[1] = GetGame().GetWorld().GetSurfaceY(reachable[0], reachable[2]);
							if (!IsInOcean(reachable))
								candidate = reachable;
						}
					}
				}
			}
		}

		if (!IsLegalInbound(candidate, center, players, centerMax, applyPlayerMax))
			return false;

		outPos = candidate;
		return true;
	}

	static vector FindInboundInfantrySpawn(vector center, int sectorIndex)
	{
		if (center == vector.Zero)
			return vector.Zero;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		bool fightNear = IsFightNearAo(center, players);
		vector found;

		if (TryInfantryPhase(center, players, CENTER_MIN_M, CENTER_MAX_M, sectorIndex, fightNear, found))
			return found;

		if (sectorIndex >= 0)
		{
			if (TryInfantryPhase(center, players, CENTER_MIN_M, CENTER_MAX_M, -1, fightNear, found))
				return found;
		}

		if (TryInfantryPhase(center, players, CENTER_MIN_M, HARD_CAP_FROM_CENTER_M, -1, fightNear, found))
			return found;

		if (fightNear)
		{
			if (TryInfantryPhase(center, players, CENTER_MIN_M, HARD_CAP_FROM_CENTER_M, -1, false, found))
				return found;
		}

		Print(string.Format("[IA][SpawnPlacement] miss infantry center=%1 players=%2", center.ToString(), players.Count()), LogLevel.WARNING);
		return vector.Zero;
	}

	static bool TryInfantryPhase(vector center, array<vector> players, float minR, float maxR, int sectorIndex, bool applyPlayerMax, out vector outPos)
	{
		outPos = vector.Zero;
		int attempt;
		for (attempt = 0; attempt < SAME_RADIUS_TRIES; attempt++)
		{
			vector sample = SamplePolar(center, minR, maxR, sectorIndex);
			if (sectorIndex >= 0 && !IsInSector(sample, center, sectorIndex))
				continue;

			vector snapped;
			if (TrySnapInfantryPoint(sample, center, players, maxR, applyPlayerMax, snapped))
			{
				outPos = snapped;
				return true;
			}
		}
		return false;
	}

	static vector FindInboundVehicleSpawn(vector center, int roadGroup, int sectorIndex)
	{
		if (center == vector.Zero)
			return vector.Zero;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		bool fightNear = IsFightNearAo(center, players);
		vector found;

		if (TryVehiclePhase(center, players, roadGroup, CENTER_MIN_M, CENTER_MAX_M, sectorIndex, fightNear, found))
			return found;

		if (sectorIndex >= 0)
		{
			if (TryVehiclePhase(center, players, roadGroup, CENTER_MIN_M, CENTER_MAX_M, -1, fightNear, found))
				return found;
		}

		if (TryVehiclePhase(center, players, roadGroup, CENTER_MIN_M, HARD_CAP_FROM_CENTER_M, -1, fightNear, found))
			return found;

		if (fightNear)
		{
			if (TryVehiclePhase(center, players, roadGroup, CENTER_MIN_M, HARD_CAP_FROM_CENTER_M, -1, false, found))
				return found;
		}

		Print(string.Format("[IA][SpawnPlacement] miss vehicle center=%1 players=%2", center.ToString(), players.Count()), LogLevel.WARNING);
		return vector.Zero;
	}

	static bool TryVehiclePhase(vector center, array<vector> players, int roadGroup, float minR, float maxR, int sectorIndex, bool applyPlayerMax, out vector outPos)
	{
		outPos = vector.Zero;
		int attempt;
		for (attempt = 0; attempt < SAME_RADIUS_TRIES; attempt++)
		{
			vector roadPos = IA_VehicleManager.FindRoadInAnnulus(center, minR, maxR, roadGroup);
			if (roadPos == vector.Zero)
				continue;

			if (sectorIndex >= 0 && !IsInSector(roadPos, center, sectorIndex))
				continue;

			roadPos[1] = GetGame().GetWorld().GetSurfaceY(roadPos[0], roadPos[2]);
			if (!IsLegalInbound(roadPos, center, players, maxR, applyPlayerMax))
				continue;

			outPos = roadPos;
			return true;
		}
		return false;
	}
}
