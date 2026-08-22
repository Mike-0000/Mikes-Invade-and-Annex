///////////////////////////////////////////////////////////////////////
// IA_MortarPitPlacer - auto-place a MortarPit marker for AO groups
// that do not already have a map-authored one.
///////////////////////////////////////////////////////////////////////
class IA_MortarPitPlacer
{
	protected static const float BUFFER_FROM_SITES_M = 100.0;
	protected static const float RING_INNER_EXTRA_M = 250.0;
	protected static const float RING_OUTER_EXTRA_M = 900.0;
	protected static const float RING_WIDEN_EXTRA_M = 400.0;
	protected static const float STRICT_SLOPE = 0.12;
	protected static const float RELAXED_SLOPE = 0.25;
	protected static const float SLOPE_SAMPLE_DIST = 8.0;
	protected static const float OCCUPANCY_RADIUS = 6.0;
	protected static const float DEFAULT_PIT_RADIUS = 40.0;
	protected static const int MORTAR_GRID_MIN = 2;
	protected static const int MORTAR_GRID_MAX = 4;
	protected static const int SAMPLE_COUNT = 48;

	//----------------------------------------------------------------------------------------------
	static void EnsureForGroup(int groupNumber)
	{
		if (!Replication.IsServer())
			return;

		if (IA_AreaMarker.HasMortarPitMarkerForGroup(groupNumber))
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 already has a MortarPit marker.", groupNumber), LogLevel.NORMAL);
			return;
		}

