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

	protected void Check(bool passed, string description)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][DynamicAISpawningConfigTest] " + description, LogLevel.ERROR);
	}
}
#endif
