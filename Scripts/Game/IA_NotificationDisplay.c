//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class IA_NotificationDisplay : SCR_InfoDisplayExtended
{
	protected RichTextWidget m_wInfoText;
	protected RichTextWidget m_RedText;
	protected RichTextWidget m_YellowText;

	protected Widget m_wNotificationOverlay;

	//------------------------------------------------------------------------------------------------
	//! Starts drawing the display
	override void DisplayStartDraw(IEntity owner)
	{
		//Print("IA_NotificationDisplay::DisplayStartDraw - Initializing notification display for owner: " + owner, LogLevel.NORMAL);
		super.DisplayStartDraw(owner);

		// Assuming your layout file has widgets named "InfoText" and "NotificationOverlay"
		m_wNotificationOverlay = m_wRoot.FindAnyWidget("ia_root"); 
		m_wInfoText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText0"));
		m_RedText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText1"));
		m_YellowText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText3"));
		
		//Print("IA_NotificationDisplay::DisplayStartDraw - Widgets found - Overlay: " + (m_wNotificationOverlay != null) + " InfoText: " + (m_wInfoText != null) + " RedText: " + (m_RedText != null) + " YellowText: " + (m_YellowText != null), LogLevel.NORMAL);
		
		ShowHUD(false, "");
		m_bIsEnabled = false;
		
		//Print("IA_NotificationDisplay::DisplayStartDraw - Initialization complete, display disabled", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//override void DisplayUpdate(IEntity owner, float timeSlice)
	//{
		// Not currently used for simple notifications
	//}

	//------------------------------------------------------------------------------------------------
	//! Toggles if HUD should be shown or hidden
	protected void ShowHUD(bool show, string color)
	{
		//Print("IA_NotificationDisplay::ShowHUD - Called with show: " + show + " color: " + color + " enabled: " + m_bIsEnabled, LogLevel.NORMAL);
		
		if(!show){
			//Print("IA_NotificationDisplay::ShowHUD - Hiding all widgets", LogLevel.NORMAL);
			if(m_wNotificationOverlay)
				m_wNotificationOverlay.SetVisible(show);
			if(m_RedText)
				m_RedText.SetVisible(show);
			if(m_wInfoText)
				m_wInfoText.SetVisible(show);
			if(m_YellowText)
				m_YellowText.SetVisible(show);
			return;
		}
			
		//Print("IA_NotificationDisplay::ShowHUD - Showing widgets for color: " + color, LogLevel.NORMAL);
		if (m_wNotificationOverlay)
			m_wNotificationOverlay.SetVisible(show);
		if (m_wInfoText && color == "")
			m_wInfoText.SetVisible(show);
		else if(m_RedText && color == "red")
			m_RedText.SetVisible(show);
		else if(m_YellowText && color == "yellow")
			m_YellowText.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a notification message
	void ShowNotification(string message, bool show, string color)
	{
		//Print("IA_NotificationDisplay::ShowNotification - Message: '" + message + "' show: " + show + " color: '" + color + "'", LogLevel.NORMAL);
		
		if(color == "red")
			m_RedText.SetText(message);
		else if(color == "")
			m_wInfoText.SetText(message);
		else if(color == "yellow")
			m_YellowText.SetText(message);
		else{
			//Print("IA_NotificationDisplay::ShowNotification - Unknown color '" + color + "', defaulting to yellow", LogLevel.NORMAL);
			m_YellowText.SetText(message);
			ShowHUD(show, "yellow");
			m_bIsEnabled = show; 
			return;
		}
		ShowHUD(show, color);
		m_bIsEnabled = show; 
		
		//Print("IA_NotificationDisplay::ShowNotification - Notification displayed, enabled state: " + m_bIsEnabled, LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	// Method to be called when a new task is created
	void DisplayTaskCreatedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayTaskCreatedNotification - Task: '" + taskName + "' creating red notification with 5s auto-hide", LogLevel.NORMAL);
		ShowNotification("New Objective: " + taskName, true, "red");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 5000, false); // Hide after 5 seconds
	}
		void DisplayTaskCompletedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayTaskCompletedNotification - Task: '" + taskName + "' creating yellow notification with 9s auto-hide", LogLevel.NORMAL);
		ShowNotification("Objective Completed: " + taskName, true, "yellow");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 9000, false); // Hide after 5 seconds
	}

	//------------------------------------------------------------------------------------------------
	// Method to be called when a task is completed
	void DisplayAreaCompletedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayAreaCompletedNotification - Area: '" + taskName + "' creating default notification with 12s auto-hide", LogLevel.NORMAL);
		ShowNotification("Objective Area Completed, RTB and await tasking.", true, "");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 12000, false); // Hide after 5 seconds
	}

	//------------------------------------------------------------------------------------------------
	// Method to hide the notification display
	void HideNotification()
	{
		//Print("IA_NotificationDisplay::HideNotification - Hiding all notifications, current enabled state: " + m_bIsEnabled, LogLevel.NORMAL);
		ShowNotification("", false, "");
		//Print("IA_NotificationDisplay::HideNotification - All notifications hidden", LogLevel.NORMAL);
	}
}; 