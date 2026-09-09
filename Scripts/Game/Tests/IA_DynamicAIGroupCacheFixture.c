#ifdef WORKBENCH
// WORKBENCH-only fixture; exercise cache accessors without a preview-world pawn.
class IA_DynamicAIGroupCacheFixture : IA_DynamicAIGroupCache
{
	void SetCachedForTest(bool cached)
	{
		m_bCached = cached;
	}

	void AddPendingForTest(IA_DynamicAIUnit unit)
	{
		m_aUnits.Insert(unit);
		m_bCached = true;
		m_bWaking = true;
		m_bUrgentWake = true;
	}
}
#endif
