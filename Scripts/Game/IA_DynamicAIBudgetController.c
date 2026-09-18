// Budget admission is separate from engine creation. Once admitted, a soldier's
// retry is mandatory; an unadmitted reserve may wait as player priorities change.
class IA_DynamicAIBudgetController
{
	protected ref array<ref IA_DynamicAIBudgetEntry> m_aEntries = {};
	protected ref array<IA_DynamicAIBudgetCache> m_aCensus = {};
	// Refusals survive a time-sliced sweep. Restarting at the farthest group
	// every tick can permanently hide all groups behind one expensive refusal.
	protected ref array<IA_DynamicAIBudgetCache> m_aEvictTried = {};
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
	protected int m_iLastOperations;
	protected int m_iCivilianCursor;
	protected int m_iOccupancyCursor;
	protected int m_iOptionalRestores;
	protected int m_iCaptureSeeds;
	protected int m_iTargetFullDenials;
	protected float m_fNearestRestoreM = -1;
	protected float m_fFarthestEvictM = -1;

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
		m_aEvictTried.Clear();
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
		ref array<IA_DynamicAIGroupCache> civilians = {};
		foreach (IA_DynamicAIGroupCache item : groups)
		{
			if (!item || item.IsFinished() || !item.IsOwnerLive())
				continue;
			if (item.IsCivilianCache())
			{
				civilians.Insert(item);
				continue;
			}
			if (item.IsVehicleCache())
			{
				ref IA_DynamicAIBudgetCache vehicleCache = IA_DynamicAIBudgetCache.Cast(item);
				if (vehicleCache && vehicleCache.IsBudgetActive())
					vehicleCache.DisableBudget();
				if (vehicleCache && vehicleCache.IsBudgetActive())
					active.Insert(vehicleCache);
				continue;
			}
			ref IA_DynamicAIBudgetCache cache = IA_DynamicAIBudgetCache.Cast(item);
			if (!cache)
				continue;
			if (budget > 0)
				cache.EnableBudget();
			else
				cache.DisableBudget();
			if (cache.IsBudgetActive())
				active.Insert(cache);
		}
		// Only an unfinished military ledger keeps the budget-0/OFF drain alive.
		// Civilians otherwise strand the dispatcher here and stop distance-only
		// military caching after the ledger has finished draining.
		m_bActive = !active.IsEmpty();
		if (!m_bActive && budget <= 0)
			return;
		m_iLastOperations = 0;
		if (now >= m_iNextProtectionMs)
		{
			m_iNextProtectionMs = now + 1000;
			foreach (IA_DynamicAIBudgetCache protectedCache : active)
				protectedCache.CheckWake(players, budget > 0);
			foreach (IA_DynamicAIGroupCache civilianCache : civilians)
			{
				if (civilianCache && budget > 0)
					civilianCache.CheckWake(players, true);
			}
		}
		if (budget > 0)
			PlanSlice(active, players, now, budget);
		Service(active, players, now, budget);
		int started = ClockMs();
		if (budget > 0)
			ServiceCivilians(civilians, players, now, started);
		ServiceOccupancy(groups, players, now, started);
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

	protected bool CacheTried(array<IA_DynamicAIBudgetCache> tried, IA_DynamicAIBudgetCache cache)
	{
		if (!cache)
			return true;
		foreach (IA_DynamicAIBudgetCache existing : tried)
		{
			if (existing == cache)
				return true;
		}
		return false;
	}

	protected IA_DynamicAIBudgetCache PickNearestOptional(array<IA_DynamicAIBudgetCache> active, int now)
	{
		ref IA_DynamicAIBudgetCache best;
		float bestDist = 1000000000;
		int count = active.Count();
		for (int index = 0; index < count; index++)
		{
			IA_DynamicAIBudgetCache cache = active[index];
			if (!cache || !cache.WantsOptionalRestore(now))
				continue;
			float distance = cache.GetNearestPlayerDistance();
			if (cache.HasRecentCombat())
				distance = Math.Max(0, distance - IA_DynamicAISpawning.GetTuning().m_iDynamicAIRetentionBiasM);
			if (best)
			{
				if (distance > bestDist)
					continue;
				if (distance == bestDist && cache.GetBudgetOrder() >= best.GetBudgetOrder())
					continue;
			}
			best = cache;
			bestDist = distance;
		}
		return best;
	}

