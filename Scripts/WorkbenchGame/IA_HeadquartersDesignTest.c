#ifdef WORKBENCH
// Headquarters recipes, grounding, sockets and the scrappy/HQ selection split.
// Layout data only: live spawning, replication and AI crewing need a game run.
[WorkbenchPluginAttribute(name: "IA headquarters design regression", wbModules: {"ResourceManager"})]
class IA_HeadquartersDesignTest : IA_BaseFoundationProbe
{
	override void RunCommandline()
	{
		CheckRecipes();
		CheckSelection();
		Print(string.Format("[IA][HeadquartersDesignTest] %1 recipes, 24 headings; failures=%2", 6 * IA_HeadquartersRecipes.VARIANTS, m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void CheckRecipes()
	{
		for (int size = 0; size < 6; size++)
		{
			for (int variant = 0; variant < IA_HeadquartersRecipes.VARIANTS; variant++)
			{
				ref IA_DynamicSiteLayout layout = IA_BaseDesignLibrary.CreateLayout(size, IA_BaseDesignLibrary.HQ_BASE + variant);
				Check(layout && layout.m_bComposed, "headquarters recipe exists");
				if (!layout)
					continue;
				Check(IA_HeadquartersSiteLayout.Cast(layout) != null, "headquarters layout class");
				Check(layout.m_iLayoutId == size, "size id");
				Check(IA_BaseDesignLibrary.IsHeadquarters(layout.m_iDesignVariant), "variant routes to headquarters history");
				Check(layout.m_aModules.Count() > 0 && layout.m_aModules[0].m_iRole == IA_DynamicSiteModuleRole.Hq, "module 0 is the command building");
				Check(layout.m_aModules.Count() <= IA_DynamicSitePlacer.MAX_ROOTS, "root cap");
				int expanded = 0;
				int walls = 0;
				int belt = 0;
				array<int> sides = {0, 0, 0, 0};
				foreach (IA_DynamicSiteModule module : layout.m_aModules)
				{
					Check(module.m_Prefab != string.Empty, "resolved prefab");
					expanded += module.m_iEstimatedExpandedEntities;
					if (module.m_iPerimeterSide >= 0)
						sides[module.m_iPerimeterSide] = sides[module.m_iPerimeterSide] + 1;
					if (module.m_sId.StartsWith("wall_"))
					{
						walls++;
						Check(module.m_iGroundingPolicy == IA_DynamicSiteGrounding.UprightPad && !module.m_bFollowTerrainPlane, "walls stand upright");
						Check(module.m_fMaxFoundationLiftM > 0 && module.m_fMaxFoundationLiftM < 1.0, "wall lift within buried foundation");
						Check(!module.m_bRequired, "wall runs are collectively required");
					}
					if (module.m_iRole == IA_DynamicSiteModuleRole.Dressing)
					{
						belt++;
						Check(module.m_iGroundingPolicy == IA_DynamicSiteGrounding.TerrainSegment && !module.m_bRequired, "belt snaps and never vetoes");
					}
					if (module.m_iRole == IA_DynamicSiteModuleRole.Tower || module.m_iRole == IA_DynamicSiteModuleRole.Hq)
						Check(!module.m_bFollowTerrainPlane && module.m_fMaxFoundationLiftM > 0, "buildings stand upright");
				}
				Check(walls >= 30, "concrete wall runs");
				Check(belt >= 10, "obstacle belt");
				Check(layout.HasPerimeterCoverage(sides), "four-sided coverage");
				int guns = layout.m_aEmplacements.Count();
				layout.PrepareEmplacements();
				Check(layout.m_aEmplacements.Count() == guns, "no legacy socket duplication");
				Check(guns >= 4 && guns <= IA_EmplacementProfile.GunCap(size), "shared weapon cap");
				array<int> armed = {0, 0, 0, 0};
				foreach (IA_DynamicBaseEmplacementSpec spec : layout.m_aEmplacements)
				{
					Check(spec.m_Socket != null, "casemate socket");
					if (spec.m_iSide >= 0)
						armed[spec.m_iSide] = armed[spec.m_iSide] + 1;
					for (int heading = 0; heading < 24; heading++)
					{
						vector parent[4], result[4], identity[4], local[4];
						layout.BuildRootTransform("100 20 100", heading * 15, parent);
						Math3D.MatrixIdentity4(identity);
						spec.m_Socket.Transform(identity, local);
						spec.m_Socket.Transform(parent, result);
						Check(Math.AbsFloat(vector.Distance(result[3], parent[3]) - local[3].Length()) < 0.01, "socket transform");
					}
				}
				Check(armed[0] >= 1 && armed[1] >= 1 && armed[2] >= 1 && armed[3] >= 1, "each concrete face has a casemate gun");
				Check(expanded + guns * 12 <= IA_DynamicSitePlacer.MAX_EXPANDED, "expanded hardware budget");
				foreach (vector post : layout.m_aGuardPosts)
				{
					vector center;
					float radius;
					Check(IA_BaseGarrisonArea.Resolve(layout, post, center, radius), "outdoor garrison post accepted");
				}
			}
		}
		// Scrappy recipes still resolve through the dispatcher unchanged.
		ref IA_DynamicSiteLayout scrappy = IA_BaseDesignLibrary.CreateLayout(0, 0);
		Check(scrappy && IA_HeadquartersSiteLayout.Cast(scrappy) == null && scrappy.m_iDesignVariant == 0, "scrappy dispatch");
	}

	protected void CheckSelection()
	{
		IA_BaseDesignLibrary.ResetHistory();
		int headquarters = 0;
		int streak = 0;
		bool previous = false;
		int previousArchetype = -1;
		for (int i = 0; i < 400; i++)
		{
			int variant = IA_BaseDesignLibrary.SelectDesign(i * 7919 + 12345, 50);
			bool hq = IA_BaseDesignLibrary.IsHeadquarters(variant);
			if (hq)
			{
				headquarters++;
				int archetype = (variant - IA_BaseDesignLibrary.HQ_BASE) / 3;
				Check(variant - IA_BaseDesignLibrary.HQ_BASE < IA_HeadquartersRecipes.VARIANTS, "headquarters variant range");
				Check(archetype != previousArchetype, "headquarters archetype rotation");
				previousArchetype = archetype;
			}
			else
			{
				Check(variant >= 0 && variant < 20, "scrappy variant range");
			}
			if (i > 0 && hq == previous)
				streak++;
			else
				streak = 1;
			Check(streak <= 2, "no three-in-a-row style streak at 50%");
			previous = hq;
			IA_BaseDesignLibrary.Committed(variant);
		}
		Check(headquarters >= 160 && headquarters <= 240, "roughly even split");
		Print(string.Format("[IA][HeadquartersDesignTest] selection hq=%1/400", headquarters), LogLevel.NORMAL);

		IA_BaseDesignLibrary.ResetHistory();
		for (int j = 0; j < 50; j++)
		{
			Check(!IA_BaseDesignLibrary.IsHeadquarters(IA_BaseDesignLibrary.SelectDesign(j * 104729, 0)), "0% keeps scrappy only");
			Check(IA_BaseDesignLibrary.IsHeadquarters(IA_BaseDesignLibrary.SelectDesign(j * 104729, 100)), "100% keeps headquarters only");
		}
		// Rejected terrain never commits, so selection is stable for one AO seed.
		Check(IA_BaseDesignLibrary.SelectDesign(424242, 50) == IA_BaseDesignLibrary.SelectDesign(424242, 50), "deterministic per seed");
		IA_BaseDesignLibrary.ResetHistory();
	}
}
#endif
