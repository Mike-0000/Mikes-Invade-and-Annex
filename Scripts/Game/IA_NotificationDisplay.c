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
	protected static const bool USE_MIKES_UI = true;

	protected RichTextWidget m_wInfoText;
	protected RichTextWidget m_RedText;
	protected RichTextWidget m_YellowText;
	protected Widget m_wNotificationOverlay;

	protected ref MUI_HudHost m_HudHost;
	protected MUI_Runtime m_Runtime;
	protected ref MUI_Surface m_ToastSurface;
	protected ref MUI_Label m_ToastHeader;
	protected ref MUI_Label m_ToastMessage;

	protected ref array<ref IA_NotificationInfo> m_notificationQueue = new array<ref IA_NotificationInfo>();
	protected bool m_bIsDisplaying = false;

	//------------------------------------------------------------------------------------------------
	override void DisplayStartDraw(IEntity owner)
	{
		super.DisplayStartDraw(owner);

		m_wNotificationOverlay = m_wRoot.FindAnyWidget("ia_root");
		m_wInfoText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText0"));
		m_RedText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText1"));
		m_YellowText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("RichText3"));

		if (USE_MIKES_UI)
		{
			if (OpenMikesUI())
			{
				ShowHUD(false, "");
				// Keep enabled so DisplayUpdate ticks MUI every frame (intro / FX).
				m_bIsEnabled = true;
				return;
			}
			Print("[IA_NotificationDisplay] Mike's UI mount failed, using legacy HUD", LogLevel.WARNING);
		}

		ShowHUD(false, "");
		m_bIsEnabled = false;
	}

	//------------------------------------------------------------------------------------------------
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		super.DisplayUpdate(owner, timeSlice);
		if (m_HudHost)
			m_HudHost.Tick(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override void DisplayStopDraw(IEntity owner)
	{
		CloseMikesUI();
		super.DisplayStopDraw(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool OpenMikesUI()
	{
		if (!m_wRoot)
			return false;

		m_HudHost = new MUI_HudHost();
		if (!m_HudHost.Open(m_wRoot, "IA_NotificationDisplay"))
		{
			m_HudHost = null;
			return false;
		}

		m_Runtime = m_HudHost.GetRuntime();
		BuildToastUI();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseMikesUI()
	{
		m_Runtime = null;
		if (m_HudHost)
		{
			m_HudHost.Close();
			m_HudHost = null;
		}
		m_ToastSurface = null;
		m_ToastHeader = null;
		m_ToastMessage = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildToastUI()
	{
		MUI_ThemeData theme = m_Runtime.GetTheme();

		ref MUI_Panel overlay = m_Runtime.CreatePanel("overlay");
		overlay.MakePassThroughOverlay();
		overlay.GetStyle().m_fPadT = 72;
		overlay.SetIntro(0, 0.01, 0);

		m_ToastSurface = m_Runtime.CreateCard("toast");
		m_ToastSurface.SetWidth(780);
		m_ToastSurface.SetHugHeight();
		m_ToastSurface.SetPaddingTRBL(16, 22, 24, 22);
		m_ToastSurface.SetGap(8);
		m_ToastSurface.SetAlign(0.5, 0.0);
		m_ToastSurface.GetStyle().m_bBlockHit = false;
		m_ToastSurface.SetVisible(false);

		m_ToastHeader = m_Runtime.CreateLabel("SECTOR ALERT", "toastHeader");
		m_ToastHeader.SetFontSize(theme.FONT_SMALL);
		m_ToastHeader.SetBold(true);

		m_ToastMessage = m_Runtime.CreateLabel("", "message");
		m_ToastMessage.SetFontSize(theme.FONT_BODY);
		m_ToastMessage.SetBold(true);

		m_ToastSurface.AddChild(m_ToastHeader);
		m_ToastSurface.AddChild(m_ToastMessage);

		overlay.AddChild(m_ToastSurface);
		m_Runtime.SetRoot(overlay);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowHUD(bool show, string color)
	{
		if (m_Runtime && m_ToastSurface)
		{
			if (!show)
			{
				m_ToastSurface.SetVisible(false);
				return;
			}

			Color textColor = ResolveToastColor(color);
			if (m_ToastHeader)
				m_ToastHeader.SetColor(textColor);
			if (m_ToastMessage)
				m_ToastMessage.SetColor(textColor);

			m_ToastSurface.SetVisible(true);
			m_ToastSurface.SetIntro(0, 0.4, -36);
			if (m_ToastHeader)
				m_ToastHeader.SetIntro(0.05, 0.35, -18);
			if (m_ToastMessage)
				m_ToastMessage.SetIntro(0.1, 0.35, -14);
			return;
		}

		if (!show)
		{
			if (m_wNotificationOverlay)
				m_wNotificationOverlay.SetVisible(show);
			if (m_RedText)
				m_RedText.SetVisible(show);
			if (m_wInfoText)
				m_wInfoText.SetVisible(show);
			if (m_YellowText)
				m_YellowText.SetVisible(show);
			return;
		}

		if (m_wNotificationOverlay)
			m_wNotificationOverlay.SetVisible(show);
		if (m_wInfoText && color == "")
			m_wInfoText.SetVisible(show);
		else if (m_RedText && color == "red")
			m_RedText.SetVisible(show);
		else if (m_YellowText && color == "yellow")
			m_YellowText.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveToastColor(string color)
	{
		ref MUI_ThemeData theme;
		if (m_Runtime)
			theme = m_Runtime.GetTheme();
		else
			theme = MUI_ThemeData.CreateUplink();

		if (color == "red")
			return theme.Danger;
		if (color == "yellow")
			return theme.Accent;
		return theme.Live;
	}

	//------------------------------------------------------------------------------------------------
	void QueueNotification(string message, string color, int duration, bool show = true)
	{
		if (!show)
			return;

		m_notificationQueue.Insert(new IA_NotificationInfo(message, color, duration));

		if (!m_bIsDisplaying)
			ProcessNotificationQueue();
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessNotificationQueue()
	{
		if (m_bIsDisplaying || m_notificationQueue.IsEmpty())
			return;

		m_bIsDisplaying = true;

		IA_NotificationInfo info = m_notificationQueue[0];
		m_notificationQueue.Remove(0);

		_InternalShowNotification(info.m_sMessage, true, info.m_sColor);

		GetGame().GetCallqueue().CallLater(this.HideCurrentAndProcessNext, info.m_iDuration);
	}

	//------------------------------------------------------------------------------------------------
	protected void _InternalShowNotification(string message, bool show, string color)
	{
		if (m_Runtime && m_ToastMessage)
		{
			if (show)
				m_ToastMessage.SetText(message);
			else
				m_ToastMessage.SetText("");
			ShowHUD(show, color);
			return;
		}

		if (color == "red")
			m_RedText.SetText(message);
		else if (color == "")
			m_wInfoText.SetText(message);
		else if (color == "yellow")
			m_YellowText.SetText(message);
		else
		{
			m_YellowText.SetText(message);
			ShowHUD(show, "yellow");
			m_bIsEnabled = show;
			return;
		}
		ShowHUD(show, color);
		m_bIsEnabled = show;
	}

	//------------------------------------------------------------------------------------------------
	protected void HideCurrentAndProcessNext()
	{
		_InternalShowNotification("", false, "");
		m_bIsDisplaying = false;
		GetGame().GetCallqueue().CallLater(this.ProcessNotificationQueue, 1000);
	}

	//------------------------------------------------------------------------------------------------
	void DisplayTaskCreatedNotification(string taskName)
	{
		QueueNotification("New Objective: " + taskName, "red", 5000);
	}

	//------------------------------------------------------------------------------------------------
	void DisplaySideTaskCreatedNotification(string taskName)
	{
		QueueNotification("New Side Objective: " + taskName, "red", 5000);
	}

	//------------------------------------------------------------------------------------------------
	void DisplayTaskCompletedNotification(string taskName)
	{
		QueueNotification("Objective Completed: " + taskName, "yellow", 9000);
	}

	//------------------------------------------------------------------------------------------------
	void DisplayAreaCompletedNotification(string taskName)
	{
		QueueNotification("Objective Area Completed, RTB and await tasking.", "", 12000);
	}

	//------------------------------------------------------------------------------------------------
	void HideNotification()
	{
		m_notificationQueue.Clear();
		_InternalShowNotification("", false, "");
		m_bIsDisplaying = false;
		GetGame().GetCallqueue().Remove(this.ProcessNotificationQueue);
		GetGame().GetCallqueue().Remove(this.HideCurrentAndProcessNext);
	}
}
