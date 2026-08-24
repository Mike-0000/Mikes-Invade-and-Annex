///////////////////////////////////////////////////////////////////////
// IA_RadioRelayGrid
// Sprinkles always-on native radio relays across the whole map so
// handheld VON (1300 m) has no dead zones. Not a capture/coverage net.
///////////////////////////////////////////////////////////////////////
class IA_RadioRelayGrid
{
	protected static const ResourceName RELAY_PREFAB = "{3D4DCD05B7C716E3}Prefabs/IA_RadioCoverage.et";
	protected static const float GRID_STEP_M = 1500.0;
	protected static const float RELAY_HEIGHT_M = 50.0;
	protected static const float MARKER_PAD_M = 2000.0;
	protected static const int GRID_MAX_AXIS = 24;

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
		if (!ResolveBounds(minX, maxX, minZ, maxZ))
		{
			Print("[IA][Radio] Could not resolve map bounds for relay grid.", LogLevel.ERROR);
			return;
		}

		int countX = AxisCount(maxX - minX);
		int countZ = AxisCount(maxZ - minZ);
		float stepX = AxisStep(maxX - minX, countX);
		float stepZ = AxisStep(maxZ - minZ, countZ);

		int spawned;
		int ix;
		int iz;
		for (ix = 0; ix < countX; ix++)
		{
			float x = minX;
			if (countX > 1)
				x = minX + (stepX * ix);
			else
				x = (minX + maxX) * 0.5;

			for (iz = 0; iz < countZ; iz++)
			{
				float z = minZ;
				if (countZ > 1)
					z = minZ + (stepZ * iz);
				else
					z = (minZ + maxZ) * 0.5;

				if (SpawnRelay(res, world, x, z))
					spawned++;
			}
		}

		s_bSpawned = true;
		Print(string.Format("[IA][Radio] Spawned %1 always-on relays on a %2x%3 grid.", spawned, countX, countZ), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ResolveBounds(out float minX, out float maxX, out float minZ, out float maxZ)
	{
		minX = 0;
		maxX = 0;
		minZ = 0;
		maxZ = 0;
		bool usedWorldBounds;
		bool hasBounds;

		IEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
		{
			vector worldMin;
			vector worldMax;
			worldEnt.GetWorldBounds(worldMin, worldMax);
			minX = worldMin[0];
			maxX = worldMax[0];
			minZ = worldMin[2];
			maxZ = worldMax[2];
			if ((maxX - minX) > 1 && (maxZ - minZ) > 1)
			{
				hasBounds = true;
				usedWorldBounds = true;
			}
		}

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

		if (!hasBounds)
			return false;

		if (!usedWorldBounds)
		{
			minX = minX - MARKER_PAD_M;
			maxX = maxX + MARKER_PAD_M;
			minZ = minZ - MARKER_PAD_M;
			maxZ = maxZ + MARKER_PAD_M;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static int AxisCount(float span)
	{
		if (span < 1)
			return 1;

		int count = span / GRID_STEP_M;
		float remainder = span - (count * GRID_STEP_M);
		if (remainder > 0.01)
			count = count + 1;

		if (count < 2)
			count = 2;

		if (count > GRID_MAX_AXIS)
			count = GRID_MAX_AXIS;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static float AxisStep(float span, int count)
	{
		if (count <= 1)
			return 0;

		return span / (count - 1);
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
		if (radio)
		{
			radio.SetPower(true);
			radio.SetEncryptionKey("chickenNuggets");
		}

		return true;
	}
}
