enum IA_GroupDangerType
{
    ProjectileImpact,
    WeaponFire,
    ProjectileFlyby,
    Explosion,
    EnemySpotted
}

// Add this enum after the existing IA_GroupDangerType enum
enum IA_GroupTacticalState
{
    Neutral,       // Default state, no specific tactical objective
    Attacking,     // Actively seeking and engaging enemies
    Defending,     // Holding position and engaging enemies that approach
    Retreating,    // Falling back to regroup/recover
    Flanking,      // Attempting to move around enemies to hit from side/rear
    LastStand,     // Final defensive posture when heavily outnumbered
    Regrouping,     // Gathering forces before next action
    DefendPatrol,    // Patrol behavior using defend orders
    InVehicle,       // Group is currently assigned to a vehicle
	Escaping,      // Unconditionally moving to an escape point, ignoring combat
}



// Class to track danger events for processing
class IA_GroupDangerEvent
{
    IA_GroupDangerType m_type;
    vector m_position;
    IEntity m_source;
    float m_intensity;
    bool m_isSuppressed;
    int m_time;
}

// Class to handle async road search for group spawning
class IA_RoadSearchState
{
    // Search parameters
    vector m_initialPos;
    IA_Faction m_faction;
    int m_unitCount;
    Faction m_areaFaction;
    int m_activeGroup;
    bool m_useExactPosition = false;
    
    // Search state
    int m_currentDistanceIndex = 0;
    int m_alternativeAttempt = 0;
    bool m_tryingAlternatives = false;
    vector m_foundSpawnPos = vector.Zero;
    bool m_roadFound = false;
    
    // Search configuration
    ref array<int> m_searchDistances = {25, 50, 75, 100, 200, 300, 400, 800, 1200};
    static const int ALTERNATIVE_ATTEMPTS = 3;
    static const int SEARCH_DELAY_MS = 150; // Delay between search attempts
    
    // Callback for when search completes
    IA_AreaInstance m_callbackInstance = null;
    string m_callbackMethod = "";
    
    void IA_RoadSearchState(vector initialPos, IA_Faction faction, int unitCount, Faction areaFaction, int activeGroup, bool useExactPosition = false)
    {
        m_initialPos = initialPos;
        m_faction = faction;
        m_unitCount = unitCount;
        m_areaFaction = areaFaction;
        m_activeGroup = activeGroup;
        m_useExactPosition = useExactPosition;
    }
    
    void SetCallback(IA_AreaInstance instance, string methodName)
    {
        m_callbackInstance = instance;
        m_callbackMethod = methodName;
    }
}

class IA_AiGroup
{
    private SCR_AIGroup m_group;
    private bool        m_isSpawned = false;
    private bool        m_isCivilian = false;
    private IA_SquadType m_squadType;
    private IA_Faction   m_faction;
    
    private IA_SideObjective m_OwningSideObjective;
    
    private vector      m_initialPosition;
    private vector      m_lastOrderPosition;
    private vector      m_lastConfirmedPosition = vector.Zero;
    private int         m_lastOrderTime = 0;
    
    // Vehicle movement state
    private bool        m_isDriving = false;
    private IEntity     m_referencedEntity = null;
    private vector      m_drivingTarget = vector.Zero;
    private int         m_lastVehicleOrderTime = 0;
    private const int   VEHICLE_ORDER_UPDATE_INTERVAL = 5;
    
    // Danger state tracking
    private float       m_currentDangerLevel = 0.0;
    private int         m_lastDangerEventTime = 0;
    private vector      m_lastDangerPosition = vector.Zero;
    private ref array<ref IA_GroupDangerEvent> m_dangerEvents = {};
    private int         m_consecutiveDangerEvents = 0;
    
    // Reaction system constants and variables
    private const float BULLET_IMPACT_DISTANCE_MAX = 10.0;
    private const float PROJECTILE_FLYBY_RADIUS = 13.0;
    private const float GUNSHOT_AUDIBLE_DISTANCE = 500.0;
    private const float SUPPRESSED_AUDIBLE_DISTANCE = 90.0;
    private const float EXPLOSION_REACT_DISTANCE = 220.0;
    private const float VEHICLE_PRIORITY_MOVE_LEVEL = 200.0;
	private const float CIVILIAN_VEHICLE_PRIORITY_MOVE_LEVEL = 400.0;

    // Logging and rate limiting
    private static const int BEHAVIOR_LOG_RATE_LIMIT_SECONDS = 3;
    private int m_lastDangerDetectLogTime = 0;
    private int m_lastDangerTriggerLogTime = 0;
    private int m_lastApplyUnderFireLogTime = 0;
    private int m_lastApplyEnemySpottedLogTime = 0;
    
    // Combat state tracking
    IA_Faction m_engagedEnemyFaction = IA_Faction.NONE;
    
    // State evaluation scheduling
    private int m_nextStateEvaluationTime = 0;
    private const int MIN_STATE_EVALUATION_INTERVAL = 3000; // milliseconds, increased from 5000
    private const int MAX_STATE_EVALUATION_INTERVAL = 4000; // milliseconds, increased from 15000
    private bool m_isStateEvaluationScheduled = false;
    
    // Tactical state
    private IA_GroupTacticalState m_tacticalState = IA_GroupTacticalState.Neutral;
    private int m_tacticalStateStartTime = 0;
    private int m_tacticalStateMinDuration = 60; // seconds, increased from 30
    private vector m_tacticalStateTarget = vector.Zero;
    private IEntity m_tacticalStateEntity = null;
    private IA_Area m_lastAssignedArea = null;
    private bool m_isStateManagedByAuthority = false; // New flag

    // Danger event processing performance optimization
    private const int AGENT_EVENT_CHECK_INTERVAL_MS = Math.RandomInt(700,800); // Check agent events every 300ms
    private int m_lastAgentEventCheckTime = 0;
    private static const int DANGER_PROCESS_INTERVAL_MS = Math.RandomInt(300,400); // Process at most one danger event every 150ms
    private int m_lastDangerProcessTime = 0;

    // Flanking state tracking
    private bool m_isInFlankingPhase = false; // True if in first phase of flanking (moving to flank position)
    private vector m_originalThreatPosition = vector.Zero; // The original position that triggered the flanking maneuver

    private int m_initialUnitCount = 0; // Store the original number of units in the group

    // Add after the m_isStateManagedByAuthority variable
    private bool m_hasPendingStateRequest = false;
    private IA_GroupTacticalState m_requestedState = IA_GroupTacticalState.Neutral;
    private vector m_requestedStatePosition = vector.Zero;
    private IEntity m_requestedStateEntity = null;
	private IA_AreaInstance m_owningAreaInstance;

	Faction m_groupFaction;
	
    // Defend mission mode
    private bool m_isInDefendMode = false;
    private vector m_defendTarget = vector.Zero;
    
    // Staggered spawning state
    private int m_pendingUnitsToSpawn = 0;
    private int m_unitsSpawnedCount = 0;
    private vector m_staggeredSpawnPos = vector.Zero;
    private IA_Faction m_staggeredSpawnFaction = IA_Faction.NONE;
    private Faction m_staggeredAreaFaction = null;
	
	// Assassination obj
	
