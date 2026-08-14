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

	protected Widget m_wHost;
	protected ref MUI_Runtime m_Runtime;
	protected ref MUI_Card m_ToastCard;
	protected ref MUI_Label m_ToastKicker;
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
		if (m_Runtime)
			m_Runtime.Tick(timeSlice);
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

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_wHost = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 50, m_wRoot);
		if (!m_wHost)
			return false;

		FrameSlot.SetAnchorMin(m_wHost, 0, 0);
		FrameSlot.SetAnchorMax(m_wHost, 1, 1);
		FrameSlot.SetOffsets(m_wHost, 0, 0, 0, 0);
		// HUD manager sets IGNORE_CURSOR on layout roots; child canvases must too or they eat all clicks.
		m_wHost.SetFlags(WidgetFlags.IGNORE_CURSOR);
		if (m_wRoot)
			m_wRoot.SetFlags(WidgetFlags.IGNORE_CURSOR);

		m_Runtime = new MUI_Runtime();
		if (!m_Runtime.MountPassive(m_wHost))
		{
			m_Runtime.Unmount();
			m_Runtime = null;
			m_wHost.RemoveFromHierarchy();
			m_wHost = null;
			return false;
		}

		HideLegacyChildren();
		BuildToastUI();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseMikesUI()
	{
		if (m_Runtime)
		{
			m_Runtime.Unmount();
			m_Runtime = null;
		}
		if (m_wHost)
		{
			m_wHost.RemoveFromHierarchy();
			m_wHost = null;
		}
		m_ToastCard = null;
		m_ToastKicker = null;
		m_ToastMessage = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void HideLegacyChildren()
	{
		if (!m_wRoot)
			return;

		Widget child = m_wRoot.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			if (child != m_wHost)
				child.SetVisible(false);
			child = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildToastUI()
	{
		ref MUI_Panel overlay = m_Runtime.CreatePanel("overlay");
		overlay.MakePassThroughOverlay();
		overlay.GetStyle().m_fPadT = 72;
		overlay.SetIntro(0, 0.01, 0);

		m_ToastCard = m_Runtime.CreateCard("toast");
		m_ToastCard.SetWidth(720);
		m_ToastCard.SetPadding(20);
		m_ToastCard.GetStyle().m_fPadT = 16;
		m_ToastCard.GetStyle().m_fPadB = 16;
		m_ToastCard.SetGap(8);
		m_ToastCard.SetAlign(0.5, 0.0);
		m_ToastCard.GetStyle().m_bBlockHit = false;
		m_ToastCard.SetVisible(false);

		m_ToastKicker = m_Runtime.CreateLabel("SECTOR ALERT", "kicker");
		m_ToastKicker.SetFontSize(MUI_Theme.FONT_SMALL);
		m_ToastKicker.SetMuted(true);

		m_ToastMessage = m_Runtime.CreateLabel("", "message");
		m_ToastMessage.SetFontSize(MUI_Theme.FONT_BODY);
		m_ToastMessage.SetBold(true);

		m_ToastCard.AddChild(m_ToastKicker);
		m_ToastCard.AddChild(m_ToastMessage);

		overlay.AddChild(m_ToastCard);
		m_Runtime.SetRoot(overlay);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowHUD(bool show, string color)
	{
		if (m_Runtime && m_ToastCard)
		{
			if (!show)
			{
				m_ToastCard.SetVisible(false);
				return;
			}

			Color textColor = ResolveToastColor(color);
			if (m_ToastMessage)
				m_ToastMessage.SetColor(textColor);
			if (m_ToastKicker)
				m_ToastKicker.SetColor(textColor);

			m_ToastCard.SetVisible(true);
			m_ToastCard.SetIntro(0, 0.4, -36);
			if (m_ToastKicker)
				m_ToastKicker.SetIntro(0.05, 0.35, -18);
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
		if (color == "red")
			return MUI_Theme.Danger;
		if (color == "yellow")
			return MUI_Theme.Accent;
		return MUI_Theme.Live;
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
};
