//------------------------------------------------------------------------------------------------
//! Server: the one timed exchange with the stats service.
//!
//! Every IA_ApiTunables.SyncIntervalS seconds whatever waits goes out in a single request to
//! /sync: the statistics events, the transport points, the ratings players wait on and, while
//! somebody has a leaderboard open, the boards and own lines the menu opens on. The answer
//! brings each part's result. With nothing waiting and nobody looking, nothing is sent.
//!
//! A batch goes again unchanged, under the same id, until the stats service says it has it, so
//! an answer that is lost costs nothing twice. A stats service without /sync is served through
//! the separate routes as before, and is asked again once in a long while.
//------------------------------------------------------------------------------------------------
class IA_ApiSync
{
	static const int MODE_UNKNOWN = 0;		// no exchange has been answered yet
	static const int MODE_SYNC = 1;			// the stats service answers /sync
	static const int MODE_SEPARATE = 2;		// it has no /sync: the separate routes carry everything

	// A timer that comes round a moment early must not skip a whole turn.
	protected static const int BACKOFF_SLACK_MS = 5000;
	protected static const int WAIT_MARGIN_MS = 2000;

	protected static ref IA_ApiSync s_Instance;

	protected int m_iMode;
	protected bool m_bStarted;
	protected bool m_bInFlight;
	protected bool m_bEverSent;
	protected int m_iSentMs;
	protected int m_iNextTickMs;
	protected int m_iFailures;
	protected int m_iNextAttemptMs;
	protected int m_iReprobeMs;
	protected int m_iSerial;

	// What the exchange now out carries.
	protected bool m_bSentStats;
	protected bool m_bSentTransport;
	protected bool m_bSentRatings;
	protected bool m_bSentBoards;

#ifdef WORKBENCH
	protected int m_iProbeSkewMs;
	protected int m_iProbeLose;
	protected string m_sProbeMembers;
	protected bool m_bProbeManual;
	protected string m_sProbeStats;
	protected string m_sProbeTransport;
	protected int m_iProbeAnswerChars;
#endif

