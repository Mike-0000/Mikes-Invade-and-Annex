//------------------------------------------------------------------------------------------------
//! Server: one request to the stats service and the players waiting on its answer.
//------------------------------------------------------------------------------------------------
class IA_BoardFetch
{
	string m_sKey;
	int m_iBoard;
	int m_iSort;
	bool m_bDescending;
	int m_iOffset;
	int m_iLimit;			// 0 asks for the player's own line only
	bool m_bWantMine;		// also ask where m_sGuid stands
	string m_sGuid;

	ref array<int> m_aPlayers = {};
	ref array<int> m_aViews = {};

	//------------------------------------------------------------------------------------------------
	void AddWaiter(int playerId, int viewId)
	{
		int count = m_aPlayers.Count();
		for (int i = 0; i < count; i++)
		{
			if (m_aPlayers[i] != playerId)
				continue;
			// A player looks at one view at a time; only the latest is worth answering.
			m_aViews[i] = viewId;
			return;
		}
		m_aPlayers.Insert(playerId);
		m_aViews.Insert(viewId);
	}

	//------------------------------------------------------------------------------------------------
	//! The player has asked for something else and no longer waits on this.
	//! \return true when nobody is left waiting
	bool DropWaiter(int playerId)
	{
		int at = m_aPlayers.Find(playerId);
		if (at >= 0)
		{
			m_aPlayers.RemoveOrdered(at);
			m_aViews.RemoveOrdered(at);
		}
		return m_aPlayers.IsEmpty();
	}
}
