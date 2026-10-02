//------------------------------------------------------------------------------------------------
//! What the leaderboard menu and the server agree on: the boards, the sort keys, the page size
//! and the answers a page request can get. A board is asked for a page at a time and sent as
//! short strings, so its size on the wire does not grow with the number of players.
//------------------------------------------------------------------------------------------------
class IA_BoardProtocol
{
	static const int BOARD_SESSION = 0;
	static const int BOARD_SERVER = 1;
	static const int BOARD_GLOBAL = 2;
	static const int BOARD_SERVERS = 3;
	static const int BOARD_COUNT = 4;

	static const int SORT_SCORE = 0;
	static const int SORT_KILLS = 1;
	static const int SORT_DEATHS = 2;
	static const int SORT_KD = 3;
	static const int SORT_HVT = 4;
	static const int SORT_GUARD = 5;
	static const int SORT_OBJ = 6;
	static const int SORT_TRANSPORT = 7;
	static const int SORT_INSERTIONS = 8;
	static const int SORT_PLAYERS = 9;
	static const int SORT_COUNT = 10;

	static const int STATUS_OK = 0;
	static const int STATUS_FAILED = 1;		// the stats service did not answer
	static const int STATUS_OFFLINE = 2;	// this server has no stats service to ask
	static const int STATUS_BUSY = 3;		// asked too fast, or too many requests are waiting

	static const int PAGE_ROWS = 25;
	static const int MAX_OFFSET = 1000000;

	//! Longest packed-rows string put in one RPC. A row is about 60 characters, 130 at most.
	static const int CHUNK_CHARS = 700;

	//------------------------------------------------------------------------------------------------
	static bool IsBoard(int board)
	{
		return board >= 0 && board < BOARD_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSort(int sortKey)
	{
		return sortKey >= 0 && sortKey < SORT_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the board's name in the /leaderboard route; the session board has none
	static string BoardName(int board)
	{
		if (board == BOARD_SERVER)
			return "server";
		if (board == BOARD_GLOBAL)
			return "global";
		if (board == BOARD_SERVERS)
			return "servers";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return the sort key's name in the /leaderboard route
	static string SortName(int sortKey)
	{
		if (sortKey == SORT_KILLS)
			return "kills";
		if (sortKey == SORT_DEATHS)
			return "deaths";
		if (sortKey == SORT_KD)
			return "kd";
		if (sortKey == SORT_HVT)
			return "hvt";
		if (sortKey == SORT_GUARD)
			return "guard";
		if (sortKey == SORT_OBJ)
			return "obj";
		if (sortKey == SORT_TRANSPORT)
			return "transport";
		if (sortKey == SORT_INSERTIONS)
			return "insertions";
		if (sortKey == SORT_PLAYERS)
			return "players";
		return "score";
	}

	//------------------------------------------------------------------------------------------------
	//! \return the first row of the page that holds this row
	static int PageOffset(int index)
	{
		if (index < 0)
			return 0;
		return index - index % PAGE_ROWS;
	}
}
