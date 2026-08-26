// IA_BuildingHoldFinder.c
// Picks a few interior standing positions per objective so dedicated Hold
// groups can spawn in buildings. Does not retask occupying patrols.

class IA_BuildingHoldSpot
{
	vector m_pos;
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
			if (IA_GmHoldPost.HasHoldNear(spot.m_pos, MIN_SEP_M))
				continue;
			if (IsNearExistingSpot(spot.m_pos, outSpots, MIN_SEP_M))
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

		vector pos;
		if (!TryCoverPostInBuilding(mins, maxs, coverPosts, pos))
		{
			if (!TrySampleInterior(mins, maxs, pos))
				return null;
		}

		ref IA_BuildingHoldSpot spot = new IA_BuildingHoldSpot();
		spot.m_pos = pos;
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
			if (vector.DistanceSq(spot.m_pos, pos) <= distSq)
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

			ref IA_BuildingHoldSpot spot = new IA_BuildingHoldSpot();
			spot.m_pos = post;
			spot.m_radius = IA_GmHoldPost.DEFAULT_RADIUS;
			outSpots.Insert(spot);
		}
	}
}
