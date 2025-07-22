// Scripts/Game/IA_LeaderboardManagerComponent.c
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Manages and replicates the global leaderboard data.")]
class IA_LeaderboardManagerComponentClass: ScriptComponentClass
{
};

class IA_LeaderboardManagerComponent : ScriptComponent
{
    [RplProp(onRplName: "OnLeaderboardDataChanged")]
    private string m_sReplicatedLeaderboardJson;
	
	[RplProp(onRplName: "OnServerLeaderboardDataChanged")]
    private string m_sReplicatedServerLeaderboardJson;
	
	[RplProp(onRplName: "OnGlobalServerLeaderboardDataChanged")]
	private string m_sReplicatedGlobalServerLeaderboardJson;
	
	private const int LEADERBOARD_FETCH_INTERVAL = 30; // 30 seconds

    private ref ScriptInvoker m_OnLeaderboardDataUpdated;
	private ref ScriptInvoker m_OnServerLeaderboardDataUpdated;
	private ref ScriptInvoker m_OnGlobalServerLeaderboardDataUpdated;
    private static IA_LeaderboardManagerComponent s_Instance;

    static IA_LeaderboardManagerComponent GetInstance()
    {
        return s_Instance;
    }

    ScriptInvoker GetOnLeaderboardDataUpdated()
    {
        if (!m_OnLeaderboardDataUpdated)
            m_OnLeaderboardDataUpdated = new ScriptInvoker();
        return m_OnLeaderboardDataUpdated;
    }
	
	ScriptInvoker GetOnServerLeaderboardDataUpdated()
    {
        if (!m_OnServerLeaderboardDataUpdated)
            m_OnServerLeaderboardDataUpdated = new ScriptInvoker();
        return m_OnServerLeaderboardDataUpdated;
    }
	
	ScriptInvoker GetOnGlobalServerLeaderboardDataUpdated()
    {
        if (!m_OnGlobalServerLeaderboardDataUpdated)
            m_OnGlobalServerLeaderboardDataUpdated = new ScriptInvoker();
        return m_OnGlobalServerLeaderboardDataUpdated;
    }

    //------------------------------------------------------------------------------------------------
    // OVERRIDES
    //------------------------------------------------------------------------------------------------
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        SetEventMask(owner, EntityEvent.INIT);
    }
    
    override void EOnInit(IEntity owner)
    {
        if (s_Instance)
        {
            Print("IA_LeaderboardManagerComponent already exists. Deleting this one.", LogLevel.WARNING);
            //delete this;
            return;
        }
        s_Instance = this;

        // Fetching is now handled by IA_ApiHandler on init and after stat submission.
        // No longer need to call fetch from here.

        if (!Replication.IsServer())
		{
            GetOnLeaderboardDataUpdated().Invoke(m_sReplicatedLeaderboardJson);
			GetOnServerLeaderboardDataUpdated().Invoke(m_sReplicatedServerLeaderboardJson);
			GetOnGlobalServerLeaderboardDataUpdated().Invoke(m_sReplicatedGlobalServerLeaderboardJson);
		}
    }

    override void OnDelete(IEntity owner)
    {
        if (s_Instance == this)
            s_Instance = null;

        super.OnDelete(owner);
    }

    //------------------------------------------------------------------------------------------------
    // PUBLIC API
    //------------------------------------------------------------------------------------------------

    // Called by the API handler on the server
    void UpdateLeaderboardData(string jsonData)
    {
        if (!Replication.IsServer())
        {
            Print("UpdateLeaderboardData can only be called on the server.", LogLevel.ERROR);
            return;
        }

        m_sReplicatedLeaderboardJson = jsonData;
        Replication.BumpMe();

        // Also invoke for server-side logic if needed
        GetOnLeaderboardDataUpdated().Invoke(jsonData);
    }
	
	void UpdateServerLeaderboardData(string jsonData)
    {
        if (!Replication.IsServer())
        {
            Print("UpdateServerLeaderboardData can only be called on the server.", LogLevel.ERROR);
            return;
        }

        m_sReplicatedServerLeaderboardJson = jsonData;
        Replication.BumpMe();

        // Also invoke for server-side logic if needed
        GetOnServerLeaderboardDataUpdated().Invoke(jsonData);
    }
	
	void UpdateGlobalServerLeaderboardData(string jsonData)
	{
		if(!Replication.IsServer())
		{
			Print("UpdateGlobalServerLeaderboardData can only be called on the server.", LogLevel.ERROR);
			return;
		}
		
		m_sReplicatedGlobalServerLeaderboardJson = jsonData;
		Replication.BumpMe();
		
		GetOnGlobalServerLeaderboardDataUpdated().Invoke(jsonData);
	}
	
	// These methods are no longer needed as the ApiHandler drives the updates.
	/*
	private void FetchLeaderboardData()
    {
        if (!Replication.IsServer()) return;

        Print("IA_LeaderboardManagerComponent: Server is fetching new leaderboard data.", LogLevel.NORMAL);
        IA_ApiHandler.GetInstance().FetchGlobalLeaderboardForServer();
    }
	
	private void FetchServerLeaderboardData()
    {
        if (!Replication.IsServer()) return;

        Print("IA_LeaderboardManagerComponent: Server is fetching new SERVER-ONLY leaderboard data.", LogLevel.NORMAL);
        IA_ApiHandler.GetInstance().FetchServerLeaderboard();
    }
	
	private void FetchGlobalServerLeaderboardData()
	{
		if (!Replication.IsServer()) return;
		
		Print("IA_LeaderboardManagerComponent: Server is fetching new GLOBAL SERVER leaderboard data.", LogLevel.NORMAL);
		IA_ApiHandler.GetInstance().FetchGlobalServerLeaderboard();
	}
	*/
    
    string GetCachedLeaderboardData()
    {
        return m_sReplicatedLeaderboardJson;
    }
	
	string GetCachedServerLeaderboardData()
    {
        return m_sReplicatedServerLeaderboardJson;
    }
	
	string GetCachedGlobalServerLeaderboardData()
	{
		return m_sReplicatedGlobalServerLeaderboardJson;
	}

    //------------------------------------------------------------------------------------------------
    // REPLICATION
    //------------------------------------------------------------------------------------------------
    private void OnLeaderboardDataChanged()
    {
        // This is called on clients when the data arrives
        Print("IA_LeaderboardManagerComponent: Replicated leaderboard data received on client.", LogLevel.NORMAL);
        GetOnLeaderboardDataUpdated().Invoke(m_sReplicatedLeaderboardJson);
    }
	
	private void OnServerLeaderboardDataChanged()
    {
        // This is called on clients when the server-specific data arrives
        Print("IA_LeaderboardManagerComponent: Replicated SERVER leaderboard data received on client.", LogLevel.NORMAL);
        GetOnServerLeaderboardDataUpdated().Invoke(m_sReplicatedServerLeaderboardJson);
    }
	
	private void OnGlobalServerLeaderboardDataChanged()
	{
		// This is called on clients when the global server data arrives
		Print("IA_LeaderboardManagerComponent: Replicated GLOBAL SERVER leaderboard data received on client.", LogLevel.NORMAL);
		GetOnGlobalServerLeaderboardDataUpdated().Invoke(m_sReplicatedGlobalServerLeaderboardJson);
	}
}; 