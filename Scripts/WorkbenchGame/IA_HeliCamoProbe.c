#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Play-mode probe for a camouflage pattern that does not depend on a helicopter's UV layout.
//! Screenshots go to the profile folder as IA_HeliCamoProbe_<name>.bmp.png.
//!   -iaCamoMode 1  blobs projected as decals onto a stock Huey, which is then moved and turned
//!   -iaCamoMode 2  the same on an Mi-8
//!   -iaCamoMode 3  a paint channel Huey whose channel material is edited by hand for the run
//!                  (-iaCamoChannel picks the channel), moved and turned to see whether its pattern
//!                  stays on the hull
//!   -iaCamoMode 5  what a material accepts at run time: a texture, a UV source, a new material
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter camo probe", wbModules: {"ResourceManager"})]
class IA_HeliCamoProbe : WorkbenchPlugin
{
	protected static const string WORLD = "worlds/Showcase/PBR_Vehicles.ent";
	protected static const ResourceName HUEY = "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et";
	protected static const ResourceName MI8 = "{DF5CCB7C0FF049F4}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et";
	protected static const ResourceName BLOB = "{E83A241E3710C32F}Assets/Decals/Asphalt/Patch/Data/Decal_Asphalt_Patch_Blob_01.emat";
	protected static const ResourceName PATTERN = "{23CB2FC0292A4C28}Assets/Weapons/Rifles/M16A2/Data/T_OliveGreen_Sand_Stripes_BC.edds";
	protected static const int GAME_MODE_TIMEOUT_MS = 240000;
	protected static const int PROJECTORS = 260;
	protected static const ResourceName PAINT = "{BB440784A103CB06}Assets/_SharedData/Metal/ST_MetalPaint_Coated_1m_BCR.edds";
	protected static const string MADE_BLOB = "IA_HeliCamoProbe_Blob";
	protected static const ResourceName BLOB_ALPHA = "{E4D7209795F9170F}Assets/Decals/Asphalt/Patch/Data/Decal_Asphalt_Patch_Blob_01_A.edds";
	protected static const float PROJECT_DEPTH_M = 0.7;

	protected vector m_vSide;
	protected vector m_vAway;
	protected ref array<IEntity> m_aParts = {};
	protected ref array<Decal> m_aDecals = {};
	protected ref array<ref Material> m_aMade = {};
	protected int m_iMadeLevel = 1;
	protected bool m_bDiagnose;
	// The decal material blobs are projected with: a vanilla one, or one made at run time.
	protected string m_sBlob = BLOB;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		string arg;
		int mode = 1;
		if (System.GetCLIParam("iaCamoMode", arg) && !arg.IsEmpty())
			mode = arg.ToInt();
		int channel = 12;
		if (System.GetCLIParam("iaCamoChannel", arg) && !arg.IsEmpty())
			channel = arg.ToInt();
		float distance = 13;
		if (System.GetCLIParam("iaCamoDistance", arg) && !arg.IsEmpty())
			distance = arg.ToFloat();

