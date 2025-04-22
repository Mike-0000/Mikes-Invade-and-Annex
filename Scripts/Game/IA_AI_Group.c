// Add these enums at the top of the file, before the IA_AiGroup class
enum IA_GroupDangerType
{
    ProjectileImpact,
    WeaponFire,
    ProjectileFlyby,
    Explosion,
    EnemySpotted
}

///////////////////////////////////////////////////////////////////////
// 3) AI GROUP
///////////////////////////////////////////////////////////////////////
class IA_AiGroup
{
    private SCR_AIGroup m_group = null;
    private bool        m_isSpawned = false;
    private vector      m_initialPosition;

    private IA_SquadType m_squadType;
    private IA_Faction   m_faction;
    private bool         m_isCivilian;

    private vector m_lastOrderPosition;
    private int    m_lastOrderTime;

    private bool   m_isDriving = false;
    private vector m_drivingTarget = vector.Zero;
    private IEntity m_referencedEntity;

    private IA_Faction m_engagedEnemyFaction = IA_Faction.NONE;

    // Add reaction manager
    private ref IA_AIReactionManager m_reactionManager = null;

    // Add a member variable to track last vehicle order update time
    private int m_lastVehicleOrderTime = 0;
    
    // Set minimum time between vehicle order updates (in seconds)
    private static const int VEHICLE_ORDER_UPDATE_INTERVAL = 3;

    // Add these variables to the class variables section near the top
    private int m_nextStateEvaluationTime = 0;
    private const int MIN_STATE_EVALUATION_INTERVAL = 5000; // milliseconds
    private const int MAX_STATE_EVALUATION_INTERVAL = 15000; // milliseconds
    private bool m_isStateEvaluationScheduled = false;

    // Danger reaction system parameters
    private const float BULLET_IMPACT_DISTANCE_MAX = 10.0;
    private const float PROJECTILE_FLYBY_RADIUS = 13.0;
    private const float GUNSHOT_AUDIBLE_DISTANCE = 500.0;
    private const float SUPPRESSED_AUDIBLE_DISTANCE = 100.0;
    private const float EXPLOSION_REACT_DISTANCE = 220.0;
    
    // Danger event tracking
    private int m_lastDangerEventTime = 0;
    private vector m_lastDangerPosition = vector.Zero;
    private IA_GroupDangerType m_lastDangerType = IA_GroupDangerType.EnemySpotted;
    private float m_currentDangerLevel = 0.0; // 0.0-1.0 scale
    private int m_consecutiveDangerEvents = 0;

    // Rate limiting for logging
    private static const int BEHAVIOR_LOG_RATE_LIMIT_SECONDS = 3;
    private int m_lastApplyUnderFireLogTime = 0;
    private int m_lastApplyEnemySpottedLogTime = 0;
    private int m_lastDangerDetectLogTime = 0;
    private int m_lastDangerTriggerLogTime = 0;

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac)
    {
        //Initialize basic properties
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
        
        // Initialize danger system
        m_lastDangerEventTime = 0;
        m_lastDangerDetectLogTime = 0;
        m_lastDangerTriggerLogTime = 0;
        m_currentDangerLevel = 0.0;
        m_consecutiveDangerEvents = 0;
        
        // Create reaction manager
        m_reactionManager = new IA_AIReactionManager();
        
        // --- BEGIN ADDED DEBUG LOG ---
        string managerStatus;
        if (m_reactionManager)
            managerStatus = "CREATED";
        else
            managerStatus = "NULL";
            
        Print("[IA_AI_INIT_DEBUG] Group created - Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              " | SquadType: " + typename.EnumToString(IA_SquadType, m_squadType) + 
              " | m_reactionManager: " + managerStatus, LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
	    // --- BEGIN ADDED DEBUG LOG ---
        Print("[IA_AI_CRITICAL] CreateMilitaryGroup START - Faction: " + typename.EnumToString(IA_Faction, faction) + 
              " | SquadType: " + typename.EnumToString(IA_SquadType, squadType),
              LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---
	    IA_AiGroup grp = new IA_AiGroup(initialPos,squadType,faction);
	    
	    // Add critical log after object creation
	    Print("[IA_AI_CRITICAL] Military group object created, isCivilian=" + grp.m_isCivilian, LogLevel.WARNING);
	    
	    grp.m_lastOrderPosition = initialPos;
	    grp.m_initialPosition   = initialPos;
	    grp.m_lastOrderTime     = 0;
	    grp.m_squadType         = squadType;
	    grp.m_faction           = faction;
	    grp.m_isCivilian        = false;
	    
	    // Add critical log just before returning
	    Print("[IA_AI_CRITICAL] CreateMilitaryGroup returning group with faction=" + 
	          typename.EnumToString(IA_Faction, grp.m_faction) + ", isCivilian=" + grp.m_isCivilian, LogLevel.WARNING);
	    return grp;
	}

	static string GetRandomUnitPrefab(IA_Faction faction){

         SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to get faction manager!", LogLevel.ERROR);
            return "";
        }

        // Get the actual faction for catalog query
        Faction actualFaction;
        if (faction == IA_Faction.US)
            actualFaction = factionManager.GetFactionByKey("US");
        else if (faction == IA_Faction.USSR)
            actualFaction = factionManager.GetFactionByKey("USSR");
        else {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Unsupported faction for vehicle crew: " + faction, LogLevel.ERROR);
            return "";
        }

        SCR_Faction scrFaction = SCR_Faction.Cast(actualFaction);
        if (!scrFaction) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to cast to SCR_Faction", LogLevel.ERROR);
            return "";
        }

        // Get the entity catalog from the faction
        SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
        if (!entityCatalog) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to get character catalog", LogLevel.ERROR);
            return "";
        }

        // Query the catalog for character prefabs of this faction
        array<SCR_EntityCatalogEntry> characterEntries = {};
		array<EEditableEntityLabel> includedLabels = {};
		int randInt = Math.RandomInt(0,10);
		
		if(faction == IA_Faction.USSR){
			switch(randInt){
				case 0: // ROLE_ANTITANK
					includedLabels = {EEditableEntityLabel.ROLE_ANTITANK};
					break;
				case 1:
					includedLabels = {EEditableEntityLabel.ROLE_GRENADIER};
					break;
				case 2:
					includedLabels = {EEditableEntityLabel.ROLE_LEADER};
					break;
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
			}
		}
		// --- BEGIN NEW US FACTION LOGIC ---
		else if (faction == IA_Faction.US)
		{
		    // Similar role distribution for US faction
		    switch(randInt)
		    {
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
		// --- END NEW US FACTION LOGIC ---
		// --- BEGIN FIA FACTION LOGIC (Optional: Defaulting to Rifleman for now) ---
		else if (faction == IA_Faction.FIA)
		{
		    // FIA might have a different structure or subset of roles
		    // For now, let's default to Rifleman, or you can specify FIA roles
		    includedLabels = {EEditableEntityLabel.ROLE_RIFLEMAN};
		    // Example: Add more FIA specific roles if needed
		    // switch(randInt) { ... FIA specific logic ... }
		}
		// --- END FIA FACTION LOGIC ---

		array<EEditableEntityLabel> excludedLabels = {};
        entityCatalog.GetFullFilteredEntityList(characterEntries, includedLabels, excludedLabels);
        if (characterEntries.IsEmpty()) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] No character entries found for faction!", LogLevel.ERROR);
            return "";
        }


        return characterEntries[Math.RandomInt(1, characterEntries.Count())].GetPrefab();



    }

	bool HasActiveWaypoint()
	{
		if (!m_group)
		{
			Print("[VEHICLE_DEBUG] HasActiveWaypoint - group is null", LogLevel.WARNING);
			return false;
		}
		
		array<AIWaypoint> wps = {};
		m_group.GetWaypoints(wps);
		
		bool hasWaypoints = !wps.IsEmpty();
		if (m_isDriving && !hasWaypoints)
		{
			Print("[VEHICLE_DEBUG] HasActiveWaypoint - Vehicle group has no waypoints!", LogLevel.WARNING);
		}
		
		return hasWaypoints;
	}

	
    static IA_AiGroup CreateCivilianGroup(vector initialPos)
    {
        //Print("[DEBUG] CreateCivilianGroup called with pos: " + initialPos, LogLevel.NORMAL);
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV);
        grp.m_isCivilian = true;
        // Civilian groups are simpler, let PerformSpawn handle their single unit spawn
        return grp;
    }

    // New factory method for spawning a specific number of units for a vehicle crew
    static IA_AiGroup CreateGroupForVehicle(Vehicle vehicle, IA_Faction faction, int unitCount)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // Spawn slightly above vehicle origin

        // Create the IA_AiGroup wrapper first
        IA_AiGroup grp = new IA_AiGroup(spawnPos, IA_SquadType.Riflemen, faction);
        grp.m_isCivilian = false;

        // Create an empty SCR_AIGroup
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
            //Print("[IA_AiGroup.CreateGroupForVehicle] Could not load base group resource!", LogLevel.ERROR);
            return null;
        }
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(spawnPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Could not cast spawned entity to SCR_AIGroup!", LogLevel.ERROR);
            delete groupEnt;
            return null;
        }
        
        // Adjust group position
        vector groundPos = spawnPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        grp.m_group.SetOrigin(groundPos);

        // Get the actual compartments to check which ones are available
        BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
        if (!compartmentManager) {
            //Print("[IA_AiGroup.CreateGroupForVehicle] Vehicle has no compartment manager!", LogLevel.ERROR);
            return null;
        }
        
        array<BaseCompartmentSlot> compartments = {};
        compartmentManager.GetCompartments(compartments);
        
        // Filter out compartments that are not suitable for AI occupation
        array<BaseCompartmentSlot> usableCompartments = {};
        
        // Collect valid compartment types for AI
        foreach (BaseCompartmentSlot slot : compartments) {
            // Skip already occupied compartments
            if (slot.GetOccupant())
                continue;
                
            // Check compartment type - focus on driver, co-driver, and passenger compartments
            ECompartmentType type = slot.GetType();
            
            // Filter based on compartment type - only include compartments AI can use
            if (type == ECompartmentType.PILOT ||  
                type == ECompartmentType.CARGO || 
                type == ECompartmentType.TURRET) {
                
                // Additional check - make sure the compartment is accessible
                if (slot.IsCompartmentAccessible())
                    usableCompartments.Insert(slot);
            }
        }
        
        // Log the actual compartment counts for debugging
       //Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Vehicle has " + compartments.Count() + 
//              " total compartments, " + usableCompartments.Count() + " usable compartments", LogLevel.NORMAL);
        
        // Use the smaller of unitCount or usableCompartments to avoid spawning too many units
        int actualUnitsToSpawn = Math.Min(unitCount, usableCompartments.Count());
