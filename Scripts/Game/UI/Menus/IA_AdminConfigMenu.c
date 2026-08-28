//------------------------------------------------------------------------------------------------
//! Runtime admin tuning for IA_Config. Live source of truth; the mission .conf is a
//! workshop baseline only. Uses MUI blank layout (no legacy widget path).
//------------------------------------------------------------------------------------------------
class IA_AdminConfigMenu : MUI_MenuBase
{
	protected ref MUI_Tabs m_Tabs;
	protected ref MUI_Panel m_PageScaling;
	protected ref MUI_Panel m_PageCiv;
	protected ref MUI_Panel m_PageArty;
	protected ref MUI_Panel m_PageHq;
	protected ref MUI_Panel m_PageFactions;
	protected ref MUI_Panel m_PageQrf;
	protected ref MUI_Panel m_PageDirector;

	protected ref MUI_Toggle m_GmModeToggle;
	protected ref MUI_Toggle m_GmAutoActivateToggle;
	protected ref MUI_Toggle m_GmAutoQrfToggle;
	protected ref MUI_Toggle m_GmAutoArtyToggle;
	protected ref MUI_Toggle m_GmAutoSideToggle;
	protected ref MUI_Toggle m_GmAutoSupportToggle;

	protected ref MUI_Button m_QrfInfantryBtn;
	protected ref MUI_Button m_QrfMotorizedBtn;
	protected ref MUI_Button m_QrfMechanizedBtn;
	protected ref MUI_Button m_QrfArmouredBtn;
	protected ref MUI_Button m_QrfAirborneBtn;

	protected ref MUI_NumericField m_AIField;
	protected ref MUI_NumericField m_StaticAIField;
	protected ref MUI_NumericField m_MilVehField;

	protected ref MUI_NumericField m_civField;
	protected ref MUI_NumericField m_CivVehField;
	protected ref MUI_NumericField m_RevoltField;
	protected ref MUI_NumericField m_RevoltNotifField;
	protected ref MUI_NumericField m_RevoltReinfField;
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

