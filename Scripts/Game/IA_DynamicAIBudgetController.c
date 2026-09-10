// Budget admission is separate from engine creation. Once admitted, a soldier's
// retry is mandatory; an unadmitted reserve may wait as player priorities change.
class IA_DynamicAIBudgetController
{
	protected ref array<ref IA_DynamicAIBudgetEntry> m_aEntries = {};
	protected ref array<IA_DynamicAIBudgetCache> m_aCensus = {};
	protected int m_iScanCursor;
	protected int m_iNextPlanMs;
	protected int m_iQuota;
	protected int m_iNextProtectionMs;
	protected int m_iMandatoryCursor;
	protected int m_iEvictCursor;
	protected int m_iOptionalCursor;
	protected int m_iNextReportMs;
	protected int m_iWindowStartMs;
	protected int m_iRestoreAttempts;
	protected int m_iEvictions;
	protected int m_iMaxWorkMs;
	protected int m_iMaxScanMs;
	protected int m_iProtectedDemand;
	protected int m_iLastPlanMs;
	protected int m_iBudget;
	protected bool m_bActive;

	protected int ClockMs()
	{
		return System.GetTickCount();
	}

	bool IsActive()
	{
		return m_bActive;
	}

	void SettingsChanged()
	{
		m_aCensus.Clear();
		m_aEntries.Clear();
		m_iScanCursor = 0;
		m_iNextPlanMs = 0;
		m_iNextProtectionMs = 0;
	}

	void Tick(array<IA_DynamicAIGroupCache> groups, array<vector> players, int now, int budget)
	{
		if (budget != m_iBudget)
		{
			SettingsChanged();
			m_iBudget = budget;
		}
		ref array<IA_DynamicAIBudgetCache> active = {};
		foreach (IA_DynamicAIGroupCache item : groups)
		{
			ref IA_DynamicAIBudgetCache cache = IA_DynamicAIBudgetCache.Cast(item);
			if (!cache || cache.IsFinished() || !cache.IsOwnerLive())
				continue;
			if (budget > 0)
				cache.EnableBudget();
			else
				cache.DisableBudget();
			if (cache.IsBudgetActive())
				active.Insert(cache);
		}
		m_bActive = !active.IsEmpty();
		if (!m_bActive)
			return;
		if (now >= m_iNextProtectionMs)
		{
			m_iNextProtectionMs = now + 1000;
			foreach (IA_DynamicAIBudgetCache protectedCache : active)
				protectedCache.CheckWake(players, budget > 0);
		}
		if (budget > 0)
			PlanSlice(active, players, now, budget);
		Service(active, players, now, budget);
		if (IA_Log.IsDebugEnabled())
		{
			if (m_iNextReportMs == 0)
			{
				m_iNextReportMs = now + 30000;
				m_iWindowStartMs = now;
			}
			if (now >= m_iNextReportMs)
				Report(active, now, budget);
		}
	}

	protected void PlanSlice(array<IA_DynamicAIBudgetCache> active, array<vector> players, int now, int budget)
	{
		int started = ClockMs();
		if (m_aCensus.IsEmpty())
		{
			if (now < m_iNextPlanMs)
				return;
			m_aCensus.Copy(active);
			m_aEntries.Clear();
			m_iScanCursor = 0;
			m_iQuota = Math.Max(1, Math.Ceil(active.Count() / 10.0));
			m_iNextPlanMs = now + 1000;
		}
		int total = m_aCensus.Count();
		for (int visited = 0; visited < m_iQuota && m_iScanCursor < total; visited++)
		{
			ref IA_DynamicAIBudgetCache cache = m_aCensus[m_iScanCursor++];
			if (cache && cache.IsBudgetActive() && cache.IsOwnerLive() && !cache.IsFinished())
			{
				ref IA_DynamicAIBudgetEntry entry = new IA_DynamicAIBudgetEntry();
				entry.m_Cache = cache;
				cache.Describe(entry, players, now);
				m_aEntries.Insert(entry);
			}
			if (ClockMs() - started >= IA_DynamicAISpawning.SCAN_WORK_MS)
				break;
		}
		if (m_iScanCursor >= total)
		{
			IA_DynamicAIBudgetAllocator.Allocate(m_aEntries, budget);
			m_iProtectedDemand = 0;
			foreach (IA_DynamicAIBudgetEntry planned : m_aEntries)
			{
				if (!planned.m_Cache || !planned.m_Cache.IsOwnerLive() || planned.m_Cache.IsFinished())
					continue;
				m_iProtectedDemand += planned.m_iProtected;
				planned.m_Cache.SetAllocation(planned.m_iDesired, now);
			}
			m_iLastPlanMs = now;
			m_aCensus.Clear();
		}
		if (IA_Log.IsDebugEnabled())
			m_iMaxScanMs = Math.Max(m_iMaxScanMs, ClockMs() - started);
	}

