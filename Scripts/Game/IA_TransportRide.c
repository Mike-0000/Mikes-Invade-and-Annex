//------------------------------------------------------------------------------------------------
//! One passenger's current helicopter ride, from boarding until it is credited or dropped.
//------------------------------------------------------------------------------------------------
class IA_TransportRide
{
	IEntity m_Vehicle;
	vector m_vBoardPos;
	// Last player seen in the pilot seat, so a pilot who steps out first still gets the credit.
	int m_iPilotPlayerId;
	string m_sPilotGuid;
	string m_sPilotName;
	bool m_bExited;
	int m_iExitMs;
}
