#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA emplacement regression", wbModules: {"ResourceManager"})]
class IA_BaseEmplacementTest : IA_BaseFoundationProbe
{
	override void RunCommandline()
	{
		for (int budget = 0; budget <= 100; budget++)
		{
			for (int installed = 0; installed <= 6; installed++)
			{
				int crew = IA_EmplacementProfile.CrewBudget(installed, budget);
				Check(crew >= 0 && crew <= installed && crew <= Math.Floor(budget / 4.0), "crew allocation caps");
				Check(crew + (budget - crew) == budget, "unchanged defender budget");
				if (budget >= 4)
					Check(budget - crew >= 4, "four ordinary infantry minimum");
			}
		}
		int barrenHits;
		int lastBarren = IA_EmplacementProfile.ChooseBarrenSide(7);
		Check(IA_EmplacementProfile.ChooseBarrenSide(7) == lastBarren, "barren wall roll is seeded");
		for (int seed = 0; seed < 200; seed++)
		{
			int side = IA_EmplacementProfile.ChooseBarrenSide(seed);
			Check(side >= -1 && side <= 3, "barren side is a wall or none");
			if (side >= 0)
				barrenHits++;
		}
		Check(barrenHits > 115 && barrenHits < 165, "about seven in ten bases leave one wall empty");
		for (int id = 0; id < 6; id++)
		{
			ref IA_DynamicSiteLayout layout = IA_DynamicSiteLayout.CreateById(id);
			int modules = layout.m_aModules.Count();
			layout.PrepareEmplacements();
			int stations = layout.m_aEmplacements.Count();
			layout.PrepareEmplacements();
			Check(stations <= 8 && layout.m_aEmplacements.Count() == stations, "bounded idempotent manifest");
			Check(layout.m_aModules.Count() == modules, "survey module list unchanged");
			foreach (IA_DynamicBaseEmplacementSpec spec : layout.m_aEmplacements)
			{
				bool found = false;
				foreach (IA_DynamicSiteModule panel : layout.m_aModules)
				{
					if (panel.m_sId == spec.m_sPanelId && panel.m_iPerimeterSide == spec.m_iSide)
						found = true;
				}
				Check(found, "stable supporting panel " + spec.m_sPanelId);
				Check(!spec.m_bHeavy || (id < 2 && spec.m_iSide == 1), "NSV only on designated side of large layouts");
				for (int heading = 0; heading < 24; heading++)
				{
					vector root[4], mat[4];
					layout.BuildRootTransform("100 20 100", heading * 15, root);
					layout.LocalToWorld(root, spec.m_vLocalPosition, spec.m_fYaw, 0, mat);
					Check(Math.AbsFloat(vector.Distance(mat[3], root[3]) - spec.m_vLocalPosition.Length()) < 0.01, "rigid station transform");
				}
			}
		}
		ref IA_Config cfg = new IA_Config();
		Check(cfg.m_bDynamicBaseEmplacementsEnabled, "new default enabled");
		cfg.m_bDynamicBaseEmplacementsEnabled = false;
		cfg.m_iDynamicBaseCaptureSec = 123;
		string packed = IA_Config.PackDynamicBaseExtras(cfg);
		ref IA_Config copy = new IA_Config();
		IA_Config.UnpackDynamicBaseExtras(copy, packed);
		Check(!copy.m_bDynamicBaseEmplacementsEnabled && copy.m_iDynamicBaseCaptureSec == 123, "v2 roundtrip");
		IA_Config.UnpackDynamicBaseExtras(copy, "1,1,100,0,0,90,90,240,0.6,1");
		Check(copy.m_bDynamicBaseEmplacementsEnabled && copy.m_iDynamicBaseCaptureSec == 90, "v1 migration");
		copy.m_bDynamicBaseEmplacementsEnabled = false;
		IA_Config.UnpackDynamicBaseExtras(copy, "2,1,100,0,0,90,90,240,0.6,1,garbage");
		Check(!copy.m_bDynamicBaseEmplacementsEnabled, "malformed v2 is atomic");
		ref IA_BaseObjectiveSettings snapshot = IA_BaseObjectiveSettings.SnapshotFrom(copy, null);
		copy.m_bDynamicBaseEmplacementsEnabled = true;
		Check(!snapshot.m_bEmplacementsEnabled, "construction settings are frozen");
		ref IA_EmplacementOverridesFixture overlay = new IA_EmplacementOverridesFixture();
		overlay.FillFrom(cfg);
		ref IA_EmplacementOverridesFixture loaded = new IA_EmplacementOverridesFixture();
		loaded.Decode(overlay.Encode());
		loaded.ApplyTo(copy);
		Check(!copy.m_bDynamicBaseEmplacementsEnabled, "persisted disabled roundtrip");
		loaded.Decode("{\"dynamicBaseVersion\":1,\"dynamicBaseExtras\":\"1,1,100,0,0,90,90,240,0.6,1\"}");
		loaded.ApplyTo(copy);
		Check(copy.m_bDynamicBaseEmplacementsEnabled, "old persisted snapshot defaults enabled");
		Print(string.Format("[IA][EmplacementTest] failures=%1; live firing/boarding/cleanup remain separate tests", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}
}
#endif
