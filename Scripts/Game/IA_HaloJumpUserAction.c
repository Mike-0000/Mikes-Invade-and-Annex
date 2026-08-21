//------------------------------------------------------------------------------------------------
//! I&A Role Switcher HALO action. Keeps HALO available only on low-population sessions.
//! The cutoff is IA_Config.m_iHaloJumpMaxPlayers (admin config, default 15).
//------------------------------------------------------------------------------------------------
class IA_HaloJumpUserAction : MHJ_HaloJumpUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return CanBePerformedScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		return IsBelowPlayerLimit();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		super.PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsBelowPlayerLimit()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		return playerManager.GetPlayerCount() < IA_MissionInitializer.GetHaloJumpMaxPlayers();
	}
}
