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
		TestBlockedEvictionProgress();
		TestCivilianDoesNotTrapBudgetDrain();
		TestTimeBudget();
		TestBudgetOffDrain();
		TestRetiredCost();
		TestCivilianBudgetIsolation();
		TestVehicleBudgetIsolation();
		TestCaptureSeedOverBudget();
		TestPopInPrefersFartherReserve();
		TestCombatOptionalThroughput();
		TestBudgetFlowTelemetry();
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
		eviction.Configure(worker, trace, "eviction", 0, 2, false);
		eviction.m_iEvictRemaining = 2;
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
		Check(single.m_iCreated == 1 && single.GetUnrestoredCount() == 11, "a positive budget stops mandatory restoration at the shared target");
		worker.RunService(active, 101, 1);
		Check(single.m_iCreated == 1 && single.GetUnrestoredCount() == 11, "remaining reserves stay cached until eviction or later capacity");
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
		worker.RunService(active, 1, 8);
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
			worker.RunService(active, 1 + tick * 100, 20);
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
			cache.SetNearestForTest((index + 1) * 100);
			retained.Insert(cache);
			active.Insert(cache);
		}
		for (int tick = 0; tick < 3; tick++)
			worker.RunService(active, 1 + tick * 100, 20);
		Check(trace.Count() == 3 && retained[0].m_iCreated == 3 && retained[1].m_iCreated == 0 && retained[2].m_iCreated == 0, "optional restoration prefers the nearest eligible squad");
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
			cache.SetNearestForTest((index + 1) * 100);
			retained.Insert(cache);
			active.Insert(cache);
		}
		for (int tick = 0; tick < 3; tick++)
			worker.RunService(active, 1 + tick * 100, 1);
		Check(trace.Count() == 3 && retained[0].m_iEvictions == 0 && retained[1].m_iEvictions == 0 && retained[2].m_iEvictions == 3, "eviction prefers the farthest overallocated squad");
	}

	protected void TestBlockedEvictionProgress()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture blocked = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture removable = new IA_DynamicAIBudgetServiceCacheFixture();
		blocked.Configure(worker, trace, "blocked", 0, 4, false);
		blocked.SetNearestForTest(5000);
		blocked.m_iRefusalMs = IA_DynamicAISpawning.WORK_BUDGET_MS;
		removable.Configure(worker, trace, "removable", 0, 4, false);
		removable.SetNearestForTest(3000);
		removable.m_iEvictRemaining = 4;
		ref array<IA_DynamicAIBudgetCache> active = {blocked, removable};
		worker.RunService(active, 1, 1);
		Check(removable.m_iEvictions == 0, "a slow refusal respects the current tick's time limit");
		worker.RunService(active, 101, 1);
		Check(removable.m_iEvictions == 4, "the next tick reaches removable troops behind a slow blocked farthest group");
		worker.RunService(active, 201, 1);
		blocked.m_iEvictRemaining = 1;
		worker.RunService(active, 301, 1);
		Check(blocked.m_iEvictions == 1, "a completed sweep revisits previously blocked groups");
	}

	protected void TestCivilianDoesNotTrapBudgetDrain()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref IA_DynamicAICivilianServiceCacheFixture civilian = new IA_DynamicAICivilianServiceCacheFixture();
		ref array<IA_DynamicAIGroupCache> groups = {civilian};
		worker.RunTick(groups, 1, 70);
		int callsBefore = civilian.m_iTryCacheCalls;
		worker.RunTick(groups, 101, 0);
		Check(!worker.IsActive(), "civilians alone do not retain the budget worker after a zero-budget drain");
		Check(civilian.m_iTryCacheCalls == callsBefore, "the drain cannot cache civilians when Dynamic AI may be off");
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

	protected void TestCivilianBudgetIsolation()
	{
		ref IA_DynamicAIBudgetCacheFixture civilian = new IA_DynamicAIBudgetCacheFixture();
		civilian.SetCivilianCacheForTest(true);
		civilian.EnableBudget();
		Check(!civilian.IsBudgetActive(), "civilian caches cannot enter the military budget allocator");

		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref IA_DynamicAICivilianServiceCacheFixture civilianService = new IA_DynamicAICivilianServiceCacheFixture();
		ref array<IA_DynamicAIGroupCache> groups = {civilianService};
		worker.RunTick(groups, 1, 160);
		Check(!civilianService.IsBudgetActive() && civilianService.m_iTryCacheCalls > 0, "budget ticks service civilians on a separate pool after military work");
	}

	protected void TestVehicleBudgetIsolation()
	{
		ref IA_DynamicAIBudgetCacheFixture vehicle = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup crew = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(vehicle, 4);
		crew.SetVehicleCrewForTest(true);
		vehicle.Init(crew);
		vehicle.EnableBudget();
		Check(!vehicle.IsBudgetActive(), "vehicle crews cannot enter the military soldier budget");

		ref IA_DynamicAIBudgetCacheFixture mortar = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup mortarCrew = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(mortar, 1);
		mortarCrew.SetSeatedAssignedMortarForTest(true);
		mortar.Init(mortarCrew);
		mortar.EnableBudget();
		Check(!mortar.IsBudgetActive(), "assigned mortar-vehicle gunners cannot enter the military soldier budget");
	}

	protected void TestCaptureSeedOverBudget()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture live = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture contested = new IA_DynamicAIBudgetServiceCacheFixture();
		live.Configure(worker, trace, "live", 0, 1, false);
		contested.Configure(worker, trace, "contested", 3, 0, false);
		ref array<IA_DynamicAIBudgetCache> active = {live, contested};
		worker.RunService(active, 1, 1);
		Check(contested.m_iCreated == 0, "a full target withholds a fully cached squad that nobody is capturing");
		contested.RequestCaptureSeed();
		worker.RunService(active, 101, 1);
		Check(contested.m_iCreated == 1 && contested.GetUnrestoredCount() == 2, "a contested objective seeds exactly one defender over the target");
		worker.RunService(active, 201, 1);
		Check(contested.m_iCreated == 1, "the seeded squad's remaining reserves wait for nearest-first capacity");
	}

	protected void TestPopInPrefersFartherReserve()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture squad = new IA_DynamicAIBudgetServiceCacheFixture();
		squad.Configure(worker, trace, "popin", 2, 1, false);
		IA_DynamicAIUnit close = squad.GetUnitForTest(0);
		IA_DynamicAIUnit farther = squad.GetUnitForTest(1);
		close.m_fNearestPlayerM = 10;
		farther.m_fNearestPlayerM = 80;
		Check(squad.RestoreBudgetUnit(1, true) && farther.m_bRestored && !close.m_bRestored, "optional restore prefers a reserve outside the pop-in band");
		Check(squad.RestoreBudgetUnit(2, true) && close.m_bRestored, "the close reserve still restores when it is the only remaining slot");
	}

	protected void TestCombatOptionalThroughput()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture fight = new IA_DynamicAIBudgetServiceCacheFixture();
		fight.Configure(worker, trace, "fight", 8, 0, false);
		fight.m_bCombatForTest = true;
		fight.SetNearestForTest(120);
		ref array<IA_DynamicAIBudgetCache> active = {fight};
		worker.RunService(active, 1, 10);
		Check(fight.m_iCreated == 6, "a nearby combat squad receives two extra optional restorations in the same tick");
	}

	protected void TestBudgetFlowTelemetry()
	{
		ref IA_DynamicAIBudgetControllerFixture worker = new IA_DynamicAIBudgetControllerFixture();
		ref array<string> trace = {};
		ref IA_DynamicAIBudgetServiceCacheFixture live = new IA_DynamicAIBudgetServiceCacheFixture();
		ref IA_DynamicAIBudgetServiceCacheFixture contested = new IA_DynamicAIBudgetServiceCacheFixture();
		live.Configure(worker, trace, "live", 0, 1, false);
		contested.Configure(worker, trace, "contested", 3, 0, true);
		contested.SetNearestForTest(90);
		ref array<IA_DynamicAIBudgetCache> active = {live, contested};
		worker.RunService(active, 1, 1);
		Check(worker.GetDeniedOverBudgetForTest() > 0, "a full target records denied mandatory restorations");
		contested.RequestCaptureSeed();
		worker.RunService(active, 101, 1);
		Check(worker.GetCaptureSeedsForTest() == 1, "a capture seed is counted when it creates the extra defender");
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