//       //Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Spawning " + actualUnitsToSpawn + 
 //             " individual units for faction " + faction + " (requested: " + unitCount + 
 //             ", usable compartments: " + usableCompartments.Count() + ")", LogLevel.NORMAL);

        // Spawn individual units and add them to group
        for (int i = 0; i < actualUnitsToSpawn; i++)
        {
			string charPrefabPath = GetRandomUnitPrefab(faction);
			
 //       	Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Using character prefab: " + charPrefabPath, LogLevel.NORMAL);

            // Spawn the character slightly offset from group position
            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to load character resource!", LogLevel.ERROR);
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to spawn character entity!", LogLevel.ERROR);
                continue;
            }

            // Add character to the SCR_AIGroup
            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                //Print("[IA_AiGroup.CreateGroupForVehicle] Failed to add character " + i + " to group.", LogLevel.ERROR);
                IA_Game.AddEntityToGc(charEntity);
            } else {
                //Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Added unit " + i + " to group.", LogLevel.NORMAL);
                grp.SetupDeathListenerForUnit(charEntity);
            }
        }

        // Mark the group as spawned since we manually added units
        grp.m_isSpawned = true;

        // Perform water check for the group's final position
        grp.WaterCheck();

        //Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Finished creating group with " + grp.GetAliveCount() + " units.", LogLevel.NORMAL);
        return grp;
    }

    // --- New factory method for spawning military groups from individual units ---
    static IA_AiGroup CreateMilitaryGroupFromUnits(vector initialPos, IA_Faction faction, int unitCount)
    {
        Print("[IA_AI_CRITICAL] CreateMilitaryGroupFromUnits START - Faction: " + typename.EnumToString(IA_Faction, faction) + 
              ", unitCount: " + unitCount, LogLevel.WARNING);
              
        if (unitCount <= 0)
            return null;

        // Create the IA_AiGroup wrapper first
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, faction); // SquadType might be less relevant here
        grp.m_isCivilian = false;

        // Create an empty SCR_AIGroup based on faction
        Resource groupRes;
        switch(faction){
            case IA_Faction.USSR:
                groupRes = Resource.Load("{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et");
                break;
            case IA_Faction.US:
                groupRes = Resource.Load("{EACD97CF4A702FAE}Prefabs/Groups/BLUFOR/Group_US_Base.et");
                break;
            case IA_Faction.FIA:
                groupRes = Resource.Load("{242BC3C6BCE96EA5}Prefabs/Groups/INDFOR/Group_FIA_Base.et");
                break;
            default: // Handle CIV or other cases if needed, though this is for military
                //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Unsupported faction for military group: " + faction, LogLevel.ERROR);
                return null;
        }

        if (!groupRes) {
            //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Could not load base group resource!", LogLevel.ERROR);
            return null;
        }

        // Spawn the base group entity and adjust its position to ground level
        vector groundPos = initialPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(groundPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Could not cast spawned entity to SCR_AIGroup!", LogLevel.ERROR);
            delete groupEnt; // Clean up the spawned group entity
            return null;
        }

        // Set the origin again after casting just in case
        grp.m_group.SetOrigin(groundPos);

        Print("[IA_AI_CRITICAL] CreateMilitaryGroupFromUnits: Spawning " + unitCount + " individual units for faction " + 
              typename.EnumToString(IA_Faction, faction), LogLevel.WARNING);

        // Spawn individual units and add them to group
        for (int i = 0; i < unitCount; i++)
        {
            string charPrefabPath = GetRandomUnitPrefab(faction);
            if (charPrefabPath.IsEmpty())
            {
                 //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to get random unit prefab for faction " + faction + ", skipping unit " + i, LogLevel.WARNING);
                 continue;
            }

            //Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Using character prefab: " + charPrefabPath, LogLevel.NORMAL);

            // Spawn the character slightly offset from group position
            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to load character resource: " + charPrefabPath, LogLevel.ERROR);
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to spawn character entity!", LogLevel.ERROR);
                continue;
            }

            // Add character to the SCR_AIGroup
            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                //Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to add character " + i + " to group.", LogLevel.ERROR);
                IA_Game.AddEntityToGc(charEntity); // Schedule for deletion if adding failed
            } else {
                //Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Added unit " + i + " to group.", LogLevel.NORMAL);
                grp.SetupDeathListenerForUnit(charEntity); // Setup death listener for the added unit
            }
        }

        // IMPORTANT FIX: Instead of manually setting m_isSpawned, call the proper Spawn() method 
        // to set up all necessary systems, including danger event handling
        Print("[IA_AI_CRITICAL] CreateMilitaryGroupFromUnits: Calling Spawn() to set up event handlers", LogLevel.WARNING);
        grp.Spawn(IA_AiOrder.Patrol, initialPos);
        
        // Perform water check for the group's final position
        // Note: WaterCheck() is already called inside Spawn(), so this is redundant
        // grp.WaterCheck();

        Print("[IA_AI_CRITICAL] CreateMilitaryGroupFromUnits: Finished creating group with " + 
              grp.GetAliveCount() + " units. isSpawned=" + grp.IsSpawned(), LogLevel.WARNING);
        return grp;
    }

    IA_Faction GetEngagedEnemyFaction()
    {
		if(m_isCivilian)
			return IA_Faction.NONE;
        // Keep the original function implementation and just add this logging before the return
        Print("[IA_NEW_DEBUG] GetEngagedEnemyFaction called. Group faction: " + m_faction, LogLevel.WARNING);
		if(IsEngagedWithEnemy()){
			return m_engagedEnemyFaction;
		}
        return IA_Faction.NONE; // Or whatever your original return value is
    }

    bool IsEngagedWithEnemy()
    {
		if(m_isCivilian)
			return false;
        // First, check if we have a valid enemy faction set
        bool result = m_engagedEnemyFaction != IA_Faction.NONE;
        
        // Military units (non-civilian) should engage when they detect danger
        if (!result && !m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            // If we've had a danger event in the last 60 seconds, consider the unit engaged
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
        
        Print("[IA_NEW_DEBUG] IsEngagedWithEnemy called. Group faction: " + m_faction + ", Result: " + result, LogLevel.WARNING);
        return result;
    }

    void MarkAsUnengaged()
    {
        //Print("[DEBUG] MarkAsUnengaged called. Resetting engaged enemy faction.", LogLevel.NORMAL);
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
            //Print("[IA_AiGroup.AddWaypoint] Group or waypoint is null.", LogLevel.WARNING);
            return;
        }
        m_group.AddWaypoint(waypoint);
        //Print("[DEBUG] IA_AiGroup.AddWaypoint: Waypoint added to internal SCR_AIGroup.", LogLevel.NORMAL);
    }

    void AddOrder(vector origin, IA_AiOrder order, bool topPriority = false)
    {
        // Store last order data
        m_lastOrderPosition = origin;
        m_lastOrderTime = System.GetUnixTime();
        
        // If this is a vehicle group, update vehicle orders first
        if (m_isDriving)
        {
            Print("[VEHICLE_DEBUG] AddOrder - Passing to UpdateVehicleOrders for vehicle group. Order: " + order + ", Position: " + origin, LogLevel.NORMAL);
            UpdateVehicleOrders();
            return; // Let the vehicle order system handle it
        }
        
        //Print("[DEBUG] AddOrder called with origin: " + origin + ", order: " + order + ", topPriority: " + topPriority, LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            //Print("[IA_AiGroup.AddOrder] Group not spawned!", LogLevel.WARNING);
            return;
        }
        if (!m_group)
        {
            //Print("[DEBUG] m_group is null in AddOrder.", LogLevel.NORMAL);
            return;
        }

        // Restrict infantry waypoints to a single zone, while vehicle waypoints can navigate across the zone group
        if (!m_isDriving && order != IA_AiOrder.GetInVehicle)
        {
            // Get the current group number from VehicleManager's active group
            int currentGroupNumber = IA_VehicleManager.GetActiveGroup();
            
            // Find the closest zone to the current group position
            IA_AreaMarker closestMarker = IA_AreaMarker.GetMarkerAtPosition(m_group.GetOrigin());
            
            if (closestMarker)
            {
                // Get the zone radius to constrain waypoints
                float zoneRadius = closestMarker.GetRadius();
                vector zoneOrigin = closestMarker.GetZoneCenter();
                
                // Calculate distance from requested waypoint to zone center
                float distToZoneCenter = vector.Distance(origin, zoneOrigin);
                
                // If the requested waypoint is outside the zone, constrain it
                if (distToZoneCenter > zoneRadius * 0.8) // Use 80% of radius to keep units within the zone's boundaries
                {
                    //Print("[DEBUG_INFANTRY_WAYPOINT] Original waypoint at " + origin + " is outside zone radius (" + zoneRadius + ")", LogLevel.NORMAL);
                    
                    // Generate a random point within the zone instead
                    origin = IA_Game.rng.GenerateRandomPointInRadius(1, zoneRadius * 0.8, zoneOrigin);
                    
                    //Print("[DEBUG_INFANTRY_WAYPOINT] Constrained waypoint to zone: " + origin, LogLevel.NORMAL);
                }
            }
            else
            {
                //Print("[DEBUG_INFANTRY_WAYPOINT] No marker found near group position " + m_group.GetOrigin() + ", using original waypoint", LogLevel.WARNING);
            }
        }

        // Adjust spawn height based on terrain
        float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
        //Print("[DEBUG] Surface Y calculated: " + y, LogLevel.NORMAL);
        origin[1] = y + 0.5;

        // For vehicle orders, snap to nearest road - EXCEPT GetInVehicle orders which should go directly to the vehicle
        if (m_isDriving && order != IA_AiOrder.GetInVehicle)
        {
            //Print("[DEBUG_VEHICLE_WAYPOINT] Finding nearest road point for vehicle order at: " + origin, LogLevel.NORMAL);
            // Get the current active group from the IA_VehicleManager instead of using group ID
            int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
            vector roadOrigin = IA_VehicleManager.FindRandomRoadEntityInZone(origin, 300, currentActiveGroup);
            if (roadOrigin != vector.Zero)
            {
                //Print("[DEBUG_VEHICLE_WAYPOINT] Road point found at: " + roadOrigin + ", distance from original: " + vector.Distance(origin, roadOrigin), LogLevel.NORMAL);
                origin = roadOrigin;
                //Print("[DEBUG] Vehicle order snapped to road at: " + origin, LogLevel.NORMAL);
            }
            else
            {
                //Print("[DEBUG_VEHICLE_WAYPOINT] No road found near vehicle order, using original position", LogLevel.WARNING);
                //Print("[DEBUG] No road found near vehicle order, using original position", LogLevel.WARNING);
            }
        }

        ResourceName rname = IA_AiOrderResource(order);
        Resource res = Resource.Load(rname);
        if (!res)
        {
            //Print("[IA_AiGroup.AddOrder] Missing resource: " + rname, LogLevel.ERROR);
            return;
        }
        //Print("[DEBUG] Resource loaded successfully: " + rname, LogLevel.NORMAL);

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        //Print("[DEBUG] Spawned waypoint entity.", LogLevel.NORMAL);
        SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
        if (!w)
        {
            //Print("[IA_AiGroup.AddOrder] Could not cast to AIWaypoint: " + rname, LogLevel.ERROR);
            return;
        }
        //Print("[DEBUG] Successfully cast entity to SCR_AIWaypoint.", LogLevel.NORMAL);

        if (m_isDriving || topPriority)
        {
            w.SetPriorityLevel(2000);
            //Print("[DEBUG_VEHICLE_WAYPOINT] Set waypoint priority level to 2000 for vehicle order", LogLevel.NORMAL);
            //Print("[DEBUG] Set waypoint priority level to 2000.", LogLevel.NORMAL);
        }

        m_group.AddWaypoint(w);
        //Print("[DEBUG] Waypoint added to group.", LogLevel.NORMAL);
        //Print("[DEBUG] Order timestamp updated to " + m_lastOrderTime, LogLevel.NORMAL);
    }

    void SetInitialPosition(vector pos)
    {
        //Print("[DEBUG] SetInitialPosition called with pos: " + pos, LogLevel.NORMAL);
        m_initialPosition = pos;
    }

    int TimeSinceLastOrder()
    {
        int delta = System.GetUnixTime() - m_lastOrderTime;
        //Print("[DEBUG] TimeSinceLastOrder called. Delta: " + delta, LogLevel.NORMAL);
        return delta;
    }

    bool HasOrders()
    {
        if (!m_group)
        {
            //Print("[DEBUG] HasOrders called but m_group is null.", LogLevel.NORMAL);
            return false;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        //Print("[DEBUG] HasOrders found " + wps.Count() + " waypoints.", LogLevel.NORMAL);
        return !wps.IsEmpty();
    }

    bool IsDriving()
    {
        //Print("[DEBUG_VEHICLE_LOGIC] IsDriving() called, returning: " + m_isDriving + ", referenced entity: " + m_referencedEntity, LogLevel.NORMAL);
        return m_isDriving;
    }

    void RemoveAllOrders(bool resetLastOrderTime = false)
    {
        if (!m_group)
            return;
        
        Print("[VEHICLE_DEBUG] RemoveAllOrders called, resetTime: " + resetLastOrderTime + ", isDriving: " + m_isDriving + ", entity: " + m_referencedEntity, LogLevel.NORMAL);
        
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        
        if (!wps.IsEmpty())
        {
            Print("[VEHICLE_DEBUG] RemoveAllOrders - Removing " + wps.Count() + " waypoints", LogLevel.NORMAL);
        }
        
        foreach (AIWaypoint w : wps)
        {
            m_group.RemoveWaypoint(w);
        }
        
        // Only reset driving state if explicitly requested through resetLastOrderTime
        if (resetLastOrderTime)
        {
            m_lastOrderTime = 0;
            
            // If this is a vehicle group, be very cautious about clearing the driving state
            if (m_isDriving && m_referencedEntity)
            {
                // Verify this is really a vehicle before clearing
                Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
                if (vehicle)
                {
                    Print("[VEHICLE_DEBUG] RemoveAllOrders - Resetting driving state for vehicle group as requested", LogLevel.WARNING);
                    // This is a valid vehicle group and resetLastOrderTime was explicitly requested
                    // Use ClearVehicleReference to properly clean up
                    ClearVehicleReference();
                }
                else
                {
                    Print("[VEHICLE_DEBUG] RemoveAllOrders - Found inconsistency: IsDriving=true but invalid vehicle reference", LogLevel.ERROR);
                    // If referenced entity is not a vehicle, clear the state
                    m_isDriving = false;
                    m_referencedEntity = null;
                }
            }
        }
    }

    int GetAliveCount()
    {
        //Print("[DEBUG] GetAliveCount called.", LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            if (m_isCivilian)
            {
                //Print("[DEBUG] Group is civilian and not spawned. Returning count 1.", LogLevel.NORMAL);
                return 1;
            }
            else
            {
                int count = IA_SquadCount(m_squadType, m_faction);
                //Print("[DEBUG] Group is military and not spawned. Returning squad count: " + count, LogLevel.NORMAL);
                return count;
            }
        }
        if (!m_group)
        {	
            //Print("[DEBUG] m_group is null in GetAliveCount. Returning 0.", LogLevel.NORMAL);
            return 0;
        }
        int aliveCount = m_group.GetPlayerAndAgentCount();
        //Print("[DEBUG] Alive count from group: " + aliveCount, LogLevel.NORMAL);
        return aliveCount;
    }

    vector GetOrigin()
    {
        if (!m_group)
        {
            //Print("[DEBUG] GetOrigin called but m_group is null. Returning vector.Zero.", LogLevel.NORMAL);
            return vector.Zero;
        }
        vector origin = m_group.GetOrigin();
        //Print("[DEBUG] GetOrigin returning: " + origin, LogLevel.NORMAL);
        return origin;
    }

    bool IsSpawned()
    {
		if (GetAliveCount() == 0){
			m_isSpawned = false;
		}
        //Print("[DEBUG] IsSpawned called. Returning: " + m_isSpawned, LogLevel.NORMAL);
        return m_isSpawned;
    }

