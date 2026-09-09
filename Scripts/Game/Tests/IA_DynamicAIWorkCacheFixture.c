#ifdef WORKBENCH
// Replace only world operations; scheduling, selection and budgets remain real.
class IA_DynamicAIWorkCacheFixture : IA_DynamicAIGroupCache
{
	IA_DynamicAIWorkQueueFixture m_Clock;
	ref array<string> m_aTrace;
	string m_sLabel;
	bool m_bOwnerLive = true;
	bool m_bCaptureAllowed = true;
	bool m_bRestoreBackoff;
	int m_iCandidateSoldiers;
	int m_iPendingSoldiers;
	int m_iCapturedSoldiers;
	int m_iCacheCalls;
	int m_iRestoreCalls;
	int m_iRestoredSoldiers;
	int m_iRetireCalls;
	int m_iRestoreCostMs;
	int m_iCacheCostMs;
	int m_iLastPlayerCount;

	void ConfigureWake(IA_DynamicAIWorkQueueFixture queue, int soldiers, bool urgent, int requestedMs)
	{
		m_Clock = queue;
		m_bCached = true;
		m_bWaking = true;
		m_bUrgentWake = urgent;
		m_iWakeRequestedMs = requestedMs;
		m_iPendingSoldiers = soldiers;
	}

	void ConfigureCandidate(IA_DynamicAIWorkQueueFixture queue, int soldiers)
	{
		m_Clock = queue;
		m_iCandidateSoldiers = soldiers;
	}

	override bool IsOwnerLive()
	{
		return m_bOwnerLive;
	}

	override int GetCandidateSoldierCount()
	{
		return m_iCandidateSoldiers;
	}

	override int GetUnrestoredCount()
	{
		return m_iPendingSoldiers;
	}

	override bool TryCache(array<vector> players, int now, bool allowCapture)
	{
		m_iCacheCalls++;
		m_iLastPlayerCount = players.Count();
		m_Clock.AdvanceClockMs(m_iCacheCostMs);
		if (!m_bOwnerLive || !m_bCaptureAllowed || !allowCapture || m_bCached || m_bFinished)
			return false;
		m_iCapturedSoldiers = m_iCandidateSoldiers;
		m_iPendingSoldiers = m_iCandidateSoldiers;
		m_bCached = true;
		if (m_aTrace)
			m_aTrace.Insert(m_sLabel);
		return true;
	}

	override bool RestoreNext(int now)
	{
		m_iRestoreCalls++;
		if (!IsWaking() || !m_bOwnerLive || m_bRestoreBackoff || m_iPendingSoldiers <= 0)
			return false;
		m_Clock.AdvanceClockMs(m_iRestoreCostMs);
		m_iPendingSoldiers--;
		m_iRestoredSoldiers++;
		if (m_aTrace)
			m_aTrace.Insert(m_sLabel);
		if (m_iPendingSoldiers == 0)
		{
			m_bCached = false;
			m_bWaking = false;
		}
		return true;
	}

	override void Retire()
	{
		m_iRetireCalls++;
		m_bOwnerLive = false;
		m_bCached = false;
		m_bWaking = false;
		m_bFinished = true;
		m_iPendingSoldiers = 0;
	}
}
#endif
