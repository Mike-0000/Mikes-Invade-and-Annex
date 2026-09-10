#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI budget config regression", wbModules: {"ResourceManager"})]
class IA_DynamicAIBudgetConfigTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		ref IA_Config source = new IA_Config();
		ref IA_Config restored = new IA_Config();
		Check(!source.m_bDynamicAISpawningEnabled && source.m_iDynamicAIBudget == 160, "new configurations retain the legacy toggle and a budget of 160");

		source.m_iDynamicAIBudget = 235;
		Check(IA_Config.UnpackDynamicAIBudget(restored, IA_Config.PackDynamicAIBudget(source)) && restored.m_iDynamicAIBudget == 235, "the admin token retains the chosen integer budget");
		Check(IA_Config.UnpackDynamicAIBudget(restored, "0") && restored.m_iDynamicAIBudget == 0, "zero explicitly selects distance-only spawning");
		Check(IA_Config.UnpackDynamicAIBudget(restored, "-10") && restored.m_iDynamicAIBudget == 0, "negative admin values clamp to zero");
		Check(IA_Config.UnpackDynamicAIBudget(restored, "999999") && restored.m_iDynamicAIBudget == 2000, "oversized admin values clamp to the supported maximum");
		Check(!IA_Config.UnpackDynamicAIBudget(restored, "") && restored.m_iDynamicAIBudget == 2000, "an empty admin token preserves the existing budget");
		Check(!IA_Config.UnpackDynamicAIBudget(restored, "80oops") && restored.m_iDynamicAIBudget == 2000, "trailing junk cannot partially change the budget");
		Check(!IA_Config.UnpackDynamicAIBudget(restored, "80.5") && restored.m_iDynamicAIBudget == 2000, "fractional input is rejected at the integer wire boundary");
		Check(!IA_Config.UnpackDynamicAIBudget(restored, "9999999999999999999") && restored.m_iDynamicAIBudget == 2000, "integer overflow cannot silently disable the budget");

		ref IA_DynamicAISpawningOverridesFixture saved = new IA_DynamicAISpawningOverridesFixture();
		ref IA_DynamicAISpawningOverridesFixture loaded = new IA_DynamicAISpawningOverridesFixture();
		source.m_bDynamicAISpawningEnabled = true;
		source.m_iDynamicAIBudget = 235;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(restored.m_bDynamicAISpawningEnabled && restored.m_iDynamicAIBudget == 235, "enabled custom budget survives the server profile roundtrip");

		source.m_bDynamicAISpawningEnabled = false;
		source.m_iDynamicAIBudget = 0;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(!restored.m_bDynamicAISpawningEnabled && restored.m_iDynamicAIBudget == 0, "disabled master switch and zero budget roundtrip independently");

		restored.m_iDynamicAIBudget = 320;
		loaded.Decode("{\"v\":2,\"dynamicAISpawning\":1}");
		loaded.ApplyTo(restored);
		Check(restored.m_bDynamicAISpawningEnabled && restored.m_iDynamicAIBudget == 320, "an older profile preserves an explicit mission budget");
		restored.m_iDynamicAIBudget = IA_Config.DYNAMIC_AI_BUDGET_DEFAULT;
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 160, "an older profile also preserves the default budget");

		loaded.Decode("{\"dynamicAIBudget\":\"invalid\"}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 160, "malformed persisted budgets preserve the mission setting");
		loaded.Decode("{\"dynamicAIBudget\":80.5}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 160, "fractional persisted values cannot truncate silently");
		loaded.Decode("{\"dynamicAIBudget\":9999999999999999999}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 160, "overflow-sized persisted values preserve the mission setting");
		loaded.Decode("{\"dynamicAIBudget\":-1}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 0, "negative persisted budgets clamp to zero");
		loaded.Decode("{\"dynamicAIBudget\":2001}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == 2000, "persisted budgets obey the upper bound");

		source.m_iDynamicAIBudget = 5000;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(source.m_iDynamicAIBudget == 2000 && restored.m_iDynamicAIBudget == 2000, "saving clamps direct mission configuration before persistence");
		source.m_iDynamicAIBudget = -10;
		Check(IA_Config.PackDynamicAIBudget(source) == "0" && source.m_iDynamicAIBudget == 0, "admin packing also clamps direct mission configuration");

		Print(string.Format("[IA][DynamicAIBudgetConfigTest] failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Check(bool condition, string description)
	{
		if (condition)
			return;
		m_iFailures++;
		Print("[IA][DynamicAIBudgetConfigTest] " + description, LogLevel.ERROR);
	}
}
#endif
