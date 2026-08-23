// MoveToLocation.bt runs this after Group Move fails. Vanilla then does
// m_Group.GetLeaderEntity().GetOrigin() with only m_Group checked. A
// living group with no leader (wipe, staggered spawn, leader slot empty)
// is `#return` NULL. Fail the action instead of taking that GetOrigin.

modded class SCR_AIProcessFailedMovementResult
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (m_bReturnRunning)
			return ENodeResult.RUNNING;

		if (m_Group && !m_Group.GetLeaderEntity())
		{
			if (m_GroupUtilityComponent)
				FailAction();
			return ENodeResult.FAIL;
		}

		return super.EOnTaskSimulate(owner, dt);
	}
};
