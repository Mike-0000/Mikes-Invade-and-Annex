//------------------------------------------------------------------------------------------------
//! Server: answers a player's request for one page of a leaderboard.
//!
//! Nothing is replicated. The menu asks through the player's controller for a board, a sort
//! order and an offset; the answer goes back to that one player as a few short RPCs
//! (see SCR_PlayerController.IA_SendLeaderboardPage). The session board is read from RAM. The
//! others come from the stats service through IA_BoardService, which holds what was answered
//! and limits how often the service is asked.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Serves leaderboard pages to players on request.")]
class IA_LeaderboardManagerComponentClass : ScriptComponentClass
{
}

class IA_LeaderboardManagerComponent : ScriptComponent
{
	protected ref IA_BoardService m_Service;
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
		if (m_Service)
			m_Service.Close();
		m_Service = null;
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

		IA_BoardService service = Service();
		if (!IA_BoardProtocol.IsBoard(board) || !IA_BoardProtocol.IsSort(sortKey) || offset < 0 || offset > IA_BoardProtocol.MAX_OFFSET)
		{
			service.SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_FAILED);
			return;
		}

		int pageOffset = IA_BoardProtocol.PageOffset(offset);
		if (board == IA_BoardProtocol.BOARD_SESSION)
		{
			ServeSession(playerId, viewId, sortKey, descending, pageOffset);
			return;
		}

		service.Request(playerId, viewId, board, sortKey, descending, pageOffset);
	}

	//------------------------------------------------------------------------------------------------
	//! The service is made by the first request, which only a server gets.
	protected IA_BoardService Service()
	{
		if (!m_Service)
		{
			m_Service = new IA_BoardService();
			m_Service.Open();
		}
		return m_Service;
	}

	//------------------------------------------------------------------------------------------------
	protected void ServeSession(int playerId, int viewId, int sortKey, bool descending, int offset)
	{
		IA_BoardService service = Service();
		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (!session)
		{
			service.SendStatus(playerId, viewId, offset, IA_BoardProtocol.STATUS_OFFLINE);
			return;
		}

		ref array<string> rows = {};
		string mine;
		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		int total = session.BuildBoardPage(sortKey, descending, offset, IA_BoardProtocol.PAGE_ROWS, guid, playerId, rows, mine);
		service.Send(playerId, viewId, IA_BoardProtocol.STATUS_OK, total, offset, rows, mine);
	}
}
