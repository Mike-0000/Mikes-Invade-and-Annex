//------------------------------------------------------------------------------------------------
//! Leaderboard menu: the session, this server, every server and the servers themselves, each
//! a board that scrolls over all of its rows and sorts by any column, with local options on
//! the last tab.
//!
//! The menu holds only the rows it has been shown. It asks the server for the page under the
//! table, a page at a time, through the player controller; the answer comes back as a head
//! and short row chunks (see IA_BoardProtocol), so no board is too long to send.
//------------------------------------------------------------------------------------------------
class IA_StatisticsMenu : MUI_MenuBase
{
	protected static const int TAB_OPTIONS = 4;
	protected static const float FRAME_W = 1180;
	protected static const float OPTIONS_H = 494;

	// Longer than the server waits on the stats service (20 s), so its refusal comes first.
	protected static const float ASK_TIMEOUT_S = 25.0;
	protected static const float ASK_GAP_S = 0.15;
	protected static const float RETRY_BUSY_S = 1.2;
	protected static const float RETRY_FAILED_S = 6.0;
	protected static const float RETRY_OFFLINE_S = 15.0;
	protected static const float REFRESH_SESSION_S = 5.0;
	protected static const float REFRESH_STORED_S = 60.0;

	protected static const int LINK_ASKING = 0;
	protected static const int LINK_LIVE = 1;
	protected static const int LINK_LOST = 2;
	protected static const int LINK_OFFLINE = 3;

	protected static const string FOOT_LEADERBOARD = "Session board is local to this restart. Set ./profile/MikesInvadeAndAnnex/server_name.txt for the server board";
	protected static const string FOOT_OPTIONS = "Stored in ./profile/MikesInvadeAndAnnex/local_options.json  •  This machine only";
	protected static const string SUB_OPTIONS = "Local HUD settings  •  This machine only";
	protected static const string TOOL_NOTE = "Pick a column to sort, again to reverse  •  FLIGHT: transport pilot rating  •  LIFTS: insertions flown";

	protected static IA_StatisticsMenu s_Instance;
	// Tags every board and sort order ever shown, so a late answer to an old one is told apart.
	protected static int s_iViewSeed;

	protected ref IA_UplinkFrame m_Frame;
	protected ref IA_UplinkTabs m_Tabs;
	protected ref MUI_Row m_Tools;
	protected ref IA_UplinkButton m_PilotsBtn;
	protected ref IA_UplinkButton m_TopBtn;
	protected ref IA_UplinkButton m_MineBtn;
	protected ref IA_BoardModel m_Model;
	protected ref IA_LeaderboardBoard m_Board;
	protected ref MUI_Panel m_Options;
	protected ref MUI_Toggle m_HideRankToggle;
	protected ref MUI_Toggle m_HidePromoToggle;
	protected ref IA_UplinkNote m_FootNote;

	// The order each board was last left in.
	protected ref array<int> m_aSort = {};
	protected ref array<bool> m_aDescending = {};

	protected int m_iBoard = -1;
	protected int m_iViewId;
	protected int m_iAskedPage = -1;
	protected float m_fAskAge;
	protected float m_fSinceAsk = 1;
	protected float m_fHold;
	protected float m_fRefresh;

#ifdef WORKBENCH
	// A Workbench probe fills the boards in a world with no game mode and no stats service.
	protected static bool s_bProbe;
	protected static int s_iProbeTotal;
	protected static int s_iProbeMine;
	protected static int s_iProbeStatus;
#endif

	//------------------------------------------------------------------------------------------------
	//! The head of a page the server answered with, from the player controller.
	static void OnBoardHead(int viewId, int status, int total, int offset, string mine)
	{
		if (s_Instance)
			s_Instance.HandleHead(viewId, status, total, offset, mine);
	}

