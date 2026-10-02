//------------------------------------------------------------------------------------------------
//! Server: what stands between the players' leaderboard menus and the stats service.
//!
//! A page of a board is held once and shared. Whoever asks for it within its lifetime is
//! answered from here, and players who ask for the same missing page wait on one request.
//! Where a player stands is held apart, one line per player, and joined to the page only as
//! the answer leaves, so no player is sent another's line.
//!
//! Every request to the stats service is paid for from an allowance (IA_ApiTunables): so many
//! an hour for the server, and a share of that for each player. With the allowance spent, a
//! page that is held is served however old it is, and one that is not is refused with the
//! seconds to wait. Nothing a stats batch does clears what is held; it ages out.
//!
//! While somebody looks at a board in the order the menu opens on, the timed exchange
//! (IA_ApiSync) brings its first rows and the lookers' own lines along at no request of their
//! own. They are held here like any other answer, so that view costs the allowance nothing.
//! Other orders, rows further down and a rank in another order are asked for as above.
//!
//! IA_LeaderboardManagerComponent owns the one in play and checks what clients send.
//------------------------------------------------------------------------------------------------
class IA_BoardService
{
	static const int COUNT_ASKED = 0;			// requests from players
	static const int COUNT_SERVED_HELD = 1;		// answered at once from a page and own line still fresh
	static const int COUNT_SERVED_SHORT = 2;	// answered with an old page or no own line: allowance spent, or the service failed
	static const int COUNT_JOINED = 3;			// waited on a request another player had caused
	static const int COUNT_FETCH_PAGE = 4;		// page requests sent to the stats service
	static const int COUNT_FETCH_OWN = 5;		// own-line requests sent to the stats service
	static const int COUNT_LIMITED = 6;			// refused: allowance spent and nothing held
	static const int COUNT_KINDS = 7;

	protected static const int QUEUE_MAX = 32;
	protected static const int FETCH_TIMEOUT_MS = 20000;
	protected static const int WAIT_MAX_S = 900;
	protected static const int PLAYERS_KEPT = 96;
	protected static const string KEY_SEP = "|";
	protected static const string KEY_OWN = "me";

	protected ref IA_BoardCache m_Pages;
	protected ref IA_BoardCache m_Own;
	protected ref IA_BoardBucket m_Budget = new IA_BoardBucket();
	protected ref map<int, ref IA_BoardBucket> m_mPlayerBudgets = new map<int, ref IA_BoardBucket>();
	protected ref array<ref IA_BoardFetch> m_aQueue = {};
	protected ref IA_BoardFetch m_Active;
	protected ref array<int> m_aCounts = {};
	protected static IA_BoardService s_Instance;

	// What the timed exchange brought, by board: the first rows by score, and the tag that names them.
	protected ref array<ref IA_BoardPage> m_aSnaps = {};
	protected ref array<string> m_aEtags = {};
	// Until when each board, and each player's own line, is wanted from the timed exchange.
	protected ref array<int> m_aLookUntilMs = {};
	protected ref array<bool> m_aLooked = {};
	protected ref map<string, int> m_mViewers = new map<string, int>();
	// What the exchange now out was asked for.
	protected ref array<int> m_aAskedBoards = {};
	protected ref array<string> m_aAskedPlayers = {};

#ifdef WORKBENCH
	//! The player the leaderboard menu stands for when a probe hands it a service.
	static const int PROBE_MENU_PLAYER = 1;
	protected static const int PROBE_ANSWER_MS = 40;

	protected bool m_bProbe;
	protected bool m_bProbeNote;
	protected ref map<int, string> m_mProbeIdentity = new map<int, string>();
	protected int m_iProbeTotal;
	protected int m_iProbeSkewMs;
	protected int m_iProbeFailures;
	protected ref map<int, int> m_mProbeStatus = new map<int, int>();
	protected ref map<int, int> m_mProbeTotal = new map<int, int>();
	protected ref map<int, int> m_mProbeRows = new map<int, int>();
	protected ref map<int, string> m_mProbeMine = new map<int, string>();
#endif

