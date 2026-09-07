#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA composition design regression", wbModules: {"ResourceManager"})]
class IA_BaseDesignTest : IA_BaseFoundationProbe
{
	override void RunCommandline()
	{
		for (int size = 0; size < 6; size++)
		{
			for (int variant = 0; variant < 20; variant++)
			{
				ref IA_DynamicSiteLayout layout = IA_BaseDesignRecipes.Create(size, variant);
				Check(layout && layout.m_bComposed, "recipe exists");
				if (!layout)
					continue;
				int expanded = 0;
				array<int> sides = {0, 0, 0, 0};
				foreach (IA_DynamicSiteModule module : layout.m_aModules)
				{
					expanded += module.m_iEstimatedExpandedEntities;
					Check(module.m_bFollowTerrainPlane || module.m_iGroundingPolicy == IA_DynamicSiteGrounding.TerrainSegment, "composition or wall grounding enabled");
					if (module.m_iPerimeterSide >= 0)
						sides[module.m_iPerimeterSide] = sides[module.m_iPerimeterSide] + 1;
				}
				Check(layout.HasPerimeterCoverage(sides), "four-sided coverage");
				int guns = layout.m_aEmplacements.Count();
				layout.PrepareEmplacements();
				Check(layout.m_aEmplacements.Count() == guns, "no legacy socket duplication");
				Check(guns <= IA_EmplacementProfile.GunCap(size), "shared weapon cap");
				Check(guns >= 4, "at least one gun socket per wall");
				array<int> armed = {0, 0, 0, 0};
				foreach (IA_DynamicBaseEmplacementSpec spec : layout.m_aEmplacements)
				{
					if (spec.m_iSide >= 0)
						armed[spec.m_iSide] = armed[spec.m_iSide] + 1;
				}
				Check(armed[0] >= 1 && armed[1] >= 1 && armed[2] >= 1 && armed[3] >= 1, "each sandbag face has a gun");
				Check(expanded + guns * 12 <= IA_DynamicSitePlacer.MAX_EXPANDED, "expanded hardware budget");
				foreach (vector post : layout.m_aGuardPosts)
				{
					vector center;
					float radius;
					Check(IA_BaseGarrisonArea.Resolve(layout, post, center, radius), "outdoor garrison post accepted");
				}
				foreach (IA_DynamicBaseEmplacementSpec spec : layout.m_aEmplacements)
				{
					Check(spec.m_Socket != null, "retained socket ownership");
					for (int heading = 0; heading < 24; heading++)
					{
						vector parent[4], result[4], identity[4], local[4];
						layout.BuildRootTransform("100 20 100", heading * 15, parent);
						Math3D.MatrixIdentity4(identity);
						spec.m_Socket.Transform(identity, local);
						spec.m_Socket.Transform(parent, result);
						Check(Math.AbsFloat(vector.Distance(result[3], parent[3]) - local[3].Length()) < 0.01, "nested socket transform");
					}
				}
			}
		}
		int previousTheme = -1;
		for (int i = 0; i < 40; i++)
		{
			int selected = IA_BaseDesignLibrary.Select(7);
			int theme = selected / 4;
			Check(theme != previousTheme, "committed theme rotation");
			IA_BaseDesignLibrary.Committed(selected);
			previousTheme = theme;
		}
		Print(string.Format("[IA][BaseDesignTest] 120 recipes, 24 headings; failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}
}
#endif
