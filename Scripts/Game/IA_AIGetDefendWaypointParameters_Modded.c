// ActivityDefend.bt re-reads GetCurrentWaypoint every tick with no
// WaypointIn. Same 0.3s DecideActivity window as GetInNearest: swapping to
// Move/S&D while Defend is still running NodeErrors. FAIL so the tree
// aborts instead of throwing. A missing defend preset is the same miss.

modded class SCR_AIGetDefendWaypointParameters
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		IEntity waypointEntity;
		if (!GetVariableIn(PORT_WAYPOINT_IN, waypointEntity))
		{
			AIGroup group = AIGroup.Cast(owner);
			if (!group)
				return ENodeResult.FAIL;
			waypointEntity = group.GetCurrentWaypoint();
		}
		SCR_DefendWaypoint wp = SCR_DefendWaypoint.Cast(waypointEntity);
		if (!wp)
			return ENodeResult.FAIL;

		SCR_DefendWaypointPreset defendPreset = wp.GetCurrentDefendPreset();
		if (!defendPreset)
			return ENodeResult.FAIL;

		return super.EOnTaskSimulate(owner, dt);
	}
};
