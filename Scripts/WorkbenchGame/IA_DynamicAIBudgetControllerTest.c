#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI budget controller regression", wbModules: {"ResourceManager"})]
class IA_DynamicAIBudgetControllerTest : WorkbenchPlugin
{
	protected int m_iFailures;
	protected int m_iChecks;

	override void RunCommandline()
	{
		TestMandatoryPriorityAndSharedCap();
		TestSingleGroupThroughput();
		TestEvictionBeforeOptional();
		TestFullCapacity();
		TestFailedReservation();
		TestFailedAttemptCap();
		TestMandatoryCursorFairness();
		TestOptionalCursorFairness();
		TestEvictionCursorFairness();
		TestTimeBudget();
		TestBudgetOffDrain();
		TestRetiredCost();
		Print(string.Format("[IA][DynamicAIBudgetControllerTest] checks=%1 failures=%2", m_iChecks, m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestMandatoryPriorityAndSharedCap()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture optional = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture eviction = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture mandatory = new IA_DynamicAIBudgetServiceCacheFixture();
		optional.Configure(worker, trace, "optional", 8, 0, false);
		eviction.Configure(worker, trace, "eviction", 0, 4, false);
		eviction.m_iEvictRemaining = 4;
		mandatory.Configure(worker, trace, "mandatory", 2, 0, true);
		ref array<IA_DynamicAIBudgetCache> active = {optional, eviction, mandatory};
		worker.RunService(active, 1, 4);
		Check(trace.Count() == 4 && mandatory.m_iCreated == 2 && eviction.m_iEvictions == 2 && optional.m_iCreated == 0, "mandatory restoration and eviction share one four-operation allowance");
		if (trace.Count() == 4)
			Check(trace[0] == "mandatory:restore" && trace[1] == "mandatory:restore" && trace[2] == "eviction:evict", "mandatory restoration runs before eviction despite registry order");
	}

	protected void TestSingleGroupThroughput()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture single = new IA_DynamicAIBudgetServiceCacheFixture();
		single.Configure(worker, trace, "single", 12, 0, true);
		ref array<IA_DynamicAIBudgetCache> active = {single};
		worker.RunService(active, 1, 1);
		Check(single.m_iCreated == 4, "one mandatory squad can use all four attempts and exceed the soft target");
		worker.RunService(active, 101, 1);
		Check(single.m_iCreated == 8 && single.GetUnrestoredCount() == 4, "remaining squad members progress on the following tick");
	}

	protected void TestEvictionBeforeOptional()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture optional = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture eviction = new IA_DynamicAIBudgetServiceCacheFixture();
		optional.Configure(worker, trace, "optional", 5, 0, false);
		eviction.Configure(worker, trace, "eviction", 0, 2, false);
		eviction.m_iEvictRemaining = 1;
		ref array<IA_DynamicAIBudgetCache> active = {optional, eviction};
		worker.RunService(active, 1, 2);
		Check(eviction.m_iEvictions == 1 && optional.m_iCreated == 1 && trace.Count() == 2, "optional restoration uses only capacity actually released by eviction");
		if (trace.Count() == 2)
			Check(trace[0] == "eviction:evict" && trace[1] == "optional:restore", "physical capacity is released before the replacement is created");
	}

