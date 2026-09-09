// Opt-in authority-side cache. Distance requests are unconditional and never expire.
class IA_DynamicAISpawning
{
	static const float WAKE_DISTANCE_M = 1000;
	static const float CACHE_DISTANCE_M = 1500;
	static const int CACHE_QUIET_MS = 60000;
	static const int COMBAT_QUIET_SEC = 60;
	static const int SCAN_INTERVAL_MS = 1000;
	static const int TICK_INTERVAL_MS = 100;
	static const int RESTORE_ATTEMPTS_PER_TICK = 4;
	static const int RESTORE_WORK_MS = 4;
	protected static ref array<IA_DynamicAIGroupCache> s_aGroups = {};
	protected static bool s_bRunning;
	protected static int s_iNextScanMs;
	protected static int s_iRestoreCursor;
	protected static int s_iNextDiagnosticMs;

	static bool IsEnabled()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init || !init.GetConfig())
			return false;
		return init.GetConfig().m_bDynamicAISpawningEnabled;
	}

	static void Register(IA_DynamicAIGroupCache cache)
	{
		if (!Replication.IsServer() || !cache || s_aGroups.Contains(cache))
			return;
		s_aGroups.Insert(cache);
		if (IsEnabled())
			Start();
	}

	static void ResetForMission()
	{
		if (!Replication.IsServer())
			return;
		GetGame().GetCallqueue().Remove(Tick);
		s_aGroups.Clear();
		s_bRunning = false;
		s_iNextScanMs = 0;
		s_iRestoreCursor = 0;
		s_iNextDiagnosticMs = 0;
	}

	static void RetireForArea(IA_AreaInstance area)
	{
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (cache && cache.GetOwner() && cache.GetOwner().GetDynamicAIOwner() == area)
				cache.Retire();
		}
	}

	protected static void Start()
	{
		if (s_bRunning || !GetGame() || !GetGame().GetCallqueue())
			return;
		s_bRunning = true;
		s_iNextScanMs = 0;
		GetGame().GetCallqueue().CallLater(Tick, TICK_INTERVAL_MS, true);
	}

	static void OnSettingsChanged()
	{
		if (!Replication.IsServer())
			return;
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache)
				continue;
			cache.ResetQuietPeriod();
			if (!IsEnabled())
				cache.RequestWake();
		}
		if (!s_aGroups.IsEmpty())
			Start();
	}

	// Deliberate removal is not group elimination. Off/ordinary groups use vanilla.
	static bool IsVirtualizingGroup(SCR_AIGroup group)
	{
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (cache && cache.IsCached() && cache.GetOwner() && cache.GetOwner().GetSCR_AIGroup() == group)
				return true;
		}
		return false;
	}

	static bool EnsureReadyInRadius(vector center, float radius)
	{
		if (!Replication.IsServer())
			return true;
		bool ready = true;
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache || !cache.IsOwnerLive() || !cache.Intersects(center, radius))
				continue;
			cache.RequestWake();
			ready = false;
		}
		if (!ready)
			Start();
		return ready;
	}

	protected static void Tick()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld())
			return;
		int now = System.GetTickCount();
		bool enabled = IsEnabled();
		if (now >= s_iNextScanMs)
		{
			s_iNextScanMs = now + SCAN_INTERVAL_MS;
			ref array<vector> players = {};
			IA_SpawnPlacement.CollectPlayerPositions(players);
			bool cachedThisScan;
			for (int i = s_aGroups.Count() - 1; i >= 0; i--)
			{
				IA_DynamicAIGroupCache cache = s_aGroups[i];
				if (!cache)
				{
					s_aGroups.Remove(i);
					continue;
				}
				if (!cache.IsOwnerLive())
					cache.Retire();
				if (cache.IsFinished())
				{
					s_aGroups.Remove(i);
					continue;
				}
				if (cache.IsCached())
					cache.CheckWake(players, enabled);
				else if (enabled)
				{
					if (cache.TryCache(players, now, !cachedThisScan))
						cachedThisScan = true;
				}
			}
		}

		if (IA_Log.IsDebugEnabled())
		{
			if (enabled && now >= s_iNextDiagnosticMs)
			{
				s_iNextDiagnosticMs = now + 30000;
				ReportCoverage();
			}
		}

		// Shared work limit, not a population cap. Every queued group gets turns.
		int total = s_aGroups.Count();
		int attempts;
		int startMs = System.GetTickCount();
		for (int inspected = 0; inspected < total && attempts < RESTORE_ATTEMPTS_PER_TICK; inspected++)
		{
			if (s_iRestoreCursor >= total)
				s_iRestoreCursor = 0;
			IA_DynamicAIGroupCache pending = s_aGroups[s_iRestoreCursor];
			s_iRestoreCursor++;
			if (pending && pending.RestoreNext(now))
				attempts++;
			if (System.GetTickCount() - startMs >= RESTORE_WORK_MS)
				break;
		}

		if (enabled && total > 0)
			return;
		foreach (IA_DynamicAIGroupCache remaining : s_aGroups)
		{
			if (remaining && remaining.IsCached())
				return;
		}
		GetGame().GetCallqueue().Remove(Tick);
		s_bRunning = false;
	}

	protected static void ReportCoverage()
	{
		if (IA_Log.IsDebugEnabled())
		{
			ref array<string> statuses = {};
			ref array<int> groupCounts = {};
			ref array<int> soldierCounts = {};
			foreach (IA_DynamicAIGroupCache cache : s_aGroups)
			{
				if (!cache || !cache.IsOwnerLive() || cache.IsFinished())
					continue;
				string status = cache.GetDiagnosticStatus();
				int index = statuses.Find(status);
				if (index < 0)
				{
					index = statuses.Insert(status);
					groupCounts.Insert(0);
					soldierCounts.Insert(0);
				}
				groupCounts[index] = groupCounts[index] + 1;
				soldierCounts[index] = soldierCounts[index] + cache.GetOwner().GetAliveCount();
			}
			string report = "[IA][DynamicAI] Coverage (registered area groups; groups/soldiers):";
			foreach (int category, string label : statuses)
			{
				report += string.Format(" %1=%2/%3;", label, groupCounts[category], soldierCounts[category]);
			}
			Print(report, LogLevel.NORMAL);
		}
	}
}
