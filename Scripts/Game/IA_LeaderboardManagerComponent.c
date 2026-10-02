//------------------------------------------------------------------------------------------------
//! Server: answers a player's request for one page of a leaderboard.
//!
//! Nothing is replicated. The menu asks through the player's controller for a board, a sort
//! order and an offset; the answer goes back to that one player as a few short RPCs
//! (see SCR_PlayerController.IA_SendLeaderboardPage). The session board is read from RAM, the
//! others from the stats service one page at a time. Answers are kept for a short while so
//! players reading the same board share a request, and requests go out one at a time.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Serves leaderboard pages to players on request.")]
class IA_LeaderboardManagerComponentClass : ScriptComponentClass
{
}

class IA_LeaderboardManagerComponent : ScriptComponent
{
	protected static const int CACHE_TTL_MS = 45000;
	protected static const int CACHE_MAX = 400;
	protected static const int QUEUE_MAX = 32;
	protected static const int FETCH_TIMEOUT_MS = 20000;
	protected static const string KEY_SEP = "|";

	protected ref map<string, ref IA_BoardPage> m_mPages = new map<string, ref IA_BoardPage>();
	protected ref map<string, ref IA_BoardPage> m_mMine = new map<string, ref IA_BoardPage>();
	protected ref array<ref IA_BoardFetch> m_aQueue = {};
	protected ref IA_BoardFetch m_Active;
	protected static IA_LeaderboardManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static IA_LeaderboardManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (s_Instance && s_Instance != this)
		{
			Print("[IA][Leaderboard] Instance already exists.", LogLevel.WARNING);
			return;
		}
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(this.OnFetchTimeout);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Answer one player's request. Every argument comes from a client, so all are checked here.
	//! \param viewId the client's tag for this board and sort order, sent back with the answer
	void Request(int playerId, int viewId, int board, int sortKey, bool descending, int offset)
	{
		if (!Replication.IsServer())
			return;

		if (!IA_BoardProtocol.IsBoard(board) || !IA_BoardProtocol.IsSort(sortKey) || offset < 0 || offset > IA_BoardProtocol.MAX_OFFSET)
		{
			SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_FAILED);
			return;
		}

		int pageOffset = IA_BoardProtocol.PageOffset(offset);
		if (board == IA_BoardProtocol.BOARD_SESSION)
		{
			ServeSession(playerId, viewId, sortKey, descending, pageOffset);
			return;
		}

		if (TryServe(playerId, viewId, board, sortKey, descending, pageOffset))
			return;

