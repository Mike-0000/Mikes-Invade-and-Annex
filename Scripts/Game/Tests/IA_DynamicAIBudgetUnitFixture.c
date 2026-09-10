#ifdef WORKBENCH
// Replace entity life-state inspection only; saved/dead/restore flags stay real.
class IA_DynamicAIBudgetUnitFixture : IA_DynamicAIUnit
{
	override bool IsLogicallyAlive()
	{
		return !m_bDead;
	}
}
#endif
