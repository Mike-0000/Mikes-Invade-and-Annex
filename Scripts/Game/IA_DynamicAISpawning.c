// Opt-in authority-side cache. Distance requests are unconditional and never expire.
class IA_DynamicAISpawning
{
	// Compatibility defaults. Runtime distances/timing come from GetTuning().
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
	static const int COST_SAMPLE_MS = 250;
	// Nearest-first restore must not materialize a soldier in a player's face.
	static const float POPIN_MIN_M = 60;
	// Far soldiers beyond keep-release may shed this soon when a closer squad is waiting.
	static const int PREEMPT_MIN_LIVE_SEC = 5;
	protected static ref array<IA_DynamicAIGroupCache> s_aGroups = {};
	protected static ref IA_DynamicAIWorkQueue s_Work = new IA_DynamicAIWorkQueue();
	protected static ref IA_DynamicAIBudgetController s_Budget = new IA_DynamicAIBudgetController();
	protected static ref IA_Config s_DefaultTuning = new IA_Config();
	protected static bool s_bRunning;
	protected static int s_iNextScanMs;
	protected static int s_iNextWakeScanMs;
	protected static int s_iScanCursor;
	protected static int s_iScanRemaining;
	protected static int s_iScanQuota;
	protected static int s_iScanMaxMs;
	protected static int s_iWakeScanMaxMs;
	protected static int s_iNextDiagnosticMs;
	protected static int s_iCostSampleMs;
	protected static int s_iCostSample;
	protected static int s_iReservesSeeded;

