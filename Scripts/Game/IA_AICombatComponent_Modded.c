// Keep STATIC_ARTILLERY gunners on the tube, and keep vehicle gunners in
// movable hulls. Vanilla combat dismounts a turret when the target is
// outside traverse; mortars and car/truck MGs fail that check on contact.
// Child-entity turrets also miss the hull driver, so vanilla treats them
// as unmanned. Usage for mortars lives on a parent of GetVehicle().

modded class SCR_AICombatComponent
{
	protected bool m_bLoggedStayMountedTurretSkip;

	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided, out float threatPriority)
	{
		if (CurrentVehicleIsStaticArtillery())
			return false;

		if (CurrentEntityIsOnMovableVehicle())
		{
			if (!m_bLoggedStayMountedTurretSkip)
			{
				Print("[IA] StayMounted skip turret-dismount (movable vehicle)", LogLevel.DEBUG);
				m_bLoggedStayMountedTurretSkip = true;
			}

			return false;
		}

		return super.DismountTurretCondition(targetPos, targetPosProvided, threatPriority);
	}

	protected bool CurrentVehicleIsStaticArtillery()
	{
		IEntity veh = m_CurrentVehicle;
		if (!veh && m_CurrentCompartmentSlot)
			veh = m_CurrentCompartmentSlot.GetVehicle();
		if (!veh)
			return false;

		IEntity usageOwner;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(veh, usageOwner);
		if (!usage)
			return false;

		return usage.GetVehicleType() == EAIVehicleType.STATIC_ARTILLERY;
	}

	protected bool CurrentEntityIsOnMovableVehicle()
	{
		IEntity ent = m_CurrentVehicle;
		if (!ent && m_CurrentCompartmentSlot)
			ent = m_CurrentCompartmentSlot.GetVehicle();
		if (!ent)
			return false;

		IEntity walk = ent;
		while (walk)
		{
			if (Vehicle.Cast(walk))
				return true;

			walk = walk.GetParent();
		}

		return false;
	}
};
