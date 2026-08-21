// MovePlanning dumps every static/unpilotable vehicle. Mortars match that filter,
// so a defend/move plan would order gunners off the tubes. Skip STATIC_ARTILLERY.
// Override EOnTaskSimulate: Testing_State is not marked override in vanilla and
// may not bind, which is why gunners still hopped off immediately after boarding.

modded class SCR_AILeaveStaticVehicles
{
	protected bool m_bLoggedStayMountedLeave;

	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (m_Utility && m_Utility.m_VehicleMgr)
		{
			ref array<ref SCR_AIGroupVehicle> used = new array<ref SCR_AIGroupVehicle>();
			m_Utility.m_VehicleMgr.GetAllVehicles(used);
			foreach (SCR_AIGroupVehicle groupVehicle : used)
			{
				if (GroupVehicleIsStaticArtillery(groupVehicle))
					return ENodeResult.SUCCESS;
			}
		}

		if (!m_bLoggedStayMountedLeave)
		{
			Print("[IA] StayMounted LeaveStaticVehicles proceeding (non-artillery)", LogLevel.DEBUG);
			m_bLoggedStayMountedLeave = true;
		}

		return super.EOnTaskSimulate(owner, dt);
	}

	protected bool GroupVehicleIsStaticArtillery(SCR_AIGroupVehicle groupVehicle)
	{
		if (!groupVehicle)
			return false;

		SCR_AIVehicleUsageComponent usage = groupVehicle.GetVehicleUsageComponent();
		if (usage && usage.GetVehicleType() == EAIVehicleType.STATIC_ARTILLERY)
			return true;

		IEntity ent = groupVehicle.GetEntity();
		if (!ent)
			return false;

		IEntity usageOwner;
		SCR_AIVehicleUsageComponent found = SCR_AIVehicleUsageComponent.FindOnNearestParent(ent, usageOwner);
		if (found && found.GetVehicleType() == EAIVehicleType.STATIC_ARTILLERY)
			return true;

		return false;
	}
};
