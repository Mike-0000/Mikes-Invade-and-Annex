// Keep STATIC_ARTILLERY gunners on the tube. Vanilla combat dismounts a turret
// when the target is outside traverse; mortars fail that check on close contact.
// Usage lives on a parent of GetVehicle() for slotted compositions.

modded class SCR_AICombatComponent
{
	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided, out float threatPriority)
	{
		if (CurrentVehicleIsStaticArtillery())
			return false;

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
};
