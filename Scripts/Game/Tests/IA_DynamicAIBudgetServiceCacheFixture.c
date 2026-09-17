#ifdef WORKBENCH
// Real admission, pending-roster selection and retry logic; replace engine effects.
class IA_DynamicAIBudgetServiceCacheFixture : IA_DynamicAIBudgetCache
{
	IA_DynamicAIBudgetControllerFixture m_Clock;
	ref array<string> m_aTrace;
	string m_sLabel;
	bool m_bOwnerLive = true;
	int m_iPhysical;
	int m_iRestoreAttempts;
	int m_iCreated;
	int m_iEvictions;
	int m_iEvictRemaining;
	int m_iFailRemaining;
	int m_iOperationMs;

	void Configure(IA_DynamicAIBudgetControllerFixture clock, array<string> trace, string label, int pending, int physical, bool mandatory)
	{
		m_Clock = clock;
		m_aTrace = trace;
		m_sLabel = label;
		m_iPhysical = physical;
		m_bBudgetActive = true;
		m_bForceFull = mandatory;
		m_bCached = pending > 0;
		for (int index = 0; index < pending; index++)
		{
			ref IA_DynamicAIUnit unit = new IA_DynamicAIUnit();
			m_aUnits.Insert(unit);
		}
		SetAllocation(pending + physical, 1);
	}

	void SetNearestForTest(float distance)
	{
		m_fNearest = distance;
	}

	override bool IsOwnerLive()
	{
		return m_bOwnerLive;
	}

	override int GetLogicalAliveCount()
	{
		return m_iPhysical + GetUnrestoredCount();
	}

	int GetReservedCount()
	{
		int count;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_bBudgetAdmitted && !unit.m_bRestored && !unit.m_bDead)
				count++;
		}
		return count;
	}

	override int GetBudgetCost()
	{
		if (!m_bOwnerLive || m_bFinished)
			return 0;
		return m_iPhysical + GetReservedCount();
	}

	override protected int PhysicalAliveCount()
	{
		return m_iPhysical;
	}

	override protected void RestoreUnit(IA_DynamicAIUnit unit, int now)
	{
		m_iRestoreAttempts++;
		m_Clock.AdvanceClockMs(m_iOperationMs);
		m_aTrace.Insert(m_sLabel + ":restore");
		if (m_iFailRemaining > 0)
		{
			m_iFailRemaining--;
			unit.RecordFailure(now);
			return;
		}
		unit.m_bRestored = true;
		m_iPhysical++;
		m_iCreated++;
	}

	override bool EvictBudgetUnit(array<vector> players, int now)
	{
		if (!m_bOwnerLive || m_bFinished || m_iEvictRemaining <= 0 || m_iPhysical <= 0)
			return false;
		m_Clock.AdvanceClockMs(m_iOperationMs);
		m_iEvictRemaining--;
		m_iPhysical--;
		m_iEvictions++;
		m_aTrace.Insert(m_sLabel + ":evict");
		return true;
	}

	override protected void UpdateState()
	{
		// Native group pause/resume is outside this scheduler test's scope.
		m_bCached = GetUnrestoredCount() > 0;
		if (!m_bCached)
		{
			m_bForceFull = false;
			m_bWaking = false;
		}
	}
}
#endif
