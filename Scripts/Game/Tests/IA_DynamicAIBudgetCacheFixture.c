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

	void SeedSettingsStateForTest()
	{
		m_bClose = true;
		m_iEvictSince = 100;
		m_iPlanAt = 200;
		m_iRetryEvictAt = 9000;
		m_iLastCasualtyMs = 77;
	}

	bool IsCloseForTest()
	{
		return m_bClose;
	}

	bool IsPlanInvalidForTest()
	{
		return m_iPlanAt == 0 && m_iEvictSince == -1;
	}

	int GetEvictionRetryAtForTest()
	{
		return m_iRetryEvictAt;
	}

	int GetLastCasualtyForTest()
	{
		return m_iLastCasualtyMs;
	}

	override int GetBudgetCost()
	{
		return m_Owner.GetDynamicAIPhysicalAliveCount();
	}

	void SetDesiredForTest(int desired)
	{
		m_iDesired = desired;
	}

	void SetNearestForTest(float distance)
	{
		m_fNearest = distance;
	}

	void CheckProtectionForTest(array<vector> players)
	{
		CheckProtection(players);
	}
}
#endif
