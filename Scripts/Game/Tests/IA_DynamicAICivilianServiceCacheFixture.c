#ifdef WORKBENCH
// Civilian distance-cache service without a preview-world pawn or military budget.
class IA_DynamicAICivilianServiceCacheFixture : IA_DynamicAIBudgetCache
{
	int m_iTryCacheCalls;

	override bool IsCivilianCache()
	{
		return true;
	}

	override bool IsOwnerLive()
	{
		return true;
	}

	override bool TryCache(array<vector> players, int now, bool allowCapture)
	{
		m_iTryCacheCalls++;
		return false;
	}
}
#endif
