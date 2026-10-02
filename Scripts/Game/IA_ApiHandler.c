// Game/IA_ApiHandler.c

class IA_ApiRequest
{
    string ToJson();
}

class IA_ApiRegisterServerRequest : IA_ApiRequest
{
    string serverName;
    string ownerEmail;

    void IA_ApiRegisterServerRequest(string name, string email)
    {
        serverName = name;
        ownerEmail = email;
    }

    override string ToJson()
    {
        string json = "{";
        json = json + "\"serverName\": \"" + IA_JsonEscape(serverName) + "\",";
        json = json + "\"ownerEmail\": \"" + IA_JsonEscape(ownerEmail) + "\"";
        json = json + "}";
        return json;
    }
}

class IA_ApiSubmitStatsRequest : IA_ApiRequest
{
    string serverGuid;
	string serverName;
    string matchData; 

    void IA_ApiSubmitStatsRequest(string guid, string name, string data)
    {
        serverGuid = guid;
		serverName = name;
        matchData = data;
    }

    override string ToJson()
    {
        // matchData is already a JSON string, so we don't wrap it in extra quotes.
        string json = "{";
        json = json + "\"serverGuid\": \"" + IA_JsonEscape(serverGuid) + "\",";
		json = json + "\"serverName\": \"" + IA_JsonEscape(serverName) + "\",";
        json = json + "\"matchData\": " + matchData;
        json = json + "}";
        return json;
    }
}

class IA_ApiRegisterServerResponse
{
    string serverGuid;

    static IA_ApiRegisterServerResponse FromJson(string jsonData)
    {
        IA_ApiRegisterServerResponse response = new IA_ApiRegisterServerResponse();
        // We need a robust way to parse this. Let's reuse the logic from IA_ApiConfig
        string searchKey = "\"serverGuid\":\"";
        int startIndex = jsonData.IndexOf(searchKey);
        if (startIndex == -1)
            return response;

        int valueStartIndex = startIndex + searchKey.Length();
        string tempJson = jsonData.Substring(valueStartIndex, jsonData.Length() - valueStartIndex);
        int endIndex = tempJson.IndexOf("\"");
        if (endIndex == -1)
            return response;

        response.serverGuid = tempJson.Substring(0, endIndex);
        return response;
    }
}

class IA_ApiHandler
{
	// Source and deploy steps: backend/azure-functions.
	string m_sApiBaseUrl = "https://invade-annex-api.azurewebsites.net/api";
	protected ref RestCallback m_registerCallback;
	protected ref RestCallback m_submitStatsCallback;
	protected ref RestCallback m_fetchLeaderboardCallback;
	protected ref RestCallback m_submitTransportCallback;
	protected ref RestCallback m_fetchTransportRatingsCallback;
	protected ref RestCallback m_syncCallback;
	// Held so the engine never calls into a callback that is gone.
	protected ref RestCallback m_syncAbandoned;

	// The routes requests are counted on.
	static const int ROUTE_REGISTER = 0;
	static const int ROUTE_SUBMIT_STATS = 1;
	static const int ROUTE_LEADERBOARD = 2;
	static const int ROUTE_SUBMIT_TRANSPORT = 3;
	static const int ROUTE_TRANSPORT_RATINGS = 4;
	static const int ROUTE_SYNC = 5;
	static const int ROUTE_COUNT = 6;
	protected static const int CALL_SUMMARY_MS = 600000;

	protected ref array<int> m_aCalls = {};
	protected bool m_bCallSummaryStarted;

    private static ref IA_ApiHandler s_Instance;
    private ref IA_ApiConfig m_Config;

#ifdef WORKBENCH
	protected bool m_bBaseOverridden;
	protected bool m_bProbeCapture;
	protected string m_sProbeSyncRoute;
	protected string m_sProbeSyncOutcome;
	protected ref array<int> m_aProbeCalls = {};
#endif

	//------------------------------------------------------------------------------------------------
	void IA_ApiHandler()
	{
#ifdef WORKBENCH
		// Workbench only: -iaApiBase <address> points this run at another stats service, such as
		// the local development server. It holds for this run and is written nowhere.
		string base;
		if (System.GetCLIParam("iaApiBase", base) && !base.IsEmpty())
		{
			m_sApiBaseUrl = base;
			m_bBaseOverridden = true;
			Print("[IA][API] Workbench: the stats service address for this run comes from -iaApiBase.", LogLevel.WARNING);
		}
#endif
	}

