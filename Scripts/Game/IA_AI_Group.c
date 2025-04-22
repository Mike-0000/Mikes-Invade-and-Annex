enum IA_GroupDangerType
{
    ProjectileImpact,
    WeaponFire,
    ProjectileFlyby,
    Explosion,
    EnemySpotted
}

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
    
    private int m_lastDangerEventTime = 0;
    private vector m_lastDangerPosition = vector.Zero;
    private IA_GroupDangerType m_lastDangerType = IA_GroupDangerType.EnemySpotted;
    private float m_currentDangerLevel = 0.0; // 0.0-1.0 scale
    private int m_consecutiveDangerEvents = 0;

    private static const int BEHAVIOR_LOG_RATE_LIMIT_SECONDS = 3;
    private int m_lastApplyUnderFireLogTime = 0;
    private int m_lastApplyEnemySpottedLogTime = 0;
    private int m_lastDangerDetectLogTime = 0;
    private int m_lastDangerTriggerLogTime = 0;

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac)
    {
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
        
        m_lastDangerEventTime = 0;
        m_lastDangerDetectLogTime = 0;
        m_lastDangerTriggerLogTime = 0;
        m_currentDangerLevel = 0.0;
        m_consecutiveDangerEvents = 0;
        
        m_reactionManager = new IA_AIReactionManager();
        
        string managerStatus;
        if (m_reactionManager)
            managerStatus = "CREATED";
        else
            managerStatus = "NULL";
            

    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{

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
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV);
        grp.m_isCivilian = true;
        return grp;
    }

    static IA_AiGroup CreateGroupForVehicle(Vehicle vehicle, IA_Faction faction, int unitCount)
    {
        if (!vehicle || unitCount <= 0)
            return null;

        vector spawnPos = vehicle.GetOrigin() + vector.Up; // Spawn slightly above vehicle origin

        IA_AiGroup grp = new IA_AiGroup(spawnPos, IA_SquadType.Riflemen, faction);
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

        return grp;
    }

    static IA_AiGroup CreateMilitaryGroupFromUnits(vector initialPos, IA_Faction faction, int unitCount)
    {

              
        if (unitCount <= 0)
            return null;

        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, faction); // SquadType might be less relevant here
        grp.m_isCivilian = false;

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
                return null;
        }

        if (!groupRes) {
            return null;
        }

        // Spawn the base group entity and adjust its position to ground level
        vector groundPos = initialPos;
        float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
        groundPos[1] = groundY;
        IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(groundPos));
        grp.m_group = SCR_AIGroup.Cast(groupEnt);

        if (!grp.m_group) {
            delete groupEnt; // Clean up the spawned group entity
            return null;
        }

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

        grp.Spawn(IA_AiOrder.Patrol, initialPos);

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

        ResourceName rname = IA_AiOrderResource(order);
        Resource res = Resource.Load(rname);
        if (!res)
        {
            return;
        }

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
        if (!w)
        {
            return;
        }

        if (m_isDriving)
        {
            w.SetPriorityLevel(400); // Keep vehicles as is
        }
        else
        {
            // Dynamic priority for infantry based on situation
            int priorityLevel = CalculateWaypointPriority(order);
            w.SetPriorityLevel(priorityLevel);
        }

        m_group.AddWaypoint(w);
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
        return origin;
    }

    bool IsSpawned()
    {
		if (GetAliveCount() == 0){
			m_isSpawned = false;
		}
        return m_isSpawned;
    }

