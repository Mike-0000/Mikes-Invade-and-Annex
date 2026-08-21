//------------------------------------------------------------------------------------------------
//! Runtime admin tuning for IA_Config. Uses MUI blank layout (no legacy widget path).
//------------------------------------------------------------------------------------------------
class IA_AdminConfigMenu : MUI_MenuBase
{
	protected ref MUI_Tabs m_Tabs;
	protected ref MUI_Panel m_PageScaling;
	protected ref MUI_Panel m_PageCiv;
	protected ref MUI_Panel m_PageArty;
	protected ref MUI_Panel m_PageHq;

	protected ref MUI_NumericField m_AIField;
	protected ref MUI_NumericField m_StaticAIField;
	protected ref MUI_NumericField m_MilVehField;

	protected ref MUI_NumericField m_civField;
	protected ref MUI_NumericField m_CivVehField;
	protected ref MUI_NumericField m_RevoltField;
	protected ref MUI_Toggle m_CivSpawnToggle;

	protected ref MUI_NumericField m_artyField;
	protected ref MUI_NumericField m_ArtyMinField;
	protected ref MUI_NumericField m_ArtyMaxField;
	protected ref MUI_Slider m_ArtyChanceSlider;
	protected ref MUI_Label m_ArtyChanceLabel;
	protected ref MUI_Progress m_ArtyChanceProgress;

