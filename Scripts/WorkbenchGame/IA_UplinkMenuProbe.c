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
//!   -iaUplinkPart 3  what the server holds and how often it may ask the stats service, only:
//!                    IA_BoardService against a made-up stats service, with counted requests
//!   -iaUplinkPart 4  the timed exchange with the stats service, only: every request held back
//!                    and counted, every answer made up
//!   -iaUplinkPart 5  the timed exchange against the local development server (backend/azure-functions,
//!                    npm run dev). Needs -iaApiBase http://127.0.0.1:7071/api and is refused
//!                    without it; never part of a run that names no part
//!   -iaUplinkFull 1  play full screen instead of in the editor viewport
//!   -iaUplinkLive 1  play an Invade & Annex world instead and read the boards from the stats
//!                    service this Workbench profile is registered with; nothing is made up.
//!                    With -iaApiBase it reads the development server instead, and then also
//!                    waits for the timer's exchange to bring the open board
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
	protected static const int PART_BUDGET = 3;
	protected static const int PART_SYNC = 4;
	protected static const int PART_DEV = 5;
	protected static const int ADMIN_PAGES = 9;
	protected static const int SETTLE_TIMEOUT_MS = 10000;
	protected static const int SYNC_ANSWER_TIMEOUT_MS = 15000;
	protected static const int LIVE_EXCHANGE_TIMEOUT_MS = 80000;
	// A server only the development stats service knows, and the players it is seeded with.
	protected static const string DEV_SERVER = "7e57ab1e-0000-4000-8000-000000000001";
	protected static const string DEV_PLAYER = "5eed0000-0000-4000-9000-00000000000";

	protected int m_iFailures;
	protected ref IA_BoardService m_DevBoards;
	protected ref array<int> m_aCallBase = {};
	protected ref array<int> m_aCallDelta = {};

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
		else if (part == PART_DEV)
		{
			ProbeDev();
		}
		else
		{
			if (part == 0 || part == PART_BOARD)
				ProbeBoards();
			if (part == 0 || part == PART_BUDGET)
				ProbeBudget();
			if (part == 0 || part == PART_SYNC)
				ProbeSync();
			if (part == 0 || part == PART_ADMIN)
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
		if (!IA_BoardService.ParseAnswer(answer, board, sortName, descending, offset, total, rows, mine))
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
		Expect(!IA_BoardService.ParseAnswer("{\"board\":\"global\"}", board, sortName, descending, offset, total, rows, mine), "an answer with no rows is refused");

		// What the service sends for an empty board, and when only the player's own line is asked for.
		string bare = "{\"board\":\"server\",\"sort\":\"score\",\"dir\":\"asc\",\"offset\":0,\"total\":0,\"rows\":[],\"me\":[]}";
		if (!IA_BoardService.ParseAnswer(bare, board, sortName, descending, offset, total, rows, mine))
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
		// One player takes this whole journey, which is more than one player's share of the allowance.
		IA_ApiTunables.SetPlayerShare(100);

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(2500);
		Mark(string.Format("live session total=%1", IA_StatisticsMenu.ProbeTotal()));
		Shot("live_session");

		LiveBoard(IA_BoardProtocol.BOARD_SERVER, "live_server");
		LiveExchange();

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
		Mark("live: requests sent in all: " + IA_ApiHandler.GetInstance().CallSummary());
		IA_ApiTunables.Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! The timer's own turn, with this server's board open. On the development server the
	//! exchange must bring the board, and rows no request fetched are then shown without one. On
	//! the real stats service the turn is only marked: it has the exchange or it has not.
	protected void LiveExchange()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		int waited;
		while (sync.GetMode() == IA_ApiSync.MODE_UNKNOWN && waited < LIVE_EXCHANGE_TIMEOUT_MS)
		{
			Sleep(500);
			waited = waited + 500;
		}
		Sleep(1500);
		Mark(string.Format("live exchange: mode=%1 after %2 ms", sync.GetMode(), waited));
		Mark("live exchange: requests so far: " + api.CallSummary());
		if (!api.ProbeBaseOverridden())
			return;

		bool brought;
		IA_BoardService svc = IA_BoardService.GetInstance();
		if (svc)
			brought = !svc.ProbeEtag(IA_BoardProtocol.BOARD_SERVER).IsEmpty();
		Expect(sync.GetMode() == IA_ApiSync.MODE_SYNC, "the development server answers the timer's exchange");
		Expect(brought, "and the exchange brings the board the player has open");

		if (IA_StatisticsMenu.ProbeTotal() < 100)
			return;
		int before = api.GetCallCount(IA_ApiHandler.ROUTE_LEADERBOARD);
		IA_StatisticsMenu.ProbeScroll(90);
		Expect(WaitLiveRow(90), "a row near the hundredth of this server's board");
		int cost = api.GetCallCount(IA_ApiHandler.ROUTE_LEADERBOARD) - before;
		Mark(string.Format("live exchange: row 90 holds place %1, leaderboard requests for it=%2", IA_StatisticsMenu.ProbeRankAt(90), cost));
		Expect(cost == 0, "rows the exchange brought are shown with no request");
		Shot("live_exchange_rows");
		IA_StatisticsMenu.ProbeScroll(0);
		Sleep(600);
	}

	//------------------------------------------------------------------------------------------------
	//! Open one stored board and wait for the service's first answer.
	//! \return the rows the board holds, -1 when no answer came
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
	//! \return true once the row at an index has arrived
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
	//! What the server holds and how often it may ask: IA_BoardService against a made-up stats
	//! service, first with players that are only numbers, then with the menu itself. Nothing
	//! here reaches the real stats service, and the count of requests sent says so.
	protected void ProbeBudget()
	{
		int callsBefore = IA_ApiHandler.GetInstance().GetCallTotal();

		CheckCache();
		CheckTunables();
		CheckSplit();
		CheckDefaultAllowance();
		// These two count on a minute buying a request and on a burst small enough to spend in a
		// few pages, so they run on an allowance of their own.
		IA_ApiTunables.SetBudget(60, 20);
		CheckSharing();
		CheckAllowance();
		IA_ApiTunables.Reset();
		// The menu is watched running out of requests, so its allowance is a small one too.
		IA_ApiTunables.SetBudget(12, 20);
		CheckMenuBudget();
		IA_ApiTunables.Reset();

		int calls = IA_ApiHandler.GetInstance().GetCallTotal() - callsBefore;
		Mark(string.Format("budget probe: requests sent to the stats service=%1 (%2)", calls, IA_ApiHandler.GetInstance().CallSummary()));
		Expect(calls == 0, "the budget probe sent nothing to the stats service");
	}

	//------------------------------------------------------------------------------------------------
	//! At its cap the cache drops what it has held longest and keeps the rest.
	protected void CheckCache()
	{
		ref IA_BoardCache cache = new IA_BoardCache(3);
		PutPage(cache, "a", 1);
		PutPage(cache, "b", 2);
		PutPage(cache, "c", 3);
		PutPage(cache, "d", 4);
		Expect(cache.Count() == 3 && !cache.Get("a"), "the cache drops the entry held longest at its cap");
		Expect(cache.Get("b") && cache.Get("c") && cache.Get("d"), "the cache keeps the newest entries");

		// An entry put again is the newest.
		PutPage(cache, "b", 5);
		PutPage(cache, "e", 6);
		Expect(cache.Count() == 3 && !cache.Get("c"), "a refreshed entry outlives an older one");
		IA_BoardPage kept = cache.Get("b");
		Expect(kept && kept.m_iTotal == 5 && cache.Get("d") && cache.Get("e"), "the refreshed entry holds its new page");
		Mark("cache checked");
	}

	//------------------------------------------------------------------------------------------------
	protected void PutPage(notnull IA_BoardCache cache, string key, int total)
	{
		ref IA_BoardPage page = new IA_BoardPage();
		page.m_iTotal = total;
		cache.Put(key, page);
	}

	//------------------------------------------------------------------------------------------------
	//! No value from outside can switch a limit off.
	protected void CheckTunables()
	{
		IA_ApiTunables.Reset();
		IA_ApiTunables.SetBudget(100000, 100000);
		Expect(IA_ApiTunables.BudgetPerHour() == IA_ApiTunables.BUDGET_PER_HOUR_MAX && IA_ApiTunables.BudgetBurst() == IA_ApiTunables.BUDGET_BURST_MAX, "a budget above its range is brought down to it");
		IA_ApiTunables.SetBudget(1, 0);
		Expect(IA_ApiTunables.BudgetPerHour() == IA_ApiTunables.BUDGET_PER_HOUR_MIN && IA_ApiTunables.BudgetBurst() == IA_ApiTunables.BUDGET_BURST_MAX, "a budget below its range is brought up to it, and zero changes nothing");
		IA_ApiTunables.SetPlayerShare(1);
		Expect(IA_ApiTunables.PlayerPerHour() == 1, "a player's share is never less than one request");

		IA_ApiTunables.SetBoardLives(5, 99999);
		Expect(IA_ApiTunables.BoardLifeS(IA_BoardProtocol.BOARD_SERVER) == IA_ApiTunables.LIFE_SERVER_MIN_S, "a lifetime below its range is brought up to it");
		Expect(IA_ApiTunables.BoardLifeS(IA_BoardProtocol.BOARD_GLOBAL) == IA_ApiTunables.LIFE_WIDE_MAX_S && IA_ApiTunables.BoardLifeS(IA_BoardProtocol.BOARD_SERVERS) == IA_ApiTunables.LIFE_WIDE_MAX_S, "a lifetime above its range is brought down to it");
		IA_ApiTunables.SetSyncInterval(1);
		Expect(IA_ApiTunables.SyncIntervalS() == IA_ApiTunables.SYNC_INTERVAL_MIN_S, "the sync interval has a floor");

		IA_ApiTunables.Reset();
		Expect(IA_ApiTunables.BudgetPerHour() == 120 && IA_ApiTunables.BudgetBurst() == 200, "default budget");
		Expect(IA_ApiTunables.PlayerPerHour() == 60 && IA_ApiTunables.PlayerBurst() == 100, "default share of one player");
		Expect(IA_ApiTunables.BoardLifeS(IA_BoardProtocol.BOARD_SERVER) == 120 && IA_ApiTunables.BoardLifeS(IA_BoardProtocol.BOARD_GLOBAL) == 300, "default lifetimes");
		Expect(IA_ApiTunables.SyncIntervalS() == 60, "default sync interval");
		Mark("tunables checked");
	}

	//------------------------------------------------------------------------------------------------
	//! A page goes out in strings short enough for one RPC each, with no row lost or moved.
	protected void CheckSplit()
	{
		ref array<string> rows = {};
		for (int i = 0; i < IA_BoardProtocol.PAGE_ROWS; i++)
		{
			rows.Insert(IA_BoardRow.Pack(i + 1, "The Longest Callsign In The Whole Theatre Of War", 1200, 30, 4, 9, 55, 48000, 610, 42, 0, 3));
		}

		ref array<string> chunks = {};
		ref array<int> firsts = {};
		IA_BoardProtocol.SplitRows(rows, 50, chunks, firsts);

		ref array<string> lines = {};
		int rowsSeen;
		bool sound = true;
		int chunkCount = chunks.Count();
		for (int c = 0; c < chunkCount; c++)
		{
			string chunk = chunks[c];
			if (chunk.Length() > IA_BoardProtocol.CHUNK_CHARS || firsts[c] != 50 + rowsSeen)
				sound = false;
			lines.Clear();
			chunk.Split("\n", lines, true);
			rowsSeen = rowsSeen + lines.Count();
		}
		Mark(string.Format("split: %1 rows in %2 chunks", rowsSeen, chunkCount));
		Expect(chunkCount > 1 && sound && rowsSeen == IA_BoardProtocol.PAGE_ROWS, "a page splits into chunks under the string limit, in order");

		rows.Clear();
		IA_BoardProtocol.SplitRows(rows, 75, chunks, firsts);
		Expect(chunks.Count() == 1 && firsts.Count() == 1, "a page of no rows is one chunk");
		if (chunks.Count() == 1 && firsts.Count() == 1)
			Expect(chunks[0].IsEmpty() && firsts[0] == 75, "a page of no rows is an empty chunk at its offset");
	}

	//------------------------------------------------------------------------------------------------
	//! Pages are shared by everyone who asks; a player's own line is theirs alone.
	protected void CheckSharing()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int score = IA_BoardProtocol.SORT_SCORE;
		int ok = IA_BoardProtocol.STATUS_OK;

		ref IA_BoardService svc = new IA_BoardService();
		svc.ProbeBegin(1151);
		svc.Open();

		// Two players open the same board in the same frame.
		svc.Request(11, 1, server, score, true, 0);
		svc.Request(12, 1, server, score, true, 0);
		WaitIdle(svc);
		MarkCounts("two players, one page", svc);
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_PAGE) == 1, "two players asking for one page cause one page request");
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_OWN) == 1 && svc.Count(IA_BoardService.COUNT_JOINED) == 1, "the second player waits on the first's request and adds only an own-line request");
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) == 25 && svc.ProbeAnswerTotal(11) == 1151, "the first player gets the page");
		Expect(svc.ProbeStatus(12) == ok && svc.ProbeRows(12) == 25 && svc.ProbeAnswerTotal(12) == 1151, "the second player gets the page");
		Expect(svc.ProbeMineRank(11) == svc.ProbeRankOf(11) && svc.ProbeMineRank(12) == svc.ProbeRankOf(12), "each player gets their own line and not the other's");

		// Inside the lifetime nothing is asked again, and the answer does not wait.
		int fetches = Fetches(svc);
		svc.ProbeForget(11);
		svc.ProbeForget(12);
		svc.Request(11, 2, server, score, true, 0);
		svc.Request(12, 2, server, score, true, 0);
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) == 25 && svc.ProbeStatus(12) == ok, "a page inside its lifetime is answered at once");
		WaitIdle(svc);
		MarkCounts("both reopen inside the lifetime", svc);
		Expect(Fetches(svc) == fetches && svc.Count(IA_BoardService.COUNT_SERVED_HELD) == 2, "reopening a board inside its lifetime asks the stats service nothing");
		Expect(svc.ProbeMineRank(11) == svc.ProbeRankOf(11) && svc.ProbeMineRank(12) == svc.ProbeRankOf(12), "the held answer still carries each player's own line");

		// A third player: the page is held, only their own line is missing.
		svc.Request(13, 1, server, score, true, 0);
		WaitIdle(svc);
		MarkCounts("a third player on the held page", svc);
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_PAGE) == 1 && svc.Count(IA_BoardService.COUNT_FETCH_OWN) == 2, "a later player costs one own-line request, not a page");
		Expect(svc.ProbeRows(13) == 25 && svc.ProbeMineRank(13) == svc.ProbeRankOf(13), "the third player's line is theirs");
		svc.Request(13, 2, server, score, true, 0);
		Expect(Fetches(svc) == 3 && svc.ProbeMineRank(13) == svc.ProbeRankOf(13), "a player's own line is asked for once");

		// Another page of the same board: the player's own line is already held.
		svc.Request(11, 3, server, score, true, 25);
		WaitIdle(svc);
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_PAGE) == 2 && svc.Count(IA_BoardService.COUNT_FETCH_OWN) == 2, "a second page costs one request and no own line");
		Expect(svc.ProbeRows(11) == 25 && svc.ProbeMineRank(11) == svc.ProbeRankOf(11), "the second page carries the player's own line");

		// A stats batch going in changes nothing the server holds.
		fetches = Fetches(svc);
		IA_ApiHandler.GetInstance().OnSubmitStatsSuccess(null);
		svc.ProbeForget(11);
		svc.Request(11, 4, server, score, true, 0);
		svc.Request(12, 4, server, score, true, 0);
		WaitIdle(svc);
		MarkCounts("after a stats submit", svc);
		Expect(Fetches(svc) == fetches && svc.ProbeRows(11) == 25, "a stats submit does not clear what the server holds");

		// This server's board lives two minutes, the boards all servers share five.
		svc.Request(11, 5, global, score, true, 0);
		WaitIdle(svc);
		fetches = Fetches(svc);
		svc.ProbeAdvance(121000);
		svc.Request(11, 6, global, score, true, 0);
		WaitIdle(svc);
		Expect(Fetches(svc) == fetches, "the global board is still held after two minutes");
		svc.Request(11, 7, server, score, true, 0);
		WaitIdle(svc);
		Expect(Fetches(svc) == fetches + 1, "this server's board is asked for again after its two minutes");
		svc.ProbeAdvance(180000);
		svc.Request(11, 8, global, score, true, 0);
		WaitIdle(svc);
		MarkCounts("after both lifetimes", svc);
		Expect(Fetches(svc) == fetches + 2, "the global board is asked for again after its five minutes");

		// The board of servers: the own line is this server's, so every player shares that too.
		fetches = Fetches(svc);
		svc.Request(11, 9, IA_BoardProtocol.BOARD_SERVERS, score, true, 0);
		WaitIdle(svc);
		svc.Request(12, 9, IA_BoardProtocol.BOARD_SERVERS, score, true, 0);
		WaitIdle(svc);
		Expect(Fetches(svc) == fetches + 1, "the servers board is one request for every player");
		Expect(svc.ProbeMineRank(11) == 3 && svc.ProbeMineRank(12) == 3, "every player sees this server's line on the servers board");

		// A scrollbar dragged down the board: pages asked for and left at once are never requested.
		int pages = svc.Count(IA_BoardService.COUNT_FETCH_PAGE);
		for (int i = 0; i < 12; i++)
		{
			svc.Request(14, 1, global, score, true, 100 + i * 25);
		}
		WaitIdle(svc);
		MarkCounts("a scrub over twelve pages", svc);
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_PAGE) == pages + 2, "a scrub over twelve pages requests the first and the last only");
		Expect(svc.ProbeStatus(14) == ok && svc.ProbeRows(14) == 25 && svc.ProbeMineRank(14) == svc.ProbeRankOf(14), "the scrub ends on its last page");

		// The stats service fails: a page already held still goes out, a page never held does not.
		svc.ProbeAdvance(301000);
		svc.ProbeFailNext(1);
		svc.ProbeForget(11);
		svc.Request(11, 10, server, score, true, 0);
		WaitIdle(svc);
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) == 25, "a failed refresh still serves the page held");
		svc.ProbeFailNext(1);
		svc.Request(11, 10, server, score, true, 900);
		WaitIdle(svc);
		Expect(svc.ProbeStatus(11) == IA_BoardProtocol.STATUS_FAILED, "a failed request for a page never held is reported");
		MarkCounts("sharing done", svc);

		svc.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Every request is paid for: a share for each player, a burst and an hourly rate for the server.
	protected void CheckAllowance()
	{
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int score = IA_BoardProtocol.SORT_SCORE;
		int ok = IA_BoardProtocol.STATUS_OK;
		int limited = IA_BoardProtocol.STATUS_LIMITED;
		int burst = IA_ApiTunables.PlayerBurst();

		ref IA_BoardService svc = new IA_BoardService();
		svc.ProbeBegin(5000);
		svc.Open();

		// One player pages on and on.
		int i;
		for (i = 0; i < 30; i++)
		{
			svc.Request(21, 1, global, score, true, i * 25);
			WaitIdle(svc);
		}
		MarkCounts("one player, thirty pages", svc);
		Mark(string.Format("one player refused: wait=%1 s", svc.ProbeAnswerTotal(21)));
		Expect(Fetches(svc) == burst, "one player gets a burst of requests and no more");
		Expect(svc.Count(IA_BoardService.COUNT_LIMITED) == 30 - burst, "every page past the burst is refused");
		Expect(svc.ProbeStatus(21) == limited && svc.ProbeAnswerTotal(21) >= 1 && svc.ProbeAnswerTotal(21) <= 120, "the refusal says how long to wait");

		// A second player has their own share, until the server's burst is gone.
		for (i = 0; i < 30; i++)
		{
			svc.Request(22, 1, global, score, true, 1000 + i * 25);
			WaitIdle(svc);
		}
		MarkCounts("two players, sixty pages", svc);
		Expect(Fetches(svc) == IA_ApiTunables.BudgetBurst(), "two players together get the server's burst and no more");
		Expect(svc.ProbeStatus(22) == limited, "the second player is refused in turn");

		// A third player, who has asked for nothing yet, finds the server's allowance spent.
		int fetches = Fetches(svc);
		svc.Request(23, 1, global, score, true, 3000);
		WaitIdle(svc);
		Mark(string.Format("server allowance spent: wait=%1 s", svc.ProbeAnswerTotal(23)));
		Expect(svc.ProbeStatus(23) == limited && Fetches(svc) == fetches, "with the server's allowance spent a new page is refused whoever asks");
		Expect(svc.ProbeAnswerTotal(23) >= 1 && svc.ProbeAnswerTotal(23) <= 60, "the wait is the server's, not the player's");
		svc.Request(23, 2, global, score, true, 0);
		WaitIdle(svc);
		Expect(svc.ProbeStatus(23) == ok && svc.ProbeRows(23) == 25 && Fetches(svc) == fetches, "a page already held is served without a request");
		Expect(svc.ProbeMineRank(23) == 0, "and without an own line the server may not ask for, never another player's");

		// The allowance comes back at its hourly rate: a minute buys one request.
		svc.ProbeAdvance(61000);
		svc.Request(23, 3, global, score, true, 3000);
		WaitIdle(svc);
		Expect(svc.ProbeStatus(23) == ok && Fetches(svc) == fetches + 1, "a minute later one more request may go");
		svc.Request(23, 3, global, score, true, 3025);
		WaitIdle(svc);
		Expect(svc.ProbeStatus(23) == limited && Fetches(svc) == fetches + 1, "and only one");
		MarkCounts("a minute later", svc);

		// Old rows beat no rows: with nothing left to spend, a page past its lifetime still goes out.
		svc.ProbeAdvance(301000);
		svc.ProbeDrain();
		fetches = Fetches(svc);
		int shortBefore = svc.Count(IA_BoardService.COUNT_SERVED_SHORT);
		svc.ProbeForget(21);
		svc.Request(21, 2, global, score, true, 0);
		WaitIdle(svc);
		MarkCounts("over budget, old page", svc);
		Expect(svc.ProbeStatus(21) == ok && svc.ProbeRows(21) == 25 && Fetches(svc) == fetches, "over budget, a page past its lifetime is served as it is");
		Expect(svc.Count(IA_BoardService.COUNT_SERVED_SHORT) == shortBefore + 1 && svc.ProbeMineRank(21) == svc.ProbeRankOf(21), "with the own line held from before");

		svc.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! The menu itself on the service: what opening, leaving open and hammering it cost, and what
	//! it shows when the server may not ask.
	protected void CheckMenuBudget()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int me = IA_BoardService.PROBE_MENU_PLAYER;

		ref IA_BoardService svc = new IA_BoardService();
		svc.ProbeBegin(1151);
		svc.Open();
		IA_StatisticsMenu.ProbeBegin(1151, 412);
		IA_StatisticsMenu.ProbeService(svc);

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeTab(server), "server tab on the service");
		Sleep(1500);
		MarkCounts("menu opened", svc);
		Expect(svc.Count(IA_BoardService.COUNT_FETCH_PAGE) == 1 && svc.Count(IA_BoardService.COUNT_FETCH_OWN) == 0, "opening a board costs one request, the player's own line with it");
		Expect(IA_StatisticsMenu.ProbeTotal() == 1151 && IA_StatisticsMenu.ProbeRankAt(0) == 1, "the menu shows the service's page");
		Expect(IA_StatisticsMenu.ProbeMineRank() == svc.ProbeRankOf(me), "the menu shows the player's own line");
		Shot("budget_server");

		// Closed and opened again inside the lifetime. The lifetime is cut to half a minute so a
		// menu left open can be watched across one.
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(800);
		IA_ApiTunables.SetBoardLives(30, 60);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(2500);
		Expect(IA_StatisticsMenu.ProbeTab(server), "server tab reopened");
		Sleep(1500);
		MarkCounts("menu reopened", svc);
		Expect(Fetches(svc) == 1 && IA_StatisticsMenu.ProbeRankAt(0) == 1, "reopening the menu inside the lifetime makes no request");

		int asks = IA_StatisticsMenu.ProbeAsks();
		Mark(string.Format("menu left open: asks=%1, %2, lifetime=%3 s", asks, IA_StatisticsMenu.ProbeRefresh(), IA_ApiTunables.BoardLifeS(server)));
		Sleep(20000);
		Mark(string.Format("menu left open 20 s: asks=%1, %2, lifetime=%3 s", IA_StatisticsMenu.ProbeAsks(), IA_StatisticsMenu.ProbeRefresh(), IA_ApiTunables.BoardLifeS(server)));
		Expect(Fetches(svc) == 1 && IA_StatisticsMenu.ProbeAsks() == asks, "an open menu asks nothing inside the lifetime");
		Sleep(20000);
		Mark(string.Format("menu left open 40 s: asks=%1, %2, lifetime=%3 s", IA_StatisticsMenu.ProbeAsks(), IA_StatisticsMenu.ProbeRefresh(), IA_ApiTunables.BoardLifeS(server)));
		MarkCounts("menu left open across one lifetime", svc);
		Expect(Fetches(svc) == 2 && IA_StatisticsMenu.ProbeAsks() == asks + 1, "an open menu asks once when the lifetime is over, and that is one request");
		IA_ApiTunables.Reset();
		IA_ApiTunables.SetBudget(12, 20);

		// Hammered: a sort or a scroll every 150 ms.
		ref array<int> sorts = {IA_BoardProtocol.SORT_KILLS, IA_BoardProtocol.SORT_DEATHS, IA_BoardProtocol.SORT_KD};
		for (int i = 0; i < 60; i++)
		{
			if (i % 2 == 0)
				IA_StatisticsMenu.ProbeSort(sorts[(i / 2) % 3]);
			else
				IA_StatisticsMenu.ProbeScroll((i * 137) % 1100);
			Sleep(150);
		}
		int fetches = Fetches(svc);
		MarkCounts("menu hammered", svc);
		Expect(fetches >= IA_ApiTunables.PlayerBurst() && fetches <= IA_ApiTunables.PlayerBurst() + 1, "a hammering player gets their burst of requests and no more");
		Expect(svc.Count(IA_BoardService.COUNT_LIMITED) > 0, "past the burst the server refuses");

		// A view the server holds nothing of: the board says it is busy, and for how long.
		IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_INSERTIONS);
		Sleep(1200);
		Mark(string.Format("menu refused: wait=%1 s hold=%2 s", svc.ProbeAnswerTotal(me), IA_StatisticsMenu.ProbeHold()));
		Expect(svc.ProbeStatus(me) == IA_BoardProtocol.STATUS_LIMITED && IA_StatisticsMenu.ProbeWaiting(), "the menu takes the refusal as a wait");
		Expect(IA_StatisticsMenu.ProbeTotal() < 0 && IA_StatisticsMenu.ProbeHold() > 5, "the menu waits as long as the server asks, with the notice in place of rows");
		Shot("board_busy");

		asks = IA_StatisticsMenu.ProbeAsks();
		fetches = Fetches(svc);
		Sleep(5000);
		Expect(IA_StatisticsMenu.ProbeAsks() == asks && Fetches(svc) == fetches, "the menu does not ask again inside the wait");

		// An hour on the server's clock fills the allowance; the board in its first order comes back.
		svc.ProbeAdvance(3600000);
		IA_StatisticsMenu.ProbeSort(IA_BoardProtocol.SORT_SCORE);
		IA_StatisticsMenu.ProbeTool(1);
		Sleep(2000);
		Expect(IA_StatisticsMenu.ProbeRankAt(0) == 1 && !IA_StatisticsMenu.ProbeWaiting(), "with the allowance back the board loads again");

		// Rows on screen stay while the menu waits; the wait goes under the status chip. With a
		// quicker allowance the wait is short enough to watch it end. Another player's request
		// first leaves the server holding a page this menu has never shown.
		IA_ApiTunables.SetBudget(600, 20);
		svc.Request(77, 1, server, IA_BoardProtocol.SORT_SCORE, true, 500);
		WaitIdle(svc);
		svc.ProbeDrain();
		IA_StatisticsMenu.ProbeScroll(912);
		Sleep(1500);
		Mark(string.Format("menu refused with rows: wait=%1 s hold=%2 s", svc.ProbeAnswerTotal(me), IA_StatisticsMenu.ProbeHold()));
		Expect(IA_StatisticsMenu.ProbeWaiting() && IA_StatisticsMenu.ProbeRankAt(0) == 1, "rows already shown stay while the menu waits");
		Expect(IA_StatisticsMenu.ProbeRankAt(912) == 0, "the page the server may not ask for is not there yet");
		Shot("board_busy_rows");

		// The wait is for the page that was refused. A scroll to a page the server holds is answered at once.
		fetches = Fetches(svc);
		IA_StatisticsMenu.ProbeScroll(512);
		Sleep(1200);
		Mark(string.Format("menu scrolled during the wait: row 512 holds place %1, hold=%2 s", IA_StatisticsMenu.ProbeRankAt(512), IA_StatisticsMenu.ProbeHold()));
		Expect(IA_StatisticsMenu.ProbeRankAt(512) == 513 && Fetches(svc) == fetches, "a page the server holds is shown during the wait, with no request");
		Expect(IA_StatisticsMenu.ProbeWaiting(), "and the wait for the refused page goes on");
		IA_StatisticsMenu.ProbeScroll(912);
		Sleep(300);

		asks = IA_StatisticsMenu.ProbeAsks();
		fetches = Fetches(svc);
		Sleep(4000);
		Expect(IA_StatisticsMenu.ProbeAsks() == asks && Fetches(svc) == fetches, "nothing is asked while the short wait runs");
		Sleep(10000);
		MarkCounts("menu after the wait", svc);
		Expect(IA_StatisticsMenu.ProbeRankAt(912) == 913 && !IA_StatisticsMenu.ProbeWaiting(), "when the wait is over the page arrives");
		Expect(IA_StatisticsMenu.ProbeAsks() == asks + 1 && Fetches(svc) == fetches + 1, "for one ask and one request");
		Shot("board_after_wait");

		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.IA_StatisticsMenu);
		Sleep(800);
		IA_StatisticsMenu.ProbeService(null);
		IA_StatisticsMenu.ProbeEnd();
		svc.Close();
		IA_ApiTunables.Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! The allowance as it stands with nothing changed: 120 requests an hour for the server
	//! after 200 at once, and half of each for one player. The allowance fills on the real clock
	//! too, so the waits are checked loosely.
	protected void CheckDefaultAllowance()
	{
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int kills = IA_BoardProtocol.SORT_KILLS;
		int limited = IA_BoardProtocol.STATUS_LIMITED;

		IA_ApiTunables.Reset();
		ref IA_BoardService svc = new IA_BoardService();
		svc.ProbeBegin(10000);
		svc.Open();

		int i;
		for (i = 0; i < 130; i++)
		{
			svc.Request(31, 1, global, kills, true, i * 25);
			WaitIdle(svc);
		}
		Mark(string.Format("default allowance: one player sent=%1 wait=%2 s", Fetches(svc), svc.ProbeAnswerTotal(31)));
		Expect(Fetches(svc) == 100 && svc.ProbeStatus(31) == limited, "by default one player gets a hundred requests at once");
		Expect(svc.ProbeAnswerTotal(31) > 30 && svc.ProbeAnswerTotal(31) <= 60, "and then one a minute");

		for (i = 0; i < 130; i++)
		{
			svc.Request(32, 1, global, kills, true, 4000 + i * 25);
			WaitIdle(svc);
		}
		svc.Request(33, 1, global, kills, true, 9000);
		WaitIdle(svc);
		Mark(string.Format("default allowance: server sent=%1 wait=%2 s", Fetches(svc), svc.ProbeAnswerTotal(33)));
		Expect(Fetches(svc) == 200 && svc.ProbeStatus(33) == limited, "by default the server sends two hundred requests at once and no more");
		Expect(svc.ProbeAnswerTotal(33) >= 1 && svc.ProbeAnswerTotal(33) <= 30, "and then one every half minute");

		svc.ProbeAdvance(30500);
		svc.Request(33, 1, global, kills, true, 9000);
		WaitIdle(svc);
		svc.Request(33, 1, global, kills, true, 9025);
		WaitIdle(svc);
		Expect(Fetches(svc) == 201 && svc.ProbeStatus(33) == limited, "half a minute buys one request");
		svc.Close();

		// One player asking for a new page every five seconds, for an hour of the server's clock.
		ref IA_BoardService hour = new IA_BoardService();
		hour.ProbeBegin(5000);
		hour.Open();
		for (i = 0; i < 720; i++)
		{
			hour.Request(34, 1, global, kills, true, (i % 150) * 25);
			WaitIdle(hour);
			hour.ProbeAdvance(5000);
		}
		MarkCounts("default allowance, one player asking every five seconds for an hour", hour);
		Expect(Fetches(hour) >= 155 && Fetches(hour) <= 162, "one player hammering for an hour causes their hundred requests and sixty more");
		hour.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! The timed exchange with every request held back and counted and every answer made up:
	//! when it goes, what it carries, what a lost answer costs, and what a stats service without
	//! it is sent instead. Nothing here reaches the real stats service.
	protected void ProbeSync()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		int callsBefore = api.GetCallTotal();

		IA_ApiTunables.Reset();
		IA_ApiSync.GetInstance().ProbeManual(true);
		api.ProbeCapture(true);

		CheckSyncQuiet();
		CheckSyncBatches();
		CheckSyncRatings();
		CheckSyncFallback();
		CheckSyncBoards();
		CheckSyncHints();

		SyncFresh();
		api.ProbeCapture(false);
		IA_ApiTunables.Reset();

		int calls = api.GetCallTotal() - callsBefore;
		Mark(string.Format("exchange probe: requests sent to the stats service=%1 (%2)", calls, api.CallSummary()));
		Expect(calls == 0, "the exchange probe sent nothing to the stats service");
	}

	//------------------------------------------------------------------------------------------------
	//! Nothing waiting, nothing known of the stats service.
	protected void SyncFresh()
	{
		IA_StatsManager.GetInstance().ProbeClear();
		IA_TransportPilotStore.GetInstance().ProbeClear();
		IA_ApiSync.GetInstance().ProbeReset();
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueKills(string playerId, string name, int count)
	{
		IA_StatsManager stats = IA_StatsManager.GetInstance();
		for (int i = 0; i < count; i++)
		{
			stats.QueuePlayerKill(playerId, name);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool Has(string text, string part)
	{
		return text.IndexOf(part) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A /sync answer holding \p parts, which are members without the braces.
	protected string SyncBody(string parts)
	{
		if (parts.IsEmpty())
			return "{\"status\":\"ok\"}";
		return "{\"status\":\"ok\"," + parts + "}";
	}

	//------------------------------------------------------------------------------------------------
	//! A JSON list of made-up rows, best first.
	protected string SyncRows(int count)
	{
		string rows = "[";
		for (int i = 1; i <= count; i++)
		{
			if (i > 1)
				rows = rows + ",";
			rows = rows + SyncRow(i, "Sync " + i.ToString(), "");
		}
		return rows + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! \param extra further members, led by a comma
	protected string SyncRow(int rank, string name, string extra)
	{
		int left = 2000 - rank;
		string row = string.Format("{\"r\":%1,\"n\":\"%2\",\"k\":%3,\"d\":%4,\"h\":0,\"g\":0,", rank, name, left * 3, left);
		return row + string.Format("\"o\":%1,\"s\":%2,\"t\":0,\"i\":0,\"p\":0%3}", left * 2, left * 40, extra);
	}

	//------------------------------------------------------------------------------------------------
	//! An own line as the exchange brings it.
	protected string SyncOwn(int rank, string id, string board)
	{
		return SyncRow(rank, "Sync Own", ",\"id\":\"" + id + "\",\"board\":\"" + board + "\"");
	}

	//------------------------------------------------------------------------------------------------
	//! With nothing to send and nobody looking, the timer turns and nothing goes.
	protected void CheckSyncQuiet()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		api.ProbeCapture(true);

		for (int minute = 0; minute < 60; minute++)
		{
			sync.ProbeAdvance(60000);
			sync.ProbeTick();
		}
		Mark(string.Format("exchange [an hour with nothing to send] sync=%1 submitStats=%2 submitTransport=%3", api.ProbeCaptured(IA_ApiHandler.ROUTE_SYNC), api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_STATS), api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT)));
		Expect(api.ProbeCaptured(IA_ApiHandler.ROUTE_SYNC) == 0 && !sync.ProbeInFlight(), "with nothing waiting and nobody looking no exchange is sent");
		Expect(api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_STATS) == 0 && api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT) == 0, "and nothing goes on the separate routes");
	}

	//------------------------------------------------------------------------------------------------
	//! A statistics batch goes under an id and is kept until the stats service says it has it.
	protected void CheckSyncBatches()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_StatsManager stats = IA_StatsManager.GetInstance();
		int routeSync = IA_ApiHandler.ROUTE_SYNC;
		SyncFresh();
		api.ProbeCapture(true);

		QueueKills("probe-a", "Probe A", 3);
		sync.ProbeTick();
		string id = stats.ProbeBatchId();
		string members = sync.ProbeMembers();
		Expect(api.ProbeCaptured(routeSync) == 1 && sync.ProbeInFlight() && stats.ProbeWaiting() == 0, "events waiting go out with the next turn of the timer");
		Expect(!id.IsEmpty() && Has(members, "\"statsBatchId\":\"" + id + "\"") && Has(members, "\"matchData\":["), "the exchange carries them as a batch under an id");
		Expect(!Has(members, "\"boards\"") && !Has(members, "\"players\"") && !Has(members, "\"transport\"") && !Has(members, "\"ratingIds\""), "and carries nothing nobody asked for");

		// One exchange at a time.
		QueueKills("probe-a", "Probe A", 2);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 1, "no second exchange goes while one is out");

		// The answer never comes. The batch stays as it was and goes again after a wait.
		sync.OnFailed(0, false, true);
		Expect(sync.ProbeFailures() == 1 && stats.ProbeBatchId() == id, "a lost answer keeps the batch");
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 1, "the next attempt waits");
		sync.ProbeAdvance(61000);
		sync.ProbeTick();
		members = sync.ProbeMembers();
		Expect(api.ProbeCaptured(routeSync) == 2 && Has(members, "\"statsBatchId\":\"" + id + "\""), "the same batch goes again under the same id");
		Expect(stats.ProbeWaiting() == 2, "events queued since then wait for a batch of their own");

		// It was stored by the attempt whose answer was lost.
		sync.OnAnswer(SyncBody("\"statsStatus\":\"duplicate\",\"statsPlayers\":1"));
		Expect(stats.ProbeBatchId().IsEmpty() && sync.ProbeFailures() == 0 && sync.GetMode() == IA_ApiSync.MODE_SYNC, "a batch the stats service already has is forgotten");

		// The next batch: not stored, then refused.
		sync.ProbeTick();
		string second = stats.ProbeBatchId();
		Expect(api.ProbeCaptured(routeSync) == 3 && !second.IsEmpty() && second != id, "the next batch has an id of its own");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"failed\",\"statsPlayers\":0"));
		Expect(stats.ProbeBatchId() == second, "a batch the stats service could not store is kept");
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 4 && Has(sync.ProbeMembers(), second), "and goes again with the next turn");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"rejected\",\"statsPlayers\":0"));
		Expect(stats.ProbeBatchId().IsEmpty(), "a batch the stats service refuses is dropped, not sent for ever");
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 4, "after which nothing is left to send");

		// An error and an answer that cannot be read keep the batch too, and the wait grows.
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		string third = stats.ProbeBatchId();
		sync.OnFailed(500, false, false);
		Expect(stats.ProbeBatchId() == third && sync.ProbeFailures() == 1, "an error from the stats service keeps the batch");
		sync.ProbeAdvance(61000);
		sync.ProbeTick();
		sync.OnAnswer("<html>not an answer</html>");
		Expect(api.ProbeCaptured(routeSync) == 6 && stats.ProbeBatchId() == third && sync.ProbeFailures() == 2, "an answer that cannot be read keeps the batch");
		sync.ProbeAdvance(61000);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 6, "a second failure in a row waits twice as long");
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 7 && Has(sync.ProbeMembers(), third), "and then the batch goes again");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"accepted\",\"statsPlayers\":1"));
		Expect(stats.ProbeBatchId().IsEmpty() && sync.ProbeFailures() == 0, "a stored batch is forgotten");
		Mark(string.Format("exchange [batches] sync=%1 submitStats=%2", api.ProbeCaptured(routeSync), api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_STATS)));
		Expect(api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_STATS) == 0, "none of it used the separate statistics route");
	}

	//------------------------------------------------------------------------------------------------
	//! Transport points and rating lookups travel in the exchange too.
	protected void CheckSyncRatings()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		int routeSync = IA_ApiHandler.ROUTE_SYNC;
		SyncFresh();
		api.ProbeCapture(true);

		store.AddInsertion("probe-pilot", "Probe Pilot", 20);
		store.RequestRating("probe-pilot", "Probe Pilot", System.GetTickCount());
		sync.ProbeTick();
		string id = store.ProbeBatchId();
		string members = sync.ProbeMembers();
		Expect(api.ProbeCaptured(routeSync) == 1 && !id.IsEmpty() && Has(members, "\"transport\":{\"batchId\":\"" + id + "\""), "transport points go in the exchange under their batch id");
		Expect(Has(members, "\"ratingIds\":[\"probe-pilot\"]") && store.ProbeWanted() == 0, "and so does the rating a player waits on");

		string answer = "\"transportStatus\":\"accepted\",\"transportPlayers\":1,\"ratingsStatus\":\"ok\",";
		answer = answer + "\"ratings\":[{\"rating\":320,\"playerId\":\"probe-pilot\",\"insertions\":9}],\"skins\":[]";
		sync.OnAnswer(SyncBody(answer));
		Expect(store.ProbeBatchId().IsEmpty() && store.GetRating("probe-pilot") == 320, "the answer settles the batch and brings the rating");

		// A lost answer: the same batch goes again, and its points are counted once.
		store.AddInsertion("probe-pilot", "Probe Pilot", 15);
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		id = store.ProbeBatchId();
		sync.OnFailed(0, false, true);
		Expect(!id.IsEmpty() && store.ProbeBatchId() == id && store.GetRating("probe-pilot") == 335, "a lost answer keeps the transport batch");
		sync.ProbeAdvance(61000);
		sync.ProbeTick();
		Expect(Has(sync.ProbeMembers(), "\"batchId\":\"" + id + "\""), "which goes again under the same id");
		sync.OnAnswer(SyncBody("\"transportStatus\":\"duplicate\",\"transportPlayers\":0"));
		Expect(store.ProbeBatchId().IsEmpty() && store.GetRating("probe-pilot") == 335, "and is forgotten once the stats service has it, its points counted once");

		// A rating a player waits on does not wait for the timer, but exchanges keep their distance.
		int sent = api.ProbeCaptured(routeSync);
		store.RequestRating("probe-late", "Probe Late", System.GetTickCount());
		store.SendQueuedRequests();
		Expect(api.ProbeCaptured(routeSync) == sent, "an exchange does not follow another at once");
		sync.ProbeAdvance(IA_ApiTunables.SYNC_HURRY_GAP_S * 1000 + 1000);
		store.SendQueuedRequests();
		Expect(api.ProbeCaptured(routeSync) == sent + 1 && Has(sync.ProbeMembers(), "\"ratingIds\":[\"probe-late\"]"), "a rating a player waits on sends the exchange early");
		sync.OnAnswer(SyncBody("\"ratingsStatus\":\"ok\",\"ratings\":[],\"skins\":[]"));
		Expect(store.GetRating("probe-late") == 0, "a player the stats service does not know has a rating of nothing, not an unknown one");

		Mark(string.Format("exchange [transport and ratings] sync=%1 submitTransport=%2 getTransportRatings=%3", api.ProbeCaptured(routeSync), api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT), api.ProbeCaptured(IA_ApiHandler.ROUTE_TRANSPORT_RATINGS)));
		Expect(api.ProbeCaptured(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT) == 0 && api.ProbeCaptured(IA_ApiHandler.ROUTE_TRANSPORT_RATINGS) == 0, "none of it used the separate transport routes");
	}

	//------------------------------------------------------------------------------------------------
	//! A stats service without the exchange gets the separate routes, and is asked again rarely.
	protected void CheckSyncFallback()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_StatsManager stats = IA_StatsManager.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		int routeSync = IA_ApiHandler.ROUTE_SYNC;
		int routeStats = IA_ApiHandler.ROUTE_SUBMIT_STATS;
		int routeTransport = IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT;
		int routeRatings = IA_ApiHandler.ROUTE_TRANSPORT_RATINGS;
		SyncFresh();
		api.ProbeCapture(true);

		QueueKills("probe-a", "Probe A", 2);
		store.AddInsertion("probe-pilot", "Probe Pilot", 20);
		store.RequestRating("probe-pilot", "Probe Pilot", System.GetTickCount());
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 1 && api.ProbeCaptured(routeStats) == 0, "a stats service not yet known is tried with the exchange");

		// 404: a stats service from before the route existed.
		sync.OnFailed(404, true, false);
		Expect(sync.GetMode() == IA_ApiSync.MODE_SEPARATE && !sync.UsesSync() && sync.ProbeFailures() == 0, "a stats service without the exchange is not a failure");
		Expect(api.ProbeCaptured(routeStats) == 1 && api.ProbeCaptured(routeTransport) == 1, "what the exchange carried goes on the separate routes at once");
		Expect(stats.ProbeBatchId().IsEmpty() && store.ProbeWanted() == 1, "statistics go once as before, and the rating waits for its own route");
		store.OnSubmitResult(true);
		store.SendQueuedRequests();
		Expect(api.ProbeCaptured(routeRatings) == 1 && api.ProbeCaptured(routeSync) == 1, "the rating is asked for on its own route");
		store.OnRatingsReceived("{\"ratings\":[{\"rating\":20,\"playerId\":\"probe-pilot\",\"insertions\":1}],\"skins\":[]}");
		Expect(store.GetRating("probe-pilot") == 20, "and arrives");

		// From here every turn of the timer is the separate routes, as before there was an exchange.
		for (int minute = 0; minute < 30; minute++)
		{
			QueueKills("probe-a", "Probe A", 1);
			sync.ProbeAdvance(60000);
			sync.ProbeTick();
		}
		string counts = string.Format("sync=%1 submitStats=%2 submitTransport=%3 getTransportRatings=%4", api.ProbeCaptured(routeSync), api.ProbeCaptured(routeStats), api.ProbeCaptured(routeTransport), api.ProbeCaptured(routeRatings));
		Mark("exchange [half an hour on a stats service without it] " + counts);
		Expect(api.ProbeCaptured(routeSync) == 1 && api.ProbeCaptured(routeStats) == 31, "without the exchange each turn sends statistics on their own route and does not try the exchange");

		// It is tried again once an hour has passed, and only with something to send.
		sync.ProbeAdvance(1800000);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 1, "with nothing to send the exchange is not tried again");
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 2 && api.ProbeCaptured(routeStats) == 31, "after an hour the exchange is tried once more");

		// 501: a stats service that has the route over a database that cannot serve it.
		sync.OnFailed(501, true, false);
		Expect(sync.GetMode() == IA_ApiSync.MODE_SEPARATE && api.ProbeCaptured(routeStats) == 32 && stats.ProbeBatchId().IsEmpty(), "still without it, the separate route takes the batch");

		sync.ProbeAdvance(3600000);
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 3 && !stats.ProbeBatchId().IsEmpty(), "and an hour on it is tried again");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"accepted\",\"statsPlayers\":1"));
		Expect(sync.GetMode() == IA_ApiSync.MODE_SYNC && sync.UsesSync() && stats.ProbeBatchId().IsEmpty(), "a stats service that has gained the exchange is used through it from then on");
		sync.ProbeAdvance(60000);
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == 4 && api.ProbeCaptured(routeStats) == 32, "with no more use of the separate routes");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"accepted\",\"statsPlayers\":1"));
	}

	//------------------------------------------------------------------------------------------------
	//! While somebody looks at a board, the exchange keeps its first rows and the lookers' own
	//! lines in hand, and the menu's opening view costs no request.
	protected void CheckSyncBoards()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int servers = IA_BoardProtocol.BOARD_SERVERS;
		int score = IA_BoardProtocol.SORT_SCORE;
		int ok = IA_BoardProtocol.STATUS_OK;
		int limited = IA_BoardProtocol.STATUS_LIMITED;
		int routeSync = IA_ApiHandler.ROUTE_SYNC;
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		api.ProbeCapture(true);

		ref IA_BoardService svc = new IA_BoardService();
		svc.ProbeBegin(1151);
		svc.Open();

		// Nobody has looked at a board, so none is asked for.
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		string members = sync.ProbeMembers();
		Expect(api.ProbeCaptured(routeSync) == 1 && !Has(members, "\"boards\"") && !Has(members, "\"players\""), "with nobody looking the exchange asks for no board and no own line");
		sync.OnAnswer(SyncBody("\"statsStatus\":\"accepted\",\"statsPlayers\":1"));

		// A player opens this server's board. The first look is a request of its own; from then
		// on the exchange keeps the board and the player's line in hand.
		svc.Request(11, 1, server, score, true, 0);
		WaitIdle(svc);
		Expect(Fetches(svc) == 1 && svc.ProbeStatus(11) == ok, "the first look at a board is one request");
		sync.ProbeAdvance(60000);
		svc.ProbeAdvance(60000);
		sync.ProbeTick();
		members = sync.ProbeMembers();
		Expect(Has(members, "\"players\":[\"probe-11\"]"), "while a player looks the exchange asks for that player's line");
		Expect(Has(members, "\"boards\":[{\"board\":\"server\",\"etag\":\"\"}]"), "and for the board looked at, and no other");

		string head = "\"boardsStatus\":\"ok\",\"boards\":[{\"board\":\"server\",\"etag\":\"e1\",\"total\":1200,\"unchanged\":0}],";
		string answer = head + "\"serverRows\":" + SyncRows(100) + ",\"globalRows\":[],\"serversRows\":[],";
		answer = answer + "\"own\":[" + SyncOwn(400, "probe-11", "server") + "],\"snapshotRows\":100";
		sync.OnAnswer(SyncBody(answer));
		Expect(svc.ProbeEtag(server) == "e1" && svc.ProbeEtag(global).IsEmpty(), "the rows that came with the exchange are held under their tag");

		int fetches = Fetches(svc);
		bool served = true;
		int offset;
		for (offset = 0; offset < 100; offset = offset + 25)
		{
			svc.ProbeForget(11);
			svc.Request(11, 2, server, score, true, offset);
			if (svc.ProbeStatus(11) != ok || svc.ProbeRows(11) != 25 || svc.ProbeAnswerTotal(11) != 1200 || svc.ProbeMineRank(11) != 400)
				served = false;
		}
		Expect(served && Fetches(svc) == fetches, "the first hundred rows and the player's line are served at once from what the exchange brought, for no request");
		Expect(svc.ProbeMineName(11) == "Sync Own", "the own line is the one the exchange brought");

		// Rows further down, and another order, are asked for as before.
		svc.Request(11, 2, server, score, true, 100);
		WaitIdle(svc);
		Expect(Fetches(svc) == fetches + 1 && svc.ProbeRows(11) == 25, "rows past the first hundred are still a request");
		svc.Request(11, 3, server, IA_BoardProtocol.SORT_KILLS, true, 0);
		WaitIdle(svc);
		Expect(Fetches(svc) == fetches + 2, "and so is another order");

		// The board stays in hand for as long as somebody keeps looking, and costs nothing more.
		string same = "\"boardsStatus\":\"ok\",\"boards\":[{\"board\":\"server\",\"etag\":\"e1\",\"total\":1200,\"unchanged\":1}],";
		same = same + "\"serverRows\":[],\"own\":[" + SyncOwn(400, "probe-11", "server") + "]";
		fetches = Fetches(svc);
		int sent = api.ProbeCaptured(routeSync);
		bool tagged = true;
		served = true;
		for (int minute = 0; minute < 10; minute++)
		{
			sync.ProbeAdvance(60000);
			svc.ProbeAdvance(60000);
			svc.ProbeForget(11);
			svc.Request(11, 2, server, score, true, 0);
			if (svc.ProbeStatus(11) != ok || svc.ProbeRows(11) != 25 || svc.ProbeMineRank(11) != 400)
				served = false;
			sync.ProbeTick();
			if (!Has(sync.ProbeMembers(), "{\"board\":\"server\",\"etag\":\"e1\"}"))
				tagged = false;
			sync.OnAnswer(SyncBody(same));
		}
		MarkCounts("ten minutes of looking at a board the exchange holds", svc);
		Expect(served && Fetches(svc) == fetches, "ten minutes of looking cost no request");
		Expect(tagged && api.ProbeCaptured(routeSync) == sent + 10, "for one exchange a minute, each naming the tag of the rows held");

		// An unchanged board still brings its total and the player's line up to date.
		string moved = "\"boardsStatus\":\"ok\",\"boards\":[{\"board\":\"server\",\"etag\":\"e1\",\"total\":1203,\"unchanged\":1}],";
		moved = moved + "\"serverRows\":[],\"own\":[" + SyncOwn(401, "probe-11", "server") + "]";
		sync.ProbeAdvance(60000);
		svc.ProbeAdvance(60000);
		sync.ProbeTick();
		sync.OnAnswer(SyncBody(moved));
		svc.ProbeForget(11);
		svc.Request(11, 2, server, score, true, 0);
		Expect(svc.ProbeAnswerTotal(11) == 1203 && svc.ProbeMineRank(11) == 401 && svc.ProbeEtag(server) == "e1", "an unchanged board still brings its total and the player's line up to date");

		// A second player on the same board. The exchange names both, and one with no line is told so.
		svc.Request(12, 1, server, score, true, 0);
		WaitIdle(svc);
		sync.ProbeAdvance(60000);
		svc.ProbeAdvance(60000);
		sync.ProbeTick();
		members = sync.ProbeMembers();
		Expect(Has(members, "\"probe-11\"") && Has(members, "\"probe-12\""), "every player looking is named in the exchange");
		sync.OnAnswer(SyncBody(moved));
		fetches = Fetches(svc);
		svc.ProbeForget(12);
		svc.Request(12, 2, server, score, true, 0);
		Expect(svc.ProbeStatus(12) == ok && svc.ProbeRows(12) == 25 && svc.ProbeMineRank(12) == 0 && Fetches(svc) == fetches, "a player with no line on the board gets the rows and no line, never another player's");

		// Nobody looks any more.
		sync.ProbeAdvance(121000);
		svc.ProbeAdvance(121000);
		sent = api.ProbeCaptured(routeSync);
		sync.ProbeTick();
		Expect(api.ProbeCaptured(routeSync) == sent && !sync.ProbeInFlight(), "once nobody has looked for a board's lifetime it is no longer asked for, and with nothing else to send no exchange goes");

		// The board of servers: short, and its own line is this server's.
		svc.Request(11, 4, servers, score, true, 0);
		WaitIdle(svc);
		sync.ProbeAdvance(60000);
		svc.ProbeAdvance(60000);
		sync.ProbeTick();
		members = sync.ProbeMembers();
		Expect(Has(members, "\"boards\":[{\"board\":\"servers\",\"etag\":\"\"}]") && !Has(members, "\"players\""), "only the board looked at is asked for, and no player for a board that has none");
		answer = "\"boardsStatus\":\"ok\",\"boards\":[{\"board\":\"servers\",\"etag\":\"s1\",\"total\":3,\"unchanged\":0}],";
		answer = answer + "\"serversRows\":" + SyncRows(3) + ",\"own\":[" + SyncOwn(2, "", "servers") + "]";
		sync.OnAnswer(SyncBody(answer));
		fetches = Fetches(svc);
		svc.Request(12, 3, servers, score, true, 0);
		Expect(svc.ProbeStatus(12) == ok && svc.ProbeRows(12) == 3 && svc.ProbeAnswerTotal(12) == 3 && svc.ProbeMineRank(12) == 2 && Fetches(svc) == fetches, "a short board and this server's line on it are served whole from the exchange");

		// The stats service could not read the boards: what is held stays.
		sync.ProbeAdvance(60000);
		svc.ProbeAdvance(60000);
		sent = api.ProbeCaptured(routeSync);
		sync.ProbeTick();
		sync.OnAnswer(SyncBody("\"boardsStatus\":\"failed\",\"boards\":[],\"own\":[]"));
		Expect(api.ProbeCaptured(routeSync) == sent + 1 && svc.ProbeEtag(servers) == "s1", "when the stats service cannot read the boards the rows held are kept");

		// With the allowance spent, a board the exchange will bring is waited for only until then.
		// The allowance here is a slow one, so its wait is plainly longer than an exchange away.
		IA_ApiTunables.SetBudget(12, 20);
		svc.ProbeDrain();
		svc.Request(13, 1, global, score, true, 0);
		WaitIdle(svc);
		int soon = svc.ProbeAnswerTotal(13);
		Expect(svc.ProbeStatus(13) == limited && soon >= 1 && soon <= IA_ApiTunables.SyncIntervalS() + 3, "with the allowance spent the opening view waits for the next exchange, not for the allowance");
		svc.Request(13, 2, global, IA_BoardProtocol.SORT_KILLS, true, 0);
		WaitIdle(svc);
		Mark(string.Format("exchange: refused wait for the opening view=%1 s, for another order=%2 s", soon, svc.ProbeAnswerTotal(13)));
		Expect(svc.ProbeStatus(13) == limited && svc.ProbeAnswerTotal(13) > IA_ApiTunables.SyncIntervalS() + 3, "another order waits for the allowance");
		IA_ApiTunables.SetBudget(IA_ApiTunables.BUDGET_PER_HOUR, IA_ApiTunables.BUDGET_BURST);

		svc.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! What the stats service suggests is taken, each value inside the range this server allows.
	protected void CheckSyncHints()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		api.ProbeCapture(true);
		IA_ApiTunables.Reset();

		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeTick();
		string hints = "\"statsStatus\":\"accepted\",\"nextSyncSeconds\":5,\"pageBudgetPerHour\":100000,\"pageBudgetBurst\":0,";
		sync.OnAnswer(SyncBody(hints + "\"serverPageSeconds\":45,\"globalPageSeconds\":0"));
		Expect(IA_ApiTunables.SyncIntervalS() == IA_ApiTunables.SYNC_INTERVAL_MIN_S, "a suggested interval below the range is brought up to it");
		Expect(IA_ApiTunables.BudgetPerHour() == IA_ApiTunables.BUDGET_PER_HOUR_MAX && IA_ApiTunables.BudgetBurst() == IA_ApiTunables.BUDGET_BURST, "a suggested allowance above the range is brought down to it, and zero changes nothing");
		Expect(IA_ApiTunables.BoardLifeS(server) == 45 && IA_ApiTunables.BoardLifeS(global) == IA_ApiTunables.LIFE_WIDE_S, "a suggested lifetime inside the range is taken, and zero changes nothing");

		// What the stats service sends today.
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		hints = "\"statsStatus\":\"accepted\",\"nextSyncSeconds\":60,\"pageBudgetPerHour\":120,\"pageBudgetBurst\":200,";
		sync.OnAnswer(SyncBody(hints + "\"serverPageSeconds\":120,\"globalPageSeconds\":300"));
		Expect(IA_ApiTunables.SyncIntervalS() == 60 && IA_ApiTunables.BudgetPerHour() == 120 && IA_ApiTunables.BudgetBurst() == 200, "today's suggestions are this server's defaults");
		Expect(IA_ApiTunables.BoardLifeS(server) == 120 && IA_ApiTunables.BoardLifeS(global) == 300, "for the lifetimes too");

		// An answer without suggestions changes nothing.
		IA_ApiTunables.SetBudget(30, 15);
		QueueKills("probe-a", "Probe A", 1);
		sync.ProbeAdvance(60000);
		sync.ProbeTick();
		sync.OnAnswer(SyncBody("\"statsStatus\":\"accepted\""));
		Expect(IA_ApiTunables.BudgetPerHour() == 30 && IA_ApiTunables.BudgetBurst() == 15, "an answer with no suggestions leaves what is set");
		IA_ApiTunables.Reset();
		Mark("exchange hints checked");
	}

	//------------------------------------------------------------------------------------------------
	//! The timed exchange against the local development server: the real requests and answers,
	//! with the timer turned by hand and both clocks moved on a minute at a time.
	protected void ProbeDev()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		if (!api.ProbeBaseOverridden())
		{
			Fail("the development-server part needs -iaApiBase, and never runs against the real stats service");
			return;
		}

		string guid = DEV_SERVER;
		string guidArg;
		if (System.GetCLIParam("iaUplinkServer", guidArg) && !guidArg.IsEmpty())
			guid = guidArg;
		if (!api.ProbeLink(guid))
		{
			Fail("could not stand as a server of the development stats service");
			return;
		}

		IA_ApiTunables.Reset();
		IA_ApiSync.GetInstance().ProbeManual(true);
		api.ProbeCapture(false);
		api.ProbeSyncRoute("");
		CallsSince();

		DevExchange();
		DevResend();
		DevFallback();
		DevHourIdle();
		DevHourPlay();
		DevHourLooks(false);
		DevHourLooks(true);
		DevHourHammer();
		DevLargest();

		SyncFresh();
		if (m_DevBoards)
			m_DevBoards.Close();
		m_DevBoards = null;
		IA_ApiTunables.Reset();
		Mark("dev: requests sent in all: " + api.CallSummary());
	}

	//------------------------------------------------------------------------------------------------
	//! The identity of a seeded player of the development server, 1 to 9.
	protected string DevPlayer(int number)
	{
		return DEV_PLAYER + number.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! A board service of its own for a scenario: nothing held, the allowance whole. Players 11
	//! to 14 go by seeded identities, and answers are noted instead of sent to players.
	protected void DevBoards()
	{
		if (m_DevBoards)
			m_DevBoards.Close();
		m_DevBoards = new IA_BoardService();
		m_DevBoards.ProbeNote();
		for (int i = 1; i <= 4; i++)
		{
			m_DevBoards.ProbeIdentity(10 + i, DevPlayer(i));
		}
		m_DevBoards.Open();
	}

	//------------------------------------------------------------------------------------------------
	//! Move the exchange's clock and the board service's on together.
	protected void DevStep(int ms)
	{
		IA_ApiSync.GetInstance().ProbeAdvance(ms);
		if (m_DevBoards)
			m_DevBoards.ProbeAdvance(ms);
	}

	//------------------------------------------------------------------------------------------------
	//! Wait for the answer to the exchange now out.
	protected bool WaitSync()
	{
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		int waited;
		while (sync.ProbeInFlight() && waited < SYNC_ANSWER_TIMEOUT_MS)
		{
			Sleep(20);
			waited = waited + 20;
		}
		if (sync.ProbeInFlight())
		{
			Fail("the exchange got no answer from the development server");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Requests sent on each route since this was last called, as one line. Delta() reads them.
	protected string CallsSince()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		while (m_aCallBase.Count() < IA_ApiHandler.ROUTE_COUNT)
		{
			m_aCallBase.Insert(0);
			m_aCallDelta.Insert(0);
		}
		for (int route = 0; route < IA_ApiHandler.ROUTE_COUNT; route++)
		{
			int sent = api.GetCallCount(route);
			m_aCallDelta[route] = sent - m_aCallBase[route];
			m_aCallBase[route] = sent;
		}
		string line = string.Format("sync=%1 leaderboard=%2 submitStats=%3 ", Delta(IA_ApiHandler.ROUTE_SYNC), Delta(IA_ApiHandler.ROUTE_LEADERBOARD), Delta(IA_ApiHandler.ROUTE_SUBMIT_STATS));
		line = line + string.Format("submitTransport=%1 getTransportRatings=%2 ", Delta(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT), Delta(IA_ApiHandler.ROUTE_TRANSPORT_RATINGS));
		return line + string.Format("registerServer=%1 total=%2", Delta(IA_ApiHandler.ROUTE_REGISTER), DeltaTotal());
	}

	//------------------------------------------------------------------------------------------------
	protected int Delta(int route)
	{
		if (route < 0 || route >= m_aCallDelta.Count())
			return 0;
		return m_aCallDelta[route];
	}

	//------------------------------------------------------------------------------------------------
	protected int DeltaTotal()
	{
		int total;
		foreach (int sent : m_aCallDelta)
		{
			total = total + sent;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! One request carries statistics, transport points and a rating lookup; the next brings the
	//! boards somebody looks at, and they are then served with no request at all.
	protected void DevExchange()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int servers = IA_BoardProtocol.BOARD_SERVERS;
		int score = IA_BoardProtocol.SORT_SCORE;
		int ok = IA_BoardProtocol.STATUS_OK;
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_StatsManager stats = IA_StatsManager.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		string one = DevPlayer(1);
		SyncFresh();
		DevBoards();
		IA_BoardService svc = m_DevBoards;
		CallsSince();

		QueueKills(one, "Probe One", 3);
		stats.QueuePlayerDeath(one, "Probe One");
		store.AddInsertion(one, "Probe One", 20);
		store.RequestRating(one, "Probe One", System.GetTickCount());
		sync.ProbeTick();
		WaitSync();
		Mark(string.Format("dev exchange: mode=%1 stats=%2 transport=%3 rating=%4 answer=%5 chars", sync.GetMode(), sync.ProbeStatsStatus(), sync.ProbeTransportStatus(), store.GetRating(one), sync.ProbeAnswerChars()));
		Mark("dev calls [statistics, transport points and a rating lookup] " + CallsSince());
		Expect(sync.GetMode() == IA_ApiSync.MODE_SYNC && Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && DeltaTotal() == 1, "statistics, transport points and a rating lookup are one request");
		Expect(sync.ProbeStatsStatus() == "accepted" && sync.ProbeTransportStatus() == "accepted", "the development server stores both batches");
		Expect(stats.ProbeBatchId().IsEmpty() && store.ProbeBatchId().IsEmpty(), "and both are forgotten here");
		Expect(store.GetRating(one) >= 20, "the rating comes in the same answer, the new points in it");

		// The first look at each board is a request of its own.
		svc.Request(11, 1, server, score, true, 0);
		WaitIdle(svc);
		int serverTotal = svc.ProbeAnswerTotal(11);
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) > 0 && svc.ProbeMineRank(11) > 0 && svc.ProbeMineKills(11) >= 3, "the first look at this server's board brings its rows and the player's line, the kills just sent in it");
		svc.Request(11, 2, global, score, true, 0);
		WaitIdle(svc);
		int globalTotal = svc.ProbeAnswerTotal(11);
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) == 25, "the first look at the global board brings a page");
		svc.Request(11, 3, servers, score, true, 0);
		WaitIdle(svc);
		int serversTotal = svc.ProbeAnswerTotal(11);
		Expect(svc.ProbeStatus(11) == ok && svc.ProbeRows(11) > 0, "the first look at the board of servers brings a page");
		Mark(string.Format("dev boards: server total=%1 global total=%2 servers total=%3", serverTotal, globalTotal, serversTotal));
		Mark("dev calls [the first look at three boards] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_LEADERBOARD) == 3 && DeltaTotal() == 3, "the first look at each board is one request");

		// The next exchange brings all three, a hundred rows each, and the player's lines.
		DevStep(60000);
		sync.ProbeTick();
		string members = sync.ProbeMembers();
		WaitSync();
		int full = sync.ProbeAnswerChars();
		Expect(Has(members, "\"players\":[\"" + one + "\"]"), "the exchange names the player looking");
		Expect(Has(members, "{\"board\":\"server\",\"etag\":\"\"}") && Has(members, "{\"board\":\"global\",\"etag\":\"\"}") && Has(members, "{\"board\":\"servers\",\"etag\":\"\"}"), "and asks for the three boards looked at, holding none yet");
		Expect(!svc.ProbeEtag(server).IsEmpty() && !svc.ProbeEtag(global).IsEmpty() && !svc.ProbeEtag(servers).IsEmpty(), "an answer holding three boards is read whole");
		Mark(string.Format("dev exchange: three boards answer=%1 chars", full));
		Mark("dev exchange: it came back as " + IA_ApiHandler.GetInstance().ProbeSyncOutcome());
		Mark("dev calls [the exchange that brings three boards] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && DeltaTotal() == 1, "three boards and the own lines are one request");

		// Each board's first hundred rows, read back the way the menu asks for them.
		ref array<int> totals = {serverTotal, globalTotal, serversTotal};
		bool served = true;
		int pages;
		int total;
		int offset;
		for (int board = server; board <= servers; board++)
		{
			total = totals[board - server];
			offset = 0;
			while (offset < 100 && offset < total)
			{
				svc.ProbeForget(11);
				svc.Request(11, 10 + board, board, score, true, offset);
				if (svc.ProbeStatus(11) != ok || svc.ProbeRows(11) == 0)
					served = false;
				if (board != servers && svc.ProbeMineRank(11) <= 0)
					served = false;
				pages = pages + 1;
				offset = offset + 25;
			}
		}
		// The line is built before CallsSince formats its own: a format still unjoined is lost to the next one.
		string readBack = "dev calls [" + pages.ToString() + " pages of the three boards read back] ";
		Mark(readBack + CallsSince());
		Expect(served && pages >= 9 && DeltaTotal() == 0, "the first hundred rows of every board and the player's own line are served with no request");

		// Nothing has changed: the next answer leaves the rows out.
		DevStep(60000);
		sync.ProbeTick();
		members = sync.ProbeMembers();
		WaitSync();
		Mark(string.Format("dev exchange: unchanged boards answer=%1 chars, with rows %2", sync.ProbeAnswerChars(), full));
		Expect(Has(members, "\"boards\"") && !Has(members, "\"etag\":\"\""), "boards already held are asked for by their tag");
		Expect(sync.ProbeAnswerChars() > 0 && sync.ProbeAnswerChars() < full / 2, "and come back without their rows");
		CallsSince();
	}

	//------------------------------------------------------------------------------------------------
	//! An answer lost on the way back: both batches go again under their ids and count once.
	protected void DevResend()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int score = IA_BoardProtocol.SORT_SCORE;
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_StatsManager stats = IA_StatsManager.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		IA_BoardService svc = m_DevBoards;
		string two = DevPlayer(2);

		// Where the player stands before.
		svc.Request(12, 1, server, score, true, 0);
		WaitIdle(svc);
		int before = svc.ProbeMineKills(12);
		if (before < 0)
			before = 0;
		CallsSince();

		QueueKills(two, "Probe Two", 5);
		store.AddInsertion(two, "Probe Two", 10);
		sync.ProbeLoseNext(1);
		DevStep(60000);
		sync.ProbeTick();
		string statsId = stats.ProbeBatchId();
		string transportId = store.ProbeBatchId();
		WaitSync();
		Expect(!statsId.IsEmpty() && !transportId.IsEmpty() && sync.ProbeFailures() == 1, "an answer lost on the way back counts as a failed exchange");
		Expect(stats.ProbeBatchId() == statsId && store.ProbeBatchId() == transportId, "and keeps both batches");

		DevStep(61000);
		sync.ProbeTick();
		string members = sync.ProbeMembers();
		WaitSync();
		Mark(string.Format("dev resend: stats=%1 transport=%2", sync.ProbeStatsStatus(), sync.ProbeTransportStatus()));
		Expect(Has(members, statsId) && Has(members, transportId), "both go again under the same ids");
		Expect(sync.ProbeStatsStatus() == "duplicate" && sync.ProbeTransportStatus() == "duplicate", "the development server has them already and says so");
		Expect(stats.ProbeBatchId().IsEmpty() && store.ProbeBatchId().IsEmpty() && sync.ProbeFailures() == 0, "and both are forgotten");
		Mark("dev calls [a lost answer and its resend] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 2 && DeltaTotal() == 2, "a lost answer costs one more exchange");

		// The board's lifetime on, the player's line is read again.
		DevStep(121000);
		svc.ProbeForget(12);
		svc.Request(12, 2, server, score, true, 0);
		WaitIdle(svc);
		int after = svc.ProbeMineKills(12);
		Mark(string.Format("dev resend: kills before=%1 after=%2", before, after));
		Expect(after == before + 5, "the five kills sent twice are counted once");
		CallsSince();
	}

	//------------------------------------------------------------------------------------------------
	//! A stats service that answers 404 to the exchange: the three separate routes carry
	//! everything, the exchange is tried again after an hour, and is used once it is there.
	protected void DevFallback()
	{
		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		string three = DevPlayer(3);
		int separate = IA_ApiSync.MODE_SEPARATE;
		SyncFresh();
		DevBoards();
		api.ProbeSyncRoute("/syncMissing");
		CallsSince();

		QueueKills(three, "Probe Three", 2);
		store.AddInsertion(three, "Probe Three", 15);
		store.RequestRating(three, "Probe Three", System.GetTickCount());
		sync.ProbeTick();
		WaitSync();
		Mark("dev fallback: the missing route came back as " + api.ProbeSyncOutcome());
		Expect(sync.GetMode() == separate && sync.ProbeFailures() == 0, "a stats service that answers 404 to the exchange is taken as one without it, and not as a failure");

		// The transport batch is answered, then the rating is asked for on its own route.
		int waited;
		while (!store.ProbeBatchId().IsEmpty() && waited < SYNC_ANSWER_TIMEOUT_MS)
		{
			Sleep(20);
			waited = waited + 20;
		}
		store.SendQueuedRequests();
		waited = 0;
		while (store.GetRating(three) < 0 && waited < SYNC_ANSWER_TIMEOUT_MS)
		{
			Sleep(20);
			waited = waited + 20;
		}
		Sleep(500);
		Mark(string.Format("dev fallback: mode=%1 rating=%2", sync.GetMode(), store.GetRating(three)));
		Mark("dev calls [a stats service without the exchange] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && Delta(IA_ApiHandler.ROUTE_SUBMIT_STATS) == 1, "the statistics the exchange carried go on their own route, once");
		Expect(Delta(IA_ApiHandler.ROUTE_SUBMIT_TRANSPORT) == 1 && Delta(IA_ApiHandler.ROUTE_TRANSPORT_RATINGS) == 1 && DeltaTotal() == 4, "and so do the transport points and the rating lookup");
		Expect(store.ProbeBatchId().IsEmpty() && store.GetRating(three) >= 15, "the separate routes store the points and bring the rating");

		// A minute later: the separate routes, and no further try of the exchange.
		QueueKills(three, "Probe Three", 1);
		DevStep(60000);
		sync.ProbeTick();
		Sleep(500);
		Mark("dev calls [the next minute without it] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 0 && Delta(IA_ApiHandler.ROUTE_SUBMIT_STATS) == 1 && DeltaTotal() == 1, "the next turn uses the separate route and does not try the exchange");

		// An hour later it is tried again, and is still not there.
		QueueKills(three, "Probe Three", 1);
		DevStep(3600000);
		sync.ProbeTick();
		WaitSync();
		Sleep(500);
		Mark("dev calls [an hour later, still without it] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && Delta(IA_ApiHandler.ROUTE_SUBMIT_STATS) == 1 && sync.GetMode() == separate, "an hour later the exchange is tried once, and the separate route takes the batch again");

		// The stats service gains the exchange.
		api.ProbeSyncRoute("");
		QueueKills(three, "Probe Three", 1);
		DevStep(3600000);
		sync.ProbeTick();
		WaitSync();
		Mark("dev calls [an hour later, with it] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && DeltaTotal() == 1 && sync.GetMode() == IA_ApiSync.MODE_SYNC, "once the stats service has the exchange it is used");
		Expect(sync.ProbeStatsStatus() == "accepted", "and stores the batch");
	}

	//------------------------------------------------------------------------------------------------
	//! An hour with nobody on the boards and nothing happening.
	protected void DevHourIdle()
	{
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		DevBoards();
		CallsSince();
		for (int minute = 0; minute < 60; minute++)
		{
			DevStep(60000);
			sync.ProbeTick();
			WaitSync();
		}
		Mark("dev hour [nobody looking, nothing happening] " + CallsSince());
		Expect(DeltaTotal() == 0, "an idle server sends the stats service nothing for an hour");
	}

	//------------------------------------------------------------------------------------------------
	//! An hour of play with nobody on the boards: kills every minute, transport points now and then.
	protected void DevHourPlay()
	{
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		SyncFresh();
		DevBoards();
		CallsSince();
		for (int minute = 0; minute < 60; minute++)
		{
			QueueKills(DevPlayer(1 + minute % 4), "Probe", 5);
			if (minute % 10 == 0)
				store.AddInsertion(DevPlayer(4), "Probe Four", 10);
			DevStep(60000);
			sync.ProbeTick();
			WaitSync();
		}
		Mark("dev hour [nobody looking, play going on] " + CallsSince());
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 60 && DeltaTotal() == 60, "an hour of play with nobody on the boards is sixty requests, one a minute");
	}

	//------------------------------------------------------------------------------------------------
	//! An hour of play in which the boards are opened a hundred times, by four players in turn.
	//! \param mixed every fifth look is at the global board; otherwise all are at this server's
	protected void DevHourLooks(bool mixed)
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int score = IA_BoardProtocol.SORT_SCORE;
		int ok = IA_BoardProtocol.STATUS_OK;
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		DevBoards();
		IA_BoardService svc = m_DevBoards;
		CallsSince();

		int looks;
		int unanswered;
		int noLine;
		for (int second = 0; second < 3600; second = second + 12)
		{
			if (second % 36 == 0)
			{
				int player = 11 + looks % 4;
				int board = server;
				if (mixed && looks % 5 == 4)
					board = global;
				svc.ProbeForget(player);
				svc.Request(player, 1 + board, board, score, true, 0);
				WaitIdle(svc);
				if (svc.ProbeStatus(player) != ok || svc.ProbeRows(player) == 0)
					unanswered = unanswered + 1;
				else if (svc.ProbeMineRank(player) <= 0)
					noLine = noLine + 1;
				looks = looks + 1;
			}
			if (second % 60 == 0)
			{
				QueueKills(DevPlayer(1 + (second / 60) % 4), "Probe", 2);
				sync.ProbeTick();
				WaitSync();
			}
			DevStep(12000);
		}

		string what = "dev hour [100 looks at this server's board by four players, play going on] ";
		if (mixed)
			what = "dev hour [100 looks by four players, every fifth at the global board, play going on] ";
		Mark(what + CallsSince());
		MarkCounts("the same hour, as the board service counted it", svc);
		Mark(string.Format("dev hour: looks=%1 without rows=%2 without an own line=%3", looks, unanswered, noLine));
		Expect(looks == 100 && unanswered == 0, "every one of a hundred looks is answered with rows");
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 60, "the exchange still goes once a minute");
		if (mixed)
			Expect(Delta(IA_ApiHandler.ROUTE_LEADERBOARD) <= IA_ApiTunables.BudgetBurst() + IA_ApiTunables.BudgetPerHour(), "and the looks cause no more requests than the allowance");
		else
			Expect(Delta(IA_ApiHandler.ROUTE_LEADERBOARD) <= 4 && noLine == 0, "and a hundred looks cause one request for each player's first look and none after");
	}

	//------------------------------------------------------------------------------------------------
	//! An hour of one player sorting and scrolling the global board every five seconds.
	protected void DevHourHammer()
	{
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		DevBoards();
		IA_BoardService svc = m_DevBoards;
		CallsSince();

		ref array<int> sorts = {IA_BoardProtocol.SORT_SCORE, IA_BoardProtocol.SORT_KILLS, IA_BoardProtocol.SORT_DEATHS, IA_BoardProtocol.SORT_KD};
		int asks;
		for (int second = 0; second < 3600; second = second + 5)
		{
			svc.Request(11, 1 + asks % 4, global, sorts[asks % 4], true, ((asks * 7) % 40) * 25);
			WaitIdle(svc);
			asks = asks + 1;
			if (second % 60 == 0)
			{
				QueueKills(DevPlayer(2), "Probe Two", 2);
				sync.ProbeTick();
				WaitSync();
			}
			DevStep(5000);
		}
		Mark("dev hour [one player sorting and scrolling every five seconds, play going on] " + CallsSince());
		MarkCounts("the same hour, as the board service counted it", svc);
		Expect(asks == 720 && Delta(IA_ApiHandler.ROUTE_SYNC) == 60, "the exchange goes once a minute whatever one player does");
		Expect(Delta(IA_ApiHandler.ROUTE_LEADERBOARD) <= IA_ApiTunables.PlayerBurst() + IA_ApiTunables.PlayerPerHour() + 1, "and one player hammering causes no more requests than their share of the allowance");
	}

	//------------------------------------------------------------------------------------------------
	//! The largest answer an exchange can bring: three boards of a hundred rows and the own
	//! lines of as many players as one exchange names.
	protected void DevLargest()
	{
		int server = IA_BoardProtocol.BOARD_SERVER;
		int global = IA_BoardProtocol.BOARD_GLOBAL;
		int servers = IA_BoardProtocol.BOARD_SERVERS;
		int score = IA_BoardProtocol.SORT_SCORE;
		int crowd = IA_ApiTunables.OWN_LINE_PLAYERS;
		string digits = "0123456789abcdef";
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		SyncFresh();
		DevBoards();
		IA_BoardService svc = m_DevBoards;

		// Most of these looks are refused, the allowance being what it is; they are noted all the same.
		for (int i = 1; i <= crowd; i++)
		{
			svc.ProbeIdentity(100 + i, "5eed0000-0000-4000-9000-0000000000" + digits.Get(i / 16) + digits.Get(i % 16));
			svc.Request(100 + i, 1, global, score, true, 0);
		}
		svc.Request(101, 2, server, score, true, 0);
		svc.Request(101, 3, servers, score, true, 0);
		WaitIdle(svc);
		CallsSince();

		DevStep(60000);
		sync.ProbeTick();
		string members = sync.ProbeMembers();
		WaitSync();
		Mark(string.Format("dev largest: asked with %1 characters, answer=%2 chars", members.Length(), sync.ProbeAnswerChars()));
		Mark("dev largest: it came back as " + IA_ApiHandler.GetInstance().ProbeSyncOutcome());
		Expect(!svc.ProbeEtag(server).IsEmpty() && !svc.ProbeEtag(global).IsEmpty() && !svc.ProbeEtag(servers).IsEmpty(), "the largest answer an exchange can bring is read whole");

		// A player the allowance had refused now has rows and their own line.
		int last = 100 + crowd;
		svc.ProbeForget(last);
		svc.Request(last, 4, global, score, true, 0);
		Mark("dev calls [the largest exchange, and a look after it] " + CallsSince());
		Mark(string.Format("dev largest: the last player named stands at %1", svc.ProbeMineRank(last)));
		Expect(svc.ProbeStatus(last) == IA_BoardProtocol.STATUS_OK && svc.ProbeRows(last) == 25 && svc.ProbeMineRank(last) > 0, "and gives the last of the players it named rows and their own line");
		Expect(Delta(IA_ApiHandler.ROUTE_SYNC) == 1 && DeltaTotal() == 1, "in one request");
	}

	//------------------------------------------------------------------------------------------------
	//! Requests the service has sent to the stats service, of both kinds.
	protected int Fetches(notnull IA_BoardService svc)
	{
		return svc.Count(IA_BoardService.COUNT_FETCH_PAGE) + svc.Count(IA_BoardService.COUNT_FETCH_OWN);
	}

	//------------------------------------------------------------------------------------------------
	//! Wait until the service has nothing out and nothing waiting to go.
	protected void WaitIdle(notnull IA_BoardService svc)
	{
		int waited;
		while (!svc.ProbeIdle() && waited < SETTLE_TIMEOUT_MS)
		{
			Sleep(20);
			waited = waited + 20;
		}
		if (!svc.ProbeIdle())
			Fail("the board service did not settle");
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkCounts(string what, notnull IA_BoardService svc)
	{
		string counts = string.Format("asked=%1 held=%2 short=%3 joined=%4", svc.Count(IA_BoardService.COUNT_ASKED), svc.Count(IA_BoardService.COUNT_SERVED_HELD), svc.Count(IA_BoardService.COUNT_SERVED_SHORT), svc.Count(IA_BoardService.COUNT_JOINED));
		counts = counts + string.Format(" page_requests=%1 own_requests=%2 refused=%3", svc.Count(IA_BoardService.COUNT_FETCH_PAGE), svc.Count(IA_BoardService.COUNT_FETCH_OWN), svc.Count(IA_BoardService.COUNT_LIMITED));
		Mark("counts [" + what + "] " + counts);
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
