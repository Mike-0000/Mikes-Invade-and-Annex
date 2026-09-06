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
		if (ENodeResult.FAIL == super.EOnTaskSimulate(owner, dt))
			return ENodeResult.FAIL;

		SCR_BoardingTimedWaypoint wp = SCR_BoardingTimedWaypoint.Cast(m_Waypoint);
		if (!wp)
			return ENodeResult.FAIL;

		SetVariableOut(PORT_BOARDING_PARAMS, wp.GetAllowance());
		SetVariableOut(PORT_WAYPOINT_HOLDING_TIME, wp.GetHoldingTime());
		return ENodeResult.SUCCESS;
	}
};
