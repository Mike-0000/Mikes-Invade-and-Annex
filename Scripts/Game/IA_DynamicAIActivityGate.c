// Only controls entering the cache. Restoration never consults this gate.
class IA_DynamicAIActivityGate
{
	protected int m_iFarSinceMs = -1;
	protected int m_iCacheQuietMs = IA_DynamicAISpawning.CACHE_QUIET_MS;
	protected int m_iCombatQuietSec = IA_DynamicAISpawning.COMBAT_QUIET_SEC;

	void Configure(int cacheQuietSec, int combatQuietSec)
	{
		int quietMs = Math.Max(0, cacheQuietSec) * 1000;
		int combatSec = Math.Max(0, combatQuietSec);
		if (quietMs == m_iCacheQuietMs && combatSec == m_iCombatQuietSec)
			return;
		m_iCacheQuietMs = quietMs;
		m_iCombatQuietSec = combatSec;
		Reset();
	}

	void Reset()
	{
		m_iFarSinceMs = -1;
	}

	bool CanCache(bool blocked, int nowMs, int lastDangerUnix, int nowUnix)
	{
		if (blocked)
		{
			Reset();
			return false;
		}
		if (m_iFarSinceMs < 0)
			m_iFarSinceMs = nowMs;
		if (lastDangerUnix > 0 && nowUnix - lastDangerUnix < m_iCombatQuietSec)
			return false;
		return nowMs - m_iFarSinceMs >= m_iCacheQuietMs;
	}
}
