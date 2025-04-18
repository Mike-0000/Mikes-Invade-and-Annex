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
    private const int MIN_STATE_EVALUATION_INTERVAL = 4000; // milliseconds
    private const int MAX_STATE_EVALUATION_INTERVAL = 12000; // milliseconds
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

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac)
    {
        //Print("[DEBUG] IA_AiGroup constructor called with pos: " + initialPos + ", squad: " + squad + ", faction: " + fac, LogLevel.NORMAL);
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
        m_reactionManager   = new IA_AIReactionManager();
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
	    //Print("[DEBUG] CreateMilitaryGroup called", LogLevel.NORMAL);
	    IA_AiGroup grp = new IA_AiGroup(initialPos,squadType,faction);
	    grp.m_lastOrderPosition = initialPos;
	    grp.m_initialPosition   = initialPos;
	    grp.m_lastOrderTime     = 0;
	    grp.m_squadType         = squadType;
	    grp.m_faction           = faction;
	    grp.m_isCivilian        = false;
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

        //Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Spawning " + unitCount + " individual units for faction " + faction, LogLevel.NORMAL);

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

        // Mark the group as spawned since we manually added units
        grp.m_isSpawned = true;

        // Perform water check for the group's final position
        grp.WaterCheck();

        //Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Finished creating group with " + grp.GetAliveCount() + " units.", LogLevel.NORMAL);
        return grp;
    }

    IA_Faction GetEngagedEnemyFaction()
    {
        //Print("[DEBUG] GetEngagedEnemyFaction called. Returning: " + m_engagedEnemyFaction, LogLevel.NORMAL);
        return m_engagedEnemyFaction;
    }

    bool IsEngagedWithEnemy()
    {
        bool engaged = (m_engagedEnemyFaction != IA_Faction.NONE);
        //Print("[DEBUG] IsEngagedWithEnemy called. Engaged: " + engaged, LogLevel.NORMAL);
        return engaged;
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
    if (!PerformSpawn())
    {
        //Print("[DEBUG] PerformSpawn returned false. Aborting Spawn.", LogLevel.NORMAL);
        return;
    }

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
}


	
	
	
    private bool PerformSpawn()
    {
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
        GetGame().GetCallqueue().CallLater(CheckDangerEvents, 100, true);
        
        //Print("[DEBUG] PerformSpawn completed successfully.", LogLevel.NORMAL);
        return true;
    }

    // Process a danger event at the group level
    void ProcessDangerEvent(IA_GroupDangerType dangerType, vector position, IEntity sourceEntity = null, float intensity = 0.5, bool isSuppressed = false)
    {
        // Force this to always print as WARNING to ensure visibility
        Print("[IA_AI_DANGER] Group at " + GetOrigin() + " processing danger: " + 
              typename.EnumToString(IA_GroupDangerType, dangerType) + 
              " at position " + position, LogLevel.WARNING);
        
        // Skip if group is not spawned
        if (!IsSpawned() || !m_group)
            return;
            
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
                    reactionType = IA_AIReactionType.TakingFire;
                    reactionIntensity = Math.Clamp(1.0 - (distance / BULLET_IMPACT_DISTANCE_MAX), 0.3, 0.8);
                    m_consecutiveDangerEvents++;
                    
                    Print("[IA_AI_DANGER] Projectile impact detected at " + distance + "m", LogLevel.WARNING);
                }
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
                    
                    Print("[IA_AI_DANGER] Weapon fire detected at " + distance + "m (Suppressed: " + isSuppressed + ")", LogLevel.WARNING);
                }
                break;
                
            case IA_GroupDangerType.ProjectileFlyby:
                // Always react to flybys with high priority
                shouldReact = true;
                reactionType = IA_AIReactionType.UnderAttack;
                reactionIntensity = Math.Clamp(0.7 + (intensity * 0.3), 0.7, 1.0);
                m_consecutiveDangerEvents += 2; // Flybys are more threatening
                
                Print("[IA_AI_DANGER] Projectile flyby detected!", LogLevel.WARNING);
                break;
                
            case IA_GroupDangerType.Explosion:
                // React to explosions based on distance
                if (distance <= EXPLOSION_REACT_DISTANCE)
                {
                    shouldReact = true;
                    reactionType = IA_AIReactionType.UnderAttack;
                    // Higher intensity for closer explosions
                    reactionIntensity = Math.Clamp(1.0 - (distance / EXPLOSION_REACT_DISTANCE), 0.5, 1.0);
                    m_consecutiveDangerEvents += 3; // Explosions are very threatening
                    
                    Print("[IA_AI_DANGER] Explosion detected at " + distance + "m", LogLevel.WARNING);
                }
                break;
                
            case IA_GroupDangerType.EnemySpotted:
                // Always react to enemy spotting with variable intensity
                shouldReact = true;
                reactionType = IA_AIReactionType.EnemySpotted;
                // Use provided intensity directly
                m_consecutiveDangerEvents++;
                
                Print("[IA_AI_DANGER] Enemy spotted at " + distance + "m", LogLevel.WARNING);
                break;
        }
        
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
                    if (factionKey == "US")
                        sourceFaction = IA_Faction.US;
                    else if (factionKey == "USSR")
                        sourceFaction = IA_Faction.USSR;
                    else if (factionKey == "CIV")
                        sourceFaction = IA_Faction.CIV;
                }
            }
            
            // Apply danger level as a multiplier to the reaction intensity
            float multiplier = 0.5 + (m_currentDangerLevel * 0.5);
            float finalIntensity = reactionIntensity * multiplier;
            
            // Trigger the reaction in the manager
            m_reactionManager.TriggerReaction(
                reactionType,
                finalIntensity,
                position,
                sourceEntity,
                sourceFaction
            );
            
            Print("[IA_AI_DANGER] Triggering reaction: " + typename.EnumToString(IA_AIReactionType, reactionType) + 
                  " with intensity " + finalIntensity + " (base: " + reactionIntensity + 
                  ", danger level: " + m_currentDangerLevel + ")", LogLevel.WARNING);
        }
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
        ProcessDangerEvent(IA_GroupDangerType.Explosion, explosionPosition, sourceEntity, 0.9);
    }
    
    // Called when the group spots an enemy
    void OnEnemySpotted(vector enemyPosition, IEntity enemyEntity, float threatLevel = 0.5)
    {
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
        if (!m_group)
            return;
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        if (agents.IsEmpty())
        {
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }
        
        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
                continue;
                
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
                continue;
                
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
            
            // Set up danger event processing for this agent
            ProcessAgentDangerEvents(agent);
        }
    }

    // New method to process danger events for an agent
    private void ProcessAgentDangerEvents(AIAgent agent)
    {
        if (!agent)
            return;
            
        int dangerCount = agent.GetDangerEventsCount();
        if (dangerCount <= 0)
            return;
            
        // Process each danger event
        for (int i = 0; i < dangerCount; i++)
        {
            AIDangerEvent dangerEvent = agent.GetDangerEvent();
            if (!dangerEvent)
                continue;
                
            EAIDangerEventType dangerType = dangerEvent.GetDangerType();
            AIAgent senderAgent = dangerEvent.GetSender();
            AIAgent receiverAgent = dangerEvent.GetReceiver();
            vector dangerVector = dangerEvent.GetPosition();
            
            // Map the native danger type to our reaction system
            IA_GroupDangerType mappedDangerType;
            IA_AIReactionType reactionType;
            float intensity = 0.5;
            bool isSuppressed = false;
            
            switch (dangerType)
            {
                case EAIDangerEventType.Danger_ProjectileHit:
                    mappedDangerType = IA_GroupDangerType.ProjectileImpact;
                    reactionType = IA_AIReactionType.TakingFire;
                    intensity = 0.7;
                    break;
                    
                case EAIDangerEventType.Danger_WeaponFire:
                    mappedDangerType = IA_GroupDangerType.WeaponFire;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    intensity = 0.6;
                    break;
                    
                case EAIDangerEventType.Danger_Explosion:
                    mappedDangerType = IA_GroupDangerType.Explosion;
                    reactionType = IA_AIReactionType.UnderAttack;
                    intensity = 0.9;
                    break;
                    
                case EAIDangerEventType.Danger_NewEnemy:
                    mappedDangerType = IA_GroupDangerType.EnemySpotted;
                    reactionType = IA_AIReactionType.EnemySpotted;
                    intensity = 0.8;
                    break;
                    
                case EAIDangerEventType.Danger_DamageTaken:
                    mappedDangerType = IA_GroupDangerType.ProjectileImpact;
                    reactionType = IA_AIReactionType.TakingFire;
                    intensity = 0.6;
                    break;
                    
                default:
                    continue; // Skip other danger types for now
            }
            
            // Get the source entity if available
            IEntity sourceEntity = null;
            if (senderAgent)
            {
                sourceEntity = senderAgent.GetControlledEntity();
            }
            
            // Process the danger event through our existing system
            ProcessDangerEvent(mappedDangerType, dangerVector, sourceEntity, intensity, isSuppressed);
        }
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
        if (!m_reactionManager || !IsSpawned())
            return;
            
        // Process the reactions in the manager
        m_reactionManager.ProcessReactions();
        
        // If there's an active reaction, apply the behavior
        if (m_reactionManager.HasActiveReaction())
        {
            IA_AIReactionState reaction = m_reactionManager.GetCurrentReaction();
            ApplyReactionBehavior(reaction);
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
            //Print("[DEBUG] Member killed by player.", LogLevel.NORMAL);
            
            // Use the new reaction system
            float groupSize = GetAliveCount() + 1; // +1 for the unit that just died
            float intensity = 1.0 / groupSize; // Higher intensity for smaller groups
            
            // Trigger reaction with player as source
            if (m_reactionManager)
            {
                m_reactionManager.TriggerReaction(
                    IA_AIReactionType.GroupMemberKilled, 
                    intensity, 
                    deathPosition, 
                    killerEntity, 
                    IA_Faction.CIV  // Players always treated as CIV faction for now
                );
            }
            
            // Legacy engagement system for compatibility
            if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
            {
                //Print("[IA_AiGroup.OnMemberDeath] Player killed enemy, group engaged with CIV as placeholder");
                m_engagedEnemyFaction = IA_Faction.CIV; 
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

    // Apply behavior changes based on current reaction
    private void ApplyReactionBehavior(IA_AIReactionState reaction)
    {
        // Safety check
        if (!reaction)
            return;
            
        // Apply behavior based on reaction type
        switch (reaction.GetReactionType())
        {
            case IA_AIReactionType.GroupMemberKilled:
                HandleGroupMemberKilledReaction(reaction);
                break;
                
            case IA_AIReactionType.UnderAttack:
                HandleUnderAttackReaction(reaction);
                break;
                
            case IA_AIReactionType.EnemySpotted:
                HandleEnemySpottedReaction(reaction);
                break;
                
            case IA_AIReactionType.TakingFire:
                HandleTakingFireReaction(reaction);
                break;
                
            // More handlers for other reaction types can be added later
        }
    }
    
    // Handle reaction when a group member is killed
    private void HandleGroupMemberKilledReaction(IA_AIReactionState reaction)
    {
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
    private void HandleUnderAttackReaction(IA_AIReactionState reaction)
    {
        // Update legacy engagement state
        IA_Faction sourceFaction = reaction.GetSourceFaction();
        if (sourceFaction != IA_Faction.NONE && !IsEngagedWithEnemy())
        {
            m_engagedEnemyFaction = sourceFaction;
        }
        
        // If we're in a vehicle, we might want to flee
        if (m_isDriving && m_referencedEntity)
        {
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
            // If the reaction source position is known, face that direction
            vector sourcePos = reaction.GetSourcePosition();
            if (sourcePos != vector.Zero)
            {
                RemoveAllOrders();
                AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy, true);
            }
        }
    }
    
    // Handle reaction when enemy is spotted but not attacking yet
    private void HandleEnemySpottedReaction(IA_AIReactionState reaction)
    {
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
    
    // Handle reaction when taking fire but source unknown
    private void HandleTakingFireReaction(IA_AIReactionState reaction)
    {
        // If we're not already engaged, seek cover
        if (!IsEngagedWithEnemy() && !m_isDriving)
        {
            // Move to a nearby position for cover
            vector randomDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();
            vector coverPos = m_group.GetOrigin() + randomDir * 15;
            
            RemoveAllOrders();
            AddOrder(coverPos, IA_AiOrder.Defend);
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
        
        // Check for active reactions first
        if (m_reactionManager && m_reactionManager.HasActiveReaction())
        {
            IA_AIReactionState currentReaction = m_reactionManager.GetCurrentReaction();
            Print("[IA_AI_STATE] Processing active reaction: " + 
                  typename.EnumToString(IA_AIReactionType, currentReaction.GetReactionType()) + 
                  " (Intensity: " + currentReaction.GetIntensity() + ")", 
                  LogLevel.WARNING);
                  
            // If there's an active reaction, let the reaction system handle it
            ProcessReactions();
        }
        // If no active reactions but engaged with enemy, evaluate tactical options
        else if (isEngaged && !isDriving)
        {
            Print("[IA_AI_STATE] Evaluating combat state - Enemy faction: " + 
                  typename.EnumToString(IA_Faction, GetEngagedEnemyFaction()), 
                  LogLevel.WARNING);
                  
            EvaluateCombatState();
        }
        // If in a vehicle, make sure vehicle orders are up to date
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
        // If the group has been idle for too long, consider a new activity
        else if (timeSinceLastOrder > 120 && !hasOrders)
        {
            Print("[IA_AI_STATE] Group idle for " + timeSinceLastOrder + "s - Evaluating new activities", 
                  LogLevel.WARNING);
                  
            EvaluateIdleState();
        }
        else
        {
            Print("[IA_AI_STATE] No state change needed - Maintaining current behavior", 
                  LogLevel.WARNING);
        }
        
        // Schedule the next evaluation regardless of the current state
        ScheduleNextStateEvaluation();
    }

    // Evaluate what to do during combat
    private void EvaluateCombatState()
    {
        // Only take action if we don't already have orders
        if (HasOrders())
            return;
            
        // Get information about the engaged enemy
        IA_Faction enemyFaction = GetEngagedEnemyFaction();
        
        // Determine a tactical response based on group strength, terrain, etc.
        int aliveCount = GetAliveCount();
        
        // If group is severely weakened, consider retreating
        if (aliveCount <= 2)
        {
            // Retreat away from the last order position (likely where enemies are)
            vector retreatDir = GetOrigin() - m_lastOrderPosition;
            retreatDir = retreatDir.Normalized();
            vector retreatPos = GetOrigin() + retreatDir * 50;
            
        RemoveAllOrders();
            AddOrder(retreatPos, IA_AiOrder.Patrol);
            
            Print("[IA_AI_STATE] Combat evaluation: Group retreating due to low numbers", LogLevel.WARNING);
        }
        // Otherwise, maintain combat stance
        else
        {
            // If we don't have orders, set up defensive position
            if (!HasOrders())
            {
                vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, GetOrigin());
                AddOrder(defendPos, IA_AiOrder.Defend);
                
                Print("[IA_AI_STATE] Combat evaluation: Group taking defensive position", LogLevel.WARNING);
            }
        }
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
        if (!m_group)
            return;
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        foreach (AIAgent agent : agents)
        {
            ProcessAgentDangerEvents(agent);
        }
    }
};  // End of IA_AiGroup class
