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
        json = json + "\"serverName\": \"" + serverName + "\",";
        json = json + "\"ownerEmail\": \"" + ownerEmail + "\"";
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
        json = json + "\"serverGuid\": \"" + serverGuid + "\",";
		json = json + "\"serverName\": \"" + serverName + "\",";
        json = json + "\"matchData\": " + matchData;
        json = json + "}";
        return json;
    }
}

// Helper to extract a JSON array string from a larger JSON object.
// This is needed because the built-in parser is very basic.
static string _GetJsonArrayForKey(string json, string key)
{
    string searchKey = "\"" + key + "\":[";
    int startIndex = json.IndexOf(searchKey);
    if (startIndex == -1)
    {
        Print("IA API: _GetJsonArrayForKey: Key '" + key + "' not found.", LogLevel.WARNING);
        return "[]"; // Return an empty JSON array if key not found
    }

    int valueStartIndex = startIndex + searchKey.Length() - 1; // Position of the opening bracket '['
    string fromValueStart = json.Substring(valueStartIndex, json.Length() - valueStartIndex);

    int bracketCount = 0;
    int endIndex = -1;

    for (int i = 0; i < fromValueStart.Length(); i++)
    {
        string char = fromValueStart.Get(i);
        if (char == "[")
        {
            bracketCount++;
        }
        else if (char == "]")
        {
            bracketCount--;
            if (bracketCount == 0)
            {
                endIndex = i;
                break;
            }
        }
    }

    if (endIndex == -1)
    {
        Print("IA API: _GetJsonArrayForKey: Could not find matching closing bracket for key '" + key + "'.", LogLevel.ERROR);
        return "[]"; // Return empty array if malformed
    }
        
    return fromValueStart.Substring(0, endIndex + 1);
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

class IA_RegisterServerCallback : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
		
        Print("IA API: Registration successful. Data = " + data, LogLevel.NORMAL);
        IA_ApiRegisterServerResponse response = IA_ApiRegisterServerResponse.FromJson(data);
        if (response && response.serverGuid != "")
        {
            IA_ApiConfig config = IA_ApiConfigManager.GetConfig();
            if (config)
            {
                config.m_sServerGuid = response.serverGuid;
                IA_ApiConfigManager.SaveConfig();
                Print("IA API: Server GUID " + response.serverGuid + " saved to config.", LogLevel.NORMAL);
				
				// Now that we are registered, lets get the initial leaderboard data
				IA_ApiHandler.GetInstance().FetchAllLeaderboards();
            }
        }
        else
        {
            Print("IA API: Registration response did not contain a valid serverGuid. Response: " + response, LogLevel.ERROR);
        }
    }

    override void OnError(int errorCode)
    {
        Print("IA API: Registration failed with error code: " + errorCode, LogLevel.ERROR);
    }

    override void OnTimeout()
    {
        Print("IA API: Registration request timed out.", LogLevel.ERROR);
    }
}

class IA_SubmitStatsCallback : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
        Print("IA API: Statistics submitted successfully.", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(IA_ApiHandler.GetInstance().FetchAllLeaderboards, 5000, false);
    }

    override void OnError(int errorCode)
    {
        Print("IA API: Statistics submission failed with error code: " + errorCode, LogLevel.ERROR);
    }

    override void OnTimeout()
    {
        Print("IA API: Statistics submission request timed out.", LogLevel.ERROR);
    }
}

class IA_FetchAllLeaderboardsCallback : RestCallback
{
    override void OnSuccess(string data, int dataSize)
    {
        Print("IA API: All leaderboard data received successfully.", LogLevel.NORMAL);
        
        IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
        if (manager)
        {
            string globalPlayerData = _GetJsonArrayForKey(data, "globalPlayerLeaderboard");
            string serverPlayerData = _GetJsonArrayForKey(data, "serverPlayerLeaderboard");
            string globalServerData = _GetJsonArrayForKey(data, "globalServerLeaderboard");
            
            manager.UpdateLeaderboardData(globalPlayerData);
            manager.UpdateServerLeaderboardData(serverPlayerData);
            manager.UpdateGlobalServerLeaderboardData(globalServerData);
        }
        else
        {
            Print("IA API Error: Could not find IA_LeaderboardManagerComponent instance to update data.", LogLevel.ERROR);
        }
    }

    override void OnError(int errorCode)
    {
        Print("IA API: All leaderboards request FAILED with error code: " + errorCode, LogLevel.ERROR);
    }

    override void OnTimeout()
    {
        Print("IA API: All leaderboards request TIMED OUT.", LogLevel.ERROR);
    }
}

class IA_ApiHandler
{
	//string m_sApiBaseUrl = "https://invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net/api";
	string m_sApiBaseUrl = "https://iadev-gdcxh2dkhsceacfg.eastus-01.azurewebsites.net/api";
	protected ref IA_RegisterServerCallback m_registerCallback;
	protected ref IA_SubmitStatsCallback m_submitStatsCallback;
	protected ref IA_FetchAllLeaderboardsCallback m_fetchAllLeaderboardsCallback;

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
            Print("IA API Handler: Server GUID not found, beginning registration.", LogLevel.NORMAL);
            _RegisterServer();
        }
		else
		{
			Print("IA API Handler: Server GUID exists. Fetching initial leaderboard data.", LogLevel.NORMAL);
			FetchAllLeaderboards();
		}
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
        
        m_submitStatsCallback = new IA_SubmitStatsCallback();
        ctx.POST(m_submitStatsCallback, "/submitStats", requestData.ToJson());
        Print("IA API: Submitting statistics for server: " + serverName + " with payload: " + requestData.ToJson(), LogLevel.NORMAL);
    }
    
	void FetchAllLeaderboards()
    {
		if (!m_Config || m_Config.m_sServerGuid == "")
        {
            Print("IA API: Cannot fetch leaderboards, server GUID is missing.", LogLevel.ERROR);
            return;
        }
		
        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        m_fetchAllLeaderboardsCallback = new IA_FetchAllLeaderboardsCallback();
		string url = "/getAllLeaderboards?serverGuid=" + m_Config.m_sServerGuid;
        ctx.GET(m_fetchAllLeaderboardsCallback, url);
        Print("IA API: Server is fetching all leaderboards.", LogLevel.NORMAL);
    }
	
    private void _RegisterServer()
    {
        if (!m_Config)
            return;
		m_registerCallback = new IA_RegisterServerCallback();
        RestContext ctx = GetGame().GetRestApi().GetContext(m_sApiBaseUrl);
        ctx.SetHeaders("Content-Type,application/json");
		
		string serverName = IA_ApiConfigManager.GetServerNameFromFile();
        IA_ApiRegisterServerRequest requestData = new IA_ApiRegisterServerRequest(serverName, m_Config.m_sOwnerEmail);

     
		Print("Request URL: "+ m_sApiBaseUrl + " requestData In JSON: " + requestData.ToJson() ,LogLevel.NORMAL);
        ctx.POST(m_registerCallback, "/registerServer", requestData.ToJson());
        Print("IA API: Attempting to register server...", LogLevel.NORMAL);
    }
} 