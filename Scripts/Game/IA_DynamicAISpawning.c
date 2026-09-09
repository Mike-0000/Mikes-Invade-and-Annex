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
	static const int WORK_BUDGET_MS = 4;
	static const int SCAN_WORK_MS = 2;
	static const int CACHE_SOLDIERS_PER_TICK = 8;
	static const int CACHE_AGING_MS = 2000;
	static const int WAKE_AGING_MS = 5000;
	protected static ref array<IA_DynamicAIGroupCache> s_aGroups = {};
	protected static ref IA_DynamicAIWorkQueue s_Work = new IA_DynamicAIWorkQueue();
	protected static bool s_bRunning;
	protected static int s_iNextScanMs;
	protected static int s_iNextWakeScanMs;
	protected static int s_iScanCursor;
	protected static int s_iScanRemaining;
	protected static int s_iScanQuota;
	protected static int s_iScanMaxMs;
	protected static int s_iWakeScanMaxMs;
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
		s_Work.Clear();
		s_bRunning = false;
		s_iNextScanMs = 0;
		s_iNextWakeScanMs = 0;
		s_iScanCursor = 0;
		s_iScanRemaining = 0;
		s_iScanMaxMs = 0;
		s_iWakeScanMaxMs = 0;
		s_iNextDiagnosticMs = 0;
	}

	static void RetireForArea(IA_AreaInstance area)
	{
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (cache && cache.GetOwner() && cache.GetOwner().GetDynamicAIOwner() == area)
			{
				s_Work.Forget(cache);
				cache.Retire();
			}
		}
	}

	protected static void Start()
	{
		if (s_bRunning || !GetGame() || !GetGame().GetCallqueue())
			return;
		s_bRunning = true;
		s_iNextScanMs = 0;
		s_iNextWakeScanMs = 0;
		s_Work.BeginReportWindow(System.GetTickCount());
		s_iNextDiagnosticMs = System.GetTickCount() + 30000;
		GetGame().GetCallqueue().CallLater(Tick, TICK_INTERVAL_MS, true);
	}

	static void EnqueueWake(IA_DynamicAIGroupCache cache)
	{
		if (!Replication.IsServer())
			return;
		s_Work.EnqueueWake(cache);
		Start();
	}

	static void OnSettingsChanged()
	{
		if (!Replication.IsServer())
			return;
		bool enabled = IsEnabled();
		s_Work.ClearCacheWork();
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache)
				continue;
			cache.ResetQuietPeriod();
			if (!enabled)
				cache.RequestWake(false);
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
		ref array<vector> players = {};
		// One fresh player sample per worker invocation, including the commit tick.
		IA_SpawnPlacement.CollectPlayerPositions(players);
		if (now >= s_iNextWakeScanMs)
		{
			s_iNextWakeScanMs = now + SCAN_INTERVAL_MS;
			CheckWakes(players, enabled);
		}
		ScanSlice(players, enabled, now);
		s_Work.Service(players, now, enabled);

		if (IA_Log.IsDebugEnabled())
		{
			if (now >= s_iNextDiagnosticMs)
			{
				s_iNextDiagnosticMs = now + 30000;
				ReportCoverage();
				s_Work.Report(now, s_iScanMaxMs);
				Print(string.Format("[IA][DynamicAI] Wake census maxMs=%1. Cached-position checks run independently of live eligibility slices.", s_iWakeScanMaxMs), LogLevel.NORMAL);
				s_iScanMaxMs = 0;
				s_iWakeScanMaxMs = 0;
			}
		}

		if (enabled && !s_aGroups.IsEmpty())
			return;
		if (s_Work.GetWakeCount() > 0)
			return;
		foreach (IA_DynamicAIGroupCache remaining : s_aGroups)
		{
			if (remaining && remaining.IsCached())
				return;
		}
		GetGame().GetCallqueue().Remove(Tick);
		s_bRunning = false;
	}

	protected static void CheckWakes(array<vector> players, bool enabled)
	{
		int started = System.GetTickCount();
		// Only saved positions: no live components, placement checks or snapshots.
		// This census cannot sit behind a slowly progressing live eligibility scan.
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache || !cache.IsCached() || !cache.IsOwnerLive())
				continue;
			cache.CheckWake(players, enabled);
			s_Work.EnqueueWake(cache);
		}
		if (IA_Log.IsDebugEnabled())
			s_iWakeScanMaxMs = Math.Max(s_iWakeScanMaxMs, System.GetTickCount() - started);
	}

	protected static void ScanSlice(array<vector> players, bool enabled, int now)
	{
		if (s_iScanRemaining == 0 && now >= s_iNextScanMs)
		{
			s_iNextScanMs = now + SCAN_INTERVAL_MS;
			s_iScanCursor = 0;
			s_iScanRemaining = s_aGroups.Count();
			// Spread a nominal one-second census over ten ticks. Heavy individual
			// groups may exceed a slice; continue next tick instead of rescanning.
			s_iScanQuota = Math.Max(1, Math.Ceil(s_iScanRemaining / 10.0));
		}
		int started = System.GetTickCount();
		for (int inspected = 0; inspected < s_iScanQuota && s_iScanRemaining > 0 && !s_aGroups.IsEmpty(); inspected++)
		{
			if (s_iScanCursor >= s_aGroups.Count())
				s_iScanCursor = 0;
			ref IA_DynamicAIGroupCache cache = s_aGroups[s_iScanCursor];
			s_iScanRemaining--;
			if (cache && !cache.IsOwnerLive())
				cache.Retire();
			if (!cache || cache.IsFinished())
			{
				s_Work.Forget(cache);
				s_aGroups.Remove(s_iScanCursor);
			}
			else
			{
				s_iScanCursor++;
				if (!cache.IsCached() && enabled)
				{
					if (cache.TryCache(players, now, false))
						s_Work.EnqueueCache(cache, now);
					else
						s_Work.CancelCache(cache);
				}
			}
			if (System.GetTickCount() - started >= SCAN_WORK_MS)
				break;
		}
		if (s_aGroups.IsEmpty())
			s_iScanRemaining = 0;
		if (IA_Log.IsDebugEnabled())
			s_iScanMaxMs = Math.Max(s_iScanMaxMs, System.GetTickCount() - started);
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
