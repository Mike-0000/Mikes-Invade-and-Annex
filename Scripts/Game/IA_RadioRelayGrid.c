///////////////////////////////////////////////////////////////////////
// IA_RadioRelayGrid
// Sprinkles always-on native radio relays across the whole map so
// handheld VON (1300 m) has no dead zones. Not a capture/coverage net.
//
// The handheld must reach a relay with ITS OWN transmit range. Relay
// range only hops relay-to-relay. Spacing is therefore sized to 1300 m
// handhelds, including 3D height, not to the relay's own range.
///////////////////////////////////////////////////////////////////////
class IA_RadioRelayGrid
{
	protected static const ResourceName RELAY_PREFAB = "{3D4DCD05B7C716E3}Prefabs/IA_RadioCoverage.et";
	protected static const string RELAY_ENCRYPTION = "chickenNuggets";

	// ANPRC-68 / R148 handhelds are 1300 m. Cell-center distance on a
	// square grid is (step/2)*sqrt(2). 900 m keeps that at ~636 m so
	// hills and the 80 m mast still fit under 1300 m.
	protected static const float HANDHELD_RANGE_M = 1300.0;
	protected static const float GRID_STEP_M = 900.0;
	protected static const float RELAY_HEIGHT_M = 80.0;
	protected static const float RELAY_RANGE_M = 65535.0;
	protected static const float MARKER_PAD_M = 4000.0;
	protected static const float WORLD_SPAN_SANITY_M = 45000.0;
	protected static const int GRID_MAX_AXIS = 40;

	protected static bool s_bSpawned;

	//------------------------------------------------------------------------------------------------
	static void EnsureSpawned()
	{
		if (s_bSpawned)
			return;

		if (!Replication.IsServer())
			return;

		if (!GetGame().InPlayMode())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[IA][Radio] World missing; cannot spawn relay grid.", LogLevel.ERROR);
			return;
		}

		Resource res = Resource.Load(RELAY_PREFAB);
		if (!res)
		{
			Print("[IA][Radio] Failed to load relay prefab.", LogLevel.ERROR);
			return;
		}

		float minX;
		float maxX;
		float minZ;
		float maxZ;
		if (!ResolveBounds(world, minX, maxX, minZ, maxZ))
		{
			Print("[IA][Radio] Could not resolve map bounds for relay grid.", LogLevel.ERROR);
			return;
		}

		ref array<float> xs = new array<float>();
		ref array<float> zs = new array<float>();
		FillAxis(minX, maxX, xs);
		FillAxis(minZ, maxZ, zs);

		int countX = xs.Count();
		int countZ = zs.Count();
		if (countX < 1 || countZ < 1)
		{
			Print("[IA][Radio] Relay axis lists empty after bounds resolve.", LogLevel.ERROR);
			return;
		}

		int spawned;
		int ix;
		int iz;
		for (ix = 0; ix < countX; ix++)
		{
			for (iz = 0; iz < countZ; iz++)
			{
				if (SpawnRelay(res, world, xs[ix], zs[iz]))
					spawned++;
			}
		}

		if (spawned < 1)
		{
			Print("[IA][Radio] Relay prefab spawned zero entities; will retry.", LogLevel.ERROR);
			return;
		}

