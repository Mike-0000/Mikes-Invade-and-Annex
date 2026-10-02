#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Play-mode probe for the pilot's paint bay. Opens a small world, starts game
//! mode, stands a helicopter on the first paint channel of its family in front
//! of the camera, opens the paint bay over it and walks it through its states.
//! -iaMenuFamily <key> picks the family (default uh1h); -iaMenuArt <key> draws
//! it with another silhouette, an unknown key being the generic one a modded
//! helicopter gets. Each state is announced
//! in $profile:IA_HeliPaintMenuProbe.log as "shot <name>" and held for a moment,
//! so a script watching the log can capture the window (the engine's own
//! screenshot leaves the UI out).
//!
//! The world has no game mode, no pilot and no server, so the menu is fed its
//! state through IA_HeliPaintMenu.ProbeSet. This shows what the bay draws and
//! that its input context exists. It does not show the key opening the bay from
//! a seat, or the server's side of a repaint.
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA paint bay menu probe", wbModules: {"ResourceManager"})]
class IA_HeliPaintMenuProbe : WorkbenchPlugin
{
	protected static const string WORLD = "worlds/Showcase/PBR_Vehicles.ent";
	protected static const string FAMILY = "uh1h";
	protected static const int GAME_MODE_TIMEOUT_MS = 240000;
	protected static const float DISTANCE_M = 24;

