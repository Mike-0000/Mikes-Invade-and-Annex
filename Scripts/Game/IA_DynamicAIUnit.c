// One successfully seeded soldier. A failed restoration never consumes its record.
class IA_DynamicAIUnit
{
	ResourceName m_sPrefab;
	ref Resource m_Resource;
	vector m_aTransform[4];
	IEntity m_Entity;
	bool m_bRestored;
	bool m_bDead;
	int m_iFailures;
	int m_iNextAttemptMs;
	bool m_bBudgetAdmitted;
	bool m_bBudgetProtected;
	bool m_bBudgetLeader;
	int m_iBudgetLiveSinceMs;
	float m_fNearestPlayerM = 10000000;

	RplId m_HostId;
	int m_iOccupancyMgrId;
	int m_iOccupancySlotId;
	IA_DynamicAIOccupancyKind m_eOccupancyKind;

	bool Capture(IEntity entity)
	{
		m_sPrefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		if (m_sPrefab.IsEmpty())
			return false;
		m_Resource = Resource.Load(m_sPrefab);
		if (!m_Resource)
			return false;
		entity.GetWorldTransform(m_aTransform);
		m_Entity = entity;
		return true;
	}

	// Reserve-first spawn: a soldier that was never created still owns a roster
	// slot. Restoration uses the same prefab/transform path as a captured pawn.
	void SeedReserve(ResourceName prefab, vector position)
	{
		m_sPrefab = prefab;
		m_aTransform[0] = "1 0 0";
		m_aTransform[1] = "0 1 0";
		m_aTransform[2] = "0 0 1";
		m_aTransform[3] = position;
		m_Entity = null;
		m_bRestored = false;
		m_bDead = false;
	}

	bool HasOccupancy()
	{
		if (m_eOccupancyKind == IA_DynamicAIOccupancyKind.None)
			return false;
		return m_HostId.IsValid();
	}

	void ClearOccupancy()
	{
		m_HostId = RplId.Invalid();
		m_iOccupancyMgrId = -1;
		m_iOccupancySlotId = -1;
		m_eOccupancyKind = IA_DynamicAIOccupancyKind.None;
	}

	void SetOccupancy(RplId hostId, int mgrId, int slotId, IA_DynamicAIOccupancyKind kind)
	{
		m_HostId = hostId;
		m_iOccupancyMgrId = mgrId;
		m_iOccupancySlotId = slotId;
		m_eOccupancyKind = kind;
	}

	vector GetPosition()
	{
		return m_aTransform[3];
	}

	bool IsInside(vector center, float radius)
	{
		return vector.DistanceSq(center, GetPosition()) <= radius * radius;
	}

	bool IsLogicallyAlive()
	{
		if (m_bDead)
			return false;
		if (!m_bRestored)
			return true;
		ChimeraCharacter pawn = ChimeraCharacter.Cast(m_Entity);
		return pawn && pawn.GetCharacterController() && pawn.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD;
	}

	void RecordFailure(int now)
	{
		m_iFailures++;
		m_iNextAttemptMs = now + 1000;
		if (m_iFailures == 5)
			Print(string.Format("[IA][DynamicAI] Retrying soldier restoration at %1, prefab %2. Its roster slot is retained.", GetPosition(), m_sPrefab), LogLevel.WARNING);
	}
}
