#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Play-mode probe for changing a live helicopter's skin. Opens a small world,
//! starts game mode, spawns Hueys in front of the camera, changes the skin the
//! way -iaSkinMode picks and keeps the game running to see whether the engine
//! survives. A crash leaves no PASS line. Screenshots go to the profile folder
//! as IA_HeliSkinLiveProbe_<name>.bmp.png.
//!   -iaSkinMode 0  control, no change
//!   -iaSkinMode 1  the shipped route: IA_HeliSkinPaint on paint channels. A stock Huey, one
//!                  on channel 1 and one on channel 2 stand side by side; channel 1 goes tan,
//!                  is flicked between tan and stock twenty times, then goes back to stock.
//!   -iaSkinMode 2  as 1, but channel 3 is painted before its Huey exists
//!   -iaSkinMode 3  the route that must not be used: SetObject $remap on the hull. It crashes
//!                  the engine a frame later; kept to prove this probe sees that.
//!   -iaSkinMode 4  how a stock Huey gets a channel: the editor's variant pick returns the twin on
//!                  a free channel, and a deleted helicopter gives its channel back.
//!   -iaSkinMode 5  layer map: marks each colour layer of a stock material, see ProbeLayers.
//!   -iaSkinMode 6  livery sheet: one family's airframe from three sides, a screenshot of stock
//!                  paint and of each livery, see ProbeLiveries. Add 100 to run an engine.
//! Add 100 to run the engine first, 200 to seat a pilot first (300 for both).
//!   -iaSkinDistance <metres>  how far in front of the camera the helicopters stand
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA helicopter skin live probe", wbModules: {"ResourceManager"})]
class IA_HeliSkinLiveProbe : WorkbenchPlugin
{
	// A vanilla showcase world with no game mode: nothing in it asks Workbench for script authorization.
	protected static const string WORLD = "worlds/Showcase/PBR_Vehicles.ent";
	protected static const ResourceName STOCK = "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et";
	protected static const ResourceName PILOT = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
	protected static const int GAME_MODE_TIMEOUT_MS = 240000;
	protected static const int CYCLES = 20;
	protected static const int CYCLE_GAP_MS = 150;
	// Rotor discs must not overlap once an engine runs.
	protected static const float SPACING_M = 15;
	protected static const float DISTANCE_M = 26;

	protected IEntity m_Pilot;
	// Helicopters stand side-on to the camera.
	protected vector m_vSide;
	protected vector m_vAway;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		string modeArg;
		int mode;
		if (System.GetCLIParam("iaSkinMode", modeArg) && !modeArg.IsEmpty())
			mode = modeArg.ToInt();

