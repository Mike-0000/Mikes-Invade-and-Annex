///////////////////////////////////////////////////////////////////////
// 9) IA_Game SINGLETON
///////////////////////////////////////////////////////////////////////
class IA_Game
{
	static ref array<ref IA_Area> s_allAreas = {};
    static private ref IA_Game m_instance = null;
    static bool HasInstance()
    {
        return (m_instance != null);
    }

    static ref RandomGenerator rng = new RandomGenerator();

    private bool m_periodicTaskActive = false;
    private ref array<IA_AreaInstance> m_areas = {};
	private ref array<IA_AreaInstance> m_transientAreaInstances = {};
	static private bool beenInstantiated = false;
    static private ref array<IEntity> m_entityGc = {};

    // Static reference to the currently active area instance
    static IA_AreaInstance CurrentAreaInstance = null;

    // --- BEGIN ADDED: Active Group ID ---
    static private int s_activeGroupID = -1;
    // --- END ADDED ---

    // --- BEGIN ADDED: Side Objective Manager Instance ---
    private ref IA_SideObjectiveManager m_SideObjectiveManager;
    // --- END ADDED ---

    // --- BEGIN ADDED: Flag for initial objective scaling ---
    static bool s_isInitialObjectiveSpawning = false;
    // --- END ADDED ---

    private bool m_hasInit = false;

    // Player scaling system
    private int m_lastPlayerCount = 0;
    private int m_playerCheckTimer = 0;
    
    // Scaling factors for different player counts
    private const float BASELINE_PLAYER_COUNT = 7;
    private const float MEDIUM_PLAYER_COUNT = 18;
    private const float HIGH_PLAYER_COUNT = 50;
    private const float MAX_PLAYER_COUNT = 80;
    
    // Scale caps
    private const float MIN_SCALE_FACTOR = 0.8;      // Minimum scaling for solo players
    private const float BASELINE_SCALE_FACTOR = 1.07; // Baseline scaling (at BASELINE_PLAYER_COUNT)
    private const float MAX_SCALE_FACTOR = 1.8;      // Maximum scaling cap for high player counts

    // Static method to set the current area instance
    static void SetCurrentAreaInstance(IA_AreaInstance instance)
    {
        CurrentAreaInstance = instance;
        // Potentially add debug log here
    }

    void Init()
    {
        //Print("[DEBUG] IA_Game.Init called.", LogLevel.NORMAL);
        if (m_hasInit)
            return;

        // --- BEGIN ADDED: Initialize Side Objective Manager ---
        m_SideObjectiveManager = IA_SideObjectiveManager.GetInstance();
        // --- END ADDED ---
        
        // --- BEGIN ADDED: Initialize Stats Manager ---
        if (Replication.IsServer())
        {
            IA_StatsManager.GetInstance();
        }
        // --- END ADDED ---

        // --- BEGIN MODIFIED: Enable initial objective scaling earlier ---
        // Enable scaling as soon as IA_Game is initialized for the first time.
        // This ensures it's active before any mission logic that might depend on player count
        // prior to the first objective group's explicit setup.
        EnableInitialObjectiveScaling();
        // --- END MODIFIED ---
        m_hasInit = true;
        ActivatePeriodicTask();
    }
	IA_AreaInstance getActiveArea(){
		return m_areas[0];
	}
	static IA_Game Instantiate()
	{
		if(!Replication.IsServer()){
			return m_instance;
		}
		//Print("[DEBUG] IA_Game.Instantiate called.", LogLevel.NORMAL);
		
	    if (beenInstantiated){
			//Print("[DEBUG] IA_Game.Instantiate Already Instantiated. = "+beenInstantiated, LogLevel.NORMAL);
	        return m_instance;
		}
		//Print("[DEBUG] IA_Game.Instantiate Going.", LogLevel.NORMAL);
		
	    m_instance = new IA_Game();
	    m_instance.Init();
		beenInstantiated = true;
	    return m_instance;
	}
	

    void ~IA_Game()
    {
        m_areas.Clear();
        m_entityGc.Clear();
    }

    static void AddEntityToGc(IEntity e)
    {
        m_entityGc.Insert(e);
    }

