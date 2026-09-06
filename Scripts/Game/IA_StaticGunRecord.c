class IA_StaticGunRecord
{
	ref IA_DynamicBaseEmplacementSpec m_Spec;
	ref IA_EmplacementProfile m_Profile;
	IEntity m_Root;
	IA_StaticGunComponent m_Gun;
	ref IA_AiGroup m_Crew;
	int m_iCrewSpawnMs;
	bool m_bCrewSpawnSettled;
	vector m_vOrigin;
	float m_fYaw;
	bool m_bAuthoredAccess;
	vector m_vAccess;

	bool ContainsReservedPoint(vector position, float margin = 0)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), mat);
		vector delta = position - m_vOrigin;
		float x = vector.Dot(delta, mat[0]);
		float z = vector.Dot(delta, mat[2]);
		return Math.AbsFloat(x) < 1.5 + margin && z > -4 - margin && z < 1.25 + margin;
	}

	bool HasPlayerOccupantOrTransition()
	{
		if (!m_Root)
			return false;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return true; // fail closed when the independent safety source is absent
		TurretCompartmentSlot seat;
		if (m_Gun)
			seat = m_Gun.GetSeat();
		if (seat && seat.GetOccupant() && pm.GetPlayerIdFromControlledEntity(seat.GetOccupant()) > 0)
			return true;
		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity pawn = pm.GetPlayerControlledEntity(id);
			if (!pawn)
				continue;
			IEntity parent = pawn.GetParent();
			while (parent)
			{
				if (parent == m_Root)
					return true;
				parent = parent.GetParent();
			}
			if (seat && seat.IsReservedBy(pawn))
				return true;
			ChimeraCharacter character = ChimeraCharacter.Cast(pawn);
			if (!character)
				continue;
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (access && (access.IsGettingIn() || access.IsGettingOut()))
			{
				vector mat[4];
				pawn.GetWorldTransform(mat);
				if (vector.Distance(mat[3], m_vOrigin) < 10)
					return true;
			}
		}
		return false;
	}
}
