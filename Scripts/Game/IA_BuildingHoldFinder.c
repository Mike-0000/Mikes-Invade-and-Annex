// IA_BuildingHoldFinder.c
// Picks a few interior standing positions per objective so dedicated Hold
// groups can spawn in buildings. Does not retask occupying patrols.

class IA_BuildingHoldSpot
{
	vector m_holdPos;
	vector m_spawnPos;
	float m_radius;
}

class IA_BuildingQueryCallback
{
	ref array<IEntity> m_Buildings;

	void IA_BuildingQueryCallback()
	{
		m_Buildings = new array<IEntity>();
	}

	bool FilterBuilding(IEntity e)
	{
		if (!e)
			return false;
		if (Building.Cast(e))
			return true;
		return false;
	}

	bool OnBuilding(IEntity e)
	{
		if (!m_Buildings)
			return false;
		if (m_Buildings.Count() >= IA_BuildingHoldFinder.MAX_QUERY)
			return false;
		m_Buildings.Insert(e);
		return true;
	}
}

class IA_BuildingHoldFinder
{
	static const int MAX_QUERY = 48;
	static const float MIN_FOOTPRINT_M = 5;
	static const float MIN_HEIGHT_M = 2.4;
	static const float MIN_SEP_M = 12;

	static int CountForAreaType(IA_AreaType areaType)
	{
		if (areaType == IA_AreaType.City)
			return 4;
		if (areaType == IA_AreaType.Town)
			return 3;
		if (areaType == IA_AreaType.Military)
			return 3;
		if (areaType == IA_AreaType.Docks)
			return 2;
		if (areaType == IA_AreaType.SmallMilitary)
			return 2;
		if (areaType == IA_AreaType.Property)
			return 2;
		if (areaType == IA_AreaType.Airport)
			return 2;
		if (areaType == IA_AreaType.DefendObjective)
			return 2;
		return 0;
	}

	static void FindInteriorSpots(vector center, float radius, int maxCount, notnull array<ref IA_BuildingHoldSpot> outSpots)
	{
		outSpots.Clear();
		if (maxCount <= 0)
			return;
		if (center == vector.Zero)
			return;
		if (radius <= 0)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		ref IA_BuildingQueryCallback query = new IA_BuildingQueryCallback();
		world.QueryEntitiesBySphere(center, radius, query.OnBuilding, query.FilterBuilding, EQueryEntitiesFlags.STATIC);

		ref array<vector> coverPosts = new array<vector>();
		IA_SpawnPlacement.FindGarrisonPosts(center, radius, coverPosts);

		ref array<IEntity> ranked = RankBuildings(query.m_Buildings, center, radius);
		int rankedCount = ranked.Count();
		int r;
		for (r = 0; r < rankedCount; r++)
		{
			if (outSpots.Count() >= maxCount)
				return;

			IEntity building = ranked[r];
			ref IA_BuildingHoldSpot spot = MakeSpotForBuilding(building, coverPosts);
			if (!spot)
				continue;
			if (IA_GmHoldPost.HasHoldNear(spot.m_holdPos, MIN_SEP_M))
				continue;
			if (IsNearExistingSpot(spot.m_holdPos, outSpots, MIN_SEP_M))
				continue;

			outSpots.Insert(spot);
		}

		if (outSpots.Count() >= maxCount)
			return;

		AddCoverPostFallbackSpots(coverPosts, maxCount, outSpots);
	}

	protected static ref array<IEntity> RankBuildings(array<IEntity> buildings, vector center, float radius)
	{
		ref array<IEntity> ranked = new array<IEntity>();
		if (!buildings)
			return ranked;

		ref array<float> areas = new array<float>();
		float radiusSq = radius * radius;
		int i;
		int count = buildings.Count();
		for (i = 0; i < count; i++)
		{
			IEntity building = buildings[i];
			if (!building)
				continue;

			vector mins;
			vector maxs;
			building.GetWorldBounds(mins, maxs);
			float w = maxs[0] - mins[0];
			float l = maxs[2] - mins[2];
			float h = maxs[1] - mins[1];
			if (w < MIN_FOOTPRINT_M)
				continue;
			if (l < MIN_FOOTPRINT_M)
				continue;
			if (h < MIN_HEIGHT_M)
				continue;

			vector mid;
			mid[0] = (mins[0] + maxs[0]) * 0.5;
			mid[1] = (mins[1] + maxs[1]) * 0.5;
			mid[2] = (mins[2] + maxs[2]) * 0.5;
			float dx = mid[0] - center[0];
			float dz = mid[2] - center[2];
			if ((dx * dx) + (dz * dz) > radiusSq)
				continue;

			float area = w * l;
			int insertAt = ranked.Count();
			int j;
			int rankedCount = ranked.Count();
			for (j = 0; j < rankedCount; j++)
			{
				if (area > areas[j])
				{
					insertAt = j;
					break;
				}
			}
			if (insertAt >= ranked.Count())
			{
				ranked.Insert(building);
				areas.Insert(area);
			}
			else
			{
				ranked.InsertAt(building, insertAt);
				areas.InsertAt(area, insertAt);
			}
		}

		return ranked;
	}

