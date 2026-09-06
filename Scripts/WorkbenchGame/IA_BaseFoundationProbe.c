#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA foundation measurement", wbModules: {"ResourceManager"})]
class IA_BaseFoundationProbe : WorkbenchPlugin
{
	int m_iFailures;
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAFoundationProbe");
		BaseWorld world = preview.GetRef();
		array<ResourceName> meshes = {
			"{B56A3191B96761B5}Assets/Structures/Military/Camps/Foundation_TentUSSR_01/Foundation_TentUSSR_01.xob",
			"{454CCBB26BB830DB}Assets/Structures/Military/Camps/TentUSSR_01/TentUSSR_01.xob",
			"{A9C84183EA1C4E97}Assets/Structures/Military/Camps/TentUSSR_01/TentUSSR_01_floor.xob",
			"{E4FD0E74CDB6E9BA}Assets/Structures/Military/Camps/Canvas_Cover/CanvasCover_Large.xob",
			"{01E5327CE6F3006E}Assets/Structures/Military/CamoNets/CamoNet_Tent_Soviet.xob",
			"{D74B64FED52F4479}Assets/Props/Military/Sandbags/Sandbag_01_long_high.xob",
			"{8644C452B46FBF5C}Assets/Props/Military/Sandbags/Sandbag_01_wall.xob",
			"{6404EDF8B63C3B5B}Assets/Props/Military/Sandbags/Sandbag_01_wall_solid.xob",
			"{1FD4092C6933DD2C}Assets/Props/Military/Sandbags/Sandbag_01_round_high.xob"
		};
		foreach (ResourceName name : meshes)
		{
			Resource res = Resource.Load(name);
			IEntity ent = GetGame().SpawnEntity(GenericEntity, world);
			ent.SetObject(res.GetResource().ToVObject(), "");
			vector mins, maxs;
			ent.GetBounds(mins, maxs);
			Print(string.Format("[IA][Measure] %1 mins=%2 maxs=%3", name, mins, maxs), LogLevel.NORMAL);
			delete ent;
		}
		IEntity supply = GetGame().SpawnEntityPrefab(Resource.Load(IA_DynamicSiteLayout.PREFAB_SUPPLY), world);
		vector low = "99999 99999 99999";
		vector high = "-99999 -99999 -99999";
		MeasureHierarchy(supply, low, high);
		Print(string.Format("[IA][Measure] supply aggregate mins=%1 maxs=%2", low, high), LogLevel.NORMAL);
		Check(low[0] >= -4.5 && high[0] <= 4.5 && low[2] >= -5.5 && high[2] <= 5.5, "measured supply mesh stays inside support profile");
		SCR_EntityHelper.DeleteEntityAndChildren(supply);
		CheckSupportProfiles();
		Print("[IA][Measure] DONE", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	void Check(bool passed, string message)
	{
		if (!passed)
		{
			m_iFailures++;
			Print("[IA][SupportTest] FAIL " + message, LogLevel.ERROR);
		}
	}

	void CheckSupportProfiles()
	{
		for (int id = 0; id <= IA_DynamicSiteLayout.LAYOUT_RALLY_POST; id++)
		{
			IA_DynamicSiteLayout layout = IA_DynamicSiteLayout.CreateById(id);
			ref array<int> panels = {0, 0, 0, 0};
			foreach (IA_DynamicSiteModule panel : layout.m_aModules)
			{
				if (panel.m_iPerimeterSide >= 0)
					panels[panel.m_iPerimeterSide] = panels[panel.m_iPerimeterSide] + 1;
			}
			Check(layout.HasPerimeterCoverage(panels), "complete perimeter " + layout.m_sName);
			int rear = panels[0];
			panels[0] = 0;
			Check(!layout.HasPerimeterCoverage(panels), "reject missing side " + layout.m_sName);
			panels[0] = rear;
			for (int side = 0; side < 4; side++)
				panels[side] = Math.Ceil(panels[side] * 0.60);
			Check(!layout.HasPerimeterCoverage(panels), "reject sparse total " + layout.m_sName);
			foreach (IA_DynamicSiteModule mod : layout.m_aModules)
			{
				if (mod.m_fMaxFoundationLiftM <= 0)
					continue;
				float y;
				Check(mod.ResolveSupportHeight(100, 100, 100, y) && Math.AbsFloat(y - 100) < 0.001, "flat foundation " + mod.m_sId);
				Check(mod.ResolveSupportHeight(100, 99.8, 100.2, y) && Math.AbsFloat(y - 100.1) < 0.001, "gentle uneven ground " + mod.m_sId);
				Check(!mod.ResolveSupportHeight(100, 99, 101, y), "reject excessive bearing span " + mod.m_sId);
				Check(!mod.ResolveSupportHeight(100, 100, 100.7, y), "reject excessive lift even within span " + mod.m_sId);
				Check(mod.m_aSupportPoints.Count() > 5, "sample interior bearing terrain " + mod.m_sId);
				Check(mod.m_vSupportMaxs[2] == 4.75, "include asymmetric tent entrance extent " + mod.m_sId);
				foreach (vector point : mod.m_aSupportPoints)
					Check(Math.AbsFloat(point[0]) <= mod.m_fHalfWidthM && Math.AbsFloat(point[2]) <= mod.m_fHalfDepthM, "bearing inside reserved pad " + mod.m_sId);
			}
		}
		IA_DynamicSiteLayout small = IA_DynamicSiteLayout.CreateCommandPost();
		IA_DynamicSiteModule shelter = small.m_aModules[2];
		float height;
		Check(shelter.m_Prefab == IA_DynamicSiteLayout.PREFAB_SUPPLY, "supply fixture");
		Check(shelter.m_fHalfWidthM == 11 && shelter.m_fHalfDepthM == 11, "preserve supply clearance");
		// A 3% cross-slope rises 0.27 m across the actual nine-metre shelter,
		// compared with 0.66 m across its unrelated 22-metre reserved pad.
		Check(shelter.ResolveSupportHeight(100, 99.865, 100.135, height), "accept measured supply footprint on 3 percent slope");
		Check(height == 100 && shelter.m_fMaxFoundationLiftM == 0, "do not lift unbacked shelter");
		Check(!shelter.ResolveSupportHeight(100, 99.7, 100.3, height), "retain unbacked shelter tolerance");
		Print(string.Format("[IA][SupportTest] completed failures=%1", m_iFailures), LogLevel.NORMAL);
	}
	void MeasureHierarchy(IEntity ent, inout vector low, inout vector high)
	{
		if (!ent)
			return;
		if (ent.GetVObject())
		{
			vector mins, maxs, mat[4];
			ent.GetBounds(mins, maxs);
			ent.GetWorldTransform(mat);
			for (int i = 0; i < 8; i++)
			{
				vector corner = mins;
				for (int axis = 0; axis < 3; axis++)
				{
					if (i & (1 << axis))
						corner[axis] = maxs[axis];
				}
				vector p = mat[3] + mat[0] * corner[0] + mat[1] * corner[1] + mat[2] * corner[2];
				for (int axis = 0; axis < 3; axis++)
				{
					low[axis] = Math.Min(low[axis], p[axis]);
					high[axis] = Math.Max(high[axis], p[axis]);
				}
			}
		}
		IEntity child = ent.GetChildren();
		while (child)
		{
			MeasureHierarchy(child, low, high);
			child = child.GetSibling();
		}
	}

}
#endif
