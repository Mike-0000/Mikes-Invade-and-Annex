[BaseContainerProps(configRoot: true)]
class IA_Config{

	// If any of these are set, it will override the defaults. If they are left empty, the program will revert back to default.
	// You only need to set the ones you want to override.
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "Enemy Factions", desc: "randomly rotated")]
	ref array<string> m_sDesiredEnemyFactionKeys;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "Enemy Factions", desc: "Enemy Factions for vehicles - Leave empty if you want vehicle faction to be the same as Enemy infantry faction")]
	ref array<string> m_sDesiredEnemyVehicleFactionKeys;
		
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific helicopters that will be used - at random - for generic helicopter spawns", params: "et")]
	ref array<ResourceName> m_aGenericHeliOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Attack Helicopters that will be used - at random - for attack helicopter spawns", params: "et")]
	ref array<ResourceName> m_aAttackHeliOverridePrefabs;

	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Transport Helicopters that will be used - at random - for transport helicopter spawns", params: "et")]
	ref array<ResourceName> m_aTransportHeliOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific VEHICLE_CAR's to spawn - ex. Humvee", params: "et")]
	ref array<ResourceName> m_aVehicleCarOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Armored vehicle's to spawn", params: "et")]
	ref array<ResourceName> m_aVehicleArmorOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific APC's to spawn", params: "et")]
	ref array<ResourceName> m_aAPC_OverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Truck's to spawn", params: "et")]
	ref array<ResourceName> m_aTruckOverridePrefabs;
 
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Medical Vehicle's to spawn", params: "et")]
	ref array<ResourceName> m_aMedicalVehicleOverridePrefabs;
	
	[Attribute(uiwidget: UIWidgets.Auto, category: "HQ Vehicles", desc: "Specific Medical Car's to spawn", params: "et")]
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

	[Attribute(defvalue: "true", UIWidgets.CheckBox, category: "Civilian & Resistance", desc: "Enable civilian spawning in captured zones")]
	bool m_bEnableCivilianSpawning;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Multiplier for civilian count calculation (0.5 = half, 2.0 = double)")]
	float m_fCivilianCountMultiplier;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Multiplier for civilian vehicle count calculation (0.5 = half, 2.0 = double)")]
	float m_fCivilianVehicleCountMultiplier;

	[Attribute(defvalue: "0.11", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Percentage of civilians killed to trigger a revolt (0.11 = 11%)")]
	float m_fCivilianRevoltThreshold;

	[Attribute(defvalue: "30000", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Delay (in ms) before showing the revolt notification")]
	int m_iCivilianRevoltNotificationDelay;

	[Attribute(defvalue: "180000", UIWidgets.EditBox, category: "Civilian & Resistance", desc: "Delay (in ms) before spawning revolt reinforcements")]
	int m_iCivilianRevoltReinforcementDelay;

	[Attribute(defvalue: "0.18", UIWidgets.Slider, category: "Artillery", desc: "Chance (0-1) for an artillery strike to occur during a check interval", params: "0 1 0.01")]
	float m_fArtilleryStrikeChance;

	[Attribute(defvalue: "300", UIWidgets.EditBox, category: "Artillery", desc: "Cooldown (in seconds) between artillery strikes")]
	int m_iArtilleryCooldown;

	[Attribute(defvalue: "45", UIWidgets.EditBox, category: "Artillery", desc: "Minimum delay (in seconds) from smoke to impact")]
	int m_iArtilleryMinDelay;

	[Attribute(defvalue: "70", UIWidgets.EditBox, category: "Artillery", desc: "Maximum delay (in seconds) from smoke to impact")]
	int m_iArtilleryMaxDelay;

 
 // No Getter methods. We reference the variables directly. 
 // Config access is handled through IA_MissionInitializer.GetGlobalConfig()
}