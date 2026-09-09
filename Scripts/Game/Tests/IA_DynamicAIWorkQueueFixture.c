#ifdef WORKBENCH
// Run the production worker with a deterministic clock and no game-world work.
class IA_DynamicAIWorkQueueFixture : IA_DynamicAIWorkQueue
{
	protected int m_iClockMs;

	void SetClockMs(int value)
	{
		m_iClockMs = value;
	}

	void AdvanceClockMs(int elapsed)
	{
		m_iClockMs += elapsed;
	}

	override protected int ClockMs()
	{
		return m_iClockMs;
	}
}
#endif
