// Combat and group move-planning issue GetOut at high priority. Block the
// group dismount *message* for STATIC_ARTILLERY so idle gunners are not
// ordered off the tube. Also keep Pilot/Turret on living movable vehicles
// so VehicleCombatActivity does not dump drivers of "unarmed-looking"
// technicals. Do not swallow SCR_AIGetOutVehicle itself: the vanilla
// artillery BT must hop out after aiming to load a shell, and cargo must
// still receive this message so transport benches empty on contact.
// Explicit GetOut waypoints (truck arrival) still dump crew.

modded class SCR_AIMessageHandling
{
	static override void SendDismountMessage(notnull AIAgent agent, notnull IEntity vehicleEntity, int soldierId, SCR_AIActivityBase relatedActivity,
									notnull AICommunicationComponent myComms, string sendFrom = string.Empty)
	{
		IEntity usageOwner;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(vehicleEntity, usageOwner);
		if (usage && usage.GetVehicleType() == EAIVehicleType.STATIC_ARTILLERY)
			return;

		if (ShouldKeepVehicleCrewMounted(agent, vehicleEntity, relatedActivity))
		{
			Print(string.Format("[IA] StayMounted skip group GetOut from=%1", sendFrom), LogLevel.DEBUG);
			return;
		}

		Print(string.Format("[IA] StayMounted allow group GetOut from=%1", sendFrom), LogLevel.DEBUG);

		float dismountDelay = 0.9 * soldierId;

		ref SCR_AIBoardingParameters bParams = new SCR_AIBoardingParameters();
		ref SCR_AIMessage_GetOut msg = SCR_AIMessage_GetOut.Create(vehicleEntity, bParams, relatedActivity, delay_s: dismountDelay);
		msg.SetReceiver(agent);
		myComms.RequestBroadcast(msg, agent);
	}

	protected static bool ShouldKeepVehicleCrewMounted(notnull AIAgent agent, notnull IEntity vehicleEntity, SCR_AIActivityBase relatedActivity)
	{
		if (SCR_AIGetOutActivity.Cast(relatedActivity))
			return false;

		if (!AgentIsVehicleCrew(agent))
			return false;

		if (!EntityIsOnMovableVehicle(vehicleEntity))
			return false;

		if (CrewMustEvacuate(vehicleEntity))
			return false;

		return true;
	}

	protected static bool AgentIsVehicleCrew(notnull AIAgent agent)
	{
		SCR_ChimeraAIAgent chimeraAgent = SCR_ChimeraAIAgent.Cast(agent);
		if (chimeraAgent && chimeraAgent.m_InfoComponent)
		{
			if (chimeraAgent.m_InfoComponent.HasUnitState(EUnitState.PILOT))
				return true;
			if (chimeraAgent.m_InfoComponent.HasUnitState(EUnitState.IN_TURRET))
				return true;
		}

		IEntity controlled = agent.GetControlledEntity();
		ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return false;

		ECompartmentType type = slot.GetType();
		if (type == ECompartmentType.PILOT)
			return true;
		if (type == ECompartmentType.TURRET)
			return true;

		return false;
	}

	protected static bool EntityIsOnMovableVehicle(notnull IEntity ent)
	{
		IEntity walk = ent;
		while (walk)
		{
			if (Vehicle.Cast(walk))
				return true;

			walk = walk.GetParent();
		}

		return false;
	}

	protected static bool CrewMustEvacuate(notnull IEntity vehicleEntity)
	{
		IEntity hull = vehicleEntity;
		IEntity walk = vehicleEntity;
		while (walk)
		{
			if (Vehicle.Cast(walk))
			{
				hull = walk;
				break;
			}

			walk = walk.GetParent();
		}

		if (SCR_AIVehicleUsability.VehicleIsOnFire(hull))
			return true;

		if (!SCR_AIVehicleUsability.VehicleCanMove(hull))
			return true;

		return false;
	}
};
