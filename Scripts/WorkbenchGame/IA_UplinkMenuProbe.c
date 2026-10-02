#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Opens the leaderboard and the admin menu in play mode and walks every board, sort order,
//! failure state and settings page, marking a screenshot point at each. The leaderboard is fed
//! by the menu's own stand-in server, so no game mode and no stats service are needed.
//!
//! Marks go to $profile:IA_UplinkMenuProbe.log; a line holding " shot <name>" is where the
//! caller captures the window. The wire format is checked before the menus open.
//!   -iaUplinkPart 1  leaderboard only
//!   -iaUplinkPart 2  admin menu only
//!   -iaUplinkFull 1  play full screen instead of in the editor viewport
//!   -iaUplinkLive 1  play an Invade & Annex world instead and read the boards from the stats
//!                    service this Workbench profile is registered with; nothing is made up
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA uplink menu probe", wbModules: {"ResourceManager"})]
class IA_UplinkMenuProbe : WorkbenchPlugin
{
	protected static const string WORLD = "worlds/Showcase/PBR_Vehicles.ent";
	protected static const string WORLD_LIVE = "Worlds/IA_Arland.ent";
	protected static const int LIVE_READY_TIMEOUT_MS = 240000;
	protected static const int LIVE_ANSWER_TIMEOUT_MS = 40000;
	protected static const int GAME_MODE_TIMEOUT_MS = 240000;
	protected static const int PART_BOARD = 1;
	protected static const int PART_ADMIN = 2;
	protected static const int ADMIN_PAGES = 9;

	protected int m_iFailures;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		Probe();
		if (m_iFailures == 0)
			Print("[IA][UplinkMenuProbe] PASS", LogLevel.NORMAL);
		else
			Print(string.Format("[IA][UplinkMenuProbe] FAIL failures=%1", m_iFailures), LogLevel.ERROR);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	protected void Probe()
	{
		Mark("start");
		CheckWire();

		int part;
		string partArg;
		if (System.GetCLIParam("iaUplinkPart", partArg) && !partArg.IsEmpty())
			part = partArg.ToInt();

		bool live;
		string liveArg;
		if (System.GetCLIParam("iaUplinkLive", liveArg) && !liveArg.IsEmpty())
			live = liveArg.ToInt() != 0;
		string world = WORLD;
		if (live)
			world = WORLD_LIVE;

		Workbench.OpenModule(WorldEditor);
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || !editor.SetOpenedResource(world))
		{
			Fail("could not open the probe world");
			return;
		}
		// Full screen draws the menus at the size players see; the editor viewport is about half that.
		bool fullScreen;
		string fullArg;
		if (System.GetCLIParam("iaUplinkFull", fullArg) && !fullArg.IsEmpty())
			fullScreen = fullArg.ToInt() != 0;

		Sleep(500);
		editor.SwitchToGameMode(false, fullScreen);
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

		if (live)
		{
			ProbeLive();
		}
		else
		{
			if (part != PART_ADMIN)
				ProbeBoards();
			if (part != PART_BOARD)
				ProbeAdmin();
		}

