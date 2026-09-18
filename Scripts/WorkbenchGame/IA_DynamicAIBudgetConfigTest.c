#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI budget config regression", wbModules: {"ResourceManager"})]
class IA_DynamicAIBudgetConfigTest : WorkbenchPlugin
{
	protected int m_iFailures;
	protected int m_iChecks;

	override void RunCommandline()
	{
		ref IA_Config source = new IA_Config();
		ref IA_Config restored = new IA_Config();
		Check(!source.m_bDynamicAISpawningEnabled && source.m_iDynamicAIBudget == IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "new configurations retain the legacy toggle and a budget of 70");

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
		Check(restored.m_iDynamicAIBudget == IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "an older profile also preserves the default budget");

		loaded.Decode("{\"dynamicAIBudget\":\"invalid\"}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "malformed persisted budgets preserve the mission setting");
		loaded.Decode("{\"dynamicAIBudget\":80.5}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "fractional persisted values cannot truncate silently");
		loaded.Decode("{\"dynamicAIBudget\":9999999999999999999}");
		loaded.ApplyTo(restored);
		Check(restored.m_iDynamicAIBudget == IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "overflow-sized persisted values preserve the mission setting");
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

		TestDynamicAITuning();
		Print(string.Format("[IA][DynamicAIBudgetConfigTest] checks=%1 failures=%2", m_iChecks, m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestDynamicAITuning()
	{
		ref IA_Config source = new IA_Config();
		ref IA_Config restored = new IA_Config();
		string defaults = "1000,1500,300,400,60,60,30,10,50,30,0";
		string customized = "1500,2500,500,650,80,90,45,15,75,45,1";
		Check(IA_Config.PackDynamicAIExtras(source) == defaults, "tuning defaults preserve the established distances, timers, capture seed and hard cap");
		Check(IA_Config.UnpackDynamicAIExtras(source, customized) && IA_Config.PackDynamicAIExtras(source) == customized, "all eleven tuning fields accept a valid custom admin token");
		Check(IA_Config.UnpackDynamicAIExtras(restored, IA_Config.PackDynamicAIExtras(source)) && IA_Config.PackDynamicAIExtras(restored) == customized, "all tuning fields survive the replicated/admin packed roundtrip");
		ref IA_Config legacy = new IA_Config();
		Check(IA_Config.UnpackDynamicAIExtras(legacy, "1000,1500,300,400,60,60,30,10,50") && IA_Config.PackDynamicAIExtras(legacy) == defaults, "a nine-field token keeps capture-seed and hard-cap defaults");

		ref array<string> malformed = {
			"",
			"1000,1500,300,400,60,60,30,10",
			"1000,1500,300,400,60,60,30,10,50,1",
			"1000,1500,300,400,60,60,30,10,50,30,0,1",
			"1000,1500,300,400,,60,30,10,50",
			"1000,1500,300,400,60,60,30,10,50,",
			",1000,1500,300,400,60,60,30,10,50",
			"1000,1500,300,400,60,60,30,10,invalid",
			"1000,1500,300,400,60,60,30.5,10,50",
			"1000,1500,300,400,60,60,30,10,99999999999999",
			"1000,1500,300,400,60,60,30,10,50junk"
		};
		foreach (string invalid : malformed)
		{
			Check(!IA_Config.UnpackDynamicAIExtras(restored, invalid) && IA_Config.PackDynamicAIExtras(restored) == customized, "malformed tuning leaves every existing field unchanged: " + invalid);
		}

		Check(IA_Config.UnpackDynamicAIExtras(restored, "-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0") && IA_Config.PackDynamicAIExtras(restored) == "100,150,50,100,0,10,5,1,0,5,0", "all tuning lower bounds clamp while preserving required distance gaps");
		Check(IA_Config.UnpackDynamicAIExtras(restored, "999999999,999999999,999999999,999999999,999999999,999999999,999999999,999999999,999999999,999999999,2") && IA_Config.PackDynamicAIExtras(restored) == "5000,7500,2000,3000,600,300,300,120,500,120,1", "safe large integers clamp to every upper bound without overflow");
		Check(IA_Config.UnpackDynamicAIExtras(restored, "100,150,2000,100,60,60,30,10,50,30,0") && IA_Config.PackDynamicAIExtras(restored) == "2050,2100,2000,2050,60,60,30,10,50,30,0", "close protection pushes release, wake and cache distances outward together");
		Check(IA_Config.UnpackDynamicAIExtras(restored, "5000,150,300,400,60,60,30,10,50,30,0") && restored.m_iDynamicAICacheDistanceM == 5050, "cache distance always remains at least fifty metres beyond wake distance");
		Check(IA_Config.UnpackDynamicAIExtras(restored, "100,150,300,3000,60,60,30,10,50,30,0") && restored.m_iDynamicAIWakeDistanceM == 3000 && restored.m_iDynamicAICacheDistanceM == 3050, "wake distance never falls inside the protected release distance");

		ref IA_DynamicAISpawningOverridesFixture saved = new IA_DynamicAISpawningOverridesFixture();
		ref IA_DynamicAISpawningOverridesFixture loaded = new IA_DynamicAISpawningOverridesFixture();
		source.m_bDynamicAISpawningEnabled = true;
		source.m_iDynamicAIBudget = 215;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(IA_Config.PackDynamicAIExtras(restored) == customized && restored.m_iDynamicAIBudget == 215 && restored.m_bDynamicAISpawningEnabled, "profile roundtrip preserves tuning alongside independent toggle and budget settings");
		loaded.Decode("{\"v\":2,\"dynamicAISpawning\":0,\"dynamicAIBudget\":0}");
		loaded.ApplyTo(restored);
		Check(IA_Config.PackDynamicAIExtras(restored) == customized && restored.m_iDynamicAIBudget == 0 && !restored.m_bDynamicAISpawningEnabled, "an old profile preserves explicit mission tuning while still applying its existing settings");
		ref IA_Config fresh = new IA_Config();
		loaded.ApplyTo(fresh);
		Check(IA_Config.PackDynamicAIExtras(fresh) == defaults, "an old profile preserves default tuning on a fresh mission");
		loaded.Decode("{\"dynamicAIExtras\":\"1000,1500,300,400,60,60,30,10,bad\"}");
		loaded.ApplyTo(restored);
		Check(IA_Config.PackDynamicAIExtras(restored) == customized, "malformed persisted tuning cannot partially overwrite a mission configuration");
		loaded.Decode("{\"dynamicAIExtras\":\"100,150,2000,100,0,0,0,0,-1,30,0\"}");
		loaded.ApplyTo(restored);
		Check(IA_Config.PackDynamicAIExtras(restored) == "2050,2100,2000,2050,0,10,5,1,0,30,0", "profile loading normalizes both bounds and distance relationships");
		loaded.Decode("{\"dynamicAIExtras\":\"\"}");
		loaded.ApplyTo(source);
		Check(IA_Config.PackDynamicAIExtras(source) == customized, "a later empty profile clears previous override presence without resetting mission tuning");
		source.m_iDynamicAICacheDistanceM = 0;
		source.m_iDynamicAICacheQuietSec = -1;
		saved.FillFrom(source);
		loaded.Decode(saved.Encode());
		loaded.ApplyTo(restored);
		Check(source.m_iDynamicAICacheDistanceM == 1550 && source.m_iDynamicAICacheQuietSec == 0 && IA_Config.PackDynamicAIExtras(restored) == IA_Config.PackDynamicAIExtras(source), "saving normalizes direct mission fields before writing the server profile");
	}

	protected void Check(bool condition, string description)
	{
		m_iChecks++;
		if (condition)
			return;
		m_iFailures++;
		Print("[IA][DynamicAIBudgetConfigTest] " + description, LogLevel.ERROR);
	}
}
#endif