    static IA_ApiHandler GetInstance()
    {
        if (!s_Instance)
            s_Instance = new IA_ApiHandler();
        return s_Instance;
    }

    void Init()
    {
        m_Config = IA_ApiConfigManager.GetConfig();
#ifdef WORKBENCH
		// A GUID handed out by a development server must never be saved as this profile's own.
		if (m_bBaseOverridden && m_Config && m_Config.m_sServerGuid == "")
		{
			Print("[IA][API] Workbench: this profile has no server GUID, and none is asked of a stats service named by -iaApiBase.", LogLevel.WARNING);
			return;
		}
#endif
        if (m_Config && m_Config.m_sServerGuid == "")
        {
            IA_Log.Info("IA API Handler: Server GUID not found, beginning registration.");
            _RegisterServer();
        }
		else
		{
			// Leaderboards are fetched a page at a time, when a player asks for one.
			if (IA_Log.IsDebugEnabled())
			{
				Print("IA API Handler: Server GUID exists.", LogLevel.NORMAL);
			}

			// Worked out at start, so the log shows the name and a live one is kept on a session that sends no statistics.
			IA_ServerNameResolver.ForStats();
		}

		if (IA_Log.IsDebugEnabled() && !m_bCallSummaryStarted)
		{
			m_bCallSummaryStarted = true;
			GetGame().GetCallqueue().CallLater(LogCallSummary, CALL_SUMMARY_MS, true);
		}
    }

	//------------------------------------------------------------------------------------------------
	//! \return true when this server has a GUID and so can ask the stats service anything
	bool IsLinked()
	{
#ifdef WORKBENCH
		if (m_bProbeCapture)
			return true;
#endif
		return m_Config && m_Config.m_sServerGuid != "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param route a ROUTE_ value
	//! \return requests sent on that route since the game started
	int GetCallCount(int route)
	{
		if (route < 0 || route >= m_aCalls.Count())
			return 0;
		return m_aCalls[route];
	}

	//------------------------------------------------------------------------------------------------
	//! \return requests sent on every route since the game started
	int GetCallTotal()
	{
		int total;
		foreach (int calls : m_aCalls)
		{
			total = total + calls;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Requests sent per route, as one line.
	string CallSummary()
	{
		string summary = string.Format("registerServer %1, submitStats %2, leaderboard %3, ", GetCallCount(ROUTE_REGISTER), GetCallCount(ROUTE_SUBMIT_STATS), GetCallCount(ROUTE_LEADERBOARD));
		return summary + string.Format("submitTransport %1, getTransportRatings %2, sync %3", GetCallCount(ROUTE_SUBMIT_TRANSPORT), GetCallCount(ROUTE_TRANSPORT_RATINGS), GetCallCount(ROUTE_SYNC));
	}

	//------------------------------------------------------------------------------------------------
	//! Counted where a request is handed to the engine, so none leaves uncounted.
	protected void CountCall(int route)
	{
		while (m_aCalls.Count() < ROUTE_COUNT)
		{
			m_aCalls.Insert(0);
		}
		m_aCalls[route] = m_aCalls[route] + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void LogCallSummary()
	{
		if (IA_Log.IsDebugEnabled())
		{
			Print("[IA][API] Requests sent since start: " + CallSummary() + ".", LogLevel.NORMAL);
		}
	}

    void OnRegisterSuccess(RestCallback cb)
    {
        string data = cb.GetData();
        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][API] Registration response received.", LogLevel.NORMAL);
        }
        IA_ApiRegisterServerResponse response = IA_ApiRegisterServerResponse.FromJson(data);
        if (response && response.serverGuid != "")
        {
            IA_ApiConfig config = IA_ApiConfigManager.GetConfig();
            if (config)
            {
                config.m_sServerGuid = response.serverGuid;
                IA_ApiConfigManager.SaveConfig();
                IA_Log.Info("IA API: Server GUID " + response.serverGuid + " saved to config.");
            }
        }
        else
        {
            Print("[IA][API] Registration response did not contain a valid server GUID.", LogLevel.ERROR);
        }
    }

    void OnRegisterError(RestCallback cb)
    {
        if (cb.GetRestResult() == ERestResult.EREST_ERROR_TIMEOUT)
            Print("IA API: Registration request timed out.", LogLevel.ERROR);
        else
            Print("IA API: Registration failed with error code: " + cb.GetHttpCode(), LogLevel.ERROR);
    }

    void OnSubmitStatsSuccess(RestCallback cb)
    {
        if (IA_Log.IsDebugEnabled())
        {
            Print("IA API: Statistics submitted successfully.", LogLevel.NORMAL);
        }
        // The leaderboard pages the server holds are left alone: they age out (IA_ApiTunables).
    }

    void OnSubmitStatsError(RestCallback cb)
    {
        if (cb.GetRestResult() == ERestResult.EREST_ERROR_TIMEOUT)
            Print("IA API: Statistics submission request timed out.", LogLevel.ERROR);
        else
            Print("IA API: Statistics submission failed with error code: " + cb.GetHttpCode(), LogLevel.ERROR);
    }

	//------------------------------------------------------------------------------------------------
	void OnFetchLeaderboardSuccess(RestCallback cb)
	{
		// An answer to a request that was given up on must not be taken for the one now out.
		if (cb != m_fetchLeaderboardCallback)
			return;

		IA_BoardService boards = IA_BoardService.GetInstance();
		if (boards)
			boards.OnPageReceived(cb.GetData());
	}

	//------------------------------------------------------------------------------------------------
	void OnFetchLeaderboardError(RestCallback cb)
	{
		if (cb != m_fetchLeaderboardCallback)
			return;

		if (cb.GetRestResult() == ERestResult.EREST_ERROR_TIMEOUT)
			Print("[IA][API] Leaderboard page request timed out.", LogLevel.ERROR);
		else
			Print("[IA][API] Leaderboard page request failed with error code: " + cb.GetHttpCode(), LogLevel.ERROR);

		IA_BoardService boards = IA_BoardService.GetInstance();
		if (boards)
			boards.OnPageFailed();
	}

    void SubmitStats(string jsonData)
    {
#ifdef WORKBENCH
		if (m_bProbeCapture)
		{
			ProbeCount(ROUTE_SUBMIT_STATS);
			return;
		}
#endif
        if (!m_Config || m_Config.m_sServerGuid == "")
        {
            Print("IA API: Cannot submit stats, server GUID is missing.", LogLevel.ERROR);
            return;
        }

        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        ctx.SetHeaders("Content-Type,application/json");

        // Empty when no name is known yet; the backend then keeps the one it has.
        string serverName = IA_ServerNameResolver.ForStats();

        IA_ApiSubmitStatsRequest requestData = new IA_ApiSubmitStatsRequest(m_Config.m_sServerGuid, serverName, jsonData);

        m_submitStatsCallback = new RestCallback();
        m_submitStatsCallback.SetOnSuccess(OnSubmitStatsSuccess);
        m_submitStatsCallback.SetOnError(OnSubmitStatsError);
        ctx.POST(m_submitStatsCallback, "/submitStats", requestData.ToJson());
        CountCall(ROUTE_SUBMIT_STATS);
        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][API] Statistics submission requested.", LogLevel.NORMAL);
        }
    }

