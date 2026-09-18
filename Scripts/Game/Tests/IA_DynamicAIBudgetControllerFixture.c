#ifdef WORKBENCH
// Exercise the production scheduler with a deterministic clock and no game world.
class IA_DynamicAIBudgetControllerFixture : IA_DynamicAIBudgetController
{
	protected int m_iClockMs;

	void AdvanceClockMs(int elapsed)
	{
		m_iClockMs += elapsed;
	}

	void RunService(array<IA_DynamicAIBudgetCache> active, int now, int budget)
	{
		m_iClockMs = now;
		ref array<vector> players = {};
		Service(active, players, now, budget);
	}

	void RunTick(array<IA_DynamicAIGroupCache> groups, int now, int budget)
	{
		m_iClockMs = now;
		ref array<vector> players = {};
		Tick(groups, players, now, budget);
	}

	override protected int ClockMs()
	{
		return m_iClockMs;
	}

	int GetOptionalRestoresForTest()
	{
		return m_iOptionalRestores;
	}

	int GetCaptureSeedsForTest()
	{
		return m_iCaptureSeeds;
	}

	int GetDeniedOverBudgetForTest()
	{
		return m_iTargetFullDenials;
	}

	float GetNearestRestoreMForTest()
	{
		return m_fNearestRestoreM;
	}

	float GetNearestWaitingMForTest()
	{
		return m_fNearestWaitingM;
	}
}
#endif