	protected int TotalCost(array<IA_DynamicAIBudgetCache> active)
	{
		int total;
		foreach (IA_DynamicAIBudgetCache cache : active)
		{
			if (cache && cache.IsOwnerLive() && !cache.IsFinished())
				total += cache.GetBudgetCost();
		}
		return total;
	}

	protected void Service(array<IA_DynamicAIBudgetCache> active, array<vector> players, int now, int budget)
	{
		int started = ClockMs();
		int operations;
		int total = active.Count();
		if (total == 0)
			return;
		int visits;
		// Mandatory work (nearby combat/capture/OFF/admitted retries) can exceed
		// the target. Round-robin position persists even if only one op fits.
		while (operations < IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK && visits < total + IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			// A large list of idle/backing-off reserves must not spend the
			// entire worker allowance just looking for mandatory work.
			if (visits > 0 && operations == 0 && ClockMs() - started >= 1)
				break;
			if (m_iMandatoryCursor >= total)
				m_iMandatoryCursor = 0;
			ref IA_DynamicAIBudgetCache mandatory = active[m_iMandatoryCursor++];
			visits++;
			if (mandatory && mandatory.RestoreBudgetUnit(now, false))
			{
				operations++;
				if (IA_Log.IsDebugEnabled())
					m_iRestoreAttempts++;
			}
		}
		// Release capacity before optional creation. At most four character
		// operations TOTAL per tick, including failed attempts and downed deaths.
		visits = 0;
		int releaseStarted = ClockMs();
		int beforeRelease = operations;
		while (budget > 0 && operations < 4 && visits < total + 4)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			if (visits > 0 && operations == beforeRelease && ClockMs() - releaseStarted >= 1)
				break;
			if (m_iEvictCursor >= total)
				m_iEvictCursor = 0;
			ref IA_DynamicAIBudgetCache evict = active[m_iEvictCursor++];
			visits++;
			if (evict && evict.EvictBudgetUnit(players, now))
			{
				operations++;
				if (IA_Log.IsDebugEnabled())
					m_iEvictions++;
			}
		}
		visits = 0;
		int cost;
		if (budget > 0 && operations < 4 && ClockMs() - started < IA_DynamicAISpawning.WORK_BUDGET_MS)
			cost = TotalCost(active);
		while (budget > 0 && operations < 4 && visits < total + 4)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS || cost >= budget)
				break;
			if (m_iOptionalCursor >= total)
				m_iOptionalCursor = 0;
			ref IA_DynamicAIBudgetCache optional = active[m_iOptionalCursor++];
			visits++;
			if (optional && optional.RestoreBudgetUnit(now, true))
			{
				operations++;
				cost = TotalCost(active);
				if (IA_Log.IsDebugEnabled())
					m_iRestoreAttempts++;
			}
		}
		if (IA_Log.IsDebugEnabled())
			m_iMaxWorkMs = Math.Max(m_iMaxWorkMs, ClockMs() - started);
	}

	protected void Report(array<IA_DynamicAIBudgetCache> active, int now, int budget)
	{
		if (IA_Log.IsDebugEnabled())
		{
			int pending;
			int partial;
			int demand;
			foreach (IA_DynamicAIBudgetCache cache : active)
			{
				if (!cache || !cache.IsOwnerLive() || cache.IsFinished())
					continue;
				pending += cache.GetUnrestoredCount();
				demand += cache.GetDesired();
				if (cache.IsCached() && !cache.IsPaused())
					partial++;
			}
			int cost = TotalCost(active);
			float seconds = Math.Max(1, now - m_iWindowStartMs) / 1000.0;
			Print(string.Format("[IA][DynamicAI] Budget: target=%1 physicalAndReserved=%2 protectedDemand=%3 planned=%4 virtual=%5 partialGroups=%6 overTarget=%7 planAgeMs=%8.", budget, cost, m_iProtectedDemand, demand, pending, partial, Math.Max(0, cost - budget), now - m_iLastPlanMs), LogLevel.NORMAL);
			Print(string.Format("[IA][DynamicAI] Budget work %1s: evictions=%2 restoreAttempts=%3 maxWorkerMs=%4 maxPlanSliceMs=%5.", seconds, m_iEvictions, m_iRestoreAttempts, m_iMaxWorkMs, m_iMaxScanMs), LogLevel.NORMAL);
			m_iWindowStartMs = now;
			m_iNextReportMs = now + 30000;
			m_iEvictions = 0;
			m_iRestoreAttempts = 0;
			m_iMaxWorkMs = 0;
			m_iMaxScanMs = 0;
		}
	}
}
