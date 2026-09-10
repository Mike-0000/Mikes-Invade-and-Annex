#ifdef WORKBENCH
// Expose the actual state transitions with a synthetic owner physical boundary.
// No production transition or roster-count method is replaced.
class IA_DynamicAIBudgetCacheFixture : IA_DynamicAIBudgetCache
{
	void AddForTest(IA_DynamicAIUnit unit)
	{
		m_aUnits.Insert(unit);
	}

	void ReconcileForTest()
	{
		UpdateState();
	}

	bool IsExitPendingForTest()
	{
		return m_bExitBudget;
	}

	bool IsFullRequiredForTest()
	{
		return m_bForceFull;
	}

	override int GetBudgetCost()
	{
		return m_Owner.GetDynamicAIPhysicalAliveCount();
	}
}
#endif