	//------------------------------------------------------------------------------------------------
	//! \return the service the stats service's answers go to, null when none is running
	static IA_BoardService GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void IA_BoardService()
	{
		m_Pages = new IA_BoardCache(IA_ApiTunables.CACHE_PAGES);
		m_Own = new IA_BoardCache(IA_ApiTunables.CACHE_OWN_LINES);
		for (int i = 0; i < COUNT_KINDS; i++)
		{
			m_aCounts.Insert(0);
		}
		for (int board = 0; board < IA_BoardProtocol.BOARD_COUNT; board++)
		{
			m_aSnaps.Insert(null);
			m_aEtags.Insert("");
			m_aLookUntilMs.Insert(0);
			m_aLooked.Insert(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Take the stats service's answers from now on.
	void Open()
	{
		s_Instance = this;
		// The timed exchange brings the boards players look at, so it has to be turning.
		if (Replication.IsServer())
			IA_ApiSync.GetInstance().Start();
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		GetGame().GetCallqueue().Remove(this.OnFetchTimeout);
#ifdef WORKBENCH
		GetGame().GetCallqueue().Remove(this.ProbeDeliver);
#endif
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param kind a COUNT_ value
	//! \return how often it has happened since this service started
	int Count(int kind)
	{
		if (kind < 0 || kind >= COUNT_KINDS)
			return 0;
		return m_aCounts[kind];
	}

	//------------------------------------------------------------------------------------------------
	//! Answer one player's request for a page of a stored board. The caller has checked the
	//! board, the sort key and the offset, which is the first row of a page.
	//! \param viewId the client's tag for this board and sort order, sent back with the answer
	void Request(int playerId, int viewId, int board, int sortKey, bool descending, int offset)
	{
		Bump(COUNT_ASKED);
		if (!IsLinked())
		{
			SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_OFFLINE);
			return;
		}
		NoteLook(playerId, board, sortKey, descending);
		Resolve(playerId, viewId, board, sortKey, descending, offset, false);
	}

	//------------------------------------------------------------------------------------------------
	//! What the timed exchange should ask for on behalf of the players looking at boards.
	//! \return the "players" and "boards" members of a /sync body, each led by a comma; empty
	//! when nobody has looked within a board's lifetime
	string SyncWanted()
	{
		int now = NowMs();
		m_aAskedBoards.Clear();
		m_aAskedPlayers.Clear();

		ref array<string> gone = {};
		foreach (string guid, int until : m_mViewers)
		{
			if (until - now < 0)
				gone.Insert(guid);
			else if (m_aAskedPlayers.Count() < IA_ApiTunables.OWN_LINE_PLAYERS)
				m_aAskedPlayers.Insert(guid);
		}
		foreach (string left : gone)
		{
			m_mViewers.Remove(left);
		}

		string boards;
		for (int board = IA_BoardProtocol.BOARD_SERVER; board < IA_BoardProtocol.BOARD_COUNT; board++)
		{
			if (!m_aLooked[board] || m_aLookUntilMs[board] - now < 0)
				continue;

			// The tag of rows already held lets the stats service leave them out when they are the same.
			string etag;
			if (m_aSnaps[board])
				etag = m_aEtags[board];
			else
				etag = "";
			if (!boards.IsEmpty())
				boards = boards + ",";
			boards = boards + "{\"board\":\"" + IA_BoardProtocol.BoardName(board) + "\",\"etag\":\"" + IA_JsonEscape(etag) + "\"}";
			m_aAskedBoards.Insert(board);
		}

		string members;
		if (!m_aAskedPlayers.IsEmpty())
		{
			string players;
			foreach (string asked : m_aAskedPlayers)
			{
				if (!players.IsEmpty())
					players = players + ",";
				players = players + "\"" + IA_JsonEscape(asked) + "\"";
			}
			members = ",\"players\":[" + players + "]";
		}
		if (!boards.IsEmpty())
			members = members + ",\"boards\":[" + boards + "]";
		return members;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_ApiSync with a /sync answer whose boards part succeeded. The rows and own
	//! lines in it are held as if players had asked for them: by score, high to low.
	void OnSyncBoards(notnull JsonLoadContext ctx)
	{
		int now = NowMs();
		ref array<ref IA_ApiSyncBoard> heads = {};
		ctx.ReadValue("boards", heads);
		foreach (IA_ApiSyncBoard head : heads)
		{
			if (!head)
				continue;
			int board = BoardOf(head.board);
			if (board < 0)
				continue;

			if (head.unchanged == 0)
			{
				ref array<ref IA_ApiBoardRow> rows = {};
				ctx.ReadValue(head.board + "Rows", rows);
				ref IA_BoardPage made = new IA_BoardPage();
				PackRows(rows, made.m_aRows);
				m_aSnaps[board] = made;
				m_aEtags[board] = head.etag;
			}

			// Told the rows are unchanged with none held: the next exchange asks for them outright.
			IA_BoardPage snap = m_aSnaps[board];
			if (!snap)
				continue;
			snap.m_iTotal = head.total;
			snap.m_iStampMs = now;
			FileSnapshot(board, snap);
		}

		ref array<ref IA_ApiSyncOwnRow> lines = {};
		ctx.ReadValue("own", lines);
		ref set<string> found = new set<string>();
		foreach (IA_ApiSyncOwnRow line : lines)
		{
			if (!line)
				continue;
			int lineBoard = BoardOf(line.board);
			if (lineBoard < 0)
				continue;

			ref array<string> one = {};
			one.Insert(IA_BoardRow.Pack(line.r, line.n, line.k, line.d, line.h, line.g, line.o, line.s, line.t, line.i, line.p, 0));
			FileOwn(lineBoard, line.id, one, now);
			found.Insert(OwnMark(lineBoard, line.id));
		}

		// A player asked about who has no line is not on that board, and that is an answer too.
		ref array<string> none = {};
		foreach (string asked : m_aAskedPlayers)
		{
			if (!found.Contains(OwnMark(IA_BoardProtocol.BOARD_SERVER, asked)))
				FileOwn(IA_BoardProtocol.BOARD_SERVER, asked, none, now);
			if (!found.Contains(OwnMark(IA_BoardProtocol.BOARD_GLOBAL, asked)))
				FileOwn(IA_BoardProtocol.BOARD_GLOBAL, asked, none, now);
		}
		if (m_aAskedBoards.Contains(IA_BoardProtocol.BOARD_SERVERS) && !found.Contains(OwnMark(IA_BoardProtocol.BOARD_SERVERS, "")))
			FileOwn(IA_BoardProtocol.BOARD_SERVERS, "", none, now);

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Leaderboard] Exchange brought %1 boards and %2 own lines. Held: %3 pages, %4 own lines.", heads.Count(), lines.Count(), m_Pages.Count(), m_Own.Count()), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_ApiHandler with the body of a /leaderboard answer.
	void OnPageReceived(string data)
	{
		if (!m_Active)
			return;

		string board;
		string sortName;
		bool descending;
		int offset;
		int total;
		ref array<string> rows = {};
		ref array<string> mine = {};
		if (!ParseAnswer(data, board, sortName, descending, offset, total, rows, mine))
		{
			Print("[IA][Leaderboard] The stats service sent a page that could not be read.", LogLevel.ERROR);
			FinishActive(IA_BoardProtocol.STATUS_FAILED);
			return;
		}

		// A late answer to a request that already timed out must not be filed under this one.
		if (board != IA_BoardProtocol.BoardName(m_Active.m_iBoard) || sortName != IA_BoardProtocol.SortName(m_Active.m_iSort))
			return;
		if (descending != m_Active.m_bDescending || offset != m_Active.m_iOffset)
			return;

		int now = NowMs();
		string view = ViewKey(m_Active.m_iBoard, m_Active.m_iSort, m_Active.m_bDescending);
		if (m_Active.m_iLimit > 0)
		{
			ref IA_BoardPage page = new IA_BoardPage();
			page.m_iTotal = total;
			page.m_iStampMs = now;
			page.m_aRows = rows;
			m_Pages.Put(PageKey(view, m_Active.m_iOffset), page);
		}
		// The own line that came with it is filed under its player, never with the page.
		if (m_Active.m_bWantMine)
		{
			ref IA_BoardPage own = new IA_BoardPage();
			own.m_iTotal = total;
			own.m_iStampMs = now;
			own.m_aRows = mine;
			m_Own.Put(OwnKey(view, m_Active.m_sGuid), own);
		}

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Leaderboard] Page received: board %1, sort %2, offset %3, %4 rows of %5. Held: %6 pages, %7 own lines.", board, sortName, offset, rows.Count(), total, m_Pages.Count(), m_Own.Count()), LogLevel.NORMAL);
		}
		FinishActive(IA_BoardProtocol.STATUS_OK);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_ApiHandler when the request failed.
	void OnPageFailed()
	{
		if (!m_Active)
			return;
		FinishActive(IA_BoardProtocol.STATUS_FAILED);
	}

	//------------------------------------------------------------------------------------------------
	//! Read a /leaderboard answer into packed rows.
	//! \param[out] rows the page, one packed IA_BoardRow each
	//! \param[out] mine the asking player's own line, or the asking server's; empty when unranked
	//! \return false when the body is not a leaderboard page
	static bool ParseAnswer(string data, out string board, out string sortName, out bool descending, out int offset, out int total, notnull array<string> rows, notnull array<string> mine)
	{
		ref JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromString(data))
			return false;

		string dir;
		if (!ctx.ReadValue("board", board) || !ctx.ReadValue("sort", sortName) || !ctx.ReadValue("dir", dir))
			return false;
		if (!ctx.ReadValue("offset", offset) || !ctx.ReadValue("total", total))
			return false;
		descending = dir != "asc";

		ref array<ref IA_ApiBoardRow> pageRows = {};
		if (!ctx.ReadValue("rows", pageRows))
			return false;
		ref array<ref IA_ApiBoardRow> ownRows = {};
		ctx.ReadValue("me", ownRows);

		PackRows(pageRows, rows);
		PackRows(ownRows, mine);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tell a player their request got no page.
	//! \param status an IA_BoardProtocol.STATUS_ value other than OK
	void SendStatus(int playerId, int viewId, int offset, int status)
	{
		ref array<string> none = {};
		Send(playerId, viewId, status, 0, offset, none, "");
	}

	//------------------------------------------------------------------------------------------------
	//! Send a player one page: its head, then its rows.
	//! \param mine the player's own packed line, empty when they are not on the board
	void Send(int playerId, int viewId, int status, int total, int offset, notnull array<string> rows, string mine)
	{
#ifdef WORKBENCH
		if (m_bProbe || m_bProbeNote)
		{
			ProbeRecord(playerId, viewId, status, total, offset, rows, mine);
			return;
		}
#endif
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;
		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		if (controller)
			controller.IA_SendLeaderboardPage(viewId, status, total, offset, rows, mine);
	}

	//------------------------------------------------------------------------------------------------
	//! Serve what is held, wait on a request already out, or pay for a new one.
	//! \param waited the player has just waited on a request for this page, so what is held is no free answer
	protected void Resolve(int playerId, int viewId, int board, int sortKey, bool descending, int offset, bool waited)
	{
		int now = NowMs();
		string view = ViewKey(board, sortKey, descending);
		string guid = OwnGuid(board, playerId);
		string pageKey = PageKey(view, offset);
		string ownKey = OwnKey(view, guid);

		IA_BoardPage page = m_Pages.Get(pageKey);
		IA_BoardPage own = m_Own.Get(ownKey);
		bool pageFresh = IsFresh(page, board, now);
		bool ownFresh = IsFresh(own, board, now);
		// A player the server cannot name has no line of their own to look for.
		if (guid.IsEmpty() && board != IA_BoardProtocol.BOARD_SERVERS)
			ownFresh = true;

		if (pageFresh && ownFresh)
		{
			if (!waited)
				Bump(COUNT_SERVED_HELD);
			Serve(playerId, viewId, offset, page, own);
			return;
		}

		// With the page in hand only the player's own line is missing.
		string key = pageKey;
		if (pageFresh)
			key = ownKey;

		// A player waits on one request at a time, so one player cannot fill the queue.
		LeaveOthers(playerId, key);
		if (Join(key, playerId, viewId, offset))
		{
			Bump(COUNT_JOINED);
			return;
		}

		int waitMs = AllowanceWaitMs(playerId, now);
		if (waitMs > 0)
		{
			// The timed exchange brings these rows without a request of their own, and may be sooner.
			if (sortKey == IA_BoardProtocol.SORT_SCORE && descending && offset < IA_ApiTunables.SNAPSHOT_ROWS)
			{
				int syncMs = IA_ApiSync.GetInstance().WaitMs();
				if (syncMs > 0 && syncMs < waitMs)
					waitMs = syncMs;
			}
			// Nothing may be asked for now: rows already held beat no rows, whatever their age.
			if (page)
			{
				Bump(COUNT_SERVED_SHORT);
				Serve(playerId, viewId, offset, page, own);
				return;
			}
			Bump(COUNT_LIMITED);
			SendLimited(playerId, viewId, offset, waitMs);
			return;
		}

		if (m_aQueue.Count() >= QUEUE_MAX)
		{
			SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_BUSY);
			return;
		}

		ref IA_BoardFetch fetch = new IA_BoardFetch();
		fetch.m_sKey = key;
		fetch.m_iBoard = board;
		fetch.m_iSort = sortKey;
		fetch.m_bDescending = descending;
		fetch.m_iPayer = playerId;
		if (pageFresh)
		{
			fetch.m_iOffset = 0;
			fetch.m_iLimit = 0;
			fetch.m_bWantMine = true;
			fetch.m_sGuid = guid;
		}
		else
		{
			fetch.m_iOffset = offset;
			fetch.m_iLimit = IA_BoardProtocol.PAGE_ROWS;
			// The line of the player who caused the request comes with it at no further cost.
			fetch.m_bWantMine = !ownFresh;
			if (fetch.m_bWantMine)
				fetch.m_sGuid = guid;
		}
		fetch.AddWaiter(playerId, viewId, offset);

		m_Budget.Take();
		PlayerBudget(playerId).Take();
		m_aQueue.Insert(fetch);
		Pump();
	}

	//------------------------------------------------------------------------------------------------
	//! While somebody looks at a board in the order the menu opens on, the timed exchange keeps
	//! its first rows and that player's own line in hand. One look is good for the board's lifetime.
	protected void NoteLook(int playerId, int board, int sortKey, bool descending)
	{
		if (sortKey != IA_BoardProtocol.SORT_SCORE || !descending)
			return;

		int until = NowMs() + IA_ApiTunables.BoardLifeMs(board);
		m_aLookUntilMs[board] = until;
		m_aLooked[board] = true;

		string guid = OwnGuid(board, playerId);
		if (guid.IsEmpty())
			return;
		int held;
		if (m_mViewers.Find(guid, held) && held - until > 0)
			return;
		m_mViewers.Set(guid, until);
	}

	//------------------------------------------------------------------------------------------------
	//! Hold the first rows of a board as the pages players ask for.
	protected void FileSnapshot(int board, notnull IA_BoardPage snap)
	{
		string view = ViewKey(board, IA_BoardProtocol.SORT_SCORE, true);
		int count = snap.m_aRows.Count();
		int offset;
		while (offset == 0 || offset < count)
		{
			int end = offset + IA_BoardProtocol.PAGE_ROWS;
			if (end > count)
				end = count;
			// A page cut short by the end of the snapshot, not of the board, is not a page.
			if (end - offset < IA_BoardProtocol.PAGE_ROWS && count < snap.m_iTotal)
				break;

			ref IA_BoardPage page = new IA_BoardPage();
			page.m_iTotal = snap.m_iTotal;
			page.m_iStampMs = snap.m_iStampMs;
			for (int i = offset; i < end; i++)
			{
				page.m_aRows.Insert(snap.m_aRows[i]);
			}
			m_Pages.Put(PageKey(view, offset), page);
			offset = offset + IA_BoardProtocol.PAGE_ROWS;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hold where a player stands on a board by score. \p rows is empty for a player not on it.
	protected void FileOwn(int board, string guid, notnull array<string> rows, int now)
	{
		ref IA_BoardPage own = new IA_BoardPage();
		IA_BoardPage snap = m_aSnaps[board];
		if (snap)
			own.m_iTotal = snap.m_iTotal;
		own.m_iStampMs = now;
		foreach (string row : rows)
		{
			own.m_aRows.Insert(row);
		}
		m_Own.Put(OwnKey(ViewKey(board, IA_BoardProtocol.SORT_SCORE, true), guid), own);
	}

	//------------------------------------------------------------------------------------------------
	//! The mark of an own line that came with an exchange.
	protected string OwnMark(int board, string guid)
	{
		return board.ToString() + KEY_SEP + guid;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the IA_BoardProtocol.BOARD_ value of a stored board's name, -1 for any other
	protected int BoardOf(string name)
	{
		for (int board = IA_BoardProtocol.BOARD_SERVER; board < IA_BoardProtocol.BOARD_COUNT; board++)
		{
			if (IA_BoardProtocol.BoardName(board) == name)
				return board;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return milliseconds until this player may cause a request, 0 when one may go now
	protected int AllowanceWaitMs(int playerId, int now)
	{
		int wait = m_Budget.WaitMs(now, IA_ApiTunables.BudgetPerHour(), IA_ApiTunables.BudgetBurst());
		int playerWait = PlayerBudget(playerId).WaitMs(now, IA_ApiTunables.PlayerPerHour(), IA_ApiTunables.PlayerBurst());
		if (playerWait > wait)
			wait = playerWait;
		return wait;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_BoardBucket PlayerBudget(int playerId)
	{
		IA_BoardBucket held = m_mPlayerBudgets.Get(playerId);
		if (held)
			return held;

		if (m_mPlayerBudgets.Count() >= PLAYERS_KEPT)
			ForgetIdlePlayers();
		ref IA_BoardBucket made = new IA_BoardBucket();
		m_mPlayerBudgets.Set(playerId, made);
		return made;
	}

	//------------------------------------------------------------------------------------------------
	//! An allowance that has filled up again says nothing a new one would not.
	protected void ForgetIdlePlayers()
	{
		int now = NowMs();
		int perHour = IA_ApiTunables.PlayerPerHour();
		int burst = IA_ApiTunables.PlayerBurst();
		ref array<int> idle = {};
		foreach (int playerId, IA_BoardBucket bucket : m_mPlayerBudgets)
		{
			if (bucket.IsFull(now, perHour, burst))
				idle.Insert(playerId);
		}
		foreach (int gone : idle)
		{
			m_mPlayerBudgets.Remove(gone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A request that was paid for and never sent costs nothing.
	protected void GiveBack(notnull IA_BoardFetch fetch)
	{
		m_Budget.GiveBack(IA_ApiTunables.BudgetBurst());
		IA_BoardBucket payer = m_mPlayerBudgets.Get(fetch.m_iPayer);
		if (payer)
			payer.GiveBack(IA_ApiTunables.PlayerBurst());
	}

	//------------------------------------------------------------------------------------------------
	//! The player has asked for something new. A request already sent still completes, for the
	//! cache; one still waiting to go that nobody else wants is dropped.
	protected void LeaveOthers(int playerId, string key)
	{
		if (m_Active && m_Active.m_sKey != key)
			m_Active.DropWaiter(playerId);
		for (int q = m_aQueue.Count() - 1; q >= 0; q--)
		{
			IA_BoardFetch queued = m_aQueue[q];
			if (queued.m_sKey == key)
				continue;
			if (!queued.DropWaiter(playerId))
				continue;
			GiveBack(queued);
			m_aQueue.RemoveOrdered(q);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a request under this key is out or waiting and the player now waits on it
	protected bool Join(string key, int playerId, int viewId, int offset)
	{
		if (m_Active && m_Active.m_sKey == key)
		{
			m_Active.AddWaiter(playerId, viewId, offset);
			return true;
		}
		foreach (IA_BoardFetch queued : m_aQueue)
		{
			if (queued.m_sKey != key)
				continue;
			queued.AddWaiter(playerId, viewId, offset);
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! One request is out at a time.
	protected void Pump()
	{
		while (!m_Active && !m_aQueue.IsEmpty())
		{
			m_Active = m_aQueue[0];
			m_aQueue.RemoveOrdered(0);

			if (SendFetch(m_Active))
			{
				if (m_Active.m_iLimit > 0)
					Bump(COUNT_FETCH_PAGE);
				else
					Bump(COUNT_FETCH_OWN);
				GetGame().GetCallqueue().CallLater(this.OnFetchTimeout, FETCH_TIMEOUT_MS, false);
				return;
			}

			// No server GUID: nothing can be asked, so nothing was spent and nobody waits.
			ref IA_BoardFetch unsent = m_Active;
			m_Active = null;
			GiveBack(unsent);
			AnswerWaiters(unsent, IA_BoardProtocol.STATUS_OFFLINE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when nothing was sent
	protected bool SendFetch(notnull IA_BoardFetch fetch)
	{
#ifdef WORKBENCH
		if (m_bProbe)
		{
			GetGame().GetCallqueue().CallLater(this.ProbeDeliver, PROBE_ANSWER_MS, false);
			return true;
		}
#endif
		string guid;
		if (fetch.m_bWantMine)
			guid = fetch.m_sGuid;
		string board = IA_BoardProtocol.BoardName(fetch.m_iBoard);
		string sortName = IA_BoardProtocol.SortName(fetch.m_iSort);
		return IA_ApiHandler.GetInstance().FetchLeaderboardPage(board, sortName, fetch.m_bDescending, fetch.m_iOffset, fetch.m_iLimit, guid);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFetchTimeout()
	{
		if (!m_Active)
			return;
		Print("[IA][Leaderboard] The stats service did not answer a page request in time.", LogLevel.WARNING);
		FinishActive(IA_BoardProtocol.STATUS_FAILED);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishActive(int status)
	{
		GetGame().GetCallqueue().Remove(this.OnFetchTimeout);
		ref IA_BoardFetch done = m_Active;
		m_Active = null;
		if (done)
			AnswerWaiters(done, status);
		Pump();
	}

	//------------------------------------------------------------------------------------------------
	protected void AnswerWaiters(notnull IA_BoardFetch fetch, int status)
	{
		int count = fetch.m_aPlayers.Count();
		for (int i = 0; i < count; i++)
		{
			int playerId = fetch.m_aPlayers[i];
			int viewId = fetch.m_aViews[i];
			int offset = fetch.m_aOffsets[i];
			if (status == IA_BoardProtocol.STATUS_OK)
			{
				// What arrived is held now. A player whose own line did not come with it asks for that next.
				Resolve(playerId, viewId, fetch.m_iBoard, fetch.m_iSort, fetch.m_bDescending, offset, true);
				continue;
			}

			// No answer: a page held from before is still better than none.
			if (ServeHeld(playerId, viewId, fetch.m_iBoard, fetch.m_iSort, fetch.m_bDescending, offset))
				continue;
			SendStatus(playerId, viewId, offset, status);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a page was held, of any age, and went out
	protected bool ServeHeld(int playerId, int viewId, int board, int sortKey, bool descending, int offset)
	{
		string view = ViewKey(board, sortKey, descending);
		IA_BoardPage page = m_Pages.Get(PageKey(view, offset));
		if (!page)
			return false;

		Bump(COUNT_SERVED_SHORT);
		Serve(playerId, viewId, offset, page, m_Own.Get(OwnKey(view, OwnGuid(board, playerId))));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param own the asking player's own line, null when none is held
	protected void Serve(int playerId, int viewId, int offset, notnull IA_BoardPage page, IA_BoardPage own)
	{
		string mine;
		if (own && !own.m_aRows.IsEmpty())
			mine = own.m_aRows[0];
		Send(playerId, viewId, IA_BoardProtocol.STATUS_OK, page.m_iTotal, offset, page.m_aRows, mine);
	}

	//------------------------------------------------------------------------------------------------
	//! Refuse a page because the allowance is spent. The seconds to wait go where a page's total would.
	protected void SendLimited(int playerId, int viewId, int offset, int waitMs)
	{
		int seconds = (waitMs + 999) / 1000;
		seconds = Math.ClampInt(seconds, 1, WAIT_MAX_S);
		ref array<string> none = {};
		Send(playerId, viewId, IA_BoardProtocol.STATUS_LIMITED, seconds, offset, none, "");
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsFresh(IA_BoardPage page, int board, int now)
	{
		if (!page)
			return false;
		return now - page.m_iStampMs <= IA_ApiTunables.BoardLifeMs(board);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLinked()
	{
#ifdef WORKBENCH
		if (m_bProbe)
			return true;
#endif
		return IA_ApiHandler.GetInstance().IsLinked();
	}

	//------------------------------------------------------------------------------------------------
	//! The clock answers are stamped with and the allowance fills by.
	protected int NowMs()
	{
		int now = System.GetTickCount();
#ifdef WORKBENCH
		now = now + m_iProbeSkewMs;
#endif
		return now;
	}

	//------------------------------------------------------------------------------------------------
	protected void Bump(int kind)
	{
		m_aCounts[kind] = m_aCounts[kind] + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected string ViewKey(int board, int sortKey, bool descending)
	{
		string key = board.ToString() + KEY_SEP + sortKey.ToString();
		if (descending)
			return key + KEY_SEP + "d";
		return key + KEY_SEP + "a";
	}

	//------------------------------------------------------------------------------------------------
	//! A page's key names no player, so every player asking for the page shares it.
	protected string PageKey(string view, int offset)
	{
		return view + KEY_SEP + offset.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected string OwnKey(string view, string guid)
	{
		return view + KEY_SEP + KEY_OWN + KEY_SEP + guid;
	}

	//------------------------------------------------------------------------------------------------
	//! Whose own line a board shows: the player's, or on the board of servers this server's.
	protected string OwnGuid(int board, int playerId)
	{
		if (board == IA_BoardProtocol.BOARD_SERVERS)
			return "";
#ifdef WORKBENCH
		if (m_bProbe)
			return "probe-" + playerId.ToString();
		string probeIdentity;
		if (m_mProbeIdentity.Find(playerId, probeIdentity))
			return probeIdentity;
#endif
		return SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PackRows(notnull array<ref IA_ApiBoardRow> source, notnull array<string> packed)
	{
		packed.Clear();
		foreach (IA_ApiBoardRow row : source)
		{
			if (!row)
				continue;
			packed.Insert(IA_BoardRow.Pack(row.r, row.n, row.k, row.d, row.h, row.g, row.o, row.s, row.t, row.i, row.p, 0));
		}
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: answer every request from a made-up board of \p total rows. Nothing reaches the
	//! stats service, players are named probe-<id>, and answers are noted instead of sent; those
	//! for PROBE_MENU_PLAYER also go to the open leaderboard menu.
	void ProbeBegin(int total)
	{
		m_bProbe = true;
		m_iProbeTotal = total;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: ask the real stats service, but note the answers instead of sending them to
	//! players, as ProbeBegin does. For a run against the local development server.
	void ProbeNote()
	{
		m_bProbeNote = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the identity a player goes by, for a run against the local development server.
	void ProbeIdentity(int playerId, string guid)
	{
		m_mProbeIdentity.Set(playerId, guid);
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the tag of the rows the timed exchange brought for a board, empty when none are held.
	string ProbeEtag(int board)
	{
		if (!m_aSnaps[board])
			return "";
		return m_aEtags[board];
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the name on the own line last sent to a player, empty when it had none.
	string ProbeMineName(int playerId)
	{
		string mine;
		m_mProbeMine.Find(playerId, mine);
		IA_BoardRow row = IA_BoardRow.Unpack(mine);
		if (!row)
			return "";
		return row.m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the kills on the own line last sent to a player, -1 when it had none.
	int ProbeMineKills(int playerId)
	{
		string mine;
		m_mProbeMine.Find(playerId, mine);
		IA_BoardRow row = IA_BoardRow.Unpack(mine);
		if (!row)
			return -1;
		return row.m_iKills;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: move this service's clock on.
	void ProbeAdvance(int ms)
	{
		m_iProbeSkewMs = m_iProbeSkewMs + ms;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the next \p count requests get no answer from the made-up stats service.
	void ProbeFailNext(int count)
	{
		m_iProbeFailures = count;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: spend the server's allowance and every player's.
	void ProbeDrain()
	{
		int now = NowMs();
		m_Budget.ProbeDrain(now);
		foreach (int playerId, IA_BoardBucket bucket : m_mPlayerBudgets)
		{
			bucket.ProbeDrain(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: true when no request is out and none waits to go.
	bool ProbeIdle()
	{
		return !m_Active && m_aQueue.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: forget the answer noted for a player, so the next one is told apart.
	void ProbeForget(int playerId)
	{
		m_mProbeStatus.Remove(playerId);
		m_mProbeTotal.Remove(playerId);
		m_mProbeRows.Remove(playerId);
		m_mProbeMine.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the status last sent to a player, -1 when nothing has been sent.
	int ProbeStatus(int playerId)
	{
		int status;
		if (!m_mProbeStatus.Find(playerId, status))
			return -1;
		return status;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the total last sent to a player; on a limited refusal, the seconds to wait.
	int ProbeAnswerTotal(int playerId)
	{
		int total;
		m_mProbeTotal.Find(playerId, total);
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: how many rows the page last sent to a player held.
	int ProbeRows(int playerId)
	{
		int rows;
		m_mProbeRows.Find(playerId, rows);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the place on the own line last sent to a player, 0 when it had none.
	int ProbeMineRank(int playerId)
	{
		string mine;
		m_mProbeMine.Find(playerId, mine);
		IA_BoardRow row = IA_BoardRow.Unpack(mine);
		if (!row)
			return 0;
		return row.m_iRank;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the place the made-up board gives a player; the board of servers gives this server 3.
	int ProbeRankOf(int playerId)
	{
		return playerId * 7;
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeDeliver()
	{
		if (!m_Active)
			return;
		if (m_iProbeFailures > 0)
		{
			m_iProbeFailures = m_iProbeFailures - 1;
			OnPageFailed();
			return;
		}
		OnPageReceived(ProbeBody(m_Active));
	}

	//------------------------------------------------------------------------------------------------
	//! The body the stats service would send for a request, read back through ParseAnswer.
	protected string ProbeBody(notnull IA_BoardFetch fetch)
	{
		string dir = "asc";
		if (fetch.m_bDescending)
			dir = "desc";

		string body = "{\"board\":\"" + IA_BoardProtocol.BoardName(fetch.m_iBoard) + "\",";
		body = body + "\"sort\":\"" + IA_BoardProtocol.SortName(fetch.m_iSort) + "\",\"dir\":\"" + dir + "\",";
		body = body + "\"offset\":" + fetch.m_iOffset.ToString() + ",\"total\":" + m_iProbeTotal.ToString() + ",\"rows\":[";

		int end = fetch.m_iOffset + fetch.m_iLimit;
		if (end > m_iProbeTotal)
			end = m_iProbeTotal;
		int rank;
		for (int i = fetch.m_iOffset; i < end; i++)
		{
			if (i > fetch.m_iOffset)
				body = body + ",";
			rank = i + 1;
			body = body + ProbeRow(rank, "Fixture " + rank.ToString());
		}

		body = body + "],\"me\":[";
		if (fetch.m_bWantMine)
		{
			rank = 3;
			if (!fetch.m_sGuid.IsEmpty())
				rank = ProbeRankOf(fetch.m_sGuid.Substring(6, fetch.m_sGuid.Length() - 6).ToInt());
			if (rank >= 1 && rank <= m_iProbeTotal)
				body = body + ProbeRow(rank, "Probe " + rank.ToString());
		}
		return body + "]}";
	}

	//------------------------------------------------------------------------------------------------
	//! One made-up row. Every number falls as the place does.
	protected string ProbeRow(int rank, string name)
	{
		int left = m_iProbeTotal - rank + 1;
		string row = string.Format("{\"r\":%1,\"n\":\"%2\",\"k\":%3,\"d\":%4,\"h\":%5,\"g\":%6,", rank, name, left * 3, left, left / 10, left / 5);
		return row + string.Format("\"o\":%1,\"s\":%2,\"t\":%3,\"i\":%4,\"p\":%5}", left * 2, left * 40, left * 2, left / 3, left);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeRecord(int playerId, int viewId, int status, int total, int offset, notnull array<string> rows, string mine)
	{
		m_mProbeStatus.Set(playerId, status);
		m_mProbeTotal.Set(playerId, total);
		m_mProbeRows.Set(playerId, rows.Count());
		m_mProbeMine.Set(playerId, mine);
		if (playerId != PROBE_MENU_PLAYER)
			return;

		// The menu takes the answer as the player controller hands it over.
		IA_StatisticsMenu.OnBoardHead(viewId, status, total, offset, mine);
		if (status != IA_BoardProtocol.STATUS_OK)
			return;

		ref array<string> chunks = {};
		ref array<int> firsts = {};
		IA_BoardProtocol.SplitRows(rows, offset, chunks, firsts);
		int last = chunks.Count() - 1;
		for (int i = 0; i <= last; i++)
		{
			IA_StatisticsMenu.OnBoardRows(viewId, firsts[i], i == last, chunks[i]);
		}
	}
#endif
}