void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
{

          
    if (!PerformSpawn())
    {

        return;
    }


    vector posToUse;
    if (orderPos != vector.Zero)
    {
        posToUse = orderPos;
    }
    else
    {
        posToUse = IA_Game.rng.GenerateRandomPointInRadius(5, 25, m_initialPosition);
    }

    if (!m_isDriving)
    {
        AddOrder(posToUse, initialOrder);
    }

}


	
	
	
    private bool PerformSpawn()
    {

        if (IsSpawned())
        {
            return false;
        }
        m_isSpawned = true;

        string resourceName;
        if (m_isCivilian)
        {
            resourceName = IA_RandomCivilianResourceName();
        }
        else
        {
            resourceName = IA_SquadResourceName(m_squadType, m_faction);
        }

        Resource res = Resource.Load(resourceName);
        if (!res)
        {
            return false;
        }

        IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
        if (m_isCivilian)
        {
            Resource groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
            IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
            m_group = SCR_AIGroup.Cast(groupEnt);

        }
        else
        {
            m_group = SCR_AIGroup.Cast(entity);
        }
        if (m_group)
        {
            vector groundPos = m_initialPosition;
            float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
            groundPos[1] = groundY;
            
            m_group.SetOrigin(groundPos);
            
        }

        if (!m_group)
        {

            return false;
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
        if (m_isCivilian || m_faction == IA_Faction.CIV) {
            return;
        }

        if (!IsSpawned() || !m_group)
            return;

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
                    }
                }
            }

            // Check if source is from the same faction as this group
            if (sourceFaction != IA_Faction.NONE && sourceFaction == m_faction)
            {
                return;
            }
            
            // If we could identify a valid enemy faction, set it
            if (sourceFaction != IA_Faction.NONE && sourceFaction != m_faction)
            {
                m_engagedEnemyFaction = sourceFaction;
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
                        m_lastDangerDetectLogTime = currentTime_pi;
                    }

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
                    
                    int currentTime_wf = System.GetUnixTime();
                    if (currentTime_wf - m_lastDangerDetectLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                    {
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
                    //Print("[IA_AI_DANGER] Enemy spotted at " + distance + "m", LogLevel.WARNING);
                    m_lastDangerDetectLogTime = currentTime_esd;
                }
                break;
        }
        
        // --- ADDED DEBUG LOG ---
        // Log the final decision before applying the reaction

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
                    //Print("Source Entity = " + factionKey, LogLevel.NORMAL);
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

            }

            // Apply danger level as a multiplier to the reaction intensity
            float multiplier = 0.5 + (m_currentDangerLevel * 0.5);
            float finalIntensity = reactionIntensity * multiplier;

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

        // --- END ADDED DEBUG LOG ---
        

        
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
                //Print("[IA_AI_DANGER] Danger state reset due to timeout", LogLevel.WARNING);
            }
            return;
        }
        
        // If already engaged or has active reaction, let those systems handle it
        if (IsEngagedWithEnemy() || (m_reactionManager && m_reactionManager.HasActiveReaction()))
            return;
            
        // If high danger level but no specific reaction yet, create a general heightened awareness state
        if (m_currentDangerLevel > 0.7 && !m_isDriving)
        {

            vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(10, 20, GetOrigin());
            
            if (m_lastDangerPosition != vector.Zero)
            {
                vector awayDir = GetOrigin() - m_lastDangerPosition;
                awayDir = awayDir.Normalized();
                defendPos = GetOrigin() + awayDir * 15;
            }
            
            if (!HasOrders())
            {
                AddOrder(defendPos, IA_AiOrder.Defend, true);
            }
        }
        else if (m_currentDangerLevel > 0.3 && !m_isDriving && !HasOrders())
        {
            if (m_lastDangerPosition != vector.Zero)
            {
                AddOrder(m_lastDangerPosition, IA_AiOrder.Defend);
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

					 
					FactionAffiliationComponent facComp = FactionAffiliationComponent.Cast(sourceEntity.FindComponent(FactionAffiliationComponent));
					FactionKey sourceFactionKey = facComp.GetAffiliatedFactionKey();
					if(agentFactionKey == sourceFactionKey) // Check For Friendly Fire
						continue;					
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
            // Get the source entity if available


            // If we have a sourceEntity, get detailed information about it
            string entityInfo = "NULL";
            if (sourceEntity)
            {
                entityInfo = sourceEntity.ToString();
            }
            
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
        if (m_isCivilian || m_faction == IA_Faction.CIV) {
            return;
        }
        
        if (!m_reactionManager || !IsSpawned())
            return;
            
        m_reactionManager.ProcessReactions();
        
        if (m_reactionManager.HasActiveReaction())
        {
            IA_AIReactionState reaction = m_reactionManager.GetCurrentReaction();
            if (reaction)
            {
                IA_AIReactionType reactionType = reaction.GetReactionType();

                
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
            
        IA_AIReactionType reactionType = reaction.GetReactionType();

            
        switch (reaction.GetReactionType())
        {
            case IA_AIReactionType.GroupMemberKilled:

                HandleGroupMemberKilledReaction(reaction);
                break;
                
            case IA_AIReactionType.UnderFire: // Changed from UnderAttack

                int currentTime_uf = System.GetUnixTime();
                if (currentTime_uf - m_lastApplyUnderFireLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    m_lastApplyUnderFireLogTime = currentTime_uf;
                }
                HandleUnderFireReaction(reaction); // Renamed handler
                break;
                
            case IA_AIReactionType.EnemySpotted:

                int currentTime_es = System.GetUnixTime();
                if (currentTime_es - m_lastApplyEnemySpottedLogTime > BEHAVIOR_LOG_RATE_LIMIT_SECONDS)
                {
                    m_lastApplyEnemySpottedLogTime = currentTime_es;
                }
                HandleEnemySpottedReaction(reaction);
                break;
                
        }
    }

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

    private void ScheduleNextStateEvaluation()
    {
        if (m_isStateEvaluationScheduled)
            return;
            
        int randomDelay = Math.RandomInt(MIN_STATE_EVALUATION_INTERVAL, MAX_STATE_EVALUATION_INTERVAL);
        
        GetGame().GetCallqueue().CallLater(EvaluateGroupState, randomDelay);
        m_isStateEvaluationScheduled = true;
        
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
                //Print("[IA_AI_DEATH_DEBUG] Setting enemy faction to player faction: " + typename.EnumToString(IA_Faction, playerFaction), LogLevel.WARNING);
            }
        }
        else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
        {
            ////Print("[DEBUG] Member killed by AI.", LogLevel.NORMAL);
            SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
            if (!c)
            {
                ////Print("[IA_AiGroup.OnMemberDeath] cast killerEntity to character failed!", LogLevel.ERROR);
                return;
            }
            
            string fKey = c.GetFaction().GetFactionKey();
            ////Print("[DEBUG] Killer faction key: " + fKey, LogLevel.NORMAL);
            
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
                ////Print("[IA_AiGroup.OnMemberDeath] Friendly kill => resetting engagement");
                m_engagedEnemyFaction = IA_Faction.NONE;
            }
        }
    }

    // Handle reaction when a group member is killed
    private void HandleGroupMemberKilledReaction(IA_AIReactionState reaction)
    {
        //Print("[IA_AI_BEHAVIOR_HANDLER] Executing: HandleGroupMemberKilledReaction", LogLevel.WARNING); // Added log
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

        IA_Faction sourceFaction = reaction.GetSourceFaction();
        if (sourceFaction != IA_Faction.NONE && !IsEngagedWithEnemy())
        {
            m_engagedEnemyFaction = sourceFaction;
        }

        if (m_isDriving && m_referencedEntity)
        {
            string entityInfo;
            if (m_referencedEntity)
                entityInfo = m_referencedEntity.ToString();
            else
                entityInfo = "NULL";

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

            vector sourcePos = reaction.GetSourcePosition();


            if (sourcePos != vector.Zero)
            {

                array<AIWaypoint> wpsBefore = {};
                int countBefore = 0;
                if(m_group) countBefore = m_group.GetWaypoints(wpsBefore);

                RemoveAllOrders(); // Clears existing waypoints
                AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy, true); // Adds S&D waypoint

                array<AIWaypoint> wpsAfter = {};
                int countAfter = 0;
                 if(m_group) countAfter = m_group.GetWaypoints(wpsAfter);
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
                
            //Print("[IA_AI_BEHAVIOR_DEBUG] HandleUnderFireReaction - NEITHER vehicle nor infantry branch executed!? m_isDriving=" + m_isDriving + ", m_referencedEntity=" + entityInfo, LogLevel.ERROR);
        }
        // --- END ADDED DEBUG LOG ---
    }
    
    // Handle reaction when enemy is spotted but not attacking yet
    private void HandleEnemySpottedReaction(IA_AIReactionState reaction)
    {
        //Print("[IA_AI_BEHAVIOR_HANDLER] Executing: HandleEnemySpottedReaction | Intensity: " + reaction.GetIntensity(), LogLevel.WARNING); // Added log with intensity

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
        ////Print("[DEBUG] GetFaction called. Returning: " + m_faction, LogLevel.NORMAL);
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

            if (reactionIsActive)
            {
                IA_AIReactionState currentReaction = m_reactionManager.GetCurrentReaction();

    
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


            EvaluateCombatState();
        }
        // If in a vehicle
        else if (isDriving)
        {
            Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
            string vehicleType = "Unknown";
            if (vehicle)
                vehicleType = vehicle.ClassName();


            UpdateVehicleOrders();
        }
        else if (m_currentDangerLevel > 0.0)
        {


            EvaluateDangerState();
        }
        else if (timeSinceLastOrder > 120 && !hasOrders)
        {


            EvaluateIdleState();
        }
        else
        {

        }

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
        

          
        // If the last order was very recent, let it execute
        int timeSinceLastOrder = TimeSinceLastOrder();
        if (timeSinceLastOrder < 3) {
             return;
        }

        if (!m_isCivilian && m_faction != IA_Faction.CIV && m_faction != IA_Faction.NONE)
        {
            // Check for danger events first - these need highest priority
            int currentTime = System.GetUnixTime();
            int timeSinceLastDanger = currentTime - m_lastDangerEventTime;

            if (m_lastDangerEventTime > 0 && m_engagedEnemyFaction == IA_Faction.NONE)
            {
                if (m_faction == IA_Faction.USSR)
                    m_engagedEnemyFaction = IA_Faction.US;
                else if (m_faction == IA_Faction.US)
                    m_engagedEnemyFaction = IA_Faction.USSR;

                
            }

            if (m_lastDangerEventTime > 0 && timeSinceLastDanger < 60)
            {
         
                
                vector targetPos = m_lastDangerPosition;
                if (targetPos == vector.Zero)
                {
                    targetPos = IA_Game.rng.GenerateRandomPointInRadius(20, 50, GetOrigin());
                }
                
                // Check if we have a semi-recent order to handle the threat already
                if (timeSinceLastOrder < 30 && HasOrders() && vector.Distance(m_lastOrderPosition, targetPos) < 50)
                {
                    // We have a recent order near the danger point - keep following it
                }
                else
                {

                    
                    RemoveAllOrders();
                    AddOrder(targetPos, IA_AiOrder.SearchAndDestroy, true);
                    
                    // If this unit wasn't already engaged with an enemy, set an appropriate enemy faction
                    if (m_engagedEnemyFaction == IA_Faction.NONE)
                    {
                        if (m_faction == IA_Faction.USSR)
                            m_engagedEnemyFaction = IA_Faction.US;
                        else if (m_faction == IA_Faction.US)
                            m_engagedEnemyFaction = IA_Faction.USSR;

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
                        

                        
                        RemoveAllOrders();
                        AddOrder(searchPoint, IA_AiOrder.SearchAndDestroy, true);
                    }
                    // For weaker groups, be more defensive
                    else
                    {
                        vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, currentPos);

                        
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

            //Print("[IA_AI_STATE] Combat evaluation: Group retreating due to low numbers (" + aliveCount + ")", LogLevel.WARNING);
            RemoveAllOrders();
            AddOrder(retreatPos, IA_AiOrder.Patrol); // Retreat order is Patrol
        }
        // Otherwise, maintain combat stance by defending
        else
        {
            vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, GetOrigin());
            //Print("[IA_AI_STATE] Combat evaluation: Group taking defensive position (No recent/active orders)", LogLevel.WARNING);
            RemoveAllOrders();
            AddOrder(defendPos, IA_AiOrder.Defend);
        }

        // Add debug logging to check faction identification
        //Print("[IA_AI_COMBAT_DEBUG] EvaluateCombatState: Group faction: " + m_faction + ", Enemy faction: " + enemyFaction + ", IsEngaged: " + IsEngagedWithEnemy(), LogLevel.WARNING);
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
            
            //Print("[IA_AI_STATE] Idle evaluation: Civilian group wandering", LogLevel.WARNING);
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
                
            }
            else // 30% chance to defend
            {
                vector defendPos = IA_Game.rng.GenerateRandomPointInRadius(5, 15, GetOrigin());
                AddOrder(defendPos, IA_AiOrder.Defend);
                
            }
        }
    }

    private void CheckDangerEvents()
    {

        
		if(m_faction == IA_Faction.CIV)
		{
			return;
		}

        
        if (!m_group)
        {
            return;
        }
            
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        
        
        if (agents.IsEmpty())
        {
            return;
        }
        
        foreach (AIAgent agent : agents)
        {
            if (!agent)
            {
                continue;
            }
            
            ProcessAgentDangerEvents(agent);
        }
    }

    // Calculate priority based on situational factors
    private int CalculateWaypointPriority(IA_AiOrder order)
    {
        // Base priority level starts at 100
        int priority = 10;
        
        // Increase priority based on order type
        switch (order)
        {
            case IA_AiOrder.SearchAndDestroy:
                priority += 100; // Combat orders highest priority
                break;
            case IA_AiOrder.Defend:
                priority += 50; // Defensive orders medium-high priority
                break;
            case IA_AiOrder.GetInVehicle:
                priority += 250;  // Vehicle mounting is important
                break;
            case IA_AiOrder.Patrol:
                priority += 10;  // Patrol orders lowest priority
                break;
        }
        
        // Increase priority if in danger
        if (m_currentDangerLevel > 0.0)
        {
            priority += Math.Round(100 * m_currentDangerLevel);
        }
        
        // Increase priority if engaged with enemy
        if (IsEngagedWithEnemy())
        {
            priority += 75;
        }
        
        // Adjust priority based on group strength
        int aliveCount = GetAliveCount();
        if (aliveCount == 2)
        {
            // Small groups prioritize their orders more (survival focused)
            priority += 250;
        }
		if (aliveCount == 1)
        {
            // Small groups prioritize their orders more (survival focused)
            priority += 500;
        }
        
        // More recent danger events increase priority
        if (m_lastDangerEventTime > 0)
        {
            int timeSinceLastDanger = System.GetUnixTime() - m_lastDangerEventTime;
            if (timeSinceLastDanger < 30)
            {
                priority += 75; // Recent danger gets higher priority
            }
        }
        
        return priority;
    }
};  
