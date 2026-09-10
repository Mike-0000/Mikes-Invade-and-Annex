#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI spawning config regression", wbModules: {"ResourceManager"})]
class IA_DynamicAISpawningConfigTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		ref IA_Config source = new IA_Config();
		ref IA_Config restored = new IA_Config();
		Check(!source.m_bDynamicAISpawningEnabled, "new configurations keep legacy spawning by default");

		ref IA_DynamicAISpawningOverridesFixture saved = new IA_DynamicAISpawningOverridesFixture();
		ref IA_DynamicAISpawningOverridesFixture loaded = new IA_DynamicAISpawningOverridesFixture();
		source.m_bDynamicAISpawningEnabled = true;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(restored.m_bDynamicAISpawningEnabled, "enabled setting survives the profile roundtrip");

		source.m_bDynamicAISpawningEnabled = false;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(!restored.m_bDynamicAISpawningEnabled, "disabled setting replaces a previously enabled setting");

		restored.m_bDynamicAISpawningEnabled = true;
		loaded.Decode("{\"v\":2,\"aiScale\":1}");
		loaded.ApplyTo(restored);
		Check(restored.m_bDynamicAISpawningEnabled, "old profiles preserve an explicit mission setting");
		restored.m_bDynamicAISpawningEnabled = false;
		loaded.ApplyTo(restored);
		Check(!restored.m_bDynamicAISpawningEnabled, "old profiles preserve the default legacy setting");

		restored.m_bDynamicAISpawningEnabled = true;
		loaded.Decode("{\"dynamicAISpawning\":2}");
		loaded.ApplyTo(restored);
		Check(restored.m_bDynamicAISpawningEnabled, "malformed new values preserve the mission setting");

		// Pending restoration remains a living roster entry across engine failures.
		ref IA_DynamicAIUnit soldier = new IA_DynamicAIUnit();
		soldier.m_aTransform[3] = "100 20 100";
		Check(soldier.IsLogicallyAlive(), "an unspawned saved soldier remains in the roster");
		soldier.RecordFailure(100);
		Check(!soldier.m_bRestored && soldier.IsLogicallyAlive() && soldier.m_iNextAttemptMs > 100, "failed creation retains the soldier and schedules a retry");
		Check(soldier.IsInside("100 20 105", 5), "capture includes a saved soldier on the radius boundary");
		Check(!soldier.IsInside("100 26 100", 5), "capture readiness uses the physical census three-dimensional radius");
		soldier.m_bDead = true;
		soldier.m_bRestored = true;
		Check(!soldier.IsLogicallyAlive(), "a casualty with its entity already deleted stays dead");

		TestDepartureTimeline();
		TestCombatTimeline();
		TestCycleResetTimeline();
		TestConfiguredGateTimeline();
		TestGateReconfiguration();
		TestWorkQueue();
		m_iFailures += IA_AiGroup.RunDynamicAIGroupRegression();

		Print(string.Format("[IA][DynamicAISpawningConfigTest] failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestDepartureTimeline()
	{
		ref IA_DynamicAIActivityGate gate = new IA_DynamicAIActivityGate();
		Check(!gate.CanCache(false, 1000, 0, 1700000001), "a distant garrison starts a quiet period");
		Check(!gate.CanCache(true, 35000, 0, 1700000035), "a brief flyover keeps the garrison live");
		Check(!gate.CanCache(false, 40000, 0, 1700000040), "flyover departure starts a fresh quiet period");
		Check(!gate.CanCache(false, 99999, 0, 1700000099), "59,999 milliseconds after departure is too soon");
		Check(gate.CanCache(false, 100000, 0, 1700000100), "one full minute after flyover departure permits caching");

		Check(!gate.CanCache(true, 110000, 0, 1700000110), "a returning player revokes prior eligibility");
		Check(!gate.CanCache(false, 115000, 0, 1700000115), "second departure begins its own quiet period");
		Check(!gate.CanCache(false, 174999, 0, 1700000174), "an earlier visit cannot shorten the second quiet period");
		Check(gate.CanCache(false, 175000, 0, 1700000175), "a second complete departure permits another cache cycle");
	}

	protected void TestCombatTimeline()
	{
		ref IA_DynamicAIActivityGate gate = new IA_DynamicAIActivityGate();
		Check(!gate.CanCache(false, 1001000, 0, 1700000001), "departure starts before distant fighting resumes");
		Check(!gate.CanCache(false, 1061000, 1700000060, 1700000061), "recent fighting blocks a group already distant for a minute");
		Check(!gate.CanCache(false, 1119000, 1700000060, 1700000119), "59 seconds without danger is too soon");
		Check(gate.CanCache(false, 1120000, 1700000060, 1700000120), "old combat does not permanently prevent recaching");
		Check(!gate.CanCache(false, 1125000, 1700000125, 1700000125), "renewed fighting revokes prior eligibility");
		Check(gate.CanCache(false, 1185000, 1700000125, 1700000185), "the group becomes eligible again after renewed fighting stops");
	}

	protected void TestCycleResetTimeline()
	{
		ref IA_DynamicAIActivityGate gate = new IA_DynamicAIActivityGate();
		Check(!gate.CanCache(false, 2000000, 0, 1700000200), "initial cycle starts its departure timer");
		Check(gate.CanCache(false, 2060000, 0, 1700000260), "initial cycle reaches caching eligibility");
		gate.Reset();
		Check(!gate.CanCache(false, 2060000, 0, 1700000260), "restoration reset prevents immediate recaching while players remain distant");
		Check(!gate.CanCache(false, 2119999, 0, 1700000319), "restoration reset requires the entire new quiet period");
		Check(gate.CanCache(false, 2120000, 0, 1700000320), "restored group can cache after a new full minute");

		gate.Reset();
		Check(!gate.CanCache(false, 2300000, 0, 1700000500), "settings reset cannot inherit elapsed time from the previous cycle");
		Check(!gate.CanCache(false, 2359999, 0, 1700000559), "settings reset preserves the new quiet-period boundary");
		Check(gate.CanCache(false, 2360000, 0, 1700000560), "settings reset still permits later recaching");
	}

	protected void TestConfiguredGateTimeline()
	{
		ref IA_DynamicAIActivityGate gate = new IA_DynamicAIActivityGate();
		gate.Configure(12, 25);
		Check(!gate.CanCache(false, 1000, 0, 1700000001), "custom distant quiet duration starts a new departure timer");
		Check(!gate.CanCache(false, 12999, 0, 1700000012), "custom quiet duration still blocks one millisecond before its boundary");
		Check(gate.CanCache(false, 13000, 0, 1700000013), "custom quiet seconds convert to the exact millisecond boundary");
		Check(!gate.CanCache(false, 20000, 1700000015, 1700000020), "custom combat duration protects a distant group whose quiet timer already elapsed");
		Check(!gate.CanCache(false, 39000, 1700000015, 1700000039), "custom combat duration blocks until its complete seconds boundary");
		Check(gate.CanCache(false, 40000, 1700000015, 1700000040), "custom combat protection expires at its configured duration");
		gate.Configure(0, 25);
		Check(gate.CanCache(false, 41000, 0, 1700000041), "zero distant quiet duration permits immediate entry when otherwise eligible");
		Check(!gate.CanCache(false, 41000, 1700000040, 1700000041), "zero distant quiet duration does not disable recent-combat protection");
	}

	protected void TestGateReconfiguration()
	{
		ref IA_DynamicAIActivityGate gate = new IA_DynamicAIActivityGate();
		gate.Configure(12, 25);
		Check(!gate.CanCache(false, 500000, 0, 1700000500), "reconfiguration test starts a fresh custom quiet period");
		gate.Configure(12, 25);
		Check(!gate.CanCache(false, 511999, 0, 1700000511), "reapplying unchanged values preserves the current boundary");
		gate.Configure(12, 25);
		Check(gate.CanCache(false, 512000, 0, 1700000512), "reapplying unchanged values cannot keep restarting the quiet period");
		gate.Configure(20, 25);
		Check(!gate.CanCache(false, 512000, 0, 1700000512), "changing quiet duration cannot inherit elapsed time under the previous setting");
		Check(!gate.CanCache(false, 531999, 0, 1700000531), "a changed quiet duration requires its complete new interval");
		Check(gate.CanCache(false, 532000, 0, 1700000532), "a changed quiet duration still permits eventual caching");
		gate.Configure(20, 40);
		Check(!gate.CanCache(false, 532000, 0, 1700000532), "changing only combat duration also resets the old entry eligibility");
		gate.Configure(20, 40);
		Check(!gate.CanCache(false, 552000, 1700000520, 1700000552), "the expanded combat window protects a threat older than the previous setting");
		Check(gate.CanCache(false, 560000, 1700000520, 1700000560), "the expanded combat window eventually expires without resetting each scan");
	}

	protected void TestWorkQueue()
	{
		int failuresBefore = m_iFailures;
		TestQueueDeduplication();
		TestSingleGroupThroughput();
		TestWorkerTimeBudget();
		TestWakeRoundRobin();
		TestWakePriorityAndAging();
		TestWakeRetryBackoff();
		TestStaleCacheCandidate();
		TestDisabledAndRetiredWork();
		TestOversizedCacheFairness();
		TestCacheFairnessCadence();
		TestCacheRetirementAndNullCasualty();
		Print(string.Format("[IA][DynamicAIWorkQueueTest] failures=%1", m_iFailures - failuresBefore), LogLevel.NORMAL);
	}

	protected void TestQueueDeduplication()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture candidate = new IA_DynamicAIWorkCacheFixture();
		candidate.ConfigureCandidate(queue, 3);
		queue.EnqueueCache(candidate, 1000);
		queue.EnqueueCache(candidate, 2000);
		Check(queue.GetReadyCount() == 1, "cache requests deduplicate");
		Check(queue.GetOldestCacheAge(2500) == 1500, "duplicate scans preserve the original cache request age");
		ref IA_DynamicAIWorkCacheFixture second = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture third = new IA_DynamicAIWorkCacheFixture();
		second.ConfigureCandidate(queue, 2);
		third.ConfigureCandidate(queue, 2);
		queue.EnqueueCache(second, 2000);
		queue.EnqueueCache(third, 3000);
		queue.CancelCache(candidate);
		Check(queue.GetOldestCacheAge(3500) == 1500, "cancelling the oldest of three candidates preserves FIFO age rather than swapping in the newest");
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		waking.ConfigureWake(queue, 8, false, 1000);
		queue.EnqueueWake(waking);
		queue.EnqueueWake(waking);
		Check(queue.GetWakeCount() == 1 && waking.GetWakeRequestedMs() == 1000, "wake deduplication preserves the original request timestamp");
		queue.Forget(candidate);
		queue.Forget(second);
		queue.Forget(third);
		queue.Forget(waking);
		Check(queue.GetReadyCount() == 0 && queue.GetWakeCount() == 0, "forgotten owners leave both queues");
	}

	protected void TestSingleGroupThroughput()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		waking.ConfigureWake(queue, 10, true, 1000);
		queue.EnqueueWake(waking);
		ref array<vector> players = {};
		queue.Service(players, 1000, true);
		Check(waking.m_iRestoredSoldiers == 4 && waking.GetUnrestoredCount() == 6, "one waking group can use all four attempt slots in a tick");
		queue.Service(players, 1100, true);
		Check(waking.m_iRestoredSoldiers == 8, "unfinished restoration continues on the next tick");
		queue.Service(players, 1200, true);
		Check(waking.m_iRestoredSoldiers == 10 && queue.GetWakeCount() == 0, "completed restoration is removed without extra spawn attempts");
	}

	protected void TestWorkerTimeBudget()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		waking.ConfigureWake(queue, 20, true, 1000);
		waking.m_iRestoreCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		queue.EnqueueWake(waking);
		ref IA_DynamicAIWorkCacheFixture candidate = new IA_DynamicAIWorkCacheFixture();
		candidate.ConfigureCandidate(queue, 2);
		queue.EnqueueCache(candidate, 1000);
		ref array<vector> players = {};
		queue.Service(players, 1000, true);
		Check(waking.m_iRestoredSoldiers == 1 && candidate.m_iCacheCalls == 0, "an operation reaching the elapsed budget prevents additional spawns and cache work");
		waking.m_iRestoreCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS + 7;
		queue.Service(players, 1100, true);
		Check(waking.m_iRestoredSoldiers == 2 && candidate.m_iCacheCalls == 0, "one indivisible expensive spawn can overrun but no second operation follows");
	}

	protected void TestWakeRoundRobin()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIWorkCacheFixture first = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture second = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture third = new IA_DynamicAIWorkCacheFixture();
		first.ConfigureWake(queue, 10, true, 1000);
		second.ConfigureWake(queue, 10, true, 1000);
		third.ConfigureWake(queue, 10, true, 1000);
		first.m_aTrace = trace;
		second.m_aTrace = trace;
		third.m_aTrace = trace;
		first.m_sLabel = "A";
		second.m_sLabel = "B";
		third.m_sLabel = "C";
		queue.EnqueueWake(first);
		queue.EnqueueWake(second);
		queue.EnqueueWake(third);
		ref array<vector> players = {};
		queue.Service(players, 1000, true);
		Check(trace.Count() == 4 && trace[0] == "A" && trace[1] == "B" && trace[2] == "C" && trace[3] == "A", "three equally urgent groups receive round-robin attempts without starving the middle group");
		queue.Service(players, 1100, true);
		Check(trace.Count() == 8 && trace[4] == "B" && trace[5] == "C" && trace[6] == "A" && trace[7] == "B", "round-robin position survives the worker tick boundary");
	}

	protected void TestWakePriorityAndAging()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture background = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture urgent = new IA_DynamicAIWorkCacheFixture();
		background.ConfigureWake(queue, 20, false, 1000);
		urgent.ConfigureWake(queue, 100, true, 1000);
		background.m_iRestoreCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		urgent.m_iRestoreCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		queue.EnqueueWake(background);
		queue.EnqueueWake(urgent);
		ref array<vector> players = {};
		for (int now = 1000; now < 6000; now += 100)
		{
			queue.EnqueueWake(background);
			queue.Service(players, now, true);
		}
		Check(urgent.m_iRestoredSoldiers == 50 && background.m_iRestoredSoldiers == 0, "urgent work wins while background requests are younger than five seconds");
		queue.Service(players, 6000, true);
		Check(background.m_iRestoredSoldiers == 1, "a five-second-old background request progresses despite only one slow attempt fitting per tick");
		queue.Service(players, 6100, true);
		queue.Service(players, 6200, true);
		Check(background.m_iRestoredSoldiers == 2 && urgent.m_iRestoredSoldiers == 51, "aged background and urgent work continue to alternate without resetting request age");
	}

	protected void TestWakeRetryBackoff()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture urgent = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture background = new IA_DynamicAIWorkCacheFixture();
		urgent.ConfigureWake(queue, 2, true, 1000);
		urgent.m_bRestoreBackoff = true;
		background.ConfigureWake(queue, 8, false, 1000);
		queue.EnqueueWake(urgent);
		queue.EnqueueWake(background);
		ref array<vector> players = {};
		queue.Service(players, 1000, true);
		Check(urgent.m_iRestoreCalls == 1 && urgent.m_iRestoredSoldiers == 0 && background.m_iRestoredSoldiers == 4, "an urgent request in retry backoff does not consume attempts or hide ready background work");
		Check(queue.GetWakeCount() == 2, "retry backoff retains the unresolved urgent request");
		urgent.m_bRestoreBackoff = false;
		queue.Service(players, 1100, true);
		Check(urgent.m_iRestoredSoldiers == 2 && background.m_iRestoredSoldiers == 6, "an urgent request resumes first after its retry becomes available");
	}

	protected void TestStaleCacheCandidate()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture candidate = new IA_DynamicAIWorkCacheFixture();
		candidate.ConfigureCandidate(queue, 5);
		queue.EnqueueCache(candidate, 1000);
		// Eligibility changes after scanning: the production queue must ask again.
		candidate.m_bCaptureAllowed = false;
		ref array<vector> players = {"100 20 100"};
		queue.Service(players, 1100, true);
		Check(candidate.m_iCacheCalls == 1 && !candidate.IsCached() && candidate.m_iLastPlayerCount == 1, "stale cache candidates are revalidated with current players instead of deleted from old eligibility");
		Check(queue.GetReadyCount() == 0, "a rejected transaction returns to normal eligibility scanning");
		candidate.m_bCaptureAllowed = true;
		queue.EnqueueCache(candidate, 1200);
		candidate.m_iCandidateSoldiers = 3;
		queue.Service(players, 1300, true);
		Check(candidate.m_iCapturedSoldiers == 3, "capture consumes the current survivor roster rather than its earlier scan count");
	}

	protected void TestDisabledAndRetiredWork()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture candidate = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		candidate.ConfigureCandidate(queue, 3);
		waking.ConfigureWake(queue, 8, false, 1000);
		queue.EnqueueCache(candidate, 1000);
		queue.EnqueueWake(waking);
		ref array<vector> players = {};
		queue.Service(players, 1000, false);
		Check(queue.GetReadyCount() == 0 && candidate.m_iCacheCalls == 0 && waking.m_iRestoredSoldiers == 4, "turning OFF cancels pending cache work while existing soldiers keep restoring");
		waking.m_bOwnerLive = false;
		candidate.m_bOwnerLive = false;
		queue.EnqueueCache(candidate, 1100);
		queue.Service(players, 1100, true);
		Check(waking.m_iRestoredSoldiers == 4 && waking.m_iRetireCalls == 1, "retired wake owners are removed before another spawn attempt");
		Check(candidate.m_iCacheCalls == 0 && candidate.m_iRetireCalls == 1, "retired cache owners are removed before capture");
		Check(queue.GetReadyCount() == 0 && queue.GetWakeCount() == 0, "retired owners leave no pending work");
	}

	protected void TestOversizedCacheFairness()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture large = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		large.ConfigureCandidate(queue, IA_DynamicAISpawning.CACHE_SOLDIERS_PER_TICK + 1);
		waking.ConfigureWake(queue, 100, true, 1000);
		queue.EnqueueCache(large, 1000);
		queue.EnqueueWake(waking);
		ref array<vector> players = {};
		queue.Service(players, 1000, true);
		Check(!large.IsCached() && waking.m_iRestoredSoldiers == 4, "oversized cache work cannot be appended after restore work");
		queue.Service(players, 2999, true);
		Check(!large.IsCached() && waking.m_iRestoredSoldiers == 8, "continuous waking can defer an oversized group only until its aging boundary");
		queue.Service(players, 3000, true);
		Check(large.IsCached() && large.m_iCapturedSoldiers == IA_DynamicAISpawning.CACHE_SOLDIERS_PER_TICK + 1 && waking.m_iRestoredSoldiers == 8, "an aged oversized squad gets one exclusive atomic transaction without starving indefinitely");
		queue.Service(players, 3100, true);
		Check(waking.m_iRestoredSoldiers == 12, "urgent waking resumes on the tick following oversized cache work");
	}

	protected void TestCacheFairnessCadence()
	{
		ref IA_DynamicAIWorkQueueFixture queue = new IA_DynamicAIWorkQueueFixture();
		ref IA_DynamicAIWorkCacheFixture first = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture second = new IA_DynamicAIWorkCacheFixture();
		ref IA_DynamicAIWorkCacheFixture waking = new IA_DynamicAIWorkCacheFixture();
		first.ConfigureCandidate(queue, 2);
		second.ConfigureCandidate(queue, 2);
		first.m_iCacheCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		second.m_iCacheCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		waking.ConfigureWake(queue, 100, true, 1000);
		waking.m_iRestoreCostMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		queue.EnqueueCache(first, 1000);
		queue.EnqueueCache(second, 1000);
		queue.EnqueueWake(waking);
		ref array<vector> players = {};
		queue.Service(players, 3000, true);
		Check(first.IsCached() && !second.IsCached() && waking.m_iRestoredSoldiers == 0, "an aged cache candidate receives a reserved turn under a saturated wake budget");
		queue.Service(players, 3999, true);
		Check(!second.IsCached() && waking.m_iRestoredSoldiers == 1, "cache fairness cannot reserve another tick before one second passes");
		queue.Service(players, 4000, true);
		Check(second.IsCached() && waking.m_iRestoredSoldiers == 1, "another aged cache candidate receives its turn at the one-second boundary");
	}

	protected void TestCacheRetirementAndNullCasualty()
	{
		ref IA_DynamicAIGroupCacheFixture cache = new IA_DynamicAIGroupCacheFixture();
		ref IA_DynamicAIUnit first = new IA_DynamicAIUnit();
		ref IA_DynamicAIUnit second = new IA_DynamicAIUnit();
		cache.AddPendingForTest(first);
		cache.AddPendingForTest(second);
		cache.OnUnitKilled(null);
		Check(cache.GetUnrestoredCount() == 2 && cache.GetLogicalAliveCount() == 2 && !first.m_bDead && !second.m_bDead, "a null casualty callback cannot consume pending soldiers whose entity references are also null");
		cache.Retire();
		Check(cache.IsFinished() && !cache.IsCached() && !cache.IsWaking() && !cache.HasUrgentWake() && cache.GetUnrestoredCount() == 0 && cache.GetLogicalAliveCount() == 0, "retirement latches finished state and detaches the synthetic pending roster");
		cache.Retire();
		Check(cache.IsFinished() && !cache.IsCached() && cache.GetUnrestoredCount() == 0, "repeated retirement is idempotent");
	}

	protected void Check(bool passed, string description)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][DynamicAISpawningConfigTest] " + description, LogLevel.ERROR);
	}
}
#endif