	protected ref MUI_Toggle m_EnemyAutoToggle;
	protected ref array<ref MUI_Toggle> m_EnemyFactionToggles;
	protected ref array<string> m_EnemyFactionChoiceKeys;
	protected ref MUI_Toggle m_VehicleMatchToggle;
	protected ref array<ref MUI_Toggle> m_VehicleFactionToggles;
	protected ref array<string> m_VehicleFactionChoiceKeys;
	protected bool m_bFactionUiLock;

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
			"Live source of truth  •  Changes apply on Save",
			880
		);

		m_Tabs = runtime.CreateTabs("tabs");
		m_Tabs.SetIntro(0.28, 0.4, 16);
		m_Tabs.AddTab("Scaling");
		m_Tabs.AddTab("Civilians");
		m_Tabs.AddTab("Artillery");
		m_Tabs.AddTab("HQ");
		m_Tabs.AddTab("Factions");
		m_Tabs.AddTab("QRF");
		m_Tabs.AddTab("Director");
		m_Tabs.GetOnChanged().Insert(OnAdminTabChanged);

		ref MUI_ScrollView scroll = runtime.CreateScrollView("scroll");
		scroll.SetViewportHeight(420);
		scroll.SetGap(12);
		scroll.SetIntro(0.32, 0.4, 16);

		m_PageScaling = MakePage(runtime, "pageScaling");
		m_PageCiv = MakePage(runtime, "pageCiv");
		m_PageArty = MakePage(runtime, "pageArty");
		m_PageHq = MakePage(runtime, "pageHq");
		m_PageFactions = MakePage(runtime, "pageFactions");
		m_PageQrf = MakePage(runtime, "pageQrf");
		m_PageDirector = MakePage(runtime, "pageDirector");

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

		m_RevoltField = runtime.CreateNumericField("Revolt threshold (0-1)", "revolt");
		m_RevoltField.SetRange(0, 1);
		m_RevoltField.SetStep(0.01);
		m_RevoltField.SetDecimals(2);

		m_RevoltNotifField = runtime.CreateNumericField("Revolt notification delay (seconds)", "revoltNotif");
		m_RevoltNotifField.SetRange(0, 600);
		m_RevoltNotifField.SetStep(1);
		m_RevoltNotifField.SetDecimals(0);
		m_RevoltNotifField.SetValue(30);

		m_RevoltReinfField = runtime.CreateNumericField("Revolt reinforcement delay (seconds)", "revoltReinf");
		m_RevoltReinfField.SetRange(0, 3600);
		m_RevoltReinfField.SetStep(5);
		m_RevoltReinfField.SetDecimals(0);
		m_RevoltReinfField.SetValue(180);

		m_CivSpawnToggle = runtime.CreateToggle("Enable civilian spawning", "civSpawn");

		m_PageCiv.AddChild(m_civField);
		m_PageCiv.AddChild(m_CivVehField);
		m_PageCiv.AddChild(m_RevoltField);
		m_PageCiv.AddChild(m_RevoltNotifField);
		m_PageCiv.AddChild(m_RevoltReinfField);
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

		ref MUI_Label hqPrefabLbl = runtime.CreateLabel("HQ vehicle prefab lists still come from the mission .conf (Workbench). Everything else is controlled here.", "hqPrefabLbl");
		hqPrefabLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		hqPrefabLbl.SetMuted(true);

		m_PageHq.AddChild(m_heliToggle);
		m_PageHq.AddChild(m_groundToggle);
		m_PageHq.AddChild(m_RolesToggle);
		m_PageHq.AddChild(m_HaloMaxField);
		m_PageHq.AddChild(hqPrefabLbl);

		BuildFactionPage(runtime);

		ref MUI_Label qrfLbl = runtime.CreateLabel("Spawn QRF through the normal mission path", "qrfLbl");
		qrfLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		qrfLbl.SetMuted(true);

		m_QrfInfantryBtn = runtime.CreateButton("Infantry QRF", "qrfInf");
		m_QrfInfantryBtn.GetOnClicked().Insert(OnQrfInfantry);
		m_QrfMotorizedBtn = runtime.CreateButton("Motorized QRF", "qrfMotor");
		m_QrfMotorizedBtn.GetOnClicked().Insert(OnQrfMotorized);
		m_QrfMechanizedBtn = runtime.CreateButton("Mechanized QRF", "qrfMech");
		m_QrfMechanizedBtn.GetOnClicked().Insert(OnQrfMechanized);
		m_QrfArmouredBtn = runtime.CreateButton("Armoured QRF", "qrfArmour");
		m_QrfArmouredBtn.GetOnClicked().Insert(OnQrfArmoured);
		m_QrfAirborneBtn = runtime.CreateButton("Airborne QRF", "qrfAir");
		m_QrfAirborneBtn.MakeAccent();
		m_QrfAirborneBtn.GetOnClicked().Insert(OnQrfAirborne);

		ref MUI_Row qrfRow1 = runtime.CreateRow("qrfRow1");
		qrfRow1.SetGap(12);
		qrfRow1.AddChild(m_QrfInfantryBtn);
		qrfRow1.AddChild(m_QrfMotorizedBtn);
		qrfRow1.AddChild(m_QrfMechanizedBtn);

		ref MUI_Row qrfRow2 = runtime.CreateRow("qrfRow2");
		qrfRow2.SetGap(12);
		qrfRow2.AddChild(m_QrfArmouredBtn);
		qrfRow2.AddChild(m_QrfAirborneBtn);

		m_PageQrf.AddChild(qrfLbl);
		m_PageQrf.AddChild(qrfRow1);
		m_PageQrf.AddChild(qrfRow2);

		m_GmModeToggle = runtime.CreateToggle("Game Master mode", "gmMode");
		m_GmAutoActivateToggle = runtime.CreateToggle("After Live: start Staging, or the next map AO", "gmAutoAct");
		m_GmAutoQrfToggle = runtime.CreateToggle("Auto QRF on Live AOs", "gmAutoQrf");
		m_GmAutoArtyToggle = runtime.CreateToggle("Auto artillery on Live AOs", "gmAutoArty");
		m_GmAutoSideToggle = runtime.CreateToggle("Auto side missions", "gmAutoSide");
		m_GmAutoSupportToggle = runtime.CreateToggle("Auto mortar + radio on Activate", "gmAutoSup");

		ref MUI_Button openDirBtn = runtime.CreateButton("Open Director Map", "openDir");
		openDirBtn.MakeAccent();
		openDirBtn.GetOnClicked().Insert(OnOpenDirector);

		m_PageDirector.AddChild(m_GmModeToggle);
		m_PageDirector.AddChild(m_GmAutoActivateToggle);
		m_PageDirector.AddChild(m_GmAutoQrfToggle);
		m_PageDirector.AddChild(m_GmAutoArtyToggle);
		m_PageDirector.AddChild(m_GmAutoSideToggle);
		m_PageDirector.AddChild(m_GmAutoSupportToggle);
		m_PageDirector.AddChild(openDirBtn);

		scroll.AddChild(m_PageScaling);
		scroll.AddChild(m_PageCiv);
		scroll.AddChild(m_PageArty);
		scroll.AddChild(m_PageHq);
		scroll.AddChild(m_PageFactions);
		scroll.AddChild(m_PageQrf);
		scroll.AddChild(m_PageDirector);

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
		shell.AddFooter(runtime, "Save applies now  •  Save for restart writes the server profile (applied after the mission .conf)", footerBtns);
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
	protected void BuildFactionPage(notnull MUI_Runtime runtime)
	{
		if (!m_PageFactions)
			return;

		ref MUI_Label infLbl = runtime.CreateLabel("Infantry enemies for future spawns. Auto-detect uses the usual mod-aware rotation.", "infFacLbl");
		infLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		infLbl.SetMuted(true);
		m_PageFactions.AddChild(infLbl);

		m_EnemyAutoToggle = runtime.CreateToggle("Auto-detect infantry factions", "enfAuto");
		m_EnemyAutoToggle.SetChecked(true);
		m_EnemyAutoToggle.GetOnChanged().Insert(OnEnemyAutoChanged);
		m_PageFactions.AddChild(m_EnemyAutoToggle);

		m_EnemyFactionToggles = new array<ref MUI_Toggle>();
		m_EnemyFactionChoiceKeys = new array<string>();
		ref array<string> infNames = new array<string>();
		IA_AdminConfigUtil.CollectFactionChoices(EEntityCatalogType.CHARACTER, 1, m_EnemyFactionChoiceKeys, infNames);
		AddFactionToggles(runtime, m_PageFactions, "enf_", m_EnemyFactionChoiceKeys, infNames, m_EnemyFactionToggles, false);

		ref MUI_Label vehLbl = runtime.CreateLabel("Vehicle enemies. Match infantry unless you need a different catalog (common with mixed-faction mods).", "vehFacLbl");
		vehLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		vehLbl.SetMuted(true);
		m_PageFactions.AddChild(vehLbl);

		m_VehicleMatchToggle = runtime.CreateToggle("Vehicle factions match infantry", "vehMatch");
		m_VehicleMatchToggle.SetChecked(true);
		m_VehicleMatchToggle.GetOnChanged().Insert(OnVehicleMatchChanged);
		m_PageFactions.AddChild(m_VehicleMatchToggle);

		m_VehicleFactionToggles = new array<ref MUI_Toggle>();
		m_VehicleFactionChoiceKeys = new array<string>();
		ref array<string> vehNames = new array<string>();
		IA_AdminConfigUtil.CollectFactionChoices(EEntityCatalogType.VEHICLE, 1, m_VehicleFactionChoiceKeys, vehNames);
		AddFactionToggles(runtime, m_PageFactions, "veh_", m_VehicleFactionChoiceKeys, vehNames, m_VehicleFactionToggles, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddFactionToggles(notnull MUI_Runtime runtime, notnull MUI_Panel page, string namePrefix, notnull array<string> keys, notnull array<string> names, notnull array<ref MUI_Toggle> toggles, bool vehicle)
	{
		int count = keys.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			string label = names[i];
			if (label.IsEmpty())
				label = keys[i];
			ref MUI_Toggle toggle = runtime.CreateToggle(label, namePrefix + keys[i]);
			if (vehicle)
				toggle.GetOnChanged().Insert(OnVehicleFactionToggleChanged);
			else
				toggle.GetOnChanged().Insert(OnEnemyFactionToggleChanged);
			toggles.Insert(toggle);
			page.AddChild(toggle);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnEnemyAutoChanged()
	{
		if (m_bFactionUiLock)
			return;
		if (!m_EnemyAutoToggle)
			return;
		if (!m_EnemyAutoToggle.IsChecked())
			return;

		m_bFactionUiLock = true;
		UncheckAll(m_EnemyFactionToggles);
		m_bFactionUiLock = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnEnemyFactionToggleChanged()
	{
		if (m_bFactionUiLock)
			return;
		if (!m_EnemyAutoToggle)
			return;

		m_bFactionUiLock = true;
		if (AnyChecked(m_EnemyFactionToggles))
			m_EnemyAutoToggle.SetChecked(false);
		else
			m_EnemyAutoToggle.SetChecked(true);
		m_bFactionUiLock = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVehicleMatchChanged()
	{
		if (m_bFactionUiLock)
			return;
		if (!m_VehicleMatchToggle)
			return;
		if (!m_VehicleMatchToggle.IsChecked())
			return;

		m_bFactionUiLock = true;
		UncheckAll(m_VehicleFactionToggles);
		m_bFactionUiLock = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVehicleFactionToggleChanged()
	{
		if (m_bFactionUiLock)
			return;
		if (!m_VehicleMatchToggle)
			return;

		m_bFactionUiLock = true;
		if (AnyChecked(m_VehicleFactionToggles))
			m_VehicleMatchToggle.SetChecked(false);
		else
			m_VehicleMatchToggle.SetChecked(true);
		m_bFactionUiLock = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void UncheckAll(array<ref MUI_Toggle> toggles)
	{
		if (!toggles)
			return;
		int count = toggles.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			MUI_Toggle toggle = toggles[i];
			if (toggle)
				toggle.SetChecked(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool AnyChecked(array<ref MUI_Toggle> toggles)
	{
		if (!toggles)
			return false;
		int count = toggles.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			MUI_Toggle toggle = toggles[i];
			if (toggle && toggle.IsChecked())
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected string CollectCheckedKeys(array<ref MUI_Toggle> toggles, array<string> keys)
	{
		if (!toggles || !keys)
			return IA_AdminConfigUtil.FACTION_AUTO;

		ref array<string> selected = new array<string>();
		int count = toggles.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			MUI_Toggle toggle = toggles[i];
			if (!toggle)
				continue;
			if (!toggle.IsChecked())
				continue;
			if (i >= keys.Count())
				continue;
			selected.Insert(keys[i]);
		}
		return IA_AdminConfigUtil.JoinKeys(selected);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyKeysToToggles(array<string> selected, array<ref MUI_Toggle> toggles, array<string> keys, MUI_Toggle autoToggle)
	{
		bool configured = false;
		if (selected && selected.Count() > 0)
			configured = true;

		if (toggles && keys)
		{
			int count = toggles.Count();
			int i;
			for (i = 0; i < count; i++)
			{
				MUI_Toggle toggle = toggles[i];
				if (!toggle)
					continue;
				bool on = false;
				if (configured && i < keys.Count())
					on = IA_AdminConfigUtil.ArrayContains(selected, keys[i]);
				toggle.SetChecked(on);
			}
		}

		if (autoToggle)
		{
			if (configured)
				autoToggle.SetChecked(false);
			else
				autoToggle.SetChecked(true);
		}
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
		if (m_PageFactions)
			m_PageFactions.SetVisible(index == 4);
		if (m_PageQrf)
			m_PageQrf.SetVisible(index == 5);
		if (m_PageDirector)
			m_PageDirector.SetVisible(index == 6);
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
		if (m_RevoltNotifField)
			m_RevoltNotifField.SetValue(cfg.m_iCivilianRevoltNotificationDelay / 1000.0);
		if (m_RevoltReinfField)
			m_RevoltReinfField.SetValue(cfg.m_iCivilianRevoltReinforcementDelay / 1000.0);
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
		if (m_GmModeToggle)
			m_GmModeToggle.SetChecked(cfg.m_bGameMasterMode);
		if (m_GmAutoActivateToggle)
			m_GmAutoActivateToggle.SetChecked(cfg.m_bGmAutoActivateStaging);
		if (m_GmAutoQrfToggle)
			m_GmAutoQrfToggle.SetChecked(cfg.m_bGmAutoQrf);
		if (m_GmAutoArtyToggle)
			m_GmAutoArtyToggle.SetChecked(cfg.m_bGmAutoArty);
		if (m_GmAutoSideToggle)
			m_GmAutoSideToggle.SetChecked(cfg.m_bGmAutoSideMissions);
		if (m_GmAutoSupportToggle)
			m_GmAutoSupportToggle.SetChecked(cfg.m_bGmAutoPlaceSupport);

		m_bFactionUiLock = true;
		ApplyKeysToToggles(cfg.m_sDesiredEnemyFactionKeys, m_EnemyFactionToggles, m_EnemyFactionChoiceKeys, m_EnemyAutoToggle);
		ApplyKeysToToggles(cfg.m_sDesiredEnemyVehicleFactionKeys, m_VehicleFactionToggles, m_VehicleFactionChoiceKeys, m_VehicleMatchToggle);
		m_bFactionUiLock = false;
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
			"Overrides written to the server profile. They load after the mission .conf on the next restart.",
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
			"Saved overrides cleared. Next restart uses the mission .conf.",
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
		string infantryKeysPacked = IA_AdminConfigUtil.FACTION_AUTO;
		string vehicleKeysPacked = IA_AdminConfigUtil.FACTION_AUTO;
		int revoltNotifMs = 30000;
		int revoltReinfMs = 180000;
		bool gmMode = false;
		bool gmAutoActivate = false;
		bool gmAutoQrf = true;
		bool gmAutoArty = true;
		bool gmAutoSide = false;
		bool gmAutoSupport = false;

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
		if (m_RevoltNotifField)
			revoltNotifMs = Math.Round(m_RevoltNotifField.GetValue() * 1000.0);
		if (m_RevoltReinfField)
			revoltReinfMs = Math.Round(m_RevoltReinfField.GetValue() * 1000.0);

		infantryKeysPacked = CollectCheckedKeys(m_EnemyFactionToggles, m_EnemyFactionChoiceKeys);
		if (m_EnemyAutoToggle && m_EnemyAutoToggle.IsChecked())
			infantryKeysPacked = IA_AdminConfigUtil.FACTION_AUTO;
		factionKey = IA_AdminConfigUtil.FirstKey(infantryKeysPacked);

		vehicleKeysPacked = CollectCheckedKeys(m_VehicleFactionToggles, m_VehicleFactionChoiceKeys);
		if (m_VehicleMatchToggle && m_VehicleMatchToggle.IsChecked())
			vehicleKeysPacked = IA_AdminConfigUtil.FACTION_AUTO;

		if (m_GmModeToggle)
			gmMode = m_GmModeToggle.IsChecked();
		if (m_GmAutoActivateToggle)
			gmAutoActivate = m_GmAutoActivateToggle.IsChecked();
		if (m_GmAutoQrfToggle)
			gmAutoQrf = m_GmAutoQrfToggle.IsChecked();
		if (m_GmAutoArtyToggle)
			gmAutoArty = m_GmAutoArtyToggle.IsChecked();
		if (m_GmAutoSideToggle)
			gmAutoSide = m_GmAutoSideToggle.IsChecked();
		if (m_GmAutoSupportToggle)
			gmAutoSupport = m_GmAutoSupportToggle.IsChecked();

		string packed = IA_MissionInitializer.PackAdminConfig(
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
		int gmMask = IA_MissionInitializer.PackGmAdminMask(
			gmMode,
			gmAutoActivate,
			gmAutoQrf,
			gmAutoArty,
			gmAutoSide,
			gmAutoSupport
		);
		packed = packed + "|" + gmMask.ToString();
		packed = packed + "|" + IA_MissionInitializer.PackAdminConfigExtras(revoltNotifMs, revoltReinfMs, infantryKeysPacked, vehicleKeysPacked);
		IA_MissionInitializer.SubmitPackedAdminConfig(packed, persist);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQrfInfantry()
	{
		RequestQRF(IA_QRFType.Infantry);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQrfMotorized()
	{
		RequestQRF(IA_QRFType.Motorized);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQrfMechanized()
	{
		RequestQRF(IA_QRFType.Mechanized);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQrfArmoured()
	{
		RequestQRF(IA_QRFType.Armoured);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQrfAirborne()
	{
		RequestQRF(IA_QRFType.Airborne);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestQRF(IA_QRFType type)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskForceQRF(type);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOpenDirector()
	{
		GetGame().GetMenuManager().CloseMenu(this);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_GmDirectorMenu);
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
