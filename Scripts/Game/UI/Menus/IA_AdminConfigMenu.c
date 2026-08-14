//------------------------------------------------------------------------------------------------
class IA_AdminConfigMenu : ChimeraMenuBase
{
	protected static const bool USE_MIKES_UI = true;

	protected Widget m_wRoot;
	protected ref IA_MikesMenuHost m_Host;
	protected ref MUI_TextField m_civField;
	protected ref MUI_TextField m_AIField;
	protected ref MUI_TextField m_artyField;
	protected ref MUI_Toggle m_heliToggle;
	protected ref MUI_Toggle m_groundToggle;

	protected EditBoxWidget m_civCountEdit;
	protected EditBoxWidget m_AIScaleEdit;
	protected CheckBoxWidget m_disableHQHeliCheck;
	protected CheckBoxWidget m_disableHQGroundCheck;
	protected EditBoxWidget m_artyCooldownEdit;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		m_wRoot = GetRootWidget();
		if (!m_wRoot)
			return;

		if (USE_MIKES_UI)
			OpenMikesUI();
		else
			OpenLegacy();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (!USE_MIKES_UI)
			return;
		if (m_Host)
			m_Host.Tick(tDelta);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuFocusLost()
	{
		super.OnMenuFocusLost();
		if (m_Host)
			m_Host.Blur();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (m_Host)
		{
			m_Host.Close();
			m_Host = null;
		}
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMikesUI()
	{
		m_Host = new IA_MikesMenuHost();
		if (!m_Host.Open(m_wRoot, "IA_AdminConfigMenu"))
		{
			m_Host = null;
			OpenLegacy();
			return;
		}

		BuildAdminUI();
		PopulateFromConfig();
		m_Host.GetRuntime().GetOnBack().Insert(OnMikesClose);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildAdminUI()
	{
		MUI_Runtime runtime = m_Host.GetRuntime();

		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.GetStyle().m_Fill = Color.FromInt(0);
		overlay.SetIntro(0, 0.35, 0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");
		fx.SetIntro(0, 0.55, 0);

		ref MUI_Card card = runtime.CreateCard("card");
		card.SetWidth(600);
		card.SetPadding(28);
		card.GetStyle().m_fPadT = 22;
		card.SetGap(12);
		card.SetAlign(0.5, 0.5);
		card.SetIntro(0.06, 0.55, 46);

		ref MUI_LiveHeader header = runtime.CreateLiveHeader("ADMIN CONFIG", "header");
		header.SetKicker("COMMAND UPLINK  //  SECTOR IA");
		header.SetIntro(0.16, 0.4, 18);

		ref MUI_Label subtitle = runtime.CreateLabel("MIKE'S UI   •   CANVAS COMPOSITOR   •   EXPERIMENTAL BUILD", "subtitle");
		subtitle.SetFontSize(MUI_Theme.FONT_SMALL);
		subtitle.SetMuted(true);
		subtitle.SetIntro(0.22, 0.4, 14);

		ref MUI_Hairline lineA = runtime.CreateHairline("lineA");
		lineA.SetIntro(0.26, 0.35, 8);

		ref MUI_ScrollView scroll = runtime.CreateScrollView("scroll");
		scroll.SetMaxViewportHeight(420);
		scroll.SetGap(12);
		scroll.SetIntro(0.28, 0.4, 16);

		m_civField = runtime.CreateTextField("Civilian count multiplier", "civ");
		m_civField.SetIntro(0.32, 0.4, 16);
		m_AIField = runtime.CreateTextField("AI scale multiplier", "ai");
		m_AIField.SetIntro(0.36, 0.4, 16);
		m_artyField = runtime.CreateTextField("Artillery cooldown (seconds)", "arty");
		m_artyField.SetIntro(0.40, 0.4, 16);
		m_heliToggle = runtime.CreateToggle("Disable HQ helipads", "heli");
		m_heliToggle.SetIntro(0.44, 0.4, 14);
		m_groundToggle = runtime.CreateToggle("Disable HQ ground vehicles", "ground");
		m_groundToggle.SetIntro(0.48, 0.4, 14);

		scroll.AddChild(m_civField);
		scroll.AddChild(m_AIField);
		scroll.AddChild(m_artyField);
		scroll.AddChild(m_heliToggle);
		scroll.AddChild(m_groundToggle);

		ref MUI_Hairline lineB = runtime.CreateHairline("lineB");
		lineB.SetIntro(0.50, 0.35, 8);

		ref MUI_Row buttons = runtime.CreateRow("buttons");
		buttons.SetGap(12);
		buttons.SetIntro(0.52, 0.4, 18);

		ref MUI_Button saveBtn = runtime.CreateButton("Save", "save");
		saveBtn.MakeAccent();
		saveBtn.SetIntro(0.54, 0.4, 14);
		saveBtn.GetOnClicked().Insert(OnMikesSave);

		ref MUI_Button completeBtn = runtime.CreateButton("Complete Zone", "complete");
		completeBtn.MakeDanger();
		completeBtn.SetIntro(0.58, 0.4, 14);
		completeBtn.GetOnClicked().Insert(OnMikesComplete);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.SetIntro(0.62, 0.4, 14);
		closeBtn.GetOnClicked().Insert(OnMikesClose);

		buttons.AddChild(saveBtn);
		buttons.AddChild(completeBtn);
		buttons.AddChild(closeBtn);

		ref MUI_Label foot = runtime.CreateLabel("RENDER GRAPH  •  TEXT POOL  •  EDIT BRIDGE  •  ANIM CLOCK", "foot");
		foot.SetFontSize(MUI_Theme.FONT_SMALL);
		foot.SetMuted(true);
		foot.SetIntro(0.66, 0.4, 10);

		card.AddChild(header);
		card.AddChild(subtitle);
		card.AddChild(lineA);
		card.AddChild(scroll);
		card.AddChild(lineB);
		card.AddChild(buttons);
		card.AddChild(foot);

		overlay.AddChild(fx);
		overlay.AddChild(card);
		runtime.SetRoot(overlay);
	}

	//------------------------------------------------------------------------------------------------
	protected void PopulateFromConfig()
	{
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit)
			return;
		if (!missionInit.GetConfig())
			return;

		if (m_civField)
			m_civField.SetText(missionInit.GetConfig().m_fCivilianCountMultiplier.ToString());
		if (m_AIField)
			m_AIField.SetText(missionInit.GetConfig().m_fAIScaleMultiplier.ToString());
		if (m_artyField)
			m_artyField.SetText(missionInit.GetConfig().m_iArtilleryCooldown.ToString());
		if (m_heliToggle)
			m_heliToggle.SetChecked(missionInit.GetConfig().m_bDisableHQHelipads);
		if (m_groundToggle)
			m_groundToggle.SetChecked(missionInit.GetConfig().m_bDisableHQGroundVehicles);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesSave()
	{
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit)
		{
			Print("[IA_AdminConfigMenu] ERROR: IA_MissionInitializer instance not found!", LogLevel.ERROR);
			return;
		}

		float civCount = 1.0;
		float aiScale = 1.0;
		int artyCooldown = 300;
		bool disableHeli = false;
		bool disableGround = false;

		if (m_civField)
		{
			string s = m_civField.GetText();
			if (!s.IsEmpty())
				civCount = s.ToFloat();
		}
		if (m_AIField)
		{
			string s = m_AIField.GetText();
			if (!s.IsEmpty())
				aiScale = s.ToFloat();
		}
		if (m_artyField)
		{
			string s = m_artyField.GetText();
			if (!s.IsEmpty())
				artyCooldown = s.ToInt();
		}
		if (m_heliToggle)
			disableHeli = m_heliToggle.IsChecked();
		if (m_groundToggle)
			disableGround = m_groundToggle.IsChecked();

		Print(string.Format("[IA_AdminConfigMenu] Saving Config: Civ=%1, AI=%2, Heli=%3, Ground=%4, Arty=%5", civCount, aiScale, disableHeli, disableGround, artyCooldown), LogLevel.NORMAL);
		missionInit.UpdateConfig(civCount, aiScale, disableHeli, disableGround, artyCooldown);
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesComplete()
	{
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit)
			return;
		missionInit.ForceCompleteZone();
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesClose()
	{
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenLegacy()
	{
		Widget w;

		w = m_wRoot.FindAnyWidget("IACivCountEdit");
		if (w)
			m_civCountEdit = EditBoxWidget.Cast(w);
		else
			Print("[IA_AdminConfigMenu] ERROR: IACivCountEdit not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IAAIScaleEdit");
		if (w)
			m_AIScaleEdit = EditBoxWidget.Cast(w);
		else
			Print("[IA_AdminConfigMenu] ERROR: IAAIScaleEdit not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IADisableHQHeliCheck");
		if (w)
			m_disableHQHeliCheck = CheckBoxWidget.Cast(w);
		else
			Print("[IA_AdminConfigMenu] ERROR: IADisableHQHeliCheck not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IADisableHQGroundCheck");
		if (w)
			m_disableHQGroundCheck = CheckBoxWidget.Cast(w);
		else
			Print("[IA_AdminConfigMenu] ERROR: IADisableHQGroundCheck not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IAArtyCooldownEdit");
		if (w)
			m_artyCooldownEdit = EditBoxWidget.Cast(w);
		else
			Print("[IA_AdminConfigMenu] ERROR: IAArtyCooldownEdit not found!", LogLevel.ERROR);

		Widget closeW = m_wRoot.FindAnyWidget("IACloseButton");
		if (closeW)
		{
			SCR_InputButtonComponent closeButton = SCR_InputButtonComponent.Cast(closeW.FindHandler(SCR_InputButtonComponent));
			if (closeButton)
				closeButton.m_OnActivated.Insert(OnIAMenuClose);
		}
		else
			Print("[IA_AdminConfigMenu] ERROR: IACloseButton not found!", LogLevel.ERROR);

		Widget saveW = m_wRoot.FindAnyWidget("IASaveButton");
		if (saveW)
		{
			SCR_InputButtonComponent saveButton = SCR_InputButtonComponent.Cast(saveW.FindHandler(SCR_InputButtonComponent));
			if (saveButton)
				saveButton.m_OnActivated.Insert(OnIASaveConfig);
		}
		else
			Print("[IA_AdminConfigMenu] ERROR: IASaveButton not found!", LogLevel.ERROR);

		Widget completeW = m_wRoot.FindAnyWidget("IACompleteMissionButton");
		if (completeW)
		{
			SCR_InputButtonComponent completeButton = SCR_InputButtonComponent.Cast(completeW.FindHandler(SCR_InputButtonComponent));
			if (completeButton)
				completeButton.m_OnActivated.Insert(OnIAForceCompleteMission);
		}
		else
			Print("[IA_AdminConfigMenu] ERROR: IACompleteMissionButton not found!", LogLevel.ERROR);

		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (missionInit && missionInit.GetConfig())
		{
			if (m_civCountEdit)
				m_civCountEdit.SetText(missionInit.GetConfig().m_fCivilianCountMultiplier.ToString());
			if (m_AIScaleEdit)
				m_AIScaleEdit.SetText(missionInit.GetConfig().m_fAIScaleMultiplier.ToString());
			if (m_disableHQHeliCheck)
				m_disableHQHeliCheck.SetChecked(missionInit.GetConfig().m_bDisableHQHelipads);
			if (m_disableHQGroundCheck)
				m_disableHQGroundCheck.SetChecked(missionInit.GetConfig().m_bDisableHQGroundVehicles);
			if (m_artyCooldownEdit)
				m_artyCooldownEdit.SetText(missionInit.GetConfig().m_iArtilleryCooldown.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnIAForceCompleteMission(SCR_InputButtonComponent comp)
	{
		Print("[IA_AdminConfigMenu] OnIAForceCompleteMission clicked", LogLevel.NORMAL);
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit)
			return;

		missionInit.ForceCompleteZone();
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	void OnIASaveConfig(SCR_InputButtonComponent comp)
	{
		Print("[IA_AdminConfigMenu] OnIASaveConfig clicked", LogLevel.NORMAL);
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit)
		{
			Print("[IA_AdminConfigMenu] ERROR: IA_MissionInitializer instance not found!", LogLevel.ERROR);
			return;
		}

		float civCount = 1.0;
		if (m_civCountEdit)
		{
			string s = m_civCountEdit.GetText();
			if (!s.IsEmpty())
				civCount = s.ToFloat();
		}

		float aiScale = 1.0;
		if (m_AIScaleEdit)
		{
			string s = m_AIScaleEdit.GetText();
			if (!s.IsEmpty())
				aiScale = s.ToFloat();
		}

		bool disableHeli = false;
		if (m_disableHQHeliCheck)
			disableHeli = m_disableHQHeliCheck.IsChecked();

		bool disableGround = false;
		if (m_disableHQGroundCheck)
			disableGround = m_disableHQGroundCheck.IsChecked();

		int artyCooldown = 300;
		if (m_artyCooldownEdit)
		{
			string s = m_artyCooldownEdit.GetText();
			if (!s.IsEmpty())
				artyCooldown = s.ToInt();
		}

		Print(string.Format("[IA_AdminConfigMenu] Saving Config: Civ=%1, AI=%2, Heli=%3, Ground=%4, Arty=%5", civCount, aiScale, disableHeli, disableGround, artyCooldown), LogLevel.NORMAL);
		missionInit.UpdateConfig(civCount, aiScale, disableHeli, disableGround, artyCooldown);
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	void OnIAMenuClose(SCR_InputButtonComponent comp)
	{
		GetGame().GetMenuManager().CloseMenu(this);
	}
};