	protected static ref IA_BuildingHoldSpot MakeSpotForBuilding(IEntity building, array<vector> coverPosts)
	{
		if (!building)
			return null;

		vector mins;
		vector maxs;
		building.GetWorldBounds(mins, maxs);

		vector holdPos;
		if (!TryCoverPostInBuilding(mins, maxs, coverPosts, holdPos))
		{
			if (!TrySampleInterior(mins, maxs, holdPos))
				return null;
		}

		vector spawnPos;
		if (!TrySampleGroundSpawn(mins, maxs, spawnPos))
			return null;

		ref IA_BuildingHoldSpot spot = new IA_BuildingHoldSpot();
		spot.m_holdPos = holdPos;
		spot.m_spawnPos = spawnPos;
		spot.m_radius = RadiusFromBounds(mins, maxs);
		return spot;
	}

	protected static float RadiusFromBounds(vector mins, vector maxs)
	{
		float w = maxs[0] - mins[0];
		float l = maxs[2] - mins[2];
		float minSide = w;
		if (l < minSide)
			minSide = l;

		float radius = minSide * 0.45;
		if (radius < IA_GmHoldPost.MIN_RADIUS)
			radius = IA_GmHoldPost.DEFAULT_RADIUS;
		if (radius < 5)
			radius = 5;
		if (radius > 12)
			radius = 12;
		return radius;
	}

	protected static bool TryCoverPostInBuilding(vector mins, vector maxs, array<vector> coverPosts, out vector outPos)
	{
		outPos = vector.Zero;
		if (!coverPosts)
			return false;

		int i;
		int count = coverPosts.Count();
		for (i = 0; i < count; i++)
		{
			vector post = coverPosts[i];
			if (!PosInBuildingBounds(post, mins, maxs))
				continue;
			if (!IA_SpawnPlacement.HasStandRoom(post))
				continue;
			if (IA_SpawnPlacement.HasOpenSky(post))
				continue;
			outPos = post;
			return true;
		}
		return false;
	}

	protected static bool TrySampleInterior(vector mins, vector maxs, out vector outPos)
	{
		outPos = vector.Zero;
		float midX = (mins[0] + maxs[0]) * 0.5;
		float midZ = (mins[2] + maxs[2]) * 0.5;
		float insetX = (maxs[0] - mins[0]) * 0.22;
		float insetZ = (maxs[2] - mins[2]) * 0.22;
		if (insetX < 1.2)
			insetX = 1.2;
		if (insetZ < 1.2)
			insetZ = 1.2;

		if (IA_SpawnPlacement.TryInteriorStandPos(midX, midZ, maxs[1], mins[1], outPos))
			return true;
		if (IA_SpawnPlacement.TryInteriorStandPos(midX - insetX, midZ, maxs[1], mins[1], outPos))
			return true;
		if (IA_SpawnPlacement.TryInteriorStandPos(midX + insetX, midZ, maxs[1], mins[1], outPos))
			return true;
		if (IA_SpawnPlacement.TryInteriorStandPos(midX, midZ - insetZ, maxs[1], mins[1], outPos))
			return true;
		if (IA_SpawnPlacement.TryInteriorStandPos(midX, midZ + insetZ, maxs[1], mins[1], outPos))
			return true;
		return false;
	}