	protected bool HasNearbyCombatRestore(array<IA_DynamicAIBudgetCache> active, int now)
	{
		foreach (IA_DynamicAIBudgetCache cache : active)
		{
			if (cache && cache.WantsOptionalRestore(now) && cache.HasRecentCombat())
				return true;
		}
		return false;
	}

	protected IA_DynamicAIBudgetCache PickFarthestUntried(array<IA_DynamicAIBudgetCache> active, array<IA_DynamicAIBudgetCache> tried)
	{
		ref IA_DynamicAIBudgetCache best;
		float bestDist = -1;
		int count = active.Count();
		for (int index = 0; index < count; index++)
		{
			IA_DynamicAIBudgetCache cache = active[index];
			if (!cache || CacheTried(tried, cache))
				continue;
			float distance = cache.GetNearestPlayerDistance();
			if (best)
			{
				if (distance < bestDist)
					continue;
				if (distance == bestDist && cache.GetBudgetOrder() <= best.GetBudgetOrder())
					continue;
			}
			best = cache;
			bestDist = distance;
		}
		return best;
	}

	protected void Service(array<IA_DynamicAIBudgetCache> active, array<vector> players, int now, int budget)
	{
		int started = ClockMs();
		int operations;
		int total = active.Count();
		if (total == 0)
			return;
		int visits;
		int cost = TotalCost(active);
		// OFF/budget-0 drains and already-admitted retries remain mandatory.
		// Approach and combat no longer force-full over the shared target.
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
			if (!mandatory)
				continue;
			if (budget > 0 && cost >= budget && !mandatory.IsExitingBudget() && !mandatory.HasAdmittedRetryWork() && !mandatory.HasCaptureSeedWork())
			{
				if (mandatory.HasMandatoryWork())
					m_iTargetFullDenials++;
				continue;
			}
			bool captureSeed = mandatory.HasCaptureSeedWork();
			if (mandatory.RestoreBudgetUnit(now, false))
			{
				operations++;
				cost = TotalCost(active);
				if (captureSeed)
					m_iCaptureSeeds++;
				if (IA_Log.IsDebugEnabled())
					m_iRestoreAttempts++;
			}
		}
		// Release farthest capacity before optional creation. At most four
		// character operations TOTAL per tick, including failed attempts and
		// downed deaths.
		visits = 0;
		int releaseStarted = ClockMs();
		int beforeRelease = operations;
		while (budget > 0 && operations < 4 && visits < total + 4)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			if (visits > 0 && operations == beforeRelease && ClockMs() - releaseStarted >= 1)
				break;
			ref IA_DynamicAIBudgetCache evict = PickFarthestUntried(active, m_aEvictTried);
			if (!evict)
			{
				// Start another sweep on the next tick, after every current group
				// has had a turn. Allocation replans must not reset this progress.
				m_aEvictTried.Clear();
				break;
			}
			visits++;
			// The farthest overallocated squad keeps shedding until it refuses;
			// only a refusal moves the pick to the next farthest squad.
			if (evict.EvictBudgetUnit(players, now))
			{
				operations++;
				m_fFarthestEvictM = evict.GetNearestPlayerDistance();
				if (IA_Log.IsDebugEnabled())
					m_iEvictions++;
			}
			else
				m_aEvictTried.Insert(evict);
		}
		visits = 0;
		int optionalCap = 4;
		if (HasNearbyCombatRestore(active, now))
			optionalCap = 6;
		if (budget > 0 && operations < optionalCap && ClockMs() - started < IA_DynamicAISpawning.WORK_BUDGET_MS)
			cost = TotalCost(active);
		while (budget > 0 && operations < optionalCap && visits < total + optionalCap)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS || cost >= budget)
				break;
			ref IA_DynamicAIBudgetCache optional = PickNearestOptional(active, now);
			visits++;
			if (!optional)
				break;
			if (optional.RestoreBudgetUnit(now, true))
			{
				operations++;
				cost = TotalCost(active);
				m_iOptionalRestores++;
				m_fNearestRestoreM = optional.GetNearestPlayerDistance();
				if (IA_Log.IsDebugEnabled())
					m_iRestoreAttempts++;
			}
			else
				break;
		}
		if (IA_Log.IsDebugEnabled())
			m_iMaxWorkMs = Math.Max(m_iMaxWorkMs, ClockMs() - started);
		m_iLastOperations = operations;
	}

	protected void ServiceCivilians(array<IA_DynamicAIGroupCache> civilians, array<vector> players, int now, int started)
	{
		int total = civilians.Count();
		if (total == 0)
			return;
		int visits;
		while (m_iLastOperations < IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK && visits < total + IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK)
		{
			if (ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			if (m_iCivilianCursor >= total)
				m_iCivilianCursor = 0;
			ref IA_DynamicAIGroupCache civilian = civilians[m_iCivilianCursor];
			m_iCivilianCursor = m_iCivilianCursor + 1;
			visits++;
			if (!civilian || !civilian.IsOwnerLive() || civilian.IsFinished())
				continue;
			if (civilian.IsCached())
			{
				if (civilian.IsWaking() && civilian.RestoreNext(now))
					m_iLastOperations = m_iLastOperations + 1;
				continue;
			}
			if (civilian.TryCache(players, now, true))
				m_iLastOperations = m_iLastOperations + 1;
		}
	}

	protected void ServiceOccupancy(array<IA_DynamicAIGroupCache> groups, array<vector> players, int now, int started)
	{
		int remaining = IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK - m_iLastOperations;
		if (remaining <= 0 || ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
			return;
		int total = groups.Count();
		int visits;
		while (visits < total && remaining > 0)
		{
			if (m_iOccupancyCursor >= total)
				m_iOccupancyCursor = 0;
			ref IA_DynamicAIGroupCache cache = groups[m_iOccupancyCursor];
			m_iOccupancyCursor = m_iOccupancyCursor + 1;
			visits++;
			ref IA_DynamicAIBudgetCache budget = IA_DynamicAIBudgetCache.Cast(cache);
			if (budget && budget.WantsOccupancyEviction() && budget.GetOwner())
				IA_DynamicAIOccupancyHost.Request(budget.GetOwner(), players, now);
		}
		int used = IA_DynamicAIOccupancyHost.TickAll(players, now, remaining, started);
		if (used > 0)
			m_iLastOperations = m_iLastOperations + used;
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
			Print(string.Format("[IA][DynamicAI] Budget flow: nearestRestoreM=%1 farthestEvictM=%2 captureSeeds=%3 deniedOverBudget=%4 optionalRestores=%5 reservesSeeded=%6.", m_fNearestRestoreM, m_fFarthestEvictM, m_iCaptureSeeds, m_iTargetFullDenials, m_iOptionalRestores, IA_DynamicAISpawning.GetReservesSeeded()), LogLevel.NORMAL);
			m_iWindowStartMs = now;
			m_iNextReportMs = now + 30000;
			m_iEvictions = 0;
			m_iRestoreAttempts = 0;
			m_iMaxWorkMs = 0;
			m_iMaxScanMs = 0;
			m_iOptionalRestores = 0;
			m_iCaptureSeeds = 0;
			m_iTargetFullDenials = 0;
			m_fNearestRestoreM = -1;
			m_fFarthestEvictM = -1;
			IA_DynamicAISpawning.ResetReservesSeeded();
		}
	}
}
