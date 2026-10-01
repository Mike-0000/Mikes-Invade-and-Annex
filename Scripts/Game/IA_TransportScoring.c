//------------------------------------------------------------------------------------------------
//! Rules for crediting a helicopter pilot with a combat insertion. Pure maths so
//! the Workbench regression can pin the thresholds without a running mission.
//------------------------------------------------------------------------------------------------
class IA_TransportScoring
{
	//! Points for one passenger set down at the edge of the scoring band.
	static const int BASE_POINTS = 10;
	//! Multiplier for a passenger set down on or beside the objective.
	static const float HOT_WEIGHT = 3.0;
	//! Distance past the objective circle that still counts as a hot LZ.
	static const float HOT_RANGE_M = 150;
	//! Distance past the objective circle where an insertion stops counting.
	static const float MAX_RANGE_M = 1000;
	//! Boarding-to-dropoff distance below which a ride is a seat shuffle, not transport.
	static const float MIN_TRAVEL_M = 300;
	//! One credit per passenger inside this window, whoever flew them.
	static const int PASSENGER_COOLDOWN_MS = 120000;
	//! How long a dismounted passenger may take to reach the ground alive.
	static const int SETTLE_TIMEOUT_MS = 120000;

	//------------------------------------------------------------------------------------------------
	//! \param edgeDistance metres from the dropoff to the nearest active objective circle, 0 inside it
	static float InsertionWeight(float edgeDistance)
	{
		if (edgeDistance < 0)
			edgeDistance = 0;
		if (edgeDistance > MAX_RANGE_M)
			return 0;
		if (edgeDistance <= HOT_RANGE_M)
			return HOT_WEIGHT;

		float t = (edgeDistance - HOT_RANGE_M) / (MAX_RANGE_M - HOT_RANGE_M);
		return HOT_WEIGHT - (HOT_WEIGHT - 1.0) * t;
	}

	//------------------------------------------------------------------------------------------------
	static int InsertionPoints(float edgeDistance)
	{
		return Math.Round(BASE_POINTS * InsertionWeight(edgeDistance));
	}

	//------------------------------------------------------------------------------------------------
	//! \param lastCreditMs tick of this passenger's previous credited insertion, negative when none
	static bool IsCreditableRide(float travelDistance, int nowMs, int lastCreditMs)
	{
		if (travelDistance < MIN_TRAVEL_M)
			return false;
		if (lastCreditMs < 0)
			return true;
		return nowMs - lastCreditMs >= PASSENGER_COOLDOWN_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Most one insertion can pay; the backend rejects batches that claim more.
	static int MaxPointsPerInsertion()
	{
		return Math.Round(BASE_POINTS * HOT_WEIGHT);
	}

	//------------------------------------------------------------------------------------------------
	//! Wait before retrying the stats backend: doubles per consecutive failure up to maxMs.
	static int BackoffMs(int failures, int baseMs, int maxMs)
	{
		int delay = baseMs;
		int i;
		for (i = 1; i < failures; i++)
		{
			delay = delay * 2;
			if (delay >= maxMs)
				return maxMs;
		}
		if (delay > maxMs)
			return maxMs;
		return delay;
	}

	//------------------------------------------------------------------------------------------------
	//! Edge distance from a point to one objective circle, 0 inside it. Horizontal
	//! only, matching the capture circle on the map.
	static float EdgeDistance(vector pos, vector center, float radius)
	{
		float dx = pos[0] - center[0];
		float dz = pos[2] - center[2];
		float d = Math.Sqrt(dx * dx + dz * dz) - radius;
		if (d < 0)
			return 0;
		return d;
	}
}
