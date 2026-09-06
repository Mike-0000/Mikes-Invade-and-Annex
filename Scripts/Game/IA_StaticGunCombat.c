// Vanilla may dismount to investigate threats outside a turret's arc. Preserve
// that reaction but never queue a return to an IA site-owned gun. Ordinary
// vehicles, other tripods, mortars and individual target selection are unchanged.
modded class SCR_AICombatComponent
{
	override void TryAddDismountTurretActions(vector targetPos, bool addGetOut = true, bool addGetIn = true, float dangerLookPriority = 0)
	{
		if (m_CurrentCompartmentSlot && IA_StaticGunComponent.Find(m_CurrentCompartmentSlot.GetOwner()))
			addGetIn = false;
		super.TryAddDismountTurretActions(targetPos, addGetOut, addGetIn, dangerLookPriority);
	}
}