	protected ref MUI_Toggle m_heliToggle;
	protected ref MUI_Toggle m_groundToggle;
	protected ref MUI_Toggle m_RolesToggle;
	protected ref MUI_NumericField m_HaloMaxField;
	protected ref MUI_Dropdown m_FactionDrop;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (IsMUIOpen())
			PopulateFromConfig();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMUIMountFailed()
	{
		Print("[IA_AdminConfigMenu] MUI mount failed — blank layout required.", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_AdminConfigMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		ref IA_MuiShell shell = IA_MuiShell.Create(
			runtime,
			"ADMIN CONFIG",
			"COMMAND UPLINK",
			"Live mission tuning  •  Changes apply on Save",
			640
		);

		m_Tabs = runtime.CreateTabs("tabs");
		m_Tabs.SetIntro(0.28, 0.4, 16);
		m_Tabs.AddTab("Scaling");
		m_Tabs.AddTab("Civilians");
		m_Tabs.AddTab("Artillery");
		m_Tabs.AddTab("HQ");
		m_Tabs.GetOnChanged().Insert(OnAdminTabChanged);

		ref MUI_ScrollView scroll = runtime.CreateScrollView("scroll");
		scroll.SetViewportHeight(420);
		scroll.SetGap(12);
		scroll.SetIntro(0.32, 0.4, 16);

		m_PageScaling = MakePage(runtime, "pageScaling");
		m_PageCiv = MakePage(runtime, "pageCiv");
		m_PageArty = MakePage(runtime, "pageArty");
		m_PageHq = MakePage(runtime, "pageHq");

		m_AIField = runtime.CreateNumericField("AI scale multiplier", "ai");
		m_AIField.SetRange(0.1, 10);
		m_AIField.SetStep(0.1);
		m_AIField.SetDecimals(2);

		m_StaticAIField = runtime.CreateNumericField("Static AI scale override (0 = dynamic)", "staticAi");
		m_StaticAIField.SetRange(0, 20);
		m_StaticAIField.SetStep(0.1);
		m_StaticAIField.SetDecimals(2);

		m_MilVehField = runtime.CreateNumericField("Military vehicle count multiplier", "milVeh");
		m_MilVehField.SetRange(0, 10);
		m_MilVehField.SetStep(0.1);
		m_MilVehField.SetDecimals(2);

		m_PageScaling.AddChild(m_AIField);
		m_PageScaling.AddChild(m_StaticAIField);
		m_PageScaling.AddChild(m_MilVehField);

		m_civField = runtime.CreateNumericField("Civilian count multiplier", "civ");
		m_civField.SetRange(0, 100);
		m_civField.SetStep(0.1);
		m_civField.SetDecimals(2);

		m_CivVehField = runtime.CreateNumericField("Civilian vehicle multiplier", "civVeh");
		m_CivVehField.SetRange(0, 10);
		m_CivVehField.SetStep(0.1);
		m_CivVehField.SetDecimals(2);

		m_RevoltField = runtime.CreateNumericField("Revolt threshold (0–1)", "revolt");
		m_RevoltField.SetRange(0, 1);
		m_RevoltField.SetStep(0.01);
		m_RevoltField.SetDecimals(2);

		m_CivSpawnToggle = runtime.CreateToggle("Enable civilian spawning", "civSpawn");

		m_PageCiv.AddChild(m_civField);
		m_PageCiv.AddChild(m_CivVehField);
		m_PageCiv.AddChild(m_RevoltField);
		m_PageCiv.AddChild(m_CivSpawnToggle);

		m_artyField = runtime.CreateNumericField("Artillery cooldown (seconds)", "arty");
		m_artyField.SetRange(0, 3600);
		m_artyField.SetStep(10);
		m_artyField.SetDecimals(0);

		m_ArtyMinField = runtime.CreateNumericField("Strike min delay (seconds)", "artyMin");
		m_ArtyMinField.SetRange(0, 600);
		m_ArtyMinField.SetStep(1);
		m_ArtyMinField.SetDecimals(0);

		m_ArtyMaxField = runtime.CreateNumericField("Strike max delay (seconds)", "artyMax");
		m_ArtyMaxField.SetRange(0, 600);
		m_ArtyMaxField.SetStep(1);
		m_ArtyMaxField.SetDecimals(0);

		m_ArtyChanceLabel = runtime.CreateLabel("Strike chance  18%", "artyChanceLbl");
		m_ArtyChanceLabel.SetFontSize(runtime.GetTheme().FONT_SMALL);
		m_ArtyChanceLabel.SetMuted(true);

		m_ArtyChanceSlider = runtime.CreateSlider("artyChance");
		m_ArtyChanceSlider.SetRange(0, 1);
		m_ArtyChanceSlider.SetStep(0.01);
		m_ArtyChanceSlider.SetValue(0.18);
		m_ArtyChanceSlider.GetOnChanged().Insert(OnArtyChanceChanged);

		m_ArtyChanceProgress = runtime.CreateProgress("artyChanceBar");
		m_ArtyChanceProgress.SetValue(0.18);

		m_PageArty.AddChild(m_artyField);
		m_PageArty.AddChild(m_ArtyMinField);
		m_PageArty.AddChild(m_ArtyMaxField);
		m_PageArty.AddChild(m_ArtyChanceLabel);
		m_PageArty.AddChild(m_ArtyChanceSlider);
		m_PageArty.AddChild(m_ArtyChanceProgress);

		m_heliToggle = runtime.CreateToggle("Disable HQ helipads", "heli");
		m_groundToggle = runtime.CreateToggle("Disable HQ ground vehicles", "ground");
		m_RolesToggle = runtime.CreateToggle("Enforce pilot role restrictions", "roles");

		m_HaloMaxField = runtime.CreateNumericField("HALO Jump max players (0 = off)", "haloMax");
		m_HaloMaxField.SetRange(0, 128);
		m_HaloMaxField.SetStep(1);
		m_HaloMaxField.SetDecimals(0);
		m_HaloMaxField.SetValue(IA_Config.HALO_JUMP_MAX_PLAYERS_DEFAULT);

		ref MUI_Label factionLbl = runtime.CreateLabel("Preferred enemy faction (future spawns)", "factionLbl");
		factionLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		factionLbl.SetMuted(true);

		m_FactionDrop = runtime.CreateDropdown("faction");
		m_FactionDrop.AddItem("Keep current");
		m_FactionDrop.AddItem("USSR");
		m_FactionDrop.AddItem("US");
		m_FactionDrop.AddItem("FIA");
		m_FactionDrop.SetIndex(0);

		m_PageHq.AddChild(m_heliToggle);
		m_PageHq.AddChild(m_groundToggle);
		m_PageHq.AddChild(m_RolesToggle);
		m_PageHq.AddChild(m_HaloMaxField);
		m_PageHq.AddChild(factionLbl);
		m_PageHq.AddChild(m_FactionDrop);

		scroll.AddChild(m_PageScaling);
		scroll.AddChild(m_PageCiv);
		scroll.AddChild(m_PageArty);
		scroll.AddChild(m_PageHq);

		ref MUI_Panel footerBtns = runtime.CreatePanel("footerBtns");
		footerBtns.GetStyle().m_Fill = Color.FromInt(0);
		footerBtns.GetStyle().m_fRadius = 0;
		footerBtns.GetStyle().m_fGap = 12;
		footerBtns.GetStyle().m_bBlockHit = false;
		footerBtns.SetFillWidth();
		footerBtns.SetIntro(0.52, 0.4, 18);

		ref MUI_Row persistRow = runtime.CreateRow("persistRow");
		persistRow.SetGap(12);

		ref MUI_Button saveBtn = runtime.CreateButton("Save", "save");
		saveBtn.MakeAccent();
		saveBtn.GetOnClicked().Insert(OnMikesSave);

		ref MUI_Button persistBtn = runtime.CreateButton("Save for restart", "persist");
		persistBtn.GetOnClicked().Insert(OnMikesPersist);

		ref MUI_Button clearBtn = runtime.CreateButton("Clear saved", "clearSaved");
		clearBtn.GetOnClicked().Insert(OnMikesClearPersisted);

		persistRow.AddChild(saveBtn);
		persistRow.AddChild(persistBtn);
		persistRow.AddChild(clearBtn);

		ref MUI_Row actionRow = runtime.CreateRow("actionRow");
		actionRow.SetGap(12);

		ref MUI_Button promoteBtn = runtime.CreateButton("Promote Self", "promote");
		promoteBtn.GetOnClicked().Insert(OnMikesPromoteSelf);

		ref MUI_Button completeBtn = runtime.CreateButton("Complete Zone", "complete");
		completeBtn.MakeDanger();
		completeBtn.GetOnClicked().Insert(OnMikesComplete);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMUIBack);

		actionRow.AddChild(promoteBtn);
		actionRow.AddChild(completeBtn);
		actionRow.AddChild(closeBtn);

		footerBtns.AddChild(persistRow);
		footerBtns.AddChild(actionRow);

		shell.GetCard().AddChild(m_Tabs);
		shell.GetCard().AddChild(scroll);
		shell.AddFooter(runtime, "Save applies now  •  Save for restart also writes the server profile (last-wins on boot)", footerBtns);
		shell.Mount(runtime);

		ShowAdminPage(0);
	}

