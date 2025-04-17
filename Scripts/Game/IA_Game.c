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
	static private bool beenInstantiated = false;
    static private ref array<IEntity> m_entityGc = {};

    // Static reference to the currently active area instance
    static IA_AreaInstance CurrentAreaInstance = null;

    private bool m_hasInit = false;

    // Player scaling system
    private int m_lastPlayerCount = 0;
    private int m_playerCheckTimer = 0;
    
    // Scaling factors for different player counts
    private const float BASELINE_PLAYER_COUNT = 10;  // AI count reaches 1.0 at this player count
    private const float MEDIUM_PLAYER_COUNT = 30;
    private const float HIGH_PLAYER_COUNT = 50;
    private const float MAX_PLAYER_COUNT = 100;
    
    // Scale caps
    private const float MIN_SCALE_FACTOR = 0.4;      // Minimum scaling for solo players
    private const float BASELINE_SCALE_FACTOR = 1.0; // Baseline scaling (at BASELINE_PLAYER_COUNT)
    private const float MAX_SCALE_FACTOR = 1.6;      // Maximum scaling cap for high player counts

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
    static int GetUSPlayerCount()
    {
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
		return 1.6;
        // Get current player count
        int playerCount = GetUSPlayerCount();
        
        // Handle zero players explicitly
        if (playerCount <= 0) return MIN_SCALE_FACTOR;
        
        // For 1 to BASELINE players: linear scaling from MIN to BASELINE
        if (playerCount <= BASELINE_PLAYER_COUNT) {
            if (playerCount <= 1) return MIN_SCALE_FACTOR;
            return MIN_SCALE_FACTOR + ((BASELINE_SCALE_FACTOR - MIN_SCALE_FACTOR) * (playerCount - 1) / (BASELINE_PLAYER_COUNT - 1));
        }
        
        // For BASELINE to MEDIUM players: slower growth
        else if (playerCount <= MEDIUM_PLAYER_COUNT) {
            float mediumScaleFactor = BASELINE_SCALE_FACTOR + 0.2; // +0.2 at MEDIUM_PLAYER_COUNT (1.2)
            return BASELINE_SCALE_FACTOR + ((mediumScaleFactor - BASELINE_SCALE_FACTOR) * (playerCount - BASELINE_PLAYER_COUNT) / (MEDIUM_PLAYER_COUNT - BASELINE_PLAYER_COUNT));
        }
        
        // For MEDIUM to HIGH players: continued growth
        else if (playerCount <= HIGH_PLAYER_COUNT) {
            float highScaleFactor = BASELINE_SCALE_FACTOR + 0.4; // +0.4 at HIGH_PLAYER_COUNT (1.4)
            return (BASELINE_SCALE_FACTOR + 0.2) + ((highScaleFactor - (BASELINE_SCALE_FACTOR + 0.2)) * (playerCount - MEDIUM_PLAYER_COUNT) / (HIGH_PLAYER_COUNT - MEDIUM_PLAYER_COUNT));
        }
        
        // For HIGH to MAX players: final increase to max cap
        else if (playerCount <= MAX_PLAYER_COUNT) {
            return (BASELINE_SCALE_FACTOR + 0.4) + ((MAX_SCALE_FACTOR - (BASELINE_SCALE_FACTOR + 0.4)) * (playerCount - HIGH_PLAYER_COUNT) / (MAX_PLAYER_COUNT - HIGH_PLAYER_COUNT));
        }
        
        // Cap at MAX_SCALE_FACTOR for extremely high player counts
        else {
            return MAX_SCALE_FACTOR;
        }
    }
    
    // Calculate max vehicles based on player count (also using a more gentle scaling curve)
    static int GetMaxVehiclesForPlayerCount(int baseMaxVehicles = 3)
    {
        int playerCount = GetUSPlayerCount();
        
        // Logarithmic-style scaling for vehicles
        // Base 0-15 players: baseline vehicles
        if (playerCount <= BASELINE_PLAYER_COUNT)
            return baseMaxVehicles;
        // 16-30 players: +1 vehicle
        else if (playerCount <= MEDIUM_PLAYER_COUNT)
            return baseMaxVehicles + 1;
        // 31-50 players: +2 vehicles
        else if (playerCount <= HIGH_PLAYER_COUNT)
            return baseMaxVehicles + 2;
        // 51-100 players: +3 vehicles
        else if (playerCount <= MAX_PLAYER_COUNT)
            return baseMaxVehicles + 3;
        // 100+ players: +4 vehicles (absolute maximum)
        else
            return baseMaxVehicles + 4;
    }
    
    // Update player count and log changes
    void CheckPlayerCount()
    {
        m_playerCheckTimer++;
        if (m_playerCheckTimer < 10) // Check every ~10 seconds
            return;
            
        m_playerCheckTimer = 0;
        int currentPlayerCount = GetUSPlayerCount();
        
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
	    if (m_areas.IsEmpty())
	    {
	        // Log that no areas exist yet so you can see that the task is running.
	        //Print("IA_Game.PeriodicalGameTask: m_areas is empty. Waiting for initialization.", LogLevel.NORMAL);
	        return;
	    }
	    
	    // Check player count for scaling
	    CheckPlayerCount();
	    
	    foreach (IA_AreaInstance areaInst : m_areas)
	    {
				if(!areaInst.m_area){
					//Print("areaInst.m_area is Null!!!",LogLevel.WARNING);
					return;
				}else{
					//Print("areaInst.m_area "+ areaInst.m_area.GetName() +"is NOT Null",LogLevel.NORMAL);
				areaInst.RunNextTask();
				}
	        
	    }
	}
	

    IA_AreaInstance AddArea(IA_Area area, IA_Faction fac, int strength = 0)
    {
		IA_AreaInstance inst = IA_AreaInstance.Create(area, fac, strength);
		if(!inst.m_area){
			//Print("IA_AreaInstance inst.m_area is NULL!", LogLevel.ERROR);
		}
		//Print("Adding Area " + area.GetName(), LogLevel.WARNING);
        m_areas.Insert(inst);
		//Print("m_areas.Count() = " + m_areas.Count(), LogLevel.WARNING);
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep){
            //Print("Adding Area in Replication " + area.GetName(), LogLevel.WARNING);
			rep.AddArea(inst);
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
};
