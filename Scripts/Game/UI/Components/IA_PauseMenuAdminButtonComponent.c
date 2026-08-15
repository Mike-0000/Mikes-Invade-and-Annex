class IA_PauseMenuAdminButtonComponent : SCR_ButtonTextComponent
{
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (SCR_Global.IsEditMode())
			return;

		m_OnClicked.Insert(OpenIAAdminConfigMenu);

		if (!IsAdmin())
		{
			w.SetVisible(false);
			w.SetEnabled(false);
		}
	}

	bool IsAdmin()
	{
		if (!Replication.IsRunning())
			return true;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;

		// Use the controller id, not the possessed entity — deploy/map/spectator has no character.
		return SCR_Global.IsAdmin(pc.GetPlayerId());
	}

	void OpenIAAdminConfigMenu()
	{
		if (!IsAdmin())
			return;

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_AdminConfigMenu);
	}
};

