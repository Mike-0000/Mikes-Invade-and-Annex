#ifdef WORKBENCH
class IA_SelectionTestPlacer : IA_DynamicSitePlacer
{
	int m_iFailures;
	bool m_bRejectFull;
	bool m_bRejectPlayers;
	int m_iFitMode;
	int m_iPadChecks;

	void Check(bool passed, string label)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][SelectionTest] FAIL " + label, LogLevel.ERROR);
	}

	void Setup(bool rejectFull, bool forcedFull = false)
	{
		m_bRejectFull = rejectFull;
		m_bRejectPlayers = false;
		m_iFitMode = 0;
		ResetRefinement();
		m_iSeed = 12345;
		m_Rng.SetSeed(m_iSeed);
		m_aShortlist.Clear();
		m_aLayouts = new array<ref IA_DynamicSiteLayout>();
		m_aLayouts.Insert(IA_DynamicSiteLayout.CreateFull());
		if (!forcedFull)
			m_aLayouts.Insert(IA_DynamicSiteLayout.CreateRallyPost());
		// First location fits only Rally, second fits Full. Zero effective radius.
		m_aSearchCenters = {Vector(100, 0, 100), Vector(300, 0, 100)};
		m_aSearchRadii = {-AO_MARGIN_M, -AO_MARGIN_M};
		m_Rejections = new map<string, int>();
		m_iSampleCount = 0;
		m_iSurveyLayout = 0;
		m_iSurveyHeading = 0;
		m_iHeadingIndex = 0;
		m_bSurveyAnchorActive = false;
		m_bSurveyDone = false;
	}

	void SurveyUntilCandidate()
	{
		int slices = 0;
		while (m_aShortlist.IsEmpty() && !m_bSurveyDone && slices < 20000)
		{
			StepSurvey();
			slices++;
		}
		Check(slices < 20000, "survey terminates");
	}

	int RunTests(BaseWorld world)
	{
		SetAuditWorld(world);
		Setup(false);
		SurveyUntilCandidate();
		Check(!m_aShortlist.IsEmpty(), "larger field found");
		if (!m_aShortlist.IsEmpty())
		{
			IA_DynamicSiteCandidate cand = m_aShortlist[0];
			Check(cand.m_iLayoutId == IA_DynamicSiteLayout.LAYOUT_FULL, "small first location cannot beat larger second location");
			Check(ValidateCandidate(cand), "full candidate validates");
			Check(ValidateCandidate(cand) && cand.m_iLayoutId == IA_DynamicSiteLayout.LAYOUT_FULL, "build retry keeps full size");
			// Simulate construction failure followed by a live visibility rejection.
			m_bRejectPlayers = true;
			for (int i = 0; i < FINE_HEADINGS; i++)
				Check(!ValidateCandidate(cand), "failed full never turns into rally at same anchor");
			Check(!m_bCandidateSearchPending, "remaining headings exhaust");
			Check(cand.m_iLayoutId == IA_DynamicSiteLayout.LAYOUT_FULL, "candidate size stays fixed");
		}
		int slices = 0;
		while (HasSurveyWork() && slices < 20000)
		{
			StepSurvey();
			slices++;
		}
		StepSurvey();
		Check(m_iSurveyLayout == 0 && !m_aShortlist.IsEmpty(), "untried full candidates prevent downgrade at sample limit");
		m_aShortlist.Clear();
		SurveyUntilCandidate();
		Check(m_iSurveyLayout == 1 && !m_aShortlist.IsEmpty(), "exhausted full candidates allow next size");
		Setup(true);
		SurveyUntilCandidate();
		Check(!m_aShortlist.IsEmpty() && m_iSurveyLayout == 1, "smaller fallback after full search exhausted");
		Setup(true, true);
		SurveyUntilCandidate();
		Check(m_bSurveyDone && m_aShortlist.IsEmpty(), "forced full cannot downgrade");
		for (int mode = 1; mode <= 2; mode++)
		{
			Setup(false);
			m_iFitMode = mode;
			SurveyUntilCandidate();
			Check(!m_aShortlist.IsEmpty() && m_iSurveyLayout == 0, "refinement finds full before rally");
			Check(m_iSampleCount == MAX_SAMPLE_CENTERS, "broad coverage finishes before local refinement");
			if (!m_aShortlist.IsEmpty())
			{
				IA_DynamicSiteCandidate refined = m_aShortlist[0];
				Check(refined.m_fYawDeg == 15, "15 degree fit survives survey");
				Check(ValidateCandidate(refined), "exact refined pose validates first");
				if (mode == 2)
					Check(Math.AbsFloat(refined.m_vHqAnchor[0] - 307.5) < 0.1, "nearby shift finds field fit");
			}
		}
		CheckSampling();
		CheckOptimizations();
		CheckDefendPerimeters();
		return m_iFailures;
	}

	void CheckDefendPerimeters()
	{
		for (int id = 0; id <= IA_DynamicSiteLayout.LAYOUT_RALLY_POST; id++)
		{
			IA_DynamicSiteLayout layout = IA_DynamicSiteLayout.CreateById(id);
			array<vector> posts = {};
			foreach (vector guard : layout.m_aGuardPosts)
				posts.Insert(guard);
			foreach (vector station : layout.m_aPerimeterStations)
			{
				vector inward = -station;
				inward[1] = 0;
				inward.Normalize();
				posts.Insert(station + inward * 6);
			}
			float smallest = 15;
			foreach (vector post : posts)
			{
				float radius = layout.GetDefendPostRadius(post);
				Check(radius > 0 && radius <= 15, "all authored and fallback posts have valid defend radius");
				smallest = Math.Min(smallest, radius);
				// Independently check the complete circle against the authored
				// footprint: walls are inset 0.9 m, half-thickness 0.8 m, margin 1 m.
				Check(Math.AbsFloat(post[0]) + radius <= layout.m_fHalfWidthM - 2.7 + 0.001, "defend circle stays inside east/west wall faces");
				Check(Math.AbsFloat(post[2]) + radius <= layout.m_fHalfDepthM - 2.7 + 0.001, "defend circle stays inside front/rear wall faces");
				for (int heading = 0; heading < FINE_HEADINGS; heading++)
				{
					vector origin = Vector(1000, 100, 1000);
					vector rootMat[4];
					layout.BuildRootTransform(origin, heading * 15, rootMat);
					IA_DynamicSiteInstance site = IA_DynamicSiteInstance.Create(0, 0, origin, heading * 15, layout, null);
					vector local = site.WorldToLocalFlat(layout.LocalOffsetToWorld(rootMat, post));
					Check(Math.AbsFloat(layout.GetDefendPostRadius(local) - radius) < 0.001, "rotated base preserves clipped defend radius");
				}
			}
			Check(layout.GetDefendPostRadius(Vector(layout.m_fHalfWidthM, 0, 0)) == 0, "outside post fails closed");
			Print(string.Format("[IA][DefendPostTest] layout=%1 posts=%2 smallest_radius=%3", layout.m_sName, posts.Count(), smallest), LogLevel.NORMAL);
		}
	}

	void CheckOptimizations()
	{
		Setup(false);
		IA_DynamicSiteLayout layout = m_aLayouts[0];
		Check(ResolveLayout(layout.m_iLayoutId) == layout, "candidate reuses authored layout");
		m_aSearchCenters = {vector.Zero};
		m_aSearchRadii = {200 - AO_MARGIN_M};
		Check(super.FootprintInsideUnion(vector.Zero, 15, layout), "single-circle containment accepts rotated footprint");
		m_aSearchRadii = {110 - AO_MARGIN_M};
		Check(!super.FootprintInsideUnion(vector.Zero, 0, layout), "boundary rejects protruding corners");
		m_aSearchCenters = {Vector(-90, 0, 0), Vector(90, 0, 0)};
		m_aSearchRadii = {100 - AO_MARGIN_M, 100 - AO_MARGIN_M};
		Check(!super.FootprintInsideUnion(vector.Zero, 0, layout), "corners in different circles do not hide an interior gap");
		m_aSearchRadii = {120 - AO_MARGIN_M, 120 - AO_MARGIN_M};
		Check(super.FootprintInsideUnion(vector.Zero, 0, layout), "overlapping circles retain full-grid acceptance");
		for (int heading = 0; heading < FINE_HEADINGS; heading++)
		{
			float yaw = heading * 15;
			IA_DynamicSiteInstance distanceReference = IA_DynamicSiteInstance.Create(0, 0, vector.Zero, yaw, layout, null);
			for (int i = -4; i <= 4; i++)
			{
				vector point = Vector(i * 100, 80, 190);
				Check(Math.AbsFloat(DistanceToOrientedRect(point, vector.Zero, yaw, layout) - distanceReference.DistanceToFootprint(point)) < 0.001, "allocation-free clearance matches original distance");
			}
		}
		m_iPadChecks = 0;
		Check(super.ValidateTerrain(vector.Zero, 0, layout), "ordinary validation accepts flat synthetic terrain");
		int checkedModules = 0;
		foreach (IA_DynamicSiteModule mod : layout.m_aModules)
		{
			if (mod.m_iRole != IA_DynamicSiteModuleRole.Dressing)
				checkedModules++;
			else
				Check(!mod.m_bRequired && mod.m_iPerimeterSide == -1, "dressing cannot reject a site or change wall coverage");
		}
		Check(m_iPadChecks == checkedModules, "live site validation checks all structural modules without tracing optional dressing");
		int requiredPassed = m_iTerrainModulesPassed;
		m_iPadChecks = 0;
		Check(super.ValidateTerrain(vector.Zero, 0, layout, true), "survey reuses immediate HQ screening");
		Check(m_iPadChecks == checkedModules - 1, "only duplicate HQ check is removed");
		Check(requiredPassed == m_iTerrainModulesPassed, "HQ reuse preserves refinement ranking");
	}

	void CheckSampling()
	{
		Setup(false);
		m_aSearchRadii = {100, 100};
		array<int> quadrants = {0, 0, 0, 0};
		for (int i = 0; i < 256; i++)
		{
			vector p = CoarseAnchor(i);
			vector delta = p - m_aSearchCenters[i % 2];
			Check(delta.Length() <= 100 + AO_MARGIN_M + 0.01, "coarse samples remain in expanded circle");
			Check(vector.Distance(p, CoarseAnchor(i)) < 0.001, "sample replay is deterministic");
			int quadrant = 0;
			if (delta[0] >= 0)
				quadrant++;
			if (delta[2] >= 0)
				quadrant += 2;
			quadrants[quadrant] = quadrants[quadrant] + 1;
		}
		foreach (int population : quadrants)
			Check(population > 45 && population < 85, "broad coverage reaches all quadrants");
		for (int j = 0; j < 100; j++)
		{
			m_iTerrainModulesPassed = j;
			RememberNearFit(Vector(j * 30, 0, 0));
		}
		Check(m_aRefinementCenters.Count() == MAX_REFINEMENT_CENTERS, "refinement memory is bounded");
		foreach (int score : m_aRefinementScores)
			Check(score >= 68, "stronger near fits replace weaker ones");
		ResetRefinement();
		Check(m_aRefinementCenters.IsEmpty() && m_iRefinementSample == 0, "new layout resets refinement");
	}

	override protected bool IsOcean(vector point) { return false; }
	override protected vector OriginFromHq(vector anchor, float yaw, notnull IA_DynamicSiteLayout layout) { return anchor; }
	override protected bool ValidateModulePad(vector rootMat[4], notnull IA_DynamicSiteModule mod) { m_iPadChecks++; return true; }
	override protected void CollectPrimaryCircles(notnull array<vector> centers, notnull array<float> radii)
	{
		for (int i = 0; i < m_aSearchCenters.Count(); i++)
		{
			centers.Insert(m_aSearchCenters[i]);
			radii.Insert(m_aSearchRadii[i]);
		}
	}
	override protected bool FootprintInsideUnion(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout) { return true; }
	override protected bool ValidateTerrain(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, bool hqAlreadyValidated = false)
	{
		m_iTerrainModulesPassed = 1;
		if (m_iFitMode > 0 && layout.m_iLayoutId == IA_DynamicSiteLayout.LAYOUT_FULL)
		{
			float targetX = 300;
			if (m_iFitMode == 2)
				targetX += REFINEMENT_STEP_M;
			return Math.AbsFloat(origin[0] - targetX) < 0.1 && Math.AbsFloat(origin[2] - 100) < 0.1 && yawDeg == 15;
		}
		return layout.m_iLayoutId != IA_DynamicSiteLayout.LAYOUT_FULL || (!m_bRejectFull && origin[0] > 200);
	}
	override protected bool ValidatePlayerClearance(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, IA_DynamicSiteInstance ignoreSite) { return !m_bRejectPlayers; }
	override protected bool ValidateInteriorRoutes(vector origin, float yawDeg, notnull IA_DynamicSiteLayout layout, bool includeProps) { return true; }
}
#endif
