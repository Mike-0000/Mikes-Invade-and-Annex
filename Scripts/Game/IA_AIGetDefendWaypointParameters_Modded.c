// ActivityDefend.bt re-reads GetCurrentWaypoint every tick with no
// WaypointIn. Same 0.3s DecideActivity window as GetInNearest: swapping to
// Move/S&D while Defend is still running NodeErrors. FAIL so the tree
// aborts instead of throwing. A missing defend preset is the same miss.

modded class SCR_AIGetDefendWaypointParameters
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (ENodeResult.FAIL == super.EOnTaskSimulate(owner, dt))
			return ENodeResult.FAIL;

		SCR_DefendWaypoint wp = SCR_DefendWaypoint.Cast(m_Waypoint);
		if (!wp)
			return ENodeResult.FAIL;

		SCR_DefendWaypointPreset defendPreset = wp.GetCurrentDefendPreset();
		if (!defendPreset)
			return ENodeResult.FAIL;

		defendPreset.GetTagsForSearch(m_tagsArray);

		SetVariableOut(PORT_USE_TURRETS, defendPreset.GetUseTurrets());
		SetVariableOut(PORT_SEARCH_TAGS, m_tagsArray);
		SetVariableOut(PORT_FAST_INIT, wp.GetFastInit());
		SetVariableOut(PORT_WAYPOINT_HOLDING_TIME, wp.GetHoldingTime());
		return ENodeResult.SUCCESS;
	}
};
