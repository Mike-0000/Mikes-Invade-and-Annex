// Only controls entering the cache. Restoration never consults this gate.
class IA_DynamicAIActivityGate
{
	protected int m_iFarSinceMs = -1;

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
		if (lastDangerUnix > 0 && nowUnix - lastDangerUnix < IA_DynamicAISpawning.COMBAT_QUIET_SEC)
			return false;
		return nowMs - m_iFarSinceMs >= IA_DynamicAISpawning.CACHE_QUIET_MS;
	}
}
