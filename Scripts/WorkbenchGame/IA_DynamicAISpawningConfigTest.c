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

		Print(string.Format("[IA][DynamicAISpawningConfigTest] failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
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