    private static void EntityGcTask()
    {
        if (m_entityGc.IsEmpty())
            return;
        delete m_entityGc[0];
        m_entityGc.Remove(0);
    }
	
	private void ActivatePeriodicTask()
	{
	    if (m_periodicTaskActive)
	        return;
	    m_periodicTaskActive = true;
	    // Use a static wrapper so the instance's PeriodicalGameTask is called correctly.
	    // Increase interval from 200ms to 1000ms (1 second) to reduce processing frequency
	    GetGame().GetCallqueue().CallLater(PeriodicalGameTaskWrapper, 2000, true);
	    GetGame().GetCallqueue().CallLater(EntityGcTask, 500, true);
	}

	// Static wrapper that retrieves the singleton instance and calls its PeriodicalGameTask.
	static void PeriodicalGameTaskWrapper()
	{
	    IA_Game gameInstance = IA_Game.Instantiate();
	    if (gameInstance)
	    {
	        gameInstance.PeriodicalGameTask();
	    }
	}

    // Get current US player count in the game
    static int GetPlayerCount()
    {
        // --- BEGIN MODIFIED: Check for initial objective scaling override ---
        if (s_isInitialObjectiveSpawning)
        {
            Print("[PLAYER_SCALING] GetPlayerCount: Initial objective scaling active, returning BASELINE_PLAYER_COUNT: " + BASELINE_PLAYER_COUNT, LogLevel.DEBUG);
            return BASELINE_PLAYER_COUNT;
        }
        // --- END MODIFIED ---

        int playerCount = 0;
        PlayerManager playerManager = GetGame().GetPlayerManager();
        
        if (!playerManager)
            return 0;
            
        array<int> playerIds = {};
        playerManager.GetAllPlayers(playerIds);
        
        foreach (int playerId : playerIds)
        {
            IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
            if (!playerEntity)
                continue;
			playerCount++;
			
            /*    
            Faction faction = character.GetFaction();
            if (faction && faction.GetFactionKey() == "US")
            {
                playerCount++;
            }*/
        }
        
        return playerCount;
    }
    
    // Calculate scale factor for AI spawning based on player count
    static float GetAIScaleFactor()
    {
        // Check for config override first
        IA_Config config = IA_MissionInitializer.GetGlobalConfig();
        if (config && config.m_fStaticAIScaleOverride > 0)
        {
            Print(string.Format("[PLAYER_SCALING] Using static AI scale override from config: %1", config.m_fStaticAIScaleOverride), LogLevel.DEBUG);
            return config.m_fStaticAIScaleOverride;
        }

        // Get current player count
        int playerCount = GetPlayerCount();
        
        // Calculate dynamic scale factor
        float dynamicScaleFactor;
        
        // Handle zero players explicitly
        if (playerCount <= 0) 
        {
            dynamicScaleFactor = MIN_SCALE_FACTOR;
        }
        // For 1 to BASELINE players: linear scaling from MIN to BASELINE
        else if (playerCount <= BASELINE_PLAYER_COUNT) {
            if (playerCount <= 1) 
                dynamicScaleFactor = MIN_SCALE_FACTOR;
            else
                dynamicScaleFactor = MIN_SCALE_FACTOR + ((BASELINE_SCALE_FACTOR - MIN_SCALE_FACTOR) * (playerCount - 1) / (BASELINE_PLAYER_COUNT - 1));
        }
        // For BASELINE to MEDIUM players: slower growth
        else if (playerCount <= MEDIUM_PLAYER_COUNT) {
            float mediumScaleFactor = BASELINE_SCALE_FACTOR + 0.2; // +0.2 at MEDIUM_PLAYER_COUNT (1.2)
            dynamicScaleFactor = BASELINE_SCALE_FACTOR + ((mediumScaleFactor - BASELINE_SCALE_FACTOR) * (playerCount - BASELINE_PLAYER_COUNT) / (MEDIUM_PLAYER_COUNT - BASELINE_PLAYER_COUNT));
        }
        // For MEDIUM to HIGH players: continued growth
        else if (playerCount <= HIGH_PLAYER_COUNT) {
            float highScaleFactor = BASELINE_SCALE_FACTOR + 0.4; // +0.4 at HIGH_PLAYER_COUNT (1.4)
            dynamicScaleFactor = (BASELINE_SCALE_FACTOR + 0.2) + ((highScaleFactor - (BASELINE_SCALE_FACTOR + 0.2)) * (playerCount - MEDIUM_PLAYER_COUNT) / (HIGH_PLAYER_COUNT - MEDIUM_PLAYER_COUNT));
        }
        // For HIGH to MAX players: final increase to max cap
        else if (playerCount <= MAX_PLAYER_COUNT) {
            dynamicScaleFactor = (BASELINE_SCALE_FACTOR + 0.4) + ((MAX_SCALE_FACTOR - (BASELINE_SCALE_FACTOR + 0.4)) * (playerCount - HIGH_PLAYER_COUNT) / (MAX_PLAYER_COUNT - HIGH_PLAYER_COUNT));
        }
        // Cap at MAX_SCALE_FACTOR for extremely high player counts
        else {
            dynamicScaleFactor = MAX_SCALE_FACTOR;
        }

        // Apply multiplier from config if set
        if (config && config.m_fAIScaleMultiplier != 1.0)
        {
            float finalScaleFactor = dynamicScaleFactor * config.m_fAIScaleMultiplier;
            Print(string.Format("[PLAYER_SCALING] Dynamic scale %1 multiplied by config multiplier %2 = %3", dynamicScaleFactor, config.m_fAIScaleMultiplier, finalScaleFactor), LogLevel.DEBUG);
            return finalScaleFactor;
        }

        // Return unmodified dynamic scale factor
        return dynamicScaleFactor;
    }
    