	protected static bool TrySampleGroundSpawn(vector mins, vector maxs, out vector outPos)
	{
		outPos = vector.Zero;
		float midX = (mins[0] + maxs[0]) * 0.5;
		float midZ = (mins[2] + maxs[2]) * 0.5;
		float insetX = (maxs[0] - mins[0]) * 0.32;
		float insetZ = (maxs[2] - mins[2]) * 0.32;
		if (insetX < 2.2)
			insetX = 2.2;
		if (insetZ < 2.2)
			insetZ = 2.2;

		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX, midZ, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX - insetX, midZ, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX + insetX, midZ, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX, midZ - insetZ, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX, midZ + insetZ, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX - insetX * 0.5, midZ - insetZ * 0.5, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX + insetX * 0.5, midZ - insetZ * 0.5, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX - insetX * 0.5, midZ + insetZ * 0.5, outPos))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(midX + insetX * 0.5, midZ + insetZ * 0.5, outPos))
			return true;
		return false;
	}

	static bool FindGroundSpawnForHold(vector holdPos, out vector outSpawn)
	{
		outSpawn = vector.Zero;
		if (holdPos == vector.Zero)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			ref IA_BuildingQueryCallback query = new IA_BuildingQueryCallback();
			world.QueryEntitiesBySphere(holdPos, 28, query.OnBuilding, query.FilterBuilding, EQueryEntitiesFlags.STATIC);
			if (query.m_Buildings && !query.m_Buildings.IsEmpty())
			{
				IEntity best = null;
				float bestArea = 0;
				IEntity nearest = null;
				float nearestDist = 999999;
				int i;
				int count = query.m_Buildings.Count();
				for (i = 0; i < count; i++)
				{
					IEntity building = query.m_Buildings[i];
					if (!building)
						continue;

					vector mins;
					vector maxs;
					building.GetWorldBounds(mins, maxs);
					if (PosInBuildingBounds(holdPos, mins, maxs))
					{
						float area = (maxs[0] - mins[0]) * (maxs[2] - mins[2]);
						if (!best)
						{
							best = building;
							bestArea = area;
						}
						else
						{
							if (area < bestArea)
							{
								best = building;
								bestArea = area;
							}
						}
					}

					float dist = vector.DistanceSq(building.GetOrigin(), holdPos);
					if (dist < nearestDist)
					{
						nearestDist = dist;
						nearest = building;
					}
				}

				if (!best)
					best = nearest;

				if (best)
				{
					vector mins;
					vector maxs;
					best.GetWorldBounds(mins, maxs);
					if (TrySampleGroundSpawn(mins, maxs, outSpawn))
						return true;
				}
			}
		}

		if (IA_SpawnPlacement.TryGroundFloorStandPos(holdPos[0], holdPos[2], outSpawn))
			return true;

		float ring = 1.5;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(holdPos[0] + ring, holdPos[2], outSpawn))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(holdPos[0] - ring, holdPos[2], outSpawn))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(holdPos[0], holdPos[2] + ring, outSpawn))
			return true;
		if (IA_SpawnPlacement.TryGroundFloorStandPos(holdPos[0], holdPos[2] - ring, outSpawn))
			return true;
		return false;
	}

	protected static bool PosInBuildingBounds(vector pos, vector mins, vector maxs)
	{
		if (pos[0] < mins[0] - 0.4 || pos[0] > maxs[0] + 0.4)
			return false;
		if (pos[2] < mins[2] - 0.4 || pos[2] > maxs[2] + 0.4)
			return false;
		if (pos[1] < mins[1] - 1 || pos[1] > maxs[1] + 1)
			return false;
		return true;
	}

	protected static bool IsNearExistingSpot(vector pos, array<ref IA_BuildingHoldSpot> spots, float distM)
	{
		if (!spots)
			return false;
		float distSq = distM * distM;
		int i;
		int count = spots.Count();
		for (i = 0; i < count; i++)
		{
			IA_BuildingHoldSpot spot = spots[i];
			if (!spot)
				continue;
			if (vector.DistanceSq(spot.m_holdPos, pos) <= distSq)
				return true;
		}
		return false;
	}

	protected static void AddCoverPostFallbackSpots(array<vector> coverPosts, int maxCount, notnull array<ref IA_BuildingHoldSpot> outSpots)
	{
		if (!coverPosts)
			return;

		int i;
		int count = coverPosts.Count();
		for (i = 0; i < count; i++)
		{
			if (outSpots.Count() >= maxCount)
				return;

			vector post = coverPosts[i];
			if (!IA_SpawnPlacement.HasStandRoom(post))
				continue;
			if (IA_SpawnPlacement.HasOpenSky(post))
				continue;
			if (IA_GmHoldPost.HasHoldNear(post, MIN_SEP_M))
				continue;
			if (IsNearExistingSpot(post, outSpots, MIN_SEP_M))
				continue;

			vector spawnPos;
			if (!FindGroundSpawnForHold(post, spawnPos))
				continue;

			ref IA_BuildingHoldSpot spot = new IA_BuildingHoldSpot();
			spot.m_holdPos = post;
			spot.m_spawnPos = spawnPos;
			spot.m_radius = IA_GmHoldPost.DEFAULT_RADIUS;
			outSpots.Insert(spot);
		}
	}
}