		s_bSpawned = true;
		IA_Log.Info(string.Format("[IA][Radio] Spawned %1 always-on relays on a %2x%3 grid (step %4 m, handheld %5 m, bounds %6..%7 / %8..%9).", spawned, countX, countZ, GRID_STEP_M, HANDHELD_RANGE_M, minX, maxX, minZ, maxZ));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ResolveBounds(notnull BaseWorld world, out float minX, out float maxX, out float minZ, out float maxZ)
	{
		minX = 0;
		maxX = 0;
		minZ = 0;
		maxZ = 0;

		bool hasWorldBounds;
		float worldMinX;
		float worldMaxX;
		float worldMinZ;
		float worldMaxZ;

		vector boxMin;
		vector boxMax;
		world.GetBoundBox(boxMin, boxMax);
		if (IsUsableSpan(boxMin[0], boxMax[0], boxMin[2], boxMax[2]))
		{
			worldMinX = boxMin[0];
			worldMaxX = boxMax[0];
			worldMinZ = boxMin[2];
			worldMaxZ = boxMax[2];
			hasWorldBounds = true;
		}

		IEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
		{
			vector entMin;
			vector entMax;
			worldEnt.GetWorldBounds(entMin, entMax);
			if (IsUsableSpan(entMin[0], entMax[0], entMin[2], entMax[2]))
			{
				if (!hasWorldBounds)
				{
					worldMinX = entMin[0];
					worldMaxX = entMax[0];
					worldMinZ = entMin[2];
					worldMaxZ = entMax[2];
					hasWorldBounds = true;
				}
				else
				{
					if (entMin[0] < worldMinX)
						worldMinX = entMin[0];
					if (entMax[0] > worldMaxX)
						worldMaxX = entMax[0];
					if (entMin[2] < worldMinZ)
						worldMinZ = entMin[2];
					if (entMax[2] > worldMaxZ)
						worldMaxZ = entMax[2];
				}
			}
		}

		bool hasMarkerBounds;
		float markerMinX;
		float markerMaxX;
		float markerMinZ;
		float markerMaxZ;
		CollectMarkerBounds(hasMarkerBounds, markerMinX, markerMaxX, markerMinZ, markerMaxZ);

		bool worldTooLarge;
		if (hasWorldBounds)
		{
			float spanX = worldMaxX - worldMinX;
			float spanZ = worldMaxZ - worldMinZ;
			if (spanX > WORLD_SPAN_SANITY_M || spanZ > WORLD_SPAN_SANITY_M)
				worldTooLarge = true;
		}

		if (hasWorldBounds && !worldTooLarge)
		{
			minX = worldMinX;
			maxX = worldMaxX;
			minZ = worldMinZ;
			maxZ = worldMaxZ;
			if (hasMarkerBounds)
				UnionBounds(minX, maxX, minZ, maxZ, markerMinX, markerMaxX, markerMinZ, markerMaxZ);
			return true;
		}

		if (hasMarkerBounds)
		{
			minX = markerMinX - MARKER_PAD_M;
			maxX = markerMaxX + MARKER_PAD_M;
			minZ = markerMinZ - MARKER_PAD_M;
			maxZ = markerMaxZ + MARKER_PAD_M;
			return true;
		}

		if (!hasWorldBounds)
			return false;

		minX = worldMinX;
		maxX = worldMaxX;
		minZ = worldMinZ;
		maxZ = worldMaxZ;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUsableSpan(float minX, float maxX, float minZ, float maxZ)
	{
		if ((maxX - minX) <= 1)
			return false;
		if ((maxZ - minZ) <= 1)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectMarkerBounds(out bool hasBounds, out float minX, out float maxX, out float minZ, out float maxZ)
	{
		hasBounds = false;
		minX = 0;
		maxX = 0;
		minZ = 0;
		maxZ = 0;

		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		int markerCount = markers.Count();
		int i;
		for (i = 0; i < markerCount; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;

			vector origin = marker.GetOrigin();
			float x = origin[0];
			float z = origin[2];
			if (!hasBounds)
			{
				minX = x;
				maxX = x;
				minZ = z;
				maxZ = z;
				hasBounds = true;
			}
			else
			{
				if (x < minX)
					minX = x;
				if (x > maxX)
					maxX = x;
				if (z < minZ)
					minZ = z;
				if (z > maxZ)
					maxZ = z;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void UnionBounds(inout float minX, inout float maxX, inout float minZ, inout float maxZ, float otherMinX, float otherMaxX, float otherMinZ, float otherMaxZ)
	{
		if (otherMinX < minX)
			minX = otherMinX;
		if (otherMaxX > maxX)
			maxX = otherMaxX;
		if (otherMinZ < minZ)
			minZ = otherMinZ;
		if (otherMaxZ > maxZ)
			maxZ = otherMaxZ;
	}

	//------------------------------------------------------------------------------------------------
	// Fixed 900 m pitch. Do not stretch to the AABB: stretching is what
	// pushed handhelds out of range of the nearest relay.
	protected static void FillAxis(float minVal, float maxVal, notnull array<float> coords)
	{
		coords.Clear();

		float span = maxVal - minVal;
		if (span < 1)
		{
			coords.Insert((minVal + maxVal) * 0.5);
			return;
		}

		coords.Insert(minVal);
		int count = 1;
		float x = minVal;
		while (count < GRID_MAX_AXIS)
		{
			x = x + GRID_STEP_M;
			if (x >= (maxVal - 0.01))
				break;

			coords.Insert(x);
			count++;
		}

		if (count < GRID_MAX_AXIS)
		{
			float last = coords[count - 1];
			if ((maxVal - last) > 0.01)
				coords.Insert(maxVal);
		}
		else
		{
			Print(string.Format("[IA][Radio] Axis hit %1-point cap over %2 m; far edge may be thin.", GRID_MAX_AXIS, span), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SpawnRelay(notnull Resource res, notnull BaseWorld world, float x, float z)
	{
		vector pos;
		pos[0] = x;
		pos[1] = world.GetSurfaceY(x, z) + RELAY_HEIGHT_M;
		pos[2] = z;

		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;

		IEntity relay = GetGame().SpawnEntityPrefab(res, world, params);
		if (!relay)
		{
			Print(string.Format("[IA][Radio] Failed to spawn relay at %1.", pos), LogLevel.WARNING);
			return false;
		}

		BaseRadioComponent radio = BaseRadioComponent.Cast(relay.FindComponent(BaseRadioComponent));
		if (!radio)
		{
			Print(string.Format("[IA][Radio] Relay at %1 has no BaseRadioComponent.", pos), LogLevel.WARNING);
			return true;
		}

		radio.SetPower(true);
		radio.SetEncryptionKey(RELAY_ENCRYPTION);

		int n = radio.TransceiversCount();
		int t;
		for (t = 0; t < n; t++)
		{
			BaseTransceiver trx = radio.GetTransceiver(t);
			if (!trx)
				continue;

			trx.SetRange(RELAY_RANGE_M);
		}

		return true;
	}
}
