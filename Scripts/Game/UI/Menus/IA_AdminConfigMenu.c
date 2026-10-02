//------------------------------------------------------------------------------------------------
//! Runtime admin tuning for IA_Config. Live source of truth; the mission .conf is a
//! workshop baseline only. Uses MUI blank layout (no legacy widget path).
//------------------------------------------------------------------------------------------------
class IA_AdminConfigMenu : MUI_MenuBase
{
	protected static const float FRAME_W = 1120;
	protected static const float SCROLL_H = 430;
	protected static const float PREVIEW_BTN_W = 340;

	protected ref IA_UplinkFrame m_Frame;
	protected ref IA_UplinkTabs m_Tabs;
	protected ref IA_UplinkScroll m_Scroll;
	protected ref MUI_Panel m_PageScaling;
	protected ref MUI_Panel m_PageCiv;
	protected ref MUI_Panel m_PageArty;
	protected ref MUI_Panel m_PageHq;
	protected ref MUI_Panel m_PageFactions;
	protected ref MUI_Panel m_PageQrf;
	protected ref MUI_Panel m_PageDirector;
	protected ref MUI_Panel m_PageDefense;
	protected ref MUI_Panel m_PageDynamicAI;

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
	protected ref MUI_Toggle m_DynamicAISpawningToggle;
	protected ref MUI_NumericField m_DynamicAIScaleField;
	protected ref MUI_NumericField m_DynamicAIBudgetField;
	protected ref MUI_NumericField m_DynamicAIWakeField;
	protected ref MUI_NumericField m_DynamicAICacheField;
	protected ref MUI_NumericField m_DynamicAICloseField;
	protected ref MUI_NumericField m_DynamicAIReleaseField;
	protected ref MUI_NumericField m_DynamicAICacheQuietField;
	protected ref MUI_NumericField m_DynamicAICombatQuietField;
	protected ref MUI_NumericField m_DynamicAIMinLiveField;
	protected ref MUI_NumericField m_DynamicAIEvictDelayField;
	protected ref MUI_NumericField m_DynamicAIRetentionField;
	protected ref MUI_NumericField m_DynamicAICaptureSeedField;
	protected ref MUI_Toggle m_DynamicAIHardCapToggle;
	protected ref MUI_NumericField m_MilVehField;
	protected ref IA_UplinkChoice m_NormalSkillDrop;
	protected ref IA_UplinkChoice m_EliteSkillDrop;
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
	protected ref MUI_Toggle m_DynamicBaseEmplacementsToggle;
	protected ref MUI_NumericField m_DynamicBaseChanceField;
	protected ref MUI_Toggle m_DynamicBaseInGmToggle;
	protected ref IA_UplinkChoice m_DynamicBaseSizeDrop;

#ifdef WORKBENCH
	// The open menu, for a Workbench probe to turn its pages without a pointer.
	protected static IA_AdminConfigMenu s_Probe;
#endif

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (!IsMUIOpen())
			return;

		PopulateFromConfig();
#ifdef WORKBENCH
		s_Probe = this;
#endif

