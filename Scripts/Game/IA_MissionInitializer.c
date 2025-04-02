///////////////////////////////////////////////////////////////////////
// IA_MissionInitializerClass & IA_MissionInitializer
// Place this as a GenericEntity in your world with the script set to IA_MissionInitializer.
// It runs once on the server to set up IA_Game, replication, and dynamic areas/tasks.
///////////////////////////////////////////////////////////////////////
class IA_MissionInitializerClass : GenericEntityClass
{
};



class IA_MissionInitializer : GenericEntity
{

	
    // Simple flag to ensure we only run once.
    protected bool m_bInitialized = false;
	ref array<IA_AreaMarker> m_shuffledMarkers = {};
	int m_currentIndex = 0;
	IA_AreaInstance m_currentAreaInstance = null;

	int GetCurrentIndex(){
		return m_currentIndex;
	}
	void ProceedToNextZone()
	{
	    if (m_currentIndex >= m_shuffledMarkers.Count())
	    {
	        //Print("[INFO] All zones initialized.", LogLevel.NORMAL);
	        return;
	    }
	
	    IA_AreaMarker marker = m_shuffledMarkers[m_currentIndex];
	    if (!marker)
	    {
	        m_currentIndex++;
	        ProceedToNextZone();
	        return;
	    }
	
	    vector pos = marker.GetOrigin();
	    string name = marker.GetAreaName();
	    float radius = marker.GetRadius();
	
	    IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
	    m_currentAreaInstance = IA_Game.Instantiate().AddArea(area, IA_Faction.USSR, 0);
	
	    string taskTitle = "Capture " + area.GetName();
	    string taskDesc = "Eliminate enemy presence and secure " + area.GetName();
	    m_currentAreaInstance.QueueTask(taskTitle, taskDesc, pos);
	
	    //Print("[INFO] Initialized area: " + name, LogLevel.NORMAL);
	
	    // Monitor completion
	    GetGame().GetCallqueue().CallLater(CheckCurrentZoneComplete, 5000, true);
	}

	
	void InitializeNow()
{
    //Print("[DEBUG] IA_MissionInitializer: InitializeNow started.", LogLevel.NORMAL);

    m_shuffledMarkers = {};
    array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
    if (markers.IsEmpty())
    {
        //Print("[WARNING] No IA_AreaMarkers found!", LogLevel.WARNING);
        return;
    }

    // Randomize order
    while (!markers.IsEmpty())
    {
        int i = IA_Game.rng.RandInt(0, markers.Count());
        m_shuffledMarkers.Insert(markers[i]);
        markers.Remove(i);
    }

    m_currentIndex = 0;
    ProceedToNextZone();
}


	
	
	void CheckCurrentZoneComplete()
{
    if (!m_currentAreaInstance)
        return;

    if (!m_currentAreaInstance.GetCurrentTaskEntity())
        return;
		
	IA_AreaMarker marker = m_shuffledMarkers[m_currentIndex];

	if(marker.m_FactionScores.Get("US") >= 5){
			m_currentAreaInstance.GetCurrentTaskEntity().Finish();
			
			
		}

    if (m_currentAreaInstance.GetCurrentTaskEntity().GetTaskState() == SCR_TaskState.FINISHED && !m_currentAreaInstance.HasPendingTasks())
    {
        //Print("[INFO] Zone complete. Proceeding to next.", LogLevel.NORMAL);
        m_currentIndex++;
        m_currentAreaInstance = null;
        GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete); // Clean up the loop
        ProceedToNextZone();
    }
}

	void InitDelayed(IEntity owner){
	
		//Print("[DEBUG] IA_MissionInitializer EOnInit called.", LogLevel.NORMAL);

        // Only execute on the server
        if (!Replication.IsServer())
        {
            //Print("[IA_MissionInitializer] Client/Proxy detected => skipping area initialization.", LogLevel.NORMAL);
			
            return;
        }

        // Prevent re-initialization
        if (m_bInitialized)
        {
            //Print("[DEBUG] IA_MissionInitializer already initialized. Exiting.", LogLevel.NORMAL);
            return;
        }
		
		
		
        m_bInitialized = true;

        //Print("[IA_MissionInitializer] Running on Server. Beginning dynamic area setup...", LogLevel.NORMAL);

        // 1) Check that IA_ReplicationWorkaround is present.
        if (!IA_ReplicationWorkaround.Instance())
        {
            //Print("[IA_MissionInitializer] WARNING: No IA_ReplicationWorkaround entity found!", LogLevel.ERROR);
            //Print("[IA_MissionInitializer] Please place a GenericEntity with the IA_ReplicationWorkaround script in the world!", LogLevel.NORMAL);
        }
       	//Print("[IA_MissionInitializer] DEBUG 5!", LogLevel.ERROR);

        // 2) Initialize the IA_Game Singleton.
        IA_Game game = IA_Game.Instantiate();

        // 3) Set the RNG seed.
        int seed = System.GetUnixTime();
        IA_Game.rng.SetSeed(seed);
        //Print("[IA_MissionInitializer] RNG Seed set to: " + seed, LogLevel.NORMAL);

        // 4) Retrieve all IA_AreaMarker entities from the static registry.
        array<IA_AreaMarker> markerMarkers = IA_AreaMarker.GetAllMarkers();
		
		
		
		GetGame().GetCallqueue().CallLater(InitializeNow, 200, false);
		
		
      

        //Print("[IA_MissionInitializer] Dynamic areas/tasks set up. Initialization complete.", LogLevel.NORMAL);
	
	}
	
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }
}
