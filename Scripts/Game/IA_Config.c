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

	[Attribute(defvalue: "0", UIWidgets.EditBox, category: "AI Scaling", desc: "Static AI Player Scale Factor Override (0 = use player-based scaling, >0 = fixed scale factor). Ignored while Dynamic AI Spawning is enabled; that tab's Dynamic AI scale is used instead.")]
	float m_fStaticAIScaleOverride;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Scaling", desc: "AI Player Scale Multiplier (multiplies player-based scale, ignored if static override is set). Ignored while Dynamic AI Spawning is enabled; that tab's Dynamic AI scale is used instead.")]
	float m_fAIScaleMultiplier;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "AI Scaling", desc: "Dynamic AI Spawning: repeatedly cache supported area infantry, ordinary patrols and garrisons while distant and out of combat, then restore their latest saved positions as players approach. Equipment/ammo reset. Caching kills downed AI. Disabling restores cached survivors.")]
	bool m_bDynamicAISpawningEnabled = false;

	static const int DYNAMIC_AI_BUDGET_DEFAULT = 70;
	static const int DYNAMIC_AI_BUDGET_MAX = 2000;
	static const float DYNAMIC_AI_SCALE_DEFAULT = 0.8;
	static const float DYNAMIC_AI_SCALE_MIN = 0.1;
	static const float DYNAMIC_AI_SCALE_MAX = 10;

	[Attribute(defvalue: "160", UIWidgets.EditBox, category: "AI Scaling", desc: "Dynamic AI Budget: target number of supported area soldiers present while Dynamic AI Spawning is enabled. Closest groups get priority. Protected nearby or fighting soldiers may exceed this target. 0 uses distance-only spawning. Independent of the Game Master budget.", params: "0 2000 1")]
	int m_iDynamicAIBudget = DYNAMIC_AI_BUDGET_DEFAULT;

	[Attribute(defvalue: "0.8", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "AI scale used while Dynamic AI Spawning is enabled. Replaces player-count scaling, the Scaling-tab multiplier, and the static override. 0.8 is 80 percent of baseline roster size. Changing this does not rebuild an already assigned roster.", params: "0.1 10 0.1")]
	float m_fDynamicAIScale = DYNAMIC_AI_SCALE_DEFAULT;

	[Attribute(defvalue: "1000", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Wake/admission distance in metres. Budget mode admits nearby optional squads; distance-only mode restores a whole team. Cannot be below the protected release distance.", params: "100 5000 1")]
	int m_iDynamicAIWakeDistanceM = 1000;

	[Attribute(defvalue: "1500", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Cache/retention distance in metres. Distance-only teams must be beyond this distance; budget allocations may persist out to it. At least 50 m beyond wake distance.", params: "150 7500 1")]
	int m_iDynamicAICacheDistanceM = 1500;

	[Attribute(defvalue: "300", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Close protection distance in metres. A budgeted group this close to any player restores its complete surviving roster.", params: "50 2000 1")]
	int m_iDynamicAICloseDistanceM = 300;

	[Attribute(defvalue: "400", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Protected release distance in metres. Close protection releases beyond this distance; individual soldiers inside it cannot be budget-evicted. At least 50 m beyond close distance.", params: "100 3000 1")]
	int m_iDynamicAIReleaseDistanceM = 400;

	[Attribute(defvalue: "60", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Continuous distant quiet time before whole-team caching in distance-only mode, in seconds.", params: "0 600 1")]
	int m_iDynamicAICacheQuietSec = 60;

	[Attribute(defvalue: "60", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Recent-combat protection time in seconds. Applies to budget mode and distance-only mode.", params: "10 300 1")]
	int m_iDynamicAICombatQuietSec = 60;

	[Attribute(defvalue: "30", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Minimum time a new or restored budgeted soldier stays live before eviction eligibility, in seconds.", params: "5 300 1")]
	int m_iDynamicAIMinLiveSec = 30;

	[Attribute(defvalue: "10", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Delay after a budget allocation becomes lower than the group's physical/reserved count before eviction, in seconds.", params: "1 120 1")]
	int m_iDynamicAIEvictDelaySec = 10;

	[Attribute(defvalue: "50", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "Distance ranking preference in metres for already allocated squads. Reduces repeated swaps between similarly distant groups.", params: "0 500 1")]
	int m_iDynamicAIRetentionBiasM = 50;

	[Attribute(defvalue: "30", UIWidgets.EditBox, category: "Dynamic AI Spawning", desc: "How long a contested objective may seed one defender over the soldier budget, in seconds.", params: "5 120 1")]
	int m_iDynamicAICaptureSeedSec = 30;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Dynamic AI Spawning", desc: "Hard cap: recent combat does not protect a squad from eviction. Live soldiers inside the keep-release distance still stay.")]
	bool m_bDynamicAIHardCap = false;

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

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Dynamic Base", desc: "After required AO objectives, attempt a seize-and-defend base using the normal defense settings.")]
	bool m_bDynamicBaseEnabled = true;

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Dynamic Base", desc: "Install optional finite-ammunition PKM/NSV stations in newly constructed bases. Active sites are unchanged.")]
	bool m_bDynamicBaseEmplacementsEnabled = true;

	[Attribute(defvalue: "100", UIWidgets.EditBox, category: "Dynamic Base", desc: "Chance (0-100) to select a dynamic base after required objectives.")]
	int m_iDynamicBaseChancePct = 100;

	[Attribute(defvalue: "false", UIWidgets.CheckBox, category: "Dynamic Base", desc: "Automatically start a dynamic base in Game Master mode.")]
	bool m_bDynamicBaseInGm = false;

	[Attribute(defvalue: "0", UIWidgets.EditBox, category: "Dynamic Base", desc: "0 Auto, 1 Full, 2 Compact, 3 Courtyard, 4 Roadside, 5 Command post, 6 Rally post.")]
	int m_iDynamicBaseSizeMode = 0;

	[Attribute(defvalue: "90", UIWidgets.EditBox, category: "Dynamic Base", desc: "Uncontested command-zone capture seconds.")]
	int m_iDynamicBaseCaptureSec = 90;

	[Attribute(defvalue: "90", UIWidgets.EditBox, category: "Dynamic Base", desc: "Deprecated: retained for saved-config compatibility; capture starts normal defense immediately.")]
	int m_iDynamicBaseRegroupMinSec = 90;

	[Attribute(defvalue: "240", UIWidgets.EditBox, category: "Dynamic Base", desc: "Deprecated: ignored. There is no separate base regroup or warning stage.")]
	int m_iDynamicBaseRegroupMaxSec = 240;

	[Attribute(defvalue: "0.60", UIWidgets.Slider, category: "Dynamic Base", desc: "Deprecated: ignored. Capturing the base completes assembly.", params: "0.1 1 0.01")]
	float m_fDynamicBaseRegroupFraction = 0.60;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "Dynamic Base", desc: "Garrison size multiplier (0.25-2.0).")]
	float m_fDynamicBaseGarrisonMultiplier = 1.0;

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
	static int ClampDynamicAIBudget(int budget)
	{
		if (budget < 0)
			return 0;
		if (budget > DYNAMIC_AI_BUDGET_MAX)
			return DYNAMIC_AI_BUDGET_MAX;
		return budget;
	}

	//------------------------------------------------------------------------------------------------
	static float ClampDynamicAIScale(float scale)
	{
		if (scale < DYNAMIC_AI_SCALE_MIN)
			return DYNAMIC_AI_SCALE_MIN;
		if (scale > DYNAMIC_AI_SCALE_MAX)
			return DYNAMIC_AI_SCALE_MAX;
		return scale;
	}

	//------------------------------------------------------------------------------------------------
	void ClampDynamicAISettings()
	{
		m_iDynamicAIBudget = ClampDynamicAIBudget(m_iDynamicAIBudget);
		m_fDynamicAIScale = ClampDynamicAIScale(m_fDynamicAIScale);
		m_iDynamicAICloseDistanceM = Math.Clamp(m_iDynamicAICloseDistanceM, 50, 2000);
		m_iDynamicAIReleaseDistanceM = Math.Clamp(m_iDynamicAIReleaseDistanceM, 100, 3000);
		m_iDynamicAIReleaseDistanceM = Math.Max(m_iDynamicAIReleaseDistanceM, m_iDynamicAICloseDistanceM + 50);
		m_iDynamicAIWakeDistanceM = Math.Clamp(m_iDynamicAIWakeDistanceM, 100, 5000);
		m_iDynamicAIWakeDistanceM = Math.Max(m_iDynamicAIWakeDistanceM, m_iDynamicAIReleaseDistanceM);
		m_iDynamicAICacheDistanceM = Math.Clamp(m_iDynamicAICacheDistanceM, 150, 7500);
		m_iDynamicAICacheDistanceM = Math.Max(m_iDynamicAICacheDistanceM, m_iDynamicAIWakeDistanceM + 50);
		m_iDynamicAICacheQuietSec = Math.Clamp(m_iDynamicAICacheQuietSec, 0, 600);
		m_iDynamicAICombatQuietSec = Math.Clamp(m_iDynamicAICombatQuietSec, 10, 300);
		m_iDynamicAIMinLiveSec = Math.Clamp(m_iDynamicAIMinLiveSec, 5, 300);
		m_iDynamicAIEvictDelaySec = Math.Clamp(m_iDynamicAIEvictDelaySec, 1, 120);
		m_iDynamicAIRetentionBiasM = Math.Clamp(m_iDynamicAIRetentionBiasM, 0, 500);
		m_iDynamicAICaptureSeedSec = Math.Clamp(m_iDynamicAICaptureSeedSec, 5, 120);
	}

	//------------------------------------------------------------------------------------------------
	static string PackDynamicAIExtras(notnull IA_Config cfg)
	{
		cfg.ClampDynamicAISettings();
		string packed = cfg.m_iDynamicAIWakeDistanceM.ToString();
		packed = packed + "," + cfg.m_iDynamicAICacheDistanceM.ToString();
		packed = packed + "," + cfg.m_iDynamicAICloseDistanceM.ToString();
		packed = packed + "," + cfg.m_iDynamicAIReleaseDistanceM.ToString();
		packed = packed + "," + cfg.m_iDynamicAICacheQuietSec.ToString();
		packed = packed + "," + cfg.m_iDynamicAICombatQuietSec.ToString();
		packed = packed + "," + cfg.m_iDynamicAIMinLiveSec.ToString();
		packed = packed + "," + cfg.m_iDynamicAIEvictDelaySec.ToString();
		packed = packed + "," + cfg.m_iDynamicAIRetentionBiasM.ToString();
		packed = packed + "," + cfg.m_iDynamicAICaptureSeedSec.ToString();
		if (cfg.m_bDynamicAIHardCap)
			packed = packed + ",1";
		else
			packed = packed + ",0";
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	static bool UnpackDynamicAIExtras(notnull IA_Config cfg, string packed)
	{
		ref array<string> parts = {};
		packed.Split(",", parts, false);
		if (parts.Count() != 9 && parts.Count() != 11)
			return false;
		ref array<int> values = {};
		foreach (string token : parts)
		{
			int value;
			// Nine digits plus optional sign are not needed for these settings;
			// reject oversized tokens before native integer parsing can overflow.
			if (token.Length() > 9 || !IA_DynamicParse.TryParseIntToken(token, value))
				return false;
			values.Insert(value);
		}
		// Do not apply any field until every token has passed validation.
		cfg.m_iDynamicAIWakeDistanceM = values[0];
		cfg.m_iDynamicAICacheDistanceM = values[1];
		cfg.m_iDynamicAICloseDistanceM = values[2];
		cfg.m_iDynamicAIReleaseDistanceM = values[3];
		cfg.m_iDynamicAICacheQuietSec = values[4];
		cfg.m_iDynamicAICombatQuietSec = values[5];
		cfg.m_iDynamicAIMinLiveSec = values[6];
		cfg.m_iDynamicAIEvictDelaySec = values[7];
		cfg.m_iDynamicAIRetentionBiasM = values[8];
		if (values.Count() == 11)
		{
			cfg.m_iDynamicAICaptureSeedSec = values[9];
			cfg.m_bDynamicAIHardCap = values[10] != 0;
		}
		cfg.ClampDynamicAISettings();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryParseDynamicAIBudget(string token, out int budget)
	{
		budget = 0;
		// Reject overflow-sized input instead of letting it wrap to the disabling value.
		if (token.Length() > 9 || !IA_DynamicParse.TryParseIntToken(token, budget))
			return false;
		budget = ClampDynamicAIBudget(budget);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string PackDynamicAIBudget(notnull IA_Config cfg)
	{
		cfg.ClampDynamicAISettings();
		return cfg.m_iDynamicAIBudget.ToString();
	}

	//------------------------------------------------------------------------------------------------
	static bool UnpackDynamicAIBudget(notnull IA_Config cfg, string token)
	{
		int budget;
		if (!TryParseDynamicAIBudget(token, budget))
			return false;
		cfg.m_iDynamicAIBudget = budget;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryParseDynamicAIScale(string token, out float scale)
	{
		scale = DYNAMIC_AI_SCALE_DEFAULT;
		if (!IA_DynamicParse.TryParseFloatToken(token, scale))
			return false;
		scale = ClampDynamicAIScale(scale);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string PackDynamicAIScale(notnull IA_Config cfg)
	{
		cfg.ClampDynamicAISettings();
		return cfg.m_fDynamicAIScale.ToString();
	}

	//------------------------------------------------------------------------------------------------
	static bool UnpackDynamicAIScale(notnull IA_Config cfg, string token)
	{
		float scale;
		if (!TryParseDynamicAIScale(token, scale))
			return false;
		cfg.m_fDynamicAIScale = scale;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ClampDynamicBaseSettings()
	{
		if (m_iDynamicBaseChancePct < 0)
			m_iDynamicBaseChancePct = 0;
		if (m_iDynamicBaseChancePct > 100)
			m_iDynamicBaseChancePct = 100;

		if (m_iDynamicBaseSizeMode < 0 || m_iDynamicBaseSizeMode > IA_DynamicSiteSizeMode.RallyPost)
			m_iDynamicBaseSizeMode = 0;

		if (m_iDynamicBaseCaptureSec < 30)
			m_iDynamicBaseCaptureSec = 30;
		if (m_iDynamicBaseCaptureSec > 300)
			m_iDynamicBaseCaptureSec = 300;

		if (m_iDynamicBaseRegroupMinSec < 0)
			m_iDynamicBaseRegroupMinSec = 0;
		if (m_iDynamicBaseRegroupMinSec > 300)
			m_iDynamicBaseRegroupMinSec = 300;

		if (m_iDynamicBaseRegroupMaxSec < m_iDynamicBaseRegroupMinSec)
			m_iDynamicBaseRegroupMaxSec = m_iDynamicBaseRegroupMinSec;
		if (m_iDynamicBaseRegroupMaxSec > 600)
			m_iDynamicBaseRegroupMaxSec = 600;

		if (m_fDynamicBaseRegroupFraction < 0.10)
			m_fDynamicBaseRegroupFraction = 0.10;
		if (m_fDynamicBaseRegroupFraction > 1.0)
			m_fDynamicBaseRegroupFraction = 1.0;

		if (m_fDynamicBaseGarrisonMultiplier < 0.25)
			m_fDynamicBaseGarrisonMultiplier = 0.25;
		if (m_fDynamicBaseGarrisonMultiplier > 2.0)
			m_fDynamicBaseGarrisonMultiplier = 2.0;
	}

	//------------------------------------------------------------------------------------------------
	static string PackDynamicBaseExtras(notnull IA_Config cfg)
	{
		cfg.ClampDynamicBaseSettings();
		int enabledI = 0;
		if (cfg.m_bDynamicBaseEnabled)
			enabledI = 1;
		int gmI = 0;
		if (cfg.m_bDynamicBaseInGm)
			gmI = 1;

		int emplacementsI = 0;
		if (cfg.m_bDynamicBaseEmplacementsEnabled)
			emplacementsI = 1;
		string packed = "2";
		packed = packed + "," + enabledI.ToString();
		packed = packed + "," + cfg.m_iDynamicBaseChancePct.ToString();
		packed = packed + "," + gmI.ToString();
		packed = packed + "," + cfg.m_iDynamicBaseSizeMode.ToString();
		packed = packed + "," + cfg.m_iDynamicBaseCaptureSec.ToString();
		packed = packed + "," + cfg.m_iDynamicBaseRegroupMinSec.ToString();
		packed = packed + "," + cfg.m_iDynamicBaseRegroupMaxSec.ToString();
		packed = packed + "," + cfg.m_fDynamicBaseRegroupFraction.ToString();
		packed = packed + "," + cfg.m_fDynamicBaseGarrisonMultiplier.ToString();
		packed = packed + "," + emplacementsI.ToString();
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	static void UnpackDynamicBaseExtras(notnull IA_Config cfg, string packed)
	{
		if (packed.IsEmpty())
			return;

		ref array<string> parts = new array<string>();
		packed.Split(",", parts, false);
		if (parts.Count() != 10 && parts.Count() != 11)
			return;

		int version;
		int enabledI;
		int chancePct;
		int gmI;
		int sizeMode;
		int captureSec;
		int regroupMin;
		int regroupMax;
		float regroupFrac;
		float garrisonMult;
		if (!IA_DynamicParse.TryParseIntToken(parts[0], version))
			return;
		if ((version != 1 || parts.Count() != 10) && (version != 2 || parts.Count() != 11))
			return;
		int emplacementsI = 1; // old snapshots retain the new-setting default
		if (version == 2)
		{
			if (!IA_DynamicParse.TryParseIntToken(parts[10], emplacementsI))
				return;
			if (emplacementsI != 0 && emplacementsI != 1)
				return;
		}
		if (!IA_DynamicParse.TryParseIntToken(parts[1], enabledI))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[2], chancePct))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[3], gmI))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[4], sizeMode))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[5], captureSec))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[6], regroupMin))
			return;
		if (!IA_DynamicParse.TryParseIntToken(parts[7], regroupMax))
			return;
		if (!IA_DynamicParse.TryParseFloatToken(parts[8], regroupFrac))
			return;
		if (!IA_DynamicParse.TryParseFloatToken(parts[9], garrisonMult))
			return;

		cfg.m_bDynamicBaseEnabled = enabledI != 0;
		cfg.m_bDynamicBaseEmplacementsEnabled = emplacementsI != 0;
		cfg.m_iDynamicBaseChancePct = chancePct;
		cfg.m_bDynamicBaseInGm = gmI != 0;
		cfg.m_iDynamicBaseSizeMode = sizeMode;
		cfg.m_iDynamicBaseCaptureSec = captureSec;
		cfg.m_iDynamicBaseRegroupMinSec = regroupMin;
		cfg.m_iDynamicBaseRegroupMaxSec = regroupMax;
		cfg.m_fDynamicBaseRegroupFraction = regroupFrac;
		cfg.m_fDynamicBaseGarrisonMultiplier = garrisonMult;
		cfg.ClampDynamicBaseSettings();
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
