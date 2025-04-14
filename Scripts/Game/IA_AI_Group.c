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

    // Add a member variable to track last vehicle order update time
    private int m_lastVehicleOrderTime = 0;
    
    // Set minimum time between vehicle order updates (in seconds)
    private static const int VEHICLE_ORDER_UPDATE_INTERVAL = 3;

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac)
    {
        //Print("[DEBUG] IA_AiGroup constructor called with pos: " + initialPos + ", squad: " + squad + ", faction: " + fac, LogLevel.NORMAL);
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
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
            Print("[IA_AiGroup.CreateGroupForVehicle] Failed to get faction manager!", LogLevel.ERROR);
            return "";
        }

        // Get the actual faction for catalog query
        Faction actualFaction;
        if (faction == IA_Faction.US)
            actualFaction = factionManager.GetFactionByKey("US");
        else if (faction == IA_Faction.USSR)
            actualFaction = factionManager.GetFactionByKey("USSR");
        else {
            Print("[IA_AiGroup.CreateGroupForVehicle] Unsupported faction for vehicle crew: " + faction, LogLevel.ERROR);
            return "";
        }

        SCR_Faction scrFaction = SCR_Faction.Cast(actualFaction);
        if (!scrFaction) {
            Print("[IA_AiGroup.CreateGroupForVehicle] Failed to cast to SCR_Faction", LogLevel.ERROR);
            return "";
        }

        // Get the entity catalog from the faction
        SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
        if (!entityCatalog) {
            Print("[IA_AiGroup.CreateGroupForVehicle] Failed to get character catalog", LogLevel.ERROR);
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
            Print("[IA_AiGroup.CreateGroupForVehicle] No character entries found for faction!", LogLevel.ERROR);
            return "";
        }


        return characterEntries[Math.RandomInt(1, characterEntries.Count())].GetPrefab();



    }

	bool HasActiveWaypoint()
	{
		if (!m_group)
			return false;
		
		array<AIWaypoint> wps = {};
		m_group.GetWaypoints(wps);
		
		return !wps.IsEmpty();
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
            Print("[IA_AiGroup.CreateGroupForVehicle] Could not load base group resource!", LogLevel.ERROR);
            return null;
        }
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(spawnPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            Print("[IA_AiGroup.CreateGroupForVehicle] Could not cast spawned entity to SCR_AIGroup!", LogLevel.ERROR);
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
            Print("[IA_AiGroup.CreateGroupForVehicle] Vehicle has no compartment manager!", LogLevel.ERROR);
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
        Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Vehicle has " + compartments.Count() + 
              " total compartments, " + usableCompartments.Count() + " usable compartments", LogLevel.NORMAL);
        
        // Use the smaller of unitCount or usableCompartments to avoid spawning too many units
        int actualUnitsToSpawn = Math.Min(unitCount, usableCompartments.Count());
        Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Spawning " + actualUnitsToSpawn + 
              " individual units for faction " + faction + " (requested: " + unitCount + 
              ", usable compartments: " + usableCompartments.Count() + ")", LogLevel.NORMAL);

        // Spawn individual units and add them to group
        for (int i = 0; i < actualUnitsToSpawn; i++)
        {
			string charPrefabPath = GetRandomUnitPrefab(faction);
			
        	Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Using character prefab: " + charPrefabPath, LogLevel.NORMAL);

            // Spawn the character slightly offset from group position
            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                Print("[IA_AiGroup.CreateGroupForVehicle] Failed to load character resource!", LogLevel.ERROR);
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                Print("[IA_AiGroup.CreateGroupForVehicle] Failed to spawn character entity!", LogLevel.ERROR);
                continue;
            }

            // Add character to the SCR_AIGroup
            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                Print("[IA_AiGroup.CreateGroupForVehicle] Failed to add character " + i + " to group.", LogLevel.ERROR);
                IA_Game.AddEntityToGc(charEntity);
            } else {
                Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Added unit " + i + " to group.", LogLevel.NORMAL);
                grp.SetupDeathListenerForUnit(charEntity);
            }
        }

        // Mark the group as spawned since we manually added units
        grp.m_isSpawned = true;

        // Perform water check for the group's final position
        grp.WaterCheck();

        Print("[DEBUG] IA_AiGroup.CreateGroupForVehicle: Finished creating group with " + grp.GetAliveCount() + " units.", LogLevel.NORMAL);
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
                Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Unsupported faction for military group: " + faction, LogLevel.ERROR);
                return null;
        }

        if (!groupRes) {
            Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Could not load base group resource!", LogLevel.ERROR);
            return null;
        }

        // Spawn the base group entity and adjust its position to ground level
        vector groundPos = initialPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(groundPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Could not cast spawned entity to SCR_AIGroup!", LogLevel.ERROR);
            delete groupEnt; // Clean up the spawned group entity
            return null;
        }

        // Set the origin again after casting just in case
        grp.m_group.SetOrigin(groundPos);

        Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Spawning " + unitCount + " individual units for faction " + faction, LogLevel.NORMAL);

        // Spawn individual units and add them to group
        for (int i = 0; i < unitCount; i++)
        {
            string charPrefabPath = GetRandomUnitPrefab(faction);
            if (charPrefabPath.IsEmpty())
            {
                 Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to get random unit prefab for faction " + faction + ", skipping unit " + i, LogLevel.WARNING);
                 continue;
            }

            Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Using character prefab: " + charPrefabPath, LogLevel.NORMAL);

            // Spawn the character slightly offset from group position
            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to load character resource: " + charPrefabPath, LogLevel.ERROR);
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to spawn character entity!", LogLevel.ERROR);
                continue;
            }

            // Add character to the SCR_AIGroup
            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                Print("[IA_AiGroup.CreateMilitaryGroupFromUnits] Failed to add character " + i + " to group.", LogLevel.ERROR);
                IA_Game.AddEntityToGc(charEntity); // Schedule for deletion if adding failed
            } else {
                Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Added unit " + i + " to group.", LogLevel.NORMAL);
                grp.SetupDeathListenerForUnit(charEntity); // Setup death listener for the added unit
            }
        }

        // Mark the group as spawned since we manually added units
        grp.m_isSpawned = true;

        // Perform water check for the group's final position
        grp.WaterCheck();

        Print("[DEBUG] IA_AiGroup.CreateMilitaryGroupFromUnits: Finished creating group with " + grp.GetAliveCount() + " units.", LogLevel.NORMAL);
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
        //Print("[DEBUG] GetGroupCharacters called.", LogLevel.NORMAL);
        array<SCR_ChimeraCharacter> chars = {};
        if (!m_group || !m_isSpawned)
        {
            //Print("[DEBUG] Group not spawned or m_group is null.", LogLevel.NORMAL);
            return chars;
        }

        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        //Print("[DEBUG] Found " + agents.Count() + " agents in the group.", LogLevel.NORMAL);
        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                //Print("[IA_AiGroup] Cast to SCR_ChimeraCharacter failed!", LogLevel.WARNING);
                continue;
            }
            //Print("[DEBUG] Successfully cast agent to SCR_ChimeraCharacter.", LogLevel.NORMAL);
            chars.Insert(ch);
        }
        return chars;
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
                    Print("[DEBUG_INFANTRY_WAYPOINT] Original waypoint at " + origin + " is outside zone radius (" + zoneRadius + ")", LogLevel.NORMAL);
                    
                    // Generate a random point within the zone instead
                    origin = IA_Game.rng.GenerateRandomPointInRadius(1, zoneRadius * 0.8, zoneOrigin);
                    
                    Print("[DEBUG_INFANTRY_WAYPOINT] Constrained waypoint to zone: " + origin, LogLevel.NORMAL);
                }
            }
            else
            {
                Print("[DEBUG_INFANTRY_WAYPOINT] No marker found near group position " + m_group.GetOrigin() + ", using original waypoint", LogLevel.WARNING);
            }
        }

        // Adjust spawn height based on terrain
        float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
        //Print("[DEBUG] Surface Y calculated: " + y, LogLevel.NORMAL);
        origin[1] = y + 0.5;

        // For vehicle orders, snap to nearest road - EXCEPT GetInVehicle orders which should go directly to the vehicle
        if (m_isDriving && order != IA_AiOrder.GetInVehicle)
        {
            Print("[DEBUG_VEHICLE_WAYPOINT] Finding nearest road point for vehicle order at: " + origin, LogLevel.NORMAL);
            // Get the current active group from the IA_VehicleManager instead of using group ID
            int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
            vector roadOrigin = IA_VehicleManager.FindRandomRoadEntityInZone(origin, 300, currentActiveGroup);
            if (roadOrigin != vector.Zero)
            {
                Print("[DEBUG_VEHICLE_WAYPOINT] Road point found at: " + roadOrigin + ", distance from original: " + vector.Distance(origin, roadOrigin), LogLevel.NORMAL);
                origin = roadOrigin;
                //Print("[DEBUG] Vehicle order snapped to road at: " + origin, LogLevel.NORMAL);
            }
            else
            {
                Print("[DEBUG_VEHICLE_WAYPOINT] No road found near vehicle order, using original position", LogLevel.WARNING);
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
            Print("[DEBUG_VEHICLE_WAYPOINT] Set waypoint priority level to 2000 for vehicle order", LogLevel.NORMAL);
            //Print("[DEBUG] Set waypoint priority level to 2000.", LogLevel.NORMAL);
        }

        m_group.AddWaypoint(w);
        //Print("[DEBUG] Waypoint added to group.", LogLevel.NORMAL);
        m_lastOrderPosition = origin;
        m_lastOrderTime     = System.GetUnixTime();
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
        Print("[DEBUG_VEHICLE_LOGIC] IsDriving() called, returning: " + m_isDriving + ", referenced entity: " + m_referencedEntity, LogLevel.NORMAL);
        return m_isDriving;
    }

    void RemoveAllOrders(bool resetLastOrderTime = false)
    {
        if (!m_group)
            return;
        
        Print("[DEBUG_VEHICLE_TRACKING] RemoveAllOrders called, resetTime: " + resetLastOrderTime + ", isDriving: " + m_isDriving + ", entity: " + m_referencedEntity, LogLevel.NORMAL);
        
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
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
                    Print("[DEBUG_VEHICLE_TRACKING] Resetting driving state for vehicle group as requested", LogLevel.WARNING);
                    // This is a valid vehicle group and resetLastOrderTime was explicitly requested
                    // Use ClearVehicleReference to properly clean up
                    ClearVehicleReference();
                }
                else
                {
                    Print("[DEBUG_VEHICLE_TRACKING] Found inconsistency: IsDriving=true but invalid vehicle reference", LogLevel.ERROR);
                    // If referenced entity is not a vehicle, clear the state
                    m_isDriving = false;
                    m_referencedEntity = null;
                }
            }
            else if (resetLastOrderTime)
            {
                // Regular group, just clear order time
                Print("[DEBUG_VEHICLE_TRACKING] Resetting order time for infantry group", LogLevel.NORMAL);
            }
        }
        else
        {
            // Preserve driving state when just clearing waypoints for new orders
            Print("[DEBUG_VEHICLE_TRACKING] Preserving driving state: " + m_isDriving, LogLevel.NORMAL);
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
        // For infantry (not driving), constrain to a specific zone rather than the entire group area
        if (!m_isDriving)
        {
                posToUse = IA_Game.rng.GenerateRandomPointInRadius(5, 25, m_initialPosition);
            
        }
        else
        {
            // For vehicles, use the original behavior
            posToUse = IA_Game.rng.GenerateRandomPointInRadius(5, 25, m_initialPosition);
        }
    }

    AddOrder(posToUse, initialOrder);
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
        //Print("[DEBUG] PerformSpawn completed successfully.", LogLevel.NORMAL);
        return true;
    }

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
            Print("[DEBUG_VEHICLE_TRACKING] Clearing vehicle state during despawn", LogLevel.NORMAL);
            ClearVehicleReference();
        }

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

    IA_Faction GetFaction()
    {
        //Print("[DEBUG] GetFaction called. Returning: " + m_faction, LogLevel.NORMAL);
        return m_faction;
    }

    // Public accessor for the internal SCR_AIGroup
    SCR_AIGroup GetAIGroup()
    {
        return m_group;
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

    private void SetupDeathListener()
    {
        //Print("[DEBUG] SetupDeathListener called.", LogLevel.NORMAL);
        if (!m_group)
        {
            //Print("[DEBUG] m_group is null in SetupDeathListener.", LogLevel.NORMAL);
            return;
        }
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);

        if (agents.IsEmpty())
        {
            //Print("[DEBUG] No agents found. Retrying SetupDeathListener in 1000 ms.", LogLevel.NORMAL);
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }

        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                //Print("[DEBUG] Agent does not have a valid SCR_ChimeraCharacter.", LogLevel.NORMAL);
                continue;
            }
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
            {
                //Print("[IA_AiGroup.SetupDeathListener] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
                continue;
            }
            //Print("[DEBUG] Inserting death callback for agent.", LogLevel.NORMAL);
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
        }
    }

    // Helper to set up death listener for a single unit (used by CreateGroupForVehicle)
    private void SetupDeathListenerForUnit(IEntity unitEntity)
    {
         SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(unitEntity);
         if (!ch) return;

         SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
         if (!ccc) {
             Print("[IA_AiGroup.SetupDeathListenerForUnit] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
             return;
         }
         ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
    }

    private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {
        //Print("[DEBUG] OnMemberDeath called.", LogLevel.NORMAL);
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
            //Print("[DEBUG] Member killed by player.", LogLevel.NORMAL);
            // If a player kills one of our guys and we're not yet "engaged," 
            // set some placeholder for engaged faction (CIV is used as a placeholder).
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
            }
            else
            {
                string fKey = c.GetFaction().GetFactionKey();
                //Print("[DEBUG] Killer faction key: " + fKey, LogLevel.NORMAL);
                if (fKey == "US")
                {
                    m_engagedEnemyFaction = IA_Faction.US;
                }
                else if (fKey == "USSR")
                {
                    m_engagedEnemyFaction = IA_Faction.USSR;
                }
                else if (fKey == "CIV")
                {
                    m_engagedEnemyFaction = IA_Faction.CIV;
                }
                else
                {
                    //Print("[IA_AiGroup.OnMemberDeath] unknown faction key " + fKey, LogLevel.WARNING);
                }
                // If friendly fire, reset engagement.
                if (m_engagedEnemyFaction == m_faction)
                {
                    //Print("[IA_AiGroup.OnMemberDeath] Friendly kill => resetting engagement");
                    m_engagedEnemyFaction = IA_Faction.NONE;
                }
            }
        }
    }

    private void WaterCheck()
    {
        //Print("[DEBUG] WaterCheck called (placeholder).", LogLevel.NORMAL);
        // No extra water check here because safe areas are pre-defined.
    }

    void AssignVehicle(Vehicle vehicle, vector destination)
    {
        Print("[DEBUG_VEHICLE_LOGIC] AssignVehicle called for group, vehicle: " + vehicle + ", destination: " + destination, LogLevel.NORMAL);
        Print("[DEBUG_VEHICLE_LOGIC] Initial state - m_isDriving: " + m_isDriving + ", m_referencedEntity: " + m_referencedEntity, LogLevel.NORMAL);
        
        if (!vehicle || !IsSpawned())
        {
            Print("[DEBUG_VEHICLE_LOGIC] AssignVehicle early return: vehicle valid: " + (!!vehicle) + ", isSpawned: " + IsSpawned(), LogLevel.WARNING);
            return;
        }
            
        // Try to reserve the vehicle - if we can't, don't proceed
        Print("[DEBUG_VEHICLE_LOGIC] Attempting to reserve vehicle with IA_VehicleManager", LogLevel.NORMAL);
        bool reserved = IA_VehicleManager.ReserveVehicle(vehicle, this);
        Print("[DEBUG_VEHICLE_LOGIC] Vehicle reservation result: " + reserved, LogLevel.NORMAL);
        
        if (!reserved)
        {
            Print("[DEBUG_VEHICLE_LOGIC] Failed to reserve vehicle, aborting assignment", LogLevel.WARNING);
            return;
        }
            
        Print("[DEBUG_VEHICLE_LOGIC] Setting vehicle state variables", LogLevel.NORMAL);
        m_referencedEntity = vehicle;
        m_isDriving = true;
        m_drivingTarget = destination;
        
        Print("[DEBUG_VEHICLE_LOGIC] Vehicle state after assignment - m_isDriving: " + m_isDriving + ", m_referencedEntity: " + m_referencedEntity, LogLevel.NORMAL);
        
        // Update the last order position to the vehicle's position so we can track when we've reached it
        m_lastOrderPosition = vehicle.GetOrigin();
        m_lastOrderTime = System.GetUnixTime();
        
        // Clear any existing orders
        RemoveAllOrders();
        
        // NOTE: No longer adding GetInVehicle order here since units are teleported directly into the vehicle
        // by the IA_VehicleManager._PlaceSpawnedUnitsInVehicle method
        
        Print("[DEBUG] IA_AiGroup.AssignVehicle: Vehicle assigned with destination: " + destination.ToString(), LogLevel.NORMAL);
    }
    
    void UpdateVehicleOrders()
    {
        // Only update vehicle orders at certain intervals
        int currentTime = System.GetUnixTime();
        int timeSinceLastVehicleUpdate = currentTime - m_lastVehicleOrderTime;
        
        // Skip frequent updates, only do this check every few seconds
        if (timeSinceLastVehicleUpdate < VEHICLE_ORDER_UPDATE_INTERVAL)
            return;
        
        m_lastVehicleOrderTime = currentTime;
        
        // VALIDATION: If we have IsDriving but no referenced entity, clear the state
        if (m_isDriving && !m_referencedEntity)
        {
            Print("[DEBUG_VEHICLE_REPAIR] Found inconsistency: IsDriving=true but no referenced entity. Clearing driving state.", LogLevel.WARNING);
            m_isDriving = false;
            return;
        }
        
        if (!m_isDriving || !m_referencedEntity)
        {
            Print("[DEBUG_VEHICLE_LOGIC] UpdateVehicleOrders early return: m_isDriving=" + m_isDriving + ", m_referencedEntity=" + m_referencedEntity, LogLevel.NORMAL);
            return;
        }
        
        Print("[DEBUG_VEHICLE_LOGIC] UpdateVehicleOrders processing vehicle orders", LogLevel.NORMAL);
        
        Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
        if (!vehicle)
        {
            // Invalid vehicle reference, reset state
            Print("[DEBUG_VEHICLE_LOGIC] Failed to cast m_referencedEntity to Vehicle, resetting driving state", LogLevel.ERROR);
            ClearVehicleReference(); // Use the proper method to clean up
            return;
        }
        
        // Reset vehicle waypoints after 1 minute - this helps prevent AI getting stuck
        int timeSinceLastOrder = TimeSinceLastOrder();
        if (timeSinceLastOrder >= 60)
        {
            Print("[DEBUG_VEHICLE_LOGIC] Resetting vehicle waypoints after timeout", LogLevel.NORMAL);
            RemoveAllOrders(false);
            // Find nearest road point for the destination before adding order
            Print("[DEBUG_VEHICLE_WAYPOINT] Resetting vehicle waypoint after timeout. Original target: " + m_drivingTarget, LogLevel.NORMAL);
            // Get the current active group from the IA_VehicleManager instead of using group ID
            int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
            vector roadTarget = IA_VehicleManager.FindRandomRoadEntityInZone(m_drivingTarget, 300, currentActiveGroup);
            if (roadTarget == vector.Zero) // Fallback if no road found
            {
                roadTarget = m_drivingTarget;
                Print("[DEBUG_VEHICLE_WAYPOINT] No road found near target, using original position", LogLevel.WARNING);
            }
            else
            {
                Print("[DEBUG_VEHICLE_WAYPOINT] Found road point at: " + roadTarget + ", distance from original: " + vector.Distance(m_drivingTarget, roadTarget), LogLevel.NORMAL);
            }
            m_lastOrderPosition = roadTarget; 
            AddOrder(roadTarget, IA_AiOrder.Move, true);
            Print("[DEBUG] IA_AiGroup.UpdateVehicleOrders: Resetting vehicle waypoint after timeout. New target: " + roadTarget, LogLevel.NORMAL);
            return;
        }
        
        array<SCR_ChimeraCharacter> chars = GetGroupCharacters();
        if (chars.IsEmpty())
        {
            Print("[DEBUG_VEHICLE_LOGIC] No characters in group, skipping vehicle orders", LogLevel.WARNING);
            return;
        }
        
        SCR_ChimeraCharacter driver = chars[0];
        Vehicle currentVehicle = IA_VehicleManager.GetCharacterVehicle(driver);
        
        Print("[DEBUG_VEHICLE_LOGIC] Driver current vehicle: " + currentVehicle + ", target vehicle: " + vehicle, LogLevel.NORMAL);
       /* 
        // CASE 1: Driver is not in any vehicle yet
        // For units that were teleported, this shouldn't happen during normal operations,
        // but it can happen if the driver gets out or is ejected
        if (!currentVehicle)
        {
            // We'll still add the GetInVehicle order in this case as a recovery mechanism
            Print("[DEBUG_VEHICLE_LOGIC] Driver not in vehicle, setting GetInVehicle order (recovery mechanism)", LogLevel.NORMAL);
            RemoveAllOrders();
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            Print("[DEBUG] IA_AiGroup.UpdateVehicleOrders: Driver not in vehicle, setting GetInVehicle recovery order", LogLevel.NORMAL);
            return;
        }
        
        // CASE 2: Driver is in the wrong vehicle
        if (currentVehicle != vehicle)
        {
            // This is unexpected since we teleport directly to the correct vehicle
            Print("[DEBUG_VEHICLE_LOGIC] Driver in wrong vehicle, setting GetInVehicle order (recovery mechanism)", LogLevel.WARNING);
            RemoveAllOrders();
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            Print("[DEBUG] IA_AiGroup.UpdateVehicleOrders: Driver in wrong vehicle, setting GetInVehicle recovery order", LogLevel.NORMAL);
            return;
        }
        */
        // CASE 3: Driver is in the correct vehicle but not at destination
        float distanceToDest = vector.Distance(vehicle.GetOrigin(), m_drivingTarget);
        Print("[DEBUG_VEHICLE_LOGIC] Driver in correct vehicle, distance to destination: " + distanceToDest, LogLevel.NORMAL);
        if (distanceToDest > 15)
        {
            // If no active waypoint, create one to the destination, snapped to road
            if (!HasOrders())
            {
                Print("[DEBUG_VEHICLE_LOGIC] No active waypoint, creating new one", LogLevel.NORMAL);
                RemoveAllOrders();
                // Always ensure target is on a road for vehicles
                Print("[DEBUG_VEHICLE_WAYPOINT] Creating new waypoint for vehicle. Original target: " + m_drivingTarget, LogLevel.NORMAL);
                // Get the current active group from the IA_VehicleManager instead of using group ID
                int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
                vector roadTarget = IA_VehicleManager.FindRandomRoadEntityInZone(m_drivingTarget, 300, currentActiveGroup);
                if (roadTarget == vector.Zero) // Fallback if no road found
                {
                    roadTarget = m_drivingTarget;
                    Print("[DEBUG_VEHICLE_WAYPOINT] No road found near target, using original position", LogLevel.WARNING);
                }
                else
                {
                    Print("[DEBUG_VEHICLE_WAYPOINT] Found road point at: " + roadTarget + ", distance from original: " + vector.Distance(m_drivingTarget, roadTarget), LogLevel.NORMAL);
                }
                m_lastOrderPosition = roadTarget; 
                // Explicitly use Move order for vehicles, never Defend
                AddOrder(roadTarget, IA_AiOrder.Move, true);
                Print("[DEBUG] IA_AiGroup.UpdateVehicleOrders: Setting new Move order to road target: " + roadTarget, LogLevel.NORMAL);
            }
            return;
        }
        
        // CASE 4: Reached destination, get out
        Print("[DEBUG_VEHICLE_LOGIC] Vehicle reached CASE 4", LogLevel.ERROR);
        RemoveAllOrders();
        //AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetOutOfVehicle);
        
        // Use proper cleanup method instead of directly setting member variables
        //IA_VehicleManager.ReleaseVehicleReservation(vehicle);
        //ClearVehicleReference();
        
        //Print("[DEBUG] IA_AiGroup.UpdateVehicleOrders: Vehicle reached destination, setting GetOutOfVehicle order", LogLevel.NORMAL);
    }
    
    // Manual check for completion based on vehicle position
    bool IsVehicleNearWaypoint(Vehicle vehicle, vector waypointPos)
    {
        if (!vehicle)
            return false;
            
        vector vehiclePos = vehicle.GetOrigin();
        float distance = vector.Distance(vehiclePos, waypointPos);
        
        // Be generous with distance check 
        return distance < 15;
    }

    string GetStateDescription()
    {
        if (!IsSpawned())
            return "Despawned";
            
        if (IsDriving())
        {
            vector target = GetDrivingTarget();
            // Format vector coordinates manually for printing
            return "Driving Vehicle to <" + target[0] + ", " + target[1] + ", " + target[2] + ">"; 
        }
            
        if (IsEngagedWithEnemy())
            return "Engaged with Faction: " + typename.EnumToString(IA_Faction, GetEngagedEnemyFaction());
            
        if (HasOrders())
            return "Executing Order";
            
        // Check for idleness only if not driving, engaged, or having active orders
        // Consider a group idle if the last order was given more than 60 seconds ago
        if (TimeSinceLastOrder() > 60) 
            return "Idle";
            
        // Default state if spawned but not driving, engaged, ordered, or explicitly idle
        return "Active"; 
    }

    // Add a method to help debug the m_isDriving field
    void ForceDrivingState(bool state)
    {
        Print("[DEBUG_VEHICLE_LOGIC] ForceDrivingState called, changing from " + m_isDriving + " to " + state, LogLevel.NORMAL);
        m_isDriving = state;
    }

    // Add a method to properly reset vehicle state
    void ClearVehicleReference()
    {
        Print("[DEBUG_VEHICLE_LOGIC] ClearVehicleReference called. Previous state: isDriving=" + m_isDriving + ", entity=" + m_referencedEntity, LogLevel.WARNING);
        
        // If there was a vehicle reservation, release it
        if (m_referencedEntity)
        {
            Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
            if (vehicle)
            {
                IA_VehicleManager.ReleaseVehicleReservation(vehicle);
                Print("[DEBUG_VEHICLE_LOGIC] Released vehicle reservation for " + vehicle, LogLevel.NORMAL);
            }
        }
        
        // Reset vehicle-related state variables
        m_isDriving = false;
        m_referencedEntity = null;
        m_drivingTarget = vector.Zero;
        Print("[DEBUG_VEHICLE_LOGIC] Vehicle state has been cleared", LogLevel.NORMAL);
    }

    // Add this method to handle waypoint reached callbacks
    void OnWaypointReached(AIWaypoint wp)
    {
        Print("[DEBUG_VEHICLE_TRACKING] OnWaypointReached called for waypoint: " + wp, LogLevel.NORMAL);
        // Handle any special logic when a waypoint is reached
    }
};