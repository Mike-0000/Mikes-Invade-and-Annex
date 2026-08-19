//------------------------------------------------------------------------------------------------
//! HUD sector toasts plus a persistent capture readout. Mike's UI path uses
//! IA_NotificationToast and IA_CaptureHud (custom MUI_Node). Capture progress
//! never occupies the toast queue. Legacy layout remains if HUD mount fails.
//------------------------------------------------------------------------------------------------
class IA_NotificationInfo
{
	string m_sMessage;
	string m_sColor;
	int m_iDuration;
	IA_NotificationKind m_eKind;

	void IA_NotificationInfo(string message, string color, int duration, IA_NotificationKind kind)
	{
		m_sMessage = message;
		m_sColor = color;
		m_iDuration = duration;
		m_eKind = kind;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class IA_NotificationDisplay : SCR_InfoDisplayExtended
{
	protected static const bool USE_MIKES_UI = true;
	protected static const int QUEUE_GAP_MS = 380;

	protected RichTextWidget m_wInfoText;
	protected RichTextWidget m_RedText;
	protected RichTextWidget m_YellowText;
	protected Widget m_wNotificationOverlay;

	protected ref MUI_HudHost m_HudHost;
	protected MUI_Runtime m_Runtime;
	protected ref IA_NotificationToast m_Toast;
	protected ref IA_RankHudPanel m_RankHud;
	protected ref IA_CaptureHud m_CaptureHud;

	protected ref array<ref IA_NotificationInfo> m_notificationQueue = new array<ref IA_NotificationInfo>();
	protected bool m_bIsDisplaying = false;
	protected bool m_bSuppressFinish = false;

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
		if (m_Toast)
		{
			m_Toast.GetOnFinished().Remove(OnToastFinished);
			m_Toast.Abort();
		}
		if (m_CaptureHud)
			m_CaptureHud.Abort();
		IA_LocalOptions.Get().GetOnChanged().Remove(this.ApplyLocalOptions);
		if (m_RankHud)
		{
			m_RankHud.GetOnPromoted().Remove(this.OnLocalPromoted);
			m_RankHud.Unbind();
			m_RankHud = null;
		}
		if (m_HudHost)
		{
			m_HudHost.Close();
			m_HudHost = null;
		}
		m_Toast = null;
		m_CaptureHud = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildToastUI()
	{
		ref MUI_Panel overlay = m_Runtime.CreatePanel("overlay");
		overlay.MakePassThroughOverlay();
		overlay.SetPaddingTRBL(26, 24, 36, 24);

		m_Toast = IA_NotificationToast.Create(m_Runtime);
		m_Toast.GetOnFinished().Insert(OnToastFinished);

		m_CaptureHud = IA_CaptureHud.Create(m_Runtime);
		m_RankHud = IA_RankHudPanel.Create(m_Runtime);

		overlay.AddChild(m_Toast);
		overlay.AddChild(m_CaptureHud);
		overlay.AddChild(m_RankHud.GetRoot());
		m_Runtime.SetRoot(overlay);
		m_RankHud.GetOnPromoted().Insert(this.OnLocalPromoted);
		m_RankHud.Bind();
		IA_LocalOptions.Get().GetOnChanged().Insert(this.ApplyLocalOptions);
		ApplyLocalOptions();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyLocalOptions()
	{
		IA_LocalOptions options = IA_LocalOptions.Get();
		if (m_RankHud)
			m_RankHud.SetHudVisible(!options.HideRankHud());

		if (options.HidePromotionNotifications())
			DropQueuedPromotions();
	}

	//------------------------------------------------------------------------------------------------
	protected void DropQueuedPromotions()
	{
		int i = m_notificationQueue.Count() - 1;
		while (i >= 0)
		{
			IA_NotificationInfo info = m_notificationQueue[i];
			if (info && info.m_eKind == IA_NotificationKind.Promotion)
				m_notificationQueue.Remove(i);
			i = i - 1;
		}

		if (m_Toast && m_Toast.IsPlaying() && m_Toast.GetKind() == IA_NotificationKind.Promotion)
			m_Toast.Dismiss();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowHUD(bool show, string color)
	{
		if (m_Runtime && m_Toast)
		{
			if (!show && m_Toast.IsPlaying())
				m_Toast.Dismiss();
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
	void QueueNotification(string message, string color, int duration, bool show = true)
	{
		if (!show)
			return;

		m_notificationQueue.Insert(new IA_NotificationInfo(message, color, duration, InferKind(message, color)));

		if (!m_bIsDisplaying)
			ProcessNotificationQueue();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueNotificationKind(string message, string color, int duration, IA_NotificationKind kind)
	{
		if (kind == IA_NotificationKind.Promotion && IA_LocalOptions.Get().HidePromotionNotifications())
			return;

		m_notificationQueue.Insert(new IA_NotificationInfo(message, color, duration, kind));

		if (!m_bIsDisplaying)
			ProcessNotificationQueue();
	}

	//------------------------------------------------------------------------------------------------
	protected IA_NotificationKind InferKind(string message, string color)
	{
		if (message.IndexOf("New Side Objective") != -1)
			return IA_NotificationKind.SideTaskCreated;
		if (message.IndexOf("New Objective") != -1)
			return IA_NotificationKind.TaskCreated;
		if (message.IndexOf("Objective Completed") != -1)
			return IA_NotificationKind.TaskCompleted;
		if (message.IndexOf("Area Completed") != -1)
			return IA_NotificationKind.AreaCompleted;
		if (message.IndexOf("RTB") != -1)
			return IA_NotificationKind.AreaCompleted;
		if (color == "red")
			return IA_NotificationKind.Alert;
		if (color == "yellow")
			return IA_NotificationKind.Generic;
		if (color == "green")
			return IA_NotificationKind.Success;
		return IA_NotificationKind.Generic;
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessNotificationQueue()
	{
		if (m_bIsDisplaying || m_notificationQueue.IsEmpty())
			return;

		IA_NotificationInfo info;
		while (!m_notificationQueue.IsEmpty())
		{
			info = m_notificationQueue[0];
			m_notificationQueue.Remove(0);
			if (!info)
				continue;
			if (info.m_eKind == IA_NotificationKind.Promotion && IA_LocalOptions.Get().HidePromotionNotifications())
				continue;

			m_bIsDisplaying = true;
			_InternalShowNotification(info.m_sMessage, true, info.m_sColor, info.m_eKind, info.m_iDuration);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void _InternalShowNotification(string message, bool show, string color, IA_NotificationKind kind, int durationMs)
	{
		if (m_Runtime && m_Toast)
		{
			if (show)
				m_Toast.Present(message, color, durationMs, kind);
			else
				m_Toast.Dismiss();
			m_bIsEnabled = true;
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
			if (show)
				GetGame().GetCallqueue().CallLater(this.HideCurrentAndProcessNext, durationMs);
			return;
		}
		ShowHUD(show, color);
		m_bIsEnabled = show;
		if (show)
			GetGame().GetCallqueue().CallLater(this.HideCurrentAndProcessNext, durationMs);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnToastFinished()
	{
		if (m_bSuppressFinish)
			return;
		m_bIsDisplaying = false;
		GetGame().GetCallqueue().CallLater(this.ProcessNotificationQueue, QUEUE_GAP_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void HideCurrentAndProcessNext()
	{
		_InternalShowNotification("", false, "", IA_NotificationKind.Generic, 0);
		m_bIsDisplaying = false;
		GetGame().GetCallqueue().CallLater(this.ProcessNotificationQueue, 1000);
	}

	//------------------------------------------------------------------------------------------------
	bool IsCaptureHudActive()
	{
		return m_CaptureHud != null;
	}

	//------------------------------------------------------------------------------------------------
	void DisplayTaskCreatedNotification(string taskName)
	{
		QueueNotificationKind(taskName, "red", 5000, IA_NotificationKind.TaskCreated);
	}

	//------------------------------------------------------------------------------------------------
	void DisplaySideTaskCreatedNotification(string taskName)
	{
		QueueNotificationKind(taskName, "red", 5000, IA_NotificationKind.SideTaskCreated);
	}

	//------------------------------------------------------------------------------------------------
	void DisplayTaskCompletedNotification(string taskName)
	{
		QueueNotificationKind(taskName, "yellow", 9000, IA_NotificationKind.TaskCompleted);
	}

	//------------------------------------------------------------------------------------------------
	void DisplayAreaCompletedNotification(string taskName)
	{
		string body = "Return to base and await further tasking.";
		if (!taskName.IsEmpty() && taskName != "All objectives in current area")
			body = taskName;
		QueueNotificationKind(body, "", 12000, IA_NotificationKind.AreaCompleted);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLocalPromoted(int rankId)
	{
		if (IA_LocalOptions.Get().HidePromotionNotifications())
			return;

		string fullName = IA_SessionRankLadder.GetFullName(rankId);
		string shortName = IA_SessionRankLadder.GetShortName(rankId);
		QueueNotificationKind("Promoted to " + fullName + "  //  " + shortName, "green", 7000, IA_NotificationKind.Promotion);
	}

	//------------------------------------------------------------------------------------------------
	void HideNotification()
	{
		m_notificationQueue.Clear();
		m_bIsDisplaying = false;
		m_bSuppressFinish = true;
		if (m_Toast)
			m_Toast.Abort();
		else
			ShowHUD(false, "");
		m_bSuppressFinish = false;
		GetGame().GetCallqueue().Remove(this.ProcessNotificationQueue);
		GetGame().GetCallqueue().Remove(this.HideCurrentAndProcessNext);
	}
}
