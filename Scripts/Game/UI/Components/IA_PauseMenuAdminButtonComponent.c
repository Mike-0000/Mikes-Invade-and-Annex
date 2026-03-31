class IA_PauseMenuAdminButtonComponent : SCR_ButtonTextComponent
{
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		if (SCR_Global.IsEditMode())
			return;

		if (!IsAdmin())
		{
			w.SetVisible(false);
			w.SetEnabled(false);
			return;
		}
			
		m_OnClicked.Insert(OpenIAAdminConfigMenu);
	}
	
	bool IsAdmin()
	{
		// Check if player is admin
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc) return false;
		
		// Generic check for now, can be improved with specific admin tools component check if needed
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pc.GetControlledEntity());
		return SCR_Global.IsAdmin(playerId);
	}
	
	void OpenIAAdminConfigMenu() {
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_AdminConfigMenu);
	}
};
