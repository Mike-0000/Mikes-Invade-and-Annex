///////////////////////////////////////////////////////////////////////
// IA_MortarPitPlacer - auto-place a MortarPit marker for AO groups
// that do not already have a map-authored one.
///////////////////////////////////////////////////////////////////////
class IA_MortarPitPlacer
{
	protected static const float BUFFER_FROM_SITES_M = 100.0;
	protected static const float MAX_DIST_FROM_SITE_EDGE_M = 350.0;
	protected static const float MAX_DIST_WIDEN_M = 80.0;
	protected static const float STRICT_SLOPE = 0.12;
	protected static const float RELAXED_SLOPE = 0.25;
	protected static const float SLOPE_SAMPLE_DIST = 8.0;
	protected static const float OCCUPANCY_RADIUS = 6.0;
	protected static const float DEFAULT_PIT_RADIUS = 40.0;
	protected static const int MORTAR_GRID_MIN = 2;
	protected static const int MORTAR_GRID_MAX = 4;
	protected static const int SAMPLES_PER_SITE = 24;
	protected static const float NEAR_BIAS_SOFTEN_M = 60.0;
	protected static const float FALLBACK_DIST_SLOPE_PER_M = 0.0002;

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

		array<IA_AreaMarker> sites = CollectAnchorSites(groupNumber);
		if (sites.IsEmpty())
			sites = CollectNonMortarSites(groupNumber);
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
		bool found = TryFindPosition(groupCenter, MAX_DIST_FROM_SITE_EDGE_M, STRICT_SLOPE, sites, chosen);
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 strict search failed, widening to %2 m.", groupNumber, MAX_DIST_FROM_SITE_EDGE_M + MAX_DIST_WIDEN_M), LogLevel.WARNING);
			found = TryFindPosition(groupCenter, MAX_DIST_FROM_SITE_EDGE_M + MAX_DIST_WIDEN_M, STRICT_SLOPE, sites, chosen);
		}
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 widened search failed, relaxing slope.", groupNumber), LogLevel.WARNING);
			found = TryFindPosition(groupCenter, MAX_DIST_FROM_SITE_EDGE_M + MAX_DIST_WIDEN_M, RELAXED_SLOPE, sites, chosen);
		}
		if (!found)
		{
			Print(string.Format("[IA_MortarPitPlacer] Group %1 all filters failed, using flattest fallback.", groupNumber), LogLevel.WARNING);
			chosen = FindFlattestFallback(MAX_DIST_FROM_SITE_EDGE_M + MAX_DIST_WIDEN_M, sites);
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
		if (nudged && IsStillNearObjectives(floorPos, sites, MAX_DIST_FROM_SITE_EDGE_M + MAX_DIST_WIDEN_M))
		{
			if (!IsTooCloseToAnySite(floorPos) && !IsUnderOcean(floorPos))
				chosen = floorPos;
		}

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
	// Towns / military / etc. Radio towers are often far from the fight and must not pull the pit.
	protected static array<IA_AreaMarker> CollectAnchorSites(int groupNumber)
	{
		array<IA_AreaMarker> result = {};
		array<IA_AreaMarker> markers = CollectNonMortarSites(groupNumber);
		foreach (IA_AreaMarker marker : markers)
		{
			if (!marker)
				continue;
			if (marker.GetAreaType() == IA_AreaType.RadioTower)
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
	protected static bool TryFindPosition(vector groupCenter, float maxEdgeDist, float maxSlope, array<IA_AreaMarker> sites, out vector chosen)
	{
		chosen = vector.Zero;
		ref array<vector> candidates = new array<vector>();
		CollectSiteBandCandidates(sites, maxEdgeDist, maxSlope, true, candidates);
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

		chosen = PickCloserToObjectives(pool, sites);
		return true;
	}

	//----------------------------------------------------------------------------------------------
	protected static void CollectSiteBandCandidates(array<IA_AreaMarker> sites, float maxEdgeDist, float maxSlope, bool requireOccupancy, notnull array<vector> outCandidates)
	{
		if (!sites)
			return;

		foreach (IA_AreaMarker site : sites)
		{
			if (!site)
				continue;

			vector origin = site.GetOrigin();
			float innerR = site.GetRadius() + BUFFER_FROM_SITES_M;
			float outerR = site.GetRadius() + maxEdgeDist;
			if (outerR <= innerR)
				outerR = innerR + 20.0;

			for (int i = 0; i < SAMPLES_PER_SITE; i++)
			{
				vector sample = IA_Game.rng.GenerateRandomPointInRadius(innerR, outerR, origin, false);
				sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);

				if (!IsStillNearObjectives(sample, sites, maxEdgeDist))
					continue;
				if (IsUnderOcean(sample))
					continue;
				if (IsTooCloseToAnySite(sample))
					continue;
				if (GetSlopeTangent(sample) > maxSlope)
					continue;
				if (requireOccupancy && IsOccupied(sample))
					continue;

				outCandidates.Insert(sample);
			}
		}
	}

	//----------------------------------------------------------------------------------------------
	protected static bool IsStillNearObjectives(vector pos, array<IA_AreaMarker> sites, float maxEdgeDist)
	{
		return DistanceToNearestObjective(pos, sites) <= maxEdgeDist;
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
	protected static float DistanceToNearestObjective(vector pos, array<IA_AreaMarker> sites)
	{
		float best = 999999.0;
		if (!sites)
			return best;

		foreach (IA_AreaMarker marker : sites)
		{
			if (!marker)
				continue;

			float dist = vector.Distance(pos, marker.GetOrigin()) - marker.GetRadius();
			if (dist < 0)
				dist = 0;
			if (dist < best)
				best = dist;
		}

		return best;
	}

	//----------------------------------------------------------------------------------------------
	protected static vector PickCloserToObjectives(notnull array<vector> pool, array<IA_AreaMarker> sites)
	{
		int n = pool.Count();
		if (n <= 1)
			return pool[0];

		ref array<float> weights = new array<float>();
		float totalWeight = 0;
		int i;
		for (i = 0; i < n; i++)
		{
			float dist = DistanceToNearestObjective(pool[i], sites);
			float denom = dist + NEAR_BIAS_SOFTEN_M;
			float weight = 1.0 / (denom * denom);
			weights.Insert(weight);
			totalWeight = totalWeight + weight;
		}

		if (totalWeight <= 0)
			return pool[IA_Game.rng.RandInt(0, n)];

		float roll = IA_Game.rng.RandFloat01() * totalWeight;
		float acc = 0;
		for (i = 0; i < n; i++)
		{
			acc = acc + weights[i];
			if (roll <= acc)
				return pool[i];
		}

		return pool[n - 1];
	}

	//----------------------------------------------------------------------------------------------
	protected static vector FindFlattestFallback(float maxEdgeDist, array<IA_AreaMarker> sites)
	{
		ref array<vector> candidates = new array<vector>();
		CollectSiteBandCandidates(sites, maxEdgeDist, 999.0, false, candidates);
		if (candidates.IsEmpty())
			return vector.Zero;

		vector best = vector.Zero;
		float bestScore = 999.0;
		foreach (vector sample : candidates)
		{
			float slope = GetSlopeTangent(sample);
			float dist = DistanceToNearestObjective(sample, sites);
			float score = slope + (dist * FALLBACK_DIST_SLOPE_PER_M);
			if (score < bestScore)
			{
				bestScore = score;
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

		array<IA_AreaMarker> sites = CollectAnchorSites(groupNumber);
		if (sites.IsEmpty())
			sites = CollectNonMortarSites(groupNumber);
		float nearDist = DistanceToNearestObjective(pos, sites);
		Print(string.Format("[IA_MortarPitPlacer] Auto-placed MortarPit for group %1 at %2 with %3 guns (%4 m from nearest site)", groupNumber, pos, mortarCount, nearDist), LogLevel.NORMAL);
	}
};
