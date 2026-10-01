//------------------------------------------------------------------------------------------------
//! One player's transport rating as this server knows it. The backend owns the
//! total; this is the last fetched value plus points it has not acknowledged yet.
//------------------------------------------------------------------------------------------------
class IA_TransportPilotRecord
{
	static const int RATING_UNKNOWN = -1;

	string m_sGuid;
	string m_sName;
	// Global rating across every server, RATING_UNKNOWN until the backend answers.
	int m_iGlobalRating = RATING_UNKNOWN;
	// Earned here, not yet handed to a submit batch.
	int m_iPendingPoints;
	int m_iPendingInsertions;
	// In a submit batch the backend has not acknowledged.
	int m_iSentPoints;
	int m_iSentInsertions;
	int m_iLastRequestMs;
	bool m_bRequested;

	//------------------------------------------------------------------------------------------------
	//! Rating used for unlocks. Stays unknown until the global total is fetched,
	//! so a backend outage cannot unlock anything from local points alone.
	int GetRating()
	{
		if (m_iGlobalRating < 0)
			return RATING_UNKNOWN;
		return m_iGlobalRating + m_iPendingPoints + m_iSentPoints;
	}
}