		int failures = Probe(mode, channel, distance);
		if (failures == 0)
			Print(string.Format("[IA][HeliCamoProbe] PASS mode=%1", mode), LogLevel.NORMAL);
		else
			Print(string.Format("[IA][HeliCamoProbe] FAIL mode=%1 failures=%2", mode, failures), LogLevel.ERROR);
		Workbench.Exit(failures);
	}

	//------------------------------------------------------------------------------------------------
	protected int Probe(int mode, int channel, float distance)
	{
		Mark(string.Format("start mode=%1 channel=%2", mode, channel));
		Workbench.OpenModule(WorldEditor);
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || !editor.SetOpenedResource(WORLD))
		{
			Print("[IA][HeliCamoProbe] FAIL: the test world did not open", LogLevel.ERROR);
			return 1;
		}

		Sleep(500);
		editor.SwitchToGameMode(false, false);
		int waited;
		while (editor.GetApi() && !editor.GetApi().IsGameMode())
		{
			Sleep(100);
			waited = waited + 100;
			if (waited > GAME_MODE_TIMEOUT_MS)
			{
				Print("[IA][HeliCamoProbe] FAIL: game mode did not start", LogLevel.ERROR);
				return 1;
			}
		}
		Mark("game mode");
		Sleep(8000);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[IA][HeliCamoProbe] FAIL: no game world", LogLevel.ERROR);
			return 1;
		}

		vector camera[4];
		world.GetCurrentCamera(camera);
		m_vAway = camera[2];
		m_vAway[1] = 0;
		m_vAway.Normalize();
		m_vSide = camera[0];
		m_vSide[1] = 0;
		m_vSide.Normalize();
		vector origin = camera[3] + m_vAway * distance;
		origin[1] = Math.Max(world.GetSurfaceY(origin[0], origin[2]), 0) + 0.3;

		string arg;
		if (System.GetCLIParam("iaCamoMade", arg) && !arg.IsEmpty())
		{
			m_iMadeLevel = arg.ToInt();
			if (mode != 9)
				MakeBlobMaterial();
		}

		m_bDiagnose = System.GetCLIParam("iaCamoDiagnose", arg) && arg == "1";

		int failures;
		if (mode == 1)
			failures = ProbeDecals(world, origin, HUEY, "huey");
		else if (mode == 2)
			failures = ProbeDecals(world, origin, MI8, "mi8");
		else if (mode == 3)
			failures = ProbeChannelMaterial(world, origin, channel);
		else if (mode == 6)
			failures = ProbeDecalVariants(world, origin, HUEY);
		else if (mode == 7)
			failures = ProbeDecalVariants(world, origin, MI8);
		else if (mode == 8)
			failures = ProbeProjectorVariants(world, origin, HUEY);
		else if (mode == 9)
			failures = ProbeSweep(world, origin, HUEY);
		else
			failures = ProbeMaterialApi(world, origin);

		Sleep(3000);
		Mark(string.Format("survived failures=%1", failures));
		editor.SwitchToEditMode();
		Sleep(1000);
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! A paint-only decal material built from script: a blob-shaped mask over plain paint.
	//! \param level 0 mask and paint only, 1 adds blending, 2 also keeps the hull's own normal,
	//! roughness, metalness and occlusion, 3 is paint with no mask
	protected string MakeBlob(int level)
	{
		string name = string.Format("%1_%2", MADE_BLOB, level);
		ref map<string, string> params = new map<string, string>();
		params.Insert("BCRMap", PAINT);
		params.Insert("Color", "1 1 1 1");
		if (level != 3)
			params.Insert("OpacityMap", BLOB_ALPHA);
		string materialClass = "MatPBRDecal";
		if (level == 5)
			materialClass = "MatPBRDecalSkinned";
		if (level == 4 || level == 5)
		{
			// What the vanilla hit indicator sets so that it wins the depth test on a hull.
			params.Insert("ZBias", "1");
			params.Insert("Sort", "decal");
			params.Insert("SortBias", "2");
			params.Insert("ZWrite", "0");
		}
		if (level == 1 || level == 2 || level == 4 || level == 5)
		{
			params.Insert("BlendMode", "AlphaBlend");
			params.Insert("AlphaTest", "0.3");
			params.Insert("AlphaMul", "3");
		}
		if (level == 2)
		{
			params.Insert("GBufferNormal", "1");
			params.Insert("GBufferRoughness", "1");
			params.Insert("GBufferMetalness", "1");
			params.Insert("GBufferAO", "1");
		}
		ref Material made = Material.Create(name, materialClass, params);
		Mark(string.Format("Material.Create level %1 gave %2", level, made));
		if (!made)
			return string.Empty;
		m_aMade.Insert(made);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected void MakeBlobMaterial()
	{
		string made = MakeBlob(m_iMadeLevel);
		if (!made.IsEmpty())
			m_sBlob = made;
	}

	//------------------------------------------------------------------------------------------------
	//! Blobs projected onto whatever the helicopter is made of. Nothing about its UVs is used.
	protected int ProbeDecals(BaseWorld world, vector origin, ResourceName prefab, string name)
	{
		IEntity heli = Spawn(world, origin, prefab, 0);
		if (!heli)
			return 1;
		Sleep(5000);
		Shot(name + "_before");

		m_aParts.Clear();
		CollectParts(heli);
		Mark(string.Format("%1 parts with a mesh, glass left out", m_aParts.Count()));

		int started = System.GetTickCount();
		int projectors = Spray(heli);
		Mark(string.Format("%1 projectors hit, %2 decals made, %3 ms", projectors, m_aDecals.Count(), System.GetTickCount() - started));
		Sleep(3000);
		Shot(name + "_decals");

		Move(heli, origin + m_vSide * 3 + vector.Up * 1.5, 40);
		Sleep(4000);
		Shot(name + "_decals_moved");

		Move(heli, origin, 180);
		Sleep(4000);
		Shot(name + "_decals_far_side");

		BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(heli.FindComponent(BaseVehicleControllerComponent));
		if (controller)
			controller.ForceStartEngine();
		Sleep(9000);
		Shot(name + "_decals_engine");

		World decalWorld = heli.GetWorld();
		foreach (Decal decal : m_aDecals)
		{
			decalWorld.RemoveDecal(decal);
		}
		Mark(string.Format("%1 decals removed", m_aDecals.Count()));
		int made = m_aDecals.Count();
		m_aDecals.Clear();
		Sleep(2000);
		Shot(name + "_decals_removed");

		if (made == 0)
		{
			Print("[IA][HeliCamoProbe] FAIL: no decal was created on the helicopter", LogLevel.ERROR);
			return 1;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Traces at the helicopter from all round it and projects one blob where each trace lands,
	//! along the surface normal, onto every part: one projector keeps a blob whole across a door seam.
	protected int Spray(IEntity heli)
	{
		vector mins;
		vector maxs;
		heli.GetWorldBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		vector half = (maxs - mins) * 0.5;
		float radius = half.Length() + 2;
		World decalWorld = heli.GetWorld();

		ref RandomGenerator random = new RandomGenerator();
		random.SetSeed(1234);
		ref TraceParam param = new TraceParam();
		param.Flags = TraceFlags.ENTS;

		int projectors;
		int tan = 0xFFC9B27C;
		int dark = 0xFF23281A;
		int color;
		vector direction;
		vector target;
		vector hit;
		vector normal;
		float fraction;
		float size;
		float angle;
		float y;
		float ring;
		float turn;
		Decal decal;
		IEntity root;
		int variant;
		int before;
		vector traceDirection;
		int i;
		for (i = 0; i < PROJECTORS; i++)
		{
			// Evenly spread directions (a Fibonacci sphere).
			y = 1 - 2 * (i + 0.5) / PROJECTORS;
			ring = Math.Sqrt(1 - y * y);
			turn = i * 2.39996;
			direction = Vector(Math.Cos(turn) * ring, y, Math.Sin(turn) * ring);
			target = centre + Vector(random.RandFloatXY(-0.6, 0.6) * half[0], random.RandFloatXY(-0.6, 0.6) * half[1], random.RandFloatXY(-0.6, 0.6) * half[2]);

			param.Start = target + direction * radius;
			param.End = target;
			param.TraceEnt = null;
			fraction = decalWorld.TraceMove(param, null);
			if (!param.TraceEnt)
				continue;
			root = param.TraceEnt.GetRootParent();
			if (root != heli)
				continue;

			hit = param.Start + (param.End - param.Start) * fraction;
			normal = param.TraceNorm;
			size = random.RandFloatXY(1.4, 2.6);
			angle = random.RandFloatXY(0, Math.PI2);
			if (i % 2 == 0)
				color = tan;
			else
				color = dark;

			// Diagnostic: which projector set-up paints. Tan and white go along the normal, red and
			// blue along the trace; tan and blue are turned about the projection axis.
			if (m_bDiagnose)
			{
				variant = projectors % 4;
				traceDirection = param.End - param.Start;
				traceDirection.Normalize();
				if (variant == 1)
				{
					color = 0xFFFFFFFF;
					angle = 0;
				}
				else if (variant == 2)
				{
					color = 0xFFFF2020;
					angle = 0;
					normal = -traceDirection;
				}
				else if (variant == 3)
				{
					color = 0xFF2040FF;
					normal = -traceDirection;
				}
				else
				{
					color = tan;
				}
			}

			projectors = projectors + 1;
			before = m_aDecals.Count();
			foreach (IEntity part : m_aParts)
			{
				decal = decalWorld.CreateDecal(part, hit + normal * (PROJECT_DEPTH_M * 0.5), -normal, 0, PROJECT_DEPTH_M, angle, size, 1, m_sBlob, -1, color);
				if (decal)
					m_aDecals.Insert(decal);
			}
			if (m_bDiagnose)
				Mark(string.Format("projector %1 variant %2 on %3 '%4' at %5 normal %6 size %7 decals %8", i, variant, param.TraceEnt.GetPrefabData().GetPrefabName(), param.ColliderName, hit - centre, param.TraceNorm, size, m_aDecals.Count() - before));
		}
		return projectors;
	}

	//------------------------------------------------------------------------------------------------
	//! One decal per station along the side facing the camera, each made a different way, and one
	//! on the ground as a control.
	protected int ProbeDecalVariants(BaseWorld world, vector origin, ResourceName prefab)
	{
		IEntity heli = Spawn(world, origin, prefab, 0);
		if (!heli)
			return 1;
		Sleep(5000);

		vector mins;
		vector maxs;
		heli.GetWorldBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		World decalWorld = heli.GetWorld();
		ref TraceParam param = new TraceParam();
		param.Flags = TraceFlags.ENTS | TraceFlags.WORLD;

		ResourceName indicator = "{F2E83D562703F861}Assets/Decals/Impact/DecalHitIndicator.emat";
		ResourceName blood = "{F11E9EF96AC392A7}Assets/Decals/Blood/Data/BloodStain_Large_01.emat";
		vector target;
		vector hit;
		float fraction;
		Decal decal;
		string material;
		int color;
		int made;
		int k;
		for (k = 0; k < 6; k++)
		{
			target = centre + m_vSide * ((k - 2.5) * 1.3);
			param.Start = target - m_vAway * 10;
			param.End = target + m_vAway * 3;
			param.TraceEnt = null;
			fraction = decalWorld.TraceMove(param, null);
			if (!param.TraceEnt)
			{
				Mark(string.Format("station %1: nothing hit", k));
				continue;
			}
			hit = param.Start + (param.End - param.Start) * fraction;
			decal = null;
			material = indicator;
			color = 0xFFC9B27C;
			if (k == 1)
				material = MakeBlob(0);
			else if (k == 2)
				material = MakeBlob(1);
			else if (k == 3)
				material = MakeBlob(2);
			else if (k == 4)
				material = MakeBlob(3);
			else if (k == 5)
				material = blood;
			if (k == 0 || k == 5)
				color = 0xFFFFFFFF;
			decal = decalWorld.CreateDecal(param.TraceEnt, hit - m_vAway * 0.2, m_vAway, 0, 0.5, 0, 1, 1, material, -1, color);

			if (decal)
			{
				made = made + 1;
				m_aDecals.Insert(decal);
			}
			Mark(string.Format("station %1: hit %2 at %3 normal %4 decal %5", k, param.TraceEnt, hit, param.TraceNorm, decal != null));
		}

		// Control: the same blob on the ground between the camera and the helicopter.
		target = origin - m_vAway * 5;
		param.Start = target + vector.Up * 5;
		param.End = target - vector.Up * 5;
		param.TraceEnt = null;
		fraction = decalWorld.TraceMove(param, null);
		if (param.TraceEnt)
		{
			hit = param.Start + (param.End - param.Start) * fraction;
			decal = decalWorld.CreateDecal(param.TraceEnt, hit + vector.Up * 0.3, -vector.Up, 0, 0.6, 0, 2, 1, BLOB, -1, 0xFFC9B27C);
			Mark(string.Format("ground control: hit %1 decal %2", param.TraceEnt, decal != null));
		}
		else
		{
			Mark("ground control: nothing hit");
		}

		Sleep(3000);
		Shot("variants");
		Move(heli, origin + vector.Up * 1.5, 0);
		Sleep(4000);
		Shot("variants_raised");
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! One blob per station along the side facing the camera; each station changes one thing about
	//! the projector that painted in ProbeDecalVariants.
	protected int ProbeProjectorVariants(BaseWorld world, vector origin, ResourceName prefab)
	{
		IEntity heli = Spawn(world, origin, prefab, 0);
		if (!heli)
			return 1;
		Sleep(5000);
		m_aParts.Clear();
		CollectParts(heli);

		vector mins;
		vector maxs;
		heli.GetWorldBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		World decalWorld = heli.GetWorld();
		ref TraceParam param = new TraceParam();
		param.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		string material = MakeBlob(1);

		vector target;
		vector hit;
		vector normal;
		float fraction;
		int before;
		Decal decal;
		int k;
		for (k = 0; k < 6; k++)
		{
			target = centre + m_vSide * ((k - 3) * 1.4) - vector.Up * 0.3;
			param.Start = target - m_vAway * 10;
			param.End = target + m_vAway * 3;
			param.TraceEnt = null;
			fraction = decalWorld.TraceMove(param, null);
			if (!param.TraceEnt)
			{
				Mark(string.Format("station %1: nothing hit", k));
				continue;
			}
			hit = param.Start + (param.End - param.Start) * fraction;
			normal = param.TraceNorm;
			before = m_aDecals.Count();
			decal = null;
			if (k == 0)
				decal = decalWorld.CreateDecal(param.TraceEnt, hit - m_vAway * 0.2, m_vAway, 0, 0.5, 0, 1, 1, material, -1, 0xFFC9B27C);
			else if (k == 1)
				decal = decalWorld.CreateDecal(param.TraceEnt, hit - m_vAway * 0.2, m_vAway, 0, 0.5, 0, 2, 1, material, -1, 0xFFFFFFFF);
			else if (k == 2)
				decal = decalWorld.CreateDecal(param.TraceEnt, hit - m_vAway * 0.35, m_vAway, 0, 0.7, 0, 1, 1, material, -1, 0xFFFF2020);
			else if (k == 3)
				decal = decalWorld.CreateDecal(param.TraceEnt, hit + normal * 0.2, -normal, 0, 0.5, 0, 1, 1, material, -1, 0xFF2040FF);
			else if (k == 5)
				decal = decalWorld.CreateDecal(param.TraceEnt, hit - m_vAway * 0.2, m_vAway, 0, 0.5, 1, 1, 1, material, -1, 0xFF20FF20);
			if (decal)
				m_aDecals.Insert(decal);

			if (k == 4)
			{
				foreach (IEntity part : m_aParts)
				{
					decal = decalWorld.CreateDecal(part, hit - m_vAway * 0.2, m_vAway, 0, 0.5, 0, 1, 1, material, -1, 0xFFFF20FF);
					if (decal)
						m_aDecals.Insert(decal);
				}
			}
			Mark(string.Format("station %1: '%2' at %3 normal %4 decals %5", k, param.ColliderName, hit - centre, normal, m_aDecals.Count() - before));
		}

		Sleep(3000);
		Shot("projectors");
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Six blobs along the side facing the camera, onto every part, with one projector setting
	//! stepped from station to station. -iaCamoSweep picks it: size, normal, angle or depth.
	protected int ProbeSweep(BaseWorld world, vector origin, ResourceName prefab)
	{
		string sweep = "size";
		string arg;
		if (System.GetCLIParam("iaCamoSweep", arg) && !arg.IsEmpty())
			sweep = arg;
		float height = -0.3;
		// A negative value would be read as the next argument, so the drop is given as a positive.
		if (System.GetCLIParam("iaCamoDrop", arg) && !arg.IsEmpty())
			height = -arg.ToFloat();

		IEntity heli = Spawn(world, origin, prefab, 0);
		if (!heli)
			return 1;
		Sleep(5000);
		m_aParts.Clear();
		CollectParts(heli);

		vector mins;
		vector maxs;
		heli.GetWorldBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		World decalWorld = heli.GetWorld();
		ref TraceParam param = new TraceParam();
		param.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		string material = MakeBlob(m_iMadeLevel);

		float sizes[6] = {0.6, 1.0, 1.3, 1.6, 2.0, 2.6};
		float angles[6] = {0, 0.5, 1.0, 1.57, 3.14, 45};
		float depths[6] = {0.2, 0.5, 0.7, 1.0, 2.0, 4.0};
		int colors[6] = {0xFFC9B27C, 0xFFFFFFFF, 0xFFFF2020, 0xFF2040FF, 0xFFFF20FF, 0xFF20FF20};

		vector target;
		vector hit;
		vector normal;
		vector direction;
		vector from;
		float fraction;
		float size;
		float angle;
		float depth;
		int before;
		Decal decal;
		int k;
		for (k = 0; k < 6; k++)
		{
			target = centre + m_vSide * ((k - 2.5) * 1.5) + vector.Up * height;
			param.Start = target - m_vAway * 10;
			param.End = target + m_vAway * 3;
			param.TraceEnt = null;
			fraction = decalWorld.TraceMove(param, null);
			if (!param.TraceEnt)
			{
				Mark(string.Format("sweep %1 station %2: nothing hit", sweep, k));
				continue;
			}
			hit = param.Start + (param.End - param.Start) * fraction;
			normal = param.TraceNorm;
			size = 1;
			angle = 0;
			depth = 0.5;
			direction = m_vAway;
			if (sweep == "size")
				size = sizes[k];
			else if (sweep == "angle")
				angle = angles[k];
			else if (sweep == "depth")
				depth = depths[k];
			else if (sweep == "normal" && k % 2 == 1)
				direction = -normal;
			else if (sweep == "small")
				size = 0.35;
			else if (sweep == "thin")
				depth = 0.12;

			from = hit - direction * (depth * 0.4);
			if (sweep == "thin")
				from = hit - direction * 0.02;
			before = m_aDecals.Count();
			foreach (IEntity part : m_aParts)
			{
				decal = decalWorld.CreateDecal(part, from, direction, 0, depth, angle, size, 1, material, -1, colors[k]);
				if (decal)
				{
					m_aDecals.Insert(decal);
					Mark(string.Format("  station %1 decal on %2", k, DescribePart(part)));
				}
			}
			Mark(string.Format("sweep %1 station %2: '%3' normal %4 size %5 angle %6 depth %7 decals %8", sweep, k, param.ColliderName, normal, size, angle, depth, m_aDecals.Count() - before));
		}

		Sleep(3000);
		Shot("sweep_" + sweep);
		foreach (Decal made : m_aDecals)
		{
			decalWorld.RemoveDecal(made);
		}
		m_aDecals.Clear();
		Sleep(1000);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected string DescribePart(IEntity part)
	{
		VObject mesh = part.GetVObject();
		string materials[256];
		int count = mesh.GetMaterials(materials);
		string text = FilePath.StripPath(mesh.GetResourceName());
		text = text + string.Format(" (%1 materials:", count);
		int i;
		for (i = 0; i < count && i < 8; i++)
		{
			text = text + " " + FilePath.StripPath(materials[i]);
		}
		return text + ")";
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectParts(IEntity entity)
	{
		VObject mesh = entity.GetVObject();
		if (mesh && !HasGlass(mesh))
			m_aParts.Insert(entity);

		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectParts(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasGlass(VObject mesh)
	{
		string materials[256];
		int count = mesh.GetMaterials(materials);
		string lower;
		int i;
		for (i = 0; i < count; i++)
		{
			lower = materials[i];
			lower.ToLower();
			if (!lower.Contains("glass"))
				return false;
		}
		return count > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A channel Huey whose body material was edited on disk for this run. The paint rig sets the
	//! stock colours when the helicopter spawns, so the first layer is lightened again here.
	protected int ProbeChannelMaterial(BaseWorld world, vector origin, int channel)
	{
		IEntity heli = Spawn(world, origin, IA_HeliPaintChannels.FindChannelPrefab(HUEY, channel), 0);
		if (!heli)
			return 1;
		Sleep(5000);

		Material body = Material.GetOrLoadMaterial(IA_HeliPaintChannels.GetMaterial(IA_HeliPaintChannels.SURFACE_BODY, channel), 0);
		if (!body)
		{
			Print("[IA][HeliCamoProbe] FAIL: the channel body material did not load", LogLevel.ERROR);
			return 1;
		}
		float light[4] = {0.7, 0.7, 0.7, 1};
		Mark(string.Format("Color_1 set: %1", body.SetParam("Color_1", light)));
		Sleep(2000);
		Shot(string.Format("channel%1_first", channel));

		Move(heli, origin + m_vSide * 3 + vector.Up * 1.5, 0);
		Sleep(4000);
		Shot(string.Format("channel%1_shifted", channel));

		Move(heli, origin, 40);
		Sleep(4000);
		Shot(string.Format("channel%1_turned", channel));

		Move(heli, origin + vector.Up, 90);
		Sleep(4000);
		Shot(string.Format("channel%1_nose", channel));

		// Rolled onto its side so the roof faces the camera: shows which way the pattern is projected.
		vector rolled[4];
		rolled[1] = -m_vAway;
		rolled[2] = m_vSide;
		rolled[0] = rolled[1] * rolled[2];
		rolled[3] = origin + vector.Up * 1.5;
		BaseGameEntity gameEntity = BaseGameEntity.Cast(heli);
		if (gameEntity)
			gameEntity.Teleport(rolled);
		Sleep(2500);
		Shot(string.Format("channel%1_roof", channel));
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What a live material takes from script: a texture, a UV source, and whether one can be made.
	protected int ProbeMaterialApi(BaseWorld world, vector origin)
	{
		IEntity heli = Spawn(world, origin, IA_HeliPaintChannels.FindChannelPrefab(HUEY, 1), 0);
		if (!heli)
			return 1;
		Sleep(5000);

		Material body = Material.GetOrLoadMaterial(IA_HeliPaintChannels.GetMaterial(IA_HeliPaintChannels.SURFACE_BODY, 1), 0);
		if (!body)
			return 1;

		Shot("api_before");
		float light[4] = {0.7, 0.7, 0.7, 1};
		Mark(string.Format("Color_1 set: %1", body.SetParam("Color_1", light)));
		Mark(string.Format("BCR_1 index: %1, UVSrcDetail index: %2, UVTransform_1 index: %3", body.GetParamIndex("BCR_1"), body.GetParamIndex("UVSrcDetail"), body.GetParamIndex("UVTransform_1")));
		string pattern = PATTERN;
		Mark(string.Format("BCR_1 set to a texture name: %1", body.SetParam("BCR_1", pattern)));
		Sleep(3000);
		Shot("api_texture");

		int uvWorld = 2;
		Mark(string.Format("UVSrcDetail set to 2: %1", body.SetParam("UVSrcDetail", uvWorld)));
		Sleep(3000);
		Shot("api_uvworld");

		ref map<string, string> params = new map<string, string>();
		params.Insert("BCRMap", PATTERN);
		ref Material made = Material.Create("IA_HeliCamoProbe_Made", "MatPBRBasic", params);
		Mark(string.Format("Material.Create: %1", made));
		Mark(string.Format("Material.GetMaterial finds it: %1", Material.GetMaterial("IA_HeliCamoProbe_Made")));

		ParametricMaterialInstanceComponent instance = ParametricMaterialInstanceComponent.Cast(heli.FindComponent(ParametricMaterialInstanceComponent));
		Mark(string.Format("Hull has a ParametricMaterialInstanceComponent: %1", instance));
		if (instance)
		{
			instance.SetColor(0xFFC9B27C);
			Sleep(2000);
			Shot("api_instance_color");
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(BaseWorld world, vector origin, ResourceName prefab, float yawDeg)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Pose(origin, yawDeg, params.Transform);
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params);
		Mark(string.Format("spawned %1 from %2", entity, prefab));
		if (!entity)
			Print("[IA][HeliCamoProbe] FAIL: the helicopter did not spawn: " + prefab, LogLevel.ERROR);
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Side-on to the camera, turned by yawDeg about the vertical.
	protected void Pose(vector origin, float yawDeg, out vector transform[4])
	{
		float yaw = yawDeg * Math.DEG2RAD;
		vector forward = m_vSide * Math.Cos(yaw) + m_vAway * Math.Sin(yaw);
		vector right = m_vAway * -Math.Cos(yaw) + m_vSide * Math.Sin(yaw);
		transform[0] = right;
		transform[1] = vector.Up;
		transform[2] = forward;
		transform[3] = origin;
	}

	//------------------------------------------------------------------------------------------------
	protected void Move(IEntity heli, vector origin, float yawDeg)
	{
		vector transform[4];
		Pose(origin, yawDeg, transform);
		BaseGameEntity gameEntity = BaseGameEntity.Cast(heli);
		if (gameEntity)
		{
			gameEntity.Teleport(transform);
		}
		else
		{
			heli.SetWorldTransform(transform);
			heli.Update();
		}
		Mark(string.Format("moved to %1 yaw %2", origin, yawDeg));
	}

	//------------------------------------------------------------------------------------------------
	protected void Shot(string name)
	{
		System.MakeScreenshot("$profile:IA_HeliCamoProbe_" + name + ".bmp");
		Sleep(1500);
	}

	//------------------------------------------------------------------------------------------------
	//! The console log is not flushed when the engine dies; a file closed after each line is.
	protected void Mark(string message)
	{
		Print("[IA][HeliCamoProbe] " + message, LogLevel.NORMAL);
		FileHandle file = FileIO.OpenFile("$profile:IA_HeliCamoProbe.log", FileMode.APPEND);
		if (!file)
			return;
		file.WriteLine(string.Format("%1 %2", System.GetTickCount(), message));
		file.Close();
	}
}
#endif