	//------------------------------------------------------------------------------------------------
	protected MUI_Panel MakePage(notnull MUI_Runtime runtime, string name)
	{
		ref MUI_Panel page = runtime.CreatePanel(name);
		page.GetStyle().m_Fill = Color.FromInt(0);
		page.GetStyle().m_fRadius = 0;
		page.GetStyle().m_fGap = 12;
		page.GetStyle().m_bBlockHit = false;
		page.SetFillWidth();
		return page;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAdminTabChanged()
	{
		if (!m_Tabs)
			return;
		ShowAdminPage(m_Tabs.GetIndex());
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowAdminPage(int index)
	{
		if (m_PageScaling)
			m_PageScaling.SetVisible(index == 0);
		if (m_PageCiv)
			m_PageCiv.SetVisible(index == 1);
		if (m_PageArty)
			m_PageArty.SetVisible(index == 2);
		if (m_PageHq)
			m_PageHq.SetVisible(index == 3);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnArtyChanceChanged()
	{
		if (!m_ArtyChanceSlider)
			return;
		float v = m_ArtyChanceSlider.GetValue();
		if (m_ArtyChanceProgress)
			m_ArtyChanceProgress.SetValue(v);
		if (m_ArtyChanceLabel)
			m_ArtyChanceLabel.SetText(string.Format("Strike chance  %1%%", Math.Round(v * 100.0)));
	}

	//------------------------------------------------------------------------------------------------
	protected void PopulateFromConfig()
	{
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (!cfg)
			return;

		if (m_civField)
			m_civField.SetValue(cfg.m_fCivilianCountMultiplier);
		if (m_AIField)
			m_AIField.SetValue(cfg.m_fAIScaleMultiplier);
		if (m_StaticAIField)
			m_StaticAIField.SetValue(cfg.m_fStaticAIScaleOverride);
		if (m_MilVehField)
			m_MilVehField.SetValue(cfg.m_fMilitaryVehicleCountMultiplier);
		if (m_CivVehField)
			m_CivVehField.SetValue(cfg.m_fCivilianVehicleCountMultiplier);
		if (m_RevoltField)
			m_RevoltField.SetValue(cfg.m_fCivilianRevoltThreshold);
		if (m_CivSpawnToggle)
			m_CivSpawnToggle.SetChecked(cfg.m_bEnableCivilianSpawning);
		if (m_artyField)
			m_artyField.SetValue(cfg.m_iArtilleryCooldown);
		if (m_ArtyMinField)
			m_ArtyMinField.SetValue(cfg.m_iArtilleryMinDelay);
		if (m_ArtyMaxField)
			m_ArtyMaxField.SetValue(cfg.m_iArtilleryMaxDelay);
		if (m_ArtyChanceSlider)
		{
			m_ArtyChanceSlider.SetValue(cfg.m_fArtilleryStrikeChance);
			OnArtyChanceChanged();
		}
		if (m_heliToggle)
			m_heliToggle.SetChecked(cfg.m_bDisableHQHelipads);
		if (m_groundToggle)
			m_groundToggle.SetChecked(cfg.m_bDisableHQGroundVehicles);
		if (m_RolesToggle)
			m_RolesToggle.SetChecked(cfg.m_bEnforceRoleRestrictions);
		if (m_HaloMaxField)
			m_HaloMaxField.SetValue(cfg.m_iHaloJumpMaxPlayers);

		if (m_FactionDrop && cfg.m_sDesiredEnemyFactionKeys && cfg.m_sDesiredEnemyFactionKeys.Count() > 0)
		{
			string key = cfg.m_sDesiredEnemyFactionKeys[0];
			if (key == "USSR")
				m_FactionDrop.SetIndex(1);
			else if (key == "US")
				m_FactionDrop.SetIndex(2);
			else if (key == "FIA")
				m_FactionDrop.SetIndex(3);
			else
				m_FactionDrop.SetIndex(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesSave()
	{
		SubmitAdminConfig(false);
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesPersist()
	{
		SubmitAdminConfig(true);
		SCR_HintManagerComponent.ShowCustomHint(
			"Overrides written to the server profile. They load after the mission config on the next restart.",
			"ADMIN CONFIG",
			6
		);
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesClearPersisted()
	{
		IA_MissionInitializer.ClearPersistedAdminConfig();
		SCR_HintManagerComponent.ShowCustomHint(
			"Saved overrides cleared. Next restart uses the mission config.",
			"ADMIN CONFIG",
			6
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void SubmitAdminConfig(bool persist)
	{
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!missionInit && !pc)
		{
			Print("[IA_AdminConfigMenu] ERROR: IA_MissionInitializer instance not found!", LogLevel.ERROR);
			return;
		}

		float civCount = 1.0;
		float aiScale = 1.0;
		float staticAi = 0;
		float milVeh = 1.0;
		float civVeh = 1.0;
		float revolt = 0.11;
		int artyCooldown = 300;
		int artyMin = 45;
		int artyMax = 70;
		float artyChance = 0.18;
		bool disableHeli = false;
		bool disableGround = false;
		bool enableCiv = true;
		bool enforceRoles = false;
		int haloMaxPlayers = IA_Config.HALO_JUMP_MAX_PLAYERS_DEFAULT;
		string factionKey = "";

		if (m_civField)
			civCount = m_civField.GetValue();
		if (m_AIField)
			aiScale = m_AIField.GetValue();
		if (m_StaticAIField)
			staticAi = m_StaticAIField.GetValue();
		if (m_MilVehField)
			milVeh = m_MilVehField.GetValue();
		if (m_CivVehField)
			civVeh = m_CivVehField.GetValue();
		if (m_RevoltField)
			revolt = m_RevoltField.GetValue();
		if (m_artyField)
			artyCooldown = m_artyField.GetText().ToInt();
		if (m_ArtyMinField)
			artyMin = m_ArtyMinField.GetText().ToInt();
		if (m_ArtyMaxField)
			artyMax = m_ArtyMaxField.GetText().ToInt();
		if (m_ArtyChanceSlider)
			artyChance = m_ArtyChanceSlider.GetValue();
		if (m_heliToggle)
			disableHeli = m_heliToggle.IsChecked();
		if (m_groundToggle)
			disableGround = m_groundToggle.IsChecked();
		if (m_CivSpawnToggle)
			enableCiv = m_CivSpawnToggle.IsChecked();
		if (m_RolesToggle)
			enforceRoles = m_RolesToggle.IsChecked();
		if (m_HaloMaxField)
			haloMaxPlayers = m_HaloMaxField.GetText().ToInt();
		if (m_FactionDrop && m_FactionDrop.GetIndex() > 0)
			factionKey = m_FactionDrop.GetText();

		if (persist)
		{
			IA_MissionInitializer.PersistConfig(
				civCount,
				aiScale,
				disableHeli,
				disableGround,
				artyCooldown,
				staticAi,
				milVeh,
				civVeh,
				revolt,
				enableCiv,
				enforceRoles,
				artyChance,
				artyMin,
				artyMax,
				factionKey,
				haloMaxPlayers
			);
			return;
		}

		IA_MissionInitializer.UpdateConfig(
			civCount,
			aiScale,
			disableHeli,
			disableGround,
			artyCooldown,
			staticAi,
			milVeh,
			civVeh,
			revolt,
			enableCiv,
			enforceRoles,
			artyChance,
			artyMin,
			artyMax,
			factionKey,
			haloMaxPlayers
		);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesPromoteSelf()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskPromoteSelf();
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesComplete()
	{
		IA_MissionInitializer.ForceCompleteZone();
		GetGame().GetMenuManager().CloseMenu(this);
	}
}