	//------------------------------------------------------------------------------------------------
	//! Ask for one page of a leaderboard. The answer goes to IA_BoardService, which is also the
	//! only caller: it pays for every request from the server's allowance.
	//! \param board "server", "global" or "servers"
	//! \param sortName a sort key of the /leaderboard route
	//! \param limit rows wanted; 0 asks only where the player stands
	//! \param playerId identity whose own line is wanted, empty for none
	//! \return false when nothing was sent
	bool FetchLeaderboardPage(string board, string sortName, bool descending, int offset, int limit, string playerId)
	{
		if (!m_Config || m_Config.m_sServerGuid == "")
			return false;

		string dir = "asc";
		if (descending)
			dir = "desc";

		string url = "/leaderboard?serverGuid=" + m_Config.m_sServerGuid + "&board=" + board + "&sort=" + sortName;
		url = url + "&dir=" + dir + "&offset=" + offset.ToString() + "&limit=" + limit.ToString();
		if (!playerId.IsEmpty())
			url = url + "&playerId=" + playerId;

		RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
		m_fetchLeaderboardCallback = new RestCallback();
		m_fetchLeaderboardCallback.SetOnSuccess(OnFetchLeaderboardSuccess);
		m_fetchLeaderboardCallback.SetOnError(OnFetchLeaderboardError);
		ctx.GET(m_fetchLeaderboardCallback, url);
		CountCall(ROUTE_LEADERBOARD);
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][API] Leaderboard page requested: board %1, sort %2, offset %3.", board, sortName, offset), LogLevel.NORMAL);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Add transport rating to players' global totals. The batch id lets the
	//! backend drop a resend whose first attempt did land.
	//! \param entriesJson JSON array of {playerId, playerName, points, insertions}
	//! \return false when nothing was sent
	bool SubmitTransport(string batchId, string entriesJson)
	{
#ifdef WORKBENCH
		if (m_bProbeCapture)
		{
			ProbeCount(ROUTE_SUBMIT_TRANSPORT);
			return true;
		}
#endif
		// Unregistered servers are normal in Workbench; stay quiet and let the caller retry.
		if (!m_Config || m_Config.m_sServerGuid == "")
			return false;

		string body = "{";
		body = body + "\"serverGuid\": \"" + IA_JsonEscape(m_Config.m_sServerGuid) + "\",";
		body = body + "\"batchId\": \"" + IA_JsonEscape(batchId) + "\",";
		body = body + "\"entries\": " + entriesJson;
		body = body + "}";

		RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
		ctx.SetHeaders("Content-Type,application/json");
		m_submitTransportCallback = new RestCallback();
		m_submitTransportCallback.SetOnSuccess(OnSubmitTransportSuccess);
		m_submitTransportCallback.SetOnError(OnSubmitTransportError);
		ctx.POST(m_submitTransportCallback, "/submitTransport", body);
		CountCall(ROUTE_SUBMIT_TRANSPORT);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void OnSubmitTransportSuccess(RestCallback cb)
	{
		IA_TransportPilotStore.GetInstance().OnSubmitResult(true);
	}

	//------------------------------------------------------------------------------------------------
	void OnSubmitTransportError(RestCallback cb)
	{
		Print("[IA][API] Transport rating submission failed with error code: " + cb.GetHttpCode(), LogLevel.ERROR);
		IA_TransportPilotStore.GetInstance().OnSubmitResult(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Fetch global transport ratings and the central skin thresholds.
	//! \param playerIdsJson JSON array of player identity ids
	//! \return false when nothing was sent
	bool FetchTransportRatings(string playerIdsJson)
	{
#ifdef WORKBENCH
		if (m_bProbeCapture)
		{
			ProbeCount(ROUTE_TRANSPORT_RATINGS);
			return true;
		}
#endif
		if (!m_Config || m_Config.m_sServerGuid == "")
			return false;

		string body = "{";
		body = body + "\"serverGuid\": \"" + IA_JsonEscape(m_Config.m_sServerGuid) + "\",";
		body = body + "\"playerIds\": " + playerIdsJson;
		body = body + "}";

		RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
		ctx.SetHeaders("Content-Type,application/json");
		m_fetchTransportRatingsCallback = new RestCallback();
		m_fetchTransportRatingsCallback.SetOnSuccess(OnFetchTransportRatingsSuccess);
		m_fetchTransportRatingsCallback.SetOnError(OnFetchTransportRatingsError);
		ctx.POST(m_fetchTransportRatingsCallback, "/getTransportRatings", body);
		CountCall(ROUTE_TRANSPORT_RATINGS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void OnFetchTransportRatingsSuccess(RestCallback cb)
	{
		IA_TransportPilotStore.GetInstance().OnRatingsReceived(cb.GetData());
	}

	//------------------------------------------------------------------------------------------------
	void OnFetchTransportRatingsError(RestCallback cb)
	{
		Print("[IA][API] Transport ratings request failed with error code: " + cb.GetHttpCode(), LogLevel.ERROR);
		IA_TransportPilotStore.GetInstance().OnRatingsFailed();
	}

	//------------------------------------------------------------------------------------------------
	//! The one timed exchange: all this server has to send and wants to know, in one request.
	//! IA_ApiSync builds it and takes the answer.
	//! \param members the members of the body after the server GUID, each led by a comma
	//! \return false when nothing was sent
	bool Sync(string members)
	{
#ifdef WORKBENCH
		if (m_bProbeCapture)
		{
			ProbeCount(ROUTE_SYNC);
			return true;
		}
#endif
		if (!m_Config || m_Config.m_sServerGuid == "")
			return false;

		string body = "{\"serverGuid\":\"" + IA_JsonEscape(m_Config.m_sServerGuid) + "\"";
		body = body + members + "}";

		string route = "/sync";
#ifdef WORKBENCH
		if (!m_sProbeSyncRoute.IsEmpty())
			route = m_sProbeSyncRoute;
#endif

		RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
		ctx.SetHeaders("Content-Type,application/json");
		m_syncCallback = new RestCallback();
		m_syncCallback.SetOnSuccess(OnSyncSuccess);
		m_syncCallback.SetOnError(OnSyncError);
		ctx.POST(m_syncCallback, route, body);
		CountCall(ROUTE_SYNC);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The exchange now out is given up on. Its answer, should one still come, is ignored.
	void AbandonSync()
	{
		m_syncAbandoned = m_syncCallback;
		m_syncCallback = null;
	}

	//------------------------------------------------------------------------------------------------
	void OnSyncSuccess(RestCallback cb)
	{
		if (cb != m_syncCallback)
			return;

		// Should a missing route ever be reported as an answer, it is still a missing route.
		int code = cb.GetHttpCode();
#ifdef WORKBENCH
		m_sProbeSyncOutcome = string.Format("answer, code %1, %2 characters", code, cb.GetData().Length());
#endif
		if (code == 404 || code == 501)
		{
			IA_ApiSync.GetInstance().OnFailed(code, true, false);
			return;
		}
		IA_ApiSync.GetInstance().OnAnswer(cb.GetData());
	}

	//------------------------------------------------------------------------------------------------
	void OnSyncError(RestCallback cb)
	{
		if (cb != m_syncCallback)
			return;

		int code = cb.GetHttpCode();
		ERestResult result = cb.GetRestResult();
#ifdef WORKBENCH
		m_sProbeSyncOutcome = string.Format("error, code %1, result %2, %3 characters", code, typename.EnumToString(ERestResult, result), cb.GetData().Length());
#endif
		// An older stats service has no such route (404), or has it over a database that cannot serve it (501).
		bool missing = code == 404 || code == 501 || result == ERestResult.EREST_ERROR_NOTIMPLEMENTED;
		IA_ApiSync.GetInstance().OnFailed(code, missing, result == ERestResult.EREST_ERROR_TIMEOUT);
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: true when -iaApiBase has moved this run off the real stats service.
	bool ProbeBaseOverridden()
	{
		return m_bBaseOverridden;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: stand as a registered server for this run. api_config.json is neither read nor
	//! written, and it is refused unless -iaApiBase has moved this run off the real stats service.
	bool ProbeLink(string guid)
	{
		if (!m_bBaseOverridden)
			return false;
		ref IA_ApiConfig made = new IA_ApiConfig("", guid);
		m_Config = made;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: send the timed exchange to another route, to meet a stats service that lacks it. Empty puts it back.
	void ProbeSyncRoute(string route)
	{
		m_sProbeSyncRoute = route;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: how the engine handed back the last exchange: as an answer or an error, and its code.
	string ProbeSyncOutcome()
	{
		return m_sProbeSyncOutcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: while on, statistics, transport and exchange requests are counted apart and not sent.
	void ProbeCapture(bool on)
	{
		m_bProbeCapture = on;
		m_aProbeCalls.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: requests held back on a route since capture was switched on.
	int ProbeCaptured(int route)
	{
		if (route < 0 || route >= m_aProbeCalls.Count())
			return 0;
		return m_aProbeCalls[route];
	}

	//------------------------------------------------------------------------------------------------
	protected void ProbeCount(int route)
	{
		while (m_aProbeCalls.Count() < ROUTE_COUNT)
		{
			m_aProbeCalls.Insert(0);
		}
		m_aProbeCalls[route] = m_aProbeCalls[route] + 1;
	}
#endif

    private void _RegisterServer()
    {
        if (!m_Config)
            return;

        m_registerCallback = new RestCallback();
        m_registerCallback.SetOnSuccess(OnRegisterSuccess);
        m_registerCallback.SetOnError(OnRegisterError);
        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        ctx.SetHeaders("Content-Type,application/json");

        string serverName = IA_ServerNameResolver.ForRegistration();
        IA_ApiRegisterServerRequest requestData = new IA_ApiRegisterServerRequest(serverName, m_Config.m_sOwnerEmail);

        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][API] Server registration requested.", LogLevel.NORMAL);
        }
        ctx.POST(m_registerCallback, "/registerServer", requestData.ToJson());
        CountCall(ROUTE_REGISTER);
        if (IA_Log.IsDebugEnabled())
        {
            Print("IA API: Attempting to register server...", LogLevel.NORMAL);
        }
    }
} 