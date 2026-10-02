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
	//! The server has made all the requests to the stats service it may for now and holds no
	//! rows for the page. The answer's total is the seconds until it is worth asking again.
	static const int STATUS_LIMITED = 4;

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

	//------------------------------------------------------------------------------------------------
	//! Join a page's packed rows into strings short enough for one RPC each.
	//! \param offset index on the board of the first row
	//! \param[out] chunks rows joined by line breaks; always at least one, empty for a page of no rows
	//! \param[out] firsts index on the board of each chunk's first row
	static void SplitRows(notnull array<string> rows, int offset, notnull array<string> chunks, notnull array<int> firsts)
	{
		chunks.Clear();
		firsts.Clear();

		string chunk;
		int chunkRows;
		int first = offset;
		int count = rows.Count();
		for (int i = 0; i < count; i++)
		{
			string line = rows[i];
			if (chunkRows > 0 && chunk.Length() + line.Length() >= CHUNK_CHARS)
			{
				chunks.Insert(chunk);
				firsts.Insert(first);
				chunk = "";
				chunkRows = 0;
				first = offset + i;
			}
			if (chunkRows > 0)
				chunk = chunk + "\n";
			chunk = chunk + line;
			chunkRows = chunkRows + 1;
		}
		chunks.Insert(chunk);
		firsts.Insert(first);
	}
}
