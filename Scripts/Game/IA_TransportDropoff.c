//------------------------------------------------------------------------------------------------
//! Passengers credited to one pilot that their HUD has not been told about yet.
//! IA_TransportPilotTracker reports the whole batch as a single card update.
//------------------------------------------------------------------------------------------------
class IA_TransportDropoff
{
	int m_iPilotPlayerId;
	string m_sPilotGuid;
	int m_iTroops;
	int m_iPoints;
	float m_fEdgeSum;
	//! Tick of the first passenger in the batch; an undeliverable batch expires from here.
	int m_iFirstMs;

	//------------------------------------------------------------------------------------------------
	void Add(int points, float edge)
	{
		if (edge < 0)
			edge = 0;
		m_iTroops = m_iTroops + 1;
		m_iPoints = m_iPoints + points;
		m_fEdgeSum = m_fEdgeSum + edge;
	}

	//------------------------------------------------------------------------------------------------
	//! Average metres from the dropoffs to the objective circle.
	int AverageEdge()
	{
		if (m_iTroops <= 0)
			return 0;
		return Math.Round(m_fEdgeSum / m_iTroops);
	}
}
