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

    private static ref IA_ApiHandler s_Instance;
    private ref IA_ApiConfig m_Config;
    
    static IA_ApiHandler GetInstance()
    {
        if (!s_Instance)
            s_Instance = new IA_ApiHandler();
        return s_Instance;
    }

    void Init()
    {
        m_Config = IA_ApiConfigManager.GetConfig();
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
        // The stored scores moved, so the pages the server kept are out of date.
        IA_LeaderboardManagerComponent boards = IA_LeaderboardManagerComponent.GetInstance();
        if (boards)
            boards.InvalidateCache();
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

		IA_LeaderboardManagerComponent boards = IA_LeaderboardManagerComponent.GetInstance();
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

		IA_LeaderboardManagerComponent boards = IA_LeaderboardManagerComponent.GetInstance();
		if (boards)
			boards.OnPageFailed();
	}

    void SubmitStats(string jsonData)
    {
        if (!m_Config || m_Config.m_sServerGuid == "")
        {
            Print("IA API: Cannot submit stats, server GUID is missing.", LogLevel.ERROR);
            return;
        }

        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        ctx.SetHeaders("Content-Type,application/json");

        string serverName = IA_ApiConfigManager.GetServerNameFromFile();

        IA_ApiSubmitStatsRequest requestData = new IA_ApiSubmitStatsRequest(m_Config.m_sServerGuid, serverName, jsonData);

        m_submitStatsCallback = new RestCallback();
        m_submitStatsCallback.SetOnSuccess(OnSubmitStatsSuccess);
        m_submitStatsCallback.SetOnError(OnSubmitStatsError);
        ctx.POST(m_submitStatsCallback, "/submitStats", requestData.ToJson());
        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][API] Statistics submission requested.", LogLevel.NORMAL);
        }
    }

	//------------------------------------------------------------------------------------------------
	//! Ask for one page of a leaderboard. The answer goes to IA_LeaderboardManagerComponent.
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

    private void _RegisterServer()
    {
        if (!m_Config)
            return;

        m_registerCallback = new RestCallback();
        m_registerCallback.SetOnSuccess(OnRegisterSuccess);
        m_registerCallback.SetOnError(OnRegisterError);
        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        ctx.SetHeaders("Content-Type,application/json");

        string serverName = IA_ApiConfigManager.GetServerNameFromFile();
        IA_ApiRegisterServerRequest requestData = new IA_ApiRegisterServerRequest(serverName, m_Config.m_sOwnerEmail);

        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][API] Server registration requested.", LogLevel.NORMAL);
        }
        ctx.POST(m_registerCallback, "/registerServer", requestData.ToJson());
        if (IA_Log.IsDebugEnabled())
        {
            Print("IA API: Attempting to register server...", LogLevel.NORMAL);
        }
    }
} 