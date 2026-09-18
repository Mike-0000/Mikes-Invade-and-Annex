#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI budget lifecycle regression", wbModules: {"ResourceManager"})]
class IA_DynamicAIBudgetLifecycleTest : WorkbenchPlugin
{
	protected int m_iChecks;
	protected int m_iFailures;

	override void RunCommandline()
	{
		TestHybridTransitions();
		TestDisableWaitsForReserves();
		TestCasualtyLedger();
		TestRetuningPreservesAdmittedWork();
		TestApproachDoesNotForceFull();
		TestReserveSeedAdopt();
		TestHardCapCombatEviction();
		TestNearbyPreemptionEvictionGates();
		Print(string.Format("[IA][DynamicAIBudgetLifecycleTest] checks=%1 failures=%2", m_iChecks, m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestHybridTransitions()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 3);
		cache.Init(owner);
		ref IA_DynamicAIBudgetUnitFixture first = new IA_DynamicAIBudgetUnitFixture();
		ref IA_DynamicAIBudgetUnitFixture second = new IA_DynamicAIBudgetUnitFixture();
		ref IA_DynamicAIBudgetUnitFixture third = new IA_DynamicAIBudgetUnitFixture();
		first.m_bRestored = true;
		second.m_bRestored = true;
		third.m_bRestored = true;
		cache.AddForTest(first);
		cache.AddForTest(second);
		cache.AddForTest(third);
		cache.EnableBudget();
		cache.ReconcileForTest();
		Check(cache.IsBudgetActive() && !cache.IsCached() && !cache.IsPaused(), "an entirely live ledger enters budget mode without pausing its owner");
		Check(cache.GetLogicalAliveCount() == 3 && owner.GetDynamicAISuspendCountForTest() == 0 && owner.GetDynamicAIResumeCountForTest() == 0, "enabling budget preserves the current roster without replaying lifecycle callbacks");

		first.m_bRestored = false;
		owner.SetDynamicAIPhysicalForTest(2);
		cache.ReconcileForTest();
		Check(cache.IsCached() && !cache.IsPaused() && cache.ShouldPreserveGroup(), "partial virtualization preserves the group while leaving physical members active");
		Check(cache.GetLogicalAliveCount() == 3 && cache.GetUnrestoredCount() == 1 && owner.GetDynamicAISuspendCountForTest() == 0, "partial virtualization retains every survivor without suspending teammates");

		second.m_bRestored = false;
		third.m_bRestored = false;
		owner.SetDynamicAIPhysicalForTest(0);
		cache.ReconcileForTest();
		Check(cache.IsPaused() && owner.IsDynamicAIPaused() && owner.GetDynamicAISuspendCountForTest() == 1, "removing the last physical soldier pauses the owner exactly once");
		cache.ReconcileForTest();
		Check(owner.GetDynamicAISuspendCountForTest() == 1 && cache.GetUnrestoredCount() == 3, "repeated dormant reconciliation neither repeats suspension nor loses reserve slots");

		first.m_bRestored = true;
		owner.SetDynamicAIPhysicalForTest(1);
		cache.ReconcileForTest();
		Check(cache.IsCached() && !cache.IsPaused() && owner.GetDynamicAIResumeCountForTest() == 1, "the first returning soldier resumes the owner before the full roster returns");
		second.m_bRestored = true;
		third.m_bRestored = true;
		owner.SetDynamicAIPhysicalForTest(3);
		cache.ReconcileForTest();
		Check(!cache.IsCached() && !cache.IsPaused() && !cache.ShouldPreserveGroup(), "full restoration releases virtual empty-group protection");
		Check(cache.GetLogicalAliveCount() == 3 && owner.GetDynamicAIResumeCountForTest() == 2, "full completion retains current survivors and resumes final deferred owner work once");
		cache.ReconcileForTest();
		Check(owner.GetDynamicAIResumeCountForTest() == 2, "a completed roster does not repeat final owner resumption");
		first.m_bRestored = false;
		owner.SetDynamicAIPhysicalForTest(2);
		cache.ReconcileForTest();
		Check(cache.IsCached() && !cache.IsPaused() && cache.GetLogicalAliveCount() == 3 && cache.GetUnrestoredCount() == 1, "the same surviving roster supports another partial cache cycle");
		cache.Retire();
		Check(cache.IsFinished() && !cache.IsBudgetActive() && !cache.ShouldPreserveGroup() && cache.GetUnrestoredCount() == 0, "retiring a hybrid roster clears its records and releases empty-group protection");
	}

	protected void TestDisableWaitsForReserves()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 1);
		cache.Init(owner);
		ref IA_DynamicAIBudgetUnitFixture live = new IA_DynamicAIBudgetUnitFixture();
		ref IA_DynamicAIBudgetUnitFixture reserve = new IA_DynamicAIBudgetUnitFixture();
		live.m_bRestored = true;
		cache.AddForTest(live);
		cache.AddForTest(reserve);
		cache.EnableBudget();
		cache.ReconcileForTest();
		cache.DisableBudget();
		Check(cache.IsBudgetActive() && cache.IsExitPendingForTest() && cache.IsFullRequiredForTest(), "budget zero retains its worker and ledger until reserves finish returning");
		Check(cache.GetDesired() == 2 && cache.HasMandatoryWork() && cache.GetUnrestoredCount() == 1, "budget zero makes every current survivor mandatory instead of dropping queued reserves");
		cache.DisableBudget();
		Check(cache.IsBudgetActive() && cache.GetUnrestoredCount() == 1 && owner.GetDynamicAIResumeCountForTest() == 0, "repeated disable requests cannot clear an unfinished ledger");
		reserve.m_bRestored = true;
		owner.SetDynamicAIPhysicalForTest(2);
		cache.ReconcileForTest();
		Check(!cache.IsBudgetActive() && !cache.IsExitPendingForTest() && !cache.IsCached() && !cache.IsPaused(), "the completed disable drain returns to the legacy cache path");
		Check(cache.GetUnrestoredCount() == 0 && cache.GetLogicalAliveCount() == 0 && owner.GetDynamicAIPhysicalAliveCount() == 2, "drain completion clears only the ledger and keeps the physical survivors");
		Check(owner.GetDynamicAIResumeCountForTest() == 1 && !cache.HasMandatoryWork(), "the disable drain resumes final owner work once and removes its mandatory demand");
	}

	protected void TestCasualtyLedger()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 1);
		cache.Init(owner);
		ref IA_DynamicAIBudgetUnitFixture casualty = new IA_DynamicAIBudgetUnitFixture();
		ref IA_DynamicAIBudgetUnitFixture survivor = new IA_DynamicAIBudgetUnitFixture();
		casualty.m_bRestored = true;
		cache.AddForTest(casualty);
		cache.AddForTest(survivor);
		cache.EnableBudget();
		cache.ReconcileForTest();
		cache.OnUnitKilled(null);
		Check(cache.GetLogicalAliveCount() == 2 && cache.GetUnrestoredCount() == 1 && !casualty.m_bDead && !survivor.m_bDead, "a null casualty notification cannot alias records with null entity references");

		// Simulate the state already written by the entity death boundary. Entity
		// identity, damage events and native deletion need a live gameplay test.
		casualty.m_bDead = true;
		owner.SetDynamicAIPhysicalForTest(0);
		cache.OnUnitKilled(null);
		Check(cache.IsPaused() && cache.GetLogicalAliveCount() == 1 && cache.GetUnrestoredCount() == 1 && cache.ShouldPreserveGroup(), "loss of the last physical member pauses its surviving reserve without consuming it");
		cache.RequestWake();
		Check(cache.IsWaking() && cache.GetUnrestoredCount() == 1 && !cache.IsFullRequiredForTest() && !cache.HasMandatoryWork(), "a casualty wake does not force surviving reserves over the shared budget");
		cache.DisableBudget();
		survivor.m_bRestored = true;
		owner.SetDynamicAIPhysicalForTest(1);
		cache.ReconcileForTest();
		Check(!cache.IsBudgetActive() && casualty.m_bDead && owner.GetDynamicAIPhysicalAliveCount() == 1, "the OFF drain cannot refill a dead original squad slot");
	}

	protected void TestRetuningPreservesAdmittedWork()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 1);
		cache.Init(owner);
		ref IA_DynamicAIBudgetUnitFixture live = new IA_DynamicAIBudgetUnitFixture();
		ref IA_DynamicAIBudgetUnitFixture admitted = new IA_DynamicAIBudgetUnitFixture();
		live.m_bRestored = true;
		live.m_iBudgetLiveSinceMs = 1234;
		admitted.m_bBudgetAdmitted = true;
		admitted.m_iNextAttemptMs = 9000;
		admitted.m_iFailures = 2;
		cache.AddForTest(live);
		cache.AddForTest(admitted);
		cache.EnableBudget();
		cache.ReconcileForTest();
		cache.RequestWake();
		int requestedAt = cache.GetWakeRequestedMs();
		cache.SeedSettingsStateForTest();
		cache.ResetQuietPeriod();
		Check(!cache.IsCloseForTest() && cache.IsPlanInvalidForTest(), "settings changes clear prior close hysteresis and queued eviction eligibility");
		Check(cache.IsBudgetActive() && cache.IsCached() && !cache.IsPaused() && cache.GetLogicalAliveCount() == 2, "retuning preserves a hybrid roster and its live owner");
		Check(cache.IsWaking() && !cache.IsFullRequiredForTest() && cache.HasUrgentWake() && cache.HasMandatoryWork() && cache.GetWakeRequestedMs() == requestedAt, "retuning cannot cancel or re-age an existing admitted wake");
		Check(admitted.m_bBudgetAdmitted && !admitted.m_bRestored && admitted.m_iNextAttemptMs == 9000 && admitted.m_iFailures == 2, "retuning preserves admission and restoration retry history");
		Check(live.m_iBudgetLiveSinceMs == 1234, "retuning retains the soldier's actual live age");
		Check(cache.GetEvictionRetryAtForTest() == 9000 && cache.GetLastCasualtyForTest() == 77, "retuning keeps failure backoff and casualty event time");
		cache.DisableBudget();
		cache.SeedSettingsStateForTest();
		cache.ResetQuietPeriod();
		Check(cache.IsExitPendingForTest() && cache.IsFullRequiredForTest() && cache.HasMandatoryWork() && cache.GetUnrestoredCount() == 1, "retuning cannot interrupt an unfinished OFF drain");
	}

	protected void TestApproachDoesNotForceFull()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 0);
		cache.Init(owner);
		ref IA_DynamicAIBudgetUnitFixture reserve = new IA_DynamicAIBudgetUnitFixture();
		cache.AddForTest(reserve);
		cache.EnableBudget();
		cache.ReconcileForTest();
		ref array<vector> players = {};
		players.Insert("0 0 0");
		cache.CheckProtectionForTest(players);
		Check(cache.IsCloseForTest() && cache.IsCached() && !cache.IsFullRequiredForTest() && !cache.HasMandatoryWork(), "walking into a cached objective does not force-full restore over the budget");
	}

	protected void TestReserveSeedAdopt()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 1);
		cache.Init(owner);
		ref array<ref IA_DynamicAIUnit> seeds = {};
		ref IA_DynamicAIUnit seed = new IA_DynamicAIUnit();
		seed.SeedReserve("{0000000000000000}Prefabs/Characters/Test.et", "200 20 200");
		seeds.Insert(seed);
		cache.AdoptReserveSeeds(seeds);
		Check(cache.IsBudgetActive() && cache.IsCached() && cache.GetUnrestoredCount() == 1 && cache.GetLogicalAliveCount() == 1, "reserve-first spawn records enter the ledger as unrestored survivors");
		Check(!seed.m_bRestored && !seed.m_bBudgetAdmitted && seed.GetPosition() == "200 20 200", "a seeded reserve keeps its prefab transform and waits for nearest-first restore");
	}

	protected void TestHardCapCombatEviction()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 1);
		cache.Init(owner);
		cache.EnableBudget();
		cache.SetLastCasualtyForTest(System.GetTickCount());
		cache.SetNearestForTest(120);
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		bool previousHard = tuning.m_bDynamicAIHardCap;
		tuning.m_bDynamicAIHardCap = false;
		Check(cache.CombatBlocksEvictionForTest(), "recent combat holds a nearby squad together while the shared target has room");
		cache.SetNearestForTest(4000);
		Check(!cache.CombatBlocksEvictionForTest(), "combat does not hold a squad when no player is inside wake distance");
		cache.SetNearestForTest(120);
		tuning.m_bDynamicAIHardCap = true;
		Check(!cache.CombatBlocksEvictionForTest(), "the admin hard cap lets overallocated combat squads be evicted by distance");
		tuning.m_bDynamicAIHardCap = previousHard;
	}

	protected void TestNearbyPreemptionEvictionGates()
	{
		ref IA_DynamicAIBudgetCacheFixture cache = new IA_DynamicAIBudgetCacheFixture();
		ref IA_AiGroup owner = IA_AiGroup.CreateDynamicAIBudgetOwnerForTest(cache, 4);
		cache.Init(owner);
		cache.EnableBudget();
		cache.SetNearestForTest(2000);
		Check(cache.EvictionDelayMsForTest() == IA_DynamicAISpawning.GetTuning().m_iDynamicAIEvictDelaySec * 1000, "eviction still waits the configured delay when nobody closer is waiting");
		Check(cache.MinLiveMsForTest(2000) == IA_DynamicAISpawning.GetTuning().m_iDynamicAIMinLiveSec * 1000, "far min-live stays at the configured dwell without preemption");
		cache.SetAllocation(0, 1000);
		Check(!cache.CanStartBudgetEvictionForTest(1000), "a freshly overallocated far group cannot evict during the ordinary delay");
		Check(!cache.CanStartBudgetEvictionForTest(4000), "the ordinary delay still applies a few seconds later when nobody closer is waiting");
		cache.SetNearbyPreemptForTest(true);
		Check(cache.EvictionDelayMsForTest() == 0, "nearby waiting demand skips the allocation-reduction delay on a clearly farther group");
		Check(cache.MinLiveMsForTest(2000) == IA_DynamicAISpawning.PREEMPT_MIN_LIVE_SEC * 1000, "a 2000 m soldier is removable after the short preempt min-live");
		Check(cache.MinLiveMsForTest(100) == IA_DynamicAISpawning.GetTuning().m_iDynamicAIMinLiveSec * 1000, "a 100 m soldier keeps the full min-live under preemption");
		Check(cache.CanStartBudgetEvictionForTest(1000), "preemption lets the far group start eviction on the first tick");

		cache.SetLastCasualtyForTest(System.GetTickCount());
		cache.SetNearestForTest(800);
		IA_Config tuning = IA_DynamicAISpawning.GetTuning();
		bool previousHard = tuning.m_bDynamicAIHardCap;
		tuning.m_bDynamicAIHardCap = false;
		Check(!cache.CombatBlocksEvictionForTest(), "combat does not hold a clearly farther squad when nearby demand is waiting");
		cache.SetNearbyPreemptForTest(false);
		cache.SetNearestForTest(800);
		Check(cache.CombatBlocksEvictionForTest(), "combat still holds a squad that is not being preempted");
		tuning.m_bDynamicAIHardCap = previousHard;
	}

	protected void Check(bool passed, string description)
	{
		m_iChecks++;
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][DynamicAIBudgetLifecycleTest] " + description, LogLevel.ERROR);
	}
}
#endif
