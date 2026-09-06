// A fixed authoring candidate, independent of survey modules. A candidate never
// promises an installation: actual support, sandbag and firing traces must pass.
class IA_DynamicBaseEmplacementSpec
{
	string m_sId;
	string m_sPanelId;
	vector m_vLocalPosition;
	float m_fYaw;
	int m_iSide;
	bool m_bHeavy;
	ref IA_BaseGunSocket m_Socket;

	static IA_DynamicBaseEmplacementSpec Create(string id, string panel, vector position, float yaw, int side, bool heavy)
	{
		ref IA_DynamicBaseEmplacementSpec s = new IA_DynamicBaseEmplacementSpec();
		s.m_sId = id;
		s.m_sPanelId = panel;
		s.m_vLocalPosition = position;
		s.m_fYaw = yaw;
		s.m_iSide = side;
		s.m_bHeavy = heavy;
		return s;
	}
}