	static bool IsEnabled()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init || !init.GetConfig())
			return false;
		return init.GetConfig().m_bDynamicAISpawningEnabled;
	}

	// Authority-side callers use the validated mission config directly, without
	// allocating a replicated client copy during each soldier eligibility check.
	static IA_Config GetTuning()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (init && init.GetConfig())
			return init.GetConfig();
		return s_DefaultTuning;
	}

	static int GetBudgetLimit()
	{
		if (!IsEnabled())
			return 0;
		return IA_Config.ClampDynamicAIBudget(IA_MissionInitializer.GetInstance().GetConfig().m_iDynamicAIBudget);
	}

	// Shared cost is sampled, not recomputed per soldier: a spawn burst asks this
	// question once per created soldier and every answer walks the whole registry.
	static int GetManagedCost()
	{
		int now = System.GetTickCount();
		if (s_iCostSampleMs != 0 && now - s_iCostSampleMs < COST_SAMPLE_MS)
			return s_iCostSample;
		int total;
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			ref IA_DynamicAIBudgetCache budgetCache = IA_DynamicAIBudgetCache.Cast(cache);
			if (!budgetCache || !budgetCache.IsOwnerLive() || budgetCache.IsFinished())
				continue;
			if (budgetCache.IsBudgetActive())
				total += budgetCache.GetBudgetCost();
			else
				total += budgetCache.GetOwner().GetDynamicAIPhysicalAliveCount();
		}
		s_iCostSample = total;
		s_iCostSampleMs = now;
		return total;
	}

	// True while managed soldiers already fill the shared target. Protections that
	// exist to avoid visible pop-out stay; distance-based retention does not.
	static bool IsOverTarget()
	{
		int budget = GetBudgetLimit();
		if (budget <= 0)
			return false;
		return GetManagedCost() >= budget;
	}

	// Capture QRF, AO reinforcement waves, and defense inbound waves wait for a
	// real casualty gap. Distance-only (budget 0) and Dynamic AI off keep the
	// original spawn path. Defense pulses also shrink to the remaining room so
	// a large wave cannot overshoot the shared target.
	static bool HasRoomForInboundInfantry()
	{
		if (!IsEnabled())
			return true;
		return CanAdmitInboundInfantry(GetManagedCost(), GetBudgetLimit());
	}

	static bool CanAdmitInboundInfantry(int cost, int budget)
	{
		if (budget <= 0)
			return true;
		return cost < budget;
	}

	// Remaining physical infantry slots under the shared target.
	// -1 means Dynamic AI is off or distance-only: no inbound cap.
	static int CountInboundInfantryRoom(int cost, int budget)
	{
		if (budget <= 0)
			return -1;
		int room = budget - cost;
		if (room < 0)
			return 0;
		return room;
	}

	static int GetInboundInfantryRoom()
	{
		if (!IsEnabled())
			return -1;
		return CountInboundInfantryRoom(GetManagedCost(), GetBudgetLimit());
	}

	static int ClampInboundInfantryRequestForCost(int requested, int cost, int budget)
	{
		int room = CountInboundInfantryRoom(cost, budget);
		if (room < 0)
			return requested;
		if (requested < 1)
			return 0;
		if (requested <= room)
			return requested;
		return room;
	}

	static int ClampInboundInfantryRequest(int requested)
	{
		if (!IsEnabled())
			return requested;
		return ClampInboundInfantryRequestForCost(requested, GetManagedCost(), GetBudgetLimit());
	}

	static int GetReservesSeeded()
	{
		return s_iReservesSeeded;
	}

	static void ResetReservesSeeded()
	{
		s_iReservesSeeded = 0;
	}

	// Reserve-first spawn. While the target is already full, a squad spawning
	// beyond the wake distance records its remaining soldiers instead of creating
	// them. The same nearest-first restore fills them in as players approach.
	static bool ShouldSeedReserve(IA_AiGroup group, vector position)
	{
		if (!Replication.IsServer() || !group || !group.CanSeedDynamicAIReserves())
			return false;
		if (GetBudgetLimit() <= 0)
			return false;
		float wake = GetTuning().m_iDynamicAIWakeDistanceM;
		ref array<vector> players = {};
		IA_SpawnPlacement.CollectPlayerPositions(players);
		foreach (vector player : players)
		{
			float dx = position[0] - player[0];
			float dz = position[2] - player[2];
			if (dx * dx + dz * dz <= wake * wake)
				return false;
		}
		if (!IsOverTarget())
			return false;
		s_iReservesSeeded++;
		return true;
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
		s_Budget = new IA_DynamicAIBudgetController();
		IA_DynamicAIOccupancyHost.Reset();
		s_bRunning = false;
		s_iNextScanMs = 0;
		s_iNextWakeScanMs = 0;
		s_iScanCursor = 0;
		s_iScanRemaining = 0;
		s_iScanMaxMs = 0;
		s_iWakeScanMaxMs = 0;
		s_iNextDiagnosticMs = 0;
		s_iCostSampleMs = 0;
		s_iCostSample = 0;
		s_iReservesSeeded = 0;
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
		s_Budget.SettingsChanged();
		s_iScanRemaining = 0;
		s_iNextScanMs = 0;
		s_iNextWakeScanMs = 0;
		s_Work.ClearCacheWork();
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache)
				continue;
			cache.ResetQuietPeriod();
			if (!enabled)
			{
				ref IA_DynamicAIBudgetCache budgetCache = IA_DynamicAIBudgetCache.Cast(cache);
				if (budgetCache && budgetCache.IsBudgetActive())
					budgetCache.RequestMandatoryRestore(false);
				else
					cache.RequestWake(false);
			}
		}
		if (!s_aGroups.IsEmpty())
			Start();
	}

	// Deliberate removal is not group elimination. Off/ordinary groups use vanilla.
	static IA_DynamicAIGroupCache FindCacheForPawn(IEntity pawn)
	{
		if (!pawn)
			return null;
		AIControlComponent control = AIControlComponent.Cast(pawn.FindComponent(AIControlComponent));
		if (!control)
			return null;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return null;
		AIGroup parent = agent.GetParentGroup();
		if (!parent)
			return null;
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (!cache || !cache.GetOwner())
				continue;
			if (cache.GetOwner().GetSCR_AIGroup() == parent)
				return cache;
		}
		return null;
	}

	static bool IsVirtualizingGroup(SCR_AIGroup group)
	{
		foreach (IA_DynamicAIGroupCache cache : s_aGroups)
		{
			if (cache && cache.ShouldPreserveGroup() && cache.GetOwner() && cache.GetOwner().GetSCR_AIGroup() == group)
				return true;
		}
		return false;
	}

	// Capture scoring waits on physical defenders. Cached civilians use a
	// leftover pool after military work and must not freeze seize/town capture.
	static bool BlocksCaptureReadiness(IA_DynamicAIGroupCache cache, vector center, float radius)
	{
		if (!cache || !cache.IsOwnerLive() || !cache.Intersects(center, radius))
			return false;
		if (cache.IsCivilianCache())
			return false;
		ref IA_DynamicAIBudgetCache budgetCache = IA_DynamicAIBudgetCache.Cast(cache);
		if (budgetCache && budgetCache.IsBudgetActive())
		{
			// A contested objective needs one physical defender per group so
			// the fight and capture can progress; the rest fill nearest-first.
			if (cache.IsPaused())
			{
				budgetCache.RequestCaptureSeed();
				return true;
			}
			return false;
		}
		if (cache.IsCached())
		{
			cache.RequestWake();
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
			if (BlocksCaptureReadiness(cache, center, radius))
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
		int budget = GetBudgetLimit();
		if (budget > 0 || s_Budget.IsActive())
		{
			// A single worker owns both directions during budget mode and its OFF
			// drain; legacy queues cannot independently recreate the same records.
			s_Work.Clear();
			for (int index = s_aGroups.Count() - 1; index >= 0; index--)
			{
				IA_DynamicAIGroupCache registered = s_aGroups[index];
				if (registered && !registered.IsOwnerLive())
					registered.Retire();
				if (!registered || registered.IsFinished())
					s_aGroups.Remove(index);
			}
			s_Budget.Tick(s_aGroups, players, now, budget);
			return;
		}
		if (now >= s_iNextWakeScanMs)
		{
			s_iNextWakeScanMs = now + SCAN_INTERVAL_MS;
			CheckWakes(players, enabled);
		}
		ScanSlice(players, enabled, now);
		s_Work.Service(players, now, enabled);
		IA_DynamicAIOccupancyHost.TickAll(players, now, RESTORE_ATTEMPTS_PER_TICK, now);

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
