// Source-authored scenery with separately owned, optional weapon sockets.
class IA_BaseCompositionAsset
{
	string m_sKey;
	ResourceName m_Prefab;
	vector m_vMins;
	vector m_vMaxs;
	int m_iExpanded;
	ref array<ref IA_BaseGunSocket> m_aSockets = {};

	static IA_BaseCompositionAsset Create(string key, ResourceName prefab, vector mins, vector maxs, int expanded)
	{
		ref IA_BaseCompositionAsset asset = new IA_BaseCompositionAsset();
		asset.m_sKey = key;
		asset.m_Prefab = prefab;
		asset.m_vMins = mins;
		asset.m_vMaxs = maxs;
		asset.m_iExpanded = expanded;
		return asset;
	}

	IA_BaseGunSocket AddSocket(int kind)
	{
		ref IA_BaseGunSocket socket = new IA_BaseGunSocket();
		socket.m_iKind = kind;
		m_aSockets.Insert(socket);
		return socket;
	}
}