	//------------------------------------------------------------------------------------------------
	//! One chunk of that page's rows, from the player controller.
	static void OnBoardRows(int viewId, int offset, bool last, string rows)
	{
		if (s_Instance)
			s_Instance.HandleRows(viewId, offset, last, rows);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		m_Model = new IA_BoardModel();
		m_aSort.Clear();
		m_aDescending.Clear();
		for (int board = 0; board < IA_BoardProtocol.BOARD_COUNT; board++)
		{
			m_aSort.Insert(IA_BoardProtocol.SORT_SCORE);
			m_aDescending.Insert(true);
		}

		super.OnMenuOpen();
		if (!m_Board)
			return;

		s_Instance = this;
		ShowTab(0);

		// A gamepad starts on the section rail.
		MUI_Runtime runtime = GetRuntime();
		InputManager input = GetGame().GetInputManager();
		if (runtime && input && !input.IsUsingMouseAndKeyboard())
			runtime.FocusNode(m_Tabs);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_Instance == this)
			s_Instance = null;
		if (m_Tabs)
			m_Tabs.GetOnChanged().Remove(OnTabsChanged);
		if (m_Board)
			m_Board.GetOnSortChanged().Remove(OnSortChanged);
		m_Board = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMUIMountFailed()
	{
		Print("[IA_StatisticsMenu] MUI mount failed — blank layout required.", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_StatisticsMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.SetFill(Color.FromInt(0));
		overlay.SetIntro(0, 0.35, 0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");
		fx.SetIntro(0, 0.55, 0);

		m_Frame = IA_UplinkFrame.Create(runtime, "frame", FRAME_W, "COMMAND UPLINK", "LEADERBOARD", "");

		m_Tabs = IA_UplinkTabs.Create(runtime, "tabs");
		m_Tabs.AddTab("Session");
		m_Tabs.AddTab("This Server");
		m_Tabs.AddTab("Global");
		m_Tabs.AddTab("Servers");
		m_Tabs.AddTab("Options");
		m_Tabs.GetOnChanged().Insert(OnTabsChanged);

		BuildTools(runtime);

		m_Board = IA_LeaderboardBoard.Create(runtime, "board", m_Model);
		m_Board.GetOnSortChanged().Insert(OnSortChanged);

		BuildOptionsPage(runtime);

		ref MUI_Row foot = runtime.CreateRow("foot");
		foot.SetGap(16);

		m_FootNote = IA_UplinkNote.Create(runtime, FOOT_LEADERBOARD, "footNote");
		m_FootNote.SetAlign(0, 0.5);

		ref IA_UplinkButton closeBtn = IA_UplinkButton.Create(runtime, "Close", "close");
		closeBtn.SetGrow(0);
		closeBtn.SetMinWidth(170);
		closeBtn.GetOnClicked().Insert(OnMikesClose);

		foot.AddChild(m_FootNote);
		foot.AddChild(closeBtn);

		m_Frame.AddChild(m_Tabs);
		m_Frame.AddChild(m_Tools);
		m_Frame.AddChild(m_Board);
		m_Frame.AddChild(m_Options);
		m_Frame.AddChild(foot);

		overlay.AddChild(fx);
		overlay.AddChild(m_Frame);
		runtime.SetRoot(overlay);
		runtime.SetPromptText("<action name='MenuSelect' scale='1.35'/>  Select", "<action name='MenuBack' scale='1.35'/>  Close");
	}

	//------------------------------------------------------------------------------------------------
	//! The row over the board: what the columns mean, and the jumps.
	protected void BuildTools(notnull MUI_Runtime runtime)
	{
		m_Tools = runtime.CreateRow("tools");
		m_Tools.SetGap(8);

		ref IA_UplinkNote note = IA_UplinkNote.Create(runtime, TOOL_NOTE, "toolNote");
		note.SetAlign(0, 0.5);

		m_PilotsBtn = IA_UplinkButton.Create(runtime, "Best pilots", "bestPilots");
		m_PilotsBtn.SetCompact();
		m_PilotsBtn.SetGrow(0);
		m_PilotsBtn.GetOnClicked().Insert(OnBestPilots);

		m_TopBtn = IA_UplinkButton.Create(runtime, "Top", "top");
		m_TopBtn.SetCompact();
		m_TopBtn.SetGrow(0);
		m_TopBtn.GetOnClicked().Insert(OnTop);

		m_MineBtn = IA_UplinkButton.Create(runtime, "Find me", "findMe");
		m_MineBtn.SetCompact();
		m_MineBtn.SetGrow(0);
		m_MineBtn.GetOnClicked().Insert(OnFindMe);

		m_Tools.AddChild(note);
		m_Tools.AddChild(m_PilotsBtn);
		m_Tools.AddChild(m_TopBtn);
		m_Tools.AddChild(m_MineBtn);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildOptionsPage(notnull MUI_Runtime runtime)
	{
		m_Options = runtime.CreatePanel("options");
		m_Options.SetFill(Color.FromInt(0));
		m_Options.SetRadius(0);
		m_Options.SetGap(10);
		m_Options.SetHeight(OPTIONS_H);
		m_Options.SetPaddingTRBL(6, 460, 0, 0);

		ref IA_UplinkCaption caption = IA_UplinkCaption.Create(runtime, "Heads-up display", "optCaption");
		ref IA_UplinkNote intro = IA_UplinkNote.Create(runtime, "These options apply only on this machine. They do not sync to the server or other players.", "optIntro");

		IA_LocalOptions options = IA_LocalOptions.Get();

		m_HideRankToggle = IA_UplinkToggle.Create(runtime, "Hide ranking HUD", "hideRank");
		m_HideRankToggle.SetChecked(options.HideRankHud());
		m_HideRankToggle.GetOnChanged().Insert(OnHideRankChanged);

		ref IA_UplinkNote rankHint = IA_UplinkNote.Create(runtime, "Hides the session rank chip in the top-right of the HUD.", "hideRankHint");

		m_HidePromoToggle = IA_UplinkToggle.Create(runtime, "Hide promotion notifications", "hidePromo");
		m_HidePromoToggle.SetChecked(options.HidePromotionNotifications());
		m_HidePromoToggle.GetOnChanged().Insert(OnHidePromoChanged);

		ref IA_UplinkNote promoHint = IA_UplinkNote.Create(runtime, "Skips the on-screen toast when you are promoted.", "hidePromoHint");

		m_Options.AddChild(caption);
		m_Options.AddChild(intro);
		m_Options.AddChild(m_HideRankToggle);
		m_Options.AddChild(rankHint);
		m_Options.AddChild(m_HidePromoToggle);
		m_Options.AddChild(promoHint);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHideRankChanged()
	{
		if (!m_HideRankToggle)
			return;
		IA_LocalOptions.Get().SetHideRankHud(m_HideRankToggle.IsChecked());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHidePromoChanged()
	{
		if (!m_HidePromoToggle)
			return;
		IA_LocalOptions.Get().SetHidePromotionNotifications(m_HidePromoToggle.IsChecked());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabsChanged()
	{
		if (!m_Tabs)
			return;
		ShowTab(m_Tabs.GetIndex());
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowTab(int index)
	{
		if (!m_Frame || !m_Board || !m_Options)
			return;

		bool options = index == TAB_OPTIONS;
		m_Tools.SetVisible(!options);
		m_Board.SetVisible(!options);
		m_Options.SetVisible(options);

		if (options)
		{
			m_iBoard = -1;
			m_iAskedPage = -1;
			m_Frame.SetTitle("OPTIONS");
			m_Frame.SetSubtitle(SUB_OPTIONS);
			m_Frame.SetStatus("THIS MACHINE", IA_UplinkStyle.Get().m_Cyan, false);
			m_Frame.SetNote("");
			m_FootNote.SetText(FOOT_OPTIONS);
			return;
		}

		if (!IA_BoardProtocol.IsBoard(index))
			return;

		m_Frame.SetTitle("LEADERBOARD");
		m_FootNote.SetText(FOOT_LEADERBOARD);
		OpenBoard(index);
	}

	//------------------------------------------------------------------------------------------------
	//! \param board an IA_BoardProtocol.BOARD_ value; the tabs are in the same order
	protected void OpenBoard(int board)
	{
		m_iBoard = board;
		m_Board.SetBoard(board, BoardCaption(board));
		m_Board.SetSort(m_aSort[board], m_aDescending[board]);
		m_Frame.SetSubtitle(BoardSubtitle(board));

		if (board == IA_BoardProtocol.BOARD_SERVERS)
			m_MineBtn.SetText("Find this server");
		else
			m_MineBtn.SetText("Find me");

		NewView();
	}

	//------------------------------------------------------------------------------------------------
	protected string BoardCaption(int board)
	{
		if (board == IA_BoardProtocol.BOARD_SERVER)
			return "THIS SERVER";
		if (board == IA_BoardProtocol.BOARD_GLOBAL)
			return "ALL SERVERS";
		if (board == IA_BoardProtocol.BOARD_SERVERS)
			return "SERVER STANDINGS";
		return "THIS SESSION";
	}

	//------------------------------------------------------------------------------------------------
	protected string BoardSubtitle(int board)
	{
		if (board == IA_BoardProtocol.BOARD_SERVER)
			return "Every player this server has on record";
		if (board == IA_BoardProtocol.BOARD_GLOBAL)
			return "Every player on every server running the mode";
		if (board == IA_BoardProtocol.BOARD_SERVERS)
			return "Each server's players and their combined record";
		return "Players since this restart  •  Grades and XP reset with the server";
	}

	//------------------------------------------------------------------------------------------------
	//! What the table says when the board is there and has nobody on it.
	protected string EmptyText(int board)
	{
		if (board == IA_BoardProtocol.BOARD_SESSION)
			return "Nobody has scored since this restart.";
		if (board == IA_BoardProtocol.BOARD_SERVERS)
			return "No server has sent a record yet.";
		return "No player has a record here yet.";
	}

	//------------------------------------------------------------------------------------------------
	//! Start the board over: another board, or the same one in another order.
	protected void NewView()
	{
		s_iViewSeed = s_iViewSeed + 1;
		m_iViewId = s_iViewSeed;
		m_Model.Reset();
		m_iAskedPage = -1;
		m_fHold = 0;
		m_fRefresh = RefreshPeriod();
		m_Board.SetNotice("", EmptyText(m_iBoard));
		m_Frame.SetNote("");
		ShowLink(LINK_ASKING);
	}

	//------------------------------------------------------------------------------------------------
	protected float RefreshPeriod()
	{
		if (m_iBoard == IA_BoardProtocol.BOARD_SESSION)
			return REFRESH_SESSION_S;
		return REFRESH_STORED_S;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSortChanged()
	{
		if (!m_Board || !IA_BoardProtocol.IsBoard(m_iBoard))
			return;
		m_aSort[m_iBoard] = m_Board.GetSort();
		m_aDescending[m_iBoard] = m_Board.IsDescending();
		NewView();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBestPilots()
	{
		if (!m_Board || !IA_BoardProtocol.IsBoard(m_iBoard))
			return;
		m_Board.SetSort(IA_BoardProtocol.SORT_TRANSPORT, true);
		OnSortChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTop()
	{
		if (m_Board)
			m_Board.ScrollToTop();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFindMe()
	{
		if (!m_Board)
			return;
		if (!m_Board.ScrollToMine())
			IA_UplinkStyle.ClickFail();
	}

	//------------------------------------------------------------------------------------------------
	//! The state of the link, on the chip in the header.
	protected void ShowLink(int state)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		if (state == LINK_ASKING)
		{
			m_Frame.SetStatus("RECEIVING", look.m_Tone, true);
			return;
		}
		if (state == LINK_LIVE)
		{
			if (m_iBoard == IA_BoardProtocol.BOARD_SESSION)
				m_Frame.SetStatus("LIVE", look.m_Green, false);
			else
				m_Frame.SetStatus("SYNCED", look.m_Green, false);
			return;
		}
		if (state == LINK_LOST)
		{
			m_Frame.SetStatus("NO SIGNAL", look.m_Red, false);
			if (!m_Model.HasRows())
				m_Board.SetNotice("NO SIGNAL", "The stats service did not answer. Trying again shortly.");
			return;
		}

		m_Frame.SetStatus("OFFLINE", look.m_Muted, false);
		if (!m_Model.HasRows())
			m_Board.SetNotice("BOARD OFFLINE", "This server is not linked to the stats service. The session board still works.");
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (!m_Board || !m_Model || !IA_BoardProtocol.IsBoard(m_iBoard))
			return;
		Pump(tDelta);
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the rows under the table asked for: one request at a time, never faster than the
	//! server takes them, and a slow refresh of what is already shown.
	protected void Pump(float dt)
	{
		m_fSinceAsk = m_fSinceAsk + dt;

		if (m_iAskedPage >= 0)
		{
			m_fAskAge = m_fAskAge + dt;
			if (m_fAskAge < ASK_TIMEOUT_S)
				return;

			m_Model.Fail(m_iAskedPage);
			m_iAskedPage = -1;
			m_fHold = RETRY_FAILED_S;
			ShowLink(LINK_LOST);
			return;
		}

		m_fRefresh = m_fRefresh - dt;
		if (m_fRefresh <= 0)
		{
			m_fRefresh = RefreshPeriod();
			m_Model.MarkAllStale();
		}

		if (m_fHold > 0)
		{
			m_fHold = m_fHold - dt;
			return;
		}
		if (m_fSinceAsk < ASK_GAP_S)
			return;

		int page = m_Model.FirstWanted(m_Board.GetFirstVisible(), m_Board.GetLastVisible());
		if (page >= 0)
			Ask(page);
	}

	//------------------------------------------------------------------------------------------------
	protected void Ask(int page)
	{
		m_Model.MarkAsked(page);
		m_iAskedPage = page;
		m_fAskAge = 0;
		m_fSinceAsk = 0;

#ifdef WORKBENCH
		if (s_bProbe)
		{
			ProbeAnswer(page);
			return;
		}
#endif

		// A hosting machine answers inside this call.
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			HandleHead(m_iViewId, IA_BoardProtocol.STATUS_OFFLINE, 0, page * IA_BoardProtocol.PAGE_ROWS, "");
			return;
		}
		controller.IA_AskLeaderboardPage(m_iViewId, m_iBoard, m_Board.GetSort(), m_Board.IsDescending(), page * IA_BoardProtocol.PAGE_ROWS);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleHead(int viewId, int status, int total, int offset, string mine)
	{
		if (viewId != m_iViewId || m_iAskedPage < 0 || !m_Model || !m_Board)
			return;
		// A late answer to a page this menu gave up on is not the one being waited for.
		if (offset / IA_BoardProtocol.PAGE_ROWS != m_iAskedPage)
			return;

		if (status == IA_BoardProtocol.STATUS_OK)
		{
			m_Model.OnHead(total, offset, mine);
			return;
		}

		// A refusal has no rows; the page is asked for again after a wait.
		m_Model.Fail(m_iAskedPage);
		m_iAskedPage = -1;
		if (status == IA_BoardProtocol.STATUS_BUSY)
		{
			m_fHold = RETRY_BUSY_S;
			return;
		}
		if (status == IA_BoardProtocol.STATUS_OFFLINE)
		{
			m_fHold = RETRY_OFFLINE_S;
			ShowLink(LINK_OFFLINE);
			return;
		}
		m_fHold = RETRY_FAILED_S;
		ShowLink(LINK_LOST);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleRows(int viewId, int offset, bool last, string rows)
	{
		if (viewId != m_iViewId || m_iAskedPage < 0 || !m_Model || !m_Board)
			return;
		if (offset / IA_BoardProtocol.PAGE_ROWS != m_iAskedPage)
			return;

		if (m_Model.OnRows(offset, last, rows) != m_iAskedPage)
			return;

		m_iAskedPage = -1;
		m_Board.SetNotice("", EmptyText(m_iBoard));
		ShowLink(LINK_LIVE);

		string total = IA_PilotDropoffPayload.FormatNumber(m_Model.GetTotal());
		if (m_iBoard == IA_BoardProtocol.BOARD_SERVERS)
			m_Frame.SetNote(total + " SERVERS");
		else
			m_Frame.SetNote(total + " PLAYERS");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesClose()
	{
		Close();
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: stand in for the server with a board of \p total made-up rows.
	//! \param mineRank the place the player holds on it, 0 for none
	static void ProbeBegin(int total, int mineRank)
	{
		s_bProbe = true;
		s_iProbeTotal = total;
		s_iProbeMine = mineRank;
		s_iProbeStatus = IA_BoardProtocol.STATUS_OK;
	}

	//------------------------------------------------------------------------------------------------
	static void ProbeEnd()
	{
		s_bProbe = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: what the stand-in server answers from now on, an IA_BoardProtocol.STATUS_ value.
	static void ProbeStatus(int status)
	{
		s_iProbeStatus = status;
		if (s_Instance && s_Instance.m_Board && IA_BoardProtocol.IsBoard(s_Instance.m_iBoard))
			s_Instance.NewView();
	}

	//------------------------------------------------------------------------------------------------
	static bool ProbeTab(int index)
	{
		if (!s_Instance || !s_Instance.m_Tabs)
			return false;
		s_Instance.m_Tabs.SetIndex(index);
		return s_Instance.m_Tabs.GetIndex() == index;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: sort as a click on a column head does. \return false when the board has no such column
	static bool ProbeSort(int sortKey)
	{
		if (!s_Instance || !s_Instance.m_Board)
			return false;
		s_Instance.m_Board.ChooseSort(sortKey);
		return s_Instance.m_Board.GetSort() == sortKey;
	}

	//------------------------------------------------------------------------------------------------
	static bool ProbeDescending()
	{
		if (!s_Instance || !s_Instance.m_Board)
			return false;
		return s_Instance.m_Board.IsDescending();
	}

	//------------------------------------------------------------------------------------------------
	static void ProbeScroll(int row)
	{
		if (s_Instance && s_Instance.m_Board)
			s_Instance.m_Board.ScrollToRow(row);
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: press a tool button. \param which 0 best pilots, 1 top, 2 find me
	static void ProbeTool(int which)
	{
		if (!s_Instance || !s_Instance.m_Board)
			return;
		if (which == 0)
			s_Instance.OnBestPilots();
		else if (which == 1)
			s_Instance.OnTop();
		else
			s_Instance.OnFindMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: rows the board says it has, -1 before the first answer.
	static int ProbeTotal()
	{
		if (!s_Instance || !s_Instance.m_Model)
			return -1;
		return s_Instance.m_Model.GetTotal();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the place written on the row held at an index, 0 when the row has not arrived.
	static int ProbeRankAt(int index)
	{
		if (!s_Instance || !s_Instance.m_Model)
			return 0;
		IA_BoardRow row = s_Instance.m_Model.GetRow(index);
		if (!row)
			return 0;
		return row.m_iRank;
	}

	//------------------------------------------------------------------------------------------------
	static int ProbeFirstVisible()
	{
		if (!s_Instance || !s_Instance.m_Board)
			return -1;
		return s_Instance.m_Board.GetFirstVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: flip an options toggle. \return its state after, as the local options hold it
	static bool ProbeToggleRankHud()
	{
		if (!s_Instance || !s_Instance.m_HideRankToggle)
			return false;
		s_Instance.m_HideRankToggle.SetChecked(!s_Instance.m_HideRankToggle.IsChecked());
		return IA_LocalOptions.Get().HideRankHud();
	}

	//------------------------------------------------------------------------------------------------
	//! One made-up line. Every number falls as the place does, so any column reads in order.
	protected string ProbeLine(int rank)
	{
		float total = s_iProbeTotal;
		if (total < 1)
			total = 1;
		float t = 1.0 - (rank - 1) / total;
		if (!m_Board.IsDescending())
			t = rank / total;

		int kills = 12 + t * t * 5200;
		int deaths = 4 + t * 610;
		int hvt = t * t * 96;
		int guard = t * 240;
		int obj = t * t * 1900;
		int score = 150 + t * t * 486000;
		int transport = t * t * 2400;
		int insertions = t * 380;
		int players = 0;
		int grade = 0;

		string name;
		int pick = rank % 6;
		if (pick == 0)
			name = "Sgt. Whiskey";
		else if (pick == 1)
			name = "Nightstalker";
		else if (pick == 2)
			name = "a|pipe|in|the|name";
		else if (pick == 3)
			name = "Lt. Dan";
		else if (pick == 4)
			name = "The Longest Callsign In The Whole Theatre Of War";
		else
			name = "Kowalski";
		name = name + " " + rank.ToString();

		if (m_iBoard == IA_BoardProtocol.BOARD_SERVERS)
		{
			name = "Mikes Invade and Annex #" + rank.ToString();
			players = 3 + t * 1148;
			obj = 0;
		}
		if (m_iBoard == IA_BoardProtocol.BOARD_SESSION)
		{
			score = 20 + t * t * 9400;
			grade = IA_SessionRankLadder.GetRankByXp(score);
		}

		return IA_BoardRow.Pack(rank, name, kills, deaths, hvt, guard, obj, score, transport, insertions, players, grade);
	}

	//------------------------------------------------------------------------------------------------
	//! Answer a page the way the server does: a head, then the rows in two chunks.
	protected void ProbeAnswer(int page)
	{
		int offset = page * IA_BoardProtocol.PAGE_ROWS;
		if (s_iProbeStatus != IA_BoardProtocol.STATUS_OK)
		{
			HandleHead(m_iViewId, s_iProbeStatus, 0, offset, "");
			return;
		}

		string mine;
		if (s_iProbeMine >= 1 && s_iProbeMine <= s_iProbeTotal)
			mine = ProbeLine(s_iProbeMine);
		HandleHead(m_iViewId, IA_BoardProtocol.STATUS_OK, s_iProbeTotal, offset, mine);

		int end = offset + IA_BoardProtocol.PAGE_ROWS;
		if (end > s_iProbeTotal)
			end = s_iProbeTotal;
		int split = offset + 13;
		int chunkOffset = offset;
		string chunk;
		for (int i = offset; i < end; i++)
		{
			if (i == split)
			{
				HandleRows(m_iViewId, chunkOffset, false, chunk);
				chunk = "";
				chunkOffset = split;
			}
			if (!chunk.IsEmpty())
				chunk = chunk + "\n";
			chunk = chunk + ProbeLine(i + 1);
		}
		HandleRows(m_iViewId, chunkOffset, true, chunk);
	}
#endif
}
