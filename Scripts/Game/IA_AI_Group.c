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
    InVehicle       // Group is currently assigned to a vehicle
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

class IA_AiGroup
{
    private SCR_AIGroup m_group;
    private bool        m_isSpawned = false;
    private bool        m_isCivilian = false;
    private IA_SquadType m_squadType;
    private IA_Faction   m_faction;
    
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
    private const float SUPPRESSED_AUDIBLE_DISTANCE = 100.0;
    private const float EXPLOSION_REACT_DISTANCE = 220.0;
    
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
    private const int MIN_STATE_EVALUATION_INTERVAL = 10000; // milliseconds, increased from 5000
    private const int MAX_STATE_EVALUATION_INTERVAL = 25000; // milliseconds, increased from 15000
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
    private static const int AGENT_EVENT_CHECK_INTERVAL_MS = 300; // Check agent events every 300ms
    private int m_lastAgentEventCheckTime = 0;
    private static const int DANGER_PROCESS_INTERVAL_MS = 150; // Process at most one danger event every 150ms
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

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac, int unitCount)
    {
        m_initialPosition = initialPos;
        m_squadType = squad;
        m_faction = fac;
        m_initialUnitCount = unitCount; // Store initial count passed as argument
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
        // Redirect to CreateMilitaryGroupFromUnits instead of using prefabs
        // Calculate unit count based on squad type
        int unitCount = IA_SquadCount(squadType, faction);
        
        // Create group using the new unit-based approach
        return CreateMilitaryGroupFromUnits(initialPos, faction, unitCount);
	}

	static string GetRandomUnitPrefab(IA_Faction faction){

         SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) {
            return "";
        }

        Faction actualFaction;
        if (faction == IA_Faction.US)
            actualFaction = factionManager.GetFactionByKey("US");
        else if (faction == IA_Faction.USSR)
            actualFaction = factionManager.GetFactionByKey("USSR");
        else {
            return "";
        }

        SCR_Faction scrFaction = SCR_Faction.Cast(actualFaction);
        if (!scrFaction) {
            return "";
        }

        SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
        if (!entityCatalog) {
            return "";
        }

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

		else if (faction == IA_Faction.FIA)
		{

		    includedLabels = {EEditableEntityLabel.ROLE_RIFLEMAN};

		}

		array<EEditableEntityLabel> excludedLabels = {};
        entityCatalog.GetFullFilteredEntityList(characterEntries, includedLabels, excludedLabels);
        if (characterEntries.IsEmpty()) {
            return "";
        }


        return characterEntries[Math.RandomInt(1, characterEntries.Count())].GetPrefab();



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

    static IA_AiGroup CreateGroupForVehicle(Vehicle vehicle, IA_Faction faction, int unitCount)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // Spawn slightly above vehicle origin

        IA_AiGroup grp = new IA_AiGroup(spawnPos, IA_SquadType.Riflemen, faction, unitCount);
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

        for (int i = 0; i < actualUnitsToSpawn; i++)
        {
			string charPrefabPath = GetRandomUnitPrefab(faction);
			

            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                continue;
            }

            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                IA_Game.AddEntityToGc(charEntity);
            } else {
                grp.SetupDeathListenerForUnit(charEntity);
            }
        }

        grp.m_isSpawned = true;

        grp.WaterCheck();

        // Initialize tactical state and schedule evaluation
        grp.SetTacticalState(IA_GroupTacticalState.DefendPatrol, groundPos);
        grp.ScheduleNextStateEvaluation();
        grp.SetupDeathListener(); // Ensure CheckDangerEvents is scheduled

        return grp;
    }

    static IA_AiGroup CreateMilitaryGroupFromUnits(vector initialPos, IA_Faction faction, int unitCount)
    {
        if (unitCount <= 0)
            return null;

        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, faction, unitCount);
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
            return null;
        }

        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(initialPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            delete groupEnt;
            return null;
        }

        vector groundPos = initialPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        grp.m_group.SetOrigin(groundPos);

        for (int i = 0; i < unitCount; i++)
        {
            string charPrefabPath = GetRandomUnitPrefab(faction);
            if (charPrefabPath.IsEmpty())
            {
                 continue;
            }

            vector unitSpawnPos = groundPos + IA_Game.rng.GenerateRandomPointInRadius(1, 3, vector.Zero);
            Resource charRes = Resource.Load(charPrefabPath);
            if (!charRes) {
                continue;
            }

            IEntity charEntity = GetGame().SpawnEntityPrefab(charRes, null, IA_CreateSimpleSpawnParams(unitSpawnPos));
            if (!charEntity) {
                continue;
            }

            if (!grp.m_group.AddAIEntityToGroup(charEntity)) {
                IA_Game.AddEntityToGc(charEntity); // Schedule for deletion if adding failed
            } else {
                grp.SetupDeathListenerForUnit(charEntity); // Setup death listener for the added unit
            }
        }

        // Instead of using Spawn directly, we'll handle initialization here
        grp.PerformSpawn();
        
        // Explicitly set a default tactical state for the new group
        if (initialPos != vector.Zero)
        {
            grp.SetTacticalState(IA_GroupTacticalState.DefendPatrol, initialPos, null, true); // Added authority flag
        }
        else
        {
            vector patrolPos = IA_Game.rng.GenerateRandomPointInRadius(10, 50, grp.GetOrigin());
            grp.SetTacticalState(IA_GroupTacticalState.DefendPatrol, patrolPos, null, true); // Added authority flag
        }

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
        
        //Print("[IA_NEW_DEBUG] IsEngagedWithEnemy called. Group faction: " + m_faction + ", Result: " + result, LogLevel.WARNING);
        return result;
    }

    void MarkAsUnengaged()
    {
        ////Print("[DEBUG] MarkAsUnengaged called. Resetting engaged enemy faction.", LogLevel.NORMAL);
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
        m_group.AddWaypoint(waypoint);
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

        // Restrict infantry waypoints to a single zone, while vehicle waypoints can navigate across the zone group
        if (!m_isDriving && order != IA_AiOrder.GetInVehicle)
        {
            // Get the current group number from VehicleManager's active group
            int currentGroupNumber = IA_VehicleManager.GetActiveGroup();
            
            // Find the closest zone to the current group position
            IA_AreaMarker closestMarker = IA_AreaMarker.GetMarkerAtPosition(m_group.GetOrigin());
            
            if (closestMarker)
            {
                float zoneRadius = closestMarker.GetRadius();
                vector zoneOrigin = closestMarker.GetZoneCenter();
                
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
        if (order == IA_AiOrder.Defend) 
        {
            // --- BEGIN ADDED: Debug logging for direct defend orders ---
            Print(string.Format("[IA_AiGroup.AddOrder] DIRECT DEFEND ORDER DEBUG: Group %1 | Faction: %2 | AliveCount: %3 | Current Pos: %4 | Target: %5 | ScriptLine: 330", 
                this, typename.EnumToString(IA_Faction, m_faction), GetAliveCount(), GetOrigin().ToString(), origin.ToString()), LogLevel.NORMAL);
            // --- END ADDED ---
            
            rname = "{D9C14ECEC9772CC6}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Defend.et";
        }
        else
        {
            rname = IA_AiOrderResource(order);
        }
        
        // --- BEGIN ADDED LOGGING ---
        Print(string.Format("[IA_Waypoint DEBUG AddOrder] Group %1 | Order: %2 | Waypoint Resource: %3 | Target: %4", 
            m_group, typename.EnumToString(IA_AiOrder, order), rname, origin), LogLevel.DEBUG);
        // --- END ADDED LOGGING ---
        
        Resource res = Resource.Load(rname);
        if (!res)
        {
            // --- BEGIN ADDED LOGGING ---
            Print(string.Format("[IA_Waypoint] Failed to load waypoint resource: %1 for Group %2", rname, m_group), LogLevel.ERROR);
            // --- END ADDED LOGGING ---
            return;
        }

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
		
        if (!w)
        {
            // --- BEGIN MODIFIED LOGGING ---
            string entStr = "null";
            if (waypointEnt)
                entStr = waypointEnt.ToString();
            Print(string.Format("[IA_Waypoint] Failed to cast spawned entity to SCR_AIWaypoint. Resource: %1, Origin: %2, Entity: %3 for Group %4", rname, origin, entStr, m_group), LogLevel.ERROR);
            // --- END MODIFIED LOGGING ---
            if (waypointEnt)
                IA_Game.AddEntityToGc(waypointEnt);
            return;
        }
		
        if (m_isDriving)
        {
            w.SetPriorityLevel(40); // Keep vehicles as is
        }
        else
        {
            // Dynamic priority for infantry based on situation
            int priorityLevel = CalculateWaypointPriority(order);
            w.SetPriorityLevel(priorityLevel);
        }

        // --- BEGIN ADDED: Additional logging before adding waypoint to group ---
        if (order == IA_AiOrder.Defend)
        {
            Print(string.Format("[IA_Waypoint] PRE-ADD DEFEND WAYPOINT: Group %1 | Waypoint %2 | Position %3 | Priority %4", 
                m_group, w, w.GetOrigin().ToString(), w.GetPriorityLevel()), LogLevel.NORMAL);
            
            // Add additional group state info
            array<AIWaypoint> existingWaypoints = {};
            m_group.GetWaypoints(existingWaypoints);
            Print(string.Format("[IA_Waypoint] GROUP STATE BEFORE DEFEND: Existing Waypoints: %1", 
                existingWaypoints.Count()), LogLevel.NORMAL);
        }
        // --- END ADDED ---

        m_group.AddWaypoint(w);
        
        // --- BEGIN ADDED LOGGING ---
        Print(string.Format("[IA_Waypoint] Added Order: %1 at %2 for Group %3", typename.EnumToString(IA_AiOrder, order), origin, m_group), LogLevel.WARNING);
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
            Print(string.Format("[RemoveAllOrders] Resetting flanking state for Group %1", m_group), LogLevel.NORMAL);
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
            return false;
        }
        m_isSpawned = true;

        // Skip the old prefab spawning logic for military groups since we're using CreateMilitaryGroupFromUnits
        // which already creates and populates the group with individual units

        // Only handle civilian groups or if the group was manually created without units
        if (m_isCivilian || !m_group)
        {
            string resourceName;
            if (m_isCivilian)
            {
                resourceName = IA_RandomCivilianResourceName();
                
                Resource res = Resource.Load(resourceName);
                if (!res)
                {
                    return false;
                }
                
                IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
                
                Resource groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
                IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
                m_group = SCR_AIGroup.Cast(groupEnt);
            }
            
            if (m_group)
            {
                vector groundPos = m_initialPosition;
                float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
                groundPos[1] = groundY;
                
                m_group.SetOrigin(groundPos);
                m_lastConfirmedPosition = groundPos;
            }
            
            if (!m_group)
            {
                return false;
            }
        }

        SetupDeathListener();
        WaterCheck();
        
        ScheduleNextStateEvaluation();

        GetGame().GetCallqueue().CallLater(CheckDangerEvents, 250, true);

        return true;
    }

    // Process a danger event at the group level
    void ProcessDangerEvent(IA_GroupDangerType dangerType, vector position, IEntity sourceEntity = null, float intensity = 0.5, bool isSuppressed = false)
    {
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
                        return;
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
                    
                }
            }
            
            // --- MODIFIED CHECK: Skip if source faction is unknown OR same as own faction ---
            if (sourceFaction == IA_Faction.NONE || sourceFaction == m_faction)
                return; // Ignore unknown or friendly sources
            // --- END MODIFIED CHECK ---
                
            // If we identified a valid enemy faction, set it (we know it's not NONE and not m_faction here)
            m_engagedEnemyFaction = sourceFaction;
        }
        
        // Update last danger info
        m_lastDangerEventTime = System.GetUnixTime();
        m_lastDangerPosition = position;
        m_consecutiveDangerEvents++;
        
        // Create a danger event for later processing
        IA_GroupDangerEvent dangerEvent = new IA_GroupDangerEvent();
        dangerEvent.m_type = dangerType;
        dangerEvent.m_position = position;
        dangerEvent.m_source = sourceEntity;
        dangerEvent.m_intensity = intensity;
        dangerEvent.m_isSuppressed = isSuppressed;
        dangerEvent.m_time = m_lastDangerEventTime;
        
        // Add to queue
        m_dangerEvents.Insert(dangerEvent);
        
        // Update danger level
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
        vector weaponFireCenterSum = vector.Zero;
        float weaponFireTotalWeight = 0.0;
        
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
            if (dangerEvent.m_type == IA_GroupDangerType.WeaponFire)
            {
                weaponFireCenterSum += dangerEvent.m_position * weight;
                weaponFireTotalWeight += weight;
            }
            
            // Skip ProjectileImpact for position calculation but still count for danger level
            if (dangerEvent.m_type != IA_GroupDangerType.ProjectileImpact)
            {
                // Add to weighted position calculation
                dangerCenterSum += dangerEvent.m_position * weight;
                totalWeight += weight;
            }
            
            // Update consecutive events for compatibility with old system
            m_consecutiveDangerEvents++;
        }
        
        // Normalize the danger level (cap at 1.0)
        dangerLevel = Math.Clamp(dangerLevel, 0.0, 1.0);
        
        // Prioritize weapon fire events for danger center position
        if (weaponFireTotalWeight > 0.1)
        {
            m_lastDangerPosition = weaponFireCenterSum / weaponFireTotalWeight;
        }
        // Fall back to other events (excluding ProjectileImpact) if no weapon fire events
        else if (totalWeight > 0.1)
        {
            m_lastDangerPosition = dangerCenterSum / totalWeight;
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
             ////Print("[IA_AiGroup.SetupDeathListenerForUnit] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
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
                
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
            
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
        
        // Skip for civilians
        if (m_isCivilian || m_faction == IA_Faction.CIV)
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

    private void WaterCheck()
    {

    }
    
    void UpdateVehicleOrders()
    {
        if (!m_isDriving || !m_referencedEntity)
        {
            return;
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

            ClearVehicleReference();
            return;
        }
        
        array<SCR_ChimeraCharacter> characters = GetGroupCharacters();
        if (characters.IsEmpty())
        {
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
            
            RemoveAllOrders();
            
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            
            m_isDriving = true;
            
            return;
        }
        
        // If everyone is in the vehicle and we have a destination, make sure we have a waypoint
        if (!anyOutside && m_drivingTarget != vector.Zero)
        {
            // Check if we need a new waypoint (don't have one or reached destination)
            bool hasActiveWP = HasActiveWaypoint();
            bool vehicleReachedDest = IA_VehicleManager.HasVehicleReachedDestination(vehicle, m_drivingTarget);
            

                  
            if (!hasActiveWP || vehicleReachedDest)
            {

                m_isDriving = true;
                
                IA_VehicleManager.UpdateVehicleWaypoint(vehicle, this, m_drivingTarget);
            }
        }
        else if (m_drivingTarget == vector.Zero)
        {
        }
    }

    // Add a method to properly reset vehicle state
    void ClearVehicleReference()
    {
        
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
                            playerFaction = IA_Faction.US;
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
                    Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Reapplying authority-managed state %1 for group %2", 
                        typename.EnumToString(IA_GroupTacticalState, m_tacticalState), this), LogLevel.DEBUG);
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
                Print(string.Format("[IA_AiGroup.EvaluateTacticalState] CRITICAL CONDITION: Attacking/Flanking group %1 down to last unit - requesting retreat", 
                    this), LogLevel.WARNING);
                    
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
                Print(string.Format("[IA_AiGroup.EvaluateTacticalState] SEVERE LOSSES: Attacking/Flanking group %1 at %2% strength - requesting role reassessment", 
                    this, Math.Round(survivingRatio * 100)), LogLevel.WARNING);
                    
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
                    Print(string.Format("[IA_AiGroup.EvaluateTacticalState] IMMEDIATE THREAT: Group %1 under high danger (%2) - requesting appropriate response",
                        this, m_currentDangerLevel), LogLevel.WARNING);

                    // Request appropriate response based on current state and strength
                    if (aliveCount <= 2)
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
                    }
                    // --- MODIFIED: If already assigned offensive role by authority, request attack instead of defend ---
                    else if (m_isStateManagedByAuthority && (m_tacticalState == IA_GroupTacticalState.Attacking || m_tacticalState == IA_GroupTacticalState.Flanking))
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Attacking, m_lastDangerPosition); // Attack the source of danger
                    }
                    // Replace this block
                    /*
                    else if (m_tacticalState != IA_GroupTacticalState.Defending)
                    {
                        RequestTacticalStateChange(IA_GroupTacticalState.Defending, GetOrigin());
                    }
                    */
                    // With this block
                    else // Default response for immediate threat when not retreating or already assigned offense
                    {
                        // Request attack towards the source of the danger
                        RequestTacticalStateChange(IA_GroupTacticalState.Attacking, m_lastDangerPosition);
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
                // Default to attacking when no special conditions
                else
                {
                    RequestTacticalStateChange(IA_GroupTacticalState.Attacking, targetPos);
                }
                
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
                        Print(string.Format("[IA_AiGroup.EvaluateTacticalState] Group %1 unit strength too low for attack/flank - requesting Defend", this), LogLevel.DEBUG);
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
            if (!m_isCivilian && m_faction != IA_Faction.CIV)
            {
                // Request tactical state for military units
                RequestTacticalStateChange(IA_GroupTacticalState.Retreating, GetOrigin());
            }
            else
            {
                // Direct orders for civilians
                vector retreatDir = GetOrigin() - m_lastOrderPosition;
                if (retreatDir.LengthSq() > 0.1)
                    retreatDir = retreatDir.Normalized();
                else
                    retreatDir = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1)).Normalized();
    
                vector retreatPos = GetOrigin() + retreatDir * 50;
                RemoveAllOrders();
                AddOrder(retreatPos, IA_AiOrder.Patrol);
            }
        }
        // Otherwise, maintain combat stance by defending for civilians
        else if (m_isCivilian || m_faction == IA_Faction.CIV)
        {
            // Only give direct orders for civilians
            vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, GetOrigin());
            RemoveAllOrders();
            AddOrder(defendPos, IA_AiOrder.Defend);
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
        m_drivingTarget = destination;
        
        m_lastOrderPosition = vehicle.GetOrigin();
        m_lastOrderTime = System.GetUnixTime();
        
        RemoveAllOrders();
        
        if (destination != vector.Zero) 
        {
            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, this, destination);
        }
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
            Print(string.Format("[IA_AiGroup.SetTacticalState] Group %1 changing state: %2 -> %3", 
                this, m_tacticalState, newState), LogLevel.DEBUG);
                
            // Record the time when the state changed
            m_tacticalStateStartTime = System.GetUnixTime();
        }
        
        // Authority safety check - log a warning when called without authority flag
        if (!fromAuthority)
        {
            Print(string.Format("[IA_AiGroup.SetTacticalState] WARNING: Called without authority flag. Groups should request state changes instead of setting directly."), 
                LogLevel.WARNING);
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
                this, m_tacticalState, newState), LogLevel.WARNING);
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
                Print(string.Format("[IA_AiGroup.SetTacticalState] DEFEND ORDER DEBUG: Group %1 | Faction: %2 | AliveCount: %3 | Position: %4 | Target: %5", 
                    this, m_faction, GetAliveCount(), m_lastConfirmedPosition.ToString(), defendPos.ToString()), LogLevel.NORMAL);
                
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
                    AddOrder(targetPos, IA_AiOrder.Move, true);
                }
                break;
                
            case IA_GroupTacticalState.DefendPatrol:
                // No target for patrol state - generate one
                if (m_lastAssignedArea)
                {
                    vector areaOrigin = m_lastAssignedArea.GetOrigin();
                    float radius = m_lastAssignedArea.GetRadius() * 0.75; // Stay within the area
                    
                    // Only generate a random patrol if we're not already close to the center
                    if (vector.Distance(m_lastConfirmedPosition, areaOrigin) > 15)
                    {
                        // Generate a random point to patrol to within the area
                        vector patrolPoint = IA_Game.rng.GenerateRandomPointInRadius(1, radius, areaOrigin);
                        
                        // Apply orders
                        AddOrder(patrolPoint, IA_AiOrder.Move, false);
                    }
                    else
                    {
                        // We're at the center already, just defend this position
                        AddOrder(m_lastConfirmedPosition, IA_AiOrder.Defend, false);
                    }
                }
                break;
            
            case IA_GroupTacticalState.InVehicle:
                // For groups in vehicles - maybe handled at a higher level
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
                priority += 40; // Combat orders highest priority
                break;
            case IA_AiOrder.Defend:
                return 0;
            case IA_AiOrder.GetInVehicle:
                priority += 100;  // Vehicle mounting is important
                break;
            case IA_AiOrder.PriorityMove:
                return 100;  // High priority movement
            case IA_AiOrder.Move:
                priority += 30;  // Move orders medium priority (between combat and patrol)
                break;
            case IA_AiOrder.Patrol:
                priority += 0;  // Patrol orders lowest priority
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

    // Make sure CalculateFlankingPosition is defined as a public method
    vector CalculateFlankingPosition(vector enemyPos, vector groupPos)
    {
        // Get direction from enemy to group
        vector dirFromEnemy = groupPos - enemyPos;
        dirFromEnemy = dirFromEnemy.Normalized();
        
        // Calculate perpendicular vectors (both left and right flanks)
        vector leftFlank = Vector(-dirFromEnemy[2], 0, dirFromEnemy[0]);
        vector rightFlank = Vector(dirFromEnemy[2], 0, -dirFromEnemy[0]);
        
        // Choose one flank direction randomly
        vector flankDir;
        if (Math.RandomFloat(0, 1) < 0.5)
        {
            flankDir = leftFlank;
        }
        else
        {
            flankDir = rightFlank;
        }
        
        // Calculate flanking distance based on situation
        float flankDistance = Math.RandomFloat(60, 100);
        
        // Calculate final flanking position
        vector flankPos = enemyPos + (flankDir * flankDistance);
        
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

    // Add a public setter for the assigned area
    void SetAssignedArea(IA_Area area)
    {
        m_lastAssignedArea = area;
    }

    //------------------------------------------------------------------------------------------------
    bool IsStateManagedByAuthority()
    {
        return m_isStateManagedByAuthority;
    }
    
    //------------------------------------------------------------------------------------------------

    // Add this new method before SetTacticalState method
    void RequestTacticalStateChange(IA_GroupTacticalState newState, vector targetPos = vector.Zero, IEntity targetEntity = null)
    {
        // Don't create redundant requests
        if (m_hasPendingStateRequest && m_requestedState == newState)
            return;
            
        // Log the request
        Print(string.Format("[IA_AiGroup.RequestTacticalStateChange] Group %1 requesting state change from %2 to %3", 
            this, typename.EnumToString(IA_GroupTacticalState, m_tacticalState), 
            typename.EnumToString(IA_GroupTacticalState, newState)), LogLevel.DEBUG);
            
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

};  
