class IA_PauseMenuGmDirectorButtonComponent : SCR_ButtonTextComponent
{
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (SCR_Global.IsEditMode())
			return;

		m_OnClicked.Insert(OpenIAGmDirectorMenu);

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

		return SCR_Global.IsAdmin(pc.GetPlayerId());
	}

	void OpenIAGmDirectorMenu()
	{
		if (!IsAdmin())
			return;

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_GmDirectorMenu);
	}
};