void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
{
    //Print("[DEBUG] Spawn called with initialOrder: " + initialOrder + ", orderPos: " + orderPos, LogLevel.NORMAL);
    Print("[IA_AI_CRITICAL] Spawn called - Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
          ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
          
    if (!PerformSpawn())
    {
        Print("[IA_AI_CRITICAL] PerformSpawn returned false for faction: " + 
              typename.EnumToString(IA_Faction, m_faction), LogLevel.WARNING);
        //Print("[DEBUG] PerformSpawn returned false. Aborting Spawn.", LogLevel.NORMAL);
        return;
    }
    
    Print("[IA_AI_CRITICAL] PerformSpawn succeeded for faction: " + 
          typename.EnumToString(IA_Faction, m_faction), LogLevel.WARNING);

    vector posToUse;
    if (orderPos != vector.Zero)
    {
        posToUse = orderPos;
    }
    else
    {
        // For all units not currently assigned to drive, generate a random position nearby
        posToUse = IA_Game.rng.GenerateRandomPointInRadius(5, 25, m_initialPosition);
    }

    // When spawning, if not already driving, apply the initial order
    if (!m_isDriving)
    {
        AddOrder(posToUse, initialOrder);
    }
    
    Print("[IA_AI_CRITICAL] Spawn completed for faction: " + 
          typename.EnumToString(IA_Faction, m_faction), LogLevel.WARNING);
}


	
	
	
    private bool PerformSpawn()
    {
        // Add critical debug at start of function
        Print("[IA_AI_CRITICAL] PerformSpawn START - Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
              
        //Print("[DEBUG] PerformSpawn called.", LogLevel.NORMAL);
        if (IsSpawned())
        {
            //Print("[DEBUG] Already spawned. Returning false.", LogLevel.NORMAL);
            return false;
        }
        m_isSpawned = true;

        string resourceName;
        if (m_isCivilian)
        {
            resourceName = IA_RandomCivilianResourceName();
            //Print("[DEBUG] Spawning civilian. Resource: " + resourceName, LogLevel.NORMAL);
        }
        else
        {
            resourceName = IA_SquadResourceName(m_squadType, m_faction);
            Print("[IA_AI_CRITICAL] Military group resource name: " + resourceName, LogLevel.WARNING);
            //Print("[DEBUG] Spawning military. Resource: " + resourceName, LogLevel.NORMAL);
        }

        Resource res = Resource.Load(resourceName);
        if (!res)
        {
            //Print("[IA_AiGroup.PerformSpawn] No resource: " + resourceName, LogLevel.ERROR);
            return false;
        }

        IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
        //Print("[DEBUG] Spawned main entity for group.", LogLevel.NORMAL);
        if (m_isCivilian)
        {
            // For civilians, spawn an extra group prefab and add the character
            Resource groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
            IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
            m_group = SCR_AIGroup.Cast(groupEnt);
            //Print("[DEBUG] Spawned civilian group entity.", LogLevel.NORMAL);

            if (!m_group.AddAIEntityToGroup(entity))
            {
                //Print("[IA_AiGroup.PerformSpawn] Could not add civ to group: " + resourceName, LogLevel.ERROR);
            }
            else
            {
                //Print("[DEBUG] Civilian added to group successfully.", LogLevel.NORMAL);
            }
        }
        else
        {
            m_group = SCR_AIGroup.Cast(entity);
            //Print("[DEBUG] Cast main entity to SCR_AIGroup.", LogLevel.NORMAL);
        }
        if (m_group)
        {
            vector groundPos = m_initialPosition;
            float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
            groundPos[1] = groundY;
            
            // Also update the origin if that function exists.
            m_group.SetOrigin(groundPos);
            
            //Print("[DEBUG] Group position forced to ground level: " + groundPos, LogLevel.NORMAL);
        }

        if (!m_group)
        {
            //Print("[IA_AiGroup.PerformSpawn] Could not cast to AIGroup", LogLevel.ERROR);
            return false;
        }

        SetupDeathListener();
        WaterCheck();
        
        // Register with damage manager
        /* 
        IA_DamageManager.GetInstance().RegisterAIGroup(this);
        */
        
        // Log a test message to verify logging system is working
        Print("!!! [LOG TEST] AI Group spawned at " + GetOrigin() + " with faction " + m_faction, LogLevel.WARNING);
        
        // DISABLE automatic test runs to prevent false positives
        // Test the danger event system
        // GetGame().GetCallqueue().CallLater(TestDangerSystem, 5000);
        
        // Schedule the first state evaluation when the group is spawned
        ScheduleNextStateEvaluation();
        
        // Start periodic danger event checks
        Print("[IA_DEBUG_ESSENTIAL] Scheduling periodic CheckDangerEvents - Group faction: " + 
              typename.EnumToString(IA_Faction, m_faction) + ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
        GetGame().GetCallqueue().CallLater(CheckDangerEvents, 100, true);
        Print("[IA_DEBUG_ESSENTIAL] CheckDangerEvents has been scheduled", LogLevel.WARNING);
        
        //Print("[DEBUG] PerformSpawn completed successfully.", LogLevel.NORMAL);
        Print("[IA_AI_CRITICAL] PerformSpawn COMPLETED - Faction: " + typename.EnumToString(IA_Faction, m_faction), LogLevel.WARNING);
        return true;
    }

    // Process a danger event at the group level
    void ProcessDangerEvent(IA_GroupDangerType dangerType, vector position, IEntity sourceEntity = null, float intensity = 0.5, bool isSuppressed = false)
    {
        Print("[IA_DEBUG_CRITICAL] ProcessDangerEvent - Group Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian + ", DangerType: " + typename.EnumToString(IA_GroupDangerType, dangerType), LogLevel.WARNING);
              
        // --- ADDED CHECK: Immediately skip for civilians ---
        if (m_isCivilian || m_faction == IA_Faction.CIV) {
            Print("[IA_DEBUG_CRITICAL] ProcessDangerEvent - SKIPPED because unit is civilian", LogLevel.WARNING);
            return;
        }
        // --- END OF ADDED CHECK ---

        // --- BEGIN ADDED DEBUG LOG ---
        // Log every danger event with group information, regardless of other conditions
        Print("[IA_AI_DANGER_DEBUG] ProcessDangerEvent START - Group Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              " | isCivilian: " + m_isCivilian + " | Type: " + typename.EnumToString(IA_GroupDangerType, dangerType),
              LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---

        // Skip if group is not spawned
        if (!IsSpawned() || !m_group)
        {
            // --- ADDED DEBUG LOG ---
            Print("[IA_AI_DANGER_DEBUG] ProcessDangerEvent SKIPPED - Group not spawned or null", LogLevel.ERROR);
            // --- END DEBUG LOG ---
            return;
        }
        
        // --- FRIENDLY FIRE CHECK: Check if the source entity is friendly ---
        if (sourceEntity)
        {
            // Try to get the faction of the source entity
            IA_Faction sourceFaction = IA_Faction.NONE;
            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(sourceEntity);
            
            if (character)
            {
                // Try to get faction from a character entity
                FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
                if (factionComponent)
                {
                    Faction faction = factionComponent.GetAffiliatedFaction();
                    if (faction)
                    {
                        string factionKey = faction.GetFactionKey();
                        // Map the faction key to our IA_Faction enum
                        if (factionKey == "US")
                            sourceFaction = IA_Faction.US;
                        else if (factionKey == "USSR")
                            sourceFaction = IA_Faction.USSR;
                        else if (factionKey == "FIA")
                            sourceFaction = IA_Faction.FIA;
                        else if (factionKey == "CIV")
                            sourceFaction = IA_Faction.CIV;
                        
                        Print("[IA_FRIENDLY_CHECK] Source entity faction detected: " + factionKey + 
                              " mapped to " + typename.EnumToString(IA_Faction, sourceFaction), LogLevel.WARNING);
                    }
                }
            }
            
            // Check if source is from the same faction as this group
            if (sourceFaction != IA_Faction.NONE && sourceFaction == m_faction)
            {
                Print("[IA_FRIENDLY_CHECK] Ignoring danger from friendly faction: " + 
                      typename.EnumToString(IA_Faction, sourceFaction), LogLevel.WARNING);
                      
                // Skip processing for friendly fire
                return;
            }
            
            // If we could identify a valid enemy faction, set it
            if (sourceFaction != IA_Faction.NONE && sourceFaction != m_faction)
            {
                m_engagedEnemyFaction = sourceFaction;
                Print("[IA_FRIENDLY_CHECK] Setting enemy faction to: " + 
                      typename.EnumToString(IA_Faction, m_engagedEnemyFaction), LogLevel.WARNING);
            }
            // Improvement: If we have danger but no identified enemy faction, set a reasonable fallback faction
            else if (m_engagedEnemyFaction == IA_Faction.NONE)
            {
                // Set a reasonable enemy faction based on our own faction
                if (m_faction == IA_Faction.USSR)
                    m_engagedEnemyFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    m_engagedEnemyFaction = IA_Faction.USSR;
                else if (m_faction == IA_Faction.FIA)
                    m_engagedEnemyFaction = IA_Faction.US; // FIA typically fights US
                else
                    m_engagedEnemyFaction = IA_Faction.US; // Default fallback

                Print("[IA_FRIENDLY_CHECK] No source faction identified, using fallback enemy faction: " + 
                      typename.EnumToString(IA_Faction, m_engagedEnemyFaction), LogLevel.WARNING);
            }
        }
        // --- END FRIENDLY FIRE CHECK ---
        
        // Even if we don't have a source entity, make sure we have an engaged enemy faction for non-friendly events
        else if (m_engagedEnemyFaction == IA_Faction.NONE)
        {
            // Set a reasonable enemy faction based on our own faction
            if (m_faction == IA_Faction.USSR)
                m_engagedEnemyFaction = IA_Faction.US;
            else if (m_faction == IA_Faction.US)
                m_engagedEnemyFaction = IA_Faction.USSR;
            else if (m_faction == IA_Faction.FIA)
                m_engagedEnemyFaction = IA_Faction.US; // FIA typically fights US
            else
                m_engagedEnemyFaction = IA_Faction.US; // Default fallback
                
            Print("[IA_FRIENDLY_CHECK] No source entity, using fallback enemy faction: " + 
                  typename.EnumToString(IA_Faction, m_engagedEnemyFaction), LogLevel.WARNING);
        }

        // Calculate distance to danger
        float distance = vector.Distance(GetOrigin(), position);

        // Store event information
        m_lastDangerEventTime = System.GetUnixTime();
        m_lastDangerPosition = position;
        m_lastDangerType = dangerType;

        // Calculate if this danger should be reacted to based on type and distance
        bool shouldReact = false;
        IA_AIReactionType reactionType = IA_AIReactionType.None;
        float reactionIntensity = intensity;

        // Determine reaction based on danger type
        switch (dangerType)
        {
            case IA_GroupDangerType.ProjectileImpact:
                // React to nearby impacts only
                if (distance <= BULLET_IMPACT_DISTANCE_MAX)
                {
                    shouldReact = true;
                    reactionType = IA_AIReactionType.UnderFire; // Changed from TakingFire
                    reactionIntensity = Math.Clamp(1.0 - (distance / BULLET_IMPACT_DISTANCE_MAX), 0.3, 0.8);
                    m_consecutiveDangerEvents++;

                    int currentTime_pi = System.GetUnixTime();
                    if (currentTime_pi - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                    {
                        Print("[IA_AI_DANGER] Projectile impact detected at " + distance + "m", LogLevel.WARNING);
                        m_lastDangerDetectLogTime = currentTime_pi;
                    }
                    
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AI_DANGER_DEBUG] ProjectileImpact evaluated - Distance: " + distance + 
                          " | Max: " + BULLET_IMPACT_DISTANCE_MAX + " | shouldReact: true", LogLevel.WARNING);
                    // --- END DEBUG LOG ---
                }
                // --- ADDED DEBUG LOG ---
                else {
                    Print("[IA_AI_DANGER_DEBUG] ProjectileImpact TOO FAR - Distance: " + distance + 
                          " | Max: " + BULLET_IMPACT_DISTANCE_MAX + " | shouldReact: false", LogLevel.WARNING);
                }
                // --- END DEBUG LOG ---
                break;

            case IA_GroupDangerType.WeaponFire:
                // Check if gunshot is audible based on distance and suppression
                float maxAudibleDistance;
                if (isSuppressed)
                    maxAudibleDistance = SUPPRESSED_AUDIBLE_DISTANCE;
                else
                    maxAudibleDistance = GUNSHOT_AUDIBLE_DISTANCE;
                
                if (distance <= maxAudibleDistance)
                {
                    shouldReact = true;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    // Lower intensity for distant or suppressed shots
                    float distanceFactor = Math.Clamp(1.0 - (distance / maxAudibleDistance), 0.1, 0.7);
                    float suppressionFactor;
                    if (isSuppressed)
                        suppressionFactor = 0.5;
                    else
                        suppressionFactor = 1.0;
                    
                    reactionIntensity = distanceFactor * suppressionFactor;
                    m_consecutiveDangerEvents++;
                    
                    int currentTime_wf = System.GetUnixTime();
                    if (currentTime_wf - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                    {
                        Print("[IA_AI_DANGER] Weapon fire detected at " + distance + "m (Suppressed: " + isSuppressed + ")", LogLevel.WARNING);
                        m_lastDangerDetectLogTime = currentTime_wf;
                    }
                }
                break;
                
            case IA_GroupDangerType.ProjectileFlyby:
                // Always react to flybys with high priority
                shouldReact = true;
                reactionType = IA_AIReactionType.UnderFire; // Changed from UnderAttack
                reactionIntensity = Math.Clamp(0.7 + (intensity * 0.3), 0.7, 1.0);
                m_consecutiveDangerEvents += 2; // Flybys are more threatening
                
                int currentTime_pf = System.GetUnixTime();
                if (currentTime_pf - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    Print("[IA_AI_DANGER] Projectile flyby detected!", LogLevel.WARNING);
                    m_lastDangerDetectLogTime = currentTime_pf;
                }
                break;
                
            case IA_GroupDangerType.Explosion:
                // React to explosions based on distance
                if (distance <= EXPLOSION_REACT_DISTANCE)
                {
                    shouldReact = true;
                    m_consecutiveDangerEvents += 3; // Explosions are very threatening
                    
                    int currentTime_ex = System.GetUnixTime();
                    if (currentTime_ex - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                    {
                        Print("[IA_AI_DANGER] Explosion detected at " + distance + "m", LogLevel.WARNING);
                        m_lastDangerDetectLogTime = currentTime_ex;
                    }
                }
                break;
                
            case IA_GroupDangerType.EnemySpotted:
                // Always react to enemy spotting with variable intensity
                shouldReact = true;
                reactionType = IA_AIReactionType.EnemySpotted;
                // Use provided intensity directly
                m_consecutiveDangerEvents++;
                
                int currentTime_esd = System.GetUnixTime();
                if (currentTime_esd - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    Print("[IA_AI_DANGER] Enemy spotted at " + distance + "m", LogLevel.WARNING);
                    m_lastDangerDetectLogTime = currentTime_esd;
                }
                break;
        }
        
        // --- ADDED DEBUG LOG ---
        // Log the final decision before applying the reaction
        Print("[IA_AI_DANGER_DEBUG] Final reaction decision - Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              " | shouldReact: " + shouldReact + 
              " | reactionType: " + typename.EnumToString(IA_AIReactionType, reactionType),
              LogLevel.WARNING);
        // --- END DEBUG LOG ---

        // Update danger level based on consecutive events (increases with multiple events)
        m_currentDangerLevel = Math.Clamp(0.2 * m_consecutiveDangerEvents, 0.0, 1.0);

        // Apply decay to consecutive events over time
        int timeSinceLastDanger = System.GetUnixTime() - m_lastDangerEventTime;
        if (timeSinceLastDanger > 30 && m_consecutiveDangerEvents > 0)
        {
            m_consecutiveDangerEvents = Math.Max(0, m_consecutiveDangerEvents - 1);
        }

        // If danger should cause a reaction, trigger it via reaction manager
        if (shouldReact && reactionType != IA_AIReactionType.None && m_reactionManager)
        {
            // Check source entity for faction
            IA_Faction sourceFaction = IA_Faction.NONE;
            if (sourceEntity)
            {
                SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(sourceEntity);
                if (character)
                {
                    string factionKey = character.GetFaction().GetFactionKey();
                    Print("Source Entity = " + factionKey, LogLevel.NORMAL);
                    if (factionKey == "US")
                        sourceFaction = IA_Faction.US;
                    else if (factionKey == "USSR")
                        sourceFaction = IA_Faction.USSR;
                    else if (factionKey == "CIV")
                        sourceFaction = IA_Faction.CIV;
                    else if (factionKey == "FIA")
                        sourceFaction = IA_Faction.FIA;
                }
            }
            
            // Use engaged enemy faction if we couldn't determine the source faction
            if (sourceFaction == IA_Faction.NONE)
            {
                sourceFaction = m_engagedEnemyFaction;
                Print("[IA_AI_DANGER_DEBUG] Using engaged enemy faction for reaction: " + 
                      typename.EnumToString(IA_Faction, sourceFaction), LogLevel.ERROR);
            }

            // Apply danger level as a multiplier to the reaction intensity
            float multiplier = 0.5 + (m_currentDangerLevel * 0.5);
            float finalIntensity = reactionIntensity * multiplier;

            // --- MODIFIED DEBUG LOG ---
            // Log right before triggering, including more info
            Print("[IA_AI_DANGER_DEBUG] About to TriggerReaction: " + typename.EnumToString(IA_AIReactionType, reactionType) + 
                  " | Group Faction: " + typename.EnumToString(IA_Faction, m_faction) +
                  " | isCivilian: " + m_isCivilian +
                  " | Intensity: " + finalIntensity +
                  " | Position: " + position + 
                  " | SourceFaction: " + typename.EnumToString(IA_Faction, sourceFaction),
                  LogLevel.WARNING);
            // --- END MODIFIED DEBUG LOG ---

            // Trigger the reaction in the manager
            m_reactionManager.TriggerReaction(
                reactionType,
                finalIntensity,
                position,
                sourceEntity,
                sourceFaction
            );

            int currentTime_tr = System.GetUnixTime();
            if (currentTime_tr - m_lastDangerTriggerLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
            {
                Print("[IA_AI_DANGER] Triggering reaction: " + typename.EnumToString(IA_AIReactionType, reactionType) +
                      " with intensity " + finalIntensity + " (base: " + reactionIntensity +
                      ", danger level: " + m_currentDangerLevel + ")", LogLevel.WARNING);
                m_lastDangerTriggerLogTime = currentTime_tr;
            }
        }
        // --- ADDED DEBUG LOG ---
        else if (shouldReact && reactionType != IA_AIReactionType.None) {
            string managerStatus;
            if (m_reactionManager)
                managerStatus = "NOT NULL";
            else
                managerStatus = "NULL";
                
            Print("[IA_AI_DANGER_DEBUG] NOT triggering reaction - m_reactionManager is " + 
                  managerStatus, LogLevel.ERROR);
        }
        // --- END DEBUG LOG ---
    }
    
    // New method to reset all danger and reaction states - useful for debugging and fixing false positives
    void ResetDangerState()
    {
        m_currentDangerLevel = 0.0;
        m_consecutiveDangerEvents = 0;
        m_lastDangerEventTime = 0;
        m_lastDangerPosition = vector.Zero;
        
        if (m_reactionManager)
        {
            m_reactionManager.ClearAllReactions();
        }
        
        Print("[IA_AI_DANGER] Manually reset ALL danger state and reactions for group at " + GetOrigin(), LogLevel.WARNING);
    }
    
    // Modify the reaction manager class to ensure reactions expire properly
    // Edit IA_AIReactionManager.ProcessReactions() method
    /* 
    void ProcessReactions()
    {
        // Process active reactions
        if (m_activeReactions.Count() > 0)
        {
            // Check if current reaction has expired
            int currentTime = System.GetUnixTime();
            
            // Process all reactions in the list to apply decay and remove expired ones
            for (int i = m_activeReactions.Count() - 1; i >= 0; i--)
            {
                IA_AIReactionState reaction = m_activeReactions[i];
                if (!reaction)
                {
                    m_activeReactions.RemoveOrdered(i);
                continue;
            }
                
                // Check if reaction has expired
                if (currentTime - reaction.GetStartTime() > reaction.GetDuration())
            {
                    Print("[IA_AI_REACTION] Reaction expired: " + 
                          typename.EnumToString(IA_AIReactionType, reaction.GetReactionType()), LogLevel.WARNING);
                    m_activeReactions.RemoveOrdered(i);
                continue;
            }
                
                // Apply intensity decay over time for long-lasting reactions
                if (reaction.GetDuration() > 30 && currentTime - reaction.GetStartTime() > 15)
                {
                    float originalIntensity = reaction.GetIntensity();
                    float percentRemaining = 1.0 - ((currentTime - reaction.GetStartTime()) / reaction.GetDuration());
                    float newIntensity = originalIntensity * percentRemaining;
                    reaction.SetIntensity(newIntensity);
                }
            }
            
            // Sort reactions by priority again in case intensities have changed
            if (m_activeReactions.Count() > 0)
            {
                SortReactionsByPriority();
            }
        }
    }
    */

    // Test method to verify the danger system is working
    private void TestDangerSystem()
    {
        if (!IsSpawned() || !m_group)
             return;
            
        // Create a test position in front of the group
        vector forward = Vector(1, 0, 0);
        vector testPos = GetOrigin() + forward * 20;
        
        // Print direct debug message to confirm method is running
        Print("!!! [TEST_DANGER_SYSTEM] Testing danger system for group at " + GetOrigin(), LogLevel.WARNING);
        
        // Test each danger type
        OnProjectileImpact(testPos);
        
        // Schedule more tests with delays
        GetGame().GetCallqueue().CallLater(TestWeaponFire, 1000);
        GetGame().GetCallqueue().CallLater(TestProjectileFlyby, 2000);
        GetGame().GetCallqueue().CallLater(TestExplosion, 3000);
    }
    
    private void TestWeaponFire()
    {
        if (!IsSpawned()) return;
        
        Print("!!! [TEST_DANGER_SYSTEM] Testing weapon fire reaction", LogLevel.WARNING);
        vector testPos = GetOrigin() + Vector(0, 0, 1) * 30;
        OnWeaponFired(testPos, null, false);
    }
    
    private void TestProjectileFlyby()
    {
        if (!IsSpawned()) return;
        
        Print("!!! [TEST_DANGER_SYSTEM] Testing projectile flyby reaction", LogLevel.WARNING);
        vector testPos = GetOrigin() + Vector(-1, 0, 0) * 15;
        OnProjectileFlyby(testPos);
    }
    
    private void TestExplosion()
    {
        if (!IsSpawned()) return;
        
        Print("!!! [TEST_DANGER_SYSTEM] Testing explosion reaction", LogLevel.WARNING);
        vector testPos = GetOrigin() + Vector(0, 0, -1) * 40;
        OnExplosion(testPos);
    }
    
    // Helper methods for specific danger types
    
    // Called when bullets hit near the group
    void OnProjectileImpact(vector impactPosition, IEntity shooterEntity = null)
    {
        ProcessDangerEvent(IA_GroupDangerType.ProjectileImpact, impactPosition, shooterEntity);
    }
    
    // Called when a weapon is fired that the group can hear
    void OnWeaponFired(vector shotPosition, IEntity shooterEntity = null, bool isSuppressed = false)
    {
        ProcessDangerEvent(IA_GroupDangerType.WeaponFire, shotPosition, shooterEntity, 0.5, isSuppressed);
    }
    
    // Called when a projectile passes very close to the group
    void OnProjectileFlyby(vector flybyPosition, IEntity shooterEntity = null)
    {
        ProcessDangerEvent(IA_GroupDangerType.ProjectileFlyby, flybyPosition, shooterEntity, 0.8);
    }
    
    // Called when an explosion occurs near the group
    void OnExplosion(vector explosionPosition, IEntity sourceEntity = null)
    {
        // --- BEGIN ADDED DEBUG LOG ---
		if(!sourceEntity){
			Print("[IA_EXPLOSION_DEBUG] OnExplosion called - Group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian + 
              ", Position: " + explosionPosition + 
              ", SourceEntity: IS NULL!",LogLevel.ERROR);
		}
        else 
        {
            Print("[IA_EXPLOSION_DEBUG] OnExplosion called - Group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
                ", isCivilian: " + m_isCivilian + 
                ", Position: " + explosionPosition + 
                ", SourceEntity: " + sourceEntity.ToString(),
                LogLevel.WARNING);
        }
              
        // Check if the reaction manager exists
        string managerStatus;
        if (m_reactionManager)
            managerStatus = "NOT NULL";
        else
            managerStatus = "NULL";
            
        // Check if m_group exists
        string groupStatus;
        if (m_group)
            groupStatus = "NOT NULL";
        else
            groupStatus = "NULL";
            
        Print("[IA_EXPLOSION_DEBUG] OnExplosion: m_reactionManager is " + managerStatus + 
              ", group isSpawned: " + IsSpawned() + 
              ", m_group: " + groupStatus,
              LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---
        
        ProcessDangerEvent(IA_GroupDangerType.Explosion, explosionPosition, sourceEntity, 0.9);
    }
    
    // Called when the group spots an enemy
    void OnEnemySpotted(vector enemyPosition, IEntity enemyEntity, float threatLevel = 0.5)
    {
        Print("[IA_NEW_DEBUG] OnEnemySpotted called. Group faction: " + m_faction + 
              ", Entity: " + enemyEntity + 
              ", ThreatLevel: " + threatLevel, 
              LogLevel.WARNING);
          
        ProcessDangerEvent(IA_GroupDangerType.EnemySpotted, enemyPosition, enemyEntity, threatLevel);
    }
    
    // New method to add to EvaluateGroupState to check danger level and react accordingly
    private void EvaluateDangerState()
    {
        // If no current danger, reset and return
        if (m_currentDangerLevel <= 0.0 || System.GetUnixTime() - m_lastDangerEventTime > 120)
        {
            if (m_consecutiveDangerEvents > 0)
            {
                m_consecutiveDangerEvents = 0;
                m_currentDangerLevel = 0.0;
                Print("[IA_AI_DANGER] Danger state reset due to timeout", LogLevel.WARNING);
            }
            return;
        }
        
        // If already engaged or has active reaction, let those systems handle it
        if (IsEngagedWithEnemy() || (m_reactionManager && m_reactionManager.HasActiveReaction()))
            return;
            
        // If high danger level but no specific reaction yet, create a general heightened awareness state
        if (m_currentDangerLevel > 0.7 && !m_isDriving)
        {
            // Group is in high danger but no specific threat identified
            // Take defensive actions - find cover or better position
            vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(10, 20, GetOrigin());
            
            // If we have a last known danger position, move away from it
            if (m_lastDangerPosition != vector.Zero)
            {
                vector awayDir = GetOrigin() - m_lastDangerPosition;
                awayDir = awayDir.Normalized();
                defendPos = GetOrigin() + awayDir * 15;
            }
            
            // Only issue new orders if we don't already have some
            if (!HasOrders())
            {
                Print("[IA_AI_DANGER] High danger level detected, taking defensive position", LogLevel.WARNING);
                AddOrder(defendPos, IA_AiOrder.Defend, true);
            }
        }
        else if (m_currentDangerLevel > 0.3 && !m_isDriving && !HasOrders())
        {
            // Medium danger level, increase awareness by looking in danger direction
            if (m_lastDangerPosition != vector.Zero)
            {
                Print("[IA_AI_DANGER] Medium danger level, orienting toward potential threat", LogLevel.WARNING);
                AddOrder(m_lastDangerPosition, IA_AiOrder.Defend);
            }
        }
    }

    // Helper to set up death listener for a single unit (used by CreateGroupForVehicle)
    private void SetupDeathListenerForUnit(IEntity unitEntity)
    {
         SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(unitEntity);
         if (!ch) return;

         SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
         if (!ccc) {
             //Print("[IA_AiGroup.SetupDeathListenerForUnit] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
             return;
         }
         ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
         
         // Register this unit with the damage manager
         /* 
         IA_DamageManager.GetInstance().RegisterEntity(ch, this);
         */
    }

    private void SetupDeathListener()
    {
        Print("[IA_DEBUG] SetupDeathListener started - Group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
              
        if (!m_group)
        {
            Print("[IA_DEBUG] SetupDeathListener - m_group is null, rescheduling", LogLevel.ERROR);
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        if (agents.IsEmpty())
        {
            Print("[IA_DEBUG] SetupDeathListener - No agents found, rescheduling", LogLevel.ERROR);
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }
        
        // Log the number of agents found
        Print("[IA_DEBUG] SetupDeathListener - Found " + agents.Count() + " agents", LogLevel.WARNING);
        
        foreach (AIAgent agent : agents)
        {
            if (!agent)
            {
                Print("[IA_DEBUG] SetupDeathListener - Found null agent in group", LogLevel.ERROR);
                continue;
            }
                
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                Print("[IA_DEBUG] SetupDeathListener - Failed to cast to SCR_ChimeraCharacter", LogLevel.ERROR);
                continue;
            }
                
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
            {
                Print("[IA_DEBUG] SetupDeathListener - Failed to find SCR_CharacterControllerComponent", LogLevel.ERROR);
                continue;
            }
                
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
            
            // Set up danger event processing for this agent
            ProcessAgentDangerEvents(agent);
            
            // Verify the agent has danger event capability
            int dangerCount = agent.GetDangerEventsCount();
            Print("[IA_DEBUG] SetupDeathListener - Agent has " + dangerCount + " danger events initially", LogLevel.WARNING);
        }
        
        // For military units, we want to ensure they can properly respond to danger
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            Print("[IA_DEBUG] Military unit setup complete - Scheduling immediate danger check", LogLevel.WARNING);
            // Schedule an immediate check for danger events to ensure the system is working
            GetGame().GetCallqueue().CallLater(CheckDangerEvents, 100, false);
        }
    }

    // New method to process danger events for an agent
    private void ProcessAgentDangerEvents(AIAgent agent)
    {
        if (!agent)
        {
            Print("[IA_DEBUG] ProcessAgentDangerEvents - Agent is null", LogLevel.ERROR);
            return;
        }
            
        int dangerCount = agent.GetDangerEventsCount();
        Print("[IA_DEBUG] ProcessAgentDangerEvents - Group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", Agent danger events count: " + dangerCount, LogLevel.WARNING);
              
        if (dangerCount <= 0)
            return;
           
        // --- BEGIN ADDED DEBUG LOG ---
        Print("[IA_AGENT_DANGER_DEBUG] ProcessAgentDangerEvents START - Group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", Found " + dangerCount + " danger events", LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---

        // Process each danger event
        for (int i = 0; i < dangerCount; i++)
        {
            int outCount;
            AIDangerEvent dangerEvent = agent.GetDangerEvent(i, outCount);
            if (!dangerEvent)
            {
                Print("[IA_DEBUG] ProcessAgentDangerEvents - Danger event at index " + i + " is null", LogLevel.ERROR);
                continue;
            }
                
            EAIDangerEventType dangerType = dangerEvent.GetDangerType();
            AIAgent senderAgent = dangerEvent.GetSender();
            AIAgent receiverAgent = dangerEvent.GetReceiver();
            vector dangerVector = dangerEvent.GetPosition();
            
            // --- BEGIN ADDED DEBUG LOG ---
            Print("[IA_AGENT_DANGER_DEBUG] Processing event " + i + " of type: " + dangerType + 
                  " at position: " + dangerVector, LogLevel.WARNING);
            // --- END ADDED DEBUG LOG ---
                
            // Map the native danger type to our reaction system
            IA_GroupDangerType mappedDangerType;
            IA_AIReactionType reactionType;
            float intensity = 0.5;
            bool isSuppressed = false;
            
            // Default to handling the event unless we explicitly skip it
            bool handleEvent = true;
            Print("Danger Type Detected: " + dangerType, LogLevel.NORMAL);
            switch (dangerType)
            {
                case EAIDangerEventType.Danger_ProjectileHit:
                    mappedDangerType = IA_GroupDangerType.ProjectileImpact;
                    reactionType = IA_AIReactionType.UnderFire; // Changed from TakingFire
                    intensity = 0.7;
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Mapped ProjectileHit to ProjectileImpact with intensity " + intensity, LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    break;
                    
                case EAIDangerEventType.Danger_WeaponFire:
                    mappedDangerType = IA_GroupDangerType.WeaponFire;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    intensity = 0.6;
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Mapped WeaponFire to WeaponFire with intensity " + intensity, LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    break;
                    
                case EAIDangerEventType.Danger_Explosion:
                    mappedDangerType = IA_GroupDangerType.Explosion;
                    // FIX: Add appropriate reaction type for explosions
                    reactionType = IA_AIReactionType.UnderFire; // Use UnderFire as the reaction type for explosions
                    intensity = 0.9;
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Mapped Explosion to Explosion with intensity " + intensity, LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    break;
                    
                case EAIDangerEventType.Danger_NewEnemy:
                    mappedDangerType = IA_GroupDangerType.EnemySpotted;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    intensity = 0.8;
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Mapped NewEnemy to EnemySpotted with intensity " + intensity, LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    break;
                    
                case EAIDangerEventType.Danger_DamageTaken:
                    mappedDangerType = IA_GroupDangerType.ProjectileImpact;
                    reactionType = IA_AIReactionType.UnderFire; // Changed from TakingFire
                    intensity = 0.6;
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Mapped DamageTaken to ProjectileImpact with intensity " + intensity, LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    break;
                    
                default:
                    // --- ADDED DEBUG LOG ---
                    Print("[IA_AGENT_DANGER_DEBUG] Unhandled danger type: " + dangerType + " - skipping", LogLevel.WARNING);
                    // --- END ADDED DEBUG LOG ---
                    handleEvent = false;
                    continue; // Skip other danger types for now
            }
            
            // Only proceed if we've decided to handle this event type
            if (!handleEvent)
                continue;
            
            // Get the source entity if available
            IEntity sourceEntity = null;
            if (senderAgent)
            {
                sourceEntity = senderAgent.GetControlledEntity();
                // --- ADDED DEBUG LOG ---
                string entityInfo = "NULL";
                if (sourceEntity)
                {
                    entityInfo = sourceEntity.ToString();
                }
                Print("[IA_AGENT_DANGER_DEBUG] Source entity: " + entityInfo, LogLevel.WARNING);
                // --- END ADDED DEBUG LOG ---
            }
            
            // Calculate distance to danger
            float distance = vector.Distance(GetOrigin(), dangerVector);
            Print("[IA_AGENT_DANGER_DEBUG] Distance to danger: " + distance + "m", LogLevel.WARNING);
            
            // Use distance-based logic for intensity
            switch (mappedDangerType)
            {
                case IA_GroupDangerType.ProjectileImpact:
                    // Higher intensity for closer impacts
                    if (distance < BULLET_IMPACT_DISTANCE_MAX)
                    {
                        // Scale intensity based on distance (closer = more intense)
                        intensity = Math.Clamp(1.0 - (distance / BULLET_IMPACT_DISTANCE_MAX), 0.4, 0.9);
                        Print("[IA_AGENT_DANGER_DEBUG] Adjusted ProjectileImpact intensity to " + intensity + " based on distance", LogLevel.WARNING);
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
                        Print("[IA_AGENT_DANGER_DEBUG] Adjusted ProjectileFlyby intensity to " + intensity + " based on distance", LogLevel.WARNING);
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
                        Print("[IA_AGENT_DANGER_DEBUG] Adjusted WeaponFire intensity to " + intensity + " based on distance", LogLevel.WARNING);
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
                        Print("[IA_AGENT_DANGER_DEBUG] Adjusted Explosion intensity to " + intensity + " based on distance", LogLevel.WARNING);
                    }
                    else
                    {
                        // Too far to be concerning
                        intensity = 0.0;
                        handleEvent = false;
                    }
                    break;
            }
            
            // Skip if distance-based logic determined this isn't worth reacting to
            if (!handleEvent || intensity <= 0.0)
                continue;
            
            // --- ADDED DEBUG LOG ---
            Print("[IA_AGENT_DANGER_DEBUG] About to call ProcessDangerEvent with type: " + 
                  typename.EnumToString(IA_GroupDangerType, mappedDangerType) + 
                  ", intensity: " + intensity, LogLevel.WARNING);
            // --- END ADDED DEBUG LOG ---
            
            // Process the danger event through our existing system
            ProcessDangerEvent(mappedDangerType, dangerVector, sourceEntity, intensity, isSuppressed);
        }

        Print("[IA_NEW_DEBUG] ProcessAgentDangerEvents - Group faction: " + m_faction + 
              ", Agent has had this many danger events: " + agent.GetDangerEventsCount(),
              LogLevel.WARNING);
    }

    private void WaterCheck()
    {
        //Print("[DEBUG] WaterCheck called (placeholder).", LogLevel.NORMAL);
        // No extra water check here because safe areas are pre-defined.
    }
    
    void UpdateVehicleOrders()
    {
        if (!m_isDriving || !m_referencedEntity)
        {
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - exiting early - isDriving: " + m_isDriving + ", referencedEntity: " + m_referencedEntity, LogLevel.WARNING);
            return;
        }
            
        // Skip if we've just updated recently
        int currentTime = System.GetUnixTime();
        if (currentTime - m_lastVehicleOrderTime < VEHICLE_ORDER_UPDATE_INTERVAL)
        {
            return;
        }
            
        m_lastVehicleOrderTime = currentTime;
        
        Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
        if (!vehicle)
        {
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - Invalid vehicle reference, clearing", LogLevel.ERROR);
            // Clear vehicle reference if it's no longer valid
            ClearVehicleReference();
            return;
        }
        
        // Get all group members
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        if (characters.IsEmpty())
        {
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - No group characters found", LogLevel.WARNING);
            return;
        }
            
        bool anyOutside = false;
        
        // Check if any characters are outside the vehicle
        foreach (SCR_ChimeraCharacter character : characters)
        {
            if (!character.IsInVehicle())
            {
                anyOutside = true;
                break;
            }
        }
        
        // If anyone is outside the vehicle, issue GetInVehicle order - works for both military and civilians
        if (anyOutside)
        {
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - Members outside vehicle " + vehicle + ", ordering them back in", LogLevel.NORMAL);
            
            // Clear existing orders
            RemoveAllOrders();
            
            // Add GetInVehicle order using the existing order system
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            
            // Make sure driving state is still set, especially important for civilians
            m_isDriving = true;
            
            return;
        }
        
        // If everyone is in the vehicle and we have a destination, make sure we have a waypoint
        if (!anyOutside && m_drivingTarget != vector.Zero)
        {
            // Check if we need a new waypoint (don't have one or reached destination)
            bool hasActiveWP = HasActiveWaypoint();
            bool vehicleReachedDest = IA_VehicleManager.HasVehicleReachedDestination(vehicle, m_drivingTarget);
            
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - hasActiveWaypoint: " + hasActiveWP + 
                  ", reachedDestination: " + vehicleReachedDest + 
                  ", target: " + m_drivingTarget, LogLevel.NORMAL);
                  
            if (!hasActiveWP || vehicleReachedDest)
            {
                Print("[VEHICLE_DEBUG] UpdateVehicleOrders - Creating new waypoint for vehicle " + vehicle, LogLevel.NORMAL);
                // Everyone's in the vehicle and we need a waypoint - ensure driving mode is on (especially for civilians) 
                m_isDriving = true;
                
                // Update the waypoint using the existing utility function
                IA_VehicleManager.UpdateVehicleWaypoint(vehicle, this, m_drivingTarget);
            }
        }
        else if (m_drivingTarget == vector.Zero)
        {
            Print("[VEHICLE_DEBUG] UpdateVehicleOrders - Vehicle has zero destination target!", LogLevel.ERROR);
        }
    }

    // Add a method to properly reset vehicle state
    void ClearVehicleReference()
    {
        Print("[VEHICLE_DEBUG] ClearVehicleReference called. Previous state: isDriving=" + m_isDriving + ", entity=" + m_referencedEntity, LogLevel.WARNING);
        
        // If there was a vehicle reservation, release it
        if (m_referencedEntity)
        {
            Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
            if (vehicle)
            {
                IA_VehicleManager.ReleaseVehicleReservation(vehicle);
                Print("[VEHICLE_DEBUG] Released vehicle reservation for " + vehicle, LogLevel.NORMAL);
            }
        }
        
        // Reset vehicle-related state variables
        m_isDriving = false;
        m_referencedEntity = null;
        m_drivingTarget = vector.Zero;
        Print("[VEHICLE_DEBUG] Vehicle state has been cleared", LogLevel.NORMAL);
    }

    // Return the referenced entity (usually a vehicle)
    IEntity GetReferencedEntity()
    {
        return m_referencedEntity;
    }
    
    // Return the current driving target position
    vector GetDrivingTarget()
    {
        return m_drivingTarget;
    }

    // Add a method to help debug the m_isDriving field
    void ForceDrivingState(bool state)
    {
        //Print("[DEBUG_VEHICLE_LOGIC] ForceDrivingState called, changing from " + m_isDriving + " to " + state, LogLevel.NORMAL);
        m_isDriving = state;
    }

    // Process AI reactions and apply appropriate behaviors
    void ProcessReactions()
    {
        // --- ADDED CHECK: Immediately skip for civilians ---
        if (m_isCivilian || m_faction == IA_Faction.CIV) {
            return;
        }
        // --- END OF ADDED CHECK ---
        
        // Skip if not initialized or spawned
        if (!m_reactionManager || !IsSpawned())
            return;
            
        // Process reactions in the manager
        m_reactionManager.ProcessReactions();
        
        // Check if there's an active reaction to respond to
        if (m_reactionManager.HasActiveReaction())
        {
            // Get the current reaction state
            IA_AIReactionState reaction = m_reactionManager.GetCurrentReaction();
            if (reaction)
            {
                // --- BEGIN ADDED DEBUG LOG ---
                IA_AIReactionType reactionType = reaction.GetReactionType();
                Print("[IA_AI_REACTION_DEBUG] ProcessReactions(): Group Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
                      " | Reaction Type: " + typename.EnumToString(IA_AIReactionType, reactionType) + 
                      " | isCivilian: " + m_isCivilian, LogLevel.WARNING);
                // --- END ADDED DEBUG LOG ---
                
                // Apply behavior based on the reaction state
                ApplyReactionBehavior(reaction);
            }
        }
    }
    
    // Apply specific behaviors for reactions
    private void ApplyReactionBehavior(IA_AIReactionState reaction)
    {
        // Safety check
        if (!reaction)
            return;
            
        // --- BEGIN ADDED DEBUG LOG ---
        IA_AIReactionType reactionType = reaction.GetReactionType();
        Print("[IA_AI_REACTION_DEBUG] ApplyReactionBehavior(): Group Faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              " | Reaction Type: " + typename.EnumToString(IA_AIReactionType, reactionType) + 
              " | isCivilian: " + m_isCivilian, LogLevel.WARNING);
        // --- END ADDED DEBUG LOG ---
            
        // Apply behavior based on reaction type
        switch (reaction.GetReactionType())
        {
            case IA_AIReactionType.GroupMemberKilled:
                // --- ADDED DEBUG LOG ---
                Print("[IA_AI_REACTION_DEBUG] Switch: About to call HandleGroupMemberKilledReaction", LogLevel.WARNING);
                // --- END DEBUG LOG ---
                HandleGroupMemberKilledReaction(reaction);
                break;
                
            case IA_AIReactionType.UnderFire: // Changed from UnderAttack
                // --- ADDED DEBUG LOG ---
                Print("[IA_AI_REACTION_DEBUG] Switch: About to call HandleUnderFireReaction", LogLevel.WARNING);
                // --- END DEBUG LOG ---
                int currentTime_uf = System.GetUnixTime();
                if (currentTime_uf - m_lastApplyUnderFireLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    Print("[IA_AI_BEHAVIOR] Applying UnderFire reaction behavior.", LogLevel.WARNING);
                    m_lastApplyUnderFireLogTime = currentTime_uf;
                }
                HandleUnderFireReaction(reaction); // Renamed handler
                break;
                
            case IA_AIReactionType.EnemySpotted:
                // --- ADDED DEBUG LOG ---
                Print("[IA_AI_REACTION_DEBUG] Switch: About to call HandleEnemySpottedReaction", LogLevel.WARNING);
                // --- END DEBUG LOG ---
                int currentTime_es = System.GetUnixTime();
                if (currentTime_es - m_lastApplyEnemySpottedLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    Print("[IA_AI_BEHAVIOR] Applying EnemySpotted reaction behavior.", LogLevel.WARNING);
                    m_lastApplyEnemySpottedLogTime = currentTime_es;
                }
                HandleEnemySpottedReaction(reaction);
                break;
                
            // More handlers for other reaction types can be added later
        }
    }

    static IA_AiGroup CreateCivilianGroupForVehicle(Vehicle vehicle, int unitCount)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // slightly above vehicle
        // Use the standard civilian group spawner to handle spawning
        IA_AiGroup grp = IA_AiGroup.CreateCivilianGroup(spawnPos);
        return grp;
    }
    
    void AssignVehicle(Vehicle vehicle, vector destination)
    {
        Print("[VEHICLE_DEBUG] AssignVehicle called. Vehicle: " + vehicle + ", Destination: " + destination, LogLevel.NORMAL);
        
        if (!vehicle || !IsSpawned())
        {
            Print("[VEHICLE_DEBUG] AssignVehicle failed - NULL vehicle or group not spawned", LogLevel.WARNING);
            return;
        }
        
        // Try to reserve the vehicle - if we can't, don't proceed
        bool reserved = IA_VehicleManager.ReserveVehicle(vehicle, this);
        if (!reserved)
        {
            Print("[VEHICLE_DEBUG] AssignVehicle failed - Could not reserve vehicle", LogLevel.WARNING);
            return;
        }
        
        // Save vehicle reference and update state
        m_referencedEntity = vehicle;
        m_isDriving = true;
        m_drivingTarget = destination;
        
        // Update the last order position to the vehicle's position so we can track when we've reached it
        m_lastOrderPosition = vehicle.GetOrigin();
        m_lastOrderTime = System.GetUnixTime();
        
        // Clear any existing orders
        RemoveAllOrders();
        
        // If we have a valid destination, update the vehicle waypoint
        if (destination != vector.Zero) 
        {
            Print("[VEHICLE_DEBUG] AssignVehicle - Creating waypoint to destination: " + destination, LogLevel.NORMAL);
            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, this, destination);
        }
        else
        {
            Print("[VEHICLE_DEBUG] AssignVehicle - WARNING: Destination is vector.Zero!", LogLevel.ERROR);
        }
        
        Print("[VEHICLE_DEBUG] Vehicle successfully assigned to group", LogLevel.NORMAL);
    }

    // Add this after the constructor to make sure it's properly defined
    void Despawn()
    {
        //Print("[DEBUG] Despawn called.", LogLevel.NORMAL);
        if (!IsSpawned())
        {
            //Print("[DEBUG] Group is not spawned. Nothing to despawn.", LogLevel.NORMAL);
            return;
        }
        m_isSpawned = false;
        
        // If this was a vehicle group, properly clean up
        if (m_isDriving || m_referencedEntity)
        {
            //Print("[DEBUG_VEHICLE_TRACKING] Clearing vehicle state during despawn", LogLevel.NORMAL);
            ClearVehicleReference();
        }
        
        // Unregister group members from damage manager
        /*
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        foreach (SCR_ChimeraCharacter character : characters)
        {
            if (character)
                IA_DamageManager.GetInstance().UnregisterEntity(character);
        }
        */

        // Cancel any scheduled state evaluations
        m_isStateEvaluationScheduled = false;

        if (m_group)
        {
            //Print("[DEBUG] Deactivating all members of the group.", LogLevel.NORMAL);
            m_group.DeactivateAllMembers();
        }
        RemoveAllOrders();
        IA_Game.AddEntityToGc(m_group);
        m_group = null;
        //Print("[DEBUG] Group despawned successfully.", LogLevel.NORMAL);
    }

    // Schedule the next state evaluation at a random interval
    private void ScheduleNextStateEvaluation()
    {
        if (m_isStateEvaluationScheduled)
            return;
            
        // Calculate a random delay between MIN and MAX
        int randomDelay = Math.RandomInt(MIN_STATE_EVALUATION_INTERVAL, MAX_STATE_EVALUATION_INTERVAL);
        
        GetGame().GetCallqueue().CallLater(EvaluateGroupState, randomDelay);
        m_isStateEvaluationScheduled = true;
        
        //Print("[DEBUG] Scheduled next state evaluation in " + randomDelay + "ms", LogLevel.NORMAL);
    }

    private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {
        //Print("[DEBUG] OnMemberDeath called.", LogLevel.NORMAL);
        // Get victim info
        IEntity victimEntity = memberCtrl.GetOwner();
        vector deathPosition = victimEntity.GetOrigin();
        
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
            Print("[IA_AI_DEATH_DEBUG] Member killed by player.", LogLevel.WARNING);
            
            // Improved player faction detection - Try to get actual faction instead of defaulting to CIV
            IA_Faction playerFaction = IA_Faction.NONE;
            SCR_ChimeraCharacter playerCharacter = SCR_ChimeraCharacter.Cast(killerEntity);
            
            if (playerCharacter)
            {
                // Try to get faction from the player character
                FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(playerCharacter.FindComponent(FactionAffiliationComponent));
                if (factionComponent)
                {
                    Faction faction = factionComponent.GetAffiliatedFaction();
                    if (faction)
                    {
                        string factionKey = faction.GetFactionKey();
                        // Map the faction key to our IA_Faction enum
                        if (factionKey == "US")
                            playerFaction = IA_Faction.US;
                        else if (factionKey == "USSR")
                            playerFaction = IA_Faction.USSR;
                        else if (factionKey == "FIA")
                            playerFaction = IA_Faction.FIA;
                        else if (factionKey == "CIV")
                            playerFaction = IA_Faction.CIV;
                        
                        Print("[IA_AI_DEATH_DEBUG] Player faction detected: " + factionKey + 
                              " mapped to " + typename.EnumToString(IA_Faction, playerFaction), LogLevel.WARNING);
                    }
                }
            }
            
            // If we couldn't determine player faction, use an appropriate fallback based on our own faction
            if (playerFaction == IA_Faction.NONE)
            {
                // Pick a reasonable opposing faction based on our own
                if (m_faction == IA_Faction.USSR)
                    playerFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    playerFaction = IA_Faction.USSR;
                else
                    playerFaction = IA_Faction.CIV; // Default fallback if all else fails
                
                Print("[IA_AI_DEATH_DEBUG] Couldn't determine player faction. Using fallback: " + 
                      typename.EnumToString(IA_Faction, playerFaction), LogLevel.WARNING);
            }
            
            // Use the new reaction system
            float groupSize = GetAliveCount() + 1; // +1 for the unit that just died
            float intensity = 1.0 / groupSize; // Higher intensity for smaller groups
            
            // Trigger reaction with player as source and determined faction
            if (m_reactionManager)
            {
                m_reactionManager.TriggerReaction(
                    IA_AIReactionType.GroupMemberKilled, 
                    intensity, 
                    deathPosition, 
                    killerEntity, 
                    playerFaction
                );
            }
            
            // Legacy engagement system for compatibility
            if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
            {
                m_engagedEnemyFaction = playerFaction;
                Print("[IA_AI_DEATH_DEBUG] Setting enemy faction to player faction: " + typename.EnumToString(IA_Faction, playerFaction), LogLevel.WARNING);
            }
        }
        else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
        {
            //Print("[DEBUG] Member killed by AI.", LogLevel.NORMAL);
            SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
            if (!c)
            {
                //Print("[IA_AiGroup.OnMemberDeath] cast killerEntity to character failed!", LogLevel.ERROR);
                return;
            }
            
            string fKey = c.GetFaction().GetFactionKey();
            //Print("[DEBUG] Killer faction key: " + fKey, LogLevel.NORMAL);
            
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
            
            // Use the new reaction system
            if (m_reactionManager && killerFaction != m_faction) // Skip if friendly fire
            {
                float groupSize = GetAliveCount() + 1; // +1 for the unit that just died
                float intensity = 1.0 / groupSize; // Higher intensity for smaller groups
                
                m_reactionManager.TriggerReaction(
                    IA_AIReactionType.GroupMemberKilled, 
                    intensity, 
                    deathPosition, 
                    killerEntity, 
                    killerFaction
                );
            }
            
            // Legacy engagement system
            if (killerFaction != m_faction)
            {
                m_engagedEnemyFaction = killerFaction;
            }
            // If friendly fire, reset engagement.
            if (m_engagedEnemyFaction == m_faction)
            {
                //Print("[IA_AiGroup.OnMemberDeath] Friendly kill => resetting engagement");
                m_engagedEnemyFaction = IA_Faction.NONE;
            }
        }
    }

    // Handle reaction when a group member is killed
    private void HandleGroupMemberKilledReaction(IA_AIReactionState reaction)
    {
        Print("[IA_AI_BEHAVIOR_HANDLER] Executing: HandleGroupMemberKilledReaction", LogLevel.WARNING); // Added log
        // Make sure the legacy engagement system is updated
        if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
        {
            IA_Faction sourceFaction = reaction.GetSourceFaction();
            if (sourceFaction != IA_Faction.NONE)
            {
                m_engagedEnemyFaction = sourceFaction;
            }
            else
            {
                // If we don't know the source faction, use CIV as placeholder like in original code
                m_engagedEnemyFaction = IA_Faction.CIV;
            }
        }
        
        // Only take additional action if this is a high intensity reaction
        if (reaction.GetIntensity() > 0.5)
        {
            // Get defensive position away from threat
            vector threatPos = reaction.GetSourcePosition();
            if (threatPos != vector.Zero && !m_isDriving)
            {
                // Move away from threat and take cover
                vector awayDir = m_group.GetOrigin() - threatPos;
                awayDir = awayDir.Normalized();
                vector retreatPos = m_group.GetOrigin() + awayDir * 20;
                
                RemoveAllOrders();
                AddOrder(retreatPos, IA_AiOrder.Defend, true);
            }
        }
    }
    
    // Handle reaction when under direct attack
    private void HandleUnderFireReaction(IA_AIReactionState reaction)
    {
        Print("[IA_AI_BEHAVIOR_HANDLER] Executing: HandleUnderFireReaction", LogLevel.WARNING); // Existing log

        // Update legacy engagement state
        IA_Faction sourceFaction = reaction.GetSourceFaction();

        // Existing Debug Log
        Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction Faction Check - Source: " + typename.EnumToString(IA_Faction, sourceFaction) +
              " | Group: " + typename.EnumToString(IA_Faction, m_faction), LogLevel.WARNING);

        if (sourceFaction != IA_Faction.NONE && !IsEngagedWithEnemy())
        {
            m_engagedEnemyFaction = sourceFaction;
        }

        // If we're in a vehicle, we might want to flee
        if (m_isDriving && m_referencedEntity)
        {
            // --- BEGIN MODIFIED DEBUG LOG ---
            string entityInfo;
            if (m_referencedEntity)
                entityInfo = m_referencedEntity.ToString();
            else
                entityInfo = "NULL";
                
            Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction - Executing VEHICLE branch. (m_isDriving=" + m_isDriving + ", m_referencedEntity=" + entityInfo + ")", LogLevel.WARNING);
            // --- END MODIFIED DEBUG LOG ---

            // Find a position away from the threat
            vector threatPos = reaction.GetSourcePosition();
            if (threatPos != vector.Zero)
            {
                vector awayDir = m_group.GetOrigin() - threatPos;
                awayDir = awayDir.Normalized();
                vector retreatPos = m_group.GetOrigin() + awayDir * 100;

                // Update driving target
                m_drivingTarget = retreatPos;
                UpdateVehicleOrders();
            }
        }
        else if (!m_isDriving) // If on foot, take cover and engage
        {
             // --- BEGIN ADDED DEBUG LOG ---
            Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction - Executing INFANTRY branch. (m_isDriving=" + m_isDriving + ")", LogLevel.WARNING);
            // --- END ADDED DEBUG LOG ---

            // Get the reaction source position
            vector sourcePos = reaction.GetSourcePosition();

            // --- BEGIN ADDED DEBUG LOG ---
            Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction (Infantry) - Source Position: " + sourcePos, LogLevel.WARNING);
            // --- END ADDED DEBUG LOG ---

            if (sourcePos != vector.Zero)
            {
                Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction (Infantry) - Source valid, attempting to add S&D order.", LogLevel.WARNING);
                // Log current waypoint count before adding
                array<AIWaypoint> wpsBefore = {};
                int countBefore = 0;
                if(m_group) countBefore = m_group.GetWaypoints(wpsBefore);
                Print("[IA_AI_BEHAVIOR_DEBUG] Waypoint count BEFORE AddOrder: " + countBefore, LogLevel.WARNING);

                RemoveAllOrders(); // Clears existing waypoints
                AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy, true); // Adds S&D waypoint

                // Log current waypoint count after adding
                array<AIWaypoint> wpsAfter = {};
                int countAfter = 0;
                 if(m_group) countAfter = m_group.GetWaypoints(wpsAfter);
                Print("[IA_AI_BEHAVIOR_DEBUG] Waypoint count AFTER AddOrder: " + countAfter, LogLevel.WARNING);
            }
            else
            {
                // --- BEGIN ADDED DEBUG LOG ---
                Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction (Infantry) - Source position is ZERO, cannot add S&D order.", LogLevel.ERROR);
                // --- END ADDED DEBUG LOG ---
            }
        }
        // --- BEGIN ADDED DEBUG LOG ---
        // Add a fallback log just in case something unexpected happens
        else
        {
            string entityInfo;
            if (m_referencedEntity)
                entityInfo = m_referencedEntity.ToString();
            else
                entityInfo = "NULL";
                
            Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction - NEITHER vehicle nor infantry branch executed!? m_isDriving=" + m_isDriving + ", m_referencedEntity=" + entityInfo, LogLevel.ERROR);
        }
        // --- END ADDED DEBUG LOG ---
    }
    
    // Handle reaction when enemy is spotted but not attacking yet
    private void HandleEnemySpottedReaction(IA_AIReactionState reaction)
    {
        Print("[IA_AI_BEHAVIOR_HANDLER] Executing: HandleEnemySpottedReaction | Intensity: " + reaction.GetIntensity(), LogLevel.WARNING); // Added log with intensity

        // For low intensity reactions, just observe
        if (reaction.GetIntensity() < 0.3 && !m_isDriving)
        {
            vector sourcePos = reaction.GetSourcePosition();
            if (sourcePos != vector.Zero)
            {
                RemoveAllOrders();
                AddOrder(sourcePos, IA_AiOrder.Defend);
            }
        }
        // For higher intensity, engage more directly
        else if (!m_isDriving)
        {
            vector sourcePos = reaction.GetSourcePosition();
            if (sourcePos != vector.Zero)
            {
                RemoveAllOrders();
                AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy);
            }
        }
    }
    
    IA_Faction GetFaction()
    {
        //Print("[DEBUG] GetFaction called. Returning: " + m_faction, LogLevel.NORMAL);
        return m_faction;
    }

    // Evaluate the group's state and determine appropriate actions
    private void EvaluateGroupState()
    {
        // Replace ternary operator with if-else
        string isCivilianText;
        if (m_faction == IA_Faction.CIV)
            isCivilianText = "true";
        else
            isCivilianText = "false";
       	if(!m_isCivilian)
        	Print("[IA_NEW_DEBUG] EvaluateGroupState start - Group faction: " + m_faction + 
              ", IsEngaged: " + IsEngagedWithEnemy() +
              ", IsCivilian: " + isCivilianText, 
              LogLevel.WARNING);
          
        // Reset the scheduled flag
        m_isStateEvaluationScheduled = false;
        
        // Skip evaluation if group is not spawned or is null
        if (!IsSpawned() || !m_group)
        {
            // Still schedule the next evaluation to keep the cycle going
            ScheduleNextStateEvaluation();
            return;
        }
        
        // Get current state information
        bool isEngaged = IsEngagedWithEnemy();
        bool isDriving = IsDriving();
        bool hasOrders = HasOrders();
        int timeSinceLastOrder = TimeSinceLastOrder();
        int aliveCount = GetAliveCount();
        
        // Add debug statement showing key state metrics
        Print("[IA_AI_STATE] Group at " + GetOrigin() + " | Units: " + aliveCount + 
              " | Engaged: " + isEngaged + " | Driving: " + isDriving + 
              " | HasOrders: " + hasOrders + " | TimeSinceOrder: " + timeSinceLastOrder + "s" +
              " | DangerLevel: " + m_currentDangerLevel, 
              LogLevel.WARNING);
        
        // --- MODIFIED: Only check active reactions for military units ---
        bool reactionIsActive = false;
        
        // Only process reactions for military units
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_reactionManager)
        {
            // --- BEGIN ADDED DEBUG LOG ---
            // Check reaction state *before* the main conditional block
            IA_AIReactionType currentReactionType = IA_AIReactionType.None;
            reactionIsActive = m_reactionManager.HasActiveReaction();
            if (reactionIsActive)
                currentReactionType = m_reactionManager.GetCurrentReaction().GetReactionType();
            
            Print("[IA_AI_STATE_DEBUG] Checking Reaction State - IsActive: " + reactionIsActive +
                  " | CurrentType: " + typename.EnumToString(IA_AIReactionType, currentReactionType), LogLevel.WARNING);
            // --- END ADDED DEBUG LOG ---
            
            // Check for active reactions first. If one exists, handle it and *stop* this evaluation cycle.
            // This gives the reaction behavior (like S&D from UnderFire) priority.
            if (reactionIsActive)
            {
                IA_AIReactionState currentReaction = m_reactionManager.GetCurrentReaction();
                Print("[IA_AI_STATE] Prioritizing active reaction: " +
                      typename.EnumToString(IA_AIReactionType, currentReaction.GetReactionType()) +
                      " (Intensity: " + currentReaction.GetIntensity() + ")",
                      LogLevel.WARNING);
    
                // Apply the behavior for the current reaction
                ProcessReactions();
    
                // Schedule the next evaluation and exit *now* to let the reaction behavior execute.
                ScheduleNextStateEvaluation();
                return;
            }
        }
        // --- END OF MODIFIED SECTION ---

        // If no active reactions, proceed with other state evaluations:

        // If engaged with enemy (and not driving)
        else if (isEngaged && !isDriving)
        {
            Print("[IA_AI_STATE] Evaluating combat state - Enemy faction: " +
                  typename.EnumToString(IA_Faction, GetEngagedEnemyFaction()),
                  LogLevel.WARNING);

            EvaluateCombatState();
        }
        // If in a vehicle
        else if (isDriving)
        {
            Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
            string vehicleType = "Unknown";
            if (vehicle)
                vehicleType = vehicle.ClassName();

            Print("[IA_AI_STATE] Managing vehicle state - Type: " + vehicleType +
                  " | Destination: " + m_drivingTarget,
                  LogLevel.WARNING);

            UpdateVehicleOrders();
        }
        // Evaluate danger state if not already in another active state
        else if (m_currentDangerLevel > 0.0)
        {
            Print("[IA_AI_STATE] Evaluating danger state - Level: " + m_currentDangerLevel,
                  LogLevel.WARNING);

            EvaluateDangerState();
        }
        // If the group has been idle for too long
        else if (timeSinceLastOrder > 120 && !hasOrders)
        {
            Print("[IA_AI_STATE] Group idle for " + timeSinceLastOrder + "s - Evaluating new activities",
                  LogLevel.WARNING);

            EvaluateIdleState();
        }
        // If none of the above, the group is likely following an old order or just finished one.
        else
        {
            Print("[IA_AI_STATE] No specific state change needed - Maintaining current behavior or order",
                  LogLevel.WARNING);
        }

        // Schedule the next evaluation regardless of the current state (unless already returned)
        ScheduleNextStateEvaluation();
    }

    // Evaluate what to do during combat
    private void EvaluateCombatState()
    {
        // Get reaction status with if-else instead of ternary operator
        string hasReactionText;
        if (m_reactionManager && m_reactionManager.HasActiveReaction())
            hasReactionText = "true";
        else
            hasReactionText = "false";
        
        Print("[IA_NEW_DEBUG] EvaluateCombatState - Group faction: " + m_faction + 
              ", IsEngaged: " + IsEngagedWithEnemy() + 
              ", HasReaction: " + hasReactionText +
              ", AliveCount: " + GetAliveCount() +
              ", HasOrders: " + HasOrders() +
              ", TimeSinceLastOrder: " + TimeSinceLastOrder() + "s", 
              LogLevel.WARNING);
          
        // If the last order was very recent, let it execute
        int timeSinceLastOrder = TimeSinceLastOrder();
        if (timeSinceLastOrder < 3) {
             return;
        }

        // --- AGGRESSIVE MILITARY COMBAT HANDLING ---
        // For military units ONLY - handle combat more aggressively
        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            // Check for danger events first - these need highest priority
            int currentTime = System.GetUnixTime();
            int timeSinceLastDanger = currentTime - m_lastDangerEventTime;

            // --- ENSURE WE HAVE A PROPER ENEMY FACTION ---
            // If we have a danger event but no enemy faction, check if we need to set a default one
            if (m_lastDangerEventTime > 0 && m_engagedEnemyFaction == IA_Faction.NONE)
            {
                // We know there's danger but we couldn't determine the faction - set a sensible default
                if (m_faction == IA_Faction.USSR)
                    m_engagedEnemyFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    m_engagedEnemyFaction = IA_Faction.USSR;

                
                Print("[IA_COMBAT_FACTION] No enemy faction identified, defaulting to: " + 
                      typename.EnumToString(IA_Faction, m_engagedEnemyFaction), LogLevel.WARNING);
            }
            // --- END ENEMY FACTION CHECK ---

            // More aggressive stance: respond to ANY danger in last 60 seconds
            if (m_lastDangerEventTime > 0 && timeSinceLastDanger < 60)
            {
                Print("[IA_COMBAT_ENHANCED] Military unit responding to danger - Time since event: " + 
                      timeSinceLastDanger + "s, DangerType: " + 
                      typename.EnumToString(IA_GroupDangerType, m_lastDangerType), 
                      LogLevel.WARNING);
                
                // Get a target position to attack - prefer last danger position
                vector targetPos = m_lastDangerPosition;
                if (targetPos == vector.Zero)
                {
                    // If no valid danger position, use a position near the group
                    targetPos = IA_Game.rng.GenerateRandomPointInRadius(20, 50, GetOrigin());
                }
                
                // Check if we have a semi-recent order to handle the threat already
                if (timeSinceLastOrder < 30 && HasOrders() && vector.Distance(m_lastOrderPosition, targetPos) < 50)
                {
                    // We have a recent order near the danger point - keep following it
                    Print("[IA_COMBAT_ENHANCED] Military unit already has order near danger point", 
                          LogLevel.WARNING);
                }
                else
                {
                    // Issue a new Search & Destroy order to respond aggressively
                    Print("[IA_COMBAT_ENHANCED] Military unit engaging danger - Issuing S&D at: " + 
                          targetPos, LogLevel.WARNING);
                    
                    RemoveAllOrders();
                    AddOrder(targetPos, IA_AiOrder.SearchAndDestroy, true);
                    
                    // If this unit wasn't already engaged with an enemy, set an appropriate enemy faction
                    if (m_engagedEnemyFaction == IA_Faction.NONE)
                    {
                        if (m_faction == IA_Faction.USSR)
                            m_engagedEnemyFaction = IA_Faction.US;
                        else if (m_faction == IA_Faction.US)
                            m_engagedEnemyFaction = IA_Faction.USSR;
    
                        
                        Print("[IA_COMBAT_ENHANCED] Setting enemy faction to: " + m_engagedEnemyFaction, 
                              LogLevel.WARNING);
                    }
                    
                    // Exit early - we've handled this case
                    return;
                }
            }
            
            // Even if no recent danger, military units should maintain combat readiness
            // if they're already engaged
            if (IsEngagedWithEnemy() && GetAliveCount() > 0)
            {
                // If we haven't issued a new order in a while, issue a new one to maintain pressure
                if (timeSinceLastOrder > 30)
                {
                    // Determine a tactical response based on group strength
                    int aliveCount = GetAliveCount();
                    vector currentPos = GetOrigin();
                    
                    // For stronger groups, be more aggressive
                    if (aliveCount >= 3)
                    {
                        // Generate a search point in the general direction of last danger/order
                        vector searchDir = vector.Zero;
                        
                        // Try to use last danger position as reference
                        if (m_lastDangerPosition != vector.Zero)
                        {
                            searchDir = m_lastDangerPosition - currentPos;
                            if (searchDir.LengthSq() > 0.1)
                                searchDir = searchDir.Normalized();
                            else
                                searchDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();
                        }
                        else
                        {
                            // Use random direction
                            searchDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();
                        }
                        
                        // Generate point 50-100m ahead in that direction
                        vector searchPoint = currentPos + (searchDir * Math.RandomFloat(50, 100));
                        
                        Print("[IA_COMBAT_ENHANCED] Military unit continuing search - Issuing S&D at: " + 
                              searchPoint, LogLevel.WARNING);
                        
                        RemoveAllOrders();
                        AddOrder(searchPoint, IA_AiOrder.SearchAndDestroy, true);
                    }
                    // For weaker groups, be more defensive
                    else
                    {
                        vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, currentPos);
                        
                        Print("[IA_COMBAT_ENHANCED] Military unit (weakened) taking defensive position at: " + 
                              defendPos, LogLevel.WARNING);
                        
                        RemoveAllOrders();
                        AddOrder(defendPos, IA_AiOrder.Defend);
                    }
                    
                    // Exit early - we've handled this case
                    return;
                }
            }
        }
        // --- END AGGRESSIVE MILITARY COMBAT HANDLING ---

        // If we reach here, we're either a civilian or didn't qualify for the military handling above
        // Get information about the engaged enemy
        IA_Faction enemyFaction = GetEngagedEnemyFaction();

        // If group is severely weakened, consider retreating
        int aliveCount = GetAliveCount();
        if (aliveCount <= 2)
        {
            vector retreatDir = GetOrigin() - m_lastOrderPosition;
            if (retreatDir.LengthSq() > 0.1) // Check against small threshold for robustness
                 retreatDir = retreatDir.Normalized();
            else // If last order position is same as current or zero, pick random direction
                 retreatDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();

            vector retreatPos = GetOrigin() + retreatDir * 50; // Retreat 50m

            Print("[IA_AI_STATE] Combat evaluation: Group retreating due to low numbers (" + aliveCount + ")", LogLevel.WARNING);
            RemoveAllOrders();
            AddOrder(retreatPos, IA_AiOrder.Patrol); // Retreat order is Patrol
        }
        // Otherwise, maintain combat stance by defending
        else
        {
            vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, GetOrigin());
            Print("[IA_AI_STATE] Combat evaluation: Group taking defensive position (No recent/active orders)", LogLevel.WARNING);
            RemoveAllOrders();
            AddOrder(defendPos, IA_AiOrder.Defend);
        }

        // Add debug logging to check faction identification
        Print("[IA_AI_COMBAT_DEBUG] EvaluateCombatState: Group faction: " + m_faction + ", Enemy faction: " + enemyFaction + ", IsEngaged: " + IsEngagedWithEnemy(), LogLevel.WARNING);
    }
    
    // Evaluate what to do when idle
    private void EvaluateIdleState()
    {
        // Only take action if we don't already have orders
        if (HasOrders())
            return;
            
        // For civilian units, we might want different behaviors
        if (m_isCivilian)
        {
            // Civilians might wander or find shelter
            vector wanderPos = IA_Game.rng.GenerateRandomPointInRadius(10, 30, GetOrigin());
            AddOrder(wanderPos, IA_AiOrder.Patrol);
            
            Print("[IA_AI_STATE] Idle evaluation: Civilian group wandering", LogLevel.WARNING);
        }
        // For military units, patrol or secure area
        else
        {
            // Choose randomly between patrol and defend
            float randomChoice = Math.RandomFloat(0, 1);
            
            if (randomChoice < 0.7) // 70% chance to patrol
            {
                vector patrolPos = IA_Game.rng.GenerateRandomPointInRadius(20, 50, GetOrigin());
                AddOrder(patrolPos, IA_AiOrder.Patrol);
                
                Print("[IA_AI_STATE] Idle evaluation: Military group patrolling", LogLevel.WARNING);
            }
            else // 30% chance to defend
            {
                vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 15, GetOrigin());
                AddOrder(defendPos, IA_AiOrder.Defend);
                
                Print("[IA_AI_STATE] Idle evaluation: Military group defending position", LogLevel.WARNING);
            }
        }
    }

    private void CheckDangerEvents()
    {
        // Add debug log BEFORE faction check to see if function is being called
        Print("[IA_DEBUG_ESSENTIAL] CheckDangerEvents called for group faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
        
		if(m_faction == IA_Faction.CIV)
		{
            // Add explanation for early return
            Print("[IA_DEBUG_ESSENTIAL] CheckDangerEvents - Skipping for civilian faction", LogLevel.WARNING);
			return;
		}
        
        Print("[IA_DEBUG] CheckDangerEvents processing for faction: " + typename.EnumToString(IA_Faction, m_faction) + 
              ", isCivilian: " + m_isCivilian, LogLevel.WARNING);
        
        if (!m_group)
        {
            Print("[IA_DEBUG_ESSENTIAL] CheckDangerEvents - m_group is null, cannot process", LogLevel.ERROR);
            return;
        }
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        Print("[IA_DEBUG] CheckDangerEvents - Found " + agents.Count() + " agents in group", LogLevel.WARNING);
        
        if (agents.IsEmpty())
        {
            Print("[IA_DEBUG_ESSENTIAL] CheckDangerEvents - No agents found in the group", LogLevel.ERROR);
            return;
        }
        
        foreach (AIAgent agent : agents)
        {
            if (!agent)
            {
                Print("[IA_DEBUG] CheckDangerEvents - Found null agent in group", LogLevel.ERROR);
                continue;
            }
            
            ProcessAgentDangerEvents(agent);
        }
    }
};  // End of IA_AiGroup class
