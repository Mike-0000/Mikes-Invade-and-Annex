//------------------------------------------------------------------------------------------------
//! The paint bay menu: opened by the helicopter pilot from the seat (see
//! IA_HeliPaintHotkey), it lets them change the skin of the airframe they are
//! flying. The screen is left clear so the repaint is seen on the helicopter.
//!
//! The menu asks; the server decides. It names a skin through the player
//! controller and shows what comes back. The worn skin is read from the
//! replicated skin manager, so the menu follows the airframe, not its own guess.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintMenu : MUI_MenuBase
{
	protected static const float POLL_S = 0.1;
	protected static const float ASK_STATE_S = 4.0;
	protected static const float PENDING_TIMEOUT_S = 3.0;
	protected static const float BOTTOM_MARGIN = 66;

	protected static IA_HeliPaintMenu s_Instance;
	// The server's last answer, kept between openings so the bay does not open blank.
	protected static int s_iRating = IA_TransportPilotRecord.RATING_UNKNOWN;
	protected static bool s_bAdmin;

	protected ref IA_HeliPaintBay m_Bay;
	protected float m_fPoll;
	protected float m_fAskState;
	protected int m_iPending = IA_HeliPaintBay.NO_SKIN;
	protected float m_fPending;
	protected bool m_bApplied;

#ifdef WORKBENCH
	// A Workbench probe shows the bay in a world with no game mode and no pilot.
	protected static bool s_bProbe;
	protected static bool s_bProbePaintable;
	protected static int s_iProbeChannel;
	protected static int s_iProbeWorn;
#endif

	//------------------------------------------------------------------------------------------------
	static bool IsBayOpen()
	{
		return s_Instance != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to a paint bay request, from the player controller.
	static void OnServerReply(int result, int rating, bool admin, int skinId, string thresholds)
	{
		IA_HeliSkinCatalog.ApplyPackedThresholds(thresholds);
		s_iRating = rating;
		s_bAdmin = admin;
		if (s_Instance)
			s_Instance.HandleReply(result, skinId);
	}

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_HeliPaintMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void OnMUIMountFailed()
	{
		Print("[IA][HeliPaint] Paint bay could not mount Mike's UI.", LogLevel.ERROR);
		// Nothing was built, so nothing is bound to Back: leave once the open has finished.
		GetGame().GetCallqueue().CallLater(this.CloseSelf, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseSelf()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		ref MUI_Panel root = runtime.CreatePanel("root");
		root.MakeOverlay();
		root.SetFill(Color.FromInt(0));
		root.SetPaddingTRBL(0, 0, BOTTOM_MARGIN, 0);

		m_Bay = IA_HeliPaintBay.Create(runtime);
		m_Bay.GetOnPick().Insert(OnPick);
		root.AddChild(m_Bay);
		runtime.SetRoot(root);
		runtime.SetPromptText("<action name='MenuSelect' scale='1.35'/>  Repaint", "<action name='MenuBack' scale='1.35'/>  Close");
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (!m_Bay)
			return;

		s_Instance = this;
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_HOTKEY_OPEN);
		Refresh();
		AskState();

		// A gamepad starts on the livery being flown.
		MUI_Runtime runtime = GetRuntime();
		InputManager input = GetGame().GetInputManager();
		if (runtime && input && !input.IsUsingMouseAndKeyboard())
			runtime.FocusNode(m_Bay.FindTile(ReadWorn()));
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_Instance == this)
		{
			s_Instance = null;
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_HOTKEY_CLOSE);
		}
		if (m_Bay)
			m_Bay.GetOnPick().Remove(OnPick);
		m_Bay = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (!m_Bay)
			return;

		IA_HeliPaintHotkey.KeepAlive();

		if (m_iPending != IA_HeliPaintBay.NO_SKIN)
		{
			m_fPending = m_fPending - tDelta;
			if (m_fPending <= 0)
			{
				// An accepted repaint that has not shown yet is replication running late, not a failure.
				bool applied = m_bApplied;
				ClearPending();
				if (!applied)
					m_Bay.ShowMessage("NO ANSWER FROM THE PAINT RIG - TRY AGAIN", true);
			}
		}

		m_fAskState = m_fAskState - tDelta;
		if (m_fAskState <= 0)
			AskState();

		m_fPoll = m_fPoll - tDelta;
		if (m_fPoll > 0)
			return;
		m_fPoll = POLL_S;
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \return the helicopter the local player pilots, null when they left the seat
	protected IEntity FindVehicle()
	{
		return IA_HeliPaintService.GetPilotedHelicopter(SCR_PlayerController.GetLocalControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	protected int ReadWorn()
	{
#ifdef WORKBENCH
		if (s_bProbe)
			return s_iProbeWorn;
#endif
		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		if (!skins)
			return IA_HeliSkinCatalog.SKIN_NONE;
		return skins.GetVehicleSkin(FindVehicle());
	}

	//------------------------------------------------------------------------------------------------
	//! Bring the bay in line with the airframe; closes the menu when the pilot left the seat.
	protected void Refresh()
	{
#ifdef WORKBENCH
		if (s_bProbe)
		{
			if (m_iPending != IA_HeliPaintBay.NO_SKIN && s_iProbeWorn == m_iPending)
				ClearPending();
			m_Bay.SetContext(s_bProbePaintable, s_iProbeChannel, s_iProbeWorn, s_iRating, s_bAdmin);
			return;
		}
#endif

		IEntity vehicle = FindVehicle();
		if (!vehicle)
		{
			Close();
			return;
		}

		int channel = IA_HeliSkinManagerComponent.GetVehicleChannel(vehicle);
		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		bool paintable = skins && channel != IA_HeliPaintChannels.CHANNEL_NONE;
		int worn = IA_HeliSkinCatalog.SKIN_NONE;
		if (paintable)
			worn = skins.GetVehicleSkin(vehicle);

		if (m_iPending != IA_HeliPaintBay.NO_SKIN && worn == m_iPending)
			ClearPending();
		m_Bay.SetContext(paintable, channel, worn, s_iRating, s_bAdmin);
	}

	//------------------------------------------------------------------------------------------------
	protected void AskState()
	{
		m_fAskState = ASK_STATE_S;
#ifdef WORKBENCH
		if (s_bProbe)
			return;
#endif
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.IA_AskHeliPaintState();
	}

	//------------------------------------------------------------------------------------------------
	//! The pilot picked a livery the bay believes they may wear.
	protected void OnPick(int skinId)
	{
		if (m_iPending != IA_HeliPaintBay.NO_SKIN || !m_Bay)
			return;

		m_iPending = skinId;
		m_fPending = PENDING_TIMEOUT_S;
		m_bApplied = false;
		m_Bay.SetPending(skinId);

#ifdef WORKBENCH
		if (s_bProbe)
			return;
#endif
		// A hosting machine answers inside this call.
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.IA_AskSetHeliSkin(skinId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPending()
	{
		m_iPending = IA_HeliPaintBay.NO_SKIN;
		m_bApplied = false;
		if (m_Bay)
			m_Bay.SetPending(IA_HeliPaintBay.NO_SKIN);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleReply(int result, int skinId)
	{
		if (!m_Bay)
			return;

		m_Bay.Reload();
		Refresh();
		if (!m_Bay || result == IA_HeliPaintService.RESULT_STATE)
			return;

		if (result == IA_HeliPaintService.RESULT_APPLIED)
		{
			// The bay celebrates when the airframe's skin arrives by replication.
			if (m_iPending == skinId)
				m_bApplied = true;
			return;
		}

		if (m_iPending == skinId)
			ClearPending();
		m_Bay.Refuse(skinId, RefusalText(result));
	}

	//------------------------------------------------------------------------------------------------
	protected string RefusalText(int result)
	{
		if (result == IA_HeliPaintService.RESULT_NOT_PILOT)
			return "ONLY THE PILOT CAN REPAINT THE AIRFRAME";
		if (result == IA_HeliPaintService.RESULT_NO_CHANNEL)
			return "THIS AIRFRAME HAS NO PAINT RIG";
		if (result == IA_HeliPaintService.RESULT_LOCKED)
			return "COMMAND SAYS THAT LIVERY IS NOT EARNED YET";
		if (result == IA_HeliPaintService.RESULT_SYNCING)
			return "RATING STILL SYNCING - TRY AGAIN IN A MOMENT";
		return "PAINT RIG UNAVAILABLE";
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: feed the bay a state in place of a seat, a skin manager and a server.
	static void ProbeSet(bool paintable, int channel, int worn, int rating, bool admin)
	{
		s_bProbe = true;
		s_bProbePaintable = paintable;
		s_iProbeChannel = channel;
		s_iProbeWorn = worn;
		s_iRating = rating;
		s_bAdmin = admin;
		if (s_Instance && s_Instance.m_Bay)
			s_Instance.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	static void ProbeEnd()
	{
		s_bProbe = false;
		s_iRating = IA_TransportPilotRecord.RATING_UNKNOWN;
		s_bAdmin = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: point at a livery as a gamepad would. \return false when the bay does not list it
	static bool ProbeFocus(int skinId)
	{
		if (!s_Instance || !s_Instance.m_Bay)
			return false;

		IA_HeliPaintTile tile = s_Instance.m_Bay.FindTile(skinId);
		MUI_Runtime runtime = s_Instance.GetRuntime();
		if (!tile || !runtime)
			return false;
		runtime.FocusNode(tile);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: click a livery. \return the skin now waiting for an answer, IA_HeliPaintBay.NO_SKIN for none
	static int ProbePick(int skinId)
	{
		if (!s_Instance || !s_Instance.m_Bay)
			return IA_HeliPaintBay.NO_SKIN;

		IA_HeliPaintTile tile = s_Instance.m_Bay.FindTile(skinId);
		if (tile)
			s_Instance.m_Bay.OnTilePicked(tile);
		return s_Instance.m_iPending;
	}
#endif
}
