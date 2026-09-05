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
	protected ref MUI_Panel m_PageDefense;

	protected ref MUI_Toggle m_GmModeToggle;
	protected ref MUI_Toggle m_GmAutoActivateToggle;
	protected ref MUI_Toggle m_GmAutoQrfToggle;
	protected ref MUI_Toggle m_GmAutoArtyToggle;
	protected ref MUI_Toggle m_GmAutoSideToggle;
	protected ref MUI_Toggle m_GmAutoSupportToggle;

	protected ref MUI_HintLayer m_Hints;

	protected ref MUI_Button m_QrfInfantryBtn;
	protected ref MUI_Button m_QrfMotorizedBtn;
	protected ref MUI_Button m_QrfMechanizedBtn;
	protected ref MUI_Button m_QrfArmouredBtn;
	protected ref MUI_Button m_QrfAirborneBtn;

	protected ref MUI_NumericField m_AIField;
	protected ref MUI_NumericField m_StaticAIField;
	protected ref MUI_NumericField m_MilVehField;
	protected ref MUI_Dropdown m_NormalSkillDrop;
	protected ref MUI_Dropdown m_EliteSkillDrop;
	protected ref MUI_NumericField m_NormalFireField;
	protected ref MUI_NumericField m_EliteFireField;
	protected ref MUI_NumericField m_NormalPercField;
	protected ref MUI_NumericField m_ElitePercField;

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

	protected ref MUI_Toggle m_DefendLegacyToggle;
	protected ref MUI_NumericField m_DefendDurMinField;
	protected ref MUI_NumericField m_DefendDurMaxField;
	protected ref MUI_NumericField m_DefendPrepMinField;
	protected ref MUI_NumericField m_DefendPrepMaxField;
	protected ref MUI_NumericField m_DefendEventsField;
	protected ref MUI_NumericField m_DefendThirdLowField;
	protected ref MUI_NumericField m_DefendThirdMidField;
	protected ref MUI_NumericField m_DefendThirdHighField;
	protected ref MUI_NumericField m_DefendPriSuccMinField;
	protected ref MUI_NumericField m_DefendPriSuccMaxField;
	protected ref MUI_NumericField m_DefendPriFailMinField;
	protected ref MUI_NumericField m_DefendPriFailMaxField;
	protected ref MUI_Toggle m_DefendDocSiegeToggle;
	protected ref MUI_Toggle m_DefendDocBreakToggle;
	protected ref MUI_Toggle m_DefendDocAirToggle;
	protected ref MUI_Toggle m_DefendDocCmdToggle;
	protected ref MUI_Toggle m_DefendEvtCmdToggle;
	protected ref MUI_Toggle m_DefendEvtEliteToggle;
	protected ref MUI_Toggle m_DefendEvtScoutToggle;
	protected ref MUI_Toggle m_DefendEvtConvoyToggle;
	protected ref MUI_Toggle m_DefendEvtSniperToggle;
	protected ref MUI_Slider m_DefendHotDropSlider;
	protected ref MUI_Label m_DefendHotDropLabel;
	protected ref MUI_Toggle m_DynamicBaseEnabledToggle;
	protected ref MUI_NumericField m_DynamicBaseChanceField;
	protected ref MUI_Toggle m_DynamicBaseInGmToggle;
	protected ref MUI_Dropdown m_DynamicBaseSizeDrop;

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

		m_Hints = runtime.CreateHintLayer("hints");

		m_Tabs = runtime.CreateTabs("tabs");
		m_Tabs.SetIntro(0.28, 0.4, 16);
		m_Tabs.AddTab("Scaling");
		m_Tabs.AddTab("Civilians");
		m_Tabs.AddTab("Artillery");
		m_Tabs.AddTab("HQ");
		m_Tabs.AddTab("Factions");
		m_Tabs.AddTab("QRF");
		m_Tabs.AddTab("Director");
		m_Tabs.AddTab("Defense");
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
		m_PageDefense = MakePage(runtime, "pageDefense");

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
		m_Hints.AddHint(m_Tabs, "Settings pages", "Choose a tab to view a different group of settings. Help updates to explain the open tab.");
		m_Hints.AddHint(m_AIField, "Enemy strength", "Changes how many enemy soldiers appear as the player count rises. 1 is normal, 0.5 is about half, and 2 is about double.");
		m_Hints.AddHint(m_StaticAIField, "Fixed enemy strength", "Set this above 0 to ignore the player count and keep enemy numbers at a fixed level. Leave it at 0 for normal player scaling.");
		m_Hints.AddHint(m_MilVehField, "Enemy vehicle count", "Changes how many enemy military vehicles appear. 1 is normal, 0.5 is about half, and 2 is about double.");

		ref MUI_Label combatLbl = runtime.CreateLabel("AI combat  •  Skill is aim accuracy only. Fire rate and spotting are separate.", "combatLbl");
		combatLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		combatLbl.SetMuted(true);
		m_PageScaling.AddChild(combatLbl);

		m_NormalSkillDrop = runtime.CreateDropdown("normalSkill");
		FillSkillDropdown(m_NormalSkillDrop);
		m_NormalSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(EAISkill.VETERAN));

		m_EliteSkillDrop = runtime.CreateDropdown("eliteSkill");
		FillSkillDropdown(m_EliteSkillDrop);
		m_EliteSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(EAISkill.EXPERT));

		m_NormalFireField = runtime.CreateNumericField("Normal fire rate (1 = vanilla)", "nFire");
		m_NormalFireField.SetRange(0.05, 2);
		m_NormalFireField.SetStep(0.05);
		m_NormalFireField.SetDecimals(2);
		m_NormalFireField.SetValue(1);

		m_EliteFireField = runtime.CreateNumericField("Elite fire rate", "eFire");
		m_EliteFireField.SetRange(0.05, 2);
		m_EliteFireField.SetStep(0.05);
		m_EliteFireField.SetDecimals(2);
		m_EliteFireField.SetValue(1.25);

		m_NormalPercField = runtime.CreateNumericField("Normal spotting (1 = vanilla)", "nPerc");
		m_NormalPercField.SetRange(0.1, 4);
		m_NormalPercField.SetStep(0.1);
		m_NormalPercField.SetDecimals(2);
		m_NormalPercField.SetValue(1);

		m_ElitePercField = runtime.CreateNumericField("Elite spotting", "ePerc");
		m_ElitePercField.SetRange(0.1, 4);
		m_ElitePercField.SetStep(0.1);
		m_ElitePercField.SetDecimals(2);
		m_ElitePercField.SetValue(1.5);

		ref MUI_Label nSkillLbl = runtime.CreateLabel("Normal troop skill", "nSkillLbl");
		nSkillLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		ref MUI_Label eSkillLbl = runtime.CreateLabel("Elite troop skill", "eSkillLbl");
		eSkillLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);

		m_PageScaling.AddChild(nSkillLbl);
		m_PageScaling.AddChild(m_NormalSkillDrop);
		m_PageScaling.AddChild(eSkillLbl);
		m_PageScaling.AddChild(m_EliteSkillDrop);
		m_PageScaling.AddChild(m_NormalFireField);
		m_PageScaling.AddChild(m_EliteFireField);
		m_PageScaling.AddChild(m_NormalPercField);
		m_PageScaling.AddChild(m_ElitePercField);
		m_Hints.AddHint(m_NormalSkillDrop, "Normal aim skill", "Aim accuracy for regular infantry, including commander FOB guards that are not marked elite. Veteran is the I&A default. Does not change reaction time.");
		m_Hints.AddHint(m_EliteSkillDrop, "Elite aim skill", "Aim accuracy for elite patrols and the elite fireteam at a commander FOB. Expert is the I&A default. Cylon (Cyclone) is perfect aim. Does not change reaction time.");
		m_Hints.AddHint(m_NormalFireField, "Normal fire rate", "How quickly regular infantry shoot. 1 is vanilla. Values above 1 shoot faster. Vanilla clamps 0.05 to 2.");
		m_Hints.AddHint(m_EliteFireField, "Elite fire rate", "How quickly elite infantry shoot. Default 1.25.");
		m_Hints.AddHint(m_NormalPercField, "Normal spotting", "How quickly regular infantry visually detect targets. 1 is vanilla.");
		m_Hints.AddHint(m_ElitePercField, "Elite spotting", "How quickly elite infantry visually detect targets. Default 1.5.");

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
		m_Hints.AddHint(m_civField, "Civilian population", "Changes how many civilians appear. 1 is normal, 0.5 is about half, and 2 is about double.");
		m_Hints.AddHint(m_CivVehField, "Civilian traffic", "Changes how many civilian vehicles appear. 1 is normal, 0.5 is about half, and 2 is about double.");
		m_Hints.AddHint(m_RevoltField, "When a revolt begins", "Sets how many civilians players can kill before a revolt starts. For example, 0.11 means 11 percent.");
		m_Hints.AddHint(m_RevoltNotifField, "Revolt warning delay", "How many seconds pass before players are warned that a revolt has started.");
		m_Hints.AddHint(m_RevoltReinfField, "Revolt reinforcement delay", "How many seconds pass before extra resistance fighters arrive to support the revolt.");
		m_Hints.AddHint(m_CivSpawnToggle, "Civilian spawning", "Controls whether civilians appear in areas captured by players.");

		m_artyField = runtime.CreateNumericField("Time between strikes (seconds)", "arty");
		m_artyField.SetRange(0, 3600);
		m_artyField.SetStep(10);
		m_artyField.SetDecimals(0);

		m_ArtyMinField = runtime.CreateNumericField("Smoke to impact min (seconds)", "artyMin");
		m_ArtyMinField.SetRange(0, 600);
		m_ArtyMinField.SetStep(1);
		m_ArtyMinField.SetDecimals(0);

		m_ArtyMaxField = runtime.CreateNumericField("Smoke to impact max (seconds)", "artyMax");
		m_ArtyMaxField.SetRange(0, 600);
		m_ArtyMaxField.SetStep(1);
		m_ArtyMaxField.SetDecimals(0);

		ref MUI_Label artyDelayLbl = runtime.CreateLabel("After warning smoke, each strike waits a random time in this range before rounds land. Separate from the time between strikes.", "artyDelayLbl");
		artyDelayLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		artyDelayLbl.SetMuted(true);

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
		m_PageArty.AddChild(artyDelayLbl);
		m_PageArty.AddChild(m_ArtyChanceLabel);
		m_PageArty.AddChild(m_ArtyChanceSlider);
		m_PageArty.AddChild(m_ArtyChanceProgress);
		m_Hints.AddHint(m_artyField, "Time between strikes", "The minimum number of seconds after one artillery strike before another can begin.");
		m_Hints.AddHint(m_ArtyMinField, "Shortest warning time", "The shortest possible delay between the red warning smoke and the incoming rounds.");
		m_Hints.AddHint(m_ArtyMaxField, "Longest warning time", "The longest possible delay between the red warning smoke and the incoming rounds.");
		m_Hints.AddHint(m_ArtyChanceSlider, "Chance of a strike", "Controls how often enemy artillery attacks while players are fighting at an objective. A higher value means more frequent strikes.");

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
		m_Hints.AddHint(m_heliToggle, "Disable HQ helicopters", "Turn this on to stop helicopters from appearing at the player HQ.");
		m_Hints.AddHint(m_groundToggle, "Disable HQ ground vehicles", "Turn this on to stop ground vehicles from appearing at the player HQ.");
		m_Hints.AddHint(m_RolesToggle, "Require pilot roles", "Turn this on to prevent players without a pilot role from flying restricted aircraft.");
		m_Hints.AddHint(m_HaloMaxField, "HALO player limit", "HALO jumps are available only while the connected player count is below this number. Set it to 0 to disable HALO jumps.");

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
		m_Hints.AddHint(qrfLbl, "Enemy reinforcements", "Send an enemy Quick Reaction Force toward the current objective. Choose the type of force with the buttons below.");
		m_Hints.AddHint(m_QrfInfantryBtn, "Infantry QRF", "Sends enemy soldiers on foot toward the objective.");
		m_Hints.AddHint(m_QrfMotorizedBtn, "Motorized QRF", "Sends enemy soldiers in trucks and light vehicles.");
		m_Hints.AddHint(m_QrfMechanizedBtn, "Mechanized QRF", "Sends enemy infantry supported by APCs or IFVs.");
		m_Hints.AddHint(m_QrfArmouredBtn, "Armoured QRF", "Sends tanks supported by infantry.");
		m_Hints.AddHint(m_QrfAirborneBtn, "Airborne QRF", "Drops enemy paratroopers near the objective after a short warning.");

		ref MUI_Label gmModeLbl = runtime.CreateLabel("Place and activate sites from the Director map instead.", "gmModeLbl");
		gmModeLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		gmModeLbl.SetMuted(true);

		m_GmModeToggle = runtime.CreateToggle("Don't auto-start map AOs", "gmMode");
		m_GmAutoActivateToggle = runtime.CreateToggle("After Live: start Staging, or the next map AO", "gmAutoAct");
		m_GmAutoQrfToggle = runtime.CreateToggle("Auto QRF on Live AOs", "gmAutoQrf");
		m_GmAutoArtyToggle = runtime.CreateToggle("Auto artillery on Live AOs", "gmAutoArty");
		m_GmAutoSideToggle = runtime.CreateToggle("Auto side missions", "gmAutoSide");
		m_GmAutoSupportToggle = runtime.CreateToggle("Auto mortar + radio on Activate", "gmAutoSup");

		ref MUI_Button openDirBtn = runtime.CreateButton("Open Director Map", "openDir");
		openDirBtn.MakeAccent();
		openDirBtn.GetOnClicked().Insert(OnOpenDirector);

		m_PageDirector.AddChild(gmModeLbl);
		m_PageDirector.AddChild(m_GmModeToggle);
		m_PageDirector.AddChild(m_GmAutoActivateToggle);
		m_PageDirector.AddChild(m_GmAutoQrfToggle);
		m_PageDirector.AddChild(m_GmAutoArtyToggle);
		m_PageDirector.AddChild(m_GmAutoSideToggle);
		m_PageDirector.AddChild(m_GmAutoSupportToggle);
		m_PageDirector.AddChild(openDirBtn);
		m_Hints.AddHint(m_GmModeToggle, "Manual objectives", "Stops I&A from choosing and starting objectives automatically. A Game Master can place and start them from the Director map.");
		m_Hints.AddHint(m_GmAutoActivateToggle, "Continue after an objective", "Automatically starts a staged objective, or the next unused map objective, when the current one is completed.");
		m_Hints.AddHint(m_GmAutoQrfToggle, "Automatic reinforcements", "Allows I&A to send enemy Quick Reaction Forces while players attack an objective. Turn it off if the Game Master will send them manually.");
		m_Hints.AddHint(m_GmAutoArtyToggle, "Automatic artillery", "Allows I&A to launch enemy artillery strikes while players attack an objective. Turn it off if the Game Master will control artillery.");
		m_Hints.AddHint(m_GmAutoSideToggle, "Automatic side missions", "Allows I&A to start side missions on its own. Turn it off if the Game Master will start them.");
		m_Hints.AddHint(m_GmAutoSupportToggle, "Automatic support sites", "Automatically adds enemy mortar pits and radio towers when an objective begins.");
		m_Hints.AddHint(openDirBtn, "Open the Director map", "Closes this menu and opens the map used to place, stage, and control objectives.");

		BuildDefensePage(runtime);

		scroll.AddChild(m_PageScaling);
		scroll.AddChild(m_PageCiv);
		scroll.AddChild(m_PageArty);
		scroll.AddChild(m_PageHq);
		scroll.AddChild(m_PageFactions);
		scroll.AddChild(m_PageQrf);
		scroll.AddChild(m_PageDirector);
		scroll.AddChild(m_PageDefense);

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
		m_Hints.AddHint(saveBtn, "Save", "Applies these settings to the current mission. They will not be remembered after a server restart unless you also use Save for restart.");
		m_Hints.AddHint(persistBtn, "Save for restart", "Applies these settings now and remembers them for future server restarts.");
		m_Hints.AddHint(clearBtn, "Clear saved settings", "Forgets the settings saved for future restarts. The server will return to the mission's normal settings after the next restart.");

		ref MUI_Row actionRow = runtime.CreateRow("actionRow");
		actionRow.SetGap(12);

		ref MUI_Button promoteBtn = runtime.CreateButton("Promote Self", "promote");
		promoteBtn.GetOnClicked().Insert(OnMikesPromoteSelf);

		ref MUI_Button completeBtn = runtime.CreateButton("Complete Zone", "complete");
		completeBtn.MakeDanger();
		completeBtn.GetOnClicked().Insert(OnMikesComplete);

		ref MUI_Button completeDefendBtn = runtime.CreateButton("Complete + Defend", "completeDef");
		completeDefendBtn.MakeDanger();
		completeDefendBtn.GetOnClicked().Insert(OnMikesCompleteAndDefend);

		ref MUI_Button helpBtn = runtime.CreateButton("Help", "help");
		helpBtn.GetOnClicked().Insert(OnAdminHelp);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMUIBack);

		actionRow.AddChild(helpBtn);
		actionRow.AddChild(promoteBtn);
		actionRow.AddChild(completeBtn);
		actionRow.AddChild(closeBtn);
		m_Hints.AddHint(promoteBtn, "Become Game Master", "Gives you Game Master access so you can use the Director map and other Game Master tools.");
		m_Hints.AddHint(completeBtn, "Complete the current objective", "Immediately marks the current objective as captured and moves the mission forward. Skips a defense even if one was placed.");

		ref MUI_Button seizeBaseBtn = runtime.CreateButton("Complete objectives + seize base", "seizeBase");
		seizeBaseBtn.MakeDanger();
		seizeBaseBtn.GetOnClicked().Insert(OnMikesCompleteAndSeizeBase);

		ref MUI_Row actionRow2 = runtime.CreateRow("actionRow2");
		actionRow2.SetGap(12);
		actionRow2.AddChild(completeDefendBtn);
		actionRow2.AddChild(seizeBaseBtn);
		m_Hints.AddHint(completeDefendBtn, "Complete and start field-base assault", "Finishes remaining required objectives and starts one field-base attempt. You still seize the command area, regroup, then defend that same site. If a base job is already running, this leaves it alone. If placement fails, an authored defense is forced when a Defend marker remains.");
		m_Hints.AddHint(seizeBaseBtn, "Complete objectives and seize a base", "Finishes remaining required objectives and starts one validated field-base attempt. Geometry and AI limits still apply. If placement fails, an authored defense is forced when a Defend marker remains.");

		footerBtns.AddChild(persistRow);
		footerBtns.AddChild(actionRow);
		footerBtns.AddChild(actionRow2);

		shell.GetCard().AddChild(m_Tabs);
		shell.GetCard().AddChild(scroll);
		shell.AddFooter(runtime, "Save applies now  •  Save for restart writes the server profile (applied after the mission .conf)", footerBtns);
		shell.Mount(runtime);
		shell.GetOverlay().AddChild(m_Hints);

		ShowAdminPage(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAdminHelp()
	{
		if (m_Hints)
			m_Hints.Toggle();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildDefensePage(notnull MUI_Runtime runtime)
	{
		if (!m_PageDefense)
			return;

		ref MUI_Label intro = runtime.CreateLabel("Applies to the next defense after an AO group completes. Enhanced is the default.", "defIntro");
		intro.SetFontSize(runtime.GetTheme().FONT_SMALL);
		intro.SetMuted(true);
		m_PageDefense.AddChild(intro);

		ref MUI_Label baseIntro = runtime.CreateLabel("Applies after this AO's required objectives; successful placement uses the captured base for defense.", "dynBaseIntro");
		baseIntro.SetFontSize(runtime.GetTheme().FONT_SMALL);
		baseIntro.SetMuted(true);
		m_PageDefense.AddChild(baseIntro);

		m_DynamicBaseEnabledToggle = runtime.CreateToggle("Dynamic field base after required objectives", "dynBaseOn");
		m_PageDefense.AddChild(m_DynamicBaseEnabledToggle);
		m_Hints.AddHint(m_DynamicBaseEnabledToggle, "Dynamic field base", "After required objectives, try to place a USSR field base to seize, regroup, and defend. Off uses the existing authored-defense roll.");

		m_DynamicBaseChanceField = runtime.CreateNumericField("Dynamic base chance (0 disables automatic selection)", "dynBaseChance");
		m_DynamicBaseChanceField.SetRange(0, 100);
		m_DynamicBaseChanceField.SetStep(1);
		m_DynamicBaseChanceField.SetDecimals(0);
		m_DynamicBaseChanceField.SetValue(100);
		m_PageDefense.AddChild(m_DynamicBaseChanceField);
		m_Hints.AddHint(m_DynamicBaseChanceField, "Selection chance", "0 disables automatic selection. 100 always attempts a legal site. Missed or failed placement returns to the authored 80% defense roll.");

		m_DynamicBaseInGmToggle = runtime.CreateToggle("Allow automatic dynamic base in Game Master mode", "dynBaseGm");
		m_PageDefense.AddChild(m_DynamicBaseInGmToggle);
		m_Hints.AddHint(m_DynamicBaseInGmToggle, "Game Master auto", "When Game Master mode is on, automatic base selection stays off unless this is enabled. The seize-base command still works.");

		ref MUI_Label sizeLbl = runtime.CreateLabel("Dynamic base size", "dynBaseSizeLbl");
		sizeLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		sizeLbl.SetMuted(true);
		m_PageDefense.AddChild(sizeLbl);
		m_DynamicBaseSizeDrop = runtime.CreateDropdown("dynBaseSize");
		m_DynamicBaseSizeDrop.AddItem("Auto");
		m_DynamicBaseSizeDrop.AddItem("Full");
		m_DynamicBaseSizeDrop.AddItem("Compact");
		m_DynamicBaseSizeDrop.SetIndex(0);
		m_PageDefense.AddChild(m_DynamicBaseSizeDrop);
		m_Hints.AddHint(m_DynamicBaseSizeDrop, "Base size", "Auto tries the full 180×140 m layout first, then compact. Full and Compact lock that footprint.");

		m_DefendLegacyToggle = runtime.CreateToggle("Use legacy defense (12-16 min, no events)", "defLegacy");
		m_PageDefense.AddChild(m_DefendLegacyToggle);
		m_Hints.AddHint(m_DefendLegacyToggle, "Legacy defense", "Turn this on to keep the original timer-only hold for the next defense. Leave it off for named doctrines and mini-objectives.");

		m_DefendDurMinField = runtime.CreateNumericField("Enhanced duration min (minutes)", "defDurMin");
		m_DefendDurMinField.SetRange(8, 40);
		m_DefendDurMinField.SetStep(1);
		m_DefendDurMinField.SetDecimals(0);
		m_DefendDurMinField.SetValue(18);

		m_DefendDurMaxField = runtime.CreateNumericField("Enhanced duration max (minutes)", "defDurMax");
		m_DefendDurMaxField.SetRange(8, 40);
		m_DefendDurMaxField.SetStep(1);
		m_DefendDurMaxField.SetDecimals(0);
		m_DefendDurMaxField.SetValue(22);

		m_DefendPrepMinField = runtime.CreateNumericField("PREPARE min (seconds)", "defPrepMin");
		m_DefendPrepMinField.SetRange(15, 300);
		m_DefendPrepMinField.SetStep(5);
		m_DefendPrepMinField.SetDecimals(0);
		m_DefendPrepMinField.SetValue(120);

		m_DefendPrepMaxField = runtime.CreateNumericField("PREPARE max (seconds)", "defPrepMax");
		m_DefendPrepMaxField.SetRange(15, 300);
		m_DefendPrepMaxField.SetStep(5);
		m_DefendPrepMaxField.SetDecimals(0);
		m_DefendPrepMaxField.SetValue(180);

		m_DefendEventsField = runtime.CreateNumericField("Guaranteed mini-objectives", "defEvents");
		m_DefendEventsField.SetRange(0, 3);
		m_DefendEventsField.SetStep(1);
		m_DefendEventsField.SetDecimals(0);
		m_DefendEventsField.SetValue(2);

		m_PageDefense.AddChild(m_DefendDurMinField);
		m_PageDefense.AddChild(m_DefendDurMaxField);
		m_PageDefense.AddChild(m_DefendPrepMinField);
		m_PageDefense.AddChild(m_DefendPrepMaxField);
		m_PageDefense.AddChild(m_DefendEventsField);
		m_Hints.AddHint(m_DefendDurMinField, "Shortest Enhanced hold", "Minimum minutes for an Enhanced defense clock after PREPARE.");
		m_Hints.AddHint(m_DefendDurMaxField, "Longest Enhanced hold", "Maximum minutes for an Enhanced defense clock after PREPARE.");
		m_Hints.AddHint(m_DefendPrepMinField, "Shortest PREPARE", "Minimum seconds before the main clock starts if players have not made contact.");
		m_Hints.AddHint(m_DefendPrepMaxField, "Longest PREPARE", "Maximum seconds the PREPARE phase can last before the clock starts automatically.");
		m_Hints.AddHint(m_DefendEventsField, "Guaranteed events", "How many optional mini-objectives always appear (max 3). A third event can still roll from the player-count chances below. Hunts never start in CRISIS.");

		m_DefendThirdLowField = runtime.CreateNumericField("Third-event chance (1-8 players)", "defThirdLow");
		m_DefendThirdLowField.SetRange(0, 1);
		m_DefendThirdLowField.SetStep(0.05);
		m_DefendThirdLowField.SetDecimals(2);
		m_DefendThirdLowField.SetValue(0.25);

		m_DefendThirdMidField = runtime.CreateNumericField("Third-event chance (9-16 players)", "defThirdMid");
		m_DefendThirdMidField.SetRange(0, 1);
		m_DefendThirdMidField.SetStep(0.05);
		m_DefendThirdMidField.SetDecimals(2);
		m_DefendThirdMidField.SetValue(0.50);

		m_DefendThirdHighField = runtime.CreateNumericField("Third-event chance (17+ players)", "defThirdHigh");
		m_DefendThirdHighField.SetRange(0, 1);
		m_DefendThirdHighField.SetStep(0.05);
		m_DefendThirdHighField.SetDecimals(2);
		m_DefendThirdHighField.SetValue(0.75);

		m_PageDefense.AddChild(m_DefendThirdLowField);
		m_PageDefense.AddChild(m_DefendThirdMidField);
		m_PageDefense.AddChild(m_DefendThirdHighField);
		m_Hints.AddHint(m_DefendThirdLowField, "Third event at low pop", "Chance of a third mini-objective when 1 to 8 players are connected.");
		m_Hints.AddHint(m_DefendThirdMidField, "Third event at mid pop", "Chance of a third mini-objective when 9 to 16 players are connected.");
		m_Hints.AddHint(m_DefendThirdHighField, "Third event at high pop", "Chance of a third mini-objective when 17 or more players are connected.");

		m_DefendPriSuccMinField = runtime.CreateNumericField("Priority success reduction min (sec)", "defPriSMin");
		m_DefendPriSuccMinField.SetRange(30, 600);
		m_DefendPriSuccMinField.SetStep(10);
		m_DefendPriSuccMinField.SetDecimals(0);
		m_DefendPriSuccMinField.SetValue(120);

		m_DefendPriSuccMaxField = runtime.CreateNumericField("Priority success reduction max (sec)", "defPriSMax");
		m_DefendPriSuccMaxField.SetRange(30, 600);
		m_DefendPriSuccMaxField.SetStep(10);
		m_DefendPriSuccMaxField.SetDecimals(0);
		m_DefendPriSuccMaxField.SetValue(240);

		m_DefendPriFailMinField = runtime.CreateNumericField("Priority timeout penalty min (sec)", "defPriFMin");
		m_DefendPriFailMinField.SetRange(30, 600);
		m_DefendPriFailMinField.SetStep(10);
		m_DefendPriFailMinField.SetDecimals(0);
		m_DefendPriFailMinField.SetValue(180);

		m_DefendPriFailMaxField = runtime.CreateNumericField("Priority timeout penalty max (sec)", "defPriFMax");
		m_DefendPriFailMaxField.SetRange(30, 600);
		m_DefendPriFailMaxField.SetStep(10);
		m_DefendPriFailMaxField.SetDecimals(0);
		m_DefendPriFailMaxField.SetValue(300);

		m_PageDefense.AddChild(m_DefendPriSuccMinField);
		m_PageDefense.AddChild(m_DefendPriSuccMaxField);
		m_PageDefense.AddChild(m_DefendPriFailMinField);
		m_PageDefense.AddChild(m_DefendPriFailMaxField);
		m_Hints.AddHint(m_DefendPriSuccMinField, "Priority success floor", "Shortest time removed from the hold clock when the HIGH PRIORITY event succeeds. Remaining time will not be cut below 6 minutes.");
		m_Hints.AddHint(m_DefendPriSuccMaxField, "Priority success ceiling", "Longest time removed from the hold clock when the HIGH PRIORITY event succeeds.");
		m_Hints.AddHint(m_DefendPriFailMinField, "Priority timeout floor", "Shortest extra time added when the HIGH PRIORITY deadline expires.");
		m_Hints.AddHint(m_DefendPriFailMaxField, "Priority timeout ceiling", "Longest extra time added when the HIGH PRIORITY deadline expires.");

		m_DefendDocSiegeToggle = runtime.CreateToggle("Siege doctrine", "defDocSiege");
		m_DefendDocBreakToggle = runtime.CreateToggle("Breakthrough doctrine", "defDocBreak");
		m_DefendDocAirToggle = runtime.CreateToggle("Air Assault doctrine", "defDocAir");
		m_DefendDocCmdToggle = runtime.CreateToggle("Command Offensive doctrine", "defDocCmd");
		m_DefendDocSiegeToggle.SetChecked(true);
		m_DefendDocBreakToggle.SetChecked(true);
		m_DefendDocAirToggle.SetChecked(true);
		m_DefendDocCmdToggle.SetChecked(true);
		m_PageDefense.AddChild(m_DefendDocSiegeToggle);
		m_PageDefense.AddChild(m_DefendDocBreakToggle);
		m_PageDefense.AddChild(m_DefendDocAirToggle);
		m_PageDefense.AddChild(m_DefendDocCmdToggle);
		m_Hints.AddHint(m_DefendDocSiegeToggle, "Siege", "Allows scout-to-spotting-team and deliberate infantry pushes. Disable the others to force this doctrine.");
		m_Hints.AddHint(m_DefendDocBreakToggle, "Breakthrough", "Allows motorized and mechanized dismounts. Needs a nearby road or it rerolls.");
		m_Hints.AddHint(m_DefendDocAirToggle, "Air Assault", "Allows HALO insertions with the ground attack. Needs an open LZ or it rerolls.");
		m_Hints.AddHint(m_DefendDocCmdToggle, "Command Offensive", "Allows mixed attacks directed from a commander FOB.");

		m_DefendEvtCmdToggle = runtime.CreateToggle("Commander FOB event", "defEvtCmd");
		m_DefendEvtEliteToggle = runtime.CreateToggle("Elite patrol event", "defEvtElite");
		m_DefendEvtScoutToggle = runtime.CreateToggle("Scout / spotting event", "defEvtScout");
		m_DefendEvtConvoyToggle = runtime.CreateToggle("Reinforcement convoy event", "defEvtConvoy");
		m_DefendEvtSniperToggle = runtime.CreateToggle("Sniper pair event", "defEvtSniper");
		m_DefendEvtCmdToggle.SetChecked(true);
		m_DefendEvtEliteToggle.SetChecked(true);
		m_DefendEvtScoutToggle.SetChecked(true);
		m_DefendEvtConvoyToggle.SetChecked(true);
		m_DefendEvtSniperToggle.SetChecked(true);
		m_PageDefense.AddChild(m_DefendEvtCmdToggle);
		m_PageDefense.AddChild(m_DefendEvtEliteToggle);
		m_PageDefense.AddChild(m_DefendEvtScoutToggle);
		m_PageDefense.AddChild(m_DefendEvtConvoyToggle);
		m_PageDefense.AddChild(m_DefendEvtSniperToggle);
		m_Hints.AddHint(m_DefendEvtCmdToggle, "Commander FOB", "Optional officer hunt. Success cancels the next major assault and one later event.");
		m_Hints.AddHint(m_DefendEvtEliteToggle, "Elite patrol", "Hunt a special-forces squad before it homes on gunfire.");
		m_Hints.AddHint(m_DefendEvtScoutToggle, "Scout then spotting team", "Destroy the scout before recon finishes or extra infantry squads are called onto the hold.");
		m_Hints.AddHint(m_DefendEvtConvoyToggle, "Convoy", "Destroy marked transports before they dismount extra troops at the line.");
		m_Hints.AddHint(m_DefendEvtSniperToggle, "Sniper pair", "Hunt the pair after they fire to remove precision overwatch.");

		m_DefendHotDropLabel = runtime.CreateLabel("Air Assault hot-drop chance  20%", "defHotLbl");
		m_DefendHotDropLabel.SetFontSize(runtime.GetTheme().FONT_SMALL);
		m_DefendHotDropLabel.SetMuted(true);
		m_DefendHotDropSlider = runtime.CreateSlider("defHotDrop");
		m_DefendHotDropSlider.SetRange(0, 1);
		m_DefendHotDropSlider.SetStep(0.01);
		m_DefendHotDropSlider.SetValue(0.20);
		m_DefendHotDropSlider.GetOnChanged().Insert(OnDefendHotDropChanged);
		m_PageDefense.AddChild(m_DefendHotDropLabel);
		m_PageDefense.AddChild(m_DefendHotDropSlider);
		m_Hints.AddHint(m_DefendHotDropSlider, "Hot-drop chance", "Chance an Air Assault drop lands inside or near the AO instead of a 150 to 300 meter perimeter LZ. Never on a player.");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDefendHotDropChanged()
	{
		if (!m_DefendHotDropSlider)
			return;
		float v = m_DefendHotDropSlider.GetValue();
		if (m_DefendHotDropLabel)
			m_DefendHotDropLabel.SetText(string.Format("Air Assault hot-drop chance  %1%%", Math.Round(v * 100.0)));
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
		m_Hints.AddHint(infLbl, "Enemy infantry factions", "Choose which factions future enemy foot soldiers will come from.");

		m_EnemyAutoToggle = runtime.CreateToggle("Auto-detect infantry factions", "enfAuto");
		m_EnemyAutoToggle.SetChecked(true);
		m_EnemyAutoToggle.GetOnChanged().Insert(OnEnemyAutoChanged);
		m_PageFactions.AddChild(m_EnemyAutoToggle);
		m_Hints.AddHint(m_EnemyAutoToggle, "Choose infantry automatically", "Turn this on to let I&A choose suitable enemy infantry factions. Turn it off to use your selections below.");

		m_EnemyFactionToggles = new array<ref MUI_Toggle>();
		m_EnemyFactionChoiceKeys = new array<string>();
		ref array<string> infNames = new array<string>();
		IA_AdminConfigUtil.CollectFactionChoices(EEntityCatalogType.CHARACTER, 1, m_EnemyFactionChoiceKeys, infNames);
		AddFactionToggles(runtime, m_PageFactions, "enf_", m_EnemyFactionChoiceKeys, infNames, m_EnemyFactionToggles, false);

		ref MUI_Label vehLbl = runtime.CreateLabel("Vehicle enemies. Match infantry unless you need a different catalog (common with mixed-faction mods).", "vehFacLbl");
		vehLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		vehLbl.SetMuted(true);
		m_PageFactions.AddChild(vehLbl);
		m_Hints.AddHint(vehLbl, "Enemy vehicle factions", "Choose which factions future enemy military vehicles will come from.");

		m_VehicleMatchToggle = runtime.CreateToggle("Vehicle factions match infantry", "vehMatch");
		m_VehicleMatchToggle.SetChecked(true);
		m_VehicleMatchToggle.GetOnChanged().Insert(OnVehicleMatchChanged);
		m_PageFactions.AddChild(m_VehicleMatchToggle);
		m_Hints.AddHint(m_VehicleMatchToggle, "Match infantry factions", "Turn this on to use the same enemy factions for vehicles and infantry. Turn it off to choose vehicle factions separately.");

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
		if (m_PageDefense)
			m_PageDefense.SetVisible(index == 7);
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

		if (m_DynamicBaseEnabledToggle)
			m_DynamicBaseEnabledToggle.SetChecked(cfg.m_bDynamicBaseEnabled);
		if (m_DynamicBaseChanceField)
			m_DynamicBaseChanceField.SetValue(cfg.m_iDynamicBaseChancePct);
		if (m_DynamicBaseInGmToggle)
			m_DynamicBaseInGmToggle.SetChecked(cfg.m_bDynamicBaseInGm);
		if (m_DynamicBaseSizeDrop)
		{
			int sizeIdx = cfg.m_iDynamicBaseSizeMode;
			if (sizeIdx < 0)
				sizeIdx = 0;
			if (sizeIdx > 2)
				sizeIdx = 0;
			m_DynamicBaseSizeDrop.SetIndex(sizeIdx);
		}
		if (m_DefendLegacyToggle)
			m_DefendLegacyToggle.SetChecked(cfg.m_bUseLegacyDefense);
		if (m_DefendDurMinField)
			m_DefendDurMinField.SetValue(cfg.m_iDefendDurationMinMin);
		if (m_DefendDurMaxField)
			m_DefendDurMaxField.SetValue(cfg.m_iDefendDurationMaxMin);
		if (m_DefendPrepMinField)
			m_DefendPrepMinField.SetValue(cfg.m_iDefendPrepareMinSec);
		if (m_DefendPrepMaxField)
			m_DefendPrepMaxField.SetValue(cfg.m_iDefendPrepareMaxSec);
		if (m_DefendEventsField)
			m_DefendEventsField.SetValue(cfg.m_iDefendGuaranteedEvents);
		if (m_DefendThirdLowField)
			m_DefendThirdLowField.SetValue(cfg.m_fDefendThirdChanceLow);
		if (m_DefendThirdMidField)
			m_DefendThirdMidField.SetValue(cfg.m_fDefendThirdChanceMid);
		if (m_DefendThirdHighField)
			m_DefendThirdHighField.SetValue(cfg.m_fDefendThirdChanceHigh);
		if (m_DefendPriSuccMinField)
			m_DefendPriSuccMinField.SetValue(cfg.m_iDefendPrioritySuccessMinSec);
		if (m_DefendPriSuccMaxField)
			m_DefendPriSuccMaxField.SetValue(cfg.m_iDefendPrioritySuccessMaxSec);
		if (m_DefendPriFailMinField)
			m_DefendPriFailMinField.SetValue(cfg.m_iDefendPriorityFailMinSec);
		if (m_DefendPriFailMaxField)
			m_DefendPriFailMaxField.SetValue(cfg.m_iDefendPriorityFailMaxSec);
		if (m_DefendDocSiegeToggle)
			m_DefendDocSiegeToggle.SetChecked(cfg.m_bDefendDoctrineSiege);
		if (m_DefendDocBreakToggle)
			m_DefendDocBreakToggle.SetChecked(cfg.m_bDefendDoctrineBreakthrough);
		if (m_DefendDocAirToggle)
			m_DefendDocAirToggle.SetChecked(cfg.m_bDefendDoctrineAirAssault);
		if (m_DefendDocCmdToggle)
			m_DefendDocCmdToggle.SetChecked(cfg.m_bDefendDoctrineCommand);
		if (m_DefendEvtCmdToggle)
			m_DefendEvtCmdToggle.SetChecked(cfg.m_bDefendEventCommander);
		if (m_DefendEvtEliteToggle)
			m_DefendEvtEliteToggle.SetChecked(cfg.m_bDefendEventElite);
		if (m_DefendEvtScoutToggle)
			m_DefendEvtScoutToggle.SetChecked(cfg.m_bDefendEventScoutMortar);
		if (m_DefendEvtConvoyToggle)
			m_DefendEvtConvoyToggle.SetChecked(cfg.m_bDefendEventConvoy);
		if (m_DefendEvtSniperToggle)
			m_DefendEvtSniperToggle.SetChecked(cfg.m_bDefendEventSniper);
		if (m_DefendHotDropSlider)
		{
			m_DefendHotDropSlider.SetValue(cfg.m_fDefendHotDropChance);
			OnDefendHotDropChanged();
		}

		if (m_NormalSkillDrop)
			m_NormalSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(cfg.GetAiSkillNormal()));
		if (m_EliteSkillDrop)
			m_EliteSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(cfg.GetAiSkillElite()));
		if (m_NormalFireField)
			m_NormalFireField.SetValue(cfg.m_fAiFireRateNormal);
		if (m_EliteFireField)
			m_EliteFireField.SetValue(cfg.m_fAiFireRateElite);
		if (m_NormalPercField)
			m_NormalPercField.SetValue(cfg.m_fAiPerceptionNormal);
		if (m_ElitePercField)
			m_ElitePercField.SetValue(cfg.m_fAiPerceptionElite);

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

		ref IA_Config defendPack = new IA_Config();
		if (m_DefendLegacyToggle)
			defendPack.m_bUseLegacyDefense = m_DefendLegacyToggle.IsChecked();
		if (m_DefendDurMinField)
			defendPack.m_iDefendDurationMinMin = Math.Round(m_DefendDurMinField.GetValue());
		if (m_DefendDurMaxField)
			defendPack.m_iDefendDurationMaxMin = Math.Round(m_DefendDurMaxField.GetValue());
		if (m_DefendPrepMinField)
			defendPack.m_iDefendPrepareMinSec = Math.Round(m_DefendPrepMinField.GetValue());
		if (m_DefendPrepMaxField)
			defendPack.m_iDefendPrepareMaxSec = Math.Round(m_DefendPrepMaxField.GetValue());
		if (m_DefendEventsField)
			defendPack.m_iDefendGuaranteedEvents = Math.Round(m_DefendEventsField.GetValue());
		if (m_DefendThirdLowField)
			defendPack.m_fDefendThirdChanceLow = m_DefendThirdLowField.GetValue();
		if (m_DefendThirdMidField)
			defendPack.m_fDefendThirdChanceMid = m_DefendThirdMidField.GetValue();
		if (m_DefendThirdHighField)
			defendPack.m_fDefendThirdChanceHigh = m_DefendThirdHighField.GetValue();
		if (m_DefendPriSuccMinField)
			defendPack.m_iDefendPrioritySuccessMinSec = Math.Round(m_DefendPriSuccMinField.GetValue());
		if (m_DefendPriSuccMaxField)
			defendPack.m_iDefendPrioritySuccessMaxSec = Math.Round(m_DefendPriSuccMaxField.GetValue());
		if (m_DefendPriFailMinField)
			defendPack.m_iDefendPriorityFailMinSec = Math.Round(m_DefendPriFailMinField.GetValue());
		if (m_DefendPriFailMaxField)
			defendPack.m_iDefendPriorityFailMaxSec = Math.Round(m_DefendPriFailMaxField.GetValue());
		if (m_DefendDocSiegeToggle)
			defendPack.m_bDefendDoctrineSiege = m_DefendDocSiegeToggle.IsChecked();
		if (m_DefendDocBreakToggle)
			defendPack.m_bDefendDoctrineBreakthrough = m_DefendDocBreakToggle.IsChecked();
		if (m_DefendDocAirToggle)
			defendPack.m_bDefendDoctrineAirAssault = m_DefendDocAirToggle.IsChecked();
		if (m_DefendDocCmdToggle)
			defendPack.m_bDefendDoctrineCommand = m_DefendDocCmdToggle.IsChecked();
		if (m_DefendEvtCmdToggle)
			defendPack.m_bDefendEventCommander = m_DefendEvtCmdToggle.IsChecked();
		if (m_DefendEvtEliteToggle)
			defendPack.m_bDefendEventElite = m_DefendEvtEliteToggle.IsChecked();
		if (m_DefendEvtScoutToggle)
			defendPack.m_bDefendEventScoutMortar = m_DefendEvtScoutToggle.IsChecked();
		if (m_DefendEvtConvoyToggle)
			defendPack.m_bDefendEventConvoy = m_DefendEvtConvoyToggle.IsChecked();
		defendPack.m_bDefendEventRelay = false;
		if (m_DefendEvtSniperToggle)
			defendPack.m_bDefendEventSniper = m_DefendEvtSniperToggle.IsChecked();
		if (m_DefendHotDropSlider)
			defendPack.m_fDefendHotDropChance = m_DefendHotDropSlider.GetValue();
		packed = packed + "|" + IA_Config.PackDefenseExtras(defendPack);

		ref IA_Config combatPack = new IA_Config();
		if (m_NormalSkillDrop)
			combatPack.m_eAiSkillNormal = IA_Config.MenuIndexToSkill(m_NormalSkillDrop.GetIndex());
		if (m_EliteSkillDrop)
			combatPack.m_eAiSkillElite = IA_Config.MenuIndexToSkill(m_EliteSkillDrop.GetIndex());
		if (m_NormalFireField)
			combatPack.m_fAiFireRateNormal = m_NormalFireField.GetValue();
		if (m_EliteFireField)
			combatPack.m_fAiFireRateElite = m_EliteFireField.GetValue();
		if (m_NormalPercField)
			combatPack.m_fAiPerceptionNormal = m_NormalPercField.GetValue();
		if (m_ElitePercField)
			combatPack.m_fAiPerceptionElite = m_ElitePercField.GetValue();
		packed = packed + "|" + IA_Config.PackAiCombatExtras(combatPack);

		ref IA_Config basePack = new IA_Config();
		IA_Config live = IA_MissionInitializer.GetGlobalConfig();
		if (live)
			IA_Config.UnpackDynamicBaseExtras(basePack, IA_Config.PackDynamicBaseExtras(live));
		if (m_DynamicBaseEnabledToggle)
			basePack.m_bDynamicBaseEnabled = m_DynamicBaseEnabledToggle.IsChecked();
		if (m_DynamicBaseChanceField)
			basePack.m_iDynamicBaseChancePct = Math.Round(m_DynamicBaseChanceField.GetValue());
		if (m_DynamicBaseInGmToggle)
			basePack.m_bDynamicBaseInGm = m_DynamicBaseInGmToggle.IsChecked();
		if (m_DynamicBaseSizeDrop)
			basePack.m_iDynamicBaseSizeMode = m_DynamicBaseSizeDrop.GetIndex();
		packed = packed + "|" + IA_Config.PackDynamicBaseExtras(basePack);

		IA_MissionInitializer.SubmitPackedAdminConfig(packed, persist);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillSkillDropdown(MUI_Dropdown drop)
	{
		if (!drop)
			return;
		drop.AddItem("Rookie");
		drop.AddItem("Regular");
		drop.AddItem("Veteran");
		drop.AddItem("Expert");
		drop.AddItem("Cylon");
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

	//------------------------------------------------------------------------------------------------
	protected void OnMikesCompleteAndDefend()
	{
		IA_MissionInitializer.ForceCompleteZoneAndDefend();
		GetGame().GetMenuManager().CloseMenu(this);
	}

	protected void OnMikesCompleteAndSeizeBase()
	{
		IA_MissionInitializer.ForceCompleteObjectivesAndSeizeBase();
		GetGame().GetMenuManager().CloseMenu(this);
	}
}
