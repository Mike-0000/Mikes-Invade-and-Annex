//------------------------------------------------------------------------------------------------
//! I&A already toasts task create/complete via IA_NotificationDisplay.
//! Vanilla SCR_EditorTask also posts the top-of-screen PopupUI ("COMPLETED")
//! and the GM notification log; swallow those so only the custom UI remains.
//------------------------------------------------------------------------------------------------
modded class SCR_EditorTask
{
	//------------------------------------------------------------------------------------------------
	//! Skip vanilla PopupUI ("COMPLETED", "NEW", "FAILED", "CANCELLED").
	override protected void Rpc_PopUpNotification(string prefix, bool alwaysInEditor)
	{
	}

	//------------------------------------------------------------------------------------------------
	override protected void ShowPopUpNotification(string subtitle)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Skip the vanilla notification-log entry that accompanies task state changes.
	override void ShowTaskNotification(ENotification taskNotification, bool SendOverNetwork = false)
	{
	}
}
