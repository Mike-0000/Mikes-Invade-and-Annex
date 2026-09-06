// Ordered prefab-local transforms from the scenery root to the vanilla gun.
// Keeping the chain avoids approximating nested rotations or elevated mounts.
class IA_BaseGunSocket
{
	int m_iKind; // 0 PKM, 1 NSV, 2 scoped NSV, 3 AA NSV-SPP
	ref array<vector> m_aPositions = {};
	ref array<vector> m_aAngles = {};

	void Append(vector position, vector prefabAngles)
	{
		m_aPositions.Insert(position);
		m_aAngles.Insert(prefabAngles);
	}

	void Transform(vector parent[4], out vector result[4])
	{
		Math3D.MatrixCopy(parent, result);
		int count = m_aPositions.Count();
		for (int i = 0; i < count; i++)
		{
			vector local[4], next[4];
			vector angles = m_aAngles[i];
			Math3D.AnglesToMatrix(Vector(angles[1], angles[0], angles[2]), local);
			local[3] = m_aPositions[i];
			Math3D.MatrixMultiply4(result, local, next);
			Math3D.MatrixCopy(next, result);
		}
	}
}