	protected vector m_vSide;
	protected vector m_vAway;
	protected int m_iChannel;
	protected int m_iFailures;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		Probe();
		if (m_iFailures == 0)
			Print("[IA][HeliPaintMenuProbe] PASS", LogLevel.NORMAL);
		else
			Print(string.Format("[IA][HeliPaintMenuProbe] FAIL failures=%1", m_iFailures), LogLevel.ERROR);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void Probe()
	{
		Mark("start");
		Workbench.OpenModule(WorldEditor);
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || !editor.SetOpenedResource(WORLD))
		{
			Fail("the test world did not open");
			return;
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
				Fail("game mode did not start");
				return;
			}
		}
		Mark("game mode");
		Sleep(8000);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Fail("no game world");
			return;
		}

		vector camera[4];
		world.GetCurrentCamera(camera);
		vector forward = camera[2];
		forward[1] = 0;
		forward.Normalize();
		vector right = camera[0];
		right[1] = 0;
		right.Normalize();
		vector origin = camera[3] + forward * DISTANCE_M;
		m_vSide = right;
		m_vAway = forward;
		origin[1] = Math.Max(world.GetSurfaceY(origin[0], origin[2]), 0) + 0.3;

		// GetCLIParam empties its output when the argument is absent.
		string key;
		if (!System.GetCLIParam("iaMenuFamily", key) || key.IsEmpty())
			key = FAMILY;
		IA_HeliPaintFamily family = IA_HeliPaintChannels.FindFamily(key);
		if (!family || family.m_aStockPrefabs.IsEmpty())
		{
			Fail("no paint family " + key);
			return;
		}
		string art;
		if (System.GetCLIParam("iaMenuArt", art) && !art.IsEmpty())
			family.m_sArt = art;
		m_iChannel = IA_HeliPaintChannels.ToChannel(family, 1);
		Mark(string.Format("family %1 art %2 channel %3", family.m_sKey, family.m_sArt, m_iChannel));

		IEntity heli = Spawn(world, origin, IA_HeliPaintChannels.FindChannelPrefab(family.m_aStockPrefabs[0], m_iChannel));
		if (!heli)
		{
			Fail("the helicopter did not spawn");
			return;
		}
		Sleep(4000);

		ProbeInput();
		ProbeMenu();

		IA_HeliSkinPaint.Apply(m_iChannel, IA_HeliSkinCatalog.SKIN_NONE);
		IA_HeliPaintMenu.ProbeEnd();
		Sleep(1000);
		Mark(string.Format("done failures=%1", m_iFailures));
		editor.SwitchToEditMode();
		Sleep(1000);
	}

	//------------------------------------------------------------------------------------------------
	//! The key's context and action come from this addon's input config; unknown names mean it did not merge.
	protected void ProbeInput()
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
		{
			Fail("no input manager");
			return;
		}

		bool activated = input.ActivateContext(IA_HeliPaintHotkey.CONTEXT, 2000);
		Sleep(300);
		bool active = input.IsContextActive(IA_HeliPaintHotkey.CONTEXT);
		Mark(string.Format("context activated=%1 active=%2", activated, active));
		if (!activated)
			Fail("the pilot input context is unknown to the input manager");
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeMenu()
	{
		int tanSkin = IA_HeliSkinCatalog.SKIN_DESERT_TAN;
		int none = IA_HeliSkinCatalog.SKIN_NONE;
		IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(tanSkin);
		if (!def)
		{
			Fail("the catalog has no tan skin");
			return;
		}
		int required = def.m_iRequiredPoints;
		int partial = required * 0.62;

		// Stock paint, a rating most of the way to the tan skin.
		IA_HeliPaintMenu.ProbeSet(true, m_iChannel, none, partial);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_HeliPaintMenu);
		Sleep(2500);
		if (!IA_HeliPaintMenu.IsBayOpen())
		{
			Fail("the paint bay did not open");
			return;
		}
		Shot("stock");

		if (!IA_HeliPaintMenu.ProbeFocus(tanSkin))
			Fail("the bay lists no tan livery");
		Sleep(1800);
		Shot("locked");

		if (IA_HeliPaintMenu.ProbePick(tanSkin) != IA_HeliPaintBay.NO_SKIN)
			Fail("a locked livery was sent to the server");
		Sleep(250);
		Shot("refused");

		// Rating unknown.
		IA_HeliPaintMenu.ProbeSet(true, m_iChannel, none, IA_TransportPilotRecord.RATING_UNKNOWN);
		Sleep(1800);
		Shot("syncing");

		// Earned.
		IA_HeliPaintMenu.ProbeSet(true, m_iChannel, none, required + 1840);
		Sleep(3600);
		Shot("ready");

		if (IA_HeliPaintMenu.ProbePick(tanSkin) != tanSkin)
			Fail("an unlocked livery was not sent to the server");
		Sleep(500);
		Shot("painting");

		// What the server's answer does: the channel is painted and the worn skin arrives.
		IA_HeliSkinPaint.Apply(m_iChannel, tanSkin);
		IA_HeliPaintMenu.ProbeSet(true, m_iChannel, tanSkin, required + 1840);
		Sleep(280);
		Shot("applied");
		Sleep(2500);
		Shot("worn");

		// Back on stock paint below the threshold: the livery is locked again, whoever the pilot is.
		IA_HeliPaintMenu.ProbeFocus(none);
		IA_HeliPaintMenu.ProbeSet(true, m_iChannel, none, partial);
		IA_HeliSkinPaint.Apply(m_iChannel, none);
		Sleep(600);
		IA_HeliPaintMenu.ProbeFocus(tanSkin);
		Sleep(1800);
		if (IA_HeliPaintMenu.ProbePick(tanSkin) != IA_HeliPaintBay.NO_SKIN)
			Fail("a livery below its threshold was sent to the server");
		Shot("relocked");

		// A helicopter with no paint channel.
		IA_HeliPaintMenu.ProbeSet(false, IA_HeliPaintChannels.CHANNEL_NONE, none, partial);
		Sleep(1800);
		Shot("norig");

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_HeliPaintMenu);
		Sleep(800);
		if (IA_HeliPaintMenu.IsBayOpen())
			Fail("the paint bay did not close");
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
		// MakeScreenshot leaves the UI out; a watcher of this log captures the window instead.
		Mark("shot " + name);
		Sleep(900);
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string message)
	{
		m_iFailures = m_iFailures + 1;
		Print("[IA][HeliPaintMenuProbe] FAIL: " + message, LogLevel.ERROR);
		Mark("FAIL: " + message);
	}

	//------------------------------------------------------------------------------------------------
	//! The console log is not flushed when the engine dies; a file closed after each line is.
	protected void Mark(string message)
	{
		Print("[IA][HeliPaintMenuProbe] " + message, LogLevel.NORMAL);
		FileHandle file = FileIO.OpenFile("$profile:IA_HeliPaintMenuProbe.log", FileMode.APPEND);
		if (!file)
			return;
		file.WriteLine(string.Format("%1 %2", System.GetTickCount(), message));
		file.Close();
	}
}
#endif