		int failures = Probe(mode);
		if (failures == 0)
			Print(string.Format("[IA][HeliSkinLiveProbe] PASS mode=%1", mode), LogLevel.NORMAL);
		else
			Print(string.Format("[IA][HeliSkinLiveProbe] FAIL mode=%1 failures=%2", mode, failures), LogLevel.ERROR);
		Workbench.Exit(failures);
	}

	//------------------------------------------------------------------------------------------------
	protected int Probe(int mode)
	{
		Mark(string.Format("start mode=%1", mode));
		Workbench.OpenModule(WorldEditor);
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || !editor.SetOpenedResource(WORLD))
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the test world did not open", LogLevel.ERROR);
			return 1;
		}

		Sleep(500);
		// SwitchToGameMode returns at once; the game starts some frames later.
		editor.SwitchToGameMode(false, false);
		int waited;
		while (editor.GetApi() && !editor.GetApi().IsGameMode())
		{
			Sleep(100);
			waited = waited + 100;
			if (waited > GAME_MODE_TIMEOUT_MS)
			{
				Print("[IA][HeliSkinLiveProbe] FAIL: game mode did not start", LogLevel.ERROR);
				return 1;
			}
		}
		Mark("game mode");
		Sleep(8000);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: no game world", LogLevel.ERROR);
			return 1;
		}

		vector camera[4];
		world.GetCurrentCamera(camera);
		vector forward = camera[2];
		forward[1] = 0;
		forward.Normalize();
		vector right = camera[0];
		right[1] = 0;
		right.Normalize();
		float distance = DISTANCE_M;
		string distanceArg;
		if (System.GetCLIParam("iaSkinDistance", distanceArg) && !distanceArg.IsEmpty())
			distance = distanceArg.ToFloat();
		vector origin = camera[3] + forward * distance;
		m_vSide = right;
		m_vAway = forward;
		origin[1] = Math.Max(world.GetSurfaceY(origin[0], origin[2]), 0) + 0.3;

		int technique = mode % 100;
		int setup = mode / 100;
		bool engine = setup == 1 || setup == 3;
		bool crewed = setup >= 2;
		int failures;

		if (technique == 4)
		{
			failures = ProbeSpawnRoute(world, origin);
			Mark(string.Format("survived failures=%1", failures));
			editor.SwitchToEditMode();
			Sleep(1000);
			return failures;
		}

		if (technique == 5)
		{
			failures = ProbeLayers(world, origin);
			Mark(string.Format("survived failures=%1", failures));
			editor.SwitchToEditMode();
			Sleep(1000);
			return failures;
		}

		if (technique == 6)
		{
			failures = ProbeLiveries(world, origin, engine);
			Mark(string.Format("survived failures=%1", failures));
			editor.SwitchToEditMode();
			Sleep(1000);
			return failures;
		}

		// Painted before its helicopter exists: a channel keeps its skin until the next airframe takes it.
		int tanSkin = IA_HeliSkinCatalog.SKIN_DESERT_TAN;
		if (technique == 2)
			Mark(string.Format("channel 3 painted early, %1 surfaces", IA_HeliSkinPaint.Apply(3, tanSkin)));

		// The subject of the test stands in the middle; with channels it is the Huey on channel 1.
		IEntity stock;
		IEntity heli;
		if (technique == 1 || technique == 2)
		{
			stock = Spawn(world, origin - right * SPACING_M, STOCK);
			heli = Spawn(world, origin, IA_HeliPaintChannels.FindChannelPrefab(STOCK, 1));
			if (technique == 1)
				Spawn(world, origin + right * SPACING_M, IA_HeliPaintChannels.FindChannelPrefab(STOCK, 2));
			else
				Spawn(world, origin + right * SPACING_M, IA_HeliPaintChannels.FindChannelPrefab(STOCK, 3));
		}
		else
		{
			heli = Spawn(world, origin, STOCK);
		}

		if (!heli)
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the helicopter did not spawn", LogLevel.ERROR);
			return 1;
		}
		Sleep(4000);

		if (crewed && !SeatPilot(heli, world, origin - forward * 6))
			failures = 1;
		if (engine)
		{
			BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(heli.FindComponent(BaseVehicleControllerComponent));
			if (controller)
				controller.ForceStartEngine();
			Sleep(6000);
			if (controller)
				Mark(string.Format("Engine on: %1", controller.IsEngineOn()));
		}

		Shot("before");

		if (technique == 1 || technique == 2)
			failures = failures + ProbeChannels(heli);
		else if (technique == 3)
			Mark(string.Format("remapped hull: %1", Remap(heli)));

		// The crash of the unsafe route came on the frame after the change; run a few hundred.
		Sleep(6000);
		if (technique == 3)
			Shot("remapped");

		if (heli.IsDeleted())
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the helicopter was deleted", LogLevel.ERROR);
			failures = failures + 1;
		}
		if (crewed && !IsSeated())
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the pilot left the seat", LogLevel.ERROR);
			failures = failures + 1;
		}

		Mark(string.Format("survived failures=%1", failures));
		editor.SwitchToEditMode();
		Sleep(1000);
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped route. Only channel 1 may change colour; the screenshots show whether it did.
	protected int ProbeChannels(IEntity heli)
	{
		int failures;
		int tanSkin = IA_HeliSkinCatalog.SKIN_DESERT_TAN;
		int none = IA_HeliSkinCatalog.SKIN_NONE;
		ResourceName prefab = heli.GetPrefabData().GetPrefabName();
		IA_HeliPaintFamily family = IA_HeliPaintChannels.GetFamily(1);
		if (IA_HeliSkinManagerComponent.GetVehicleChannel(heli) != 1)
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the spawned helicopter is not on paint channel 1: " + prefab, LogLevel.ERROR);
			failures = failures + 1;
		}

		VObject mesh = heli.GetVObject();
		int painted = IA_HeliSkinPaint.Apply(1, tanSkin);
		Mark(string.Format("channel 1 tan, %1 surfaces", painted));
		if (!family || painted != family.m_aSurfaces.Count())
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: not every surface was painted", LogLevel.ERROR);
			failures = failures + 1;
		}
		Sleep(3000);
		Shot("tan");

		// A menu lets a pilot flick through skins; change faster than anyone would.
		int cycle;
		for (cycle = 0; cycle < CYCLES; cycle++)
		{
			if (cycle % 2 == 0)
				IA_HeliSkinPaint.Apply(1, none);
			else
				IA_HeliSkinPaint.Apply(1, tanSkin);
			Sleep(CYCLE_GAP_MS);
		}
		Mark(string.Format("cycled %1 times, %2 ms apart", CYCLES, CYCLE_GAP_MS));
		Sleep(2000);

		IA_HeliSkinPaint.Apply(1, none);
		IA_HeliSkinPaint.Apply(3, none);
		Sleep(3000);
		Shot("restored");

		// The route is only safe because the hull keeps the mesh instance its animation is bound to.
		if (heli.GetVObject() != mesh)
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the hull mesh was replaced", LogLevel.ERROR);
			failures = failures + 1;
		}
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! How a stock Huey gets a channel: the editor's variant pick hands out the twin on a channel no
	//! live helicopter holds, and a deleted helicopter gives its channel back.
	protected int ProbeSpawnRoute(BaseWorld world, vector origin)
	{
		int failures;
		IA_HeliPaintFamily huey = IA_HeliPaintChannels.FindStockFamily(STOCK);
		failures = failures + Expect(huey != null, "the stock Huey belongs to a paint family");
		ResourceName picked = SCR_EditableEntityComponentClass.GetRandomVariant(STOCK);
		failures = failures + Expect(IA_HeliPaintChannels.FindChannel(picked) == 1, "the editor's pick for a stock Huey is its channel 1 twin: " + picked);

		IEntity first = Spawn(world, origin - m_vSide * SPACING_M, picked);
		Sleep(2000);
		failures = failures + Expect(first && IA_HeliPaintRigComponent.GetHolder(1) == first, "the spawned twin holds channel 1");
		failures = failures + Expect(IA_HeliSkinManagerComponent.GetVehicleChannel(first) == 1, "the spawned twin can be repainted");

		picked = SCR_EditableEntityComponentClass.GetRandomVariant(STOCK);
		failures = failures + Expect(IA_HeliPaintChannels.FindChannel(picked) == 2, "the next pick is the channel 2 twin: " + picked);
		IEntity second = Spawn(world, origin + m_vSide * SPACING_M, picked);
		Sleep(2000);
		failures = failures + Expect(second && IA_HeliPaintRigComponent.GetHolder(2) == second, "the second twin holds channel 2");
		failures = failures + Expect(IA_HeliPaintRigComponent.FindFreeChannel(huey) == 3, "channel 3 is the next free one");

		// Every family has channels of its own; a Huey on the pad takes none from a Hip.
		ResourceName hip = "Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et";
		IA_HeliPaintFamily hipFamily = IA_HeliPaintChannels.FindStockFamily(hip);
		failures = failures + Expect(hipFamily && hipFamily != huey, "the stock Hip belongs to a family of its own");
		picked = IA_HeliPaintRigComponent.ResolveSpawnPrefab(hip);
		failures = failures + Expect(IA_HeliPaintChannels.FindChannel(picked) == IA_HeliPaintChannels.ToChannel(hipFamily, 1), "a stock Hip gets the first channel of its family: " + picked);
		failures = failures + Expect(IA_HeliPaintChannels.GetFamily(IA_HeliPaintChannels.FindChannel(picked)) == hipFamily, "that channel belongs to the Hip family");

		ResourceName other = "Prefabs/Vehicles/Wheeled/M151A2/M151A2.et";
		failures = failures + Expect(IA_HeliPaintRigComponent.ResolveSpawnPrefab(other) == other, "a vehicle with no paint family is left as it is");

		if (first)
			SCR_EntityHelper.DeleteEntityAndChildren(first);
		Sleep(2000);
		failures = failures + Expect(!IA_HeliPaintRigComponent.GetHolder(1), "a deleted helicopter gives its channel back");
		failures = failures + Expect(IA_HeliPaintRigComponent.FindFreeChannel(huey) == 1, "channel 1 is free again");
		failures = failures + Expect(second && !second.IsDeleted() && IA_HeliPaintRigComponent.GetHolder(2) == second, "the other helicopter keeps its channel");
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows which part of an airframe each colour layer of a stock material paints: layer 1 goes red,
	//! 2 green, 3 blue and 4 yellow. Only this process's copy of the material changes; nothing is saved.
	//!   -iaSkinPrefab <prefab>           the airframe
	//!   -iaSkinMaterial <emat[,emat]>    the stock materials to mark
	//!   -iaSkinSpacing <metres>          gap between the three views (side, nose, tail)
	//!   -iaSkinParam <name> -iaSkinColors <r,g,b[/r,g,b]>   instead of marking layers, set this one
	//!                                    colour to each value in turn (screenshots tint_0, tint_1, ...)
	protected int ProbeLayers(BaseWorld world, vector origin)
	{
		string prefab;
		string materialArg;
		string spacingArg;
		System.GetCLIParam("iaSkinPrefab", prefab);
		System.GetCLIParam("iaSkinMaterial", materialArg);
		float spacing = SPACING_M;
		if (System.GetCLIParam("iaSkinSpacing", spacingArg) && !spacingArg.IsEmpty())
			spacing = spacingArg.ToFloat();
		if (prefab.IsEmpty() || materialArg.IsEmpty())
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: -iaSkinPrefab and -iaSkinMaterial are needed", LogLevel.ERROR);
			return 1;
		}

		int failures;
		IEntity side = SpawnTurned(world, origin, prefab, 0);
		IEntity nose = SpawnTurned(world, origin - m_vSide * spacing, prefab, 60);
		IEntity tail = SpawnTurned(world, origin + m_vSide * spacing, prefab, -120);
		failures = failures + Expect(side && nose && tail, "the airframe spawned three times");
		Sleep(5000);
		Shot("layers_stock");

		float red[4] = {1, 0, 0, 1};
		float green[4] = {0, 1, 0, 1};
		float blue[4] = {0, 0, 1, 1};
		float yellow[4] = {1, 1, 0, 1};
		ref array<string> names = {};
		materialArg.Split(",", names, true);
		ref array<ref Material> held = {};
		Material material;
		foreach (string name : names)
		{
			material = Material.GetOrLoadMaterial(name, 0);
			if (Expect(material != null, "material loads: " + name) != 0)
			{
				failures = failures + 1;
				continue;
			}
			held.Insert(material);
			MarkColourParams(name);
		}

		// A hull with no colour layers has one tint instead: try each given colour on it.
		string paramArg;
		string coloursArg;
		if (System.GetCLIParam("iaSkinParam", paramArg) && !paramArg.IsEmpty() && System.GetCLIParam("iaSkinColors", coloursArg) && !coloursArg.IsEmpty())
		{
			ref array<string> colours = {};
			coloursArg.Split("/", colours, true);
			ref array<string> channels = {};
			float tint[4];
			bool taken;
			int index;
			foreach (string colour : colours)
			{
				channels.Clear();
				colour.Split(",", channels, true);
				if (channels.Count() < 3)
					continue;

				tint[0] = channels[0].ToFloat();
				tint[1] = channels[1].ToFloat();
				tint[2] = channels[2].ToFloat();
				tint[3] = 1;
				taken = true;
				foreach (Material tinted : held)
				{
					if (!tinted.SetParam(paramArg, tint))
						taken = false;
				}
				failures = failures + Expect(taken, string.Format("every material took %1 = %2", paramArg, colour));
				Sleep(2500);
				Shot(string.Format("tint_%1", index));
				index = index + 1;
			}
			return failures;
		}

		foreach (Material layered : held)
		{
			layered.SetParam("Color_1", red);
			layered.SetParam("Color_2", green);
			layered.SetParam("Color_3", blue);
			layered.SetParam("Color_4", yellow);
		}
		Sleep(3000);
		Shot("layers");
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the class of a material and every colour its class has, set by the file or not.
	protected void MarkColourParams(ResourceName name)
	{
		Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
			return;

		BaseContainer source = resource.GetResource().ToBaseContainer();
		if (!source)
			return;

		string colours;
		int count = source.GetNumVars();
		int i;
		for (i = 0; i < count; i++)
		{
			if (source.GetDataVarType(i) == DataVarType.COLOR)
				colours = colours + " " + source.GetVarName(i);
		}
		Mark(string.Format("%1 is %2 with colours:%3", name, source.GetClassName(), colours));
	}

	//------------------------------------------------------------------------------------------------
	//! Every livery on one family's airframe, seen from the side, the nose and the tail. One screenshot
	//! of stock paint (livery_<family>_stock) and one per livery (livery_<family>_<key>).
	//!   -iaSkinFamily <key>      the family, uh1h when left out
	//!   -iaSkinAirframe <index>  which of its stock airframes, the first when left out
	//!   -iaSkinSpacing <metres>  gap between the three views
	protected int ProbeLiveries(BaseWorld world, vector origin, bool engine)
	{
		string key = "uh1h";
		string keyArg;
		string indexArg;
		string spacingArg;
		if (System.GetCLIParam("iaSkinFamily", keyArg) && !keyArg.IsEmpty())
			key = keyArg;
		int airframe;
		if (System.GetCLIParam("iaSkinAirframe", indexArg) && !indexArg.IsEmpty())
			airframe = indexArg.ToInt();
		float spacing = SPACING_M;
		if (System.GetCLIParam("iaSkinSpacing", spacingArg) && !spacingArg.IsEmpty())
			spacing = spacingArg.ToFloat();

		IA_HeliPaintFamily family = IA_HeliPaintChannels.FindFamily(key);
		if (!family || airframe < 0 || airframe >= family.m_aStockPrefabs.Count())
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: no such family or airframe: " + key, LogLevel.ERROR);
			return 1;
		}

		int failures;
		ResourceName stock = family.m_aStockPrefabs[airframe];
		int first = IA_HeliPaintChannels.ToChannel(family, 1);
		IEntity side = SpawnTurned(world, origin, IA_HeliPaintChannels.FindChannelPrefab(stock, first), 0);
		IEntity nose = SpawnTurned(world, origin - m_vSide * spacing, IA_HeliPaintChannels.FindChannelPrefab(stock, first + 1), 60);
		IEntity tail = SpawnTurned(world, origin + m_vSide * spacing, IA_HeliPaintChannels.FindChannelPrefab(stock, first + 2), -120);
		failures = failures + Expect(side && nose && tail, "the airframe spawned on three channels");
		if (!side)
			return failures;
		failures = failures + Expect(IA_HeliSkinManagerComponent.GetVehicleChannel(side) == first, "the first one is on the family's first channel");
		Sleep(5000);

		if (engine)
		{
			BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(side.FindComponent(BaseVehicleControllerComponent));
			if (controller)
				controller.ForceStartEngine();
			Sleep(8000);
			if (controller)
				Mark(string.Format("Engine on: %1", controller.IsEngineOn()));
		}

		VObject mesh = side.GetVObject();
		Shot("livery_" + key + "_stock");

		int surfaceCount = family.m_aSurfaces.Count();
		int painted;
		int channel;
		foreach (IA_HeliSkinDef def : IA_HeliSkinCatalog.GetDefs())
		{
			for (channel = first; channel < first + 3; channel++)
			{
				painted = IA_HeliSkinPaint.Apply(channel, def.m_iId);
			}
			failures = failures + Expect(painted == surfaceCount, def.m_sKey + " paints every surface");
			Sleep(2500);
			Shot("livery_" + key + "_" + def.m_sKey);
		}

		for (channel = first; channel < first + 3; channel++)
		{
			IA_HeliSkinPaint.Apply(channel, IA_HeliSkinCatalog.SKIN_NONE);
		}
		Sleep(2500);
		Shot("livery_" + key + "_restored");

		failures = failures + Expect(!side.IsDeleted() && side.GetVObject() == mesh, "the hull kept its mesh");
		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! \param yaw degrees turned from side-on, about the vertical
	protected IEntity SpawnTurned(BaseWorld world, vector origin, ResourceName prefab, float yaw)
	{
		float sine = Math.Sin(yaw * Math.DEG2RAD);
		float cosine = Math.Cos(yaw * Math.DEG2RAD);
		vector right = -m_vAway;
		vector forward = m_vSide;
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = right * cosine - forward * sine;
		params.Transform[1] = vector.Up;
		params.Transform[2] = right * sine + forward * cosine;
		params.Transform[3] = origin;
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params);
		Mark(string.Format("spawned %1 from %2 yaw %3", entity, prefab, yaw));
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	protected int Expect(bool condition, string what)
	{
		if (condition)
		{
			Mark("ok: " + what);
			return 0;
		}
		Mark("FAIL: " + what);
		Print("[IA][HeliSkinLiveProbe] FAIL: " + what, LogLevel.ERROR);
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(BaseWorld world, vector origin, ResourceName prefab)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = -m_vAway;
		params.Transform[1] = vector.Up;
		params.Transform[2] = m_vSide;
		params.Transform[3] = origin;
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params);
		Mark(string.Format("spawned %1 from %2", entity, prefab));
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	protected void Shot(string name)
	{
		System.MakeScreenshot("$profile:IA_HeliSkinLiveProbe_" + name + ".bmp");
		Sleep(1500);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a soldier and moves him into the pilot seat, as a skin menu would be used from there.
	protected bool SeatPilot(IEntity heli, BaseWorld world, vector origin)
	{
		IEntity pilot = Spawn(world, origin, PILOT);
		if (!pilot)
		{
			Print("[IA][HeliSkinLiveProbe] FAIL: the pilot did not spawn", LogLevel.ERROR);
			return false;
		}
		Sleep(2000);

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(pilot.FindComponent(SCR_CompartmentAccessComponent));
		bool asked = access && access.MoveInVehicle(heli, ECompartmentType.PILOT);
		Sleep(4000);
		m_Pilot = pilot;
		bool seated = IsSeated();
		Mark(string.Format("Pilot asked=%1 seated=%2", asked, seated));
		if (!seated)
			Print("[IA][HeliSkinLiveProbe] FAIL: the pilot was not seated", LogLevel.ERROR);
		return seated;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSeated()
	{
		if (!m_Pilot)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(m_Pilot.FindComponent(SCR_CompartmentAccessComponent));
		return access && access.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	//! The console log is not flushed when the engine dies; a file closed after each line is.
	protected void Mark(string message)
	{
		Print("[IA][HeliSkinLiveProbe] " + message, LogLevel.NORMAL);
		FileHandle file = FileIO.OpenFile("$profile:IA_HeliSkinLiveProbe.log", FileMode.APPEND);
		if (!file)
			return;
		file.WriteLine(string.Format("%1 %2", System.GetTickCount(), message));
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! The technique that crashed in logs_2026-09-30_21-32-54: a new mesh instance for the hull.
	protected bool Remap(IEntity ent)
	{
		VObject mesh = ent.GetVObject();
		if (!mesh)
			return false;

		string materials[256];
		int count = mesh.GetMaterials(materials);
		string remap;
		ResourceName material;
		int i;
		for (i = 0; i < count; i++)
		{
			material = ResourceName.Empty;
			if (materials[i].StartsWith("UH_1H_Body01"))
				material = IA_HeliPaintChannels.GetMaterial(0, 4);
			else if (materials[i].StartsWith("UH_1H_Interior01"))
				material = IA_HeliPaintChannels.GetMaterial(1, 4);
			else if (materials[i].StartsWith("UH_1H_Interior02"))
				material = IA_HeliPaintChannels.GetMaterial(2, 4);
			if (material.IsEmpty())
				continue;
			remap = remap + string.Format("$remap '%1' '%2';", materials[i], material);
		}
		if (remap.IsEmpty())
			return false;

		ent.SetObject(mesh, remap);
		return true;
	}
}
#endif
