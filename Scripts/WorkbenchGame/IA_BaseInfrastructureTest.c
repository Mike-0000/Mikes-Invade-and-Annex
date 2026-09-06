#ifdef WORKBENCH
// Real prefab construction/cleanup with controlled support and rejection inputs.
[WorkbenchPluginAttribute(name: "IA base infrastructure regression", wbModules: {"ResourceManager"})]
class IA_BaseInfrastructureTest : WorkbenchPlugin
{
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAInfrastructureTest");
		IA_InfrastructureTestPlacer placer = new IA_InfrastructureTestPlacer();
		int failures = placer.RunTests(preview.GetRef());
		Print(string.Format("[IA][InfrastructureTest] completed failures=%1", failures), LogLevel.NORMAL);
		Workbench.Exit(failures);
	}
}

#endif
