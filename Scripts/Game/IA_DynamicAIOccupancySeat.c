enum IA_DynamicAIOccupancyKind
{
	None,
	Vehicle,
	Gun,
	Mortar
}

enum IA_DynamicAIOccupancyState
{
	Idle,
	Qualify,
	Quiesce,
	Eject,
	Commit,
	Abort
}

// One occupied seat belonging to a host being virtualized.
class IA_DynamicAIOccupancySeat
{
	IEntity m_Pawn;
	IA_DynamicAIGroupCache m_Cache;
	RplId m_HostId;
	int m_iMgrId;
	int m_iSlotId;
	IA_DynamicAIOccupancyKind m_eKind;
	bool m_bEjected;
	bool m_bDeleted;
	bool m_bRemounted;
	vector m_aTransform[4];

	bool MatchesSlot(int mgrId, int slotId)
	{
		if (mgrId != m_iMgrId)
			return false;
		return slotId == m_iSlotId;
	}
}
