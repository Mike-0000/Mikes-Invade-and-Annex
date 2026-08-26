// GetInNearestVehicle.bt keeps ticking after the boarding waypoint is
// gone. Vanilla HasNoAvailableCompartment reads
// m_WaypointParameter.m_bIsDriverAllowed with no null check. The member
// is not a ref, so RemoveAllOrders GC's SCR_AIBoardingParameters and the
// next simulate is NULL. The sphere-query path already comments that the
// waypoint can vanish; the cached-vehicle path does not re-read the port.
// Sibling GetEmptyCompartment defaults a missing params object; this
// node does not. Fail the search instead of NPEing.

modded class SCR_AIFindAvailableVehicle
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (!m_WaypointParameter)
			GetVariableIn(PORT_SEARCH_PARAMS, m_WaypointParameter);

		if (!m_WaypointParameter)
		{
			m_VehicleToTestForCompartments = null;
			m_Compartment = null;
			ClearVariable(PORT_VEHICLE_OUT);
			ClearVariable(PORT_ROLE_OUT);
			return ENodeResult.FAIL;
		}

		return super.EOnTaskSimulate(owner, dt);
	}
};
