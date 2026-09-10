#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA dynamic AI budget regression", wbModules: {"ResourceManager"})]
class IA_DynamicAIBudgetTest : WorkbenchPlugin
{
	protected int m_iFailures;
	protected int m_iChecks;

	override void RunCommandline()
	{
		TestClusteredObjectives();
		TestProtectedReservation();
		TestStableRanking();
		TestRetentionBoundary();
		TestConfiguredRetention();
		TestSplitPlayersAndCasualties();
		TestUnlimitedAndInvalidCounts();
		TestBudgetSweep();
		Print(string.Format("[IA][DynamicAIBudgetTest] checks=%1 failures=%2", m_iChecks, m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestConfiguredRetention()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		IA_DynamicAIBudgetEntry closer = AddEntry(entries, 8, 500);
		IA_DynamicAIBudgetEntry retained = AddEntry(entries, 8, 550, 0, true, 8);
		retained.m_iRetentionBiasM = 100;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(retained.m_iDesired == 8 && closer.m_iDesired == 0, "a configured retention margin keeps the already allocated squad stable");
		retained.m_iRetentionBiasM = 0;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(closer.m_iDesired == 8 && retained.m_iDesired == 0, "zero retention chooses the nearer squad without a previous-allocation preference");
		retained.m_iRetentionBiasM = -50;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(closer.m_iDesired == 8 && retained.m_iDesired == 0, "an invalid negative retention value cannot invert the ranking preference");
	}

	protected IA_DynamicAIBudgetEntry AddEntry(array<ref IA_DynamicAIBudgetEntry> entries, int alive, float distance, int protectedCount = 0, bool inRange = true, int previous = 0)
	{
		ref IA_DynamicAIBudgetEntry entry = new IA_DynamicAIBudgetEntry();
		entry.m_iAlive = alive;
		entry.m_iPhysical = alive;
		entry.m_fDistance = distance;
		entry.m_iProtected = protectedCount;
		entry.m_bInRange = inRange;
		entry.m_iPreviousDesired = previous;
		entry.m_iOrder = entries.Count();
		entries.Insert(entry);
		return entry;
	}

	protected void TestClusteredObjectives()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		ref IA_DynamicAIBudgetEntry far = AddEntry(entries, 4, 600);
		ref IA_DynamicAIBudgetEntry nearest = AddEntry(entries, 4, 300);
		ref IA_DynamicAIBudgetEntry middle = AddEntry(entries, 4, 400);
		ref IA_DynamicAIBudgetEntry boundary = AddEntry(entries, 4, 500);
		IA_DynamicAIBudgetAllocator.Allocate(entries, 10);
		Check(nearest.m_iDesired == 4 && middle.m_iDesired == 4, "nearest clustered squads receive their full current rosters");
		Check(boundary.m_iDesired == 2 && far.m_iDesired == 0, "remaining slots create one partial boundary squad");
		Check(entries[0] == far && entries[1] == nearest && entries[2] == middle && entries[3] == boundary, "allocation preserves caller registry order");
		Check(far.m_iPhysical == 4 && boundary.m_iAlive == 4, "desired allocation does not pretend physical eviction already happened");
		IA_DynamicAIBudgetAllocator.Allocate(entries, 1);
		Check(nearest.m_iDesired == 1 && middle.m_iDesired == 0 && boundary.m_iDesired == 0 && far.m_iDesired == 0, "small budgets do not reserve a mandatory soldier for every squad");
	}

	protected void TestProtectedReservation()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		ref IA_DynamicAIBudgetEntry nearest = AddEntry(entries, 8, 400);
		ref IA_DynamicAIBudgetEntry distantCrew = AddEntry(entries, 5, 2000, 5, false);
		ref IA_DynamicAIBudgetEntry engaged = AddEntry(entries, 4, 700, 4);
		IA_DynamicAIBudgetAllocator.Allocate(entries, 12);
		Check(distantCrew.m_iDesired == 5 && engaged.m_iDesired == 4 && nearest.m_iDesired == 3, "global protected demand is reserved before closer optional demand");
		IA_DynamicAIBudgetAllocator.Allocate(entries, 2);
		Check(distantCrew.m_iDesired == 5 && engaged.m_iDesired == 4 && nearest.m_iDesired == 0, "protected overage preserves gameplay and gives no optional slots");
		engaged.m_iProtected = 1;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 10);
		Check(nearest.m_iDesired == 4 && engaged.m_iDesired == 1, "partial protection reserves only its mandatory soldiers");
	}

	protected void TestStableRanking()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		ref IA_DynamicAIBudgetEntry first = AddEntry(entries, 4, 400);
		ref IA_DynamicAIBudgetEntry second = AddEntry(entries, 4, 400);
		first.m_iOrder = 20;
		second.m_iOrder = 10;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(second.m_iDesired == 4 && first.m_iDesired == 0, "equal-distance priority follows stable registration order");
		second.m_iOrder = 20;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(first.m_iDesired == 4 && second.m_iDesired == 0, "identical rank and order preserve input order");
	}

	protected void TestRetentionBoundary()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		ref IA_DynamicAIBudgetEntry newcomer = AddEntry(entries, 4, 470);
		ref IA_DynamicAIBudgetEntry incumbent = AddEntry(entries, 4, 500, 0, true, 4);
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(incumbent.m_iDesired == 4 && newcomer.m_iDesired == 0, "small distance changes retain an existing allocation");
		newcomer.m_fDistance = 449;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(newcomer.m_iDesired == 4 && incumbent.m_iDesired == 0, "a meaningfully closer squad defeats the retention bias");
		newcomer.m_fDistance = 0;
		incumbent.m_fDistance = 20;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(newcomer.m_iDesired == 4, "retention bias cannot create negative distances");
		incumbent.m_bInRange = false;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 4);
		Check(incumbent.m_iDesired == 0, "prior allocation cannot retain an out-of-range optional squad");
	}

	protected void TestSplitPlayersAndCasualties()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		// The runtime supplies minimum distance to any player, regardless of AO.
		ref IA_DynamicAIBudgetEntry playerOneFront = AddEntry(entries, 4, 310);
		ref IA_DynamicAIBudgetEntry playerTwoFront = AddEntry(entries, 4, 320);
		ref IA_DynamicAIBudgetEntry unusedCenter = AddEntry(entries, 4, 850);
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(playerOneFront.m_iDesired == 4 && playerTwoFront.m_iDesired == 4 && unusedCenter.m_iDesired == 0, "separate player fronts compete by player distance instead of objective grouping");
		playerOneFront.m_iAlive = 1;
		playerOneFront.m_iPreviousDesired = 4;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(playerOneFront.m_iDesired == 1 && unusedCenter.m_iDesired == 3, "casualties free slots without restoring the initial squad size");
		playerTwoFront.m_iAlive = 0;
		playerTwoFront.m_iProtected = 4;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 8);
		Check(playerTwoFront.m_iDesired == 0, "stale protection cannot revive an eliminated squad");
	}

	protected void TestUnlimitedAndInvalidCounts()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		entries.Insert(null);
		ref IA_DynamicAIBudgetEntry nearby = AddEntry(entries, 7, 400);
		ref IA_DynamicAIBudgetEntry distant = AddEntry(entries, 8, 1800, 2, false);
		ref IA_DynamicAIBudgetEntry invalid = AddEntry(entries, -3, 0, 8);
		IA_DynamicAIBudgetAllocator.Allocate(entries, 0);
		Check(nearby.m_iDesired == 7 && distant.m_iDesired == 2 && invalid.m_iDesired == 0, "unlimited allocation preserves range and clamps invalid survivor counts");
		nearby.m_iProtected = -4;
		distant.m_iProtected = 20;
		IA_DynamicAIBudgetAllocator.Allocate(entries, 10);
		Check(nearby.m_iDesired == 2 && distant.m_iDesired == 8, "invalid protection is clamped to zero through current alive count");
		IA_DynamicAIBudgetAllocator.Allocate(null, 10);
		ref array<ref IA_DynamicAIBudgetEntry> empty = {};
		IA_DynamicAIBudgetAllocator.Allocate(empty, 10);
		Check(empty.IsEmpty(), "empty or absent censuses require no allocation");
	}

	protected void TestBudgetSweep()
	{
		ref array<ref IA_DynamicAIBudgetEntry> entries = {};
		AddEntry(entries, 5, 600, 2);
		AddEntry(entries, 7, 400);
		AddEntry(entries, 3, 500, 0, true, 3);
		AddEntry(entries, 4, 1900, 4, false);
		AddEntry(entries, 8, 2000, 0, false);
		for (int budget = 1; budget <= 25; budget++)
		{
			IA_DynamicAIBudgetAllocator.Allocate(entries, budget);
			int total = 0;
			bool valid = true;
			foreach (IA_DynamicAIBudgetEntry entry : entries)
			{
				total += entry.m_iDesired;
				if (entry.m_iDesired < entry.m_iProtected || entry.m_iDesired > entry.m_iAlive)
					valid = false;
			}
			int expected = Math.Min(19, Math.Max(6, budget));
			Check(valid && total == expected && entries[4].m_iDesired == 0, string.Format("budget %1 conserves protected and eligible demand", budget));
		}
	}

	protected void Check(bool passed, string description)
	{
		m_iChecks++;
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][DynamicAIBudgetTest] " + description, LogLevel.ERROR);
	}
}
#endif