	//------------------------------------------------------------------------------------------------
	static IA_ApiSync GetInstance()
	{
		if (!s_Instance)
			s_Instance = new IA_ApiSync();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Start the timer. Whatever has something to send calls this; the first call counts.
	void Start()
	{
		if (m_bStarted)
			return;
		m_bStarted = true;
		Arm();
	}

	//------------------------------------------------------------------------------------------------
	//! \return true while batches and rating lookups travel in the timed exchange; false once the
	//! stats service has shown it lacks one and the separate routes carry them
	bool UsesSync()
	{
		return m_iMode != MODE_SEPARATE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return a MODE_ value
	int GetMode()
	{
		return m_iMode;
	}

	//------------------------------------------------------------------------------------------------
	//! A player waits on something only an exchange brings, so it goes now rather than on the
	//! timer, unless one went a moment ago. The timer then counts from this one.
	void Hurry()
	{
		if (m_bInFlight || !Replication.IsServer() || !IA_ApiHandler.GetInstance().IsLinked())
			return;

		int now = NowMs();
		if (m_bEverSent && now - m_iSentMs < IA_ApiTunables.SYNC_HURRY_GAP_S * 1000)
			return;

		Exchange();
		if (m_bInFlight && m_bStarted)
			Arm();
	}

	//------------------------------------------------------------------------------------------------
	//! \return milliseconds until the next exchange should have been answered, 0 when none is
	//! known to be coming
	int WaitMs()
	{
		if (m_iMode != MODE_SYNC || !m_bStarted)
			return 0;

		int now = NowMs();
		if (IsBackingOff(now))
			return 0;
		if (m_bInFlight)
			return WAIT_MARGIN_MS;

		int left = m_iNextTickMs - now;
		if (left < 0)
			left = 0;
		return left + WAIT_MARGIN_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_ApiHandler with the body of a /sync answer.
	void OnAnswer(string data)
	{
		if (!m_bInFlight)
			return;

#ifdef WORKBENCH
		if (m_iProbeLose > 0)
		{
			m_iProbeLose = m_iProbeLose - 1;
			OnTimeout(m_iSerial);
			return;
		}
#endif
		Settle();

		ref JsonLoadContext ctx = new JsonLoadContext();
		string status;
		if (!ctx.LoadFromString(data) || !ctx.ReadValue("status", status) || status != "ok")
		{
			Fail(200, false);
			return;
		}

		m_iFailures = 0;
		if (m_iMode != MODE_SYNC)
		{
			m_iMode = MODE_SYNC;
			IA_Log.Info("[IA][API] Statistics, transport ratings and leaderboards travel in one timed exchange with the stats service.");
		}

		string part;
		if (m_bSentStats)
		{
			ctx.ReadValue("statsStatus", part);
			IA_StatsManager.GetInstance().OnSyncResult(part);
		}

		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		if (m_bSentTransport)
		{
			part = "";
			ctx.ReadValue("transportStatus", part);
			store.OnSyncBatchResult(part);
		}
		if (m_bSentRatings)
		{
			part = "";
			ctx.ReadValue("ratingsStatus", part);
			if (part == "ok")
				store.OnSyncRatings(ctx);
			else
				store.OnRatingsFailed();
		}

		IA_BoardService boards = IA_BoardService.GetInstance();
		if (m_bSentBoards && boards)
		{
			part = "";
			ctx.ReadValue("boardsStatus", part);
			// On a failure what is held stays as it is.
			if (part == "ok")
				boards.OnSyncBoards(ctx);
		}

		// What the stats service suggests, each value inside the range this server allows.
		int syncS;
		int perHour;
		int burst;
		int serverS;
		int wideS;
		ctx.ReadValue("nextSyncSeconds", syncS);
		ctx.ReadValue("pageBudgetPerHour", perHour);
		ctx.ReadValue("pageBudgetBurst", burst);
		ctx.ReadValue("serverPageSeconds", serverS);
		ctx.ReadValue("globalPageSeconds", wideS);
		IA_ApiTunables.ApplyHints(syncS, perHour, burst, serverS, wideS);

#ifdef WORKBENCH
		string probeStats;
		string probeTransport;
		ctx.ReadValue("statsStatus", probeStats);
		ctx.ReadValue("transportStatus", probeTransport);
		m_sProbeStats = probeStats;
		m_sProbeTransport = probeTransport;
		m_iProbeAnswerChars = data.Length();
#endif
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][API] Exchange answered: statistics %1, transport %2, ratings %3, boards %4.", m_bSentStats, m_bSentTransport, m_bSentRatings, m_bSentBoards), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_ApiHandler when the exchange got no usable answer.
	//! \param code the HTTP status, 0 when there was none
	//! \param missing the stats service has no /sync route to answer on
	void OnFailed(int code, bool missing, bool timedOut)
	{
		if (!m_bInFlight)
			return;
		Settle();

		// Nothing was looked at, so nothing failed: the separate routes take over.
		if (missing || m_iMode == MODE_SEPARATE)
		{
			GoSeparate();
			return;
		}
		Fail(code, timedOut);
	}

	//------------------------------------------------------------------------------------------------
	protected void Arm()
	{
		int interval = IA_ApiTunables.SyncIntervalS() * 1000;
		m_iNextTickMs = NowMs() + interval;
		GetGame().GetCallqueue().Remove(Tick);
#ifdef WORKBENCH
		if (m_bProbeManual)
			return;
#endif
		GetGame().GetCallqueue().CallLater(Tick, interval, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		// Armed anew each time, so a change to the interval takes hold with the next turn.
		Arm();
		if (!Replication.IsServer())
			return;
		Exchange();
	}

	//------------------------------------------------------------------------------------------------
	//! Send what waits: in one exchange, or on the separate routes of a stats service without it.
	protected void Exchange()
	{
		if (m_bInFlight)
			return;

		IA_ApiHandler api = IA_ApiHandler.GetInstance();
		int now = NowMs();
		// A server the stats service does not know has only the separate routes' answer to that.
		if (!api.IsLinked())
		{
			RunSeparate();
			return;
		}
		if (m_iMode == MODE_SEPARATE && now - m_iReprobeMs < 0)
		{
			RunSeparate();
			return;
		}
		if (IsBackingOff(now))
			return;

		IA_StatsManager stats = IA_StatsManager.GetInstance();
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		string members;

		string statsId;
		string events;
		bool hasStats = stats.SyncBatch(statsId, events);
		if (hasStats)
		{
			members = members + ",\"statsBatchId\":\"" + statsId + "\"";
			members = members + ",\"matchData\":" + events;
		}

		string transportId;
		string entries;
		bool hasTransport = store.SyncBatch(transportId, entries);
		if (hasTransport)
		{
			members = members + ",\"transport\":{\"batchId\":\"" + transportId + "\"";
			members = members + ",\"entries\":" + entries + "}";
		}

		string ratingIds = store.SyncRatingIds();
		bool hasRatings = !ratingIds.IsEmpty();
		if (hasRatings)
			members = members + ",\"ratingIds\":" + ratingIds;

		// Boards and own lines are asked for only while somebody looks at them.
		bool hasBoards;
		IA_BoardService boards = IA_BoardService.GetInstance();
		if (boards)
		{
			string wanted = boards.SyncWanted();
			hasBoards = !wanted.IsEmpty();
			members = members + wanted;
		}

		// Nothing waiting and nobody looking: nothing is sent.
		if (!hasStats && !hasTransport && !hasRatings && !hasBoards)
		{
			if (m_iMode == MODE_SEPARATE)
				RunSeparate();
			return;
		}

		// Empty when no name is known yet; the backend then keeps the one it has.
		string serverName = IA_ServerNameResolver.ForStats();
		if (!serverName.IsEmpty())
			members = ",\"serverName\":\"" + IA_JsonEscape(serverName) + "\"" + members;

		if (!api.Sync(members))
			return;

		store.OnSyncSent(hasTransport, hasRatings);
		m_bSentStats = hasStats;
		m_bSentTransport = hasTransport;
		m_bSentRatings = hasRatings;
		m_bSentBoards = hasBoards;
		m_bInFlight = true;
		m_bEverSent = true;
		m_iSentMs = now;
		m_iSerial = m_iSerial + 1;
		GetGame().GetCallqueue().CallLater(OnTimeout, IA_ApiTunables.SYNC_TIMEOUT_S * 1000, false, m_iSerial);
#ifdef WORKBENCH
		m_sProbeMembers = members;
#endif
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][API] Exchange sent: statistics %1, transport %2, ratings %3, boards %4.", hasStats, hasTransport, hasRatings, hasBoards), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What the separate timers did before there was an exchange.
	protected void RunSeparate()
	{
		IA_StatsManager.GetInstance().SendBatch();
		IA_TransportPilotStore.GetInstance().Flush();
	}

	//------------------------------------------------------------------------------------------------
	//! The stats service has no /sync. What the exchange carried goes on the separate routes
	//! now, and so does everything after it until the stats service is asked again.
	protected void GoSeparate()
	{
		IA_TransportPilotStore.GetInstance().OnSyncUnsent(m_bSentTransport, m_bSentRatings);
		if (m_iMode != MODE_SEPARATE)
			IA_Log.Info("[IA][API] The stats service has no timed exchange; statistics and transport ratings travel on the separate routes.");

		m_iMode = MODE_SEPARATE;
		m_iFailures = 0;
		m_iReprobeMs = NowMs() + IA_ApiTunables.SYNC_REPROBE_S * 1000;
		RunSeparate();
	}

	//------------------------------------------------------------------------------------------------
	//! No usable answer. What the exchange carried is kept and goes again, ever more slowly
	//! while the failures last.
	protected void Fail(int code, bool timedOut)
	{
		IA_TransportPilotStore store = IA_TransportPilotStore.GetInstance();
		if (m_bSentTransport)
			store.OnSubmitResult(false);
		if (m_bSentRatings)
			store.OnRatingsFailed();

		m_iFailures = m_iFailures + 1;
		int interval = IA_ApiTunables.SyncIntervalS() * 1000;
		int wait = IA_TransportScoring.BackoffMs(m_iFailures, interval, IA_ApiTunables.SYNC_BACKOFF_MAX_S * 1000);
		m_iNextAttemptMs = NowMs() + wait - BACKOFF_SLACK_MS;

		// Said once for a run of failures; the attempts after it are expected.
		if (m_iFailures > 1)
			return;
		if (timedOut)
			Print("[IA][API] The stats service did not answer the timed exchange; what it carried is kept and sent again.", LogLevel.WARNING);
		else if (code == 403)
			Print("[IA][API] The stats service does not accept this server's GUID; nothing is recorded until it does.", LogLevel.ERROR);
		else
			Print(string.Format("[IA][API] The timed exchange with the stats service failed (code %1); what it carried is kept and sent again.", code), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTimeout(int serial)
	{
		if (!m_bInFlight || serial != m_iSerial)
			return;

		IA_ApiHandler.GetInstance().AbandonSync();
		Settle();
		if (m_iMode == MODE_SEPARATE)
		{
			GoSeparate();
			return;
		}
		Fail(0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Settle()
	{
		m_bInFlight = false;
		GetGame().GetCallqueue().Remove(OnTimeout);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsBackingOff(int now)
	{
		return m_iFailures > 0 && now - m_iNextAttemptMs < 0;
	}

	//------------------------------------------------------------------------------------------------
	protected int NowMs()
	{
		int now = System.GetTickCount();
#ifdef WORKBENCH
		now = now + m_iProbeSkewMs;
#endif
		return now;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: forget everything this run has learnt about the stats service, and its clock.
	void ProbeReset()
	{
		Settle();
		m_iMode = MODE_UNKNOWN;
		m_bEverSent = false;
		m_iFailures = 0;
		m_iNextAttemptMs = 0;
		m_iReprobeMs = 0;
		m_iProbeLose = 0;
		m_sProbeMembers = "";
		m_sProbeStats = "";
		m_sProbeTransport = "";
		m_iProbeAnswerChars = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: while on, the timer does not run and only ProbeTick turns it.
	void ProbeManual(bool on)
	{
		m_bProbeManual = on;
		if (m_bStarted)
			Arm();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: what the last answer said of the statistics batch it was sent.
	string ProbeStatsStatus()
	{
		return m_sProbeStats;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: what the last answer said of the transport batch it was sent.
	string ProbeTransportStatus()
	{
		return m_sProbeTransport;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the length of the last answer that could be read.
	int ProbeAnswerChars()
	{
		return m_iProbeAnswerChars;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: one turn of the timer, now.
	void ProbeTick()
	{
		m_sProbeMembers = "";
		Arm();
		Exchange();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: move this clock on.
	void ProbeAdvance(int ms)
	{
		m_iProbeSkewMs = m_iProbeSkewMs + ms;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the next \p count answers are lost on the way back.
	void ProbeLoseNext(int count)
	{
		m_iProbeLose = count;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: true while an exchange is out.
	bool ProbeInFlight()
	{
		return m_bInFlight;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: exchanges that have failed in a row.
	int ProbeFailures()
	{
		return m_iFailures;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: what the exchange sent by the last ProbeTick carried after the server GUID; empty when it sent none.
	string ProbeMembers()
	{
		return m_sProbeMembers;
	}
#endif
}
