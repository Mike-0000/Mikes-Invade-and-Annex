#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA interior asset measurement", wbModules: {"ResourceManager"})]
class IA_BaseInteriorProbe : IA_BaseFoundationProbe
{
    override void RunCommandline()
    {
        ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAInteriorProbe");
        BaseWorld world = preview.GetRef();
        array<ResourceName> resources = {
			"{D6C4CB6E5D13C8C7}Prefabs/Structures/Civilian/MessageBoard/Filled/MessageBoard_USSR_01_RedStarNews_filled_V1.et",
			"{1A1020205E2B1710}Prefabs/Props/Military/Furniture/TableMilitary_USSR_01.et",
			"{172DD50ACF177B9E}Prefabs/Props/Military/Furniture/ChairMilitary_USSR_01.et",
			"{E4416C4C6938BEEE}Prefabs/Props/Crates/CrateWooden_02/CrateWooden_02_1x1x1.et",
			"{339AED2BCBDF3265}Prefabs/Props/Military/Camps/PalletMRE_01_Soviet.et",
			"{F4BB6B6A0486F797}Prefabs/Props/Industrial/BarrelMetal_01_military.et",
			"{62B2ADC4243454F9}Prefabs/Props/Civilian/Jerrycan_01.et",
			"{BCE07A58672955B9}Prefabs/Props/Civilian/Bucket_01.et",
			"{0F866B72D473D8C4}Prefabs/Props/Industrial/ToolBox_01/ToolBox_01_grey.et",
			"{B3D9BC5688598B6E}Prefabs/Props/VehicleParts/Tires/Wheel_Ural4320.et",
			"{A419F4547954DA05}Prefabs/Props/Construction/CableReel/CableReelWooden_01.et",
			"{0879933F4FDD2C2B}Prefabs/Props/Civilian/SackUniversal_01_Pile.et",
			"{FCBEDECB2A39B6BC}Prefabs/Props/Military/Generators/GeneratorPortable_USSR_01.et",
			"{616F4E93658E5A3A}Prefabs/Props/Military/Generators/GeneratorFloodlight_USSR_01.et",
			"{5E18F5832C69F8A9}Prefabs/Props/Military/Camps/Lamp_Interactive.et",
			"{AA96C0FB1A65B886}Prefabs/Props/Military/FieldKitchenTrailerUSSR_01.et",
			"{7A04C0C85BEC7916}Prefabs/Structures/Civilian/Latrine_01.et",
			"{97FF4DA84258FBAF}Prefabs/Props/Civilian/TableSink_01.et",
			"{C7BE6D1395342D32}Prefabs/Props/Agriculture/WaterTank_01_v1.et",
			"{BD65F1D86388640D}Prefabs/Props/Civilian/FireExtinguisher_01.et",
			"{D7F69BF7D2DCA0A8}Prefabs/Props/Industrial/Pallets/Pallet_01.et",
			"{1AC31E2AF0C54F8A}Prefabs/Props/Military/Camps/PalletAmmo_01_Soviet_camo.et",
			"{387B85277E063AEA}Prefabs/Props/EmptyStorages/USSR_MedicalBox_empty.et",
			"{F0E4FC2785C49124}Prefabs/Props/Garbage/Bins/TrashBin_03/TrashBin_USSR_03_full.et",
			"{88A1A9164B53E2E5}Prefabs/Props/Industrial/Pallets/MarsBox_01_green.et",
			"{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et",
			"{E139195E3A6231D9}Prefabs/Props/Signs/Industrial/SignIndustrial_30x20_1_NoSmoking.et",
			"{C1DC3475A54F5B8B}Prefabs/DynamicBase/IA_Dressing_Briefing.et",
			"{986F4B80B0BE52E0}Prefabs/DynamicBase/IA_Dressing_BriefingLit.et",
			"{CE0933F9935A5FD4}Prefabs/DynamicBase/IA_Dressing_BulkWater.et",
			"{BB26A698BEF55499}Prefabs/DynamicBase/IA_Dressing_Comms.et",
			"{D7D10DFBACA05AD0}Prefabs/DynamicBase/IA_Dressing_CommsLit.et",
			"{8726E742FEE65661}Prefabs/DynamicBase/IA_Dressing_EntranceLight.et",
			"{0138CE87F30058BE}Prefabs/DynamicBase/IA_Dressing_Kitchen.et",
			"{7381F407AD8A54B8}Prefabs/DynamicBase/IA_Dressing_KitchenLit.et",
			"{AC27F856D7A750A7}Prefabs/DynamicBase/IA_Dressing_Medical.et",
			"{A9D76F51AE7B5D37}Prefabs/DynamicBase/IA_Dressing_MedicalLit.et",
			"{C5FCEDF014885D44}Prefabs/DynamicBase/IA_Dressing_Mess.et",
			"{06111FFC070A5834}Prefabs/DynamicBase/IA_Dressing_MessLit.et",
			"{3F22F8B9E0EB5442}Prefabs/DynamicBase/IA_Dressing_Power.et",
			"{754823CC537E5D85}Prefabs/DynamicBase/IA_Dressing_Rest.et",
			"{6F3672FEE4B857A1}Prefabs/DynamicBase/IA_Dressing_Sanitation.et",
			"{EBACA024C71C5CAF}Prefabs/DynamicBase/IA_Dressing_Stores.et",
			"{674112EA1BA85649}Prefabs/DynamicBase/IA_Dressing_StoresCovered.et",
			"{9F0EFC4CC9CD571D}Prefabs/DynamicBase/IA_Dressing_Utility.et",
			"{A70917953C1D58AB}Prefabs/DynamicBase/IA_Dressing_Waste.et",
			"{B3E06835E2455923}Prefabs/DynamicBase/IA_Dressing_Water.et",
			"{AB8A3CB589625084}Prefabs/DynamicBase/IA_Dressing_WaterWash.et",
			"{3A2537AB39405511}Prefabs/DynamicBase/IA_Dressing_Workshop.et",
			"{965FEDAFCD3F54D5}Prefabs/DynamicBase/IA_Dressing_WorkshopLit.et",
			"{BA4D420FC21458D4}Prefabs/DynamicBase/IA_InfrastructureAsset_covered.et",
			"{099D5A2F225859E5}Prefabs/DynamicBase/IA_InfrastructureAsset_electric.et",
			"{1A0003104F1D5AA2}Prefabs/DynamicBase/IA_InfrastructureAsset_floodlight.et",
			"{BF76579A28EF5EA1}Prefabs/DynamicBase/IA_InfrastructureAsset_generator.et",
			"{C1C087BC66975A4E}Prefabs/DynamicBase/IA_InfrastructureAsset_lamp.et",
			"{F7A71209CE045FD3}Prefabs/DynamicBase/IA_InfrastructureAsset_latrine.et",
			"{D8FF66B445235D0F}Prefabs/DynamicBase/IA_InfrastructureAsset_medical.et",
			"{CFB089173D9D52F4}Prefabs/DynamicBase/IA_InfrastructureAsset_nosmoking.et"
        };
        array<int> expectedChildren = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 5, 3, 3, 4, 1, 4, 5, 4, 5, 6, 6, 6, 3, 3, 5, 4, 4, 2, 5, 4, 5, 6, 1, 0, 0, 0, 0, 1, 0, 0};
        foreach (int index, ResourceName name : resources)
        {
            IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(name), world);
            if (!ent)
            {
                m_iFailures++;
                Print("[IA][InteriorMeasure] FAILED " + name, LogLevel.ERROR);
                continue;
            }
            vector low = "99999 99999 99999";
            vector high = "-99999 -99999 -99999";
            MeasureHierarchy(ent, low, high);
            int children;
            IEntity child = ent.GetChildren();
            while (child)
            {
                children++;
                child = child.GetSibling();
            }
            Check(children >= expectedChildren[index], "all authored children attached: " + name);
            if (low[0] > high[0])
            {
                m_iFailures++;
                Print("[IA][InteriorMeasure] FAILED empty hierarchy " + name, LogLevel.ERROR);
            }
            Print(string.Format("[IA][InteriorMeasure] %1 mins=%2 maxs=%3", ent.GetPrefabData().GetPrefabName(), low, high), LogLevel.NORMAL);
            SCR_EntityHelper.DeleteEntityAndChildren(ent);
        }
        Print(string.Format("[IA][InteriorMeasure] failures=%1", m_iFailures), LogLevel.NORMAL);
        Workbench.Exit(m_iFailures);
    }
}
#endif
