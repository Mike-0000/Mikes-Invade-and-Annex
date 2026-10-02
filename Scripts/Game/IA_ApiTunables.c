//------------------------------------------------------------------------------------------------
//! How often this server talks to the stats service, in one place. Every value has a default
//! and a range. A value outside its range is brought into it, and zero or less leaves what is
//! set, so nothing the stats service suggests can switch a limit off.
//!
//! A client's menu reads the board lifetimes here too, at their defaults, to time its refresh.
//------------------------------------------------------------------------------------------------
class IA_ApiTunables
{
	//! Seconds a page or an own line of this server's board is served before it is fetched again.
	static const int LIFE_SERVER_S = 120;
	static const int LIFE_SERVER_MIN_S = 30;
	static const int LIFE_SERVER_MAX_S = 3600;

	//! The same for the boards all servers share: every player, and the servers themselves.
	static const int LIFE_WIDE_S = 300;
	static const int LIFE_WIDE_MIN_S = 60;
	static const int LIFE_WIDE_MAX_S = 3600;

	//! /leaderboard requests this server may send in an hour once the burst is spent.
	static const int BUDGET_PER_HOUR = 12;
	static const int BUDGET_PER_HOUR_MIN = 6;
	static const int BUDGET_PER_HOUR_MAX = 600;

	//! /leaderboard requests it may send at once after a quiet spell.
	static const int BUDGET_BURST = 20;
	static const int BUDGET_BURST_MIN = 1;
	static const int BUDGET_BURST_MAX = 60;

	//! The part of the hourly budget and of the burst one player may use, in percent.
	static const int PLAYER_SHARE = 50;
	static const int PLAYER_SHARE_MIN = 10;
	static const int PLAYER_SHARE_MAX = 100;

	//! Seconds between the timed exchanges with the stats service.
	static const int SYNC_INTERVAL_S = 60;
	static const int SYNC_INTERVAL_MIN_S = 30;
	static const int SYNC_INTERVAL_MAX_S = 600;

	//! Seconds that must pass after an exchange before one may go early, for a rating a player waits on.
	static const int SYNC_HURRY_GAP_S = 15;
	//! Seconds an exchange may stay unanswered before it counts as lost.
	static const int SYNC_TIMEOUT_S = 90;
	//! The longest wait between attempts while the stats service keeps failing.
	static const int SYNC_BACKOFF_MAX_S = 1800;
	//! Seconds between tries of the one exchange on a stats service that does not have it.
	static const int SYNC_REPROBE_S = 3600;

	//! Stats events in one batch, and events kept while batches cannot be delivered.
	static const int STATS_BATCH_EVENTS = 1000;
	static const int STATS_QUEUE_EVENTS = 20000;
	//! Players in one transport batch, rating lookups and own lines in one exchange.
	static const int TRANSPORT_BATCH_ENTRIES = 512;
	static const int RATING_IDS = 256;
	static const int OWN_LINE_PLAYERS = 128;
	//! Rows of a board that come with an exchange, in the order the menu opens on.
	static const int SNAPSHOT_ROWS = 100;

	//! Answers kept before the one held longest is dropped: pages, and players' own lines.
	static const int CACHE_PAGES = 240;
	static const int CACHE_OWN_LINES = 400;

	protected static int s_iLifeServerS = LIFE_SERVER_S;
	protected static int s_iLifeWideS = LIFE_WIDE_S;
	protected static int s_iBudgetPerHour = BUDGET_PER_HOUR;
	protected static int s_iBudgetBurst = BUDGET_BURST;
	protected static int s_iPlayerShare = PLAYER_SHARE;
	protected static int s_iSyncIntervalS = SYNC_INTERVAL_S;

	//------------------------------------------------------------------------------------------------
	//! \param board an IA_BoardProtocol.BOARD_ value
	static int BoardLifeS(int board)
	{
		if (board == IA_BoardProtocol.BOARD_SERVER)
			return s_iLifeServerS;
		return s_iLifeWideS;
	}

	//------------------------------------------------------------------------------------------------
	static int BoardLifeMs(int board)
	{
		return BoardLifeS(board) * 1000;
	}

	//------------------------------------------------------------------------------------------------
	static int BudgetPerHour()
	{
		return s_iBudgetPerHour;
	}

	//------------------------------------------------------------------------------------------------
	static int BudgetBurst()
	{
		return s_iBudgetBurst;
	}

	//------------------------------------------------------------------------------------------------
	//! \return requests one player may cause in an hour, at least one
	static int PlayerPerHour()
	{
		return Share(s_iBudgetPerHour);
	}

	//------------------------------------------------------------------------------------------------
	//! \return requests one player may cause at once, at least one
	static int PlayerBurst()
	{
		return Share(s_iBudgetBurst);
	}

	//------------------------------------------------------------------------------------------------
	static int SyncIntervalS()
	{
		return s_iSyncIntervalS;
	}

	//------------------------------------------------------------------------------------------------
	static void SetBoardLives(int serverS, int wideS)
	{
		s_iLifeServerS = Within(serverS, s_iLifeServerS, LIFE_SERVER_MIN_S, LIFE_SERVER_MAX_S);
		s_iLifeWideS = Within(wideS, s_iLifeWideS, LIFE_WIDE_MIN_S, LIFE_WIDE_MAX_S);
	}

	//------------------------------------------------------------------------------------------------
	static void SetBudget(int perHour, int burst)
	{
		s_iBudgetPerHour = Within(perHour, s_iBudgetPerHour, BUDGET_PER_HOUR_MIN, BUDGET_PER_HOUR_MAX);
		s_iBudgetBurst = Within(burst, s_iBudgetBurst, BUDGET_BURST_MIN, BUDGET_BURST_MAX);
	}

	//------------------------------------------------------------------------------------------------
	static void SetPlayerShare(int percent)
	{
		s_iPlayerShare = Within(percent, s_iPlayerShare, PLAYER_SHARE_MIN, PLAYER_SHARE_MAX);
	}

	//------------------------------------------------------------------------------------------------
	static void SetSyncInterval(int seconds)
	{
		s_iSyncIntervalS = Within(seconds, s_iSyncIntervalS, SYNC_INTERVAL_MIN_S, SYNC_INTERVAL_MAX_S);
	}

	//------------------------------------------------------------------------------------------------
	//! Take what the stats service suggests with an exchange. Each value passes through the
	//! range of its setter, and one that is missing or zero changes nothing.
	static void ApplyHints(int syncS, int perHour, int burst, int serverS, int wideS)
	{
		SetSyncInterval(syncS);
		SetBudget(perHour, burst);
		SetBoardLives(serverS, wideS);
	}

	//------------------------------------------------------------------------------------------------
	//! Back to the defaults.
	static void Reset()
	{
		s_iLifeServerS = LIFE_SERVER_S;
		s_iLifeWideS = LIFE_WIDE_S;
		s_iBudgetPerHour = BUDGET_PER_HOUR;
		s_iBudgetBurst = BUDGET_BURST;
		s_iPlayerShare = PLAYER_SHARE;
		s_iSyncIntervalS = SYNC_INTERVAL_S;
	}

	//------------------------------------------------------------------------------------------------
	//! \return \p value brought into low..high, or \p current when no value was given
	protected static int Within(int value, int current, int low, int high)
	{
		if (value <= 0)
			return current;
		return Math.ClampInt(value, low, high);
	}

	//------------------------------------------------------------------------------------------------
	protected static int Share(int whole)
	{
		int part = whole * s_iPlayerShare / 100;
		if (part < 1)
			part = 1;
		return part;
	}
}