    // Calculate max vehicles based on player count (also using a more gentle scaling curve)
    static int GetMaxVehiclesForPlayerCount(int baseMaxVehicles = 5)
    {
        int playerCount = GetPlayerCount();
        Print("Base Max Vehicle = " + baseMaxVehicles, LogLevel.NORMAL);
        // Logarithmic-style scaling for vehicles
        // Base 0-15 players: baseline vehicles
        if (playerCount <= BASELINE_PLAYER_COUNT)
            return baseMaxVehicles;
        // 6-20 players: +1 vehicle
        else if (playerCount <= MEDIUM_PLAYER_COUNT)
            return baseMaxVehicles + 2;
        // 20-50 players: +2 vehicles
        else if (playerCount <= HIGH_PLAYER_COUNT)
            return baseMaxVehicles + 4;
        // 51-80 players: +3 vehicles
        else if (playerCount <= MAX_PLAYER_COUNT)
            return baseMaxVehicles + 5;
        // 80+ players: +4 vehicles (absolute maximum)
        else
            return baseMaxVehicles + 6;
    }
    
    // Update player count and log changes
    void CheckPlayerCount()
    {
        m_playerCheckTimer++;
        if (m_playerCheckTimer < 10) // Check every ~10 seconds
            return;
            
        m_playerCheckTimer = 0;
        int currentPlayerCount = GetPlayerCount();
        
        if (currentPlayerCount != m_lastPlayerCount)
        {
            m_lastPlayerCount = currentPlayerCount;
            float aiScale = GetAIScaleFactor();
            int maxVehicles = GetMaxVehiclesForPlayerCount();
            
           Print("[PLAYER_SCALING] Player count changed to " + currentPlayerCount + 
                 ". AI Scale Factor: " + aiScale + 
                  ", Max Vehicles: " + maxVehicles, LogLevel.NORMAL);
                  
            // Update all area instances with new scaling
            foreach (IA_AreaInstance areaInst : m_areas)
            {
                if (!areaInst || !areaInst.m_area)
                    continue;
                    
                areaInst.UpdatePlayerScaling(currentPlayerCount, aiScale, maxVehicles);
            }
        }
    }

