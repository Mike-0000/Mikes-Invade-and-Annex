class IA_SpawnPlacement
{
	static const float PLAYER_MIN_M = 280.0;
	static const float PLAYER_MAX_M = 550.0;
	static const float DESPAWN_PLAYER_SAFE_M = 600.0;
	static const float FIGHT_NEAR_AO_M = 700.0;
	static const float CENTER_MIN_M = 220.0;
	static const float CENTER_MAX_M = 550.0;
	static const float HARD_CAP_FROM_CENTER_M = 600.0;
	static const float ARRIVE_UNPIN_M = 120.0;
	static const int SAME_RADIUS_TRIES = 8;
	static const float EMPTY_CYLINDER_R = 0.6;
	static const float EMPTY_SEARCH_R = 18.0;
	static const float UNIT_SEARCH_R = 6.0;
	static const float NAVMESH_REACH_M = 12.0;
	static const float WALKABLE_RAISE_M = 8.0;
	static const float WALKABLE_DOWN_M = 20.0;
	static const float STAND_CLEARANCE_M = 1.9;
	static const float SURFACE_BIAS_M = 0.05;
	static const float MAX_ABOVE_TERRAIN_M = 4.5;
	static const float OPEN_SKY_M = 12.0;
	static const float DROP_LZ_RAISE_M = 30.0;
	static const float DROP_LZ_DOWN_M = 40.0;
	static const float DROP_LZ_SEARCH_R = 40.0;
	static const float DROP_LZ_SEARCH_WIDE_R = 80.0;
	static const int DROP_LZ_SAMPLE_TRIES = 4;
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
			if (!playerEntity)
				continue;

			vector worldTm[4];
			playerEntity.GetWorldTransform(worldTm);
			positions.Insert(worldTm[3]);
		}
	}

	static bool IsNearAnyPlayer(vector pos, array<vector> players, float radiusM)
	{
		if (!players || players.IsEmpty())
			return false;
		if (pos == vector.Zero)
			return false;

		float radiusSq = radiusM * radiusM;
		int playerCount = players.Count();
		int i;
		for (i = 0; i < playerCount; i++)
		{
			if (vector.DistanceSq(pos, players[i]) <= radiusSq)
				return true;
		}

		return false;
	}

	//! Occupying / scale-up spawn is not inbound. If the requested town point is
	//! inside PLAYER_MIN_M, move it to a legal inbound point. When a fight is
	//! already near the AO, keep the point exact so later road-search cannot
	//! walk it onto a player. Returns false if no safe point exists.
	static bool ResolveOccupyingSpawn(vector requested, vector center, bool wasExact, out vector outPos, out bool outExact)
	{
		outPos = requested;
		outExact = wasExact;

		if (requested == vector.Zero || center == vector.Zero)
			return false;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		if (IsNearAnyPlayer(requested, players, PLAYER_MIN_M))
		{
			vector inbound = FindInboundInfantrySpawn(center, -1);
			if (inbound == vector.Zero)
				return false;

			Print(string.Format("[IA][SpawnPlacement] occupying relocate from %1 to %2", requested.ToString(), inbound.ToString()), LogLevel.NORMAL);
			outPos = inbound;
			outExact = true;
			return true;
		}

		if (IsFightNearAo(center, players))
			outExact = true;

		outPos = SnapInfantryPos(outPos, EMPTY_SEARCH_R);
		if (IsNearAnyPlayer(outPos, players, PLAYER_MIN_M))
		{
			vector inbound = FindInboundInfantrySpawn(center, -1);
			if (inbound == vector.Zero)
				return false;

			outPos = inbound;
			outExact = true;
		}

		return true;
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

	//! GetSurfaceY is the terrain mesh only. Rocks and building slabs sit above it,
	//! so snapping infantry Y to terrain puts them inside that collision.
	static bool HasStandRoom(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		ref TraceParam up = new TraceParam();
		up.Start = pos + Vector(0, 0.12, 0);
		up.End = up.Start + Vector(0, STAND_CLEARANCE_M, 0);
		up.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float coef = world.TraceMove(up, null);
		if (coef < 1.0)
			return false;

		return true;
	}

	//! Traced downward from above the position, not up from it: a ray leaving a
	//! building through the underside of its roof can miss one-sided collision,
	//! which reports an interior as open sky.
	static bool HasOpenSky(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		ref TraceParam down = new TraceParam();
		down.Start = pos + Vector(0, OPEN_SKY_M, 0);
		down.End = pos + Vector(0, 0.17, 0);
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float coef = world.TraceMove(down, null);
		if (coef < 1.0)
			return false;

		return true;
	}

	//! Streets and rooftops are valid. Interiors fail the open-sky test.
	//! Does not use TryWalkableAt — that helper rejects anything above MAX_ABOVE_TERRAIN_M.
	static bool TryDropLzAt(vector sample, out vector outPos)
	{
		outPos = vector.Zero;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float terrainY = world.GetSurfaceY(sample[0], sample[2]);
		float oceanY = world.GetOceanHeight(sample[0], sample[2]);

		vector start;
		start[0] = sample[0];
		start[1] = terrainY + DROP_LZ_RAISE_M;
		start[2] = sample[2];

		ref TraceParam down = new TraceParam();
		down.Start = start;
		down.End = start - Vector(0, DROP_LZ_DOWN_M, 0);
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.OCEAN;
		float coef = world.TraceMove(down, null);
		if (coef >= 1.0)
			return false;

		vector hit = start - Vector(0, coef * DROP_LZ_DOWN_M, 0);
		hit[1] = hit[1] + SURFACE_BIAS_M;

		if (hit[1] <= oceanY)
			return false;

		if (!HasStandRoom(hit))
			return false;

		if (!HasOpenSky(hit))
			return false;

		if (!SCR_WorldTools.TraceCilinderUtil(hit + Vector(0, 1.0, 0), EMPTY_CYLINDER_R, 2.0, TraceFlags.ENTS | TraceFlags.OCEAN, world))
			return false;

		outPos = hit;
		return true;
	}

	//! Open-sky LZ: first WORLD|ENTS surface with stand room and no ceiling.
	//! Rooftops are allowed; rooms are not.
	static bool TryFindDropLz(vector sample, float searchRadius, out vector outPos)
	{
		outPos = vector.Zero;
		if (TryDropLzAt(sample, outPos))
			return true;

		if (searchRadius <= 0.5)
			return false;

		int rings = 4;
		int ring;
		for (ring = 1; ring <= rings; ring++)
		{
			float ringF = ring;
			float ringsF = rings;
			float radius = searchRadius * (ringF / ringsF);
			int steps = 6 * ring;
			int step;
			for (step = 0; step < steps; step++)
			{
				float stepF = step;
				float stepsF = steps;
				float angle = Math.PI2 * (stepF / stepsF);
				vector probe;
				probe[0] = sample[0] + Math.Cos(angle) * radius;
				probe[2] = sample[2] + Math.Sin(angle) * radius;
				probe[1] = sample[1];
				if (TryDropLzAt(probe, outPos))
					return true;
			}
		}

		return false;
	}

	static bool TryWalkableAt(vector sample, out vector outPos)
	{
		outPos = vector.Zero;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float terrainY = world.GetSurfaceY(sample[0], sample[2]);
		float oceanY = world.GetOceanHeight(sample[0], sample[2]);

		vector start;
		start[0] = sample[0];
		start[1] = terrainY + WALKABLE_RAISE_M;
		start[2] = sample[2];

		ref TraceParam down = new TraceParam();
		down.Start = start;
		down.End = start - Vector(0, WALKABLE_DOWN_M, 0);
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.OCEAN;
		float coef = world.TraceMove(down, null);
		if (coef >= 1.0)
			return false;

		vector hit = start - Vector(0, coef * WALKABLE_DOWN_M, 0);
		hit[1] = hit[1] + SURFACE_BIAS_M;

		if (hit[1] <= oceanY)
			return false;

		float aboveTerrain = hit[1] - terrainY;
		if (aboveTerrain > MAX_ABOVE_TERRAIN_M)
			return false;

		if (!HasStandRoom(hit))
			return false;

		if (!SCR_WorldTools.TraceCilinderUtil(hit + Vector(0, 1.0, 0), EMPTY_CYLINDER_R, 2.0, TraceFlags.ENTS | TraceFlags.OCEAN, world))
			return false;

		outPos = hit;
		return true;
	}

	//! Finds a nearby point with standing room on the first WORLD|ENTS surface,
	//! not the terrain mesh. Search radius 0 only tests the sample.
	static bool TryFindWalkableInfantryPos(vector sample, float searchRadius, out vector outPos)
	{
		outPos = vector.Zero;
		if (TryWalkableAt(sample, outPos))
			return true;

		if (searchRadius <= 0.5)
			return false;

		int rings = 4;
		int ring;
		for (ring = 1; ring <= rings; ring++)
		{
			float ringF = ring;
			float ringsF = rings;
			float radius = searchRadius * (ringF / ringsF);
			int steps = 6 * ring;
			int step;
			for (step = 0; step < steps; step++)
			{
				float stepF = step;
				float stepsF = steps;
				float angle = Math.PI2 * (stepF / stepsF);
				vector probe;
				probe[0] = sample[0] + Math.Cos(angle) * radius;
				probe[2] = sample[2] + Math.Sin(angle) * radius;
				probe[1] = sample[1];
				if (TryWalkableAt(probe, outPos))
					return true;
			}
		}

		return false;
	}

	static vector SnapInfantryPos(vector sample, float searchRadius)
	{
		vector walked;
		if (TryFindWalkableInfantryPos(sample, searchRadius, walked))
			return walked;

		return sample;
	}

	static bool TrySnapInfantryPoint(vector sample, vector center, array<vector> players, float centerMax, bool applyPlayerMax, out vector outPos)
	{
		outPos = vector.Zero;
		if (IsInOcean(sample))
			return false;

		vector emptyPos;
		if (!TryFindWalkableInfantryPos(sample, EMPTY_SEARCH_R, emptyPos))
			return false;

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
						if (reachable != vector.Zero && !IsInOcean(reachable) && HasStandRoom(reachable))
							candidate = reachable;
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

	//! Floor under a roof. First WORLD|ENTS hit from above is the roof; a second
	//! down-trace finds the standing position. Open-sky / no-stand rejects courtyards and roofs.
	static bool TryInteriorStandPos(float x, float z, float roofTopY, float minY, out vector outPos)
	{
		outPos = vector.Zero;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float terrainY = world.GetSurfaceY(x, z);
		float startY = roofTopY + 2;
		if (startY < terrainY + 3)
			startY = terrainY + 8;

		float endY = minY;
		if (endY > startY - 2)
			endY = terrainY - 0.5;

		vector start = Vector(x, startY, z);
		vector end = Vector(x, endY, z);

		ref TraceParam down = new TraceParam();
		down.Start = start;
		down.End = end;
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float coef = world.TraceMove(down, null);
		if (coef >= 1.0)
			return false;

		float span = startY - endY;
		vector firstHit = start;
		firstHit[1] = startY - (coef * span);

		if (firstHit[1] < terrainY + 1.8)
			return false;

		float drop = 0.35;
		while (drop <= 1.45)
		{
			vector start2 = firstHit;
			start2[1] = firstHit[1] - drop;
			down.Start = start2;
			down.End = end;
			coef = world.TraceMove(down, null);
			if (coef < 1.0)
			{
				float span2 = start2[1] - endY;
				vector floorPos = start2;
				floorPos[1] = start2[1] - (coef * span2) + SURFACE_BIAS_M;
				if (HasStandRoom(floorPos) && !HasOpenSky(floorPos))
				{
					outPos = floorPos;
					return true;
				}
			}
			drop = drop + 0.35;
		}

		return false;
	}

	//! Vanilla building garrison: CoverPost / ObservationPost smart actions
	//! on structures. Used with IA_AiOrder.Hold (Wait waypoint, infinite).
	static void FindGarrisonPosts(vector center, float radius, notnull array<vector> outPosts)
	{
		outPosts.Clear();
		if (center == vector.Zero)
			return;
		if (radius <= 0)
			return;

		ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!chimeraWorld)
			return;

		AISmartActionSystem saSystem = AISmartActionSystem.Cast(chimeraWorld.FindSystem(AISmartActionSystem));
		if (!saSystem)
			return;

		ref array<string> tags = new array<string>();
		tags.Insert("CoverPost");
		tags.Insert("ObservationPost");

		ref array<AISmartActionComponent> found = new array<AISmartActionComponent>();
		int count = saSystem.FindSmartActions(found, center, radius, tags, EAIFindSmartAction_TagTest.AnySet);
		if (count <= 0)
			return;

		int i;
		int foundCount = found.Count();
		for (i = 0; i < foundCount; i++)
		{
			AISmartActionComponent sa = found[i];
			if (!sa)
				continue;
			if (!sa.IsActionAccessible())
				continue;

			IEntity owner = sa.GetOwner();
			if (!owner)
				continue;

			vector pos = owner.GetOrigin() + sa.GetActionOffset();
			if (pos == vector.Zero)
				continue;

			outPosts.Insert(pos);
		}
	}
}
