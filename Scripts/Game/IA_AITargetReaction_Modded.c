// Vanilla hops STATIC_WEAPON gunners off the seat when they "can't attack"
// (target outside a tight traverse). Posted IA crews stay on that station.
modded class SCR_AITargetReaction_RetreatFromEnemy
{
	override void PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, BaseTarget baseTarget, vector lastSeenPosition)
	{
		if (IA_StaticGunAssignment.IsPostedPawn(utility.m_OwnerEntity))
			return;
		super.PerformReaction(utility, threatSystem, baseTarget, lastSeenPosition);
	}
}