	void PeriodicalGameTask()
	{
	    //Print("IA_Game.PeriodicalGameTask: Periodical Game Task has started", LogLevel.NORMAL);
	    
	    // Update active defend mission first
	    if (m_activeDefendMission && m_activeDefendMission.IsActive())
	    {
	        m_activeDefendMission.UpdateDefendMission();
	        // Defend mission will handle its own completion and notification
	    }
	    
	    // --- BEGIN MODIFIED: Update Side Objective Manager ---
	    // This should run regardless of main area status
	    if (m_SideObjectiveManager)
	        m_SideObjectiveManager.Update(2.0); // Corresponds to the 2000ms call interval
		
	    if (m_areas.IsEmpty() && m_transientAreaInstances.IsEmpty())
	    {
	        // Log that no areas exist yet so you can see that the task is running.
	        //Print("IA_Game.PeriodicalGameTask: m_areas is empty. Waiting for initialization.", LogLevel.NORMAL);
	        return;
	    }
	    
	    // Check player count for scaling
	    CheckPlayerCount();
	    
	    foreach (IA_AreaInstance areaInst : m_areas)
	    {
				// --- BEGIN ADDED: Filter by Active Group ID ---
				if (areaInst && areaInst.GetAreaGroup() != s_activeGroupID)
				{
					// Print(string.Format("[IA_Game.PeriodicalTask] Skipping Area '%1' (Group %2) - Active Group is %3", 
					// 	areaInst.m_area.GetName(), areaInst.GetAreaGroup(), s_activeGroupID), LogLevel.DEBUG);
					continue;
				}
				// --- END ADDED ---
				
				if(!areaInst || !areaInst.m_area){
					//Print("areaInst.m_area is Null!!!",LogLevel.WARNING);
					continue; // Use continue instead of return to process other areas
				}else{
					//Print("areaInst.m_area "+ areaInst.m_area.GetName() +"is NOT Null",LogLevel.NORMAL);
					areaInst.RunNextTask();
				}
	        
	    }
		
		// --- BEGIN ADDED: Process Transient Areas ---
		// Process transient areas separately, iterating backwards for safe removal
		for (int i = m_transientAreaInstances.Count() - 1; i >= 0; i--)
	    {
	        IA_AreaInstance transientAreaInst = m_transientAreaInstances[i];
	        if (transientAreaInst && transientAreaInst.m_area)
	        {
	            transientAreaInst.RunNextTask();
	        }
	        else
	        {
	            // Clean up invalid transient areas that might have been nulled out elsewhere
	            m_transientAreaInstances.Remove(i);
	        }
	    }
		// --- END ADDED ---
	}
	

