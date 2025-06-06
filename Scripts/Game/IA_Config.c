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

	[Attribute(defvalue: "0", UIWidgets.EditBox, category: "AI Scaling", desc: "Static AI Player Scale Factor Override (0 = use dynamic scaling, >0 = fixed scale factor)")]
	float m_fStaticAIScaleOverride;

	[Attribute(defvalue: "1.0", UIWidgets.EditBox, category: "AI Scaling", desc: "AI Player Scale Multiplier (multiplies dynamic scale factor, ignored if static override is set)")]
	float m_fAIScaleMultiplier;
 
 // No Getter methods. We reference the variables directly. 
 // Config access is handled through IA_MissionInitializer.GetGlobalConfig()
}