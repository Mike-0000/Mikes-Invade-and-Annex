#ifdef WORKBENCH
// Measures the stock structures that tools/author_headquarters.py wraps.
// Preview geometry only: this proves the resources load and gives mesh bounds,
// not live replication, destruction or AI behaviour.
[WorkbenchPluginAttribute(name: "IA headquarters asset measurement", wbModules: {"ResourceManager"})]
class IA_HeadquartersAssetProbe : IA_BaseCompositionProbe
{
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAHeadquartersAssetProbe");
		BaseWorld world = preview.GetRef();
		m_World = world;
		Measure(world, "ConcreteWallUSSR_V1", "{27EB771E3DD9C354}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V1.et");
		Measure(world, "ConcreteWallUSSR_V2", "{4BBF43FE811D4C7A}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V2.et");
		Measure(world, "ConcreteWallUSSR_V3", "{AE230F078DF8DB11}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V3.et");
		Measure(world, "ConcreteWallUSSR_Pillar", "{850FA846E37E385D}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_pillar.et");
		Measure(world, "ConcreteWallUSSR_Single", "{9A25B3AF92191E1F}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_single.et");
		Measure(world, "ConcreteWall01_3m", "{60E5729BEA18F875}Prefabs/Structures/Walls/Concrete/ConcreteWall_01/ConcreteWall_01_3m.et");
		Measure(world, "ConcreteWall01_6mA", "{2FBF5DEDFA25D947}Prefabs/Structures/Walls/Concrete/ConcreteWall_01/ConcreteWall_01_6m_A.et");
		Measure(world, "ConcreteWall02_4m", "{879865F07EA907B0}Prefabs/Structures/Walls/Concrete/ConcreteWall_02/ConcreteWall_02_4m.et");
		Measure(world, "Bunker01_1fp", "{44B2776836744D69}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_1fp.et");
		Measure(world, "Bunker01_3fp", "{EE5219B38424AB12}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_3fp.et");
		Measure(world, "Bunker01_1fpCamo", "{A2726797F70F52D8}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_1fp_Camo.et");
		Measure(world, "BunkerSPS", "{6214C73708EA0E2D}Prefabs/Structures/Military/Fortifications/Bunker_SPS/Bunker_SPS.et");
		Measure(world, "BunkerSPS2M", "{BA8463780B060A68}Prefabs/Structures/Military/Fortifications/Bunker_SPS/Bunker_SPS2M.et");
		Measure(world, "GuardTowerUSSR", "{52D3F2118C1B68E0}Prefabs/Structures/Military/Houses/GuardTower_USSR_01/GuardTower_USSR_01_green.et");
		Measure(world, "GuardTower01", "{FBCE1861B987EA68}Prefabs/Structures/Military/Houses/GuardTower_01/GuardTower_01.et");
		Measure(world, "GuardHouse01", "{3F91DEEC9C78E473}Prefabs/Structures/Military/Houses/GuardHouse_01/GuardHouse_01.et");
		Measure(world, "GuardBox01", "{F50905235FBAA094}Prefabs/Structures/Military/Houses/GuardBox_01/GuardBox_01_beige.et");
		Measure(world, "BarracksHQ", "{173E31DE5DE36ABA}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_USSR_01_headquarters_base.et");
		Measure(world, "BarracksMilitary", "{2CB4D91249389DFD}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_USSR_01_military_base.et");
		Measure(world, "ShelterMilitary", "{4BE4C27399CF3B00}Prefabs/Structures/Military/Bunkers/ShelterMilitary_E_01/ShelterMilitary_E_01.et");
		Measure(world, "AmmoDump02", "{0921B79E3B234115}Prefabs/Structures/Military/Bunkers/AmmoDump_E_02/AmmoDump_E_02_base.et");
		Measure(world, "Dragontooth_V1", "{C16C62041E2AAAB2}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V1.et");
		Measure(world, "Dragontooth_V2", "{8B7B5323534404A7}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V2.et");
		Measure(world, "VehicleObstacle01", "{9E46573024812588}Prefabs/Props/Military/AntiVehicleObstacle_USSR_01/VehicleObstacle_USSR_01.et");
		Measure(world, "VehicleObstacle02", "{AB6514FF76709AB3}Prefabs/Props/Military/AntiVehicleObstacle_USSR_02/VehicleObstacle_USSR_02.et");
		Measure(world, "Hedgehog", "{A46D45922F8CAD6C}Prefabs/Props/Military/Fortification/CzechHedgehog_01_painted.et");
		Measure(world, "BarbedTriple", "{93E06E731212BD96}Prefabs/Structures/Walls/BarbedWire/BarbedTape_01/BarbedTape_01_Triple.et");
		Measure(world, "BarbedCoil", "{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et");
		Measure(world, "KnifeRest", "{DDF59362051B28BC}Prefabs/Props/Military/Fortification/BarbedTape_KnifeRest.et");
		Measure(world, "NetFence6m", "{40FD7452AC292C40}Prefabs/Structures/Walls/BarbedWire/NetFence_USSR_01/NetFence_USSR_01_6m_V1.et");
		Measure(world, "FlagPoleUSSR", "{651E545B53BBD034}Prefabs/Structures/Military/Flags/FlagPole_02/FlagPole_02_V1_USSR.et");
		Measure(world, "Floodlight", "{616F4E93658E5A3A}Prefabs/Props/Military/Generators/GeneratorFloodlight_USSR_01.et");
		Measure(world, "ShelterStorage", "{D7B21E69929A0546}Prefabs/Structures/Military/Camps/Shelters/ShelterStorageUSSR_01.et");
		Measure(world, "DirtCoverLong", "{39F7EE885940DC8F}Prefabs/Structures/Military/Fortifications/DirtCover_01/DirtCover_01_long_v1.et");
		Measure(world, "SandbagBunker", "{F0EC223F2A1094C0}Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_burlap.et");
		Print(string.Format("[IA][HeadquartersAssetProbe] failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}
}
#endif