    IA_AreaInstance AddArea(IA_Area area, IA_Faction fac, Faction AreaFaction, int strength = 0, int groupID = -1)
    {
        IA_AreaInstance inst = IA_AreaInstance.Create(area, fac, AreaFaction, strength, groupID);
        
        if(!inst || !inst.m_area){
            Print("IA_Game.AddArea: Failed to create or received null IA_AreaInstance or area!", LogLevel.ERROR);
            return null;
        }

        Print(string.Format("[IA_Game.AddArea] Adding Area '%1' (Group %2) to m_areas.", area.GetName(), groupID), LogLevel.WARNING);
        m_areas.Insert(inst);
        Print(string.Format("[IA_Game.AddArea] m_areas.Count() = %1 after adding %2", m_areas.Count(), area.GetName()), LogLevel.WARNING);

        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep){
            // Print("Adding Area to ReplicationWorkaround: " + area.GetName(), LogLevel.WARNING);
            rep.AddArea(inst); // Add to replication system
        }
        return inst;
    }

    IA_AreaInstance FindAreaInstance(string name)
    {
        foreach (IA_AreaInstance a : m_areas)
        {
            if (a.m_area.GetName() == name)
                return a;
        }
        return null;
    }

    // --- BEGIN ADDED: Set Active Group ID ---
    static void SetActiveGroupID(int groupID)
    {
        s_activeGroupID = groupID;
        // Print(string.Format("[IA_Game] Active group ID set to: %1", s_activeGroupID), LogLevel.NORMAL);
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Clear All Areas ---
    void ClearAllAreas()
    {
        if (m_areas)
        {
            //Print(string.Format("[IA_Game.ClearAllAreas] Clearing %1 existing area instances.", m_areas.Count()), LogLevel.NORMAL);
            foreach (IA_AreaInstance areaInst : m_areas)
            {
                if (areaInst && areaInst.m_area)
                {
                    Print(string.Format("[IA_Game.ClearAllAreas] Removing area instance: %1", areaInst.m_area.GetName()), LogLevel.DEBUG);
                }
            }
            m_areas.Clear();
        }
        else
        {
            //Print("[IA_Game.ClearAllAreas] m_areas was null, initializing.", LogLevel.DEBUG);
            m_areas = new array<IA_AreaInstance>();
        }
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Clear All Area Definitions ---
    static void ClearAllAreaDefinitions()
    {
        if (s_allAreas)
        {
            //Print(string.Format("[IA_Game.ClearAllAreaDefinitions] Clearing %1 existing area definitions.", s_allAreas.Count()), LogLevel.NORMAL);
            s_allAreas.Clear();
        }
        else
        {
            //Print("[IA_Game.ClearAllAreaDefinitions] s_allAreas was null, initializing.", LogLevel.DEBUG);
            s_allAreas = new array<ref IA_Area>();
        }
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Methods to control initial objective scaling ---
    static void EnableInitialObjectiveScaling()
    {
        s_isInitialObjectiveSpawning = true;
        Print("[PLAYER_SCALING] Initial objective scaling ENABLED. Using BASELINE_PLAYER_COUNT for upcoming spawns.", LogLevel.NORMAL);
    }

    static void DisableInitialObjectiveScaling()
    {
        s_isInitialObjectiveSpawning = false;
        Print("[PLAYER_SCALING] Initial objective scaling DISABLED. Reverting to actual player count.", LogLevel.NORMAL);
    }
    // --- END ADDED ---
    
    // --- BEGIN ADDED: Defend Mission Management ---
    private ref IA_DefendMission m_activeDefendMission = null;
    
    void SetActiveDefendMission(IA_DefendMission mission)
    {
        m_activeDefendMission = mission;
        Print("[IA_Game] Active defend mission set", LogLevel.NORMAL);
    }
    
    IA_DefendMission GetActiveDefendMission()
    {
        return m_activeDefendMission;
    }
    
    bool HasActiveDefendMission()
    {
        return (m_activeDefendMission != null && m_activeDefendMission.IsActive());
    }
    
    array<IA_AreaInstance> GetAreaInstances()
    {
        return m_areas;
    }

    static IA_AreaInstance GetAreaForPosition(vector pos)
    {
        IA_Game game = IA_Game.Instantiate();
        if (!game)
            return null;

        foreach (IA_AreaInstance areaInst : game.GetAreaInstances())
        {
            if (areaInst && areaInst.GetArea() && areaInst.GetArea().IsPositionInside(pos))
            {
                return areaInst;
            }
        }
        return null;
    }

    // --- BEGIN ADDED: Helper to get instance by name ---
    IA_AreaInstance GetAreaInstance(string name)
    {
        foreach (IA_AreaInstance instance : m_areas)
        {
            if (instance && instance.m_area && instance.m_area.GetName() == name)
                return instance;
        }
        return null;
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Global Notification Helper ---
    static void S_TriggerGlobalNotification(string messageType, string message)
    {
        array<int> playerIDs = {};
        GetGame().GetPlayerManager().GetAllPlayers(playerIDs);
        foreach (int playerID : playerIDs)
        {
            PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerID);
            if(pc)
            {
                SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
                if(character)
                {
                    character.SetUIOne(messageType, message, playerID);
                }
            }
        }
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Transient Area Management ---
    void AddTransientArea(IA_AreaInstance inst)
    {
        if (inst && m_transientAreaInstances.Find(inst) == -1)
        {
            Print(string.Format("[IA_Game.AddTransientArea] Adding Transient Area '%1' to m_transientAreaInstances.", inst.GetArea().GetName()), LogLevel.DEBUG);
            m_transientAreaInstances.Insert(inst);
        }
    }
    
    void RemoveTransientArea(IA_AreaInstance inst)
    {
        if (inst)
        {
            int index = m_transientAreaInstances.Find(inst);
            if (index != -1)
            {
                Print(string.Format("[IA_Game.RemoveTransientArea] Removing Transient Area '%1' from m_transientAreaInstances.", inst.GetArea().GetName()), LogLevel.DEBUG);
                m_transientAreaInstances.Remove(index);
            }
        }
    }
    // --- END ADDED ---
};
