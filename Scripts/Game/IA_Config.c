//------------------------------------------------------------------------------------------------
//! Workshop / world baseline. The in-game Admin Config menu is the live source of
//! truth (Save, and Save for restart → $profile). This .conf still loads first so
//! existing missions keep working; leave fields empty unless you need a map default.
//! HQ vehicle prefab lists stay here — they need Workbench resource pickers.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class IA_Config{

	[Attribute(uiwidget: UIWidgets.Auto, category: "Enemy Factions", desc: "Infantry factions, randomly rotated. Empty = auto-detect. Prefer the in-game Admin Config menu.")]
	ref array<string> m_sDesiredEnemyFactionKeys;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "Enemy Factions", desc: "Vehicle factions. Empty = same as infantry. Prefer the in-game Admin Config menu.")]
	ref array<string> m_sDesiredEnemyVehicleFactionKeys;
		
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Generic helicopters for HQ pads (Admin Config cannot pick .et files).", params: "et")]
	ref array<ResourceName> m_aGenericHeliOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Attack helicopters for HQ pads.", params: "et")]
	ref array<ResourceName> m_aAttackHeliOverridePrefabs;

	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Transport helicopters for HQ pads.", params: "et")]
	ref array<ResourceName> m_aTransportHeliOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Cars for HQ pads (e.g. Humvee).", params: "et")]
	ref array<ResourceName> m_aVehicleCarOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Armored vehicles for HQ pads.", params: "et")]
	ref array<ResourceName> m_aVehicleArmorOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. APCs for HQ pads.", params: "et")]
	ref array<ResourceName> m_aAPC_OverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Trucks for HQ pads.", params: "et")]
	ref array<ResourceName> m_aTruckOverridePrefabs;
 
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Medical vehicles for HQ pads.", params: "et")]
	ref array<ResourceName> m_aMedicalVehicleOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Workshop-only. Medical cars for HQ pads.", params: "et")]
	ref array<ResourceName> m_aMedicalCarOverridePrefabs;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "HQ Vehicles", desc: "Disable all Helicopter spawning at HQ (Transport, Generic, Attack)")]
	bool m_bDisableHQHelipads;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "HQ Vehicles", desc: "Disable all Ground Vehicle spawning at HQ (Everything else)")]
	bool m_bDisableHQGroundVehicles;

	[Attribute(defvalue: "0", UIWidgets.EditBox, category: "AI Scaling", desc: "Static AI Player Scale Factor Override (0 = use dynamic scaling, >0 = fixed scale factor)")]
	float m_fStaticAIScaleOverride;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Scaling", desc: "AI Player Scale Multiplier (multiplies dynamic scale factor, ignored if static override is set)")]
	float m_fAIScaleMultiplier;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Scaling", desc: "Multiplier for military vehicle count calculation (0.5 = half, 2.0 = double)")]
	float m_fMilitaryVehicleCountMultiplier;
 
	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Roles", desc: "Enforce role restrictions for vehicle pilots.")]
	bool m_bEnforceRoleRestrictions;

	static const int HALO_JUMP_MAX_PLAYERS_DEFAULT = 15;

	[Attribute(defvalue: "15", UIWidgets.EditBox, category: "Roles", desc: "HALO Jump is available while connected players are below this count. 0 disables HALO.")]
	int m_iHaloJumpMaxPlayers = HALO_JUMP_MAX_PLAYERS_DEFAULT;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Civilian & Resistance", desc: "Enable civilian spawning in captured zones")]
	bool m_bEnableCivilianSpawning;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Multiplier for civilian count calculation (0.5 = half, 2.0 = double)")]
	float m_fCivilianCountMultiplier;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Multiplier for civilian vehicle count calculation (0.5 = half, 2.0 = double)")]
	float m_fCivilianVehicleCountMultiplier;

	[Attribute(defvalue: "0.11", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Percentage of civilians killed to trigger a revolt (0.11 = 11%)")]
	float m_fCivilianRevoltThreshold;

	[Attribute(defvalue: "30000", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Delay (in ms) before showing the revolt notification")]
	int m_iCivilianRevoltNotificationDelay = 30000;

	[Attribute(defvalue: "180000", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Delay (in ms) before spawning revolt reinforcements")]
	int m_iCivilianRevoltReinforcementDelay = 180000;

	[Attribute(defvalue: "0.18", UIWidgets.Slider, category: "Artillery", desc: "Chance (0-1) for an artillery strike to occur during a check interval", params: "0 1 0.01")]
	float m_fArtilleryStrikeChance;

	[Attribute(defvalue: "300", UIWidgets.EditBox, category: "Artillery", desc: "Seconds to wait after a strike before another can start")]
	int m_iArtilleryCooldown;

	[Attribute(defvalue: "45", UIWidgets.EditBox, category: "Artillery", desc: "Minimum seconds from warning smoke to mortar impact")]
	int m_iArtilleryMinDelay;

	[Attribute(defvalue: "70", UIWidgets.EditBox, category: "Artillery", desc: "Maximum seconds from warning smoke to mortar impact")]
	int m_iArtilleryMaxDelay;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Game Master", desc: "Game Master mode: do not auto-start or auto-advance AOs. The GM places and activates sites.")]
	bool m_bGameMasterMode;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Game Master", desc: "When Live completes, start Staging if it has sites, otherwise the next unused map AO.")]
	bool m_bGmAutoActivateStaging = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Game Master", desc: "Allow automatic QRF while a Live AO is under attack.")]
	bool m_bGmAutoQrf = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Game Master", desc: "Allow automatic artillery while a Live AO is under attack.")]
	bool m_bGmAutoArty = true;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Game Master", desc: "Allow automatic side-mission picks (Assassination). Off = GM starts them.")]
	bool m_bGmAutoSideMissions;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Game Master", desc: "On Activate, auto-place a mortar pit and radio towers like classic I&A.")]
	bool m_bGmAutoPlaceSupport;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Defense", desc: "Use the original 12-16 minute timer-only hold for the next defense.")]
	bool m_bUseLegacyDefense;

	[Attribute(defvalue: "18", UIWidgets.EditBox, category: "Defense", desc: "Enhanced defense duration minimum (minutes).")]
	int m_iDefendDurationMinMin = 18;

	[Attribute(defvalue: "22", UIWidgets.EditBox, category: "Defense", desc: "Enhanced defense duration maximum (minutes).")]
	int m_iDefendDurationMaxMin = 22;

	[Attribute(defvalue: "120", UIWidgets.EditBox, category: "Defense", desc: "PREPARE phase minimum (seconds).")]
	int m_iDefendPrepareMinSec = 120;

	[Attribute(defvalue: "180", UIWidgets.EditBox, category: "Defense", desc: "PREPARE phase maximum (seconds).")]
	int m_iDefendPrepareMaxSec = 180;

	[Attribute(defvalue: "2", UIWidgets.EditBox, category: "Defense", desc: "Guaranteed mini-objectives per Enhanced hold (0-3).")]
	int m_iDefendGuaranteedEvents = 2;

	[Attribute(defvalue: "0.25", UIWidgets.Slider, category: "Defense", desc: "Third-event chance at 1-8 connected players.", params: "0 1 0.01")]
	float m_fDefendThirdChanceLow = 0.25;

	[Attribute(defvalue: "0.50", UIWidgets.Slider, category: "Defense", desc: "Third-event chance at 9-16 connected players.", params: "0 1 0.01")]
	float m_fDefendThirdChanceMid = 0.50;

	[Attribute(defvalue: "0.75", UIWidgets.Slider, category: "Defense", desc: "Third-event chance at 17+ connected players.", params: "0 1 0.01")]
	float m_fDefendThirdChanceHigh = 0.75;

	[Attribute(defvalue: "120", UIWidgets.EditBox, category: "Defense", desc: "HIGH PRIORITY success time reduction minimum (seconds).")]
	int m_iDefendPrioritySuccessMinSec = 120;

	[Attribute(defvalue: "240", UIWidgets.EditBox, category: "Defense", desc: "HIGH PRIORITY success time reduction maximum (seconds).")]
	int m_iDefendPrioritySuccessMaxSec = 240;

	[Attribute(defvalue: "180", UIWidgets.EditBox, category: "Defense", desc: "HIGH PRIORITY timeout penalty minimum (seconds).")]
	int m_iDefendPriorityFailMinSec = 180;

	[Attribute(defvalue: "300", UIWidgets.EditBox, category: "Defense", desc: "HIGH PRIORITY timeout penalty maximum (seconds).")]
	int m_iDefendPriorityFailMaxSec = 300;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow Siege doctrine.")]
	bool m_bDefendDoctrineSiege = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow Breakthrough doctrine.")]
	bool m_bDefendDoctrineBreakthrough = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow Air Assault doctrine.")]
	bool m_bDefendDoctrineAirAssault = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow Command Offensive doctrine.")]
	bool m_bDefendDoctrineCommand = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow Commander FOB event.")]
	bool m_bDefendEventCommander = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow elite patrol event.")]
	bool m_bDefendEventElite = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow scout-to-spotting-team event.")]
	bool m_bDefendEventScoutMortar = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow reinforcement convoy event.")]
	bool m_bDefendEventConvoy = true;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Defense", desc: "Unused. Signal-relay events stay disabled until inbound HUD cues exist.")]
	bool m_bDefendEventRelay = false;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Defense", desc: "Allow sniper pair event.")]
	bool m_bDefendEventSniper = true;

	[Attribute(defvalue: "0.20", UIWidgets.Slider, category: "Defense", desc: "Air Assault hot-drop chance (0-1). Remainder uses a 150-300 m perimeter LZ.", params: "0 1 0.01")]
	float m_fDefendHotDropChance = 0.20;

	[Attribute(defvalue: EAISkill.VETERAN.ToString(), UIWidgets.ComboBox, "Aim skill for normal infantry. Vanilla EAISkill only changes aim-error sigma.", "", ParamEnumArray.FromEnum(EAISkill), category: "AI Combat")]
	EAISkill m_eAiSkillNormal = EAISkill.VETERAN;

	[Attribute(defvalue: EAISkill.EXPERT.ToString(), UIWidgets.ComboBox, "Aim skill for elite groups.", "", ParamEnumArray.FromEnum(EAISkill), category: "AI Combat")]
	EAISkill m_eAiSkillElite = EAISkill.EXPERT;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Combat", desc: "Fire-rate coefficient for normal infantry. 1 is vanilla. Above 1 shoots faster (vanilla clamps 0.05-2).")]
	float m_fAiFireRateNormal = 1.0;

	[Attribute(defvalue: "1.25", UIWidgets.EditBox, category: "AI Combat", desc: "Fire-rate coefficient for elite groups.")]
	float m_fAiFireRateElite = 1.25;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Combat", desc: "Visual spotting multiplier for normal infantry. 1 is vanilla.")]
	float m_fAiPerceptionNormal = 1.0;

	[Attribute(defvalue: "1.5", UIWidgets.EditBox, category: "AI Combat", desc: "Visual spotting multiplier for elite groups.")]
	float m_fAiPerceptionElite = 1.5;

	static const int DEFEND_DOC_SIEGE = 1;
	static const int DEFEND_DOC_BREAKTHROUGH = 2;
	static const int DEFEND_DOC_AIR = 4;
	static const int DEFEND_DOC_COMMAND = 8;
	static const int DEFEND_EVT_COMMANDER = 1;
	static const int DEFEND_EVT_ELITE = 2;
	static const int DEFEND_EVT_SCOUT = 4;
	static const int DEFEND_EVT_CONVOY = 8;
	static const int DEFEND_EVT_RELAY = 16;
	static const int DEFEND_EVT_SNIPER = 32;

	//------------------------------------------------------------------------------------------------
	bool UseLegacyDefense()
	{
		return m_bUseLegacyDefense;
	}

	//------------------------------------------------------------------------------------------------
	int GetDefenseDoctrineMask()
	{
		int mask = 0;
		if (m_bDefendDoctrineSiege)
			mask = mask | DEFEND_DOC_SIEGE;
		if (m_bDefendDoctrineBreakthrough)
			mask = mask | DEFEND_DOC_BREAKTHROUGH;
		if (m_bDefendDoctrineAirAssault)
			mask = mask | DEFEND_DOC_AIR;
		if (m_bDefendDoctrineCommand)
			mask = mask | DEFEND_DOC_COMMAND;
		return mask;
	}

	//------------------------------------------------------------------------------------------------
	void SetDefenseDoctrineMask(int mask)
	{
		m_bDefendDoctrineSiege = (mask & DEFEND_DOC_SIEGE) != 0;
		m_bDefendDoctrineBreakthrough = (mask & DEFEND_DOC_BREAKTHROUGH) != 0;
		m_bDefendDoctrineAirAssault = (mask & DEFEND_DOC_AIR) != 0;
		m_bDefendDoctrineCommand = (mask & DEFEND_DOC_COMMAND) != 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetDefenseEventMask()
	{
		int mask = 0;
		if (m_bDefendEventCommander)
			mask = mask | DEFEND_EVT_COMMANDER;
		if (m_bDefendEventElite)
			mask = mask | DEFEND_EVT_ELITE;
		if (m_bDefendEventScoutMortar)
			mask = mask | DEFEND_EVT_SCOUT;
		if (m_bDefendEventConvoy)
			mask = mask | DEFEND_EVT_CONVOY;
		if (m_bDefendEventRelay)
			mask = mask | DEFEND_EVT_RELAY;
		if (m_bDefendEventSniper)
			mask = mask | DEFEND_EVT_SNIPER;
		return mask;
	}

	//------------------------------------------------------------------------------------------------
	void SetDefenseEventMask(int mask)
	{
		m_bDefendEventCommander = (mask & DEFEND_EVT_COMMANDER) != 0;
		m_bDefendEventElite = (mask & DEFEND_EVT_ELITE) != 0;
		m_bDefendEventScoutMortar = (mask & DEFEND_EVT_SCOUT) != 0;
		m_bDefendEventConvoy = (mask & DEFEND_EVT_CONVOY) != 0;
		m_bDefendEventRelay = (mask & DEFEND_EVT_RELAY) != 0;
		m_bDefendEventSniper = (mask & DEFEND_EVT_SNIPER) != 0;
	}

	//------------------------------------------------------------------------------------------------
	void ClampDefenseSettings()
	{
		if (m_iDefendDurationMinMin < 8)
			m_iDefendDurationMinMin = 8;
		if (m_iDefendDurationMaxMin < m_iDefendDurationMinMin)
			m_iDefendDurationMaxMin = m_iDefendDurationMinMin;
		if (m_iDefendDurationMaxMin > 40)
			m_iDefendDurationMaxMin = 40;

		if (m_iDefendPrepareMinSec < 15)
			m_iDefendPrepareMinSec = 15;
		if (m_iDefendPrepareMaxSec < m_iDefendPrepareMinSec)
			m_iDefendPrepareMaxSec = m_iDefendPrepareMinSec;
		if (m_iDefendPrepareMaxSec > 300)
			m_iDefendPrepareMaxSec = 300;

		if (m_iDefendGuaranteedEvents < 0)
			m_iDefendGuaranteedEvents = 0;
		if (m_iDefendGuaranteedEvents > 3)
			m_iDefendGuaranteedEvents = 3;

		if (m_fDefendThirdChanceLow < 0)
			m_fDefendThirdChanceLow = 0;
		if (m_fDefendThirdChanceLow > 1)
			m_fDefendThirdChanceLow = 1;
		if (m_fDefendThirdChanceMid < 0)
			m_fDefendThirdChanceMid = 0;
		if (m_fDefendThirdChanceMid > 1)
			m_fDefendThirdChanceMid = 1;
		if (m_fDefendThirdChanceHigh < 0)
			m_fDefendThirdChanceHigh = 0;
		if (m_fDefendThirdChanceHigh > 1)
			m_fDefendThirdChanceHigh = 1;

		if (m_iDefendPrioritySuccessMinSec < 30)
			m_iDefendPrioritySuccessMinSec = 30;
		if (m_iDefendPrioritySuccessMaxSec < m_iDefendPrioritySuccessMinSec)
			m_iDefendPrioritySuccessMaxSec = m_iDefendPrioritySuccessMinSec;
		if (m_iDefendPriorityFailMinSec < 30)
			m_iDefendPriorityFailMinSec = 30;
		if (m_iDefendPriorityFailMaxSec < m_iDefendPriorityFailMinSec)
			m_iDefendPriorityFailMaxSec = m_iDefendPriorityFailMinSec;

		if (m_fDefendHotDropChance < 0)
			m_fDefendHotDropChance = 0;
		if (m_fDefendHotDropChance > 1)
			m_fDefendHotDropChance = 1;
	}

	//------------------------------------------------------------------------------------------------
	static EAISkill SnapAiSkill(EAISkill skill, EAISkill fallback)
	{
		if (skill == EAISkill.NOOB)
			return skill;
		if (skill == EAISkill.ROOKIE)
			return skill;
		if (skill == EAISkill.REGULAR)
			return skill;
		if (skill == EAISkill.VETERAN)
			return skill;
		if (skill == EAISkill.EXPERT)
			return skill;
		if (skill == EAISkill.CYLON)
			return skill;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	void ClampAiCombatSettings()
	{
		m_eAiSkillNormal = SnapAiSkill(m_eAiSkillNormal, EAISkill.VETERAN);
		m_eAiSkillElite = SnapAiSkill(m_eAiSkillElite, EAISkill.EXPERT);

		if (m_fAiFireRateNormal < 0.05)
			m_fAiFireRateNormal = 0.05;
		if (m_fAiFireRateNormal > 2)
			m_fAiFireRateNormal = 2;
		if (m_fAiFireRateElite < 0.05)
			m_fAiFireRateElite = 0.05;
		if (m_fAiFireRateElite > 2)
			m_fAiFireRateElite = 2;

		if (m_fAiPerceptionNormal < 0.1)
			m_fAiPerceptionNormal = 0.1;
		if (m_fAiPerceptionNormal > 4)
			m_fAiPerceptionNormal = 4;
		if (m_fAiPerceptionElite < 0.1)
			m_fAiPerceptionElite = 0.1;
		if (m_fAiPerceptionElite > 4)
			m_fAiPerceptionElite = 4;
	}

	//------------------------------------------------------------------------------------------------
	EAISkill GetAiSkillNormal()
	{
		ClampAiCombatSettings();
		return m_eAiSkillNormal;
	}

	//------------------------------------------------------------------------------------------------
	EAISkill GetAiSkillElite()
	{
		ClampAiCombatSettings();
		return m_eAiSkillElite;
	}

	//------------------------------------------------------------------------------------------------
	static int SkillToMenuIndex(EAISkill skill)
	{
		if (skill == EAISkill.CYLON)
			return 4;
		if (skill == EAISkill.EXPERT)
			return 3;
		if (skill == EAISkill.VETERAN)
			return 2;
		if (skill == EAISkill.REGULAR)
			return 1;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static EAISkill MenuIndexToSkill(int index)
	{
		if (index == 4)
			return EAISkill.CYLON;
		if (index == 3)
			return EAISkill.EXPERT;
		if (index == 2)
			return EAISkill.VETERAN;
		if (index == 1)
			return EAISkill.REGULAR;
		return EAISkill.ROOKIE;
	}

	//------------------------------------------------------------------------------------------------
	static string PackAiCombatExtras(notnull IA_Config cfg)
	{
		cfg.ClampAiCombatSettings();
		int skillN = cfg.m_eAiSkillNormal;
		int skillE = cfg.m_eAiSkillElite;
		string packed = skillN.ToString();
		packed = packed + "," + skillE.ToString();
		packed = packed + "," + cfg.m_fAiFireRateNormal.ToString();
		packed = packed + "," + cfg.m_fAiFireRateElite.ToString();
		packed = packed + "," + cfg.m_fAiPerceptionNormal.ToString();
		packed = packed + "," + cfg.m_fAiPerceptionElite.ToString();
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	static void UnpackAiCombatExtras(notnull IA_Config cfg, string packed)
	{
		if (packed.IsEmpty())
			return;

		ref array<string> parts = new array<string>();
		packed.Split(",", parts, false);
		if (parts.Count() < 6)
			return;

		int skillN = parts[0].ToInt();
		int skillE = parts[1].ToInt();
		cfg.m_eAiSkillNormal = skillN;
		cfg.m_eAiSkillElite = skillE;
		cfg.m_fAiFireRateNormal = parts[2].ToFloat();
		cfg.m_fAiFireRateElite = parts[3].ToFloat();
		cfg.m_fAiPerceptionNormal = parts[4].ToFloat();
		cfg.m_fAiPerceptionElite = parts[5].ToFloat();
		cfg.ClampAiCombatSettings();
	}

	//------------------------------------------------------------------------------------------------
	static string PackDefenseExtras(notnull IA_Config cfg)
	{
		cfg.ClampDefenseSettings();
		int legacyI = 0;
		if (cfg.m_bUseLegacyDefense)
			legacyI = 1;

		string packed = legacyI.ToString();
		packed = packed + "," + cfg.m_iDefendDurationMinMin.ToString();
		packed = packed + "," + cfg.m_iDefendDurationMaxMin.ToString();
		packed = packed + "," + cfg.m_iDefendPrepareMinSec.ToString();
		packed = packed + "," + cfg.m_iDefendPrepareMaxSec.ToString();
		packed = packed + "," + cfg.m_iDefendGuaranteedEvents.ToString();
		packed = packed + "," + cfg.m_fDefendThirdChanceLow.ToString();
		packed = packed + "," + cfg.m_fDefendThirdChanceMid.ToString();
		packed = packed + "," + cfg.m_fDefendThirdChanceHigh.ToString();
		packed = packed + "," + cfg.m_iDefendPrioritySuccessMinSec.ToString();
		packed = packed + "," + cfg.m_iDefendPrioritySuccessMaxSec.ToString();
		packed = packed + "," + cfg.m_iDefendPriorityFailMinSec.ToString();
		packed = packed + "," + cfg.m_iDefendPriorityFailMaxSec.ToString();
		packed = packed + "," + cfg.GetDefenseDoctrineMask().ToString();
		packed = packed + "," + cfg.GetDefenseEventMask().ToString();
		packed = packed + "," + cfg.m_fDefendHotDropChance.ToString();
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	static void UnpackDefenseExtras(notnull IA_Config cfg, string packed)
	{
		if (packed.IsEmpty())
			return;

		ref array<string> parts = new array<string>();
		packed.Split(",", parts, false);
		if (parts.Count() < 16)
			return;

		cfg.m_bUseLegacyDefense = parts[0].ToInt() != 0;
		cfg.m_iDefendDurationMinMin = parts[1].ToInt();
		cfg.m_iDefendDurationMaxMin = parts[2].ToInt();
		cfg.m_iDefendPrepareMinSec = parts[3].ToInt();
		cfg.m_iDefendPrepareMaxSec = parts[4].ToInt();
		cfg.m_iDefendGuaranteedEvents = parts[5].ToInt();
		cfg.m_fDefendThirdChanceLow = parts[6].ToFloat();
		cfg.m_fDefendThirdChanceMid = parts[7].ToFloat();
		cfg.m_fDefendThirdChanceHigh = parts[8].ToFloat();
		cfg.m_iDefendPrioritySuccessMinSec = parts[9].ToInt();
		cfg.m_iDefendPrioritySuccessMaxSec = parts[10].ToInt();
		cfg.m_iDefendPriorityFailMinSec = parts[11].ToInt();
		cfg.m_iDefendPriorityFailMaxSec = parts[12].ToInt();
		cfg.SetDefenseDoctrineMask(parts[13].ToInt());
		cfg.SetDefenseEventMask(parts[14].ToInt());
		cfg.m_fDefendHotDropChance = parts[15].ToFloat();
		cfg.ClampDefenseSettings();
	}

 // No Getter methods. We reference the variables directly. 
 // Config access is handled through IA_MissionInitializer.GetGlobalConfig()
}