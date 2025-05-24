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
		super.DisplayStartDraw(owner);

		// Assuming your layout file has widgets named "InfoText" and "NotificationOverlay"
		m_wNotificationOverlay = m_wRoot.FindAnyWidget("ia_root"); 
		m_wInfoText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText0"));
		m_RedText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText1"));
		m_YellowText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText3"));
		ShowHUD(false, "");
		m_bIsEnabled = false;
	}

	//------------------------------------------------------------------------------------------------
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		// Not currently used for simple notifications
	}

	//------------------------------------------------------------------------------------------------
	//! Toggles if HUD should be shown or hidden
	protected void ShowHUD(bool show, string color)
	{
		if(!show){
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
		if(color == "red")
			m_RedText.SetText(message);
		else if(color == "")
			m_wInfoText.SetText(message);
		else if(color == "yellow")
			m_YellowText.SetText(message);
		else{
			m_YellowText.SetText(message);
			ShowHUD(show, "yellow");
			m_bIsEnabled = show; 
			return;
		}
		ShowHUD(show, color);
		m_bIsEnabled = show; 
	}
	
	//------------------------------------------------------------------------------------------------
	// Method to be called when a new task is created
	void DisplayTaskCreatedNotification(string taskName)
	{
		ShowNotification("New Objective: " + taskName, true, "red");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 5000, false); // Hide after 5 seconds
	}
		void DisplayTaskCompletedNotification(string taskName)
	{
		ShowNotification("Objective Completed: " + taskName, true, "yellow");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 9000, false); // Hide after 5 seconds
	}

	//------------------------------------------------------------------------------------------------
	// Method to be called when a task is completed
	void DisplayAreaCompletedNotification(string taskName)
	{
		ShowNotification("Objective Area Completed, RTB and await tasking.", true, "");
		// Optional: Auto-hide after a delay
		GetGame().GetCallqueue().CallLater(this.HideNotification, 12000, false); // Hide after 5 seconds
	}

	//------------------------------------------------------------------------------------------------
	// Method to hide the notification display
	void HideNotification()
	{
		ShowNotification("", false, "");
	}
}; 