		Enqueue(playerId, viewId, board, sortKey, descending, pageOffset);
	}

	//------------------------------------------------------------------------------------------------
	//! The stored scores changed; what is cached no longer matches them.
	void InvalidateCache()
	{
		m_mPages.Clear();
		m_mMine.Clear();
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

		if (m_mPages.Count() + m_mMine.Count() > CACHE_MAX)
			InvalidateCache();

		int now = System.GetTickCount();
		string view = ViewKey(m_Active.m_iBoard, m_Active.m_iSort, m_Active.m_bDescending);
		if (m_Active.m_iLimit > 0)
		{
			ref IA_BoardPage page = new IA_BoardPage();
			page.m_iTotal = total;
			page.m_iStampMs = now;
			page.m_aRows = rows;
			m_mPages.Set(view + KEY_SEP + m_Active.m_iOffset.ToString(), page);
		}
		if (m_Active.m_bWantMine)
		{
			ref IA_BoardPage own = new IA_BoardPage();
			own.m_iTotal = total;
			own.m_iStampMs = now;
			own.m_aRows = mine;
			m_mMine.Set(view + KEY_SEP + m_Active.m_sGuid, own);
		}

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][Leaderboard] Page received: board %1, sort %2, offset %3, %4 rows of %5.", board, sortName, offset, rows.Count(), total), LogLevel.NORMAL);
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

	//------------------------------------------------------------------------------------------------
	protected void ServeSession(int playerId, int viewId, int sortKey, bool descending, int offset)
	{
		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (!session)
		{
			SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_OFFLINE);
			return;
		}

		ref array<string> rows = {};
		string mine;
		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		int total = session.BuildBoardPage(sortKey, descending, offset, IA_BoardProtocol.PAGE_ROWS, guid, playerId, rows, mine);
		Send(playerId, viewId, IA_BoardProtocol.STATUS_OK, total, offset, rows, mine);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the page and the player's own line were both at hand and went out
	protected bool TryServe(int playerId, int viewId, int board, int sortKey, bool descending, int offset)
	{
		string view = ViewKey(board, sortKey, descending);
		IA_BoardPage page = Fresh(m_mPages, view + KEY_SEP + offset.ToString());
		IA_BoardPage own = Fresh(m_mMine, view + KEY_SEP + MineGuid(board, playerId));
		if (!page || !own)
			return false;

		string mine;
		if (!own.m_aRows.IsEmpty())
			mine = own.m_aRows[0];
		Send(playerId, viewId, IA_BoardProtocol.STATUS_OK, page.m_iTotal, offset, page.m_aRows, mine);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Enqueue(int playerId, int viewId, int board, int sortKey, bool descending, int offset)
	{
		string view = ViewKey(board, sortKey, descending);
		string guid = MineGuid(board, playerId);
		bool havePage = Fresh(m_mPages, view + KEY_SEP + offset.ToString()) != null;
		bool haveMine = Fresh(m_mMine, view + KEY_SEP + guid) != null;

		int limit = IA_BoardProtocol.PAGE_ROWS;
		if (havePage)
			limit = 0;

		string key = view + KEY_SEP + offset.ToString() + KEY_SEP + limit.ToString();
		if (!haveMine)
			key = key + KEY_SEP + "me" + KEY_SEP + guid;

		// A player waits on one page at a time, so one player cannot fill the queue. A request
		// already sent still completes, for the cache.
		if (m_Active && m_Active.m_sKey != key)
			m_Active.DropWaiter(playerId);
		for (int q = m_aQueue.Count() - 1; q >= 0; q--)
		{
			if (m_aQueue[q].m_sKey == key)
				continue;
			if (m_aQueue[q].DropWaiter(playerId))
				m_aQueue.RemoveOrdered(q);
		}

		if (m_Active && m_Active.m_sKey == key)
		{
			m_Active.AddWaiter(playerId, viewId);
			return;
		}
		foreach (IA_BoardFetch queued : m_aQueue)
		{
			if (queued.m_sKey != key)
				continue;
			queued.AddWaiter(playerId, viewId);
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
		fetch.m_iOffset = offset;
		fetch.m_iLimit = limit;
		fetch.m_bWantMine = !haveMine;
		fetch.m_sGuid = guid;
		fetch.AddWaiter(playerId, viewId);
		m_aQueue.Insert(fetch);
		Pump();
	}

	//------------------------------------------------------------------------------------------------
	protected void Pump()
	{
		while (!m_Active && !m_aQueue.IsEmpty())
		{
			m_Active = m_aQueue[0];
			m_aQueue.RemoveOrdered(0);

			string guid;
			if (m_Active.m_bWantMine)
				guid = m_Active.m_sGuid;
			string board = IA_BoardProtocol.BoardName(m_Active.m_iBoard);
			string sortName = IA_BoardProtocol.SortName(m_Active.m_iSort);
			if (IA_ApiHandler.GetInstance().FetchLeaderboardPage(board, sortName, m_Active.m_bDescending, m_Active.m_iOffset, m_Active.m_iLimit, guid))
			{
				GetGame().GetCallqueue().CallLater(this.OnFetchTimeout, FETCH_TIMEOUT_MS, false);
				return;
			}

			// No server GUID yet: nothing can be asked, so nobody waits.
			AnswerWaiters(m_Active, IA_BoardProtocol.STATUS_OFFLINE);
			m_Active = null;
		}
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
			if (status == IA_BoardProtocol.STATUS_OK && TryServe(playerId, viewId, fetch.m_iBoard, fetch.m_iSort, fetch.m_bDescending, fetch.m_iOffset))
				continue;

			// The service answered, but the page it leaned on has since left the cache: ask again.
			int answer = status;
			if (answer == IA_BoardProtocol.STATUS_OK)
				answer = IA_BoardProtocol.STATUS_BUSY;
			SendStatus(playerId, viewId, fetch.m_iOffset, answer);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IA_BoardPage Fresh(notnull map<string, ref IA_BoardPage> cache, string key)
	{
		IA_BoardPage page = cache.Get(key);
		if (!page)
			return null;
		if (System.GetTickCount() - page.m_iStampMs > CACHE_TTL_MS)
			return null;
		return page;
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
	//! Whose own line a board shows: the player's, or on the board of servers this server's.
	protected string MineGuid(int board, int playerId)
	{
		if (board == IA_BoardProtocol.BOARD_SERVERS)
			return "";
		return SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendStatus(int playerId, int viewId, int offset, int status)
	{
		ref array<string> none = {};
		Send(playerId, viewId, status, 0, offset, none, "");
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(int playerId, int viewId, int status, int total, int offset, notnull array<string> rows, string mine)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;
		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		if (controller)
			controller.IA_SendLeaderboardPage(viewId, status, total, offset, rows, mine);
	}
}