		// A gamepad starts on the section rail.
		MUI_Runtime runtime = GetRuntime();
		InputManager input = GetGame().GetInputManager();
		if (runtime && input && !input.IsUsingMouseAndKeyboard())
			runtime.FocusNode(m_Tabs);
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_Probe == this)
			s_Probe = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	static bool ProbeTab(int index)
	{
		if (!s_Probe || !s_Probe.m_Tabs)
			return false;
		s_Probe.m_Tabs.SetIndex(index);
		return s_Probe.m_Tabs.GetIndex() == index;
	}

	//------------------------------------------------------------------------------------------------
	static void ProbeScroll(float y)
	{
		if (s_Probe && s_Probe.m_Scroll)
			s_Probe.m_Scroll.SetScrollY(y);
	}

	//------------------------------------------------------------------------------------------------
	//! \return whether the help overlay is open after the toggle
	static bool ProbeHelp()
	{
		if (!s_Probe || !s_Probe.m_Hints)
			return false;
		s_Probe.OnAdminHelp();
		return s_Probe.m_Hints.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! \return how many controls the open page offers, the rail and the action bar included
	static int ProbeControls()
	{
		if (!s_Probe)
			return 0;
		MUI_Runtime runtime = s_Probe.GetRuntime();
		if (!runtime)
			return 0;
		ref array<MUI_Node> found = {};
		runtime.CollectFocusables(found);
		return found.Count();
	}
#endif

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
		IA_UplinkStyle look = IA_UplinkStyle.Get();

		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.SetFill(Color.FromInt(0));
		overlay.SetIntro(0, 0.35, 0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");
		fx.SetIntro(0, 0.55, 0);

		m_Frame = IA_UplinkFrame.Create(runtime, "frame", FRAME_W, "COMMAND UPLINK", "ADMIN CONFIG", "Live source of truth  •  Changes apply on Save");
		m_Frame.SetStatus("LIVE CONFIG", look.m_Green, false);

		m_Hints = runtime.CreateHintLayer("hints");

		m_Tabs = IA_UplinkTabs.Create(runtime, "tabs");
		m_Tabs.AddTab("Scaling");
		m_Tabs.AddTab("Civilians");
		m_Tabs.AddTab("Artillery");
		m_Tabs.AddTab("HQ");
		m_Tabs.AddTab("Factions");
		m_Tabs.AddTab("QRF");
		m_Tabs.AddTab("Director");
		m_Tabs.AddTab("Defense");
		m_Tabs.AddTab("Dynamic AI");
		m_Tabs.GetOnChanged().Insert(OnAdminTabChanged);

		m_Scroll = IA_UplinkScroll.Create(runtime, "scroll");
		m_Scroll.SetViewportHeight(SCROLL_H);

		m_PageScaling = MakePage(runtime, "pageScaling");
		m_PageCiv = MakePage(runtime, "pageCiv");
		m_PageArty = MakePage(runtime, "pageArty");
		m_PageHq = MakePage(runtime, "pageHq");
		m_PageFactions = MakePage(runtime, "pageFactions");
		m_PageQrf = MakePage(runtime, "pageQrf");
		m_PageDirector = MakePage(runtime, "pageDirector");
		m_PageDefense = MakePage(runtime, "pageDefense");
		m_PageDynamicAI = MakePage(runtime, "pageDynamicAI");

		m_AIField = IA_UplinkField.Create(runtime, "AI scale multiplier", "ai");
		m_AIField.SetRange(0.1, 10);
		m_AIField.SetStep(0.1);
		m_AIField.SetDecimals(2);

		m_StaticAIField = IA_UplinkField.Create(runtime, "Static AI scale override (0 = dynamic)", "staticAi");
		m_StaticAIField.SetRange(0, 20);
		m_StaticAIField.SetStep(0.1);
		m_StaticAIField.SetDecimals(2);

		m_MilVehField = IA_UplinkField.Create(runtime, "Military vehicle count multiplier", "milVeh");
		m_MilVehField.SetRange(0, 10);
		m_MilVehField.SetStep(0.1);
		m_MilVehField.SetDecimals(2);

		AddCaption(runtime, m_PageScaling, "Enemy strength", "strengthCap");
		AddPair(runtime, m_PageScaling, "aiRow", m_AIField, m_StaticAIField);
		AddPair(runtime, m_PageScaling, "milVehRow", m_MilVehField, null);
		m_Hints.AddHint(m_Tabs, "Settings pages", "Choose a tab to view a different group of settings. Help updates to explain the open tab.");
		m_Hints.AddHint(m_AIField, "Enemy strength", "Multiplies player-based AI scaling while Dynamic AI Spawning is OFF and no static override is set. Dynamic AI Spawning ON uses the Dynamic AI scale on that tab; this saved value is kept for when it is switched OFF.");
		m_Hints.AddHint(m_StaticAIField, "Fixed enemy strength", "When Dynamic AI Spawning is OFF, a value above 0 replaces player-based AI scaling; 0 uses normal scaling. Dynamic AI Spawning ON uses the Dynamic AI scale on that tab and keeps this saved override for when it is switched OFF.");
		m_Hints.AddHint(m_MilVehField, "Enemy vehicle count", "Changes how many enemy military vehicles appear. 1 is normal, 0.5 is about half, and 2 is about double.");

		ref IA_UplinkNote combatLbl = IA_UplinkNote.Create(runtime, "Skill is aim accuracy only. Fire rate and spotting are separate.", "combatLbl");
		AddCaption(runtime, m_PageScaling, "AI combat", "combatCap");
		m_PageScaling.AddChild(combatLbl);

		m_NormalSkillDrop = IA_UplinkChoice.Create(runtime, "Normal troop skill", "normalSkill");
		FillSkillDropdown(m_NormalSkillDrop);
		m_NormalSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(EAISkill.VETERAN));

		m_EliteSkillDrop = IA_UplinkChoice.Create(runtime, "Elite troop skill", "eliteSkill");
		FillSkillDropdown(m_EliteSkillDrop);
		m_EliteSkillDrop.SetIndex(IA_Config.SkillToMenuIndex(EAISkill.EXPERT));

		m_NormalFireField = IA_UplinkField.Create(runtime, "Normal fire rate (1 = vanilla)", "nFire");
		m_NormalFireField.SetRange(0.05, 2);
		m_NormalFireField.SetStep(0.05);
		m_NormalFireField.SetDecimals(2);
		m_NormalFireField.SetValue(1);

		m_EliteFireField = IA_UplinkField.Create(runtime, "Elite fire rate", "eFire");
		m_EliteFireField.SetRange(0.05, 2);
		m_EliteFireField.SetStep(0.05);
		m_EliteFireField.SetDecimals(2);
		m_EliteFireField.SetValue(1.25);

		m_NormalPercField = IA_UplinkField.Create(runtime, "Normal spotting (1 = vanilla)", "nPerc");
		m_NormalPercField.SetRange(0.1, 4);
		m_NormalPercField.SetStep(0.1);
		m_NormalPercField.SetDecimals(2);
		m_NormalPercField.SetValue(1);

		m_ElitePercField = IA_UplinkField.Create(runtime, "Elite spotting", "ePerc");
		m_ElitePercField.SetRange(0.1, 4);
		m_ElitePercField.SetStep(0.1);
		m_ElitePercField.SetDecimals(2);
		m_ElitePercField.SetValue(1.5);

		AddPair(runtime, m_PageScaling, "skillRow", m_NormalSkillDrop, m_EliteSkillDrop);
		AddPair(runtime, m_PageScaling, "fireRow", m_NormalFireField, m_EliteFireField);
		AddPair(runtime, m_PageScaling, "percRow", m_NormalPercField, m_ElitePercField);
		m_Hints.AddHint(m_NormalSkillDrop, "Normal aim skill", "Aim accuracy for regular infantry, including commander FOB guards that are not marked elite. Veteran is the I&A default. Does not change reaction time.");
		m_Hints.AddHint(m_EliteSkillDrop, "Elite aim skill", "Aim accuracy for elite patrols and the elite fireteam at a commander FOB. Expert is the I&A default. Cylon (Cyclone) is perfect aim. Does not change reaction time.");
		m_Hints.AddHint(m_NormalFireField, "Normal fire rate", "How quickly regular infantry shoot. 1 is vanilla. Values above 1 shoot faster. Vanilla clamps 0.05 to 2.");
		m_Hints.AddHint(m_EliteFireField, "Elite fire rate", "How quickly elite infantry shoot. Default 1.25.");
		m_Hints.AddHint(m_NormalPercField, "Normal spotting", "How quickly regular infantry visually detect targets. 1 is vanilla.");
		m_Hints.AddHint(m_ElitePercField, "Elite spotting", "How quickly elite infantry visually detect targets. Default 1.5.");

		m_civField = IA_UplinkField.Create(runtime, "Civilian count multiplier", "civ");
		m_civField.SetRange(0, 100);
		m_civField.SetStep(0.1);
		m_civField.SetDecimals(2);

		m_CivVehField = IA_UplinkField.Create(runtime, "Civilian vehicle multiplier", "civVeh");
		m_CivVehField.SetRange(0, 10);
		m_CivVehField.SetStep(0.1);
		m_CivVehField.SetDecimals(2);

		m_RevoltField = IA_UplinkField.Create(runtime, "Revolt threshold (0-1)", "revolt");
		m_RevoltField.SetRange(0, 1);
		m_RevoltField.SetStep(0.01);
		m_RevoltField.SetDecimals(2);

		m_RevoltNotifField = IA_UplinkField.Create(runtime, "Revolt notification delay (seconds)", "revoltNotif");
		m_RevoltNotifField.SetRange(0, 600);
		m_RevoltNotifField.SetStep(1);
		m_RevoltNotifField.SetDecimals(0);
		m_RevoltNotifField.SetValue(30);

		m_RevoltReinfField = IA_UplinkField.Create(runtime, "Revolt reinforcement delay (seconds)", "revoltReinf");
		m_RevoltReinfField.SetRange(0, 3600);
		m_RevoltReinfField.SetStep(5);
		m_RevoltReinfField.SetDecimals(0);
		m_RevoltReinfField.SetValue(180);

		m_CivSpawnToggle = IA_UplinkToggle.Create(runtime, "Enable civilian spawning", "civSpawn");

		AddCaption(runtime, m_PageCiv, "Population", "civCap");
		AddPair(runtime, m_PageCiv, "civSpawnRow", m_CivSpawnToggle, null);
		AddPair(runtime, m_PageCiv, "civRow", m_civField, m_CivVehField);
		AddCaption(runtime, m_PageCiv, "Revolt", "revoltCap");
		AddPair(runtime, m_PageCiv, "revoltRow", m_RevoltField, m_RevoltNotifField);
		AddPair(runtime, m_PageCiv, "revoltReinfRow", m_RevoltReinfField, null);
		m_Hints.AddHint(m_civField, "Civilian population", "Changes how many civilians appear. 1 is normal, 0.5 is about half, and 2 is about double.");
		m_Hints.AddHint(m_CivVehField, "Civilian traffic", "Changes how many civilian vehicles appear. 1 is normal, 0.5 is about half, and 2 is about double.");
		m_Hints.AddHint(m_RevoltField, "When a revolt begins", "Sets how many civilians players can kill before a revolt starts. For example, 0.11 means 11 percent.");
		m_Hints.AddHint(m_RevoltNotifField, "Revolt warning delay", "How many seconds pass before players are warned that a revolt has started.");
		m_Hints.AddHint(m_RevoltReinfField, "Revolt reinforcement delay", "How many seconds pass before extra resistance fighters arrive to support the revolt.");
		m_Hints.AddHint(m_CivSpawnToggle, "Civilian spawning", "Controls whether civilians appear in areas captured by players.");

		m_artyField = IA_UplinkField.Create(runtime, "Time between strikes (seconds)", "arty");
		m_artyField.SetRange(0, 3600);
		m_artyField.SetStep(10);
		m_artyField.SetDecimals(0);

		m_ArtyMinField = IA_UplinkField.Create(runtime, "Smoke to impact min (seconds)", "artyMin");
		m_ArtyMinField.SetRange(0, 600);
		m_ArtyMinField.SetStep(1);
		m_ArtyMinField.SetDecimals(0);

		m_ArtyMaxField = IA_UplinkField.Create(runtime, "Smoke to impact max (seconds)", "artyMax");
		m_ArtyMaxField.SetRange(0, 600);
		m_ArtyMaxField.SetStep(1);
		m_ArtyMaxField.SetDecimals(0);

		ref IA_UplinkNote artyDelayLbl = IA_UplinkNote.Create(runtime, "After warning smoke, each strike waits a random time in this range before rounds land. Separate from the time between strikes.", "artyDelayLbl");

		m_ArtyChanceLabel = IA_UplinkNote.Create(runtime, "Strike chance  18%", "artyChanceLbl");
		m_ArtyChanceLabel.SetColor(look.m_Gold);

		m_ArtyChanceSlider = IA_UplinkSlider.Create(runtime, "artyChance");
		m_ArtyChanceSlider.SetRange(0, 1);
		m_ArtyChanceSlider.SetStep(0.01);
		m_ArtyChanceSlider.SetValue(0.18);
		m_ArtyChanceSlider.GetOnChanged().Insert(OnArtyChanceChanged);

		m_ArtyChanceProgress = IA_UplinkMeter.Create(runtime, "artyChanceBar");
		m_ArtyChanceProgress.SetValue(0.18);

		AddCaption(runtime, m_PageArty, "Strike timing", "artyCap");
		AddPair(runtime, m_PageArty, "artyRow", m_artyField, null);
		AddPair(runtime, m_PageArty, "artyDelayRow", m_ArtyMinField, m_ArtyMaxField);
		m_PageArty.AddChild(artyDelayLbl);
		AddCaption(runtime, m_PageArty, "Strike chance", "artyChanceCap");
		m_PageArty.AddChild(m_ArtyChanceLabel);
		m_PageArty.AddChild(m_ArtyChanceSlider);
		m_PageArty.AddChild(m_ArtyChanceProgress);
		m_Hints.AddHint(m_artyField, "Time between strikes", "The minimum number of seconds after one artillery strike before another can begin.");
		m_Hints.AddHint(m_ArtyMinField, "Shortest warning time", "The shortest possible delay between the red warning smoke and the incoming rounds.");
		m_Hints.AddHint(m_ArtyMaxField, "Longest warning time", "The longest possible delay between the red warning smoke and the incoming rounds.");
		m_Hints.AddHint(m_ArtyChanceSlider, "Chance of a strike", "Controls how often enemy artillery attacks while players are fighting at an objective. A higher value means more frequent strikes.");

		m_heliToggle = IA_UplinkToggle.Create(runtime, "Disable HQ helipads", "heli");
		m_groundToggle = IA_UplinkToggle.Create(runtime, "Disable HQ ground vehicles", "ground");
		m_RolesToggle = IA_UplinkToggle.Create(runtime, "Enforce pilot role restrictions", "roles");

		m_HaloMaxField = IA_UplinkField.Create(runtime, "HALO Jump max players (0 = off)", "haloMax");
		m_HaloMaxField.SetRange(0, 128);
		m_HaloMaxField.SetStep(1);
		m_HaloMaxField.SetDecimals(0);
		m_HaloMaxField.SetValue(IA_Config.HALO_JUMP_MAX_PLAYERS_DEFAULT);

		ref IA_UplinkNote hqPrefabLbl = IA_UplinkNote.Create(runtime, "HQ vehicle prefab lists still come from the mission .conf (Workbench). Everything else is controlled here.", "hqPrefabLbl");

		AddCaption(runtime, m_PageHq, "Headquarters", "hqCap");
		AddPair(runtime, m_PageHq, "hqVehRow", m_heliToggle, m_groundToggle);
		AddPair(runtime, m_PageHq, "hqRoleRow", m_RolesToggle, null);
		AddPair(runtime, m_PageHq, "haloRow", m_HaloMaxField, null);
		m_PageHq.AddChild(hqPrefabLbl);
		m_Hints.AddHint(m_heliToggle, "Disable HQ helicopters", "Turn this on to stop helicopters from appearing at the player HQ.");
		m_Hints.AddHint(m_groundToggle, "Disable HQ ground vehicles", "Turn this on to stop ground vehicles from appearing at the player HQ.");
		m_Hints.AddHint(m_RolesToggle, "Require pilot roles", "Turn this on to prevent players without a pilot role from flying restricted aircraft.");
		m_Hints.AddHint(m_HaloMaxField, "HALO player limit", "HALO jumps are available only while the connected player count is below this number. Set it to 0 to disable HALO jumps.");

		BuildPilotPreviewSection(runtime);
		BuildFactionPage(runtime);

		ref IA_UplinkNote qrfLbl = IA_UplinkNote.Create(runtime, "Spawn QRF through the normal mission path", "qrfLbl");

		m_QrfInfantryBtn = IA_UplinkButton.Create(runtime, "Infantry QRF", "qrfInf");
		m_QrfInfantryBtn.GetOnClicked().Insert(OnQrfInfantry);
		m_QrfMotorizedBtn = IA_UplinkButton.Create(runtime, "Motorized QRF", "qrfMotor");
		m_QrfMotorizedBtn.GetOnClicked().Insert(OnQrfMotorized);
		m_QrfMechanizedBtn = IA_UplinkButton.Create(runtime, "Mechanized QRF", "qrfMech");
		m_QrfMechanizedBtn.GetOnClicked().Insert(OnQrfMechanized);
		m_QrfArmouredBtn = IA_UplinkButton.Create(runtime, "Armoured QRF", "qrfArmour");
		m_QrfArmouredBtn.GetOnClicked().Insert(OnQrfArmoured);
		m_QrfAirborneBtn = IA_UplinkButton.Create(runtime, "Airborne QRF", "qrfAir");
		m_QrfAirborneBtn.MakeAccent();
		m_QrfAirborneBtn.GetOnClicked().Insert(OnQrfAirborne);

		ref MUI_Row qrfRow1 = runtime.CreateRow("qrfRow1");
		qrfRow1.SetGap(10);
		qrfRow1.AddChild(m_QrfInfantryBtn);
		qrfRow1.AddChild(m_QrfMotorizedBtn);
		qrfRow1.AddChild(m_QrfMechanizedBtn);

		ref MUI_Row qrfRow2 = runtime.CreateRow("qrfRow2");
		qrfRow2.SetGap(10);
		qrfRow2.AddChild(m_QrfArmouredBtn);
		qrfRow2.AddChild(m_QrfAirborneBtn);

		AddCaption(runtime, m_PageQrf, "Quick reaction force", "qrfCap");
		m_PageQrf.AddChild(qrfLbl);
		m_PageQrf.AddChild(qrfRow1);
		m_PageQrf.AddChild(qrfRow2);
		m_Hints.AddHint(qrfLbl, "Enemy reinforcements", "Send an enemy Quick Reaction Force toward the current objective. Choose the type of force with the buttons below.");
		m_Hints.AddHint(m_QrfInfantryBtn, "Infantry QRF", "Sends enemy soldiers on foot toward the objective.");
		m_Hints.AddHint(m_QrfMotorizedBtn, "Motorized QRF", "Sends enemy soldiers in trucks and light vehicles.");
		m_Hints.AddHint(m_QrfMechanizedBtn, "Mechanized QRF", "Sends enemy infantry supported by APCs or IFVs.");
		m_Hints.AddHint(m_QrfArmouredBtn, "Armoured QRF", "Sends tanks supported by infantry.");
		m_Hints.AddHint(m_QrfAirborneBtn, "Airborne QRF", "Drops enemy paratroopers near the objective after a short warning.");

		ref IA_UplinkNote gmModeLbl = IA_UplinkNote.Create(runtime, "Place and activate sites from the Director map instead.", "gmModeLbl");

		m_GmModeToggle = IA_UplinkToggle.Create(runtime, "Don't auto-start map AOs", "gmMode");
		m_GmAutoActivateToggle = IA_UplinkToggle.Create(runtime, "After Live: start Staging, or the next map AO", "gmAutoAct");
		m_GmAutoQrfToggle = IA_UplinkToggle.Create(runtime, "Auto QRF on Live AOs", "gmAutoQrf");
		m_GmAutoArtyToggle = IA_UplinkToggle.Create(runtime, "Auto artillery on Live AOs", "gmAutoArty");
		m_GmAutoSideToggle = IA_UplinkToggle.Create(runtime, "Auto side missions", "gmAutoSide");
		m_GmAutoSupportToggle = IA_UplinkToggle.Create(runtime, "Auto mortar + radio on Activate", "gmAutoSup");

		ref MUI_Button openDirBtn = IA_UplinkButton.Create(runtime, "Open Director Map", "openDir");
		openDirBtn.MakeAccent();
		openDirBtn.GetOnClicked().Insert(OnOpenDirector);

		openDirBtn.SetMinWidth(280);

		AddCaption(runtime, m_PageDirector, "Game Master direction", "gmCap");
		m_PageDirector.AddChild(gmModeLbl);
		AddPair(runtime, m_PageDirector, "gmRow1", m_GmModeToggle, m_GmAutoActivateToggle);
		AddPair(runtime, m_PageDirector, "gmRow2", m_GmAutoQrfToggle, m_GmAutoArtyToggle);
		AddPair(runtime, m_PageDirector, "gmRow3", m_GmAutoSideToggle, m_GmAutoSupportToggle);
		m_PageDirector.AddChild(openDirBtn);
		m_Hints.AddHint(m_GmModeToggle, "Manual objectives", "Stops I&A from choosing and starting objectives automatically. A Game Master can place and start them from the Director map.");
		m_Hints.AddHint(m_GmAutoActivateToggle, "Continue after an objective", "Automatically starts a staged objective, or the next unused map objective, when the current one is completed.");
		m_Hints.AddHint(m_GmAutoQrfToggle, "Automatic reinforcements", "Allows I&A to send enemy Quick Reaction Forces while players attack an objective. Turn it off if the Game Master will send them manually.");
		m_Hints.AddHint(m_GmAutoArtyToggle, "Automatic artillery", "Allows I&A to launch enemy artillery strikes while players attack an objective. Turn it off if the Game Master will control artillery.");
		m_Hints.AddHint(m_GmAutoSideToggle, "Automatic side missions", "Allows I&A to start side missions on its own. Turn it off if the Game Master will start them.");
		m_Hints.AddHint(m_GmAutoSupportToggle, "Automatic support sites", "Automatically adds enemy mortar pits and radio towers when an objective begins.");
		m_Hints.AddHint(openDirBtn, "Open the Director map", "Closes this menu and opens the map used to place, stage, and control objectives.");

		BuildDefensePage(runtime);
		BuildDynamicAIPage(runtime);

		m_Scroll.AddChild(m_PageScaling);
		m_Scroll.AddChild(m_PageCiv);
		m_Scroll.AddChild(m_PageArty);
		m_Scroll.AddChild(m_PageHq);
		m_Scroll.AddChild(m_PageFactions);
		m_Scroll.AddChild(m_PageQrf);
		m_Scroll.AddChild(m_PageDirector);
		m_Scroll.AddChild(m_PageDefense);
		m_Scroll.AddChild(m_PageDynamicAI);

		// Everyday actions on the first row, the ones that end an objective on the second.
		ref MUI_Row persistRow = runtime.CreateRow("persistRow");
		persistRow.SetGap(10);

		ref MUI_Button saveBtn = IA_UplinkButton.Create(runtime, "Save", "save");
		saveBtn.MakeAccent();
		saveBtn.GetOnClicked().Insert(OnMikesSave);

		ref MUI_Button persistBtn = IA_UplinkButton.Create(runtime, "Save for restart", "persist");
		persistBtn.GetOnClicked().Insert(OnMikesPersist);

		ref MUI_Button clearBtn = IA_UplinkButton.Create(runtime, "Clear saved", "clearSaved");
		clearBtn.GetOnClicked().Insert(OnMikesClearPersisted);

		persistRow.AddChild(saveBtn);
		persistRow.AddChild(persistBtn);
		persistRow.AddChild(clearBtn);
		m_Hints.AddHint(saveBtn, "Save", "Applies these settings to the current mission. They will not be remembered after a server restart unless you also use Save for restart.");
		m_Hints.AddHint(persistBtn, "Save for restart", "Applies these settings now and remembers them for future server restarts.");
		m_Hints.AddHint(clearBtn, "Clear saved settings", "Forgets the settings saved for future restarts. The server will return to the mission's normal settings after the next restart.");

		ref MUI_Button promoteBtn = IA_UplinkButton.Create(runtime, "Promote Self", "promote");
		promoteBtn.GetOnClicked().Insert(OnMikesPromoteSelf);

		ref MUI_Button completeBtn = IA_UplinkButton.Create(runtime, "Complete Zone", "complete");
		completeBtn.MakeDanger();
		completeBtn.GetOnClicked().Insert(OnMikesComplete);

		ref MUI_Button completeDefendBtn = IA_UplinkButton.Create(runtime, "Complete + Defend", "completeDef");
		completeDefendBtn.MakeDanger();
		completeDefendBtn.GetOnClicked().Insert(OnMikesCompleteAndDefend);

		ref MUI_Button helpBtn = IA_UplinkButton.Create(runtime, "Help", "help");
		helpBtn.GetOnClicked().Insert(OnAdminHelp);

		ref MUI_Button closeBtn = IA_UplinkButton.Create(runtime, "Close", "close");
		closeBtn.GetOnClicked().Insert(OnMUIBack);

		persistRow.AddChild(helpBtn);
		persistRow.AddChild(promoteBtn);
		persistRow.AddChild(closeBtn);
		m_Hints.AddHint(promoteBtn, "Become Game Master", "Gives you Game Master access so you can use the Director map and other Game Master tools.");
		m_Hints.AddHint(completeBtn, "Complete the current objective", "Immediately marks the current objective as captured and moves the mission forward. Skips a defense even if one was placed.");

		ref MUI_Button seizeBaseBtn = IA_UplinkButton.Create(runtime, "Complete objectives + seize base", "seizeBase");
		seizeBaseBtn.MakeDanger();
		seizeBaseBtn.GetOnClicked().Insert(OnMikesCompleteAndSeizeBase);

		ref MUI_Row actionRow = runtime.CreateRow("actionRow");
		actionRow.SetGap(10);
		actionRow.AddChild(completeBtn);
		actionRow.AddChild(completeDefendBtn);
		actionRow.AddChild(seizeBaseBtn);
		m_Hints.AddHint(completeDefendBtn, "Complete and start field-base assault", "Finishes remaining required objectives and starts one field-base attempt. Seize the command area, then start the normal defense at that same site without an extra regroup or warning countdown. If a base job is already running, this leaves it alone. If placement fails, an authored defense is forced when a Defend marker remains.");
		m_Hints.AddHint(seizeBaseBtn, "Complete objectives and seize a base", "Finishes remaining required objectives and starts one validated field-base attempt. Geometry and AI limits still apply. If placement fails, an authored defense is forced when a Defend marker remains.");

		ref IA_UplinkNote footNote = IA_UplinkNote.Create(runtime, "Save applies now  •  Save for restart writes the server profile (applied after the mission .conf)", "footNote");

		m_Frame.AddChild(m_Tabs);
		m_Frame.AddChild(m_Scroll);
		m_Frame.AddChild(persistRow);
		m_Frame.AddChild(actionRow);
		m_Frame.AddChild(footNote);

		overlay.AddChild(fx);
		overlay.AddChild(m_Frame);
		overlay.AddChild(m_Hints);
		runtime.SetRoot(overlay);
		runtime.SetPromptText("<action name='MenuSelect' scale='1.35'/>  Select", "<action name='MenuBack' scale='1.35'/>  Close");

		ShowAdminPage(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAdminHelp()
	{
		if (m_Hints)
			m_Hints.Toggle();
	}

	//------------------------------------------------------------------------------------------------
	//! Two controls side by side; a null \p right leaves that half of the row free.
	protected void AddPair(notnull MUI_Runtime runtime, notnull MUI_Panel page, string name, notnull MUI_Node left, MUI_Node right)
	{
		page.AddChild(IA_UplinkPair.Create(runtime, name, left, right));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCaption(notnull MUI_Runtime runtime, notnull MUI_Panel page, string text, string name)
	{
		page.AddChild(IA_UplinkCaption.Create(runtime, text, name));
	}

	//------------------------------------------------------------------------------------------------
	//! Solo test path for the transport pilot card: replays scripted updates on this client's HUD.
	protected void BuildPilotPreviewSection(notnull MUI_Runtime runtime)
	{
		ref IA_UplinkNote previewLbl = IA_UplinkNote.Create(runtime, "Plays on your HUD only. No passengers needed, no points awarded.", "pilotPreviewLbl");

		ref MUI_Button seatBtn = IA_UplinkButton.Create(runtime, "Pilot seat", "pilotPrevSeat");
		seatBtn.GetOnClicked().Insert(OnPilotPreviewSeat);
		ref MUI_Button landingBtn = IA_UplinkButton.Create(runtime, "Full landing", "pilotPrevLanding");
		landingBtn.GetOnClicked().Insert(OnPilotPreviewLanding);
		ref MUI_Button farBtn = IA_UplinkButton.Create(runtime, "Far landing", "pilotPrevFar");
		farBtn.GetOnClicked().Insert(OnPilotPreviewFarLanding);
		ref MUI_Button syncBtn = IA_UplinkButton.Create(runtime, "Total syncing", "pilotPrevSync");
		syncBtn.GetOnClicked().Insert(OnPilotPreviewSyncing);
		ref MUI_Button unlockBtn = IA_UplinkButton.Create(runtime, "Skin unlock", "pilotPrevUnlock");
		unlockBtn.GetOnClicked().Insert(OnPilotPreviewUnlock);
		ref MUI_Button lateBtn = IA_UplinkButton.Create(runtime, "Late unlock", "pilotPrevLate");
		lateBtn.GetOnClicked().Insert(OnPilotPreviewLateUnlock);
		ref MUI_Button ownedBtn = IA_UplinkButton.Create(runtime, "Unlocked seat", "pilotPrevOwned");
		ownedBtn.GetOnClicked().Insert(OnPilotPreviewUnlockedSeat);
		ref MUI_Button allBtn = IA_UplinkButton.Create(runtime, "Play all", "pilotPrevAll");
		allBtn.MakeAccent();
		allBtn.GetOnClicked().Insert(OnPilotPreviewAll);
		ref MUI_Button skinBtn = IA_UplinkButton.Create(runtime, "Cycle nearest heli skin", "pilotPrevSkin");
		skinBtn.GetOnClicked().Insert(OnSkinPreview);

		// One width for all nine, so the three rows stand as a grid.
		ref array<MUI_Button> previewBtns = {seatBtn, landingBtn, farBtn, syncBtn, unlockBtn, lateBtn, ownedBtn, allBtn, skinBtn};
		foreach (MUI_Button previewBtn : previewBtns)
		{
			previewBtn.SetMinWidth(PREVIEW_BTN_W);
		}

		ref MUI_Row previewRow1 = runtime.CreateRow("pilotPrevRow1");
		previewRow1.SetGap(10);
		previewRow1.AddChild(seatBtn);
		previewRow1.AddChild(landingBtn);
		previewRow1.AddChild(farBtn);

		ref MUI_Row previewRow2 = runtime.CreateRow("pilotPrevRow2");
		previewRow2.SetGap(10);
		previewRow2.AddChild(syncBtn);
		previewRow2.AddChild(unlockBtn);
		previewRow2.AddChild(lateBtn);

		ref MUI_Row previewRow3 = runtime.CreateRow("pilotPrevRow3");
		previewRow3.SetGap(10);
		previewRow3.AddChild(ownedBtn);
		previewRow3.AddChild(allBtn);
		previewRow3.AddChild(skinBtn);

		AddCaption(runtime, m_PageHq, "Pilot card preview", "pilotPreviewCap");
		m_PageHq.AddChild(previewLbl);
		m_PageHq.AddChild(previewRow1);
		m_PageHq.AddChild(previewRow2);
		m_PageHq.AddChild(previewRow3);
		m_Hints.AddHint(previewLbl, "Pilot card preview", "Closes the menus and plays the transport pilot card on your own HUD. Nothing is sent to the server and no rating is earned.");
		m_Hints.AddHint(seatBtn, "Pilot seat", "The rating card a pilot sees on taking the pilot seat.");
		m_Hints.AddHint(landingBtn, "Full landing", "Twelve passengers set down inside the objective, stepping out over four seconds onto one card.");
		m_Hints.AddHint(farBtn, "Far landing", "Five passengers set down short of the objective, for the lower weight and the distance label.");
		m_Hints.AddHint(syncBtn, "Total syncing", "A landing reported before the global total is known; the total arrives while the card is open.");
		m_Hints.AddHint(unlockBtn, "Skin unlock", "The landing that crosses the skin threshold, with passengers still stepping out afterwards.");
		m_Hints.AddHint(lateBtn, "Late unlock", "The unlock arriving on a rating card, after points were banked while the total was unknown.");
		m_Hints.AddHint(ownedBtn, "Unlocked seat", "The rating card of a pilot who already owns the skin.");
		m_Hints.AddHint(allBtn, "Play all", "Plays every preview in turn; each waits for the previous card to leave.");
		m_Hints.AddHint(skinBtn, "Cycle nearest heli skin", "Repaints the nearest paintable helicopter with its next livery, for everyone, where it stands. It works from the pilot's seat with the engine running. Press again to step through the liveries and back to stock. No rating is needed or earned.");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildDynamicAIPage(notnull MUI_Runtime runtime)
	{
		m_DynamicAISpawningToggle = IA_UplinkToggle.Create(runtime, "Dynamic AI Spawning", "dynamicAiSpawning");
		AddCaption(runtime, m_PageDynamicAI, "Dynamic AI spawning", "dynamicAiCap");
		AddPair(runtime, m_PageDynamicAI, "dynamicAiOnRow", m_DynamicAISpawningToggle, null);
		m_Hints.AddHint(m_DynamicAISpawningToggle, "Dynamic AI Spawning", "Caches supported soldiers and restores survivors as needed. ON uses the Dynamic AI scale below instead of player-count scaling, even with budget 0. Far squads that would exceed the budget record reserves instead of creating every soldier; the leader still spawns. Vehicles and assigned mortar gunners stay spawned. OFF restores reserves and resumes saved scaling. Positions and casualties persist; equipment/ammo reset.");
		m_DynamicAIScaleField = IA_UplinkField.Create(runtime, "Dynamic AI scale", "dynamicAiScale");
		m_DynamicAIScaleField.SetRange(IA_Config.DYNAMIC_AI_SCALE_MIN, IA_Config.DYNAMIC_AI_SCALE_MAX);
		m_DynamicAIScaleField.SetStep(0.1);
		m_DynamicAIScaleField.SetDecimals(2);
		m_DynamicAIScaleField.SetValue(IA_Config.DYNAMIC_AI_SCALE_DEFAULT);
		m_Hints.AddHint(m_DynamicAIScaleField, "Dynamic AI scale", "Roster strength while Dynamic AI Spawning is ON. Replaces player-count scaling and the Scaling-tab multiplier or static override. 0.8 is 80 percent of the baseline roster. 1.0 is full strength. Changing this does not rebuild an already assigned roster.");
		m_DynamicAIBudgetField = MakeDynamicAIField(runtime, "Dynamic AI Budget (soldiers; 0 = distance only)", "dynamicAiBudget", 0, IA_Config.DYNAMIC_AI_BUDGET_MAX, 10, IA_Config.DYNAMIC_AI_BUDGET_DEFAULT, "Target count of physical infantry. Far new squads seed reserves once this number is already live. Nearest squads restore first. Farther squads stay cached or are evicted. Live soldiers already inside 400 m are not deleted immediately. Vehicles and assigned mortar gunners stay spawned. 0 is distance-only caching.");

		AddPair(runtime, m_PageDynamicAI, "dynamicAiScaleRow", m_DynamicAIScaleField, m_DynamicAIBudgetField);

		ref IA_UplinkNote modes = IA_UplinkNote.Create(runtime, "Budget 0: distance only. Positive budget: nearest squads first. Dynamic AI scale applies while ON. Save applies now; Save for restart also remembers changes.", "dynamicAiModes");
		m_PageDynamicAI.AddChild(modes);

		AddCaption(runtime, m_PageDynamicAI, "Distances (meters)  |  Applies to every player", "dynamicAiDistances");
		m_DynamicAIWakeField = MakeDynamicAIField(runtime, "Wake / eligibility distance (m)", "dynamicAiWake", 100, 5000, 50, 1000, "Distance only: requests the whole squad. Budget mode: admits squads to nearest-first allocation up to the soldier budget. Larger values provide more approach time.");
		m_DynamicAICacheField = MakeDynamicAIField(runtime, "Cache / outer retention distance (m)", "dynamicAiCache", 150, 7500, 50, 1500, "Distance only: all soldiers must remain beyond this distance for the quiet period. Budget mode: an existing allocation remains eligible out to this distance.");
		m_DynamicAICloseField = MakeDynamicAIField(runtime, "Close keep distance (m; budget)", "dynamicAiClose", 50, 2000, 50, 300, "Live soldiers inside this band stay in the close-keep hysteresis. They are not deleted while you remain this close. Walking in does not restore every cached squad over the budget; nearest groups fill first.");
		m_DynamicAIReleaseField = MakeDynamicAIField(runtime, "Keep release distance (m; budget)", "dynamicAiRelease", 100, 3000, 50, 400, "Close-keep hysteresis ends beyond this distance. Individual live soldiers inside it cannot be removed. Farther teammates of the same squad can still be cached to free budget for nearer groups.");

		AddPair(runtime, m_PageDynamicAI, "dynamicAiWakeRow", m_DynamicAIWakeField, m_DynamicAICacheField);
		AddPair(runtime, m_PageDynamicAI, "dynamicAiKeepRow", m_DynamicAICloseField, m_DynamicAIReleaseField);

		ref IA_UplinkNote distanceOrder = IA_UplinkNote.Create(runtime, "Save keeps release at least 50 m beyond protection, wake at least as far as release, and cache at least 50 m beyond wake.", "dynamicAiDistanceOrder");
		m_PageDynamicAI.AddChild(distanceOrder);

		AddCaption(runtime, m_PageDynamicAI, "Timing (seconds)", "dynamicAiTiming");
		m_DynamicAICacheQuietField = MakeDynamicAIField(runtime, "Departure quiet time (s; distance only)", "dynamicAiCacheQuiet", 0, 600, 5, 60, "How long a squad must remain outside cache distance before distance-only removal. Returning players restart the timer. 0 removes the departure delay; other safety checks still apply.");
		m_DynamicAICombatQuietField = MakeDynamicAIField(runtime, "Combat quiet time (s; both modes)", "dynamicAiCombatQuiet", 10, 300, 5, 60, "Protects recent combat only while a player is inside wake distance. Native AI-vs-AI targets do not keep far squads spawned. Lower values release nearby troops sooner after fighting.");
		m_DynamicAIMinLiveField = MakeDynamicAIField(runtime, "Minimum live time (s; budget)", "dynamicAiMinLive", 5, 300, 5, 30, "Newly observed or restored soldiers stay live for at least this long. Prevents rapid removal immediately after a spawn.");
		m_DynamicAIEvictDelayField = MakeDynamicAIField(runtime, "Allocation reduction delay (s; budget)", "dynamicAiEvictDelay", 1, 120, 1, 10, "A lower squad allocation must remain pending for this long before removals begin. Higher values smooth brief priority changes; lower values free capacity sooner.");
		AddPair(runtime, m_PageDynamicAI, "dynamicAiQuietRow", m_DynamicAICacheQuietField, m_DynamicAICombatQuietField);
		AddPair(runtime, m_PageDynamicAI, "dynamicAiLiveRow", m_DynamicAIMinLiveField, m_DynamicAIEvictDelayField);

		AddCaption(runtime, m_PageDynamicAI, "Allocation stability", "dynamicAiStability");
		m_DynamicAIRetentionField = MakeDynamicAIField(runtime, "Existing allocation preference (m; budget)", "dynamicAiRetention", 0, 500, 10, 50, "Subtracts this distance from the ranking of already allocated squads. Reduces swapping between similar distances. 0 uses nearest distance without this preference.");
		m_DynamicAICaptureSeedField = MakeDynamicAIField(runtime, "Capture seed window (s; budget)", "dynamicAiCaptureSeed", 5, 120, 5, 30, "A contested objective may restore one defender over the budget for this long so capture can start. The rest of that squad still fills nearest-first.");
		m_DynamicAIHardCapToggle = IA_UplinkToggle.Create(runtime, "Hard cap (combat does not block eviction)", "dynamicAiHardCap");
		AddPair(runtime, m_PageDynamicAI, "dynamicAiStabilityRow", m_DynamicAIRetentionField, m_DynamicAICaptureSeedField);
		AddPair(runtime, m_PageDynamicAI, "dynamicAiHardCapRow", m_DynamicAIHardCapToggle, null);
		m_Hints.AddHint(m_DynamicAIHardCapToggle, "Hard cap", "When on, a squad in recent combat can still be evicted if it is overallocated and outside the keep-release distance. Close soldiers stay. Use this when you want the budget to stay tight during a running fight.");
	}

	//------------------------------------------------------------------------------------------------
	//! A whole-number field with its hint; the caller places it.
	protected MUI_NumericField MakeDynamicAIField(notnull MUI_Runtime runtime, string label, string name, int minimum, int maximum, int step, int initialValue, string hint)
	{
		ref MUI_NumericField field = IA_UplinkField.Create(runtime, label, name);
		field.SetRange(minimum, maximum);
		field.SetStep(step);
		field.SetDecimals(0);
		field.SetValue(initialValue);
		m_Hints.AddHint(field, label, hint);
		return field;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildDefensePage(notnull MUI_Runtime runtime)
	{
		if (!m_PageDefense)
			return;

		ref IA_UplinkNote intro = IA_UplinkNote.Create(runtime, "Applies to the next defense after an AO group completes. Enhanced is the default.", "defIntro");

		ref IA_UplinkNote baseIntro = IA_UplinkNote.Create(runtime, "Applies after this AO's required objectives; successful placement uses the captured base for defense.", "dynBaseIntro");
		AddCaption(runtime, m_PageDefense, "Field base", "dynBaseCap");
		m_PageDefense.AddChild(baseIntro);

		m_DynamicBaseEnabledToggle = IA_UplinkToggle.Create(runtime, "Dynamic field base after required objectives", "dynBaseOn");
		m_Hints.AddHint(m_DynamicBaseEnabledToggle, "Dynamic field base", "After required objectives, try to place a USSR field base to seize and immediately defend using the same defense settings, doctrines and phases as an authored position. Off uses the existing authored-defense roll.");

		m_DynamicBaseEmplacementsToggle = IA_UplinkToggle.Create(runtime, "Optional finite-ammunition base emplacements", "dynBaseGuns");
		AddPair(runtime, m_PageDefense, "dynBaseRow", m_DynamicBaseEnabledToggle, m_DynamicBaseEmplacementsToggle);
		m_Hints.AddHint(m_DynamicBaseEmplacementsToggle, "Base emplacements", "Allow PKM, NSV and AA positions in newly constructed bases. Crews come from the existing defender budget. Off leaves the fortifications unarmed; it does not alter an active base.");

		m_DynamicBaseChanceField = IA_UplinkField.Create(runtime, "Dynamic base chance (0 disables automatic selection)", "dynBaseChance");
		m_DynamicBaseChanceField.SetRange(0, 100);
		m_DynamicBaseChanceField.SetStep(1);
		m_DynamicBaseChanceField.SetDecimals(0);
		m_DynamicBaseChanceField.SetValue(100);
		m_Hints.AddHint(m_DynamicBaseChanceField, "Selection chance", "0 disables automatic selection. 100 always attempts a legal site. Missed or failed placement returns to the authored 80% defense roll.");

		m_DynamicBaseInGmToggle = IA_UplinkToggle.Create(runtime, "Allow automatic dynamic base in Game Master mode", "dynBaseGm");
		AddPair(runtime, m_PageDefense, "dynBaseGmRow", m_DynamicBaseInGmToggle, null);
		m_Hints.AddHint(m_DynamicBaseInGmToggle, "Game Master auto", "When Game Master mode is on, automatic base selection stays off unless this is enabled. The seize-base command still works.");

		m_DynamicBaseSizeDrop = IA_UplinkChoice.Create(runtime, "Dynamic base size", "dynBaseSize");
		m_DynamicBaseSizeDrop.AddItem("Auto");
		m_DynamicBaseSizeDrop.AddItem("Full");
		m_DynamicBaseSizeDrop.AddItem("Compact");
		m_DynamicBaseSizeDrop.AddItem("Courtyard (88 x 76 m)");
		m_DynamicBaseSizeDrop.AddItem("Roadside (60 x 96 m)");
		m_DynamicBaseSizeDrop.AddItem("Command post (64 x 56 m)");
		m_DynamicBaseSizeDrop.AddItem("Rally post (36 x 48 m)");
		m_DynamicBaseSizeDrop.SetIndex(0);
		AddPair(runtime, m_PageDefense, "dynBaseSizeRow", m_DynamicBaseChanceField, m_DynamicBaseSizeDrop);
		m_Hints.AddHint(m_DynamicBaseSizeDrop, "Base size", "Auto tries six layouts, from the full operating base down to a 36 x 48 m fortified rally post. Choose a named layout to require that design. Smaller bases have fewer occupying guards; capture and the normal defense take place at the same base.");

		m_DefendLegacyToggle = IA_UplinkToggle.Create(runtime, "Use legacy defense (12-16 min, no events)", "defLegacy");
		AddCaption(runtime, m_PageDefense, "Defense clock", "defClockCap");
		m_PageDefense.AddChild(intro);
		AddPair(runtime, m_PageDefense, "defLegacyRow", m_DefendLegacyToggle, null);
		m_Hints.AddHint(m_DefendLegacyToggle, "Legacy defense", "Turn this on to keep the original timer-only hold for the next defense. Leave it off for named doctrines and mini-objectives.");

		m_DefendDurMinField = IA_UplinkField.Create(runtime, "Enhanced duration min (minutes)", "defDurMin");
		m_DefendDurMinField.SetRange(8, 40);
		m_DefendDurMinField.SetStep(1);
		m_DefendDurMinField.SetDecimals(0);
		m_DefendDurMinField.SetValue(18);

		m_DefendDurMaxField = IA_UplinkField.Create(runtime, "Enhanced duration max (minutes)", "defDurMax");
		m_DefendDurMaxField.SetRange(8, 40);
		m_DefendDurMaxField.SetStep(1);
		m_DefendDurMaxField.SetDecimals(0);
		m_DefendDurMaxField.SetValue(22);

		m_DefendPrepMinField = IA_UplinkField.Create(runtime, "PREPARE min (seconds)", "defPrepMin");
		m_DefendPrepMinField.SetRange(15, 300);
		m_DefendPrepMinField.SetStep(5);
		m_DefendPrepMinField.SetDecimals(0);
		m_DefendPrepMinField.SetValue(120);

		m_DefendPrepMaxField = IA_UplinkField.Create(runtime, "PREPARE max (seconds)", "defPrepMax");
		m_DefendPrepMaxField.SetRange(15, 300);
		m_DefendPrepMaxField.SetStep(5);
		m_DefendPrepMaxField.SetDecimals(0);
		m_DefendPrepMaxField.SetValue(180);

		m_DefendEventsField = IA_UplinkField.Create(runtime, "Guaranteed mini-objectives", "defEvents");
		m_DefendEventsField.SetRange(0, 3);
		m_DefendEventsField.SetStep(1);
		m_DefendEventsField.SetDecimals(0);
		m_DefendEventsField.SetValue(2);

		AddPair(runtime, m_PageDefense, "defDurRow", m_DefendDurMinField, m_DefendDurMaxField);
		AddPair(runtime, m_PageDefense, "defPrepRow", m_DefendPrepMinField, m_DefendPrepMaxField);
		m_Hints.AddHint(m_DefendDurMinField, "Shortest Enhanced hold", "Minimum minutes for an Enhanced defense clock after PREPARE.");
		m_Hints.AddHint(m_DefendDurMaxField, "Longest Enhanced hold", "Maximum minutes for an Enhanced defense clock after PREPARE.");
		m_Hints.AddHint(m_DefendPrepMinField, "Shortest PREPARE", "Minimum seconds before the main clock starts if players have not made contact.");
		m_Hints.AddHint(m_DefendPrepMaxField, "Longest PREPARE", "Maximum seconds the PREPARE phase can last before the clock starts automatically.");
		m_Hints.AddHint(m_DefendEventsField, "Guaranteed events", "How many optional mini-objectives always appear (max 3). A third event can still roll from the player-count chances below. Hunts never start in CRISIS.");

		m_DefendThirdLowField = IA_UplinkField.Create(runtime, "Third-event chance (1-8 players)", "defThirdLow");
		m_DefendThirdLowField.SetRange(0, 1);
		m_DefendThirdLowField.SetStep(0.05);
		m_DefendThirdLowField.SetDecimals(2);
		m_DefendThirdLowField.SetValue(0.25);

		m_DefendThirdMidField = IA_UplinkField.Create(runtime, "Third-event chance (9-16 players)", "defThirdMid");
		m_DefendThirdMidField.SetRange(0, 1);
		m_DefendThirdMidField.SetStep(0.05);
		m_DefendThirdMidField.SetDecimals(2);
		m_DefendThirdMidField.SetValue(0.50);

		m_DefendThirdHighField = IA_UplinkField.Create(runtime, "Third-event chance (17+ players)", "defThirdHigh");
		m_DefendThirdHighField.SetRange(0, 1);
		m_DefendThirdHighField.SetStep(0.05);
		m_DefendThirdHighField.SetDecimals(2);
		m_DefendThirdHighField.SetValue(0.75);

		AddCaption(runtime, m_PageDefense, "Mini-objectives", "defEventsCap");
		AddPair(runtime, m_PageDefense, "defEventsRow", m_DefendEventsField, m_DefendThirdLowField);
		AddPair(runtime, m_PageDefense, "defThirdRow", m_DefendThirdMidField, m_DefendThirdHighField);
		m_Hints.AddHint(m_DefendThirdLowField, "Third event at low pop", "Chance of a third mini-objective when 1 to 8 players are connected.");
		m_Hints.AddHint(m_DefendThirdMidField, "Third event at mid pop", "Chance of a third mini-objective when 9 to 16 players are connected.");
		m_Hints.AddHint(m_DefendThirdHighField, "Third event at high pop", "Chance of a third mini-objective when 17 or more players are connected.");

		m_DefendPriSuccMinField = IA_UplinkField.Create(runtime, "Priority success reduction min (sec)", "defPriSMin");
		m_DefendPriSuccMinField.SetRange(30, 600);
		m_DefendPriSuccMinField.SetStep(10);
		m_DefendPriSuccMinField.SetDecimals(0);
		m_DefendPriSuccMinField.SetValue(120);

		m_DefendPriSuccMaxField = IA_UplinkField.Create(runtime, "Priority success reduction max (sec)", "defPriSMax");
		m_DefendPriSuccMaxField.SetRange(30, 600);
		m_DefendPriSuccMaxField.SetStep(10);
		m_DefendPriSuccMaxField.SetDecimals(0);
		m_DefendPriSuccMaxField.SetValue(240);

		m_DefendPriFailMinField = IA_UplinkField.Create(runtime, "Priority timeout penalty min (sec)", "defPriFMin");
		m_DefendPriFailMinField.SetRange(30, 600);
		m_DefendPriFailMinField.SetStep(10);
		m_DefendPriFailMinField.SetDecimals(0);
		m_DefendPriFailMinField.SetValue(180);

		m_DefendPriFailMaxField = IA_UplinkField.Create(runtime, "Priority timeout penalty max (sec)", "defPriFMax");
		m_DefendPriFailMaxField.SetRange(30, 600);
		m_DefendPriFailMaxField.SetStep(10);
		m_DefendPriFailMaxField.SetDecimals(0);
		m_DefendPriFailMaxField.SetValue(300);

		AddCaption(runtime, m_PageDefense, "High priority event", "defPriorityCap");
		AddPair(runtime, m_PageDefense, "defPriSRow", m_DefendPriSuccMinField, m_DefendPriSuccMaxField);
		AddPair(runtime, m_PageDefense, "defPriFRow", m_DefendPriFailMinField, m_DefendPriFailMaxField);
		m_Hints.AddHint(m_DefendPriSuccMinField, "Priority success floor", "Shortest time removed from the hold clock when the HIGH PRIORITY event succeeds. Remaining time will not be cut below 6 minutes.");
		m_Hints.AddHint(m_DefendPriSuccMaxField, "Priority success ceiling", "Longest time removed from the hold clock when the HIGH PRIORITY event succeeds.");
		m_Hints.AddHint(m_DefendPriFailMinField, "Priority timeout floor", "Shortest extra time added when the HIGH PRIORITY deadline expires.");
		m_Hints.AddHint(m_DefendPriFailMaxField, "Priority timeout ceiling", "Longest extra time added when the HIGH PRIORITY deadline expires.");

		m_DefendDocSiegeToggle = IA_UplinkToggle.Create(runtime, "Siege doctrine", "defDocSiege");
		m_DefendDocBreakToggle = IA_UplinkToggle.Create(runtime, "Breakthrough doctrine", "defDocBreak");
		m_DefendDocAirToggle = IA_UplinkToggle.Create(runtime, "Air Assault doctrine", "defDocAir");
		m_DefendDocCmdToggle = IA_UplinkToggle.Create(runtime, "Command Offensive doctrine", "defDocCmd");
		m_DefendDocSiegeToggle.SetChecked(true);
		m_DefendDocBreakToggle.SetChecked(true);
		m_DefendDocAirToggle.SetChecked(true);
		m_DefendDocCmdToggle.SetChecked(true);
		AddCaption(runtime, m_PageDefense, "Doctrines", "defDocCap");
		AddPair(runtime, m_PageDefense, "defDocRow1", m_DefendDocSiegeToggle, m_DefendDocBreakToggle);
		AddPair(runtime, m_PageDefense, "defDocRow2", m_DefendDocAirToggle, m_DefendDocCmdToggle);
		m_Hints.AddHint(m_DefendDocSiegeToggle, "Siege", "Allows scout-to-spotting-team and deliberate infantry pushes. Disable the others to force this doctrine.");
		m_Hints.AddHint(m_DefendDocBreakToggle, "Breakthrough", "Allows motorized and mechanized dismounts. Needs a nearby road or it rerolls.");
		m_Hints.AddHint(m_DefendDocAirToggle, "Air Assault", "Allows HALO insertions with the ground attack. Needs an open LZ or it rerolls.");
		m_Hints.AddHint(m_DefendDocCmdToggle, "Command Offensive", "Allows mixed attacks directed from a commander FOB.");

		m_DefendEvtCmdToggle = IA_UplinkToggle.Create(runtime, "Commander FOB event", "defEvtCmd");
		m_DefendEvtEliteToggle = IA_UplinkToggle.Create(runtime, "Elite patrol event", "defEvtElite");
		m_DefendEvtScoutToggle = IA_UplinkToggle.Create(runtime, "Scout / spotting event", "defEvtScout");
		m_DefendEvtConvoyToggle = IA_UplinkToggle.Create(runtime, "Reinforcement convoy event", "defEvtConvoy");
		m_DefendEvtSniperToggle = IA_UplinkToggle.Create(runtime, "Sniper pair event", "defEvtSniper");
		m_DefendEvtCmdToggle.SetChecked(true);
		m_DefendEvtEliteToggle.SetChecked(true);
		m_DefendEvtScoutToggle.SetChecked(true);
		m_DefendEvtConvoyToggle.SetChecked(true);
		m_DefendEvtSniperToggle.SetChecked(true);
		AddCaption(runtime, m_PageDefense, "Mini-objective types", "defEvtCap");
		AddPair(runtime, m_PageDefense, "defEvtRow1", m_DefendEvtCmdToggle, m_DefendEvtEliteToggle);
		AddPair(runtime, m_PageDefense, "defEvtRow2", m_DefendEvtScoutToggle, m_DefendEvtConvoyToggle);
		AddPair(runtime, m_PageDefense, "defEvtRow3", m_DefendEvtSniperToggle, null);
		m_Hints.AddHint(m_DefendEvtCmdToggle, "Commander FOB", "Optional officer hunt. Success cancels the next major assault and one later event.");
		m_Hints.AddHint(m_DefendEvtEliteToggle, "Elite patrol", "Hunt a special-forces squad before it homes on gunfire.");
		m_Hints.AddHint(m_DefendEvtScoutToggle, "Scout then spotting team", "Destroy the scout before recon finishes or extra infantry squads are called onto the hold.");
		m_Hints.AddHint(m_DefendEvtConvoyToggle, "Convoy", "Destroy marked transports before they dismount extra troops at the line.");
		m_Hints.AddHint(m_DefendEvtSniperToggle, "Sniper pair", "Hunt the pair after they fire to remove precision overwatch.");

		m_DefendHotDropLabel = IA_UplinkNote.Create(runtime, "Air Assault hot-drop chance  20%", "defHotLbl");
		m_DefendHotDropLabel.SetColor(IA_UplinkStyle.Get().m_Gold);
		m_DefendHotDropSlider = IA_UplinkSlider.Create(runtime, "defHotDrop");
		m_DefendHotDropSlider.SetRange(0, 1);
		m_DefendHotDropSlider.SetStep(0.01);
		m_DefendHotDropSlider.SetValue(0.20);
		m_DefendHotDropSlider.GetOnChanged().Insert(OnDefendHotDropChanged);
		AddCaption(runtime, m_PageDefense, "Air assault", "defHotCap");
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

		ref IA_UplinkNote infLbl = IA_UplinkNote.Create(runtime, "Infantry enemies for future spawns. Auto-detect uses the usual mod-aware rotation.", "infFacLbl");
		AddCaption(runtime, m_PageFactions, "Enemy infantry", "infFacCap");
		m_PageFactions.AddChild(infLbl);
		m_Hints.AddHint(infLbl, "Enemy infantry factions", "Choose which factions future enemy foot soldiers will come from.");

		m_EnemyAutoToggle = IA_UplinkToggle.Create(runtime, "Auto-detect infantry factions", "enfAuto");
		m_EnemyAutoToggle.SetChecked(true);
		m_EnemyAutoToggle.GetOnChanged().Insert(OnEnemyAutoChanged);
		AddPair(runtime, m_PageFactions, "enfAutoRow", m_EnemyAutoToggle, null);
		m_Hints.AddHint(m_EnemyAutoToggle, "Choose infantry automatically", "Turn this on to let I&A choose suitable enemy infantry factions. Turn it off to use your selections below.");

		m_EnemyFactionToggles = new array<ref MUI_Toggle>();
		m_EnemyFactionChoiceKeys = new array<string>();
		ref array<string> infNames = new array<string>();
		IA_AdminConfigUtil.CollectFactionChoices(EEntityCatalogType.CHARACTER, 1, m_EnemyFactionChoiceKeys, infNames);
		AddFactionToggles(runtime, m_PageFactions, "enf_", m_EnemyFactionChoiceKeys, infNames, m_EnemyFactionToggles, false);

		ref IA_UplinkNote vehLbl = IA_UplinkNote.Create(runtime, "Vehicle enemies. Match infantry unless you need a different catalog (common with mixed-faction mods).", "vehFacLbl");
		AddCaption(runtime, m_PageFactions, "Enemy vehicles", "vehFacCap");
		m_PageFactions.AddChild(vehLbl);
		m_Hints.AddHint(vehLbl, "Enemy vehicle factions", "Choose which factions future enemy military vehicles will come from.");

		m_VehicleMatchToggle = IA_UplinkToggle.Create(runtime, "Vehicle factions match infantry", "vehMatch");
		m_VehicleMatchToggle.SetChecked(true);
		m_VehicleMatchToggle.GetOnChanged().Insert(OnVehicleMatchChanged);
		AddPair(runtime, m_PageFactions, "vehMatchRow", m_VehicleMatchToggle, null);
		m_Hints.AddHint(m_VehicleMatchToggle, "Match infantry factions", "Turn this on to use the same enemy factions for vehicles and infantry. Turn it off to choose vehicle factions separately.");

		m_VehicleFactionToggles = new array<ref MUI_Toggle>();
		m_VehicleFactionChoiceKeys = new array<string>();
		ref array<string> vehNames = new array<string>();
		IA_AdminConfigUtil.CollectFactionChoices(EEntityCatalogType.VEHICLE, 1, m_VehicleFactionChoiceKeys, vehNames);
		AddFactionToggles(runtime, m_PageFactions, "veh_", m_VehicleFactionChoiceKeys, vehNames, m_VehicleFactionToggles, true);
	}

	//------------------------------------------------------------------------------------------------
	//! One switch per faction, two to a row.
	protected void AddFactionToggles(notnull MUI_Runtime runtime, notnull MUI_Panel page, string namePrefix, notnull array<string> keys, notnull array<string> names, notnull array<ref MUI_Toggle> toggles, bool vehicle)
	{
		int count = keys.Count();
		MUI_Toggle held;
		int i;
		for (i = 0; i < count; i++)
		{
			string label = names[i];
			if (label.IsEmpty())
				label = keys[i];
			ref MUI_Toggle toggle = IA_UplinkToggle.Create(runtime, label, namePrefix + keys[i]);
			if (vehicle)
				toggle.GetOnChanged().Insert(OnVehicleFactionToggleChanged);
			else
				toggle.GetOnChanged().Insert(OnEnemyFactionToggleChanged);
			toggles.Insert(toggle);

			if (!held)
			{
				held = toggle;
				continue;
			}
			AddPair(runtime, page, namePrefix + "row" + i.ToString(), held, toggle);
			held = null;
		}
		if (held)
			AddPair(runtime, page, namePrefix + "rowLast", held, null);
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
		if (m_PageDynamicAI)
			m_PageDynamicAI.SetVisible(index == 8);

		// Each section opens at its top.
		if (m_Scroll)
			m_Scroll.SetScrollY(0);
		if (m_Frame)
			m_Frame.SetNote(string.Format("SECTION %1 OF 9", index + 1));
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
		if (m_DynamicAISpawningToggle)
			m_DynamicAISpawningToggle.SetChecked(cfg.m_bDynamicAISpawningEnabled);
		if (m_DynamicAIScaleField)
			m_DynamicAIScaleField.SetValue(IA_Config.ClampDynamicAIScale(cfg.m_fDynamicAIScale));
		if (m_DynamicAIBudgetField)
			m_DynamicAIBudgetField.SetValue(IA_Config.ClampDynamicAIBudget(cfg.m_iDynamicAIBudget));
		if (m_DynamicAIWakeField)
			m_DynamicAIWakeField.SetValue(cfg.m_iDynamicAIWakeDistanceM);
		if (m_DynamicAICacheField)
			m_DynamicAICacheField.SetValue(cfg.m_iDynamicAICacheDistanceM);
		if (m_DynamicAICloseField)
			m_DynamicAICloseField.SetValue(cfg.m_iDynamicAICloseDistanceM);
		if (m_DynamicAIReleaseField)
			m_DynamicAIReleaseField.SetValue(cfg.m_iDynamicAIReleaseDistanceM);
		if (m_DynamicAICacheQuietField)
			m_DynamicAICacheQuietField.SetValue(cfg.m_iDynamicAICacheQuietSec);
		if (m_DynamicAICombatQuietField)
			m_DynamicAICombatQuietField.SetValue(cfg.m_iDynamicAICombatQuietSec);
		if (m_DynamicAIMinLiveField)
			m_DynamicAIMinLiveField.SetValue(cfg.m_iDynamicAIMinLiveSec);
		if (m_DynamicAIEvictDelayField)
			m_DynamicAIEvictDelayField.SetValue(cfg.m_iDynamicAIEvictDelaySec);
		if (m_DynamicAIRetentionField)
			m_DynamicAIRetentionField.SetValue(cfg.m_iDynamicAIRetentionBiasM);
		if (m_DynamicAICaptureSeedField)
			m_DynamicAICaptureSeedField.SetValue(cfg.m_iDynamicAICaptureSeedSec);
		if (m_DynamicAIHardCapToggle)
			m_DynamicAIHardCapToggle.SetChecked(cfg.m_bDynamicAIHardCap);
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
		if (m_DynamicBaseEmplacementsToggle)
			m_DynamicBaseEmplacementsToggle.SetChecked(cfg.m_bDynamicBaseEmplacementsEnabled);
		if (m_DynamicBaseChanceField)
			m_DynamicBaseChanceField.SetValue(cfg.m_iDynamicBaseChancePct);
		if (m_DynamicBaseInGmToggle)
			m_DynamicBaseInGmToggle.SetChecked(cfg.m_bDynamicBaseInGm);
		if (m_DynamicBaseSizeDrop)
		{
			int sizeIdx = cfg.m_iDynamicBaseSizeMode;
			if (sizeIdx < 0)
				sizeIdx = 0;
			if (sizeIdx > IA_DynamicSiteSizeMode.RallyPost)
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
		if (m_DynamicBaseEmplacementsToggle)
			basePack.m_bDynamicBaseEmplacementsEnabled = m_DynamicBaseEmplacementsToggle.IsChecked();
		if (m_DynamicBaseChanceField)
			basePack.m_iDynamicBaseChancePct = Math.Round(m_DynamicBaseChanceField.GetValue());
		if (m_DynamicBaseInGmToggle)
			basePack.m_bDynamicBaseInGm = m_DynamicBaseInGmToggle.IsChecked();
		if (m_DynamicBaseSizeDrop)
			basePack.m_iDynamicBaseSizeMode = m_DynamicBaseSizeDrop.GetIndex();
		packed = packed + "|" + IA_Config.PackDynamicBaseExtras(basePack);

		bool dynamicAISpawning = true;
		if (live)
			dynamicAISpawning = live.m_bDynamicAISpawningEnabled;
		if (m_DynamicAISpawningToggle)
			dynamicAISpawning = m_DynamicAISpawningToggle.IsChecked();
		if (dynamicAISpawning)
			packed = packed + "|1";
		else
			packed = packed + "|0";
		ref IA_Config dynamicAiPack = new IA_Config();
		if (live)
		{
			dynamicAiPack.m_iDynamicAIBudget = live.m_iDynamicAIBudget;
			dynamicAiPack.m_fDynamicAIScale = live.m_fDynamicAIScale;
			IA_Config.UnpackDynamicAIExtras(dynamicAiPack, IA_Config.PackDynamicAIExtras(live));
		}
		if (m_DynamicAIScaleField)
			dynamicAiPack.m_fDynamicAIScale = m_DynamicAIScaleField.GetValue();
		if (m_DynamicAIBudgetField)
			dynamicAiPack.m_iDynamicAIBudget = Math.Round(m_DynamicAIBudgetField.GetValue());
		if (m_DynamicAIWakeField)
			dynamicAiPack.m_iDynamicAIWakeDistanceM = Math.Round(m_DynamicAIWakeField.GetValue());
		if (m_DynamicAICacheField)
			dynamicAiPack.m_iDynamicAICacheDistanceM = Math.Round(m_DynamicAICacheField.GetValue());
		if (m_DynamicAICloseField)
			dynamicAiPack.m_iDynamicAICloseDistanceM = Math.Round(m_DynamicAICloseField.GetValue());
		if (m_DynamicAIReleaseField)
			dynamicAiPack.m_iDynamicAIReleaseDistanceM = Math.Round(m_DynamicAIReleaseField.GetValue());
		if (m_DynamicAICacheQuietField)
			dynamicAiPack.m_iDynamicAICacheQuietSec = Math.Round(m_DynamicAICacheQuietField.GetValue());
		if (m_DynamicAICombatQuietField)
			dynamicAiPack.m_iDynamicAICombatQuietSec = Math.Round(m_DynamicAICombatQuietField.GetValue());
		if (m_DynamicAIMinLiveField)
			dynamicAiPack.m_iDynamicAIMinLiveSec = Math.Round(m_DynamicAIMinLiveField.GetValue());
		if (m_DynamicAIEvictDelayField)
			dynamicAiPack.m_iDynamicAIEvictDelaySec = Math.Round(m_DynamicAIEvictDelayField.GetValue());
		if (m_DynamicAIRetentionField)
			dynamicAiPack.m_iDynamicAIRetentionBiasM = Math.Round(m_DynamicAIRetentionField.GetValue());
		if (m_DynamicAICaptureSeedField)
			dynamicAiPack.m_iDynamicAICaptureSeedSec = Math.Round(m_DynamicAICaptureSeedField.GetValue());
		if (m_DynamicAIHardCapToggle)
			dynamicAiPack.m_bDynamicAIHardCap = m_DynamicAIHardCapToggle.IsChecked();
		packed = packed + "|" + IA_Config.PackDynamicAIBudget(dynamicAiPack);
		packed = packed + "|" + IA_Config.PackDynamicAIExtras(dynamicAiPack);
		packed = packed + "|" + IA_Config.PackDynamicAIScale(dynamicAiPack);

		IA_MissionInitializer.SubmitPackedAdminConfig(packed, persist);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillSkillDropdown(IA_UplinkChoice drop)
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
	protected void OnPilotPreviewSeat()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.Seat);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewLanding()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.Landing);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewFarLanding()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.FarLanding);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewSyncing()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.Syncing);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewUnlock()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.Unlock);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewLateUnlock()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.LateUnlock);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewUnlockedSeat()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.UnlockedSeat);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPilotPreviewAll()
	{
		PlayPilotPreview(IA_PilotHudPreviewScene.All);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayPilotPreview(IA_PilotHudPreviewScene scene)
	{
		IA_PilotHudPreview.PlayLocal(scene);

		// The card is drawn on the HUD, which the pause menu hides while it is open.
		MenuManager menus = GetGame().GetMenuManager();
		menus.CloseMenuByPreset(ChimeraMenuPreset.PauseMenu);
		menus.CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Solo test path for helicopter skins; the server repaints the airframe and answers with a hint.
	protected void OnSkinPreview()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskPreviewHeliSkin();

		MenuManager menus = GetGame().GetMenuManager();
		menus.CloseMenuByPreset(ChimeraMenuPreset.PauseMenu);
		menus.CloseMenu(this);
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
