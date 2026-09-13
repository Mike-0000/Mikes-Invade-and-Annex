#ifdef WORKBENCH
// Decision table only. Does not spawn a world or a live field-base.
[WorkbenchPluginAttribute(name: "IA Complete + Defend action table", wbModules: {"ResourceManager"})]
class IA_AdminCompleteAndDefendTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.None) == IA_AdminCompleteAndDefend.START_CHAIN, "none starts chain");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Completed) == IA_AdminCompleteAndDefend.START_CHAIN, "completed starts chain");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Cancelled) == IA_AdminCompleteAndDefend.START_CHAIN, "cancelled starts chain");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Placing) == IA_AdminCompleteAndDefend.WAIT_PLACING, "placing waits");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Seize) == IA_AdminCompleteAndDefend.BYPASS_TO_DEFEND, "seize bypasses to defense");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Regroup) == IA_AdminCompleteAndDefend.BYPASS_TO_DEFEND, "retired regroup still bypasses");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Warning) == IA_AdminCompleteAndDefend.BYPASS_TO_DEFEND, "retired warning still bypasses");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Defend) == IA_AdminCompleteAndDefend.ALREADY_DEFENDING, "defend already active");
		Check(IA_AdminCompleteAndDefend.Resolve(IA_BaseObjectivePhase.Failed) == IA_AdminCompleteAndDefend.RECOVER_FAILED, "failed recovers");
		Print(string.Format("[IA][CompleteAndDefendTest] completed failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Check(bool passed, string label)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][CompleteAndDefendTest] FAIL " + label, LogLevel.ERROR);
	}
}

#endif
