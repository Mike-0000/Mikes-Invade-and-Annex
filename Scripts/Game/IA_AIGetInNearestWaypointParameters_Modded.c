// GetInNearestVehicle.bt re-reads GetCurrentWaypoint every tick with no
// WaypointIn. Group.bt only re-runs DecideActivity every 0.3s, so a same-
// frame RemoveAllOrders + Move/Defend leaves ActivityGetIn running against
// the new class. Vanilla NodeErrors ("Wrong class of provided Waypoint!").
// Parent already FAILs when the waypoint is gone; treat a class mismatch
// the same so the tree aborts and DecideActivity can pick the new activity.

modded class SCR_AIGetInNearestWaypointParameters
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
		if (!SCR_BoardingTimedWaypoint.Cast(waypointEntity))
			return ENodeResult.FAIL;

		return super.EOnTaskSimulate(owner, dt);
	}
};