	bool m_HVTGroup = false;
	
    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac, int unitCount, bool HVTGroup = false)
    {
		
        m_initialPosition = initialPos;
        m_squadType = squad;
        m_faction = fac;
        m_initialUnitCount = unitCount;
		m_HVTGroup = HVTGroup;
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction, Faction AreaFaction)
	{
        // Redirect to CreateMilitaryGroupFromUnits instead of using prefabs
        // Calculate unit count based on squad type
        int unitCount = IA_SquadCount(squadType, faction);
        
        // Create group using the new unit-based approach
        return CreateMilitaryGroupFromUnits(initialPos, faction, unitCount, AreaFaction);
	}

	string GetRandomUnitPrefab(IA_Faction faction, Faction DesiredFaction){

		/*
        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) {
            return "";
        }
		Faction USFaction = factionManager.GetFactionByKey("US");
        array<Faction> actualFactions = {};
		array<Faction> factionGet = {};
        if (faction == IA_Faction.US)
            actualFactions.Insert(USFaction);
        else if (faction == IA_Faction.USSR){
			factionManager.GetFactionsList(factionGet);
			foreach (Faction currentFaction : factionGet){
				if(USFaction.IsFactionEnemy(currentFaction))
					actualFactions.Insert(currentFaction); // Build List of Enemy Factions
			}
		}
        else {
            return "";
        }
		*/
		int randInt = Math.RandomInt(0,12);

		array<EEditableEntityLabel> includedLabels = {};

		if(faction == IA_Faction.USSR){
			switch(randInt){
				case 11:
				case 0: // ROLE_ANTITANK
					includedLabels = {EEditableEntityLabel.ROLE_ANTITANK};
					break;
				case 1:
					includedLabels = {EEditableEntityLabel.ROLE_GRENADIER};
					break;
				//case 2:
				//	includedLabels = {EEditableEntityLabel.ROLE_LEADER};
				//	break;
				case 2:
				case 3: 
					includedLabels = {EEditableEntityLabel.ROLE_MACHINEGUNNER};
					break;
				case 4: 
					includedLabels = {EEditableEntityLabel.ROLE_MEDIC};
					break;
				case 5: 
					includedLabels = {EEditableEntityLabel.ROLE_RADIOOPERATOR};
					break;
				case 10:
				case 6:
					includedLabels = {EEditableEntityLabel.ROLE_RIFLEMAN};
					break;
				case 7:
					includedLabels = {EEditableEntityLabel.ROLE_SHARPSHOOTER};
					break;
				case 8: 
					includedLabels = {EEditableEntityLabel.ROLE_AMMOBEARER};
					break;
				case 9: 
					includedLabels = {EEditableEntityLabel.ROLE_SCOUT};
					break;
				case 12: 
					includedLabels = {EEditableEntityLabel.ROLE_SAPPER};
					break;
			}
		}
		// --- BEGIN NEW US FACTION LOGIC ---
		else if (faction == IA_Faction.US)
		{
		    // Similar role distribution for US faction
		    switch(randInt)
		    {
				case 10:
		        case 0: includedLabels = {EEditableEntityLabel.ROLE_ANTITANK}; break;
		        case 1: includedLabels = {EEditableEntityLabel.ROLE_GRENADIER}; break;
		        case 2: includedLabels = {EEditableEntityLabel.ROLE_LEADER}; break;
		        case 3: includedLabels = {EEditableEntityLabel.ROLE_MACHINEGUNNER}; break;
		        case 4: includedLabels = {EEditableEntityLabel.ROLE_MEDIC}; break;
		        case 5: includedLabels = {EEditableEntityLabel.ROLE_RADIOOPERATOR}; break;
		        case 10:
		        case 6: includedLabels = {EEditableEntityLabel.ROLE_RIFLEMAN}; break;
		        case 7: includedLabels = {EEditableEntityLabel.ROLE_SHARPSHOOTER}; break;
		        case 8: includedLabels = {EEditableEntityLabel.ROLE_AMMOBEARER}; break; // Assuming US has ammo bearers
		        case 9: includedLabels = {EEditableEntityLabel.ROLE_SCOUT}; break;       // Assuming US has scouts
		    }
		}
		else if (faction == IA_Faction.FIA)
		{

		    includedLabels = {EEditableEntityLabel.ROLE_RIFLEMAN};

		}
		if(m_HVTGroup == true){
		
			return "{5117311FB822FD1F}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Officer.et";
		
		}
		SCR_EntityCatalog entityCatalog;
		SCR_Faction scrFaction = SCR_Faction.Cast(DesiredFaction);
		entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
		if (!scrFaction)
            return "";
		if (!entityCatalog)
			return "";
		array<EEditableEntityLabel> excludedLabels = {};
		
		array<SCR_EntityCatalogEntry> characterEntries = {};
		entityCatalog.GetFullFilteredEntityList(characterEntries, includedLabels, excludedLabels);
		if (characterEntries.IsEmpty()) {
            return "";
        }
		
		
		
		if (characterEntries.IsEmpty())
			return "";
		
        return characterEntries[Math.RandomInt(0, characterEntries.Count())].GetPrefab();



    }

	bool HasActiveWaypoint()
	{
		if (!m_group)
		{
			return false;
		}
		
		array<AIWaypoint> wps = {};
		m_group.GetWaypoints(wps);
		
		bool hasWaypoints = !wps.IsEmpty();
		if (m_isDriving && !hasWaypoints)
		{
		}
		
		return hasWaypoints;
	}

	
    static IA_AiGroup CreateCivilianGroup(vector initialPos)
    {
        // Pass unit count 1 to the constructor
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV, 1);
        //grp.m_initialUnitCount = 1; // No longer needed here
        grp.m_isCivilian = true;
        return grp;
    }

    static IA_AiGroup CreateGroupForVehicle(Vehicle vehicle, IA_Faction faction, int unitCount, Faction AreaFaction)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // Spawn slightly above vehicle origin

        IA_AiGroup grp = new IA_AiGroup(spawnPos, IA_SquadType.Riflemen, faction, unitCount);
        grp.m_isCivilian = false;
        grp.m_referencedEntity = vehicle; // Store vehicle reference for later use

		Resource groupRes;
		switch(faction){
			case IA_Faction.USSR:
				groupRes = Resource.Load("{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et");
				break;
			case IA_Faction.US:
				groupRes = Resource.Load("{EACD97CF4A702FAE}Prefabs/Groups/BLUFOR/Group_US_Base.et");
				break;
			case IA_Faction.CIV:
				groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
				break;
			case IA_Faction.FIA:
				groupRes = Resource.Load("{242BC3C6BCE96EA5}Prefabs/Groups/INDFOR/Group_FIA_Base.et");
				break;
		}
        if (!groupRes) {
            return null;
        }
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(spawnPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            delete groupEnt;
            return null;
        }
        
        vector groundPos = spawnPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        grp.m_group.SetOrigin(groundPos);

        BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
        if (!compartmentManager) {
            return null;
        }
        
        array<BaseCompartmentSlot> compartments = {};
        compartmentManager.GetCompartments(compartments);
        
        array<BaseCompartmentSlot> usableCompartments = {};
        
        foreach (BaseCompartmentSlot slot : compartments) {
            if (slot.GetOccupant())
                continue;
                
            ECompartmentType type = slot.GetType();
            
            if (type == ECompartmentType.PILOT ||  
                type == ECompartmentType.CARGO || 
                type == ECompartmentType.TURRET) {
                
                if (slot.IsCompartmentAccessible())
                    usableCompartments.Insert(slot);
            }
        }
        

        int actualUnitsToSpawn = Math.Min(unitCount, usableCompartments.Count());

        // Set up staggered spawning state for vehicle groups
        grp.m_pendingUnitsToSpawn = actualUnitsToSpawn;
        grp.m_unitsSpawnedCount = 0;
        grp.m_staggeredSpawnPos = groundPos;
        grp.m_staggeredSpawnFaction = faction;
        grp.m_staggeredAreaFaction = AreaFaction;
        
        Print(string.Format("[IA_AiGroup.CreateGroupForVehicle] Starting staggered spawning of %1 units for vehicle group faction %2", actualUnitsToSpawn, faction), LogLevel.NORMAL);
        
        // Start spawning the first unit immediately
        grp.SpawnNextUnit();

        return grp;
    }

    // Legacy synchronous version - now just creates the group at initial position without road search
    // Use this when:
    // - You need the group immediately (e.g., for vehicle spawning, reinforcements)
    // - You already have a good spawn position
    // - Performance is not a concern (small number of groups)
    static IA_AiGroup CreateMilitaryGroupFromUnits(vector initialPos, IA_Faction faction, int unitCount, Faction AreaFaction, bool HVTGroup = false, bool useExactPosition = false)
    {
        if (unitCount <= 0)
            return null;

        // For backward compatibility, create the group immediately at the initial position
        // The async road search should be initiated separately by callers that want it
        return CreateMilitaryGroupAtPosition(initialPos, faction, unitCount, AreaFaction, HVTGroup, useExactPosition);
    }
    
    // Start an async road search and group creation
    // Use this when:
    // - Creating multiple groups at startup (prevents frame drops)
    // - Road position is important for AI navigation
    // - You can handle the group creation callback
    static void StartAsyncMilitaryGroupCreation(vector initialPos, IA_Faction faction, int unitCount, Faction AreaFaction, IA_AreaInstance callbackInstance = null, bool useExactPosition = false)
    {
        if (unitCount <= 0)
            return;
            
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        
        // Create search state
        IA_RoadSearchState searchState = new IA_RoadSearchState(initialPos, faction, unitCount, AreaFaction, activeGroup, useExactPosition);
        if (callbackInstance)
        {
            searchState.SetCallback(callbackInstance, "OnAsyncGroupCreated");
        }
        
        // Start the search
        GetGame().GetCallqueue().CallLater(PerformNextRoadSearch, IA_RoadSearchState.SEARCH_DELAY_MS, false, searchState);
    }
    
    // Perform one step of the road search
    static void PerformNextRoadSearch(IA_RoadSearchState searchState)
    {
        if (!searchState)
            return;

        if (searchState.m_useExactPosition)
        {
            searchState.m_foundSpawnPos = searchState.m_initialPos;
            searchState.m_roadFound = false; // Not technically a road, but we have our position
            CompleteAsyncGroupCreation(searchState);
            return;
        }
            
        // If not trying alternatives yet
        if (!searchState.m_tryingAlternatives)
        {
            // Check if we've exhausted all distance searches
            if (searchState.m_currentDistanceIndex >= searchState.m_searchDistances.Count())
            {
                // Move to alternative search phase
                searchState.m_tryingAlternatives = true;
                searchState.m_alternativeAttempt = 0;
                
                Print(string.Format("[IA_AiGroup.PerformNextRoadSearch] No road found within %1m of %2. Starting alternative search...", 
                    searchState.m_searchDistances[searchState.m_searchDistances.Count() - 1], 
                    searchState.m_initialPos.ToString()), LogLevel.WARNING);
                    
                // Continue with alternative search
                GetGame().GetCallqueue().CallLater(PerformNextRoadSearch, IA_RoadSearchState.SEARCH_DELAY_MS, false, searchState);
                return;
            }
            
            // Try current search distance
            int searchDistance = searchState.m_searchDistances[searchState.m_currentDistanceIndex];
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(searchState.m_initialPos, searchDistance, searchState.m_activeGroup);
            
            if (roadPos != vector.Zero)
            {
                // Found a road!
                searchState.m_foundSpawnPos = roadPos;
                searchState.m_roadFound = true;
                
                Print(string.Format("[IA_AiGroup.PerformNextRoadSearch] Found road at distance %1m from initial pos %2. Road pos: %3", 
                    searchDistance, searchState.m_initialPos.ToString(), roadPos.ToString()), LogLevel.NORMAL);
                    
                // Complete the group creation
                CompleteAsyncGroupCreation(searchState);
                return;
            }
            
            // No road found at this distance, try next
            searchState.m_currentDistanceIndex++;
            GetGame().GetCallqueue().CallLater(PerformNextRoadSearch, IA_RoadSearchState.SEARCH_DELAY_MS, false, searchState);
        }
        else
        {
            // Alternative search phase
            if (searchState.m_alternativeAttempt >= IA_RoadSearchState.ALTERNATIVE_ATTEMPTS)
            {
                // All searches exhausted, use initial position
                searchState.m_foundSpawnPos = searchState.m_initialPos;
                searchState.m_roadFound = false;
                
                Print(string.Format("[IA_AiGroup.PerformNextRoadSearch] WARNING: No road found for group spawn. Using initial position %1. AI navigation may be impaired!", 
                    searchState.m_initialPos.ToString()), LogLevel.WARNING);
                    
                CompleteAsyncGroupCreation(searchState);
                return;
            }
            
            // Try alternative search from random point
            float angle = Math.RandomFloat(0, Math.PI2);
            float distance = Math.RandomFloat(100, 500);
            vector searchPoint;
            searchPoint[0] = searchState.m_initialPos[0] + Math.Cos(angle) * distance;
            searchPoint[1] = searchState.m_initialPos[1];
            searchPoint[2] = searchState.m_initialPos[2] + Math.Sin(angle) * distance;
            
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(searchPoint, 200, searchState.m_activeGroup);
            
            if (roadPos != vector.Zero)
            {
                searchState.m_foundSpawnPos = roadPos;
                searchState.m_roadFound = true;
                
                Print(string.Format("[IA_AiGroup.PerformNextRoadSearch] Found road via alternative search (attempt %1) at %2", 
                    searchState.m_alternativeAttempt + 1, roadPos.ToString()), LogLevel.NORMAL);
                    
                CompleteAsyncGroupCreation(searchState);
                return;
            }
            
            // Try next alternative
            searchState.m_alternativeAttempt++;
            GetGame().GetCallqueue().CallLater(PerformNextRoadSearch, IA_RoadSearchState.SEARCH_DELAY_MS, false, searchState);
        }
    }
    
    // Complete the async group creation
    static void CompleteAsyncGroupCreation(IA_RoadSearchState searchState)
    {
        if (!searchState)
            return;
            
        // Create the group at the found position
        IA_AiGroup grp = CreateMilitaryGroupAtPosition(searchState.m_foundSpawnPos, searchState.m_faction, 
            searchState.m_unitCount, searchState.m_areaFaction, false, searchState.m_useExactPosition);
            
        // Call the callback if set
        if (searchState.m_callbackInstance && searchState.m_callbackMethod != "")
        {
            // Note: This is a simplified callback mechanism. In real implementation,
            // you might need to use a more robust callback system
            searchState.m_callbackInstance.OnAsyncGroupCreated(grp, searchState.m_roadFound);
        }
    }
	
	
	
	
    
    // Create a military group at a specific position (no road search)
    static IA_AiGroup CreateMilitaryGroupAtPosition(vector spawnPos, IA_Faction faction, int unitCount, Faction AreaFaction, bool HVTGroup = false, bool useExactPosition = false)
    {
        if (unitCount <= 0)
            return null;

        // --- BEGIN MODIFIED: Road Spawning for Synchronous Groups (Bypass for HVT) ---
        vector finalSpawnPos = spawnPos;
        if (!HVTGroup && !useExactPosition)
        {
            int activeGroup = IA_VehicleManager.GetActiveGroup();
            // Search for a road within 150m of the requested spawn position
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(spawnPos, 150, activeGroup);
            
            if (roadPos != vector.Zero)
            {
                finalSpawnPos = roadPos;
                Print(string.Format("[IA_AiGroup.CreateMilitaryGroupAtPosition] Road spawn found. Original: %1, Road: %2", spawnPos, finalSpawnPos), LogLevel.DEBUG);
            }
        }
        else
        {
            Print(string.Format("[IA_AiGroup.CreateMilitaryGroupAtPosition] HVT or exact position spawn detected, bypassing road snapping. Spawning at exact location: %1", finalSpawnPos.ToString()), LogLevel.DEBUG);
        }
        // --- END MODIFIED ---

        IA_AiGroup grp = new IA_AiGroup(finalSpawnPos, IA_SquadType.Riflemen, faction, unitCount, HVTGroup);
        grp.m_isCivilian = false;

        Resource groupRes;
        switch(faction){
            case IA_Faction.USSR:
                groupRes = Resource.Load("{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et");
                break;
            case IA_Faction.US:
                groupRes = Resource.Load("{EACD97CF4A702FAE}Prefabs/Groups/BLUFOR/Group_US_Base.et");
                break;
            case IA_Faction.CIV:
                groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
                break;
            case IA_Faction.FIA:
                groupRes = Resource.Load("{242BC3C6BCE96EA5}Prefabs/Groups/INDFOR/Group_FIA_Base.et");
                break;
        }

        if (!groupRes) {
            Print(string.Format("[IA_AiGroup.CreateMilitaryGroupAtPosition] Failed to load group resource for faction %1", faction), LogLevel.ERROR);
            return null;
        }

        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(finalSpawnPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            delete groupEnt;
            Print(string.Format("[IA_AiGroup.CreateMilitaryGroupAtPosition] Failed to create SCR_AIGroup for faction %1 at %2", faction, finalSpawnPos.ToString()), LogLevel.ERROR);
            return null;
        }

        vector groundPos = finalSpawnPos;
		if (!HVTGroup)
        {
	        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
	        groundPos[1] = groundY;
		}
        grp.m_group.SetOrigin(groundPos);

        // Set up staggered spawning state
        grp.m_pendingUnitsToSpawn = unitCount;
        grp.m_unitsSpawnedCount = 0;
        grp.m_staggeredSpawnPos = groundPos;
        grp.m_staggeredSpawnFaction = faction;
        grp.m_staggeredAreaFaction = AreaFaction;
        
        Print(string.Format("[IA_AiGroup.CreateMilitaryGroupAtPosition] Starting staggered spawning of %1 units for faction %2 at %3", unitCount, faction, finalSpawnPos.ToString()), LogLevel.NORMAL);
        
        // Start spawning the first unit immediately
        grp.SpawnNextUnit();

        return grp;
    }

    IA_Faction GetEngagedEnemyFaction()
    {
		if(m_isCivilian)
			return IA_Faction.NONE;

		if(IsEngagedWithEnemy()){
			return m_engagedEnemyFaction;
		}
        return IA_Faction.NONE; // Or whatever your original return value is
    }

    bool IsEngagedWithEnemy()
    {
		if(m_isCivilian)
			return false;
        bool result = m_engagedEnemyFaction != IA_Faction.NONE;
        
        if (!result && !m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            int currentTime = System.GetUnixTime();
            int timeSinceLastDanger = currentTime - m_lastDangerEventTime;
            
            if (m_lastDangerEventTime > 0 && timeSinceLastDanger < 60)
            {
                // Consider the unit engaged if there was recent danger
                result = true;
                    // For military units, set an appropriate enemy faction based on this unit's faction
                    
                m_engagedEnemyFaction = IA_Faction.US;
                
            }
        }
        
        //////Print("[IA_NEW_DEBUG] IsEngagedWithEnemy called. Group faction: " + m_faction + ", Result: " + result, LogLevel.WARNING);
        return result;
    }

    void MarkAsUnengaged()
    {
        ////////Print("[DEBUG] MarkAsUnengaged called. Resetting engaged enemy faction.", LogLevel.NORMAL);
        m_engagedEnemyFaction = IA_Faction.NONE;
    }

    array<SCR_ChimeraCharacter> GetGroupCharacters()
    {
        array<SCR_ChimeraCharacter> characters = {};
        
        if (!m_group)
            return characters;
            
        // Get all agents in the group
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        // Convert agents to characters
        foreach (AIAgent agent : agents)
        {
            if (!agent)
                continue;
                
            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (character)
                characters.Insert(character);
        }
        
        return characters;
    }

    // Add a pre-existing waypoint to the group
    void AddWaypoint(SCR_AIWaypoint waypoint)
    {
        if (!m_group || !waypoint)
        {
            ////Print("[IA_AiGroup.AddWaypoint] Group or waypoint is null.", LogLevel.WARNING);
            return;
        }
        
        // --- BEGIN ENHANCED: More detailed waypoint type validation ---
        // Get the actual type of the waypoint
        string waypointTypeName = waypoint.Type().ToString();
        
        // Check if this is a defend waypoint
        SCR_DefendWaypoint defendTest = SCR_DefendWaypoint.Cast(waypoint);
        bool isDefendWaypoint = (defendTest != null);
        
        // Log waypoint addition with type info
        Print(string.Format("[IA_AiGroup.AddWaypoint] Adding waypoint to Group %1 | Faction: %2 | Waypoint Type: %3 | Is SCR_DefendWaypoint: %4 | Group State: %5", 
            this, typename.EnumToString(IA_Faction, m_faction), waypointTypeName, isDefendWaypoint, 
            typename.EnumToString(IA_GroupTacticalState, m_tacticalState)), LogLevel.DEBUG);
        
        // Validate waypoint type based on current tactical state
        if (m_tacticalState == IA_GroupTacticalState.Defending)
        {
            // For defending state, we should ideally have a SCR_DefendWaypoint
            if (!isDefendWaypoint)
            {
                Print(string.Format("[IA_AiGroup.AddWaypoint] ERROR: Group %1 (Faction: %2) is in Defending state but received non-DefendWaypoint type: %3. This may cause issues!", 
                    this, typename.EnumToString(IA_Faction, m_faction), waypointTypeName), LogLevel.ERROR);
                
                // Log a stack trace to help identify where this is coming from
                Print("[IA_AiGroup.AddWaypoint] Stack trace for non-defend waypoint:", LogLevel.ERROR);
                // Still add it, but log the warning
            }
        }
        // --- END ENHANCED ---
        
        m_group.AddWaypointToGroup(waypoint);
        ////Print("[DEBUG] IA_AiGroup.AddWaypoint: Waypoint added to internal SCR_AIGroup.", LogLevel.NORMAL);
    }

    void AddOrder(vector origin, IA_AiOrder order, bool topPriority = false)
    {
        // Store last order data
        m_lastOrderPosition = origin;
        m_lastOrderTime = System.GetUnixTime();
        
        // If this is a vehicle group, update vehicle orders first
        if (m_isDriving)
        {
            UpdateVehicleOrders();
            return; // Let the vehicle order system handle it
        }
        
        if (!m_isSpawned)
        {
            return;
        }
        if (!m_group)
        {
            return;
        }
		vector zoneOrigin;
		float zoneRadius;
        // Restrict infantry waypoints to a single zone, while vehicle waypoints can navigate across the zone group
        if (!m_isDriving && order != IA_AiOrder.GetInVehicle)
        {
            // Get the current group number from VehicleManager's active group
            int currentGroupNumber = IA_VehicleManager.GetActiveGroup();
            
            // Find the closest zone to the current group position
            IA_AreaMarker closestMarker = IA_AreaMarker.GetMarkerAtPosition(m_group.GetOrigin());
            
            if (closestMarker)
            {
                zoneRadius = closestMarker.GetRadius();
                zoneOrigin = closestMarker.GetZoneCenter();
                
                float distToZoneCenter = vector.Distance(origin, zoneOrigin);
                
                if (distToZoneCenter > zoneRadius * 0.8) // Use 80% of radius to keep units within the zone's boundaries
                {
                    
                    origin = IA_Game.rng.GenerateRandomPointInRadius(1, zoneRadius * 0.8, zoneOrigin);
                    
                }
            }

        }
        // Adjust spawn height based on terrain
        float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		

		
		
		
		
        origin[1] = y + 0.5;
		if(zoneOrigin && zoneRadius)
		{
			float yDiff = origin[1] - zoneOrigin[1];
			if (yDiff < 0) yDiff = -yDiff;
			if(yDiff > zoneRadius*0.25) // Add logic to check if the origin's Y value is farther away from the Area's Origin Point than the Area's Radius.
				origin[1] = zoneOrigin[1];
		}
		
        // --- BEGIN WATER CHECK ---
        if (WaterCheck(origin))
        {
            Print(string.Format("[IA_AiGroup.AddOrder] Proposed waypoint at %1 for order %2 is in water. Requesting state re-evaluation to find a new target.", 
                origin.ToString(), typename.EnumToString(IA_AiOrder, order)), LogLevel.WARNING);
            
            // Request a neutral state at current group position to trigger re-evaluation by authority or self.
            // This is how the group "requests a new waypoint" - by asking for a new task.
            RequestTacticalStateChange(IA_GroupTacticalState.Neutral, GetOrigin());
            return; // Do not add this order.
        }
        // --- END WATER CHECK ---
		
        // For vehicle orders, snap to nearest road - EXCEPT GetInVehicle orders which should go directly to the vehicle
        if (m_isDriving && order != IA_AiOrder.GetInVehicle)
        {

            int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
            vector roadOrigin = IA_VehicleManager.FindRandomRoadEntityInZone(origin, 300, currentActiveGroup);
            if (roadOrigin != vector.Zero)
            {
                origin = roadOrigin;
            }
            
        }

        ResourceName rname;
        // Special handling for Defend orders to prevent errors
        //Print("IA_AiOrder is = " + typename.EnumToString(IA_AiOrder, order));
        if (order == IA_AiOrder.Defend) 
        {
            // --- BEGIN ADDED: Check for side objective groups ---
            if (IsObjectiveUnit() && m_OwningSideObjective)
            {
                // Check if this is the HVT group
                IA_AssassinationObjective assassinObj = IA_AssassinationObjective.Cast(m_OwningSideObjective);
                if (assassinObj && assassinObj.IsHVTGroup(this))
                {
                    rname = "{FAD1D789EE291964}Prefabs/AI/Waypoints/AIWaypoint_Defend_Large.et";
                    //Print(string.Format("[IA_AiGroup.AddOrder] Using HVTDefend waypoint for HVT group %1", this), LogLevel.DEBUG);
                }
                else
                {
                    // Use guard defend waypoint for other objective units
                    rname = "{FAD1D789EE291964}Prefabs/AI/Waypoints/AIWaypoint_Defend_Large.et";
                    //Print(string.Format("[IA_AiGroup.AddOrder] Using GuardDefend waypoint for guard group %1", this), LogLevel.DEBUG);
                }
            }
            else
            {
                // Standard defend waypoint for non-objective units
                rname = "{FAD1D789EE291964}Prefabs/AI/Waypoints/AIWaypoint_Defend_Large.et";
            }
            // --- END ADDED ---
            
            // --- BEGIN ADDED: Debug logging for defend orders with faction info ---
            Print(string.Format("[IA_AiGroup.AddOrder] DEFEND ORDER CREATION: Group %1 | Faction: %2 | IsCivilian: %3 | Position: %4", 
                this, typename.EnumToString(IA_Faction, m_faction), m_isCivilian, origin.ToString()), LogLevel.DEBUG);
            // --- END ADDED ---
        }
        else if (order == IA_AiOrder.DefendSmall) // Added condition for SearchAndDestroy
        {
            // --- BEGIN ADDED: Check for side objective HVT using DefendSmall ---
            if (IsObjectiveUnit() && m_OwningSideObjective)
            {
                IA_AssassinationObjective assassinObj = IA_AssassinationObjective.Cast(m_OwningSideObjective);
                if (assassinObj && assassinObj.IsHVTGroup(this))
                {
                    // Use HVT defend waypoint even for DefendSmall orders
                    rname = "{2FCBE5C76E285A7B}Prefabs/AI/Waypoints/AIWaypoint_DefendSmall.et";
                }
                else
                {
                    // Standard defend small for guards
                    rname = "{2FCBE5C76E285A7B}Prefabs/AI/Waypoints/AIWaypoint_DefendSmall.et";
                }
            }
            else
            {
                // Standard defend small waypoint
                rname = "{2FCBE5C76E285A7B}Prefabs/AI/Waypoints/AIWaypoint_DefendSmall.et";
            }
            // --- END ADDED ---
            
            // --- BEGIN ADDED: Debug logging for defend small orders ---
            Print(string.Format("[IA_AiGroup.AddOrder] DEFENDSMALL ORDER CREATION: Group %1 | Faction: %2 | Position: %3", 
                this, typename.EnumToString(IA_Faction, m_faction), origin.ToString()), LogLevel.WARNING);
            // --- END ADDED ---
        }
        else if (order == IA_AiOrder.SearchAndDestroy) // Added condition for SearchAndDestroy
        {
            rname = "{EE9A99488B40628B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_SearchAndDestroy.et";
        }
        else
        {
            rname = IA_AiOrderResource(order);
        }
        
        // --- BEGIN ADDED LOGGING ---
        //Print(string.Format("[IA_Waypoint DEBUG AddOrder] Group %1 | Order: %2 | Waypoint Resource: %3 | Target: %4", 
        //    m_group, typename.EnumToString(IA_AiOrder, order), rname, origin), LogLevel.NORMAL);
        // --- END ADDED LOGGING ---
        
        Resource res = Resource.Load(rname);
        if (!res)
        {
            // --- BEGIN ADDED LOGGING ---
            //Print(string.Format("[IA_Waypoint] Failed to load waypoint resource: %1 for Group %2", rname, m_group), LogLevel.ERROR);
            // --- END ADDED LOGGING ---
            return;
        }

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        SCR_AIWaypoint w = null; // Initialize w to null

        if (order == IA_AiOrder.Defend || order == IA_AiOrder.DefendSmall)
        {
            SCR_DefendWaypoint defendW = SCR_DefendWaypoint.Cast(waypointEnt);
            if (defendW)
            {
                // --- BEGIN ADDED: Log successful defend waypoint creation ---
                Print(string.Format("[IA_AiGroup.AddOrder] SUCCESS: Created SCR_DefendWaypoint for Group %1 | Faction: %2 | Type: %3", 
                    this, typename.EnumToString(IA_Faction, m_faction), waypointEnt.Type()), LogLevel.DEBUG);
                // --- END ADDED ---
                
                // If specific SCR_DefendWaypoint methods were needed, they could be called on 'defendW' here.
                w = defendW; // Assign to the SCR_AIWaypoint variable for common operations
            }
            else
            {
                // Handle failed cast to SCR_DefendWaypoint
                string entStr = "null"; if (waypointEnt) entStr = waypointEnt.ToString();
                Print(string.Format("[IA_Waypoint] FAILED CAST: Entity type is %1. Resource: %2, Origin: %3, Entity: %4 for Group %5 | Faction: %6", 
                    waypointEnt.Type(), rname, origin, entStr, m_group, typename.EnumToString(IA_Faction, m_faction)), LogLevel.ERROR);
            }
        }
        else if (order == IA_AiOrder.SearchAndDestroy) // Added condition for SearchAndDestroy
        {
            SCR_SearchAndDestroyWaypoint sadW = SCR_SearchAndDestroyWaypoint.Cast(waypointEnt);
            if (sadW)
            {
                // If specific SCR_SearchAndDestroyWaypoint methods were needed, they could be called on 'sadW' here.
                w = sadW; // Assign to the SCR_AIWaypoint variable for common operations
            }
            else
            {
                // Handle failed cast to SCR_SearchAndDestroyWaypoint
                string entStr = "null"; if (waypointEnt) entStr = waypointEnt.ToString();
                Print(string.Format("[IA_Waypoint] Failed to cast spawned entity to SCR_SearchAndDestroyWaypoint. Resource: %1, Origin: %2, Entity: %3 for Group %4", rname, origin, entStr, m_group), LogLevel.ERROR);
            }
        }
        else
        {
            w = SCR_AIWaypoint.Cast(waypointEnt);
            if (!w)
            {
                 // Handle failed cast to SCR_AIWaypoint for non-defend orders
                string entStr = "null"; if (waypointEnt) entStr = waypointEnt.ToString();
                Print(string.Format("[IA_Waypoint] Failed to cast spawned entity to SCR_AIWaypoint. Resource: %1, Origin: %2, Entity: %3 for Group %4", rname, origin, entStr, m_group), LogLevel.ERROR);
            }
        }

        if (!w) // This single check now covers failure from either cast, or if waypointEnt itself was null (though SpawnEntityPrefab usually handles that)
        {
            if (waypointEnt) IA_Game.AddEntityToGc(waypointEnt); // Ensure waypointEnt is GC'd if it was created but cast failed
            return;
        }

        if (m_isDriving)
        {
            w.SetPriorityLevel(VEHICLE_PRIORITY_MOVE_LEVEL); // Keep vehicles as is
        }
        else
        {
            // --- BEGIN MODIFIED: Special priority handling for objective units ---
            if (IsObjectiveUnit() && m_OwningSideObjective && (order == IA_AiOrder.Defend || order == IA_AiOrder.DefendSmall))
            {
                IA_AssassinationObjective assassinObj = IA_AssassinationObjective.Cast(m_OwningSideObjective);
                if (assassinObj && assassinObj.IsHVTGroup(this))
                {
                    // HVT gets highest priority defend
                    w.SetPriorityLevel(300);
                    
                    // Set tight completion radius for HVT
                    if (w.Type().IsInherited(SCR_DefendWaypoint))
                    {
                        SCR_DefendWaypoint defendWP = SCR_DefendWaypoint.Cast(w);
                        if (defendWP)
                        {
                            defendWP.SetCompletionRadius(4);
                        }
                    }
                    
                    Print(string.Format("[IA_AiGroup.AddOrder] Set HVT waypoint priority to 1250 with 4m completion radius", this), LogLevel.DEBUG);
                }
                else
                {
                    // Guards get high but not maximum priority
                    w.SetPriorityLevel(140);
                    
                    // Set wider completion radius for guards
                    if (w.Type().IsInherited(SCR_DefendWaypoint))
                    {
                        SCR_DefendWaypoint defendWP = SCR_DefendWaypoint.Cast(w);
                        if (defendWP)
                        {
                            defendWP.SetCompletionRadius(Math.RandomInt(15, 40));
                        }
                    }
                    
                    Print(string.Format("[IA_AiGroup.AddOrder] Set Guard waypoint priority to 300 with 15-40m completion radius", this), LogLevel.DEBUG);
                }
            }
            else
            {
                // Dynamic priority for regular infantry based on situation
                int priorityLevel = CalculateWaypointPriority(order);
                w.SetPriorityLevel(priorityLevel);
            }
            // --- END MODIFIED ---
        }

        // --- BEGIN ADDED: Additional logging before adding waypoint to group ---
        if (order == IA_AiOrder.Defend)
        {
          //  //Print(string.Format("[IA_Waypoint] PRE-ADD DEFEND WAYPOINT: Group %1 | Waypoint %2 | Position %3 | Priority %4", 
         //       m_group, w, w.GetOrigin().ToString(), w.GetPriorityLevel()), LogLevel.NORMAL);
            
            // Add additional group state info
            array<AIWaypoint> existingWaypoints = {};
            m_group.GetWaypoints(existingWaypoints);
          //  //Print(string.Format("[IA_Waypoint] GROUP STATE BEFORE DEFEND: Existing Waypoints: %1", 
          //      existingWaypoints.Count()), LogLevel.NORMAL);
        }
        // --- END ADDED ---

        m_group.AddWaypointToGroup(w);
        
        // --- BEGIN ADDED LOGGING ---
        //Print(string.Format("[IA_Waypoint] Added Order: %1 at %2 for Group %3", typename.EnumToString(IA_AiOrder, order), origin, m_group), LogLevel.WARNING);
        // --- END ADDED LOGGING ---
    }

    void SetInitialPosition(vector pos)
    {
        m_initialPosition = pos;
    }

    int TimeSinceLastOrder()
    {
        int delta = System.GetUnixTime() - m_lastOrderTime;
        return delta;
    }

    bool HasOrders()
    {
        if (!m_group)
        {
            return false;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        return !wps.IsEmpty();
    }

    bool IsDriving()
    {
        return m_isDriving;
    }

    void RemoveAllOrders(bool resetLastOrderTime = false)
    {
        if (!m_group)
            return;
               
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
                
        foreach (AIWaypoint w : wps)
        {
            m_group.RemoveWaypoint(w);
        }
        
        // Reset flanking phase state when all orders are removed
        if (m_isInFlankingPhase)
        {
         //   //Print(string.Format("[RemoveAllOrders] Resetting flanking state for Group %1", m_group), LogLevel.NORMAL);
            m_isInFlankingPhase = false;
        }
        
        if (resetLastOrderTime)
        {
            m_lastOrderTime = 0;
            
            if (m_isDriving && m_referencedEntity)
            {
                Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
                if (vehicle)
                {

                    ClearVehicleReference();
                }
                else
                {

                    m_isDriving = false;
                    m_referencedEntity = null;
                }
            }
        }
    }

    int GetAliveCount()
    {
        if (!m_isSpawned)
        {
            if (m_isCivilian)
            {
                return 1;
            }
            else
            {
                int count = IA_SquadCount(m_squadType, m_faction);
                return count;
            }
        }
        if (!m_group)
        {	
            return 0;
        }
        int aliveCount = m_group.GetPlayerAndAgentCount();
        return aliveCount;
    }

    vector GetOrigin()
    {
        if (!m_group)
        {
            return vector.Zero;
        }
        vector origin = m_group.GetOrigin();
        m_lastConfirmedPosition = origin;
        return origin;
    }

    bool IsSpawned()
    {
        return m_isSpawned;
    }

    void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
    {
        if (IsSpawned())
        {
            return;
        }
        
        if (!PerformSpawn())
        {
            // Failed to spawn the group
            return;
        }
        
        // Ensure the assigned area is found and set for civilians before setting their initial tactical state.
        if (m_isCivilian)
        {
            // TryFindAndSetAssignedArea will only act if m_lastAssignedArea is currently null,
            // and it uses m_initialPosition which is set when the civilian group is created.
            TryFindAndSetAssignedArea(); 
        }
        
        // Initialize the default tactical state based on the initial order
        IA_GroupTacticalState initialState = IA_GroupTacticalState.Neutral;
        
        // Map initial order to appropriate tactical state
        if (initialOrder == IA_AiOrder.Defend) 
        {
            initialState = IA_GroupTacticalState.Defending;
        } 
        else if (initialOrder == IA_AiOrder.Patrol) 
        {
            initialState = IA_GroupTacticalState.DefendPatrol;
        }
        else if (initialOrder == IA_AiOrder.SearchAndDestroy) 
        {
            initialState = IA_GroupTacticalState.Attacking;
        }
        else if (initialOrder == IA_AiOrder.Move || initialOrder == IA_AiOrder.PriorityMove)
        {
            // For both move types, use a neutral state but the direct order will still be applied
            initialState = IA_GroupTacticalState.Neutral;
        }
        
        // Set the initial tactical state with explicit position
        vector posToUse;
        if (orderPos != vector.Zero)
        {
            posToUse = orderPos;
        }
        else
        {
            posToUse = m_initialPosition;
        }
        
        // Ensure a tactical state is set
        SetTacticalState(initialState, posToUse);
        
        // If using a move order, apply it directly instead of relying on the tactical state
        if (initialOrder == IA_AiOrder.Move || initialOrder == IA_AiOrder.PriorityMove)
        {
            AddOrder(posToUse, initialOrder, true);
        }
        else
        {
            ApplyTacticalStateOrders();
        }
    }


	
	
	
    private bool PerformSpawn()
    {
        if (IsSpawned())
        {
            return true; // Already spawned, e.g., by CreateGroupForVehicle or CreateMilitaryGroupFromUnits
        }
        // m_isSpawned will be set to true at the end of successful spawning

        if (m_isCivilian)
        {
            // For civilians, try to find a road position
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(m_initialPosition, 300, IA_VehicleManager.GetActiveGroup());
            vector spawnPos;
            
            // Use road position if found, otherwise use the initial position
            if (roadPos != vector.Zero) {
                spawnPos = roadPos;
            } else {
                spawnPos = m_initialPosition;
            }
            
            // Create the SCR_AIGroup entity for the civilian
            Resource groupPrefabRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
            if (!groupPrefabRes)
            {
                 return false;
            }
            IEntity groupEntity = GetGame().SpawnEntityPrefab(groupPrefabRes, null, IA_CreateSimpleSpawnParams(spawnPos));
            m_group = SCR_AIGroup.Cast(groupEntity);

            if (!m_group)
            {
                if (groupEntity) IA_Game.AddEntityToGc(groupEntity); // Clean up group entity if it was spawned
                return false;
            }
            
            // Restore old spawning logic - spawn civilian directly
            string resourceName = IA_RandomCivilianResourceName();
            Resource charRes = Resource.Load(resourceName);
            if (!charRes)
            {
                return false;
            }
            
            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSurfaceAdjustedSpawnParams(spawnPos));
            if (!charEntity)
            {
                return false;
            }

            // Add the spawned civilian character to the SCR_AIGroup
            if (!m_group.AddAIEntityToGroup(charEntity))
            {
                IA_Game.AddEntityToGc(charEntity); // Clean up character
                IA_Game.AddEntityToGc(m_group);    // Clean up the group as well since it's unusable
                m_group = null;
                return false;
            }
            // If successfully added, setup death listener for this specific unit
            SetupDeathListenerForUnit(charEntity);
            
            Print(string.Format("[IA_AiGroup.PerformSpawn] Using direct spawning for civilian at %1", spawnPos.ToString()), LogLevel.NORMAL);
        }
        else // Military group
        {
            // This path assumes m_group was already created and populated by methods like CreateMilitaryGroupFromUnits
            // or CreateGroupForVehicle, and PerformSpawn is called for common setup.
            // If m_group is null for a military group here, it's an error in initialization flow.
            if (!m_group) 
            {
                Print(string.Format("[IA_AiGroup.PerformSpawn] Military group %1 (Faction: %2) m_group is null. This indicates an issue in group initialization prior to calling PerformSpawn.", 
                    this, m_faction), LogLevel.ERROR);
                return false; 
            }
        }
        
        // Common setup for all successfully spawned/validated groups
        if (!m_group) 
        {
            ////Print("[IA_AiGroup.PerformSpawn] m_group is null after specific spawn logic. Critical error.", LogLevel.ERROR);
            return false;
        }

        m_isSpawned = true; // Set spawned to true only after successful creation/validation of entities and group

        vector groundPos;
        if (m_initialPosition != vector.Zero) { 
            groundPos = m_initialPosition;
        } else { 
            groundPos = m_group.GetOrigin(); // Fallback to m_group's current origin
        }

        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        m_group.SetOrigin(groundPos);
        m_lastConfirmedPosition = groundPos;
            
        // General SetupDeathListener call - for military this also schedules CheckDangerEvents.
        // For civilians, their specific unit listener is already set.
        SetupDeathListener(); 
        ScheduleNextStateEvaluation();

        // The recurring CheckDangerEvents is mainly for military AI reactions.
        // Civilians don't process danger events in the same way.
        // This was previously in SetupDeathListener, moved here for clarity.
        // Note: The one-time call to CheckDangerEvents in old SetupDeathListener is now covered by individual agent processing.
        // The recurring one is below.
		
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            GetGame().GetCallqueue().CallLater(CheckDangerEvents, Math.RandomInt(250, 750), true); // Recurring for military
        }

        return true;
    }

    // Process a danger event at the group level
    void ProcessDangerEvent(IA_GroupDangerType dangerType, vector position, IEntity sourceEntity = null, float intensity = 0.5, bool isSuppressed = false)
    {
        // Skip danger processing if in defend mode or escaping
        if (m_isInDefendMode || m_tacticalState == IA_GroupTacticalState.Escaping)
            return;
            
        // Rate limit processing per group
        int currentTime_proc = GetGame().GetWorld().GetWorldTime(); // Use world time for milliseconds
        if (currentTime_proc - m_lastDangerProcessTime < DANGER_PROCESS_INTERVAL_MS)
        {
            return; // Skip processing if too soon
        }
        m_lastDangerProcessTime = currentTime_proc;

        // Skip for civilians
        if (m_isCivilian || m_faction == IA_Faction.CIV)
            return;
            
        if (!IsSpawned() || !m_group)
            return;
    
        // Try to get the faction of the source entity
        IA_Faction sourceFaction = IA_Faction.NONE;
        if (sourceEntity)
        {
            // Try to get faction from a character entity
            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(sourceEntity);
            if (character)
            {
                FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
                if (factionComponent)
                {
                    Faction faction = factionComponent.GetAffiliatedFaction();
                    if (!faction)
                        return; // Ignore if faction cannot be determined
                    string factionKey = faction.GetFactionKey();
                    // Map the faction key to our IA_Faction enum
                    if (factionKey == "US")
                        sourceFaction = IA_Faction.US;
                    else if (factionKey == "USSR")
                        sourceFaction = IA_Faction.USSR;
                    else if (factionKey == "FIA")
                        sourceFaction = IA_Faction.FIA;
                    else if (factionKey == "CIV")
                        sourceFaction = IA_Faction.CIV; // Include CIV to potentially ignore civilian sources later
                        
                }
            }
            
            // --- MODIFIED CHECK: Skip if source faction is unknown OR same as own faction OR civilian ---
            if (sourceFaction == IA_Faction.NONE || sourceFaction == m_faction || sourceFaction == IA_Faction.CIV)
                return; // Ignore unknown, friendly, or civilian sources
            // --- END MODIFIED CHECK ---
                
            // If we identified a valid enemy faction, set it (we know it's not NONE, CIV and not m_faction here)
            m_engagedEnemyFaction = sourceFaction;
        }
        
        // Update last danger info - Shared for both infantry and vehicles
        m_lastDangerEventTime = System.GetUnixTime();
        
        // --- BEGIN ADDED: Vehicle specific handling ---
        // If driving, we only care about the timestamp for the simple reaction.
        // No need to store detailed events or update danger level for vehicles for now.
        if (m_isDriving)
        {
           // //Print(string.Format("[IA_AiGroup.ProcessDangerEvent] Vehicle Group %1 recorded danger event from %2 at %3. LastDangerTime: %4", 
           //     this, sourceFaction, position.ToString(), m_lastDangerEventTime), LogLevel.NORMAL);
            return; // Exit early for vehicles after updating timestamp
        }
        // --- END ADDED ---

        // --- Infantry specific handling (original logic) ---
        m_lastDangerPosition = position;
        m_consecutiveDangerEvents++;
        
        // Create a danger event for later processing (infantry only)
        IA_GroupDangerEvent dangerEvent = new IA_GroupDangerEvent();
        dangerEvent.m_type = dangerType;
        dangerEvent.m_position = position;
        dangerEvent.m_source = sourceEntity;
        dangerEvent.m_intensity = intensity;
        dangerEvent.m_isSuppressed = isSuppressed;
        dangerEvent.m_time = m_lastDangerEventTime;
        
        // Add to queue (infantry only)
        m_dangerEvents.Insert(dangerEvent);
        
        // Update danger level (infantry only)
        EvaluateDangerState(); 
    }
    
    void ResetDangerState()
    {
        m_dangerEvents.Clear();
        m_currentDangerLevel = 0.0;
        m_consecutiveDangerEvents = 0;
        m_lastDangerEventTime = 0;
        m_lastDangerPosition = vector.Zero;
    }
    
    void OnProjectileImpact(vector impactPosition, IEntity shooterEntity = null)
    {
        ProcessDangerEvent(IA_GroupDangerType.ProjectileImpact, impactPosition, shooterEntity, 0.3);
    }
    
    void OnWeaponFired(vector shotPosition, IEntity shooterEntity = null, bool isSuppressed = false)
    {
        ProcessDangerEvent(IA_GroupDangerType.WeaponFire, shotPosition, shooterEntity, 0.4, isSuppressed);
    }
    
    void OnProjectileFlyby(vector flybyPosition, IEntity shooterEntity = null)
    {
        ProcessDangerEvent(IA_GroupDangerType.ProjectileFlyby, flybyPosition, shooterEntity, 0.3);
    }
    
    void OnExplosion(vector explosionPosition, IEntity sourceEntity = null)
    {
        ProcessDangerEvent(IA_GroupDangerType.Explosion, explosionPosition, sourceEntity, 0.7);
    }
    
    void OnEnemySpotted(vector enemyPosition, IEntity enemyEntity, float threatLevel = 0.5)
    {
        ProcessDangerEvent(IA_GroupDangerType.EnemySpotted, enemyPosition, enemyEntity, threatLevel);
    }
    
    private void EvaluateDangerState()
    {
        // Early exit if there are no danger events to process
        if (m_dangerEvents.IsEmpty())
        {
            m_currentDangerLevel = 0.0;
            m_consecutiveDangerEvents = 0;
            return;
        }
        
        // Current time to calculate recency
        int currentTime = System.GetUnixTime();
        
        // Calculate the combined danger level from all events
        float dangerLevel = 0.0;
        vector dangerCenterSum = vector.Zero;
        float totalWeight = 0.0;
        
        // First collect only weapon fire events for position calculation
        // Collect relevant positions for median calculation
        array<vector> relevantPositions = {};
        
        // Process all danger events, but weigh recent events higher
        foreach (IA_GroupDangerEvent dangerEvent : m_dangerEvents)
        {
            // Calculate how recent this event is (0-30 seconds)
            int timeSince = currentTime - dangerEvent.m_time;
            
            // Events older than 30 seconds have minimal impact
            if (timeSince > 30)
                continue;
                
            // Calculate recency factor (1.0 for now, 0.0 for 30 seconds ago)
            float recencyFactor = 1.0 - (timeSince / 30.0);
            
            // Calculate weight based on intensity and recency
            float weight = dangerEvent.m_intensity * recencyFactor;
            
            // Apply suppression modifier if applicable
            if (dangerEvent.m_isSuppressed)
                weight *= 0.5;
                
            // Add to total danger level regardless of event type
            dangerLevel += weight;
            
            // Special handling for WeaponFire events - track them separately
            if (dangerEvent.m_type == IA_GroupDangerType.WeaponFire ||
                dangerEvent.m_type == IA_GroupDangerType.Explosion ||
                dangerEvent.m_type == IA_GroupDangerType.EnemySpotted)
            {
                // Only consider recent events for position
                if (recencyFactor > 0) // Equivalent to timeSince <= 30
                {
                    relevantPositions.Insert(dangerEvent.m_position);
                }
            }
            
            // Update consecutive events for compatibility with old system
            m_consecutiveDangerEvents++;
        }
        
        // Normalize the danger level (cap at 1.0)
        dangerLevel = Math.Clamp(dangerLevel, 0.0, 1.0);
        
        // Prioritize weapon fire events for danger center position
        // Calculate median position if there are relevant events
        int posCount = relevantPositions.Count();
        if (posCount > 0)
        {
            // Sort positions by x-coordinate to find the median
            // Note: This is a simplified median based on one axis.
            IA_VectorUtils.SortVectorsByX(relevantPositions);
            
            // Find the middle index
            int medianIndex = posCount / 2;
            
            // Set the last danger position to the median position
            m_lastDangerPosition = relevantPositions[medianIndex];
        }
        else
        {
            // No relevant recent danger events, reset position
            // Keep the existing logic for danger level, but reset position if no source points found.
            // Or should we keep the last known position? Resetting seems safer if no current threat loc.
            m_lastDangerPosition = vector.Zero;
        }
        
        // Update the current danger level
        m_currentDangerLevel = dangerLevel;
        
        // Clear old events
        for (int i = m_dangerEvents.Count() - 1; i >= 0; i--)
        {
            if (currentTime - m_dangerEvents[i].m_time > 30)
            {
                m_dangerEvents.Remove(i);
            }
        }
    }

    private void SetupDeathListenerForUnit(IEntity unitEntity)
    {
         SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(unitEntity);
         if (!ch) return;

         SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
         if (!ccc) {
             ////////Print("[IA_AiGroup.SetupDeathListenerForUnit] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
             return;
         }
         ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
         

    }

    private void SetupDeathListener()
    {

              
        if (!m_group)
        {
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        if (agents.IsEmpty())
        {
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }
        

        
        foreach (AIAgent agent : agents)
        {
            if (!agent)
            {
                continue;
            }
                
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                continue;
            }
                
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
            {
                continue;
            }
            EAISkill aiSkill = EAISkill.EXPERT;
			SCR_AICombatComponent combatComponent = SCR_AICombatComponent.Cast(agent.FindComponent(SCR_AICombatComponent));
			if (combatComponent)
				combatComponent.SetAISkill(aiSkill);
			
            // ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath); //This line is now removed
            
            // Set up danger event processing for this agent
            ProcessAgentDangerEvents(agent);
            
            // Verify the agent has danger event capability
            int dangerCount = agent.GetDangerEventsCount();
        }
        
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            GetGame().GetCallqueue().CallLater(CheckDangerEvents, 100, false);
        }
    }

    // New method to process danger events for an agent
    private void ProcessAgentDangerEvents(AIAgent agent)
    {
        if (!agent)
            return;
        
        // Skip for civilians or if the group is escaping
        if (m_isCivilian || m_faction == IA_Faction.CIV || m_tacticalState == IA_GroupTacticalState.Escaping)
            return;
        
        int dangerCount = agent.GetDangerEventsCount();
        
        if (dangerCount <= 0)
            return;
        
        IEntity agentEntity = agent.GetControlledEntity(); 
        FactionAffiliationComponent agentFacComp = FactionAffiliationComponent.Cast(agentEntity.FindComponent(FactionAffiliationComponent));
        FactionKey agentFactionKey = agentFacComp.GetAffiliatedFactionKey();
        
        for (int i = 0; i < dangerCount; i++)
        {
            int outCount;
            AIDangerEvent dangerEvent = agent.GetDangerEvent(i, outCount);
            if (!dangerEvent)
            {
                continue;
            }
                
            EAIDangerEventType dangerType = dangerEvent.GetDangerType();
            vector dangerVector = dangerEvent.GetPosition();
            
            IEntity sourceEntity = null;

            IA_GroupDangerType mappedDangerType;
            IA_AIReactionType reactionType;
            float intensity = 0.5;
            bool isSuppressed = false;
            
            bool handleEvent = true;
            switch (dangerType)
            {
                case EAIDangerEventType.Danger_WeaponFire:
                    mappedDangerType = IA_GroupDangerType.WeaponFire;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    AIDangerEventWeaponFire eventWeaponFire = AIDangerEventWeaponFire.Cast(dangerEvent);
                    sourceEntity = eventWeaponFire.GetObject();

                    // Check if sourceEntity is valid before proceeding
                    if (sourceEntity)
                    {
                        FactionAffiliationComponent facComp = FactionAffiliationComponent.Cast(sourceEntity.FindComponent(FactionAffiliationComponent));
                        // Check if facComp is valid before checking faction key
                        if (facComp)
                        {
                            FactionKey sourceFactionKey = facComp.GetAffiliatedFactionKey();
                            if(agentFactionKey == sourceFactionKey) // Check For Friendly Fire
                                continue; 
                        }else{
                            continue;
                        }
                        // else: Cannot determine source faction, assume hostile for now
                    }
                    // else: Cannot determine source entity, assume hostile

                    intensity = 0.6;
                    break;
                    
                case EAIDangerEventType.Danger_Explosion:
                    mappedDangerType = IA_GroupDangerType.Explosion;
                    reactionType = IA_AIReactionType.UnderFire; 
                    intensity = 0.9;
                    break;
                    
                case EAIDangerEventType.Danger_NewEnemy:
                    mappedDangerType = IA_GroupDangerType.EnemySpotted;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    intensity = 0.8;
                    break;
                    
                    
                default:
                    handleEvent = false;
                    continue; 
            }
            
            if (!handleEvent)
                continue;
                
            // Calculate distance to danger
            float distance = vector.Distance(GetOrigin(), dangerVector);
            
            // Use distance-based logic for intensity
            switch (mappedDangerType)
            {
                case IA_GroupDangerType.ProjectileImpact:
                    // Higher intensity for closer impacts
                    if (distance < BULLET_IMPACT_DISTANCE_MAX)
                    {
                        // Scale intensity based on distance (closer = more intense)
                        intensity = Math.Clamp(1.0 - (distance / BULLET_IMPACT_DISTANCE_MAX), 0.4, 0.9);
                    }
                    else
                    {
                        // Too far to care much
                        intensity = 0.2;
                    }
                    break;
                    
                case IA_GroupDangerType.ProjectileFlyby:
                    // Similar to impacts but slightly lower intensity
                    if (distance < PROJECTILE_FLYBY_RADIUS)
                    {
                        intensity = Math.Clamp(0.8 - (distance / PROJECTILE_FLYBY_RADIUS), 0.3, 0.7);
                    }
                    else
                    {
                        intensity = 0.1;
                    }
                    break;
                    
                case IA_GroupDangerType.WeaponFire:
                    // Scale based on audible distance
                    float maxDistance;
                    if (isSuppressed)
                    {
                        maxDistance = SUPPRESSED_AUDIBLE_DISTANCE;
                    }
                    else
                    {
                        maxDistance = GUNSHOT_AUDIBLE_DISTANCE;
                    }
                    
                    if (distance < maxDistance)
                    {
                        intensity = Math.Clamp(0.6 - (distance / maxDistance) * 0.5, 0.1, 0.6);
                    }
                    else
                    {
                        // Too far to hear
                        intensity = 0.0;
                        handleEvent = false;
                    }
                    break;
                    
                case IA_GroupDangerType.Explosion:
                    // Explosions have wider effect radius
                    if (distance < EXPLOSION_REACT_DISTANCE)
                    {
                        intensity = Math.Clamp(1.0 - (distance / EXPLOSION_REACT_DISTANCE), 0.3, 0.9);
                    }
                    else
                    {
                        // Too far to be concerning
                        intensity = 0.0;
                        handleEvent = false;
                    }
                    break;
            }
            
            if (!handleEvent || intensity <= 0.0)
                continue;
                
            ProcessDangerEvent(mappedDangerType, dangerVector, sourceEntity, intensity, isSuppressed);
        }
    }

    private bool WaterCheck(vector requestedLocation)
    {
		float oceanHeight = GetGame().GetWorld().GetOceanBaseHeight();
		return requestedLocation[1] <= oceanHeight; // True if at or below ocean level
    }
	

    
    void UpdateVehicleOrders()
    {
        if (!m_isDriving || !m_referencedEntity)
        {
            // If we are not supposed to be driving, or have no vehicle entity, nothing to do.
            // However, if we have a driving target but m_isDriving is false or m_referencedEntity is null,
            // this might be a state we can recover from.
            if (m_drivingTarget != vector.Zero && m_referencedEntity) // Still have a target and an entity reference
            {
                ////Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] WARNING: Group %1 m_isDriving is false but has drivingTarget %2 and referencedEntity. Attempting recovery.", 
                //    this, m_drivingTarget.ToString()), LogLevel.WARNING);
                m_isDriving = true; // Force driving state back
                // Proceed to waypoint update logic
            }
            else if (m_drivingTarget != vector.Zero && !m_referencedEntity) // Still have a target but lost entity reference
            {
                Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] CRITICAL: Group %1 lost referencedEntity but still has drivingTarget %2. Cannot recover without entity.",
                    this, m_drivingTarget.ToString()), LogLevel.ERROR);
                ClearVehicleReference(); // Cannot recover without an entity reference.
                return;
            }
            else
            {
                return; // No driving target, or no entity and no driving flag.
            }
        }
            
        int currentTime = System.GetUnixTime();
    
        if (currentTime - m_lastVehicleOrderTime < VEHICLE_ORDER_UPDATE_INTERVAL)
        {
            return;
        }
            
        m_lastVehicleOrderTime = currentTime;
        
        Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
        if (!vehicle)
        {
            // --- BEGIN MODIFICATION FOR RESILIENCE ---
            if (m_drivingTarget != vector.Zero)
            {
                Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] WARNING: Group %1 Vehicle.Cast(m_referencedEntity) failed but m_drivingTarget (%2) exists. Attempting to continue.", 
                    this, m_drivingTarget.ToString()), LogLevel.WARNING);
                
                // We assume m_referencedEntity still holds the intended vehicle, even if cast failed temporarily.
                // Try to update waypoint directly. If m_referencedEntity is truly invalid, UpdateVehicleWaypoint should handle it.
                // We need a valid Vehicle reference for UpdateVehicleWaypoint. If it cannot be cast, we cannot proceed.
                // This path implies a more serious issue if the entity can no longer be cast to Vehicle.
                // For now, we'll log it and proceed to ClearVehicleReference as the original code did,
                // as forcing it without a valid castable Vehicle might lead to further errors.
                // A more robust solution might involve trying to re-acquire the vehicle if m_referencedEntity is stale.
                ////Print("Vehicle Gone or uncastable, removing from sync service", LogLevel.WARNING);
                ClearVehicleReference();
                return;
            }
            else
            {
                // No driving target, so it's safe to clear.
                ////Print("Vehicle Gone (or uncastable) and no driving target, removing from sync service",LogLevel.WARNING);
                ClearVehicleReference();
                return;
            }
            // --- END MODIFICATION FOR RESILIENCE ---
        }
        
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        if (characters.IsEmpty())
        {
            // No units left in the group, something is wrong
            // TODO: Maybe despawn the group or handle this case?
            return;
        }
            
        bool anyOutside = false;
        
        foreach (SCR_ChimeraCharacter character : characters)
        {
            if (!character.IsInVehicle())
            {
                anyOutside = true;
                break;
            }
        }
        
        if (anyOutside)
        {
            // Someone is outside, order them back in
            ////Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] Group %1 members outside vehicle. Ordering GetInVehicle.", this), LogLevel.NORMAL);
            RemoveAllOrders();
            
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            
            // Ensure driving state is set correctly
            m_isDriving = true;
            
            return; // Don't proceed with movement orders until everyone is inside
        }
        
        // If everyone is in the vehicle and we have a destination, make sure we have a waypoint
        if (!anyOutside && m_drivingTarget != vector.Zero)
        {
            // Check if we need a new waypoint (don't have one or reached destination)
            bool hasActiveWP = HasActiveWaypoint();
            // If vehicle is null here (e.g. from a failed cast earlier that we tried to recover from),
            // HasVehicleReachedDestination might behave unexpectedly or error.
            // However, the check 'if(!vehicle)' above should prevent reaching here with a null vehicle.
            bool vehicleReachedDest = IA_VehicleManager.HasVehicleReachedDestination(vehicle, m_drivingTarget);
            
            // Debug //Print for waypoint status
            // //Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] Group %1 vehicle waypoint check: HasActiveWP=%1, ReachedDest=%2", this, hasActiveWP, vehicleReachedDest), LogLevel.NORMAL);
                  
            if (!hasActiveWP || vehicleReachedDest)
            {
                // Need a new waypoint
                //Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] Group %1 vehicle needs new waypoint to %2.", this, m_drivingTarget.ToString()), LogLevel.NORMAL);
                // Ensure driving state is set (might have been unset if we tried to recover at the start)
                m_isDriving = true; 
                
                IA_VehicleManager.UpdateVehicleWaypoint(vehicle, this, m_drivingTarget);
            }
        }
        else if (m_drivingTarget == vector.Zero)
        {
            // Vehicle group has no destination - perhaps assign a patrol or hold?
            // For now, do nothing, let it idle or follow base game logic.
            // //Print(string.Format("[IA_AiGroup.UpdateVehicleOrders] Group %1 vehicle has no driving target.", this), LogLevel.NORMAL);
        }
    }

    // Add a method to properly reset vehicle state
    void ClearVehicleReference()
    {
        
        //Print(string.Format("[IA_AiGroup.ClearVehicleReference] Group %1 clearing vehicle reference. Was driving: %2, Entity: %3, Target: %4",
//            this, m_isDriving, m_referencedEntity, m_drivingTarget.ToString()), LogLevel.NORMAL);

        if (m_referencedEntity)
        {
            Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
            if (vehicle)
            {
                IA_VehicleManager.ReleaseVehicleReservation(vehicle);
            }
        }
        
        // Reset vehicle-related state variables
        m_isDriving = false;
        m_referencedEntity = null;
        m_drivingTarget = vector.Zero;
    }

    IEntity GetReferencedEntity()
    {
        return m_referencedEntity;
    }
    
    vector GetDrivingTarget()
    {
        return m_drivingTarget;
    }

    void ForceDrivingState(bool state)
    {
        m_isDriving = state;
    }

    void ProcessReactions()
    {
        // This function is now a no-op as reaction processing is handled by the area instance
            return;
    }

    private void ScheduleNextStateEvaluation()
    {
        if (m_isStateEvaluationScheduled)
            return;
        
        // Random interval between min and max times
        int interval = Math.RandomInt(MIN_STATE_EVALUATION_INTERVAL, MAX_STATE_EVALUATION_INTERVAL);
        
        // Schedule the next evaluation
        GetGame().GetCallqueue().CallLater(EvaluateGroupState, interval, false);
        
        m_isStateEvaluationScheduled = true;
        m_nextStateEvaluationTime = System.GetUnixTime() + (interval / 1000);
    }

    private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {

        IEntity victimEntity = memberCtrl.GetOwner();
        vector deathPosition = victimEntity.GetOrigin();
        
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
			int playerID = killer.GetInstigatorPlayerID();
			if (playerID > 0)
			{
				string playerGuid = GetGame().GetBackendApi().GetPlayerUID(playerID);
				string playerName = GetGame().GetPlayerManager().GetPlayerName(playerID);
				
				if (m_OwningSideObjective)
				{
					IA_AssassinationObjective assassinObj = IA_AssassinationObjective.Cast(m_OwningSideObjective);
					if (assassinObj)
					{
						// This death is part of an assassination objective
						if (assassinObj.IsHVTGroup(this))
						{
							// This is the HVT's group. The HVT's death is tracked separately in IA_SideObjective.OnHVTKilled.
							// Do nothing here to prevent logging a duplicate PlayerKill.
						}
						else
						{
							// This is a guard's group.
							IA_StatsManager.GetInstance().QueueHVTGuardKill(playerGuid, playerName);
						}
					}
					else
					{
						// Part of some other type of side objective. Log as a normal player kill.
						IA_StatsManager.GetInstance().QueuePlayerKill(playerGuid, playerName);
					}
				}
				else
				{
					// Not part of a side objective. Log as a normal player kill.
					IA_StatsManager.GetInstance().QueuePlayerKill(playerGuid, playerName);
				}
			}
            
            IA_Faction playerFaction = IA_Faction.NONE;
            SCR_ChimeraCharacter playerCharacter = SCR_ChimeraCharacter.Cast(killerEntity);
            
            if (playerCharacter)
            {
                FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(playerCharacter.FindComponent(FactionAffiliationComponent));
                if (factionComponent)
                {
                    Faction faction = factionComponent.GetAffiliatedFaction();
                    if (faction)
                    {
                        string factionKey = faction.GetFactionKey();
                        // Map the faction key to our IA_Faction enum
                        if (factionKey == "US")
						{
							if (m_isCivilian && m_owningAreaInstance)
								m_owningAreaInstance.OnCivilianKilledByPlayer();
                            playerFaction = IA_Faction.US;
						}
                        else if (factionKey == "USSR")
                            playerFaction = IA_Faction.USSR;
                        else if (factionKey == "FIA")
                            playerFaction = IA_Faction.FIA;
                        else if (factionKey == "CIV")
                            playerFaction = IA_Faction.CIV;

                    }
                }
            }
            
            if (playerFaction == IA_Faction.NONE)
            {
                if (m_faction == IA_Faction.USSR)
                    playerFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    playerFaction = IA_Faction.USSR;
                else
                    playerFaction = IA_Faction.CIV; // Default fallback if all else fails

            }
            
            // Legacy engagement system for compatibility
            if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
            {
                m_engagedEnemyFaction = playerFaction;
            }
        }
        else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
        {
            SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
            if (!c)
            {
                return;
            }
            
            string fKey = c.GetFaction().GetFactionKey();
            
            // Determine the faction
            IA_Faction killerFaction = IA_Faction.NONE;
            if (fKey == "US")
            {
                killerFaction = IA_Faction.US;
            }
            else if (fKey == "USSR")
            {
                killerFaction = IA_Faction.USSR;
            }
            else if (fKey == "CIV")
            {
                killerFaction = IA_Faction.CIV;
            }
            else if (fKey == "FIA")
            {
                killerFaction = IA_Faction.FIA;
            }
            
            // Legacy engagement system
            if (killerFaction != m_faction)
            {
                m_engagedEnemyFaction = killerFaction;
            }
            // If friendly fire, reset engagement.
            if (m_engagedEnemyFaction == m_faction)
            {
                m_engagedEnemyFaction = IA_Faction.NONE;
            }
        }
    }

    // Evaluate and potentially change tactical state based on situation
    void EvaluateTacticalState()
    {
        // If the group is in the Escaping state, it must not be interrupted.
        if (m_tacticalState == IA_GroupTacticalState.Escaping)
        {
            // If the group somehow lost its move order, re-issue it with high priority.
            if (!HasOrders() && m_tacticalStateTarget != vector.Zero)
            {
                AddOrder(m_tacticalStateTarget, IA_AiOrder.PriorityMove, true);
            }
            return; // Do not allow any other logic to override the escape.
        }
		
        // Override tactical state evaluation in defend mode - but don't constantly re-set the state
        if (m_isInDefendMode && m_defendTarget != vector.Zero)
        {
            // In defend mode, only ensure we have orders if we don't have any
            // Don't constantly change the tactical state - it should already be set correctly
            if (!HasOrders())
            {
                // Re-add the SearchAndDestroy order if it was somehow lost
                AddOrder(m_defendTarget, IA_AiOrder.SearchAndDestroy, true);
                Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Defend mode group lost orders, re-adding SearchAndDestroy at %1", m_defendTarget.ToString()), LogLevel.WARNING);
            }
            return; // Don't do any further state evaluation in defend mode
        }
        
        // If the last order was very recent, let it execute
        int timeSinceLastOrder = TimeSinceLastOrder();
        if (timeSinceLastOrder < 3) {
             return;
        }

        // If the state is being managed by an external authority (like AreaInstance),
        // do not allow the group to change its own tactical state autonomously.
        if (m_isStateManagedByAuthority)
        {
            // When under authority control, only check for critical emergency conditions
            // that might warrant a state change request to the authority
            
            // If we don't have orders but should have orders for this state type, reapply them
            if (!HasOrders())
            {
                // Only reapply for states that should have persistent orders
                if (m_tacticalState == IA_GroupTacticalState.Attacking || 
                    m_tacticalState == IA_GroupTacticalState.Defending || 
                    m_tacticalState == IA_GroupTacticalState.DefendPatrol ||
                    m_tacticalState == IA_GroupTacticalState.Flanking ||
                    m_tacticalState == IA_GroupTacticalState.LastStand)
                {
					if(m_tacticalState == IA_GroupTacticalState.DefendPatrol && timeSinceLastOrder < 150)
						return;
						
                    //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Reapplying authority-managed state %1 for group %2", 
//                        typename.EnumToString(IA_GroupTacticalState, m_tacticalState), this), LogLevel.NORMAL);
                    ApplyTacticalStateOrders();
                }
            }
            
            // Even under authority control, check for critical conditions that should force a state change request
            // These are emergency conditions that may override current orders
            int aliveCount = GetAliveCount();
            int initialCount = GetInitialUnitCount();
            
            // Critical condition 1: Flanking/attackers down to last unit
            if ((m_tacticalState == IA_GroupTacticalState.Attacking || m_tacticalState == IA_GroupTacticalState.Flanking) && 
                aliveCount == 1 && initialCount > 2)
            {
                // Request retreat to preserve the last unit
                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] CRITICAL CONDITION: Attacking/Flanking group %1 down to last unit - requesting retreat", 
//                    this), LogLevel.WARNING);
                    
                // Request state change rather than making it directly
                RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
                return;
            }
            
            // Critical condition 2: Severe losses (less than 33% of original strength) for attackers/flankers
            if ((m_tacticalState == IA_GroupTacticalState.Attacking || m_tacticalState == IA_GroupTacticalState.Flanking) && 
                initialCount >= 3 && aliveCount < (initialCount / 3))
            {
                float survivingRatio;
                if (initialCount > 0) {
                    survivingRatio = aliveCount / (float)initialCount;
                } else {
                    survivingRatio = 0;
                }
                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] SEVERE LOSSES: Attacking/Flanking group %1 at %2% strength - requesting role reassessment", 
//                    this, Math.Round(survivingRatio * 100)), LogLevel.WARNING);
                    
                // High danger - request retreat or defend based on current danger level
                if (m_currentDangerLevel > 0.7)
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
                }
                else
                {
                    // Lower danger - hold position but request switch to defending
                    RequestTacticalStateChange(IA_GroupTacticalState.Defending, GetOrigin());
                }
                return;
            }
            
            // Critical condition 3: Under immediate threat with appropriate response needed
            if (m_currentDangerLevel > 0.8 && m_lastDangerPosition != vector.Zero)
            {
                int timeSinceLastDanger = System.GetUnixTime() - m_lastDangerEventTime;
                if (timeSinceLastDanger < 10) // Very recent danger (last 10 seconds)
                {
                    //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] IMMEDIATE THREAT: Group %1 under high danger (%2) - requesting appropriate response",
//                        this, m_currentDangerLevel), LogLevel.WARNING);

                    // Request appropriate response based on current state and strength
                    if (aliveCount <= 2)
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
                    }
                    else // More than 2 units alive
                    {
                        // --- BEGIN ADDED: Distance check for flanking --- 
                        vector currentPos = GetOrigin();
                        float distanceSqToThreat = vector.DistanceSq(currentPos, m_lastDangerPosition);
                        bool threatFarEnoughForFlank = distanceSqToThreat >= (80 * 80); // 80 meters squared
                        // --- END ADDED --- 
                        
                        // If already assigned an offensive role by authority, maintain offense
                        // MODIFIED: Handle Attacking and Flanking separately
                        if (m_isStateManagedByAuthority && m_tacticalState == IA_GroupTacticalState.Attacking)
                        {
                             // 35% chance to request flanking, otherwise attack - ONLY if far enough
                            if (threatFarEnoughForFlank && Math.RandomFloat01() < 0.35)
                            {
                                RequestTacticalStateChange(IA_GroupTacticalState.Flanking, m_lastDangerPosition);
                                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Threat Response: Requesting FLANKING (Assigned Attacker, Dist=%.1fm)", this, Math.Sqrt(distanceSqToThreat)), LogLevel.NORMAL);
                            }
                            else
                            {
                                RequestTacticalStateChange(IA_GroupTacticalState.Attacking, m_lastDangerPosition); // Continue attacking
                                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Threat Response: Requesting ATTACKING (Assigned Attacker, Dist=%.1fm, ThreatTooClose=%1)", this, Math.Sqrt(distanceSqToThreat), !threatFarEnoughForFlank), LogLevel.NORMAL);
                            }
                        }
                        // If already assigned Flanking, continue flanking (don't request Attacking)
                        else if (m_isStateManagedByAuthority && m_tacticalState == IA_GroupTacticalState.Flanking)
                        {
                             // Continue Flanking - potentially re-evaluate flank position if threat moved significantly?
                             // For now, just don't request a change to attacking. Maybe request Flanking again?
                             // Let's request Flanking again to potentially update the target position if needed by AreaInstance logic.
                             // RequestTacticalStateChange(IA_GroupTacticalState.Flanking, m_lastDangerPosition); // REMOVED: Don't re-request flank if already flanking.
                             //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Threat Response: Maintaining FLANKING (Already Flanking, Dist=%.1fm)", this, Math.Sqrt(distanceSqToThreat)), LogLevel.NORMAL); // MODIFIED Log message
                        }
                        else // Not currently assigned an offensive role by authority
                        {
                             // Higher chance (50%) to request flanking if not already attacking/flanking - ONLY if far enough
                            if (threatFarEnoughForFlank && Math.RandomFloat01() < 0.50)
                            {
                                RequestTacticalStateChange(IA_GroupTacticalState.Flanking, m_lastDangerPosition);
                                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Threat Response: Requesting FLANKING (Not Assigned Offense, Dist=%.1fm)", this, Math.Sqrt(distanceSqToThreat)), LogLevel.NORMAL);
                            }
                            else
                            {
                                // Default response: request attack towards the source of the danger
                                // This case implies the current state is not Attacking or Flanking
                                RequestTacticalStateChange(IA_GroupTacticalState.Attacking, m_lastDangerPosition);
                                //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Threat Response: Requesting ATTACKING (Not Assigned Offense, Dist=%.1fm, ThreatTooClose=%1)", this, Math.Sqrt(distanceSqToThreat), !threatFarEnoughForFlank), LogLevel.NORMAL);
                            }
                        }
                    }
                    return;
                }
            }
            
            return; // Exit early for authority-managed groups
        }

        // --- AUTONOMOUS BEHAVIOR FOR NON-AUTHORITY MANAGED GROUPS ---
        // This section only executes for groups NOT managed by the area authority
        
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            // Check for danger events first - these need highest priority
            int currentTime = System.GetUnixTime();
            int timeSinceLastDanger = currentTime - m_lastDangerEventTime;

            // Update engagement status if needed
            if (m_lastDangerEventTime > 0 && m_engagedEnemyFaction == IA_Faction.NONE)
            {
                if (m_faction == IA_Faction.USSR)
                    m_engagedEnemyFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    m_engagedEnemyFaction = IA_Faction.USSR;
            }

            // Detect and react to danger
            if (m_lastDangerEventTime > 0 && timeSinceLastDanger < 90)
            {
                // Get position to react to
                vector targetPos = m_lastDangerPosition;
                if (targetPos == vector.Zero)
                {
                    targetPos = IA_Game.rng.GenerateRandomPointInRadius(20, 50, GetOrigin());
                }
                
                // Request tactical state based on situation
                int aliveCount = GetAliveCount();
                float dangerLevel = m_currentDangerLevel;
                int initialCount = GetInitialUnitCount();
                float strengthRatio;
                if (initialCount > 0) {
                    strengthRatio = aliveCount / (float)initialCount;
                } else {
                    strengthRatio = 1.0;
                }
                
                // Request appropriate state change based on situation
                // High danger and critically low strength (<=25%) - retreat
                if ((dangerLevel > 0.6 && aliveCount <= 2) || (dangerLevel > 0.5 && strengthRatio <= 0.25))
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Retreating, targetPos);
                }
                // High danger but good numbers - defend
                else if (dangerLevel > 0.65)
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Defending, GetOrigin());
                }
                // Critical numbers, make last stand
                else if (aliveCount == 1 && dangerLevel > 0.3)
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.LastStand, GetOrigin());
                }
                // Good numbers with flanking opportunity
                else if (aliveCount >= 3 && strengthRatio >= 0.6 && Math.RandomFloat(0, 1) < 0.45)
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Flanking, targetPos);
                }
                // Default to attacking when no special conditions, UNLESS currently flanking or defending
                else if (m_tacticalState != IA_GroupTacticalState.Flanking && m_tacticalState != IA_GroupTacticalState.Defending)
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Attacking, targetPos);
                }
                // If currently flanking or defending and none of the above conditions met, the group will continue its current state
                
                // Ensure faction is set for combat
                if (m_engagedEnemyFaction == IA_Faction.NONE)
                {
                    if (m_faction == IA_Faction.USSR)
                        m_engagedEnemyFaction = IA_Faction.US;
                    else if (m_faction == IA_Faction.US)
                        m_engagedEnemyFaction = IA_Faction.USSR;
                }
                
                return;
            }
            SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			
            // Even if no recent danger, maintain combat readiness if already engaged
            if (IsEngagedWithEnemy() && GetAliveCount() > 0 && timeSinceLastOrder > 30)
            {
                // If no tactical state set, request one based on circumstances
                if (m_tacticalState == IA_GroupTacticalState.Neutral)
                {
                    int aliveCount = GetAliveCount();
                    int initialCount = GetInitialUnitCount();
                    float strengthRatio;
                    if (initialCount > 0) {
                        strengthRatio = aliveCount / (float)initialCount;
                    } else {
                        strengthRatio = 1.0;
                    }
                    
                    // Request appropriate state based on strength
                    if (aliveCount <= 2 || strengthRatio < 0.3)
                    {
                        //Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Group %1 unit strength too low for attack/flank - requesting Defend", this), LogLevel.NORMAL);
                        RequestTacticalStateChange(IA_GroupTacticalState.Defending, GetOrigin());
                    }
                    else if (aliveCount >= 3 && strengthRatio >= 0.6 && Math.RandomFloat(0, 1) < 0.35)
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Flanking, m_lastDangerPosition);
                    }
                    else
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Attacking, m_lastDangerPosition);
                    }
                }
                // If already have a tactical state but need orders, refresh them
                else if (!HasOrders())
                {
                    ApplyTacticalStateOrders();
                }
                
                return;
            }
        }

        // For civilians or fallback for military units
        // These groups operate more independently
        
        // If group is severely weakened, consider retreating
        int aliveCount = GetAliveCount();
        if (aliveCount <= 2)
        {
            if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
            {
                // Request tactical state for military units
                RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
            }
            else // Civilian Path for aliveCount <= 2
            {
                // Only issue new orders if not driving AND has no current orders
                if (!m_isDriving && !HasOrders())
                {
                    // Direct orders for civilians
                    vector retreatDir = GetOrigin() - m_lastOrderPosition;
                    if (retreatDir.LengthSq() > 0.1)
                        retreatDir = retreatDir.Normalized();
                    else
                        retreatDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();
        
                    vector retreatPos = GetOrigin() + retreatDir * 50;
                    // No need to call RemoveAllOrders() here because HasOrders() was false
                    AddOrder(retreatPos, IA_AiOrder.Patrol);
                }
            }
        }
        // Otherwise, maintain combat stance by defending for civilians
        else if (m_isCivilian || m_faction == IA_Faction.CIV) // Civilian Path for aliveCount > 2
        {
            // Only issue new orders if not driving AND has no current orders
            if (!m_isDriving && !HasOrders())
            {
                // Only give direct orders for civilians
                vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, GetOrigin());
                // No need to call RemoveAllOrders() here because HasOrders() was false
                AddOrder(defendPos, IA_AiOrder.Defend);
            }
        }
    }
    
    // Return current tactical state as string (for debug)
    string GetTacticalStateAsString()
    {
        return typename.EnumToString(IA_GroupTacticalState, m_tacticalState);
    }

    IA_GroupTacticalState GetTacticalState()
    {
        return m_tacticalState;
    }
    

    
    // --- BEGIN ADDED: Getter for Last Danger Position ---
    vector GetLastDangerPosition()
    {
        return m_lastDangerPosition;
    }
    // --- END ADDED ---
    
    // --- BEGIN ADDED: Getter for Initial Unit Count ---
    int GetInitialUnitCount()
    {
        return m_initialUnitCount;
    }
    // --- END ADDED ---

    // Fix accidental removal of CreateCivilianGroupForVehicle and AssignVehicle methods
    static IA_AiGroup CreateCivilianGroupForVehicle(Vehicle vehicle, int unitCount)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // slightly above vehicle
        IA_AiGroup grp = IA_AiGroup.CreateCivilianGroup(spawnPos);
        return grp;
    }

    void AssignVehicle(Vehicle vehicle, vector destination)
    {
        if (!vehicle || !IsSpawned())
        {
            return;
        }
        
        bool reserved = IA_VehicleManager.ReserveVehicle(vehicle, this);
        if (!reserved)
        {
            return;
        }
        
        m_referencedEntity = vehicle;
        m_isDriving = true;
        m_drivingTarget = destination; // Keep this to store the ultimate destination
        
        m_lastOrderPosition = vehicle.GetOrigin();
        m_lastOrderTime = System.GetUnixTime();
        
        // Set the tactical state to InVehicle. This will call RemoveAllOrders internally.
        // Pass 'destination' as the target position for the InVehicle state.
        // Pass 'vehicle' as the target entity for the state.
        // Mark this as an authoritative state change.
        SetTacticalState(IA_GroupTacticalState.InVehicle, destination, vehicle, true);
        
        // The call to IA_VehicleManager.UpdateVehicleWaypoint previously here is now handled
        // by the updated SetTacticalState logic for IA_GroupTacticalState.InVehicle.
    }

    void Despawn()
    {
        if (!IsSpawned())
        {
            return;
        }
        m_isSpawned = false;
        
        if (m_isDriving || m_referencedEntity)
        {
            ClearVehicleReference();
        }
        
        m_isStateEvaluationScheduled = false;

        if (m_group)
        {
            m_group.DeactivateAllMembers();
        }
        RemoveAllOrders();
        IA_Game.AddEntityToGc(m_group);
        m_group = null;
    }

    // Add a public SetTacticalState method to replace the one we accidentally removed
    void SetTacticalState(IA_GroupTacticalState newState, vector targetPos = vector.Zero, IEntity targetEntity = null, bool fromAuthority = false)
    {
        // Add logging to show the state change
        if (m_tacticalState != newState)
        {
            //Print(string.Format("[IA_AiGroup.SetTacticalState] Group %1 changing state: %2 -> %3", 
//                this, m_tacticalState, newState), LogLevel.NORMAL);
                
            // Record the time when the state changed
            m_tacticalStateStartTime = System.GetUnixTime();
        }
        
        // Authority safety check - log a warning when called without authority flag
        if (!fromAuthority)
        {
            Print(string.Format("[IA_AiGroup.SetTacticalState] WARNING: Called without authority flag. Groups should request state changes instead of setting directly."), 
                LogLevel.DEBUG);
        }
        
        // Manage the authority flag - can be acquired by authority, or released by non-authority
        if (fromAuthority)
        {
            m_isStateManagedByAuthority = true; // Authority takes control
        }
        else if (m_isStateManagedByAuthority)
        {
            // If currently under authority control, log that we're getting a non-authority state change
            Print(string.Format("[IA_AiGroup.SetTacticalState] WARNING: Group %1 state changed from %2 to %3 by non-authority source!", 
                this, m_tacticalState, newState), LogLevel.DEBUG);
            m_isStateManagedByAuthority = false; // Only release authority when explicitly changed by non-authority
        }

        // Clear any pending state request
        m_hasPendingStateRequest = false;

        m_tacticalState = newState;
        m_tacticalStateTarget = targetPos;
        
        // Apply orders based on the state
        RemoveAllOrders();
        
        switch (m_tacticalState)
        {
            case IA_GroupTacticalState.Attacking:
                if (targetPos != vector.Zero)
                {
                    AddOrder(targetPos, IA_AiOrder.SearchAndDestroy, true);
                }
                break;
                
            case IA_GroupTacticalState.Defending:
                vector defendPos;
                if (targetPos != vector.Zero) {
                    defendPos = targetPos;
                } else {
                    defendPos = m_lastConfirmedPosition; // No target, defend where you're standing
                }
                
                // Log Defend State Info
                //Print(string.Format("[IA_AiGroup.SetTacticalState] DEFEND ORDER DEBUG: Group %1 | Faction: %2 | AliveCount: %3 | Position: %4 | Target: %5", 
                //    this, m_faction, GetAliveCount(), m_lastConfirmedPosition.ToString(), defendPos.ToString()), LogLevel.NORMAL);
                
                // Add some vertical offset to avoid waypoints in ground
                if (defendPos[1] < 5)
                {
                    defendPos[1] = defendPos[1] + 0.5;
                }
                
                AddOrder(defendPos, IA_AiOrder.Defend, true);
                break;
                
            case IA_GroupTacticalState.Flanking:
                // If a specific target is given, use that
                if (targetPos != vector.Zero)
                {
                    // --- BEGIN MODIFIED: Calculate proper flanking position ---
                    // Instead of just moving to the target position, calculate a proper flanking maneuver
                    vector groupPos = GetOrigin();
                    vector enemyPos = targetPos; // The target position is where we believe the enemy is
                    
                    // Calculate a flanking position using our utility method
                    vector flankPos = CalculateFlankingPosition(enemyPos, groupPos);
                    
                    // Store the original threat position for later use
                    m_originalThreatPosition = enemyPos;
                    m_isInFlankingPhase = true;
                    
                    // Log the flanking maneuver
                    //Print(string.Format("[IA_AiGroup.SetTacticalState] FLANKING MANEUVER: Group %1 flanking from %2 around enemy at %3 to position %4", 
//                        this, groupPos.ToString(), enemyPos.ToString(), flankPos.ToString()), LogLevel.NORMAL);
                    
                    // Add the order to move to the flanking position first
                    AddOrder(flankPos, IA_AiOrder.Move, true);
                    
                    // Then add a secondary order to attack the original position once flanking is complete
                    // This creates a two-phase flanking maneuver: first move to flank, then attack
                    AddOrder(enemyPos, IA_AiOrder.SearchAndDestroy, false);
                    // --- END MODIFIED ---
                }
                break;
                
            case IA_GroupTacticalState.DefendPatrol:
                // No target for patrol state - generate one
                if (m_lastAssignedArea)
                {
                    vector areaOrigin = m_lastAssignedArea.GetOrigin();
                    float radius = m_lastAssignedArea.GetRadius() * 0.75; // Stay within the area
                    
                    // Only generate a random patrol if we're not already close to the center
                  
                        // Generate a random point to patrol to within the area
                        vector patrolPoint = IA_Game.rng.GenerateRandomPointInRadius(1, radius, areaOrigin);
                        
                        // Apply orders
                        AddOrder(patrolPoint, IA_AiOrder.Patrol, false);

                }
                break;
            
            case IA_GroupTacticalState.InVehicle:
                // For groups in vehicles - maybe handled at a higher level
                // If m_isDriving is true, m_referencedEntity is valid, and a targetPos (destination) is provided,
                // ensure the vehicle waypoint is re-established.
                // This is crucial because RemoveAllOrders() was called at the start of SetTacticalState.
                if (m_isDriving && m_referencedEntity && targetPos != vector.Zero)
                {
                    Vehicle veh = Vehicle.Cast(m_referencedEntity);
                    // It's also good practice to ensure targetEntity (if provided for InVehicle state) matches m_referencedEntity
                    // or is the vehicle itself.
                    if (veh) // && (targetEntity == null || targetEntity == veh))
                    {
                        IA_VehicleManager.UpdateVehicleWaypoint(veh, this, targetPos); 
                    }
                }
                break;
			
			case IA_GroupTacticalState.Escaping:
                if (targetPos != vector.Zero)
                {
                    AddOrder(targetPos, IA_AiOrder.PriorityMove, true);
                }
                break;
                
            default:
                // Unknown state, do nothing
                break;
        }
    }

    // Add a public ApplyTacticalStateOrders method to reapply orders based on current state
    void ApplyTacticalStateOrders()
    {
        // Simply call SetTacticalState with the current state and authority status
        SetTacticalState(m_tacticalState, m_tacticalStateTarget, null, m_isStateManagedByAuthority); // Pass authority status
    }

    // Make sure CheckDangerEvents is defined as a public method
    void CheckDangerEvents()
    {
        // Throttle the main check logic per group
        int currentTime_check = GetGame().GetWorld().GetWorldTime();
        if (currentTime_check - m_lastAgentEventCheckTime < AGENT_EVENT_CHECK_INTERVAL_MS)
        {
            return; // Skip check if too soon
        }
        m_lastAgentEventCheckTime = currentTime_check;

        if (!IsSpawned() || !m_group)
            return;
        
        // Skip for civilian groups
        if (m_isCivilian)
            return;
        
        // For each agent in the group, check if they have any danger events
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        foreach (AIAgent agent : agents)
        {
            if (agent)
            {
                ProcessAgentDangerEvents(agent);
            }
        }
    }

    // Make sure CalculateWaypointPriority is defined as a public method
    int CalculateWaypointPriority(IA_AiOrder order)
    {
        // Base priority level starts at 0
        int priority = 0;
        
        // Increase priority based on order type
        switch (order)
        {
            case IA_AiOrder.SearchAndDestroy:
                priority += 35; 
                break;
			case IA_AiOrder.VehicleMove:
                return 0;
            case IA_AiOrder.Defend:
                return 20;
			case IA_AiOrder.DefendSmall:
                return 40;
            case IA_AiOrder.GetInVehicle:
                priority += 100;  // Vehicle mounting is important
                break;
            case IA_AiOrder.PriorityMove:
                // If the group is escaping, give it the highest possible priority.
                if (m_tacticalState == IA_GroupTacticalState.Escaping)
                    return 350;
                return 60;  // High priority movement
            case IA_AiOrder.Move:
                priority += 30;  // Move orders medium priority (between combat and patrol)
                break;
            case IA_AiOrder.Patrol:
                priority += 1;  // Patrol orders lowest priority
                break;
        }
        
        // Increase priority if in danger
        if (m_currentDangerLevel > 0.0)
        {
            priority += Math.Round(1 * m_currentDangerLevel);
        }
        
        // Increase priority if engaged with enemy
        if (IsEngagedWithEnemy())
        {
            priority += 15;
        }
        
        // Adjust priority based on group strength
        int aliveCount = GetAliveCount();
        if (aliveCount == 2)
        {
            // Small groups prioritize their orders more (survival focused)
            priority += 10;
        }
        if (aliveCount == 1)
        {
            // Small groups prioritize their orders more (survival focused)
            priority += 30;
        }
        
        // More recent danger events increase priority
        if (m_lastDangerEventTime > 0)
        {
            int timeSinceLastDanger = System.GetUnixTime() - m_lastDangerEventTime;
            if (timeSinceLastDanger < 30)
            {
                priority += 5; // Recent danger gets higher priority
            }
        }
        
        return priority;
    }

    // Improved flanking position calculation
    vector CalculateFlankingPosition(vector enemyPos, vector groupPos)
    {
        // Get direction from enemy to group
        vector dirFromEnemy = groupPos - enemyPos;
        float distanceToEnemy = dirFromEnemy.Length();
        dirFromEnemy = dirFromEnemy.Normalized();
        
        // --- BEGIN MODIFIED FLANKING LOGIC ---
        // Calculate perpendicular direction vectors (both left and right flanks)
        vector leftFlankDir = Vector(-dirFromEnemy[2], 0, dirFromEnemy[0]).Normalized();
        vector rightFlankDir = Vector(dirFromEnemy[2], 0, -dirFromEnemy[0]).Normalized();
        
        // Calculate flanking distance - varies based on distance to enemy and group size
        float baseFlanking = 80; // Base flanking distance
        int aliveCount = GetAliveCount();
        
        // Scale flank distance based on original distance to ensure it's proportional
        float distanceScale = Math.Clamp(distanceToEnemy / 200, 0.5, 2.0);
        float groupSizeScale = Math.Clamp(aliveCount / 5.0, 0.7, 1.3); // Larger groups flank wider
        
        // Add some randomness to flanking distance (60-120% of calculated value)
        float randomFactor = Math.RandomFloat(0.6, 1.2);
        
        // Calculate final flanking distance with all factors
        float flankDistance = baseFlanking * distanceScale * groupSizeScale * randomFactor;
        
        // Ensure minimum and maximum flanking distances
        flankDistance = Math.Clamp(flankDistance, 50, 150);
        
        // Get a slight forward component to enable encirclement
        vector forwardComponent = -dirFromEnemy * Math.RandomFloat(0, 0.4); // Small random forward push
        
        // Calculate final potential flanking positions including the forward component
        vector finalLeftFlankDir = (leftFlankDir + forwardComponent).Normalized();
        vector finalRightFlankDir = (rightFlankDir + forwardComponent).Normalized();
        
        vector leftFlankPos = enemyPos + (finalLeftFlankDir * flankDistance);
        vector rightFlankPos = enemyPos + (finalRightFlankDir * flankDistance);
        
        // Calculate squared distances from group's current position to potential flank positions
        float distSqToLeft = vector.DistanceSq(groupPos, leftFlankPos);
        float distSqToRight = vector.DistanceSq(groupPos, rightFlankPos);
        
        // Choose the closer flanking position
        vector flankPos;
        if (distSqToLeft <= distSqToRight)
        {
            flankPos = leftFlankPos;
            //Print(string.Format("[IA_AiGroup.CalculateFlankingPosition] Group %1 choosing LEFT flank (Closer: %.1fm vs %.1fm)", 
//                this, Math.Sqrt(distSqToLeft), Math.Sqrt(distSqToRight)), LogLevel.NORMAL);
        }
        else
        {
            flankPos = rightFlankPos;
            //Print(string.Format("[IA_AiGroup.CalculateFlankingPosition] Group %1 choosing RIGHT flank (Closer: %.1fm vs %.1fm)", 
            //    this, Math.Sqrt(distSqToRight), Math.Sqrt(distSqToLeft)), LogLevel.NORMAL);
        }
        
        //Print(string.Format("[IA_AiGroup.CalculateFlankingPosition] Calculated flank position at %1 (distance=%.1fm)", 
//            flankPos.ToString(), flankDistance), LogLevel.NORMAL);
        // --- END MODIFIED FLANKING LOGIC ---
        
        return flankPos;
    }

    // Make sure we still have EvaluateGroupState method
    void EvaluateGroupState()
    {
        m_isStateEvaluationScheduled = false;
        
        if (!IsSpawned() || !m_group)
            return;
        
        // Evaluate danger state
        EvaluateDangerState();
        
        // Evaluate tactical state
        EvaluateTacticalState();
        
        // Schedule next evaluation
        ScheduleNextStateEvaluation();
    }

    // Add GetFaction method that was missing
    IA_Faction GetFaction()
    {
        return m_faction;
    }
    
	SCR_AIGroup GetSCR_AIGroup()
	{
		return m_group;
	}
	
    void SetDefendMode(bool enable, vector defendPoint = vector.Zero)
    {
        m_isInDefendMode = enable;
        m_defendTarget = defendPoint;
        
        if (enable && defendPoint != vector.Zero)
        {
            Print(string.Format("[IA_AiGroup] Setting defend mode ON for group, target: %1", defendPoint.ToString()), LogLevel.NORMAL);
            
            // Force Search & Destroy order on defend point
            RemoveAllOrders(true);
            AddOrder(defendPoint, IA_AiOrder.SearchAndDestroy, true);
            
            // Set tactical state directly
            SetTacticalState(IA_GroupTacticalState.Attacking, defendPoint, null, true);
        }
        else
        {
            Print("[IA_AiGroup] Setting defend mode OFF for group", LogLevel.NORMAL);
        }
    }
    
    // Add getter for defend mode
    bool IsInDefendMode()
    {
        return m_isInDefendMode;
    }

    // Add a public setter for the assigned area
    void SetAssignedArea(IA_Area area)
    {
        string currentAreaStr;
        if (m_lastAssignedArea)
            currentAreaStr = m_lastAssignedArea.ToString();
        else
            currentAreaStr = "NULL";

        m_lastAssignedArea = area;
    }

    //------------------------------------------------------------------------------------------------
    bool IsStateManagedByAuthority()
    {
        return m_isStateManagedByAuthority;
    }
    
    //------------------------------------------------------------------------------------------------
    
    // Method to spawn the next unit in the staggered spawning process
    void SpawnNextUnit()
    {
        if (m_pendingUnitsToSpawn <= 0)
        {
            // All units spawned, finalize the group
            OnStaggeredSpawningComplete();
            return;
        }
        
        // Get the prefab path for this unit
        string charPrefabPath = GetRandomUnitPrefab(m_staggeredSpawnFaction, m_staggeredAreaFaction);
        if (charPrefabPath == "")
        {
            // Failed to get prefab, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        // Generate spawn position
        vector unitSpawnPos = m_staggeredSpawnPos;
        if (!m_HVTGroup)
        {
            unitSpawnPos = m_staggeredSpawnPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
        }
        Resource charRes = Resource.Load(charPrefabPath);
        if (!charRes)
        {
            // Failed to load resource, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
        if (!charEntity)
        {
            // Failed to spawn entity, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        // Add to group
        if (!m_group.AddAIEntityToGroup(charEntity))
        {
            IA_Game.AddEntityToGc(charEntity); // Schedule for deletion if adding failed
        }
        else
        {
            SetupDeathListenerForUnit(charEntity); // Setup death listener for the added unit
            m_unitsSpawnedCount++;
        }
        
        // Decrement pending count and schedule next spawn
        m_pendingUnitsToSpawn--;
        if (m_pendingUnitsToSpawn > 0)
        {
            GetGame().GetCallqueue().CallLater(SpawnNextUnit, 100, false);
        }
        else
        {
            OnStaggeredSpawningComplete();
        }
    }
    
    // Called when all units have been spawned (or attempted)
    void OnStaggeredSpawningComplete()
    {
        // Mark as spawned
        PerformSpawn();
        
        Print(string.Format("[IA_AiGroup.OnStaggeredSpawningComplete] Staggered spawning complete. Spawned %1 units for faction %2", 
            m_unitsSpawnedCount, m_staggeredSpawnFaction), LogLevel.NORMAL);
        
        // Special handling for hostile civilian groups (marked by having m_groupFaction set)
        if (m_groupFaction && m_groupFaction.GetFactionKey() == "USSR" && !m_referencedEntity)
        {
            // Ensure the underlying SCR_AIGroup has the correct faction set
            m_group.SetFaction(m_groupFaction);
            
            // Arm the hostile civilians
            ArmGroupWithLongGuns();
            
            // Set default state
            if (m_staggeredSpawnPos != vector.Zero)
            {
                SetTacticalState(IA_GroupTacticalState.DefendPatrol, m_staggeredSpawnPos, null, true);
            }
            else
            {
                vector patrolPos = IA_Game.rng.GenerateRandomPointInRadius(10, 50, GetOrigin());
                SetTacticalState(IA_GroupTacticalState.DefendPatrol, patrolPos, null, true);
            }
        }
        // Special handling for vehicle groups
        else if (m_referencedEntity)
        {
            // Order units to get in the vehicle immediately
            AddOrder(m_referencedEntity.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            
            // Initialize tactical state and schedule evaluation only if not in defend mode
            if (!IsInDefendMode() && !m_lastAssignedArea)
            {
                SetTacticalState(IA_GroupTacticalState.DefendPatrol, m_staggeredSpawnPos);
            }
            else
            {
                Print(string.Format("[IA_AiGroup.OnStaggeredSpawningComplete] Vehicle group has defend mode (%1) or assigned area (%2), skipping default state assignment", 
                    IsInDefendMode(), m_lastAssignedArea != null), LogLevel.NORMAL);
            }
            
            ScheduleNextStateEvaluation();
            SetupDeathListener(); // Ensure CheckDangerEvents is scheduled
        }
        else
        {
            // Non-vehicle groups - original logic
            // Only set default state if not in defend mode and no area assigned
            if (!IsInDefendMode() && !m_lastAssignedArea)
            {
                // Explicitly set a default tactical state for the new group
                if (m_staggeredSpawnPos != vector.Zero)
                {
                    SetTacticalState(IA_GroupTacticalState.DefendPatrol, m_staggeredSpawnPos, null, true); // Added authority flag
                }
                else
                {
                    vector patrolPos = IA_Game.rng.GenerateRandomPointInRadius(10, 50, GetOrigin());
                    SetTacticalState(IA_GroupTacticalState.DefendPatrol, patrolPos, null, true); // Added authority flag
                }
            }
            else
            {
                Print(string.Format("[IA_AiGroup.OnStaggeredSpawningComplete] Group has defend mode (%1) or assigned area (%2), skipping default state assignment", 
                    IsInDefendMode(), m_lastAssignedArea != null), LogLevel.NORMAL);
            }
        }
        
        // Clear staggered spawning state
        m_staggeredSpawnPos = vector.Zero;
        m_staggeredSpawnFaction = IA_Faction.NONE;
        m_staggeredAreaFaction = null;
    }

    //------------------------------------------------------------------------------------------------
    
    // Method to spawn the next hostile civilian unit in the staggered spawning process
    void SpawnNextHostileCivilianUnit()
    {
        if (m_pendingUnitsToSpawn <= 0)
        {
            // All units spawned, finalize the group
            OnStaggeredSpawningComplete();
            return;
        }
        
        // Get civilian prefab path
        string charPrefabPath = IA_RandomCivilianResourceName();
        if (charPrefabPath == "")
        {
            // Failed to get prefab, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextHostileCivilianUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        // Generate spawn position
        vector unitSpawnPos = m_staggeredSpawnPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
        Resource charRes = Resource.Load(charPrefabPath);
        if (!charRes)
        {
            // Failed to load resource, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextHostileCivilianUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
        if (!charEntity)
        {
            // Failed to spawn entity, try next unit
            m_pendingUnitsToSpawn--;
            if (m_pendingUnitsToSpawn > 0)
            {
                GetGame().GetCallqueue().CallLater(SpawnNextHostileCivilianUnit, 100, false);
            }
            else
            {
                OnStaggeredSpawningComplete();
            }
            return;
        }
        
        // Add to group
        if (!m_group.AddAIEntityToGroup(charEntity))
        {
            IA_Game.AddEntityToGc(charEntity); // Schedule for deletion if adding failed
        }
        else
        {
            SetupDeathListenerForUnit(charEntity); // Setup death listener for the added unit
            m_unitsSpawnedCount++;
        }
        
        // Decrement pending count and schedule next spawn
        m_pendingUnitsToSpawn--;
        if (m_pendingUnitsToSpawn > 0)
        {
            GetGame().GetCallqueue().CallLater(SpawnNextHostileCivilianUnit, 100, false);
        }
        else
        {
            OnStaggeredSpawningComplete();
        }
    }

    //------------------------------------------------------------------------------------------------

    // Add this new method before SetTacticalState method
    void RequestTacticalStateChange(IA_GroupTacticalState newState, vector targetPos = vector.Zero, IEntity targetEntity = null)
    {
        // Don't create redundant requests
        if (m_hasPendingStateRequest && m_requestedState == newState)
            return;
            
        // Log the request
        //Print(string.Format("[IA_AiGroup.RequestTacticalStateChange] Group %1 requesting state change from %2 to %3", 
//            this, typename.EnumToString(IA_GroupTacticalState, m_tacticalState), 
//            typename.EnumToString(IA_GroupTacticalState, newState)), LogLevel.NORMAL);
            
        // Set pending request data
        m_hasPendingStateRequest = true;
        m_requestedState = newState;
        m_requestedStatePosition = targetPos;
        m_requestedStateEntity = targetEntity;
    }

    // Add these getter methods
    bool HasPendingStateRequest()
    {
        return m_hasPendingStateRequest;
    }

    IA_GroupTacticalState GetRequestedState()
    {
        return m_requestedState;
    }

    vector GetRequestedStatePosition()
    {
        return m_requestedStatePosition;
    }

    IEntity GetRequestedStateEntity()
    {
        return m_requestedStateEntity;
    }

    // Clear pending state request (called when request is processed)
    void ClearStateRequest()
    {
        m_hasPendingStateRequest = false;
    }

    // Method to get current danger level
    float GetCurrentDangerLevel()
    {
        return m_currentDangerLevel;
    }
    
    // Alias for GetCurrentDangerLevel to match call in AreaInstance
    float GetDangerLevel()
    {
        return m_currentDangerLevel;
    }
    
    // Return the Unix timestamp when the tactical state was last changed
    int GetLastStateChangeTime()
    {
        return m_tacticalStateStartTime;
    }
    
    // --- BEGIN ADDED: Getter for Last Danger Event Time ---
    int GetLastDangerEventTime()
    {
        return m_lastDangerEventTime;
    }
    // --- END ADDED ---

    // NEW PRIVATE HELPER METHOD
    private void TryFindAndSetAssignedArea()
    {
        if (m_lastAssignedArea) // Already have an area, no need to search
        {
            // Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 already has m_lastAssignedArea: %2. Skipping search.", this, m_lastAssignedArea.ToString()), LogLevel.NORMAL);
            return;
        }

        Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 at %2 attempting to find its area (m_lastAssignedArea is currently NULL).", 
            this, m_initialPosition.ToString()), LogLevel.NORMAL);

        IA_AreaMarker foundMarker = null;
        array<IA_AreaMarker> allMarkers = IA_AreaMarker.GetAllMarkers();
        int markersSearchedCount = 0;
        
        if (allMarkers)
            markersSearchedCount = allMarkers.Count();
        else 
            markersSearchedCount = -1; // Indicate null array if that happens

        if (allMarkers && !allMarkers.IsEmpty())
        {
            Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1: Found %2 total area markers to check.", this, markersSearchedCount), LogLevel.NORMAL);
            foreach (IA_AreaMarker marker : allMarkers)
            {
                if (marker && marker.IsPositionInside(m_initialPosition))
                {
                    foundMarker = marker;
                    Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 at %2 found to be INSIDE marker %3 (Center: %4, Radius: %5)",
                        this, m_initialPosition.ToString(), marker.ToString(), marker.GetOrigin().ToString(), marker.GetRadius()), LogLevel.NORMAL);
                    break;
                }
            }
        }
        else
        {
             Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1: IA_AreaMarker.GetAllMarkers() returned null or empty array. Count: %2", this, markersSearchedCount), LogLevel.WARNING);
        }

        if (foundMarker)
        {
            IA_Area areaToAssign = IA_Area.Cast(foundMarker.FindComponent(IA_Area));
            if (areaToAssign)
            {
                SetAssignedArea(areaToAssign); // This will use the updated SetAssignedArea with logging
                // Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 SUCCESS: Found marker %2 and IA_Area component %3. Area set.", this, foundMarker.ToString(), areaToAssign.ToString()), LogLevel.NORMAL);
            }
            else
            {
                Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 FAILURE: Found marker %2, but FAILED TO FIND/CAST IA_Area component on it.",
                    this, foundMarker.ToString()), LogLevel.WARNING);
            }
        }
        else
        {
            Print(string.Format("[IA_AiGroup.TryFindAndSetAssignedArea] Group %1 FAILURE: Could NOT find an area marker that its initial position %2 is inside. (Searched %3 markers)",
                this, m_initialPosition.ToString(), markersSearchedCount), LogLevel.WARNING);
        }
    }
    // END NEW PRIVATE HELPER METHOD
    //------------------------------------------------------------------------------------------------

    // Removed OnUnitAdded, OnAllVehicleGroupMembersSpawned, and OnAllHostileCiviliansSpawned callbacks
    // as they are no longer needed with direct spawning

    void ArmGroupWithPistols()
    {
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        foreach(SCR_ChimeraCharacter character : characters)
        {
            if (character && !character.GetDamageManager().IsDestroyed())
            {
                CharacterControllerComponent charController = character.GetCharacterController();
                if (!charController)
                    continue;
    
                SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(charController.GetInventoryStorageManager());
                if (!inventoryManager)
                {
                    Print(string.Format("[IA_AiGroup] Character %1 has no SCR_InventoryStorageManagerComponent.", character), LogLevel.WARNING);
                    continue;
                }
                
                EntitySpawnParams spawnParams = new EntitySpawnParams();
                spawnParams.TransformMode = ETransformMode.WORLD;
                character.GetTransform(spawnParams.Transform);
                
                Resource weaponRes = Resource.Load("{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et");
                IEntity weaponEntity = GetGame().SpawnEntityPrefab(weaponRes, GetGame().GetWorld(), spawnParams);
    
                if (!weaponEntity)
                {
                    Print("[IA_AiGroup] Failed to spawn weapon for arming.", LogLevel.ERROR);
                    continue;
                }
                inventoryManager.EquipWeapon(weaponEntity, null, true);
    
                Resource magazineRes = Resource.Load("{9C05543A503DB80E}Prefabs/Weapons/Magazines/Magazine_9x19_M9_15rnd_Ball.et");
                for (int i = 0; i < 2; i++)
                {
                    IEntity magazineEntity = GetGame().SpawnEntityPrefab(magazineRes, GetGame().GetWorld(), spawnParams);
                    if (magazineEntity && !inventoryManager.TryInsertItem(magazineEntity))
                    {
                        Print(string.Format("[IA_AiGroup] Failed to add magazine to character %1 inventory.", character), LogLevel.WARNING);
                        IA_Game.AddEntityToGc(magazineEntity);
                    }
                }
            }
        }
    }

    void ArmGroupWithLongGuns()
    {
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        foreach(SCR_ChimeraCharacter character : characters)
        {
            if (character && !character.GetDamageManager().IsDestroyed())
            {
                CharacterControllerComponent charController = character.GetCharacterController();
                if (!charController)
                    continue;
    
                SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(charController.GetInventoryStorageManager());
                if (!inventoryManager)
                {
                    Print(string.Format("[IA_AiGroup] Character %1 has no SCR_InventoryStorageManagerComponent.", character), LogLevel.WARNING);
                    continue;
                }
                
                EntitySpawnParams spawnParams = new EntitySpawnParams();
                spawnParams.TransformMode = ETransformMode.WORLD;
                character.GetTransform(spawnParams.Transform);
                
                Resource weaponRes = Resource.Load("{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et");
                IEntity weaponEntity = GetGame().SpawnEntityPrefab(weaponRes, GetGame().GetWorld(), spawnParams);
    
                if (!weaponEntity)
                {
                    Print("[IA_AiGroup] Failed to spawn weapon for arming.", LogLevel.ERROR);
                    continue;
                }
                inventoryManager.EquipWeapon(weaponEntity, null, true);
    
                Resource magazineRes = Resource.Load("{0A84AA5A3884176F}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Last_5Tracer.et");
                for (int i = 0; i < 3; i++)
                {
                    IEntity magazineEntity = GetGame().SpawnEntityPrefab(magazineRes, GetGame().GetWorld(), spawnParams);
                    if (magazineEntity && !inventoryManager.TryInsertItem(magazineEntity))
                    {
                        Print(string.Format("[IA_AiGroup] Failed to add magazine to character %1 inventory.", character), LogLevel.WARNING);
                        IA_Game.AddEntityToGc(magazineEntity);
                    }
                }
            }
        }
    }
    
    static IA_AiGroup CreateHostileCivilianGroup(vector spawnPos, int unitCount, Faction AreaFaction)
    {
        if (unitCount <= 0)
            return null;
    
        // Use USSR faction for the group, but civilian character models
        IA_AiGroup grp = new IA_AiGroup(spawnPos, IA_SquadType.Riflemen, IA_Faction.USSR, unitCount);
        grp.m_isCivilian = false; // Treat them as combatants
    
        Resource groupRes = Resource.Load("{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et"); // USSR Group Prefab
    
        if (!groupRes) {
            Print("[IA_AiGroup.CreateHostileCivilianGroup] Failed to load group resource for USSR.", LogLevel.ERROR);
            return null;
        }
    
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(spawnPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);
    
        if (!grp.m_group) {
            delete groupEnt;
            Print(string.Format("[IA_AiGroup.CreateHostileCivilianGroup] Failed to create SCR_AIGroup for USSR at %1", spawnPos.ToString()), LogLevel.ERROR);
            return null;
        }
    
        vector groundPos = spawnPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        grp.m_group.SetOrigin(groundPos);
    
        // KEY DIFFERENCE: Use civilian prefabs
        // Set up staggered spawning state
        grp.m_pendingUnitsToSpawn = unitCount;
        grp.m_unitsSpawnedCount = 0;
        grp.m_staggeredSpawnPos = groundPos;
        grp.m_staggeredSpawnFaction = IA_Faction.USSR; // Always USSR for hostile civilians
        grp.m_staggeredAreaFaction = AreaFaction;
        
        // Mark that this group needs arming after spawning
        // We'll use a special marker to identify hostile civilian groups
        grp.m_groupFaction = GetGame().GetFactionManager().GetFactionByKey("USSR");
        
        Print(string.Format("[IA_AiGroup.CreateHostileCivilianGroup] Starting staggered spawning of %1 hostile civilians at %2", unitCount, spawnPos.ToString()), LogLevel.NORMAL);
        
        // Start spawning the first unit immediately
        grp.SpawnNextHostileCivilianUnit();
    
        return grp;
    }
    
    void SetOwningSideObjective(IA_SideObjective objective)
    {
        m_OwningSideObjective = objective;
    }
    
    IA_SideObjective GetOwningSideObjective()
    {
        return m_OwningSideObjective;
    }
    
    bool IsObjectiveUnit()
    {
        return m_OwningSideObjective != null;
    }

	void SetOwningAreaInstance(IA_AreaInstance owner)
	{
		m_owningAreaInstance = owner;
	}

};  
