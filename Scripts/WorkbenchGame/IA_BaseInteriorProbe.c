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
			"{C1DC3475A54F5B8B}Prefabs/DynamicBase/IA_Dressing_Briefing.et",
			"{C5FCEDF014885D44}Prefabs/DynamicBase/IA_Dressing_Mess.et",
			"{EBACA024C71C5CAF}Prefabs/DynamicBase/IA_Dressing_Stores.et",
			"{9F0EFC4CC9CD571D}Prefabs/DynamicBase/IA_Dressing_Utility.et",
			"{B3E06835E2455923}Prefabs/DynamicBase/IA_Dressing_Water.et",
			"{3A2537AB39405511}Prefabs/DynamicBase/IA_Dressing_Workshop.et"
        };
        foreach (ResourceName name : resources)
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
