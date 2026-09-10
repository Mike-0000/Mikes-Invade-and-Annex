// One census result. The allocator writes only m_iDesired; the owner retains
// authority over physical entities and admission to the bounded work queue.
class IA_DynamicAIBudgetEntry
{
	IA_DynamicAIBudgetCache m_Cache;
	int m_iAlive;
	int m_iPhysical;
	int m_iProtected;
	int m_iPreviousDesired;
	int m_iDesired;
	float m_fDistance;
	bool m_bInRange;
	int m_iOrder;
}
