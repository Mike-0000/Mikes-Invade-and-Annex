// Weak pending references: the area-owned wrapper remains the cache's owner.
// Whole-group cache commits are atomic; elapsed limits apply between operations.
class IA_DynamicAIWorkQueue
{
	protected ref array<IA_DynamicAIGroupCache> m_aCacheReady = {};
	protected ref array<int> m_aCacheQueuedMs = {};
	protected ref array<IA_DynamicAIGroupCache> m_aWaking = {};
	protected int m_iLastCacheServiceMs;
	protected int m_iReportStartMs;
	protected int m_iCachedSoldiers;
	protected int m_iRestoredSlots;
	protected int m_iRestoreAttempts;
	protected int m_iCacheMaxMs;
	protected int m_iSnapshotMaxMs;
	protected int m_iRemovalMaxMs;
	protected int m_iRestoreMaxMs;
	protected int m_iWorkerMaxMs;
	protected int m_iBudgetStops;

	void Clear()
	{
		ClearCacheWork();
		m_aWaking.Clear();
		m_iLastCacheServiceMs = 0;
	}

	void BeginReportWindow(int now)
	{
		m_iReportStartMs = now;
		m_iCachedSoldiers = 0;
		m_iRestoredSlots = 0;
		m_iRestoreAttempts = 0;
		m_iCacheMaxMs = 0;
		m_iSnapshotMaxMs = 0;
		m_iRemovalMaxMs = 0;
		m_iRestoreMaxMs = 0;
		m_iWorkerMaxMs = 0;
		m_iBudgetStops = 0;
	}

	void ClearCacheWork()
	{
		m_aCacheReady.Clear();
		m_aCacheQueuedMs.Clear();
	}

	void EnqueueCache(IA_DynamicAIGroupCache cache, int now)
	{
		if (!cache || cache.IsCached() || cache.IsFinished() || m_aCacheReady.Contains(cache))
			return;
		m_aCacheReady.Insert(cache);
		m_aCacheQueuedMs.Insert(now);
	}

	void CancelCache(IA_DynamicAIGroupCache cache)
	{
		int index = m_aCacheReady.Find(cache);
		if (index < 0)
			return;
		m_aCacheReady.RemoveOrdered(index);
		m_aCacheQueuedMs.RemoveOrdered(index);
	}

	void EnqueueWake(IA_DynamicAIGroupCache cache)
	{
		if (cache && cache.IsWaking() && !cache.IsFinished() && !m_aWaking.Contains(cache))
			m_aWaking.Insert(cache);
	}

	void Forget(IA_DynamicAIGroupCache cache)
	{
		CancelCache(cache);
		m_aWaking.RemoveItemOrdered(cache);
	}

	int GetReadyCount()
	{
		return m_aCacheReady.Count();
	}

	int GetWakeCount()
	{
		return m_aWaking.Count();
	}

	int GetOldestCacheAge(int now)
	{
		if (m_aCacheQueuedMs.IsEmpty())
			return 0;
		return Math.Max(0, now - m_aCacheQueuedMs[0]);
	}

	protected int ClockMs()
	{
		return System.GetTickCount();
	}

	// Aging is based on the original request, not this tick's progress. One slow
	// spawn per tick therefore cannot starve an old background restoration.
	protected int NextWakeIndex(int now)
	{
		foreach (int index, IA_DynamicAIGroupCache cache : m_aWaking)
		{
			if (!cache || cache.IsFinished() || !cache.IsOwnerLive())
				return index;
			if (cache.HasUrgentWake() || now - cache.GetWakeRequestedMs() >= IA_DynamicAISpawning.WAKE_AGING_MS)
				return index;
		}
		return 0;
	}

