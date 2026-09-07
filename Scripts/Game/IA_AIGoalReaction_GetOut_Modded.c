// DefendWaypoint.OnDeselected broadcasts GetOut directly, not through
// SendDismountMessage. Drop that goal for a posted IA gunner.
modded class SCR_AIGoalReaction_GetOutVehicle
{
	override void PerformReaction(notnull SCR_AIUtilityComponent utility, SCR_AIMessageBase message)
	{
		if (IA_StaticGunAssignment.IsPostedPawn(utility.m_OwnerEntity))
			return;
		super.PerformReaction(utility, message);
	}

	override void PerformReaction(notnull SCR_AIGroupUtilityComponent utility, SCR_AIMessageBase message)
	{
		if (utility.m_Owner)
		{
			array<AIAgent> agents = {};
			utility.m_Owner.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (IA_StaticGunAssignment.IsPostedAgent(agent))
					return;
			}
		}
		super.PerformReaction(utility, message);
	}
}
