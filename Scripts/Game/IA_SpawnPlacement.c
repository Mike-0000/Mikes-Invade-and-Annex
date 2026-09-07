class IA_SpawnPlacement
{
	static const float PLAYER_MIN_M = 280.0;
	static const float PLAYER_MAX_M = 550.0;
	static const float DESPAWN_PLAYER_SAFE_M = 600.0;
	static const float FIGHT_NEAR_AO_M = 700.0;
	static const float CENTER_MIN_M = 220.0;
	static const float CENTER_MAX_M = 550.0;
	static const float HARD_CAP_FROM_CENTER_M = 600.0;
	static const float OCCUPY_MIN_M = 80.0;
	static const float OCCUPY_MAX_M = 250.0;
	static const float HOLD_ORIGIN_MIN_M = 15.0;
	static const float HOLD_ORIGIN_MAX_M = 80.0;
	static const float HOLD_APPROACH_MIN_M = 6.0;
	static const float HOLD_APPROACH_MAX_M = 22.0;
	static const float HOLD_ENTER_M = 10.0;
	static const float HOLD_APPROACH_ARRIVE_M = 7.0;
	static const float REINF_MIN_M = 80.0;
	static const float REINF_MAX_M = 180.0;
	static const float REINF_PLAYER_MIN_M = 100.0;
	//! Defense waves must sit outside the hold, not in the 80-180 m reinforcement ring players already occupy.
	static const float DEFEND_WAVE_MIN_M = 280.0;
	static const float DEFEND_WAVE_MAX_M = 520.0;
	static const float DEFEND_WAVE_PLAYER_MIN_M = 280.0;
	static const float DEFEND_DROP_PLAYER_MIN_M = 200.0;
	static const float DEFEND_DROP_HOT_PLAYER_MIN_M = 150.0;
	static const int SAFE_ORIGIN_ROAD_TRIES = 8;
	static const int SAFE_ORIGIN_MESH_TRIES = 20;
	static const float SAFE_ORIGIN_REACH_M = 16.0;
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
	static const float SHELL_COLUMN_M = 200.0;
	static const float SHELL_FLOOR_EPS_M = 1.5;
	static const float SHELL_WORLD_COVER_M = 2.5;
	static const float SHELL_XZ_MIN_M = 10.0;
	static const float SHELL_HEIGHT_MIN_M = 6.0;
	static const int SHELL_SKIP_MAX = 6;
	static const float GROUND_FLOOR_PROBE_M = 2.15;
	static const float GROUND_FLOOR_MAX_ABOVE_M = 1.35;
	static const float INTERIOR_INBOUND_M = 1.7;
	static const int INTERIOR_WALL_BLOCKED_MIN = 5;
	static const float DROP_LZ_RAISE_M = 30.0;
	static const float DROP_LZ_DOWN_M = 40.0;
	static const float DROP_LZ_SEARCH_R = 40.0;
	static const float DROP_LZ_SEARCH_WIDE_R = 80.0;
	static const int DROP_LZ_SAMPLE_TRIES = 8;
	static const float DROP_WAVE_SEPARATION_M = 80.0;
	static const string NAVMESH_PROJECT = "Soldiers";
	static const float FLAT_SITE_STRICT_SLOPE = 0.12;
	static const float FLAT_SITE_RELAXED_SLOPE = 0.25;
	static const float FLAT_SITE_SLOPE_SAMPLE_M = 10.0;
	static const float FLAT_SITE_FOOTPRINT_M = 12.0;
	static const float FLAT_SITE_STRICT_HEIGHT_M = 2.5;
	static const float FLAT_SITE_RELAXED_HEIGHT_M = 4.0;
	static const float FLAT_SITE_NEAR_BEST_SLOPE = 0.04;
	static const int FLAT_SITE_ATTEMPTS = 80;
	static const float FLAT_SITE_PLAYER_MIN_M = 200.0;

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
			vector worldPos = worldTm[3];
			if (worldPos == vector.Zero)
				continue;

			positions.Insert(worldPos);
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
			vector playerPos = players[i];
			if (playerPos == vector.Zero)
				continue;

			float dx = pos[0] - playerPos[0];
			float dz = pos[2] - playerPos[2];
			if ((dx * dx + dz * dz) <= radiusSq)
				return true;
		}

		return false;
	}

	//! Occupying origin on road or Soldiers navmesh inside the occupying budget.
	//! Requested scatter / marker is the search anchor, not a spawn pose.
	static bool ResolveOccupyingSpawn(vector requested, vector center, bool wasExact, out vector outPos, out bool outExact)
	{
		outPos = vector.Zero;
		outExact = false;

		vector anchor = center;
		if (requested != vector.Zero)
			anchor = requested;
		if (anchor == vector.Zero)
			return false;

		outPos = FindOccupyingInfantryOrigin(anchor, -1);
		if (outPos == vector.Zero)
			return false;

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
			vector playerPos = players[i];
			if (playerPos == vector.Zero)
				continue;

			float dx = pos[0] - playerPos[0];
			float dz = pos[2] - playerPos[2];
			float dsq = (dx * dx) + (dz * dz);
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

	//! Large horizontal footprint plus height: hangar, warehouse, monument.
	//! Trees are skipped before this runs.
	static bool IsLargeShellEntity(IEntity ent)
	{
		if (!ent)
			return false;

		vector mins;
		vector maxs;
		ent.GetWorldBounds(mins, maxs);
		float sx = maxs[0] - mins[0];
		float sz = maxs[2] - mins[2];
		float sy = maxs[1] - mins[1];
		if (sx <= SHELL_XZ_MIN_M)
			return false;
		if (sz <= SHELL_XZ_MIN_M)
			return false;
		if (sy <= SHELL_HEIGHT_MIN_M)
			return false;

		return true;
	}

	//! First building-like shell from well above must be the stand floor.
	//! Trees and small props are skipped so forests and streets stay valid.
	//! HasOpenSky stays 12 m for garrison; do not reuse this there.
	static bool IsOutdoorStandPose(vector pos)
	{
		if (pos == vector.Zero)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float startY = pos[1] + SHELL_COLUMN_M;
		float endY = pos[1] + 0.17;
		float span = startY - endY;
		if (span < 1.0)
			return true;

		ref TraceParam down = new TraceParam();
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ref array<IEntity> skipped = new array<IEntity>();
		down.ExcludeArray = skipped;

		int skipCount;
		for (skipCount = 0; skipCount <= SHELL_SKIP_MAX; skipCount++)
		{
			down.Start = Vector(pos[0], startY, pos[2]);
			down.End = Vector(pos[0], endY, pos[2]);
			float coef = world.TraceMove(down, null);
			if (coef >= 1.0)
				return true;

			float hitY = startY - (coef * span);
			if (hitY <= pos[1] + SHELL_FLOOR_EPS_M)
				return true;

			IEntity hitEnt = down.TraceEnt;
			if (!hitEnt)
			{
				if ((hitY - pos[1]) > SHELL_WORLD_COVER_M)
					return false;

				return true;
			}

			if (hitEnt.IsInherited(BaseTree))
			{
				skipped.Insert(hitEnt);
				continue;
			}

			if (Building.Cast(hitEnt))
				return false;

			if (IsLargeShellEntity(hitEnt))
				return false;

			skipped.Insert(hitEnt);
		}

		return true;
	}

	static vector OutdoorOrOrigin(vector origin, vector candidate)
	{
		if (IsOutdoorStandPose(candidate))
			return candidate;

		return origin;
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

		if (!IsOutdoorStandPose(hit))
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

		if (!IsOutdoorStandPose(hit))
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

	static bool PassesSafeOriginDistance(vector pos, vector anchor, float minR, float maxR, array<vector> players, float playerMin, int sectorIndex)
	{
		if (pos == vector.Zero)
			return false;
		if (IsInOcean(pos))
			return false;

		float fromAnchor = vector.DistanceXZ(pos, anchor);
		if (fromAnchor < minR)
			return false;
		if (fromAnchor > maxR)
			return false;
		if (sectorIndex >= 0 && !IsInSector(pos, anchor, sectorIndex))
			return false;
		if (playerMin > 0.5 && IsNearAnyPlayer(pos, players, playerMin))
			return false;

		return true;
	}

	static bool PassesSafeOrigin(vector pos, vector anchor, float minR, float maxR, array<vector> players, float playerMin, int sectorIndex)
	{
		if (!PassesSafeOriginDistance(pos, anchor, minR, maxR, players, playerMin, sectorIndex))
			return false;
		if (!IsOutdoorStandPose(pos))
			return false;

		return true;
	}

	static bool TryNavmeshReachable(vector sample, out vector outPos)
	{
		outPos = vector.Zero;
		if (sample == vector.Zero)
			return false;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return false;

		aiWorld.RequestNavmeshLoad(sample);
		NavmeshWorldComponent navmesh = aiWorld.GetNavmeshWorldComponent(NAVMESH_PROJECT);
		if (!navmesh)
			return false;

		if (!navmesh.IsTileLoaded(sample) && !navmesh.IsTileRequested(sample))
			navmesh.LoadTileIn(sample);

		if (!navmesh.IsTileLoaded(sample) && !navmesh.IsTileValid(sample))
			return false;

		vector reachable = sample;
		if (!navmesh.GetReachablePoint(sample, SAFE_ORIGIN_REACH_M, reachable))
			return false;
		if (reachable == vector.Zero)
			return false;
		if (IsInOcean(reachable))
			return false;

		outPos = reachable;
		return true;
	}

	//! Road in [minR, maxR], else Soldiers GetReachablePoint in that ring. Never expands.
	static vector FindSafeInfantryOrigin(vector anchor, float minR, float maxR, float playerMin, int sectorIndex = -1)
	{
		if (anchor == vector.Zero)
			return vector.Zero;
		if (minR < 0)
			minR = 0;
		if (maxR < minR)
			maxR = minR;

		AIWorld preloadWorld = GetGame().GetAIWorld();
		if (preloadWorld)
			preloadWorld.RequestNavmeshLoad(anchor);

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		int enclosedRejects = 0;
		int roadGroup = IA_VehicleManager.GetActiveGroup();
		int sectorPass;
		for (sectorPass = 0; sectorPass < 2; sectorPass++)
		{
			int sector = sectorIndex;
			if (sectorPass == 1)
				sector = -1;

			int roadTry;
			for (roadTry = 0; roadTry < SAFE_ORIGIN_ROAD_TRIES; roadTry++)
			{
				vector road = IA_VehicleManager.FindRoadInAnnulus(anchor, minR, maxR, roadGroup);
				if (road == vector.Zero)
					break;
				if (!PassesSafeOriginDistance(road, anchor, minR, maxR, players, playerMin, sector))
					continue;
				if (!IsOutdoorStandPose(road))
				{
					enclosedRejects = enclosedRejects + 1;
					continue;
				}
				return road;
			}

			int meshTry;
			for (meshTry = 0; meshTry < SAFE_ORIGIN_MESH_TRIES; meshTry++)
			{
				vector sample = SamplePolar(anchor, minR, maxR, sector);
				if (sector >= 0 && !IsInSector(sample, anchor, sector))
					continue;

				vector reached;
				if (!TryNavmeshReachable(sample, reached))
					continue;
				if (!PassesSafeOriginDistance(reached, anchor, minR, maxR, players, playerMin, sector))
					continue;
				if (!IsOutdoorStandPose(reached))
				{
					enclosedRejects = enclosedRejects + 1;
					continue;
				}
				return reached;
			}

			if (sectorIndex < 0)
				break;
		}

		Print(string.Format("[IA][SpawnPlacement] miss safe origin anchor=%1 min=%2 max=%3 enclosed=%4", anchor.ToString(), minR, maxR, enclosedRejects), LogLevel.WARNING);
		return vector.Zero;
	}

	static vector FindOccupyingInfantryOrigin(vector center, int sectorIndex = -1)
	{
		return FindSafeInfantryOrigin(center, OCCUPY_MIN_M, OCCUPY_MAX_M, PLAYER_MIN_M, sectorIndex);
	}

	static vector FindHoldInfantryOrigin(vector holdPos)
	{
		if (holdPos == vector.Zero)
			return vector.Zero;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		float playerMin = 0;
		if (IsNearAnyPlayer(holdPos, players, PLAYER_MIN_M))
			playerMin = PLAYER_MIN_M;

		return FindSafeInfantryOrigin(holdPos, HOLD_ORIGIN_MIN_M, HOLD_ORIGIN_MAX_M, playerMin, -1);
	}

	//! Closest outdoor walkable point next to a building hold. Wait waypoints at
	//! interior posts are not on Soldiers navmesh, so Hold groups must Move here
	//! first and only then enter.
	static vector FindHoldApproach(vector holdPos)
	{
		if (holdPos == vector.Zero)
			return vector.Zero;

		float radius = HOLD_APPROACH_MIN_M;
		while (radius <= HOLD_APPROACH_MAX_M)
		{
			int steps = 8;
			if (radius > 12)
				steps = 12;

			vector best = vector.Zero;
			float bestDist = 999999;
			int step;
			for (step = 0; step < steps; step++)
			{
				float stepF = step;
				float stepsF = steps;
				float angle = Math.PI2 * (stepF / stepsF);
				vector probe;
				probe[0] = holdPos[0] + Math.Cos(angle) * radius;
				probe[2] = holdPos[2] + Math.Sin(angle) * radius;
				probe[1] = holdPos[1];

				vector hit;
				if (!TryWalkableAt(probe, hit))
					continue;
				if (!IsOutdoorStandPose(hit))
					continue;

				vector reached;
				if (TryNavmeshReachable(hit, reached))
				{
					if (IsOutdoorStandPose(reached))
						hit = reached;
				}

				float dist = vector.Distance(hit, holdPos);
				if (dist < bestDist)
				{
					bestDist = dist;
					best = hit;
				}
			}

			if (best != vector.Zero)
				return best;

			radius = radius + 4;
		}

		return vector.Zero;
	}

	static vector FindReinforcementInfantryOrigin(vector fightPos, int sectorIndex = -1)
	{
		return FindSafeInfantryOrigin(fightPos, REINF_MIN_M, REINF_MAX_M, REINF_PLAYER_MIN_M, sectorIndex);
	}

	//! Defense / radio-tower / side-objective waves. Players already hold the
	//! objective, so the 80-180 m reinforcement ring lands on them. Stay at least
	//! PLAYER_MIN outside the flag and every living pawn; fail closed if none.
	static vector FindDefendWaveInfantryOrigin(vector fightPos, int sectorIndex = -1)
	{
		if (fightPos == vector.Zero)
			return vector.Zero;

		vector found = FindSafeInfantryOrigin(fightPos, DEFEND_WAVE_MIN_M, DEFEND_WAVE_MAX_M, DEFEND_WAVE_PLAYER_MIN_M, sectorIndex);
		if (found != vector.Zero)
			return found;

		found = FindSafeInfantryOrigin(fightPos, DEFEND_WAVE_MIN_M, DEFEND_WAVE_MAX_M, DEFEND_WAVE_PLAYER_MIN_M, -1);
		if (found != vector.Zero)
			return found;

		found = FindSafeInfantryOrigin(fightPos, DEFEND_WAVE_MIN_M, HARD_CAP_FROM_CENTER_M, DEFEND_WAVE_PLAYER_MIN_M, -1);
		if (found != vector.Zero)
			return found;

		found = FindSafeInfantryOrigin(fightPos, CENTER_MIN_M, HARD_CAP_FROM_CENTER_M, DEFEND_WAVE_PLAYER_MIN_M, -1);
		if (found != vector.Zero)
			return found;

		Print(string.Format("[IA][SpawnPlacement] miss defend wave origin anchor=%1", fightPos.ToString()), LogLevel.WARNING);
		return vector.Zero;
	}

	static vector FindInboundInfantrySpawn(vector center, int sectorIndex)
	{
		return FindOccupyingInfantryOrigin(center, sectorIndex);
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
			if (!IsOutdoorStandPose(roadPos))
				continue;

			outPos = roadPos;
			return true;
		}
		return false;
	}

	//! Soldier-sized pocket: inbound traces must reach the point. Starting inside
	//! a thick wall makes short outbound traces look empty (they never hit).
	//! Eight directions so a long wall does not look like a corridor (only two
	//! faces blocked, the along-wall rays never leave the solid).
	static bool HasInteriorBodyClearance(vector floorPos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector chest = floorPos + Vector(0, 1.0, 0);
		float inbound = INTERIOR_INBOUND_M;
		float diag = inbound * 0.7071;
		int blocked = 0;

		ref TraceParam t = new TraceParam();
		t.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		t.Start = chest + Vector(inbound, 0, 0);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(-inbound, 0, 0);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(0, 0, inbound);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(0, 0, -inbound);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(diag, 0, diag);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(-diag, 0, diag);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(diag, 0, -diag);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		t.Start = chest + Vector(-diag, 0, -diag);
		t.End = chest;
		if (world.TraceMove(t, null) < 1.0)
			blocked = blocked + 1;

		if (blocked >= INTERIOR_WALL_BLOCKED_MIN)
			return false;
		return true;
	}

	//! Ground-floor interior only. Probe starts below typical attic height so the
	//! first surface is the walkable floor, not a sealed loft.
	static bool TryGroundFloorStandPos(float x, float z, out vector outPos)
	{
		outPos = vector.Zero;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float terrainY = world.GetSurfaceY(x, z);
		float startY = terrainY + GROUND_FLOOR_PROBE_M;
		float endY = terrainY - 0.4;

		vector start = Vector(x, startY, z);
		vector end = Vector(x, endY, z);

		ref TraceParam down = new TraceParam();
		down.Start = start;
		down.End = end;
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float coef = world.TraceMove(down, null);
		if (coef >= 1.0)
			return false;
		if (coef < 0.04)
			return false;

		float span = startY - endY;
		vector floorPos = start;
		floorPos[1] = startY - (coef * span) + SURFACE_BIAS_M;

		float aboveTerrain = floorPos[1] - terrainY;
		if (aboveTerrain < -0.15)
			return false;
		if (aboveTerrain > GROUND_FLOOR_MAX_ABOVE_M)
			return false;
		if (!HasStandRoom(floorPos))
			return false;
		if (HasOpenSky(floorPos))
			return false;
		if (!HasInteriorBodyClearance(floorPos))
			return false;

		outPos = floorPos;
		return true;
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

	//------------------------------------------------------------------------------------------------
	//! Walkable site in an annulus, kept off players. Used by Enhanced defense events.
	static vector FindEventSite(vector center, float minR, float maxR)
	{
		if (center == vector.Zero)
			return vector.Zero;
		if (minR < 80)
			minR = 80;
		if (maxR < minR + 20)
			maxR = minR + 80;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		int attempt;
		for (attempt = 0; attempt < 16; attempt++)
		{
			float angle = IA_Game.rng.RandFloat01() * Math.PI2;
			float dist = IA_Game.rng.RandFloatXY(minR, maxR);
			vector probe;
			probe[0] = center[0] + Math.Cos(angle) * dist;
			probe[2] = center[2] + Math.Sin(angle) * dist;
			probe[1] = center[1];

			vector hit;
			if (!TryWalkableAt(probe, hit))
				continue;
			if (IsNearAnyPlayer(hit, players, 200))
				continue;
			return hit;
		}

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Terrain slope as rise/run over `sampleDist` along X and Z. Same method as
	//! IA_MortarPitPlacer — tents and pits need this, not just walkable ground.
	static float GetSlopeTangent(vector pos, float sampleDist)
	{
		if (sampleDist < 1.0)
			sampleDist = FLAT_SITE_SLOPE_SAMPLE_M;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 999.0;

		float y1 = world.GetSurfaceY(pos[0] + sampleDist, pos[2]);
		float y2 = world.GetSurfaceY(pos[0] - sampleDist, pos[2]);
		float y3 = world.GetSurfaceY(pos[0], pos[2] + sampleDist);
		float y4 = world.GetSurfaceY(pos[0], pos[2] - sampleDist);

		float diff1 = y1 - y2;
		if (diff1 < 0)
			diff1 = -diff1;
		float diff2 = y3 - y4;
		if (diff2 < 0)
			diff2 = -diff2;

		float slope1 = diff1 / (2.0 * sampleDist);
		float slope2 = diff2 / (2.0 * sampleDist);
		if (slope1 > slope2)
			return slope1;
		return slope2;
	}

	//------------------------------------------------------------------------------------------------
	//! Max terrain-Y spread across a circle. Catches a hillside that a single
	//! center slope sample can under-read for a tent/sandbag cluster.
	static float GetFootprintHeightDelta(vector pos, float radius)
	{
		if (radius < 1.0)
			radius = FLAT_SITE_FOOTPRINT_M;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 999.0;

		float y0 = world.GetSurfaceY(pos[0], pos[2]);
		float yMin = y0;
		float yMax = y0;
		int i;
		for (i = 0; i < 8; i++)
		{
			float iF = i;
			float ang = Math.PI2 * (iF / 8.0);
			float y = world.GetSurfaceY(pos[0] + Math.Cos(ang) * radius, pos[2] + Math.Sin(ang) * radius);
			if (y < yMin)
				yMin = y;
			if (y > yMax)
				yMax = y;
		}

		return yMax - yMin;
	}

	//------------------------------------------------------------------------------------------------
	//! Walkable annulus site that also has to be reasonably flat. Commander FOB
	//! tents sit on GetSurfaceY — a mountain-side walkable probe still looks wrong.
	static vector FindFlatEventSite(vector center, float minR, float maxR)
	{
		if (center == vector.Zero)
			return vector.Zero;
		if (minR < 80)
			minR = 80;
		if (maxR < minR + 20)
			maxR = minR + 80;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		ref array<vector> candidates = new array<vector>();
		CollectFlatEventCandidates(center, minR, maxR, players, candidates);

		vector chosen = vector.Zero;
		if (TryPickFlatEventSite(candidates, FLAT_SITE_STRICT_SLOPE, FLAT_SITE_STRICT_HEIGHT_M, chosen))
			return chosen;

		float wideMin = minR - 50.0;
		float wideMax = maxR + 100.0;
		if (wideMin < 80)
			wideMin = 80;
		if (wideMax < wideMin + 20)
			wideMax = wideMin + 80;
		CollectFlatEventCandidates(center, wideMin, wideMax, players, candidates);

		if (TryPickFlatEventSite(candidates, FLAT_SITE_STRICT_SLOPE, FLAT_SITE_STRICT_HEIGHT_M, chosen))
			return chosen;
		if (TryPickFlatEventSite(candidates, FLAT_SITE_RELAXED_SLOPE, FLAT_SITE_RELAXED_HEIGHT_M, chosen))
		{
			if (IA_Log.IsDebugEnabled())
			{
				Print("[IA][SpawnPlacement] flat event site used relaxed slope.", LogLevel.NORMAL);
			}
			return chosen;
		}

		if (candidates.IsEmpty())
			return vector.Zero;

		Print("[IA][SpawnPlacement] flat event site using flattest fallback.", LogLevel.WARNING);
		return PickFlattestEventSite(candidates);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectFlatEventCandidates(vector center, float minR, float maxR, array<vector> players, notnull array<vector> outCandidates)
	{
		int attempt;
		for (attempt = 0; attempt < FLAT_SITE_ATTEMPTS; attempt++)
		{
			float angle = IA_Game.rng.RandFloat01() * Math.PI2;
			float dist = IA_Game.rng.RandFloatXY(minR, maxR);
			vector probe;
			probe[0] = center[0] + Math.Cos(angle) * dist;
			probe[2] = center[2] + Math.Sin(angle) * dist;
			probe[1] = center[1];

			vector hit;
			if (!TryWalkableAt(probe, hit))
				continue;
			if (IsNearAnyPlayer(hit, players, FLAT_SITE_PLAYER_MIN_M))
				continue;
			if (IsInOcean(hit))
				continue;

			outCandidates.Insert(hit);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TryPickFlatEventSite(notnull array<vector> candidates, float maxSlope, float maxHeightDelta, out vector chosen)
	{
		chosen = vector.Zero;
		ref array<vector> passers = new array<vector>();
		foreach (vector sample : candidates)
		{
			if (GetSlopeTangent(sample, FLAT_SITE_SLOPE_SAMPLE_M) > maxSlope)
				continue;
			if (GetFootprintHeightDelta(sample, FLAT_SITE_FOOTPRINT_M) > maxHeightDelta)
				continue;
			passers.Insert(sample);
		}

		if (passers.IsEmpty())
			return false;

		chosen = PickFlattestEventSiteNearBest(passers);
		return chosen != vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector PickFlattestEventSite(notnull array<vector> pool)
	{
		int n = pool.Count();
		if (n <= 0)
			return vector.Zero;
		if (n == 1)
			return pool[0];

		vector best = pool[0];
		float bestScore = 999.0;
		int i;
		for (i = 0; i < n; i++)
		{
			float score = ScoreFlatEventSite(pool[i]);
			if (score < bestScore)
			{
				bestScore = score;
				best = pool[i];
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector PickFlattestEventSiteNearBest(notnull array<vector> pool)
	{
		int n = pool.Count();
		if (n <= 0)
			return vector.Zero;
		if (n == 1)
			return pool[0];

		float bestScore = 999.0;
		int i;
		for (i = 0; i < n; i++)
		{
			float score = ScoreFlatEventSite(pool[i]);
			if (score < bestScore)
				bestScore = score;
		}

		ref array<vector> nearBest = new array<vector>();
		for (i = 0; i < n; i++)
		{
			float score = ScoreFlatEventSite(pool[i]);
			if (score <= bestScore + FLAT_SITE_NEAR_BEST_SLOPE)
				nearBest.Insert(pool[i]);
		}

		int nearCount = nearBest.Count();
		if (nearCount <= 0)
			return pool[0];
		if (nearCount == 1)
			return nearBest[0];
		return nearBest[IA_Game.rng.RandInt(0, nearCount)];
	}

	//------------------------------------------------------------------------------------------------
	protected static float ScoreFlatEventSite(vector pos)
	{
		float slope = GetSlopeTangent(pos, FLAT_SITE_SLOPE_SAMPLE_M);
		float heightDelta = GetFootprintHeightDelta(pos, FLAT_SITE_FOOTPRINT_M);
		return slope + (heightDelta * 0.02);
	}

	//------------------------------------------------------------------------------------------------
	//! Random open-sky sample in an annulus. Does not spiral from `center` — that
	//! search is deterministic and stacked every Air Assault wave on one pad.
	static bool TrySampleAnnulusDropLz(vector center, float minR, float maxR, int attempts, array<vector> players, float playerR, array<vector> avoidLzs, float avoidR, out vector outLz)
	{
		outLz = vector.Zero;
		if (center == vector.Zero)
			return false;
		if (attempts < 1)
			return false;
		if (maxR < minR)
			maxR = minR;

		int attempt;
		for (attempt = 0; attempt < attempts; attempt++)
		{
			float angle = IA_Game.rng.RandFloat01() * Math.PI2;
			float dist = IA_Game.rng.RandFloatXY(minR, maxR);
			vector probe;
			probe[0] = center[0] + Math.Cos(angle) * dist;
			probe[2] = center[2] + Math.Sin(angle) * dist;
			probe[1] = center[1];

			vector lz;
			if (!TryFindDropLz(probe, DROP_LZ_SEARCH_WIDE_R, lz))
				continue;
			if (IsNearAnyPlayer(lz, players, playerR))
				continue;
			if (avoidR > 0.5 && IsNearAnyPlayer(lz, avoidLzs, avoidR))
				continue;
			outLz = lz;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Air Assault LZ: hot drop stays off living pawns (150 m), otherwise 200-400 m
	//! perimeter with a 200 m player floor. Never falls back onto the defend point.
	//! `avoidLzs` keeps later waves off a pad already used this defend.
	static bool TryFindDefendDropLz(vector center, bool preferHotDrop, out vector outLz, array<vector> avoidLzs = null)
	{
		outLz = vector.Zero;
		if (center == vector.Zero)
			return false;

		ref array<vector> players = new array<vector>();
		CollectPlayerPositions(players);

		if (preferHotDrop)
		{
			if (TrySampleAnnulusDropLz(center, 80, 160, 16, players, DEFEND_DROP_HOT_PLAYER_MIN_M, avoidLzs, DROP_WAVE_SEPARATION_M, outLz))
				return true;
			if (TrySampleAnnulusDropLz(center, 60, 200, 10, players, DEFEND_DROP_HOT_PLAYER_MIN_M, avoidLzs, 40, outLz))
				return true;
		}

		if (TrySampleAnnulusDropLz(center, 200, 400, 24, players, DEFEND_DROP_PLAYER_MIN_M, avoidLzs, DROP_WAVE_SEPARATION_M, outLz))
			return true;
		if (TrySampleAnnulusDropLz(center, 180, 450, 16, players, DEFEND_DROP_PLAYER_MIN_M, avoidLzs, 40, outLz))
			return true;
		if (TrySampleAnnulusDropLz(center, 160, 500, 16, players, DEFEND_DROP_PLAYER_MIN_M, null, 0, outLz))
			return true;

		return false;
	}
}