		Mark(string.Format("done failures=%1", m_iFailures));
		editor.SwitchToEditMode();
		Sleep(1000);
		// A mission world is still being rebuilt for the editor; leaving during that crashes Workbench.
		if (live)
			Sleep(15000);
	}

	//------------------------------------------------------------------------------------------------
	//! The packed row and the service's answer, with no menu open.
	protected void CheckWire()
	{
		string line = IA_BoardRow.Pack(7, "a|pipe\nname", 1200, 30, 4, 9, 55, 48000, 610, 42, 0, 3);
		IA_BoardRow row = IA_BoardRow.Unpack(line);
		if (!row)
		{
			Fail("a packed row did not unpack");
		}
		else
		{
			Expect(row.m_iRank == 7, "unpacked rank");
			Expect(row.m_sLabel == "a|pipe name", "unpacked name keeps its separator and drops the line break");
			Expect(row.m_iKills == 1200 && row.m_iDeaths == 30, "unpacked kills and deaths");
			Expect(row.m_iTransport == 610 && row.m_iInsertions == 42, "unpacked transport rating and insertions");
			Expect(row.m_iScore == 48000 && row.m_iGrade == 3, "unpacked score and grade");
			Expect(row.Text(IA_BoardRow.F_KD) == "40.00", "kill ratio text");
			Expect(row.Text(IA_BoardRow.F_SCORE) == "48,000", "score text");
		}
		Expect(!IA_BoardRow.Unpack("1|2|3"), "a short line is refused");

		string answer = "{\"board\":\"global\",\"sort\":\"transport\",\"dir\":\"desc\",\"offset\":25,\"total\":17473,";
		answer = answer + "\"rows\":[{\"r\":26,\"n\":\"Ace\",\"k\":10,\"d\":2,\"h\":1,\"g\":3,\"o\":40,\"s\":900,\"t\":120,\"i\":14,\"p\":0},";
		answer = answer + "{\"r\":27,\"n\":\"Bo\",\"k\":8,\"d\":1,\"h\":0,\"g\":0,\"o\":12,\"s\":700,\"t\":118,\"i\":11,\"p\":0}],";
		answer = answer + "\"me\":[{\"r\":412,\"n\":\"Me\",\"k\":3,\"d\":3,\"h\":0,\"g\":0,\"o\":5,\"s\":90,\"t\":20,\"i\":2,\"p\":0}]}";

		string board;
		string sortName;
		bool descending;
		int offset;
		int total;
		ref array<string> rows = {};
		ref array<string> mine = {};
		if (!IA_LeaderboardManagerComponent.ParseAnswer(answer, board, sortName, descending, offset, total, rows, mine))
		{
			Fail("the service answer did not parse");
		}
		else
		{
			Expect(board == "global" && sortName == "transport" && descending, "answer board, sort and direction");
			Expect(offset == 25 && total == 17473, "answer offset and total");
			Expect(rows.Count() == 2 && mine.Count() == 1, "answer row counts");
			if (rows.Count() == 2)
			{
				IA_BoardRow second = IA_BoardRow.Unpack(rows[1]);
				Expect(second && second.m_iRank == 27 && second.m_iTransport == 118 && second.m_sLabel == "Bo", "answer row fields");
			}
		}
		Expect(!IA_LeaderboardManagerComponent.ParseAnswer("{\"board\":\"global\"}", board, sortName, descending, offset, total, rows, mine), "an answer with no rows is refused");

		// What the service sends for an empty board, and when only the player's own line is asked for.
		string bare = "{\"board\":\"server\",\"sort\":\"score\",\"dir\":\"asc\",\"offset\":0,\"total\":0,\"rows\":[],\"me\":[]}";
		if (!IA_LeaderboardManagerComponent.ParseAnswer(bare, board, sortName, descending, offset, total, rows, mine))
			Fail("an answer with empty rows did not parse");
		else
			Expect(rows.IsEmpty() && mine.IsEmpty() && !descending && total == 0, "an answer with empty rows is an empty page");

		Expect(IA_BoardProtocol.BoardName(IA_BoardProtocol.BOARD_SERVERS) == "servers", "board name on the wire");
		Expect(IA_BoardProtocol.SortName(IA_BoardProtocol.SORT_TRANSPORT) == "transport", "sort name on the wire");
		Expect(IA_BoardProtocol.PageOffset(63) == 50, "page offset");
		Mark("wire checked");
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeBoards()
	{
		// The largest server on record, and a player in the middle of it.
		IA_StatisticsMenu.ProbeBegin(1151, 412);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeTotal() == 1151, "session board total");
		Expect(IA_StatisticsMenu.ProbeRankAt(0) == 1, "session board first row");
		Shot("board_session");

		Expect(IA_StatisticsMenu.ProbeTab(IA_BoardProtocol.BOARD_SERVER), "server tab");
		Sleep(1500);
		Shot("board_server");

		// Each column the board sorts by.
		ref array<int> sorts = {IA_BoardProtocol.SORT_KILLS, IA_BoardProtocol.SORT_DEATHS, IA_BoardProtocol.SORT_KD, IA_BoardProtocol.SORT_HVT, IA_BoardProtocol.SORT_GUARD, IA_BoardProtocol.SORT_OBJ, IA_BoardProtocol.SORT_TRANSPORT, IA_BoardProtocol.SORT_INSERTIONS, IA_BoardProtocol.SORT_SCORE};
		int sortCount = sorts.Count();
		for (int s = 0; s < sortCount; s++)
		{
			if (!IA_StatisticsMenu.ProbeSort(sorts[s]))
				Fail("the server board would not sort by " + IA_BoardProtocol.SortName(sorts[s]));
			Sleep(700);
			if (IA_StatisticsMenu.ProbeRankAt(0) != 1)
				Fail("no rows after sorting by " + IA_BoardProtocol.SortName(sorts[s]));
		}
		Expect(!IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_PLAYERS), "a player board has no players column");

		IA_StatisticsMenu.ProbeTool(0);
		Sleep(1500);
		Expect(IA_StatisticsMenu.ProbeDescending(), "best pilots sorts downward");
		Shot("board_pilots");

		// The same column again turns the order over.
		IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_TRANSPORT);
		Sleep(1500);
		Expect(!IA_StatisticsMenu.ProbeDescending(), "a second pick reverses the order");
		Shot("board_pilots_reversed");
		IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_SCORE);
		Sleep(800);

		// Deep in the board: rows that were never sent until now.
		IA_StatisticsMenu.ProbeScroll(700);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeRankAt(700) == 701, "a row 700 down arrives");
		Expect(IA_StatisticsMenu.ProbeRankAt(300) == 0, "rows never scrolled to are not sent");
		Shot("board_deep");

		IA_StatisticsMenu.ProbeScroll(1150);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeRankAt(1150) == 1151, "the last row arrives");
		Shot("board_end");

		IA_StatisticsMenu.ProbeTool(2);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeRankAt(411) == 412, "find me brings the player's row");
		Shot("board_find_me");

		IA_StatisticsMenu.ProbeTool(1);
		Sleep(1200);
		Expect(IA_StatisticsMenu.ProbeFirstVisible() == 0, "top goes back to the first row");

		// Every player on record.
		IA_StatisticsMenu.ProbeBegin(17473, 9000);
		Expect(IA_StatisticsMenu.ProbeTab(IA_BoardProtocol.BOARD_GLOBAL), "global tab");
		Sleep(1500);
		Expect(IA_StatisticsMenu.ProbeTotal() == 17473, "global board total");
		Shot("board_global");
		IA_StatisticsMenu.ProbeScroll(17472);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeRankAt(17472) == 17473, "the last of 17,473 rows arrives");
		Shot("board_global_end");

		IA_StatisticsMenu.ProbeBegin(90, 7);
		Expect(IA_StatisticsMenu.ProbeTab(IA_BoardProtocol.BOARD_SERVERS), "servers tab");
		Sleep(1500);
		Expect(IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_PLAYERS), "servers sort by players");
		Sleep(1200);
		Shot("board_servers");

		// What the board says when there is nothing to show.
		IA_StatisticsMenu.ProbeStatus(IA_BoardProtocol.STATUS_OFFLINE);
		Sleep(1500);
		Shot("board_offline");
		IA_StatisticsMenu.ProbeStatus(IA_BoardProtocol.STATUS_FAILED);
		Sleep(1500);
		Shot("board_no_signal");
		IA_StatisticsMenu.ProbeBegin(0, 0);
		IA_StatisticsMenu.ProbeStatus(IA_BoardProtocol.STATUS_OK);
		Sleep(1500);
		Expect(IA_StatisticsMenu.ProbeTotal() == 0, "an empty board answers with no rows");
		Shot("board_empty");

		Expect(IA_StatisticsMenu.ProbeTab(4), "options tab");
		Sleep(1200);
		bool hidden = IA_StatisticsMenu.ProbeToggleRankHud();
		Sleep(600);
		Shot("board_options");
		Expect(IA_StatisticsMenu.ProbeToggleRankHud() != hidden, "the rank HUD option switches both ways");

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_StatisticsMenu);
		IA_StatisticsMenu.ProbeEnd();
		Sleep(800);
	}

	//------------------------------------------------------------------------------------------------
	//! The boards as a hosting player gets them: through the player controller, the game mode's
	//! leaderboard manager and the stats service. Only counts are marked, never names.
	protected void ProbeLive()
	{
		int waited;
		while (!IA_LeaderboardManagerComponent.GetInstance() || !SCR_PlayerController.Cast(GetGame().GetPlayerController()))
		{
			Sleep(500);
			waited = waited + 500;
			if (waited > LIVE_READY_TIMEOUT_MS)
			{
				Fail("the live world has no leaderboard manager or no player controller");
				return;
			}
		}
		Mark(string.Format("live world ready after %1 ms", waited));
		// The mission links to the stats service five seconds after it starts.
		Sleep(9000);

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(2500);
		Mark(string.Format("live session total=%1", IA_StatisticsMenu.ProbeTotal()));
		Shot("live_session");

		LiveBoard(IA_BoardProtocol.BOARD_SERVER, "live_server");

		int players = LiveBoard(IA_BoardProtocol.BOARD_GLOBAL, "live_global");
		if (players > 0)
		{
			IA_StatisticsMenu.ProbeTool(0);
			Expect(WaitLiveRow(0), "the global board sorted by transport rating");
			Shot("live_global_pilots");

			// Rows far down the board, which only a page of their own brings.
			IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_KILLS);
			Expect(WaitLiveRow(0), "the global board sorted by kills");
			int deep = players / 2;
			IA_StatisticsMenu.ProbeScroll(deep);
			Expect(WaitLiveRow(deep), "a row half way down the global board");
			Mark(string.Format("live global row %1 holds place %2", deep, IA_StatisticsMenu.ProbeRankAt(deep)));
			Shot("live_global_deep");

			IA_StatisticsMenu.ProbeScroll(players - 1);
			Expect(WaitLiveRow(players - 1), "the last row of the global board");
			Shot("live_global_end");
		}

		int servers = LiveBoard(IA_BoardProtocol.BOARD_SERVERS, "live_servers");
		if (servers > 0)
		{
			Expect(IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_PLAYERS), "servers sort by players");
			Expect(WaitLiveRow(0), "the servers board sorted by players");
			Shot("live_servers_players");
			IA_StatisticsMenu.ProbeTool(2);
			Sleep(4000);
			Shot("live_servers_find");
		}

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(800);
	}

	//------------------------------------------------------------------------------------------------
	//! Open one stored board and wait for the service's first answer.
	//! eturn the rows the board holds, -1 when no answer came
	protected int LiveBoard(int board, string shot)
	{
		if (!IA_StatisticsMenu.ProbeTab(board))
		{
			Fail("the " + IA_BoardProtocol.BoardName(board) + " tab did not open");
			return -1;
		}

		int total = -1;
		int waited;
		while (waited < LIVE_ANSWER_TIMEOUT_MS)
		{
			Sleep(250);
			waited = waited + 250;
			total = IA_StatisticsMenu.ProbeTotal();
			if (total == 0)
				break;
			if (total > 0 && IA_StatisticsMenu.ProbeRankAt(0) > 0)
				break;
		}
		Mark(string.Format("%1 total=%2 after %3 ms", shot, total, waited));
		if (total < 0)
			Fail("the " + IA_BoardProtocol.BoardName(board) + " board got no answer from the stats service");
		Sleep(600);
		Shot(shot);
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn true once the row at an index has arrived
	protected bool WaitLiveRow(int index)
	{
		int waited;
		while (waited < LIVE_ANSWER_TIMEOUT_MS)
		{
			Sleep(250);
			waited = waited + 250;
			if (IA_StatisticsMenu.ProbeRankAt(index) > 0)
			{
				Sleep(600);
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeAdmin()
	{
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_AdminConfigMenu);
		Sleep(2500);

		for (int page = 0; page < ADMIN_PAGES; page++)
		{
			if (!IA_AdminConfigMenu.ProbeTab(page))
			{
				Fail(string.Format("admin page %1 did not open", page));
				continue;
			}
			Sleep(900);
			int controls = IA_AdminConfigMenu.ProbeControls();
			Mark(string.Format("admin page %1 controls=%2", page, controls));
			// The rail and the nine action buttons are on every page.
			if (controls < 12)
				Fail(string.Format("admin page %1 offers only %2 controls", page, controls));
			Shot(string.Format("admin_%1", page));

			// Pages longer than the viewport, a screen at a time; the scroll stops at the end.
			IA_AdminConfigMenu.ProbeScroll(400);
			Sleep(700);
			Shot(string.Format("admin_%1_b", page));
			IA_AdminConfigMenu.ProbeScroll(800);
			Sleep(700);
			Shot(string.Format("admin_%1_c", page));
			IA_AdminConfigMenu.ProbeScroll(1200);
			Sleep(700);
			Shot(string.Format("admin_%1_d", page));
		}

		Expect(IA_AdminConfigMenu.ProbeTab(0), "admin first page");
		Sleep(700);
		Expect(IA_AdminConfigMenu.ProbeHelp(), "help overlay opens");
		Sleep(1500);
		Shot("admin_help");
		Expect(!IA_AdminConfigMenu.ProbeHelp(), "help overlay closes");
		Sleep(600);

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_AdminConfigMenu);
		Sleep(800);
	}

	//------------------------------------------------------------------------------------------------
	protected void Expect(bool held, string what)
	{
		if (!held)
			Fail(what);
	}

	//------------------------------------------------------------------------------------------------
	protected void Shot(string name)
	{
		Mark("shot " + name);
		Sleep(900);
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string message)
	{
		m_iFailures = m_iFailures + 1;
		Print("[IA][UplinkMenuProbe] FAIL: " + message, LogLevel.ERROR);
		Mark("FAIL: " + message);
	}

	//------------------------------------------------------------------------------------------------
	protected void Mark(string message)
	{
		Print("[IA][UplinkMenuProbe] " + message, LogLevel.NORMAL);
		FileHandle file = FileIO.OpenFile("$profile:IA_UplinkMenuProbe.log", FileMode.APPEND);
		if (!file)
			return;
		file.WriteLine(string.Format("%1 %2", System.GetTickCount(), message));
		file.Close();
	}
}
#endif
