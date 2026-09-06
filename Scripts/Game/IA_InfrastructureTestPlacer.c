#ifdef WORKBENCH
class IA_InfrastructureTestPlacer : IA_DynamicSitePlacer
{
	int m_iFailures;
	float m_fTestSpan;
	int m_iRemaining;
	string m_sBlockedId;

	void Check(bool passed, string label)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][InfrastructureTest] FAIL " + label, LogLevel.ERROR);
	}

	bool CountRemaining(IEntity ent)
	{
		m_iRemaining++;
		return true;
	}

	int RunTests(BaseWorld world)
	{
		SetAuditWorld(world);
		m_Rejections = new map<string, int>();
		int started = System.GetTickCount();
		int attempts;
		for (int id = 0; id <= IA_DynamicSiteLayout.LAYOUT_RALLY_POST; id++)
		{
			IA_DynamicSiteLayout layout = IA_DynamicSiteLayout.CreateById(id);
			foreach (IA_DynamicSiteModule mod : layout.m_aModules)
			{
				if (mod.m_iRole != IA_DynamicSiteModuleRole.Dressing)
					continue;
				Check(!mod.m_bRequired, "optional facility " + mod.m_sId);
				for (int mode = 0; mode < 3; mode++)
				{
					m_fTestSpan = mode * 0.06;
					if (mode == 2)
						m_fTestSpan = 0.5;
					IA_DynamicSiteInstance site = IA_DynamicSiteInstance.Create(1, 0, vector.Zero, mode * 37, layout, null);
					bool spawned = SpawnModule(site, mod);
					Check(spawned == (mode != 2), "flat/gentle/steep support " + mod.m_sId);
					if (spawned)
						Check(site.CountExpandedEntities() <= mod.m_iEstimatedExpandedEntities, "complete prefab budget " + mod.m_sId);
					Check(site.DeleteRoots(), "preview site cleanup accepted");
					attempts++;
				}
			}
		}
		// A failed optional facility must not prevent following modules spawning.
		m_fTestSpan = 0;
		m_ActiveLayout = IA_DynamicSiteLayout.CreateRallyPost();
		for (int i = m_ActiveLayout.m_aModules.Count() - 1; i >= 0; i--)
		{
			if (m_ActiveLayout.m_aModules[i].m_iRole != IA_DynamicSiteModuleRole.Dressing)
				m_ActiveLayout.m_aModules.Remove(i);
		}
		m_sBlockedId = m_ActiveLayout.m_aModules[0].m_sId;
		m_Building = IA_DynamicSiteInstance.Create(2, 0, vector.Zero, 0, m_ActiveLayout, null);
		m_iWorkStep = 2;
		m_iModuleIndex = 0;
		int slices;
		while (m_Building && m_Building.GetRootCount() < 2 && slices < 100)
		{
			StepSpawnModules();
			slices++;
		}
		Check(m_Building && m_Building.GetRootCount() == 2 && m_iWorkStep == 2, "blocked optional module continues construction");
		if (m_Building)
			Check(m_Building.DeleteRoots(), "preview construction cleanup accepted");
		m_Building = null;
		m_sBlockedId = "";
		m_iRemaining = 0;
		world.QueryEntitiesBySphere(vector.Zero, 500, CountRemaining);
		Check(m_iRemaining == 0, "cleanup leaves no scene children");
		Print(string.Format("[IA][InfrastructureTest] attempts=%1 work_ms=%2 remaining=%3", attempts, System.GetTickCount() - started, m_iRemaining), LogLevel.NORMAL);
		return m_iFailures;
	}

	override protected bool IsOcean(vector point) { return false; }
	override protected bool ValidatePlayerClearance(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, IA_DynamicSiteInstance ignoreSite) { return true; }
	override protected bool SampleModuleSupport(vector worldMat[4], notnull IA_DynamicSiteModule mod, out float originY)
	{
		return mod.ResolveSupportHeight(0, -m_fTestSpan * 0.5, m_fTestSpan * 0.5, originY);
	}
	override protected bool IsModuleVolumeClear(vector worldMat[4], notnull IA_DynamicSiteModule mod, IA_DynamicSiteInstance ignoreSite = null)
	{
		if (mod.m_sId == m_sBlockedId)
			return false;
		return super.IsModuleVolumeClear(worldMat, mod, ignoreSite);
	}
}
#endif
