//------------------------------------------------------------------------------------------------
//! Opens the paint bay for a helicopter pilot. The key lives in its own input
//! context, which this keeps alive only while the local player sits in a
//! helicopter's pilot seat, so the key does nothing on foot, as a passenger or
//! as co-pilot. Ticked by IA_NotificationDisplay, which owns it.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintHotkey
{
	static const string ACTION = "IA_HeliPaintMenu";
	static const string CONTEXT = "IA_HeliPilotContext";

	protected static const float SEAT_CHECK_S = 0.25;
	// Longer than a seat check, so the context never lapses between two of them.
	protected static const int CONTEXT_HOLD_MS = 500;
	protected static const int DEBOUNCE_MS = 250;
	protected static const int HINT_SECONDS = 9;

	// Once per session is enough to learn a key.
	protected static bool s_bHintShown;

	protected bool m_bListening;
	protected bool m_bPilot;
	protected float m_fSeatCheck;
	protected int m_iLastPressMs = -DEBOUNCE_MS;

	//------------------------------------------------------------------------------------------------
	//! The open paint bay keeps its own key alive, so the key that opened it closes it.
	//! A gamepad closes with Back: its paint bay input is a direction the menu uses.
	static void KeepAlive()
	{
		InputManager input = GetGame().GetInputManager();
		if (input && input.IsUsingMouseAndKeyboard())
			input.ActivateContext(CONTEXT, CONTEXT_HOLD_MS);
	}

	//------------------------------------------------------------------------------------------------
	void Tick(float dt)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (!m_bListening)
		{
			input.AddActionListener(ACTION, EActionTrigger.DOWN, OnHotkey);
			m_bListening = true;
		}

		m_fSeatCheck = m_fSeatCheck - dt;
		if (m_fSeatCheck > 0)
			return;
		m_fSeatCheck = SEAT_CHECK_S;

		IEntity vehicle = IA_HeliPaintService.GetPilotedHelicopter(SCR_PlayerController.GetLocalControlledEntity());
		bool pilot = vehicle != null;
		if (pilot && !m_bPilot)
			OnSeatTaken(vehicle);
		m_bPilot = pilot;

		// The open menu looks after the context itself.
		if (m_bPilot && !IA_HeliPaintMenu.IsBayOpen())
			input.ActivateContext(CONTEXT, CONTEXT_HOLD_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! The controlled entity changed; look at the seat again on the next tick.
	void Reset()
	{
		m_bPilot = false;
		m_fSeatCheck = 0;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		m_bPilot = false;
		if (!m_bListening)
			return;

		m_bListening = false;
		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener(ACTION, EActionTrigger.DOWN, OnHotkey);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSeatTaken(IEntity vehicle)
	{
		if (s_bHintShown)
			return;
		if (!IA_HeliSkinManagerComponent.GetInstance())
			return;
		if (IA_HeliSkinManagerComponent.GetVehicleChannel(vehicle) == IA_HeliPaintChannels.CHANNEL_NONE)
			return;

		s_bHintShown = true;
		SCR_HintManagerComponent.ShowCustomHint("Press I (gamepad: hold D-pad left) in the pilot seat to open the paint bay and change this helicopter's skin, on the ground or in flight.", "Paint bay", HINT_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHotkey(float value, EActionTrigger reason)
	{
		int now = System.GetTickCount();
		if (now - m_iLastPressMs < DEBOUNCE_MS)
			return;
		m_iLastPressMs = now;

		MenuManager menus = GetGame().GetMenuManager();
		if (!menus)
			return;

		if (IA_HeliPaintMenu.IsBayOpen())
		{
			menus.CloseMenuByPreset(ChimeraMenuPreset.IA_HeliPaintMenu);
			return;
		}

		if (menus.IsAnyMenuOpen() || menus.IsAnyDialogOpen())
			return;

		// The key is a letter: never while it is being typed into chat.
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && EditBoxWidget.Cast(workspace.GetFocusedWidget()))
			return;

		if (!IA_HeliPaintService.GetPilotedHelicopter(SCR_PlayerController.GetLocalControlledEntity()))
			return;

		menus.OpenMenu(ChimeraMenuPreset.IA_HeliPaintMenu);
	}
}