	protected void TestFullCapacity()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture protectedLive = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture optional = new IA_DynamicAIBudgetServiceCacheFixture();
		protectedLive.Configure(worker, trace, "protected", 0, 2, false);
		optional.Configure(worker, trace, "optional", 2, 0, false);
		ref array<IA_DynamicAIBudgetCache> active = {protectedLive, optional};
		worker.RunService(active, 1, 2);
		Check(optional.m_iRestoreAttempts == 0 && trace.IsEmpty(), "full physical capacity blocks optional creation without deleting protected troops");
	}

	protected void TestFailedReservation()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture admitted = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture waiting = new IA_DynamicAIBudgetServiceCacheFixture();
		admitted.Configure(worker, trace, "admitted", 1, 0, false);
		admitted.m_iFailRemaining = 1;
		waiting.Configure(worker, trace, "waiting", 2, 0, false);
		ref array<IA_DynamicAIBudgetCache> active = {admitted, waiting};
		worker.RunService(active, 1, 1);
		Check(admitted.m_iRestoreAttempts == 1 && admitted.GetReservedCount() == 1 && admitted.GetBudgetCost() == 1, "a failed optional creation becomes a real admitted reservation");
		Check(waiting.m_iRestoreAttempts == 0, "another optional soldier cannot consume the failed soldier's reserved capacity");
		worker.RunService(active, 500, 1);
		Check(admitted.m_iRestoreAttempts == 1 && admitted.GetReservedCount() == 1 && waiting.m_iRestoreAttempts == 0, "backoff retains the reservation without repeated engine attempts");
		worker.RunService(active, 1001, 1);
		Check(admitted.m_iCreated == 1 && admitted.GetReservedCount() == 0 && admitted.GetBudgetCost() == 1, "admitted retry succeeds at full capacity and converts its reservation into one physical soldier");
		Check(waiting.m_iRestoreAttempts == 0, "successful retry does not accidentally free its occupied slot");
	}

	protected void TestFailedAttemptCap()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture failing = new IA_DynamicAIBudgetServiceCacheFixture();
		failing.Configure(worker, trace, "failing", 8, 0, true);
		failing.m_iFailRemaining = 8;
		ref array<IA_DynamicAIBudgetCache> active = {failing};
		worker.RunService(active, 1, 1);
		Check(failing.m_iRestoreAttempts == 4 && failing.m_iCreated == 0 && failing.GetReservedCount() == 4, "failed engine attempts consume the shared operation cap and retain every admitted slot");
		Check(failing.GetUnrestoredCount() == 8, "an all-failure tick never loses pending soldiers");
	}

	protected void TestMandatoryCursorFairness()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref array<ref IA_DynamicAIBudgetServiceCacheFixture> retained = {};
		ref array<IA_DynamicAIBudgetCache> active = {};
		for (int index = 0; index < 5; index++)
		{
			ref IA_DynamicAIBudgetServiceCacheFixture cache = new IA_DynamicAIBudgetServiceCacheFixture();
			cache.Configure(worker, trace, index.ToString(), 3, 0, true);
			cache.m_iOperationMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
			retained.Insert(cache);
			active.Insert(cache);
		}
		for (int tick = 0; tick < 5; tick++)
			worker.RunService(active, 1 + tick * 100, 1);
		bool fair = trace.Count() == 5;
		foreach (IA_DynamicAIBudgetServiceCacheFixture result : retained)
			fair = fair && result.m_iCreated == 1;
		Check(fair, "mandatory cursor reaches every squad when only one engine operation fits per tick");
	}

	protected void TestOptionalCursorFairness()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref array<ref IA_DynamicAIBudgetServiceCacheFixture> retained = {};
		ref array<IA_DynamicAIBudgetCache> active = {};
		for (int index = 0; index < 3; index++)
		{
			ref IA_DynamicAIBudgetServiceCacheFixture cache = new IA_DynamicAIBudgetServiceCacheFixture();
			cache.Configure(worker, trace, index.ToString(), 3, 0, false);
			cache.m_iOperationMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
			retained.Insert(cache);
			active.Insert(cache);
		}
		for (int tick = 0; tick < 3; tick++)
			worker.RunService(active, 1 + tick * 100, 20);
		bool fair = trace.Count() == 3;
		foreach (IA_DynamicAIBudgetServiceCacheFixture result : retained)
			fair = fair && result.m_iCreated == 1;
		Check(fair, "optional cursor persists across time-budget exhaustion instead of repeatedly favoring the first squad");
	}

	protected void TestEvictionCursorFairness()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref array<ref IA_DynamicAIBudgetServiceCacheFixture> retained = {};
		ref array<IA_DynamicAIBudgetCache> active = {};
		for (int index = 0; index < 3; index++)
		{
			ref IA_DynamicAIBudgetServiceCacheFixture cache = new IA_DynamicAIBudgetServiceCacheFixture();
			cache.Configure(worker, trace, index.ToString(), 0, 3, false);
			cache.m_iEvictRemaining = 3;
			cache.m_iOperationMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
			retained.Insert(cache);
			active.Insert(cache);
		}
		for (int tick = 0; tick < 3; tick++)
			worker.RunService(active, 1 + tick * 100, 1);
		bool fair = trace.Count() == 3;
		foreach (IA_DynamicAIBudgetServiceCacheFixture result : retained)
			fair = fair && result.m_iEvictions == 1;
		Check(fair, "eviction cursor reaches every overallocated squad under a one-operation time budget");
	}

	protected void TestTimeBudget()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture slow = new IA_DynamicAIBudgetServiceCacheFixture();
		slow.Configure(worker, trace, "slow", 8, 0, true);
		slow.m_iOperationMs = IA_DynamicAISpawning.WORK_BUDGET_MS + 7;
		ref array<IA_DynamicAIBudgetCache> active = {slow};
		worker.RunService(active, 1, 8);
		Check(slow.m_iCreated == 1, "an indivisible engine operation may exceed the soft time budget but prevents a second operation that tick");
		worker.RunService(active, 101, 8);
		Check(slow.m_iCreated == 2, "an oversized operation cannot permanently block future ticks");
	}

	protected void TestBudgetOffDrain()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture draining = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture optional = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture eviction = new IA_DynamicAIBudgetServiceCacheFixture();
		draining.Configure(worker, trace, "draining", 2, 0, false);
		draining.DisableBudget();
		optional.Configure(worker, trace, "optional", 2, 0, false);
		eviction.Configure(worker, trace, "eviction", 0, 3, false);
		eviction.m_iEvictRemaining = 3;
		ref array<IA_DynamicAIBudgetCache> active = {optional, eviction, draining};
		worker.RunService(active, 1, 0);
		Check(draining.m_iCreated == 2 && optional.m_iCreated == 0 && eviction.m_iEvictions == 0, "budget zero services the mandatory drain without optional admission or eviction");
	}

	protected void TestRetiredCost()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture retired = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture optional = new IA_DynamicAIBudgetServiceCacheFixture();
		retired.Configure(worker, trace, "retired", 2, 100, true);
		retired.m_bOwnerLive = false;
		optional.Configure(worker, trace, "optional", 2, 0, false);
		ref array<IA_DynamicAIBudgetCache> active = {retired, optional};
		worker.RunService(active, 1, 1);
		Check(retired.m_iRestoreAttempts == 0 && optional.m_iCreated == 1, "a retired owner neither receives restoration nor blocks remaining optional capacity");
	}

	protected void Check(bool condition, string description)
	{
		m_iChecks++;
		if (condition)
			return;
		m_iFailures++;
		Print("[IA][DynamicAIBudgetControllerTest] " + description, LogLevel.ERROR);
	}
}
#endif
