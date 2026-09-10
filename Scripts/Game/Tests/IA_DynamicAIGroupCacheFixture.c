#ifdef WORKBENCH
// WORKBENCH-only fixture; exercise cache accessors without a preview-world pawn.
class IA_DynamicAIGroupCacheFixture : IA_DynamicAIGroupCache
{
	protected bool m_bHybridForTest;

	override bool IsPaused()
	{
		return m_bCached && !m_bHybridForTest;
	}

	void SetCachedForTest(bool cached)
	{
		m_bCached = cached;
	}

	void SetHybridForTest(bool hybrid)
	{
		m_bHybridForTest = hybrid;
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
