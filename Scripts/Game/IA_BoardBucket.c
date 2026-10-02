//------------------------------------------------------------------------------------------------
//! Server: an allowance of requests. It holds up to a burst, each request takes one, and it
//! fills again at a steady rate. The time is handed in, so every allowance reads one clock.
//!
//!   if (bucket.WaitMs(now, perHour, burst) == 0)
//!       bucket.Take();
//------------------------------------------------------------------------------------------------
class IA_BoardBucket
{
	// A request is allowed a hair early rather than a frame late.
	protected static const float WHOLE = 0.999;
	protected static const float HOUR_MS = 3600000;

	protected float m_fHeld;
	protected int m_iStampMs;
	protected bool m_bStarted;

	//------------------------------------------------------------------------------------------------
	//! \param perHour requests it gains in an hour
	//! \param burst the most it holds; it starts full
	//! \return milliseconds until a request is allowed, 0 when one is allowed now
	int WaitMs(int nowMs, int perHour, int burst)
	{
		Fill(nowMs, perHour, burst);
		if (m_fHeld >= WHOLE)
			return 0;
		if (perHour < 1)
			perHour = 1;

		float wait = (1 - m_fHeld) * HOUR_MS / perHour;
		int whole = Math.Ceil(wait);
		if (whole < 1)
			whole = 1;
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Spend one request. Call only after WaitMs returned 0.
	void Take()
	{
		m_fHeld = m_fHeld - 1;
		if (m_fHeld < 0)
			m_fHeld = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A request that was paid for was never sent.
	void GiveBack(int burst)
	{
		m_fHeld = m_fHeld + 1;
		if (m_fHeld > burst)
			m_fHeld = burst;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when nothing has been spent that has not since come back
	bool IsFull(int nowMs, int perHour, int burst)
	{
		Fill(nowMs, perHour, burst);
		return m_fHeld >= burst - 0.001;
	}

	//------------------------------------------------------------------------------------------------
	protected void Fill(int nowMs, int perHour, int burst)
	{
		if (!m_bStarted)
		{
			m_bStarted = true;
			m_fHeld = burst;
			m_iStampMs = nowMs;
			return;
		}

		float elapsed = nowMs - m_iStampMs;
		m_iStampMs = nowMs;
		if (elapsed > 0)
			m_fHeld = m_fHeld + elapsed * perHour / HOUR_MS;
		if (m_fHeld > burst)
			m_fHeld = burst;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: spend everything.
	void ProbeDrain(int nowMs)
	{
		m_bStarted = true;
		m_fHeld = 0;
		m_iStampMs = nowMs;
	}
#endif
}
