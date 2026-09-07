// Site-owned guns are a posted station. Vanilla hops a turret after 1.2 s when
// a contact sits a few degrees outside traverse, then MovePlanning dumps every
// static unpilotable seat. Keep the gunner on this gun; assignment remounts
// if something still ejects them. Ordinary vehicles, other tripods, mortars
// and individual target selection are unchanged.
modded class SCR_AICombatComponent
{
	override void TryAddDismountTurretActions(vector targetPos, bool addGetOut = true, bool addGetIn = true, float dangerLookPriority = 0)
	{
		if (m_CurrentCompartmentSlot && IA_StaticGunComponent.FindOnNearestParent(m_CurrentCompartmentSlot.GetOwner()))
		{
			addGetOut = false;
			addGetIn = false;
		}
		super.TryAddDismountTurretActions(targetPos, addGetOut, addGetIn, dangerLookPriority);
	}
}
