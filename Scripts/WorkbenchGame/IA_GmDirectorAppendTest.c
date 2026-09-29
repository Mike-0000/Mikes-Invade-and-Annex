#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA GM director live-append group regression", wbModules: {"ResourceManager"})]
class IA_GmDirectorAppendTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		Check(IA_GmDirector.PickLiveGroupForAppend(1001, 0) == 0, "the active mission group wins over a leftover director Live id");
		Check(IA_GmDirector.PickLiveGroupForAppend(-1, 2) == 2, "authored map AOs are Live while director Live id is still -1");
		Check(IA_GmDirector.PickLiveGroupForAppend(1001, -1) == 1001, "director Live is used when no mission group is active");
		Check(IA_GmDirector.PickLiveGroupForAppend(-1, -1) == -1, "no active AO still needs BeginLiveGroup");
		Print(string.Format("[IA][GmDirectorAppendTest] failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Check(bool cond, string message)
	{
		if (cond)
			return;
		m_iFailures = m_iFailures + 1;
		Print("[IA][GmDirectorAppendTest] FAIL: " + message, LogLevel.ERROR);
	}
}
#endif