	void Service(array<vector> players, int now, bool enabled)
	{
		if (!enabled)
			ClearCacheWork();
		int started = ClockMs();
		bool cacheServiced;
		bool exclusive;
		// Under sustained waking load, reserve at most one tick per second for an
		// old cache candidate. This also lets an oversized squad make progress.
		if (enabled && GetReadyCount() > 0 && GetOldestCacheAge(now) >= IA_DynamicAISpawning.CACHE_AGING_MS && now - m_iLastCacheServiceMs >= 1000)
			cacheServiced = ServiceCache(players, now, true, exclusive);

		int attempts;
		ref array<IA_DynamicAIGroupCache> deferred = {};
		int visitLimit = GetWakeCount() + IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK;
		for (int visited = 0; !exclusive && visited < visitLimit && attempts < IA_DynamicAISpawning.RESTORE_ATTEMPTS_PER_TICK; visited++)
		{
			if (m_aWaking.IsEmpty() || ClockMs() - started >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				break;
			int index = NextWakeIndex(now);
			// Hold across callbacks, remove membership before invoking world work.
			ref IA_DynamicAIGroupCache pending = m_aWaking[index];
			m_aWaking.RemoveOrdered(index);
			if (!pending || pending.IsFinished())
				continue;
			if (!pending.IsOwnerLive())
			{
				pending.Retire();
				continue;
			}
			int before;
			if (IA_Log.IsDebugEnabled())
				before = pending.GetUnrestoredCount();
			int operationStart = ClockMs();
			bool attempted = pending.RestoreNext(now);
			if (attempted)
				attempts++;
			if (IA_Log.IsDebugEnabled())
			{
				m_iRestoreMaxMs = Math.Max(m_iRestoreMaxMs, ClockMs() - operationStart);
				m_iRestoredSlots += Math.Max(0, before - pending.GetUnrestoredCount());
			}
			if (pending.IsOwnerLive())
			{
				if (attempted)
					EnqueueWake(pending);
				else if (pending.IsWaking())
					deferred.Insert(pending);
			}
			else
				pending.Retire();
		}
		// Retry-backoff requests are temporarily absent from selection so one
		// urgent but unavailable unit cannot hide lower-priority ready work.
		foreach (IA_DynamicAIGroupCache waiting : deferred)
			EnqueueWake(waiting);

		if (enabled && !cacheServiced && ClockMs() - started < IA_DynamicAISpawning.WORK_BUDGET_MS)
			ServiceCache(players, now, attempts == 0, exclusive);
		if (IA_Log.IsDebugEnabled())
		{
			int elapsed = ClockMs() - started;
			m_iWorkerMaxMs = Math.Max(m_iWorkerMaxMs, elapsed);
			m_iRestoreAttempts += attempts;
			if (elapsed >= IA_DynamicAISpawning.WORK_BUDGET_MS)
				m_iBudgetStops++;
		}
	}

	protected bool ServiceCache(array<vector> players, int now, bool allowExclusive, out bool exclusive)
	{
		exclusive = false;
		if (m_aCacheReady.IsEmpty())
			return false;
		ref IA_DynamicAIGroupCache candidate = m_aCacheReady[0];
		if (candidate && candidate.IsOwnerLive() && !candidate.IsCached() && !candidate.IsFinished())
		{
			exclusive = candidate.GetCandidateSoldierCount() > IA_DynamicAISpawning.CACHE_SOLDIERS_PER_TICK;
			if (exclusive && !allowExclusive)
				return false;
		}
		m_aCacheReady.RemoveOrdered(0);
		m_aCacheQueuedMs.RemoveOrdered(0);
		m_iLastCacheServiceMs = now;
		if (!candidate || candidate.IsFinished())
			return true;
		if (!candidate.IsOwnerLive())
		{
			candidate.Retire();
			return true;
		}
		int operationStart = ClockMs();
		bool cached = candidate.TryCache(players, now, true);
		if (IA_Log.IsDebugEnabled())
		{
			m_iCacheMaxMs = Math.Max(m_iCacheMaxMs, ClockMs() - operationStart);
			if (cached)
			{
				m_iCachedSoldiers += candidate.GetUnrestoredCount();
				m_iSnapshotMaxMs = Math.Max(m_iSnapshotMaxMs, candidate.GetLastSnapshotMs());
				m_iRemovalMaxMs = Math.Max(m_iRemovalMaxMs, candidate.GetLastRemovalMs());
			}
		}
		return true;
	}

	void Report(int now, int scanMaxMs)
	{
		if (IA_Log.IsDebugEnabled())
		{
			int oldestWake;
			int pendingSoldiers;
			foreach (IA_DynamicAIGroupCache cache : m_aWaking)
			{
				if (!cache || !cache.IsWaking() || !cache.IsOwnerLive())
					continue;
				oldestWake = Math.Max(oldestWake, now - cache.GetWakeRequestedMs());
				pendingSoldiers += cache.GetUnrestoredCount();
			}
			float seconds = Math.Max(1, now - m_iReportStartMs) / 1000.0;
			Print(string.Format("[IA][DynamicAI] Work queues: cacheGroups=%1 oldestMs=%2; wakeGroups=%3 pendingSoldiers=%4 oldestMs=%5.", GetReadyCount(), GetOldestCacheAge(now), GetWakeCount(), pendingSoldiers, oldestWake), LogLevel.NORMAL);
			Print(string.Format("[IA][DynamicAI] Work window %1s: removed=%2 (%3/s); resolvedRestoreSlots=%4 (%5/s), attempts=%6.", seconds, m_iCachedSoldiers, m_iCachedSoldiers / seconds, m_iRestoredSlots, m_iRestoredSlots / seconds, m_iRestoreAttempts), LogLevel.NORMAL);
			Print(string.Format("[IA][DynamicAI] Work maxMs: scanSlice=%1 cache=%2 snapshot=%3 removal=%4 restore=%5 worker=%6; budgetStops=%7. Limits are checked between operations.", scanMaxMs, m_iCacheMaxMs, m_iSnapshotMaxMs, m_iRemovalMaxMs, m_iRestoreMaxMs, m_iWorkerMaxMs, m_iBudgetStops), LogLevel.NORMAL);
			BeginReportWindow(now);
		}
	}
}
