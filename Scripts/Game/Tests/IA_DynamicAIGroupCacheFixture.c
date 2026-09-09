#ifdef WORKBENCH
// WORKBENCH-only fixture; exercise cache accessors without a preview-world pawn.
class IA_DynamicAIGroupCacheFixture : IA_DynamicAIGroupCache
{
	void SetCachedForTest(bool cached)
	{
		m_bCached = cached;
	}
}
#endif