		array<IA_AreaMarker> sites = CollectNonMortarSites(groupNumber);
		if (sites.IsEmpty())
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 has no sites to anchor placement. Skipping.", groupNumber), LogLevel.WARNING);
			return;
		}

		vector groupCenter;
		float footprint;
		ComputeGroupFootprint(sites, groupCenter, footprint);
		if (groupCenter == vector.Zero)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 footprint center invalid. Skipping.", groupNumber), LogLevel.WARNING);
			return;
		}

		vector chosen = vector.Zero;
		bool found = TryFindPosition(groupCenter, footprint, RING_INNER_EXTRA_M, RING_OUTER_EXTRA_M, STRICT_SLOPE, chosen);
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 strict search failed, widening ring.", groupNumber), LogLevel.WARNING);
			found = TryFindPosition(groupCenter, footprint, RING_INNER_EXTRA_M, RING_OUTER_EXTRA_M + RING_WIDEN_EXTRA_M, STRICT_SLOPE, chosen);
		}
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 widened search failed, relaxing slope.", groupNumber), LogLevel.WARNING);
			found = TryFindPosition(groupCenter, footprint, RING_INNER_EXTRA_M, RING_OUTER_EXTRA_M + RING_WIDEN_EXTRA_M, RELAXED_SLOPE, chosen);
		}
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 all filters failed, using flattest fallback.", groupNumber), LogLevel.WARNING);
			chosen = FindFlattestFallback(groupCenter, footprint + RING_INNER_EXTRA_M, footprint + RING_OUTER_EXTRA_M + RING_WIDEN_EXTRA_M);
		}

		if (chosen == vector.Zero)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 could not find any position.", groupNumber), LogLevel.ERROR);
			return;
		}

		chosen[1] = GetGame().GetWorld().GetSurfaceY(chosen[0], chosen[2]);

		vector floorPos = chosen;
		TraceSphere traceParam = new TraceSphere();
		traceParam.Radius = 2.0;
		traceParam.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		traceParam.LayerMask = EPhysicsLayerDefs.Projectile;
		bool nudged = SCR_EmptyPositionHelper.TryFindNearbyFloorPosition(
			GetGame().GetWorld(), chosen, traceParam, 3.0, 4.0, 40.0, false, 0.0, floorPos);
		if (nudged)
			chosen = floorPos;

		SpawnRuntimeMarker(groupNumber, chosen);
	}

	//----------------------------------------------------------------------------------------------
	protected static array<IA_AreaMarker> CollectNonMortarSites(int groupNumber)
	{
		array<IA_AreaMarker> result = {};
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersByGroup(groupNumber);
		foreach (IA_AreaMarker marker : markers)
		{
			if (!marker)
				continue;
			IA_AreaType t = marker.GetAreaType();
			if (t == IA_AreaType.DefendObjective)
				continue;
			if (t == IA_AreaType.MortarPit)
				continue;
			result.Insert(marker);
		}
		return result;
	}

	//----------------------------------------------------------------------------------------------
	protected static void ComputeGroupFootprint(array<IA_AreaMarker> sites, out vector center, out float footprint)
	{
		center = vector.Zero;
		footprint = 0;
		if (!sites || sites.IsEmpty())
			return;

		vector sum = vector.Zero;
		int count = 0;
		foreach (IA_AreaMarker marker : sites)
		{
			if (!marker)
				continue;
			sum = sum + marker.GetOrigin();
			count = count + 1;
		}
		if (count <= 0)
			return;

		center = sum / count;

		float maxFoot = 0;
		foreach (IA_AreaMarker marker2 : sites)
		{
			if (!marker2)
				continue;
			float dist = vector.Distance(center, marker2.GetOrigin()) + marker2.GetRadius();
			if (dist > maxFoot)
				maxFoot = dist;
		}
		footprint = maxFoot;
	}

	//----------------------------------------------------------------------------------------------
	protected static bool TryFindPosition(vector groupCenter, float footprint, float innerExtra, float outerExtra, float maxSlope, out vector chosen)
	{
		chosen = vector.Zero;
		ref array<vector> candidates = new array<vector>();
		float innerR = footprint + innerExtra;
		float outerR = footprint + outerExtra;
		if (innerR < 50)
			innerR = 50;
		if (outerR <= innerR)
			outerR = innerR + 200;

		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			vector sample = IA_Game.rng.GenerateRandomPointInRadius(innerR, outerR, groupCenter);
			sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);

			if (!PassesFilters(sample, maxSlope))
				continue;

			candidates.Insert(sample);
		}

		if (candidates.IsEmpty())
			return false;

		ref array<vector> elevated = new array<vector>();
		foreach (vector c : candidates)
		{
			if (c[1] >= groupCenter[1])
				elevated.Insert(c);
		}

		ref array<vector> pool = candidates;
		if (!elevated.IsEmpty())
			pool = elevated;

		int idx = IA_Game.rng.RandInt(0, pool.Count());
		chosen = pool[idx];
		return true;
	}

	//----------------------------------------------------------------------------------------------
	protected static bool PassesFilters(vector pos, float maxSlope)
	{
		if (IsUnderOcean(pos))
			return false;
		if (IsTooCloseToAnySite(pos))
			return false;
		if (GetSlopeTangent(pos) > maxSlope)
			return false;
		if (IsOccupied(pos))
			return false;
		return true;
	}

	//----------------------------------------------------------------------------------------------
	protected static bool IsUnderOcean(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		float oceanY = world.GetOceanHeight(pos[0], pos[2]);
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (surfaceY <= oceanY + 0.5)
			return true;
		return false;
	}

	//----------------------------------------------------------------------------------------------
	protected static bool IsTooCloseToAnySite(vector pos)
	{
		array<IA_AreaMarker> all = IA_AreaMarker.GetAllMarkers();
		foreach (IA_AreaMarker marker : all)
		{
			if (!marker)
				continue;
			float minDist = marker.GetRadius() + BUFFER_FROM_SITES_M;
			if (vector.Distance(pos, marker.GetOrigin()) < minDist)
				return true;
		}
		return false;
	}

	//----------------------------------------------------------------------------------------------
	protected static float GetSlopeTangent(vector pos)
	{
		float distCheck = SLOPE_SAMPLE_DIST;
		float y1 = GetGame().GetWorld().GetSurfaceY(pos[0] + distCheck, pos[2]);
		float y2 = GetGame().GetWorld().GetSurfaceY(pos[0] - distCheck, pos[2]);
		float y3 = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2] + distCheck);
		float y4 = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2] - distCheck);

		float diff1 = y1 - y2;
		if (diff1 < 0)
			diff1 = -diff1;
		float diff2 = y3 - y4;
		if (diff2 < 0)
			diff2 = -diff2;

		float slope1 = diff1 / (2.0 * distCheck);
		float slope2 = diff2 / (2.0 * distCheck);
		if (slope1 > slope2)
			return slope1;
		return slope2;
	}

	//----------------------------------------------------------------------------------------------
	protected static bool IsOccupied(vector pos)
	{
		TraceSphere trace = new TraceSphere();
		trace.Radius = OCCUPANCY_RADIUS;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Start = pos + "0 0.5 0";
		trace.End = pos + "0 3 0";

		float hit = GetGame().GetWorld().TraceMove(trace, null);
		if (hit < 1.0)
			return true;
		return false;
	}

	//----------------------------------------------------------------------------------------------
	protected static vector FindFlattestFallback(vector groupCenter, float innerR, float outerR)
	{
		vector best = vector.Zero;
		float bestSlope = 999.0;
		for (int i = 0; i < SAMPLE_COUNT; i++)
		{
			vector sample = IA_Game.rng.GenerateRandomPointInRadius(innerR, outerR, groupCenter);
			sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);
			if (IsUnderOcean(sample))
				continue;
			if (IsTooCloseToAnySite(sample))
				continue;

			float slope = GetSlopeTangent(sample);
			if (slope < bestSlope)
			{
				bestSlope = slope;
				best = sample;
			}
		}
		return best;
	}

	//----------------------------------------------------------------------------------------------
	protected static void SpawnRuntimeMarker(int groupNumber, vector pos)
	{
		// Spawn base Area_Marker; ConfigureRuntime sets MortarPit type/group/name.
		// Prefer typed prefab path when Workbench has registered it.
		Resource res = Resource.Load("Prefabs/IA_AreaMarkers/IA_MortarPit.et");
		if (!res)
			res = Resource.Load("{61B9AD559D2CE12D}Components/Area_Marker.et");

		if (!res)
		{
			Print("[IA_MortarPitPlacer] Failed to load IA_MortarPit / Area_Marker prefab.", LogLevel.ERROR);
			return;
		}

		IEntity ent = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(pos));
		IA_AreaMarker marker = IA_AreaMarker.Cast(ent);
		if (!marker)
		{
			Print("[IA_MortarPitPlacer] Spawned entity is not IA_AreaMarker.", LogLevel.ERROR);
			if (ent)
				IA_Game.AddEntityToGc(ent);
			return;
		}

		string name = string.Format("Mortar Pit %1", groupNumber);
		int mortarCount = IA_Game.rng.RandInt(MORTAR_GRID_MIN, MORTAR_GRID_MAX + 1);
		float radius = DEFAULT_PIT_RADIUS + (mortarCount * 4.0);
		marker.SetMortarCount(mortarCount);
		marker.ConfigureRuntime(groupNumber, name, radius);
		Print(string.Format("[IA_MortarPitPlacer] Auto-placed MortarPit for group %1 at %2 with %3 guns", groupNumber, pos, mortarCount), LogLevel.NORMAL);
	}
};
