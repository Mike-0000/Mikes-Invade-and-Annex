// Posted IA gunners stay in the seat. Vanilla retreat, waypoint deselect and
// combat dismount all queue SCR_AIGetOutVehicle at high priority; complete
// that action without leaving. Assignment unregisters before a real release.
modded class SCR_AIGetOutVehicle
{
	override float CustomEvaluate()
	{
		if (m_Utility && IA_StaticGunAssignment.IsPostedPawn(m_Utility.m_OwnerEntity))
			return 0;
		return super.CustomEvaluate();
	}

	override void OnActionSelected()
	{
		if (m_Utility && IA_StaticGunAssignment.IsPostedPawn(m_Utility.m_OwnerEntity))
		{
			Complete();
			return;
		}
		super.OnActionSelected();
	}
}
