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

	override protected int ClockMs()
	{
		return m_iClockMs;
	}
}
#endif
