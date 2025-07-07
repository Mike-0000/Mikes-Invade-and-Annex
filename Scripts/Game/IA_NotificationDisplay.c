//------------------------------------------------------------------------------------------------
class IA_NotificationInfo
{
	string m_sMessage;
	string m_sColor;
	int m_iDuration;

	void IA_NotificationInfo(string message, string color, int duration)
	{
		m_sMessage = message;
		m_sColor = color;
		m_iDuration = duration;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class IA_NotificationDisplay : SCR_InfoDisplayExtended
{
	protected RichTextWidget m_wInfoText;
	protected RichTextWidget m_RedText;
	protected RichTextWidget m_YellowText;

	protected Widget m_wNotificationOverlay;

	// --- BEGIN ADDED: Notification Queue System ---
	protected ref array<ref IA_NotificationInfo> m_notificationQueue = new array<ref IA_NotificationInfo>();
	protected bool m_bIsDisplaying = false;
	// --- END ADDED ---

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
	void QueueNotification(string message, string color, int duration, bool show = true)
	{
		if (!show)
			return;
		
		m_notificationQueue.Insert(new IA_NotificationInfo(message, color, duration));
        
        // If not already displaying a notification, start processing the queue.
        if (!m_bIsDisplaying)
        {
            ProcessNotificationQueue();
        }
	}
	
	protected void ProcessNotificationQueue()
    {
        // Don't process if a notification is already on screen or the queue is empty.
        if (m_bIsDisplaying || m_notificationQueue.IsEmpty())
        {
            return;
        }

        m_bIsDisplaying = true;

        // Get the next notification from the queue.
        IA_NotificationInfo info = m_notificationQueue[0];
        m_notificationQueue.Remove(0);

        // Actually display the notification.
        _InternalShowNotification(info.m_sMessage, true, info.m_sColor);

        // Schedule the hiding of this notification.
        GetGame().GetCallqueue().CallLater(this.HideCurrentAndProcessNext, info.m_iDuration);
    }
	
	protected void _InternalShowNotification(string message, bool show, string color)
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
	
	protected void HideCurrentAndProcessNext()
    {
        // Hide the currently displayed notification.
        _InternalShowNotification("", false, "");

        // Set displaying to false so the next notification can be shown.
        m_bIsDisplaying = false;
        
        // Wait 1 second before processing the next item in the queue.
        GetGame().GetCallqueue().CallLater(this.ProcessNotificationQueue, 1000); 
    }

	//------------------------------------------------------------------------------------------------
	// Method to be called when a new task is created
	void DisplayTaskCreatedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayTaskCreatedNotification - Task: '" + taskName + "' creating red notification with 5s auto-hide", LogLevel.NORMAL);
		QueueNotification("New Objective: " + taskName, "red", 5000);
	}
	
		void DisplaySideTaskCreatedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayTaskCreatedNotification - Task: '" + taskName + "' creating red notification with 5s auto-hide", LogLevel.NORMAL);
		QueueNotification("New Side Objective: " + taskName, "red", 5000);
	}
	
		void DisplayTaskCompletedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayTaskCompletedNotification - Task: '" + taskName + "' creating yellow notification with 9s auto-hide", LogLevel.NORMAL);
		QueueNotification("Objective Completed: " + taskName, "yellow", 9000);
	}

	//------------------------------------------------------------------------------------------------
	// Method to be called when a task is completed
	void DisplayAreaCompletedNotification(string taskName)
	{
		//Print("IA_NotificationDisplay::DisplayAreaCompletedNotification - Area: '" + taskName + "' creating default notification with 12s auto-hide", LogLevel.NORMAL);
		QueueNotification("Objective Area Completed, RTB and await tasking.", "", 12000);
	}

	//------------------------------------------------------------------------------------------------
	// Method to hide the notification display
	void HideNotification()
	{
		// This method is kept for compatibility but the queue system should be used.
		// It could clear the queue and hide the current notification.
		m_notificationQueue.Clear();
		_InternalShowNotification("", false, "");
		m_bIsDisplaying = false;
		GetGame().GetCallqueue().Remove(this.ProcessNotificationQueue);
		GetGame().GetCallqueue().Remove(this.HideCurrentAndProcessNext);
	}
}; 