// Keep STATIC_ARTILLERY gunners on the tube, IA site-owned static guns on
// their station, and vehicle gunners in movable hulls. Vanilla combat
// dismounts a turret when the target is outside traverse; mortars, posted
// PKM/NSV/AA and car/truck MGs fail that check on contact.
// Child-entity turrets also miss the hull driver, so vanilla treats them
// as unmanned. Usage for mortars lives on a parent of GetVehicle().

modded class SCR_AICombatComponent
{
	protected bool m_bLoggedStayMountedTurretSkip;

	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided, out float threatPriority)
	{
		if (CurrentVehicleIsStaticArtillery())
			return false;

		if (CurrentCompartmentIsIaStaticGun())
			return false;

		if (CurrentEntityIsOnMovableVehicle())
		{
			if (!m_bLoggedStayMountedTurretSkip)
			{
				if (IA_Log.IsDebugEnabled())
				{
					Print("[IA] StayMounted skip turret-dismount (movable vehicle)", LogLevel.NORMAL);
				}
				m_bLoggedStayMountedTurretSkip = true;
			}

			return false;
		}

		return super.DismountTurretCondition(targetPos, targetPosProvided, threatPriority);
	}

	protected bool CurrentCompartmentIsIaStaticGun()
	{
		if (m_CurrentCompartmentSlot && IA_StaticGunComponent.FindOnNearestParent(m_CurrentCompartmentSlot.GetOwner()))
			return true;
		return IA_StaticGunComponent.FindOnNearestParent(m_CurrentVehicle) != null;
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
