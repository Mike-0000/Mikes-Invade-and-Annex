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

	[Attribute(defvalue: "300", UIWidgets.EditBox, category: "Artillery", desc: "Cooldown (in seconds) between artillery strikes")]
	int m_iArtilleryCooldown;

	[Attribute(defvalue: "45", UIWidgets.EditBox, category: "Artillery", desc: "Minimum delay (in seconds) from smoke to impact")]
	int m_iArtilleryMinDelay;

	[Attribute(defvalue: "70", UIWidgets.EditBox, category: "Artillery", desc: "Maximum delay (in seconds) from smoke to impact")]
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

 
 // No Getter methods. We reference the variables directly. 
 // Config access is handled through IA_MissionInitializer.GetGlobalConfig()
}