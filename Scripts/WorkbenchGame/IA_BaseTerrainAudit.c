#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA base terrain audit", wbModules: {"WorldEditor"})]
class IA_BaseTerrainAudit : WorkbenchPlugin
{
	override void RunCommandline()
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		string worldPath = "Worlds/IA_Kolguyev.ent";
		string requestedWorld;
		if (editor.GetCmdLine("-iaAuditWorld", requestedWorld) && !requestedWorld.IsEmpty())
			worldPath = requestedWorld;
		string absolutePath;
		if (Workbench.GetAbsolutePath(worldPath, absolutePath))
			worldPath = absolutePath;
		Print("[IA][TerrainAudit] Loading read-only terrain audit", LogLevel.NORMAL);
		if (!editor.SetOpenedResource(worldPath))
		{
			Print("[IA][TerrainAudit] Load failed", LogLevel.ERROR);
			Workbench.Exit(1);
			return;
		}
		WorldEditorAPI api = editor.GetApi();
		Print(string.Format("[IA][TerrainAudit] terrain_y=%1 game_world=%2 editor_world=%3", api.GetTerrainSurfaceY(6862.125, 6738.355), GetGame().GetWorld(), api.GetWorld()), LogLevel.NORMAL);
		IA_TerrainAuditPlacer placer = new IA_TerrainAuditPlacer();
		placer.Audit(api.GetWorld(), false);
		placer.Audit(api.GetWorld(), true);
		Print("[IA][TerrainAudit] DONE", LogLevel.NORMAL);
		Workbench.Exit(0);
	}
}

class IA_TerrainAuditPlacer : IA_DynamicSitePlacer
{

	void Audit(BaseWorld world, bool includeRally)
	{
		SetAuditWorld(world);
		m_iGroupId = 0;
		m_iSeed = 12345;
		m_Rng.SetSeed(m_iSeed);
		InitializeSurvey(IA_DynamicSiteSizeMode.Auto);
		if (!includeRally)
			m_aLayouts.Remove(m_aLayouts.Count() - 1);
		Print(string.Format("[IA][TerrainAudit] includeRally=%1 seed=%2", includeRally, m_iSeed), LogLevel.NORMAL);
		Print(string.Format("[IA][TerrainAudit] circles=%1", m_aSearchCenters.Count()), LogLevel.NORMAL);
		int slices = 0;
		int started = System.GetTickCount();
		while (!m_bSurveyDone && m_aShortlist.Count() < MAX_SHORTLIST && (HasSurveyWork() || m_aShortlist.IsEmpty()) && slices < 10000)
		{
			StepSurvey();
			slices++;
		}
		foreach (string reason, int count : m_Rejections)
			Print(string.Format("[IA][TerrainAudit] reject %1=%2", reason, count), LogLevel.NORMAL);
		Print(string.Format("[IA][TerrainAudit] anchors=%1 fits=%2 slices=%3 work_ms=%4", m_iSampleCount, m_aShortlist.Count(), slices, System.GetTickCount() - started), LogLevel.NORMAL);
	}
}
#endif
