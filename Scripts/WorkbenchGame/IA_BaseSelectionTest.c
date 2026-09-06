#ifdef WORKBENCH
// Synthetic terrain isolates size selection from map geometry and player movement.
[WorkbenchPluginAttribute(name: "IA base selection regression", wbModules: {"ResourceManager"})]
class IA_BaseSelectionTest : WorkbenchPlugin
{
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IASelectionTest");
		IA_SelectionTestPlacer placer = new IA_SelectionTestPlacer();
		int failures = placer.RunTests(preview.GetRef());
		Print(string.Format("[IA][SelectionTest] completed failures=%1", failures), LogLevel.NORMAL);
		Workbench.Exit(failures);
	}
}

#endif
