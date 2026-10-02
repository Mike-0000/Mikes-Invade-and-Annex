//------------------------------------------------------------------------------------------------
//! Server: one answer of the stats service, kept for a short while so players looking at the
//! same board share one request. A page of a board, or one player's own line on it.
//------------------------------------------------------------------------------------------------
class IA_BoardPage
{
	int m_iTotal;
	int m_iStampMs;					// System.GetTickCount() when it arrived
	ref array<string> m_aRows = {};	// packed IA_BoardRow lines
}
