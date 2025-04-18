///////////////////////////////////////////////////////////////////////
// 8) IA_AreaInstance - dynamic mission/task handling (integrates area)
///////////////////////////////////////////////////////////////////////
class IA_AreaInstance
{
    IA_Area m_area;
    IA_Faction m_faction;
    int m_strength;
    private ref array<ref IA_AiGroup> m_military  = {};
    private ref array<ref IA_AiGroup> m_civilians = {};
    private ref array<IA_Faction>     m_attackingFactions = {};

    private ref IA_AreaAttackers m_aiAttackers = null;

    private IA_ReinforcementState m_reinforcements = IA_ReinforcementState.NotDone;
    private int m_reinforcementTimer = 0;
    private int m_currentTask = 0;
    private bool m_canSpawn   = true;
    
    // --- Vehicle Reinforcement System ---
    private IA_ReinforcementState m_vehicleReinforcements = IA_ReinforcementState.NotDone;
    private int m_vehicleReinforcementTimer = 0;
    private ref array<Vehicle> m_areaVehicles = {};
    private int m_maxVehicles = 3;
    private int m_vehicleCheckTimer = 0;
    private int m_lastVehicleLostTime = 0;
    private int m_replacementsPending = 0;
    
    // --- Civilian Vehicle System ---
    private ref array<Vehicle> m_areaCivVehicles = {};
    private int m_maxCivVehicles = 2;
    private int m_civVehicleCheckTimer = 0;

    // --- Dynamic Task Variables ---
    private SCR_TriggerTask m_currentTaskEntity;
    private ref array<SCR_TriggerTask> m_taskQueue = {};

    // --- Player Scaling System ---
    private float m_aiScaleFactor = 1.0;
    private int m_currentPlayerCount = 0;
    
    // Update scaling factors based on player count
    void UpdatePlayerScaling(int playerCount, float aiScaleFactor, int maxVehicles)
    {
        // Store the previous scale factor to detect significant changes
        float previousScaleFactor = m_aiScaleFactor;
        
        // Update scale values
        m_currentPlayerCount = playerCount;
        m_aiScaleFactor = aiScaleFactor;
        m_maxVehicles = maxVehicles;
        
        //Print("[PLAYER_SCALING] Area " + m_area.GetName() + " updated: AI Scale=" + m_aiScaleFactor + ", Max Vehicles=" + m_maxVehicles, LogLevel.NORMAL);
        
        // If this is a significant scale change (more than 30% difference), adjust military units
        if (m_military && !m_military.IsEmpty() && previousScaleFactor > 0) 
        {
            float scaleDifference = Math.AbsFloat(m_aiScaleFactor - previousScaleFactor) / previousScaleFactor;
            
            if (scaleDifference > 0.3) // 30% change threshold
            {
                //Print("[PLAYER_SCALING] Significant scale change detected (" + scaleDifference*100 + "%). Adjusting military units.", LogLevel.NORMAL);
                
                // For scale increase: add additional groups
                if (m_aiScaleFactor > previousScaleFactor && m_area) 
                {
                    // Add some additional units when scaling up
                    int additionalGroups = Math.Round((m_aiScaleFactor / previousScaleFactor) - 1.0);
                    if (additionalGroups > 0) 
                    {
                        //Print("[PLAYER_SCALING] Adding " + additionalGroups + " additional military groups", LogLevel.NORMAL);
                        GenerateRandomAiGroups(additionalGroups, true);
                    }
                }
                // For scale decrease: remove some groups (but never below 1)
                else if (m_aiScaleFactor < previousScaleFactor && m_military.Count() > 1)
                {
                    // Calculate how many groups to remove
                    int groupsToRemove = Math.Round(m_military.Count() * (1.0 - (m_aiScaleFactor / previousScaleFactor)));
                    
                    // Ensure we keep at least one group
                    if (groupsToRemove >= m_military.Count())
                        groupsToRemove = m_military.Count() - 1;
                        
                    if (groupsToRemove > 0)
                    {
                        //Print("[PLAYER_SCALING] Removing " + groupsToRemove + " military groups", LogLevel.NORMAL);
                        
                        // Remove groups from the end of the array
                        for (int i = 0; i < groupsToRemove; i++)
                        {
                            if (m_military.Count() <= 1)
                                break;
                                
                            int index = m_military.Count() - 1;
                            IA_AiGroup group = m_military[index];
                            if (group)
                            {
                                group.Despawn();
                            }
                            m_military.Remove(index);
                        }
                        
                        // Update overall strength
                        int totalStrength = 0;
                        foreach (IA_AiGroup g : m_military)
                        {
                            totalStrength += g.GetAliveCount();
                        }
                        OnStrengthChange(totalStrength);
                    }
                }
            }
        }
    }

	
	
	static IA_AreaInstance Create(IA_Area area, IA_Faction faction, int startStrength = 0)
	{
		if(!area){
			//Print("[DEBUG] area is NULL! ", LogLevel.NORMAL);
			return null;
		}
	    IA_AreaInstance inst = new IA_AreaInstance();  // uses implicit no-arg constructor
	
	    inst.m_area = area;
	    inst.m_faction = faction;
	    inst.m_strength = startStrength;
	
	    //Print("[DEBUG] IA_AreaInstance.Create: Initialized with area = " + area.GetName(), LogLevel.NORMAL);
	
	    inst.m_area.SetInstantiated(true);
	    
	    // Initialize scaling factor BEFORE generating AI
	    float scaleFactor = IA_Game.GetAIScaleFactor();
	    int playerCount = IA_Game.GetUSPlayerCount();
	    int maxVehicles = IA_Game.GetMaxVehiclesForPlayerCount(3);
	    inst.UpdatePlayerScaling(playerCount, scaleFactor, maxVehicles);
	    
	    //Print("[PLAYER_SCALING] New area created with scale factor: " + scaleFactor + ", player count: " + playerCount, LogLevel.NORMAL);
	
	    int groupCount = area.GetMilitaryAiGroupCount();
	    inst.GenerateRandomAiGroups(groupCount, true);
	
	    int civCount = area.GetCivilianCount();
	    inst.GenerateCivilians(civCount);
	    
	    // Also spawn initial vehicles for the area
	    inst.SpawnInitialVehicles();
	    
	    // Spawn civilian vehicles for the area
	    inst.SpawnInitialCivVehicles();
	
	    return inst;
	}
		
		SCR_TriggerTask GetCurrentTaskEntity()
	{
	    return m_currentTaskEntity;
	}
	
	bool HasPendingTasks()
	{
	    return m_currentTaskEntity != null || !m_taskQueue.IsEmpty();
	}

/*

    // Constructor
    void IA_AreaInstance(IA_Area area, IA_Faction faction, int startStrength = 0)
    {
        //Print("[DEBUG] IA_AreaInstance constructor called for area: " + area.GetName() + ", faction: " + faction + ", startStrength: " + startStrength, LogLevel.NORMAL);
        m_area     = area;
        m_faction  = faction;
        m_strength = startStrength;

        m_area.SetInstantiated(true);

        int groupCount = m_area.GetMilitaryAiGroupCount();
        GenerateRandomAiGroups(groupCount, true);

        int civCount = m_area.GetCivilianCount();
        GenerateCivilians(civCount);
    }
*/
    // --- Task Queue Methods ---
    void QueueTask(string title, string description, vector spawnOrigin)
    {
        //Print("[DEBUG] QueueTask called with title: " + title + ", description: " + description + ", spawnOrigin: " + spawnOrigin, LogLevel.NORMAL);
        if (!m_currentTaskEntity)
        {
            CreateTask(title, description, spawnOrigin);
        }
        else
        {
            Resource res = Resource.Load("{33DA4D098C409421}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
            IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
            SCR_TriggerTask task = SCR_TriggerTask.Cast(taskEnt);
            task.SetTitle(title);
            task.SetDescription(description);
            m_taskQueue.Insert(task);
            //Print("[DEBUG] Task queued. Queue length: " + m_taskQueue.Count(), LogLevel.NORMAL);
        }
    }

    private void CreateTask(string title, string description, vector spawnOrigin)
    {
        //Print("[DEBUG] CreateTask called with title: " + title + ", spawnOrigin: " + spawnOrigin, LogLevel.NORMAL);
        Resource res = Resource.Load("{33DA4D098C409421}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
        IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
        m_currentTaskEntity = SCR_TriggerTask.Cast(taskEnt);
        m_currentTaskEntity.SetTitle(title);
        m_currentTaskEntity.SetDescription(description);
        m_currentTaskEntity.Create(false);
        //Print("[DEBUG] Task created and activated.", LogLevel.NORMAL);
    }

    void CompleteCurrentTask()
    {
        //Print("[DEBUG] CompleteCurrentTask called.", LogLevel.NORMAL);
        if (m_currentTaskEntity)
        {
            m_currentTaskEntity.Finish();
            m_currentTaskEntity = null;
        }
        if (!m_taskQueue.IsEmpty())
        {
            m_currentTaskEntity = m_taskQueue[0];
            m_taskQueue.Remove(0);
            m_currentTaskEntity.Create(false);
            //Print("[DEBUG] Next queued task activated.", LogLevel.NORMAL);
        }
    }

    void UpdateTask()
    {
        if (m_currentTaskEntity && m_currentTaskEntity.GetTaskState() == SCR_TaskState.FINISHED)
        {
            CompleteCurrentTask();
        }
    }

    void RunNextTask()
    {
        m_currentTask = m_currentTask + 1;
        if (m_currentTask == 1)
        {
            // Placeholder cycle
        }
        else if (m_currentTask == 2)
        {
            ReinforcementsTask();
        }
        else if (m_currentTask == 3)
        {
            StrengthUpdateTask();
        }
        else if (m_currentTask == 4)
        {
            VehicleReinforcementsTask();
        }
        else if (m_currentTask == 5)
        {
            MilitaryOrderTask();
        }
        else if (m_currentTask == 6)
        {
            CivilianOrderTask();
        }
        else if (m_currentTask == 7)
        {
            AiAttackersTask();
        }
        else if (m_currentTask == 8)
        {
            VehicleManagementTask();
        }
        else if (m_currentTask == 9)
        {
            AIReactionsTask();
        }
        else if (m_currentTask == 10)
        {
            m_currentTask = 0;
        }
        UpdateTask();
    }

    // --- Add a group to the military list ---
    void AddMilitaryGroup(IA_AiGroup group)
    {
        if (group && m_military.Find(group) == -1) // Avoid duplicates
        {
            m_military.Insert(group);
            // Optionally update strength immediately?
            // OnStrengthChange(m_strength + group.GetAliveCount());
        }
    }

    // --- Other methods remain similar ---
    bool IsUnderAttack()
    {
        return (!m_attackingFactions.IsEmpty());
    }

    void OnAttacked(IA_Faction attacker)
    {
        if (attacker == m_faction)
            return;
        if (m_reinforcements == IA_ReinforcementState.NotDone)
        {
            m_reinforcements = IA_ReinforcementState.Countdown;
        }
        
        // Also trigger vehicle reinforcements when area is attacked
        if (m_vehicleReinforcements == IA_ReinforcementState.NotDone)
        {
            m_vehicleReinforcements = IA_ReinforcementState.Countdown;
        }
        
        // If under attack and we have few vehicles, consider requesting priority vehicle reinforcement
        if (m_areaVehicles.Count() < 2 && IA_Game.rng.RandInt(0, 100) < 30) // 30% chance
        {
            RequestPriorityVehicleReinforcement();
        }
    }

    void OnFactionChange(IA_Faction newFaction)
    {
        m_faction = newFaction;
        m_attackingFactions.Clear();
        m_strength = 0;
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep)
            rep.SetFaction(m_area.GetName(), newFaction);
        if (m_aiAttackers && newFaction != IA_Faction.CIV)
        {
            delete m_aiAttackers;
            m_aiAttackers = null;
        }
    }

    void OnStrengthChange(int newVal)
    {
        m_strength = newVal;
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep && m_area)
            rep.SetStrength(m_area.GetName(), newVal);
    }

    private void ReinforcementsTask()
    {
        if (m_reinforcements != IA_ReinforcementState.Countdown)
            return;
        m_reinforcementTimer = m_reinforcementTimer + 1;
        if (m_reinforcementTimer > 600)
        {
            m_reinforcements = IA_ReinforcementState.Done;
            //Print("[IA_AreaInstance.ReinforcementsTask] reinforcements would arrive now");
        }
    }

    private void StrengthUpdateTask()
    {
        int totalCount = 0;
        foreach (IA_AiGroup g : m_military)
        {
            totalCount = totalCount + g.GetAliveCount();
            if (g.IsEngagedWithEnemy())
            {
                IA_Faction enemyFac = g.GetEngagedEnemyFaction();
                if (enemyFac != m_faction && enemyFac != IA_Faction.NONE && !m_attackingFactions.Contains(enemyFac))
                {
                    m_attackingFactions.Insert(enemyFac);
                    OnAttacked(enemyFac);
                }
            }
        }
        if (m_aiAttackers && m_aiAttackers.IsAnyEngaged())
        {
            IA_Faction attFac = m_aiAttackers.GetFaction();
            if (!m_attackingFactions.Contains(attFac))
            {
                m_attackingFactions.Insert(attFac);
                OnAttacked(attFac);
            }
        }
        if (totalCount != m_strength)
        {
            OnStrengthChange(totalCount);
        }
        if (m_strength == 0)
        {
            if (m_attackingFactions.Count() == 1)
            {
                OnFactionChange(m_attackingFactions[0]);
            }
            else if (m_attackingFactions.Count() > 1 && m_faction != IA_Faction.NONE)
            {
                m_faction = IA_Faction.NONE;
            }
        }
    }

    private void MilitaryOrderTask()
    {
        if (!this)
            return;
            
        if (!m_area)
            return;
            
        if (!m_canSpawn || m_military.IsEmpty())
            return;
            
        IA_AiOrder infantryOrder = IA_AiOrder.Defend;
        if (IsUnderAttack())
            infantryOrder = IA_AiOrder.SearchAndDestroy;
            
        //Print("[DEBUG] MilitaryOrderTask processing " + m_military.Count() + " military groups", LogLevel.NORMAL);
        
        foreach (IA_AiGroup g : m_military)
        {
            if(!g)
                continue;
            if (g.GetAliveCount() == 0)
                continue;
            
            // VALIDATION: If a unit has a referenced entity but IsDriving is false, clear the reference
            if (g.GetReferencedEntity() && !g.IsDriving())
            {
                //Print("[DEBUG_VEHICLE_REPAIR] Found inconsistency: Group has referenced entity but IsDriving=false. Clearing reference.", LogLevel.WARNING);
                g.ClearVehicleReference();
            }
            
            //Print("[DEBUG_VEHICLE_TRACKING] Group state: " + g.GetStateDescription() + ", IsDriving: " + g.IsDriving() + ", HasReferencedEntity: " + (g.GetReferencedEntity() != null), LogLevel.NORMAL);
			
            // Check if the group is already using a vehicle
            if (g.IsDriving())
            {
                //Print("[DEBUG_VEHICLE_TRACKING] Group is marked as driving", LogLevel.NORMAL);
                // Existing logic for already driving groups (handled by UpdateVehicleOrders)
                Vehicle vehicle = Vehicle.Cast(g.GetReferencedEntity());
                if (vehicle)
                {
                    //Print("[DEBUG_VEHICLE_TRACKING] Successfully cast referenced entity to Vehicle", LogLevel.NORMAL);
                    vector vehiclePos = vehicle.GetOrigin();
                    vector drivingTarget = g.GetDrivingTarget();
                    float distance = vector.Distance(vehiclePos, drivingTarget);
                    
                    //Print("[DEBUG_VEHICLES] Vehicle at " + vehiclePos.ToString() + ", target: " + drivingTarget.ToString() + ", distance: " + distance, LogLevel.NORMAL);
                    
                    // Let UpdateVehicleOrders handle waypoints for driving groups
                    if (!g.HasActiveWaypoint())
                    {
                        // Get the center point and radius for this group
                        int currentActiveGroup = IA_VehicleManager.GetActiveGroup();
                        vector groupCenter = IA_AreaMarker.CalculateGroupCenterPoint(currentActiveGroup);
                        float groupRadius = IA_AreaMarker.CalculateGroupRadius(currentActiveGroup);
                        
                        // Use appropriate center point and radius
                        vector centerPoint;
                        if (groupCenter != vector.Zero)
                            centerPoint = groupCenter;
                        else
                            centerPoint = m_area.GetOrigin();
                        
                        float radiusToUse;
                        if (groupRadius > 0)
                            radiusToUse = groupRadius * 0.8;
                        else
                            radiusToUse = m_area.GetRadius() * 0.8;
                        
                        // Always find a completely random road entity within the area
                        //Print("[DEBUG_VEHICLE_TRACKING] No active waypoint, finding random road in area", LogLevel.NORMAL);
                        vector roadTarget = IA_VehicleManager.FindRandomRoadEntityInZone(centerPoint, radiusToUse, currentActiveGroup);
                        
                        if (roadTarget != vector.Zero)
                        {
                            //Print("[DEBUG_VEHICLE_TRACKING] Found random road at: " + roadTarget, LogLevel.NORMAL);
                            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, roadTarget);
                        }
                        else
                        {
                            // Fallback to random position if no road found
                            vector randomPos = IA_Game.rng.GenerateRandomPointInRadius(1, radiusToUse, centerPoint);
                            //Print("[DEBUG_VEHICLE_TRACKING] No road found, using random position: " + randomPos, LogLevel.WARNING);
                            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, randomPos);
                        }
                    }
                    
                    g.UpdateVehicleOrders(); 
                }
                else
                {
                    // If we can't cast to Vehicle, clear the incorrect driving state
                    //Print("[DEBUG_VEHICLE_REPAIR] IsDriving=true but failed to cast referenced entity to Vehicle. Clearing driving state.", LogLevel.ERROR);
                    g.ClearVehicleReference();
                }
                continue; // Skip to next group if already driving
            }
                
            // Logic for groups NOT currently driving
            // Random order refresh time between 30-60 seconds instead of fixed 45
            int randomOrderRefresh = 30 + IA_Game.rng.RandInt(0, 30);
            if (g.TimeSinceLastOrder() >= randomOrderRefresh)
                g.RemoveAllOrders();
                
            if (!g.HasActiveWaypoint()) // Needs a new order
            {    
                // Generate a random position for infantry
                vector potentialTargetPos;
                
                // Use area's radius and origin as fallback
                potentialTargetPos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 0.6, m_area.GetOrigin());
                
                //Print("[DEBUG] Assigning infantry order to group. Target: " + potentialTargetPos + ", Order: " + infantryOrder, LogLevel.NORMAL);
                
                g.AddOrder(potentialTargetPos, infantryOrder); 
            }
        }
    }

    private void CivilianOrderTask()
    {
		    if (!m_area)
        {
            return;
        }
        if (!m_canSpawn || m_civilians.IsEmpty())
            return;
        foreach (IA_AiGroup g : m_civilians)
        {
            if (!g.IsSpawned() || g.GetAliveCount() == 0)
                continue;
            
            // Check if the civilian group is driving a vehicle
            if (g.IsDriving())
            {
                // Civilian group is driving - update vehicle orders
                Vehicle vehicle = Vehicle.Cast(g.GetReferencedEntity());
                if (vehicle)
                {
                    vector vehiclePos = vehicle.GetOrigin();
                    vector drivingTarget = g.GetDrivingTarget();
                    float distance = vector.Distance(vehiclePos, drivingTarget);
                    
                    // Let the group handle vehicle movement
                    if (!g.HasActiveWaypoint())
                    {
                        // Set a new destination when reached or no active waypoint
                        if (distance < 30 || g.TimeSinceLastOrder() >= 60)
                        {
                            // Generate a new random position for driving
                            vector randomPos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 1.0, m_area.GetOrigin());
                            
                            // Try finding a road position
                            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(m_area.GetOrigin(), m_area.GetRadius() * 0.8, IA_VehicleManager.GetActiveGroup());
                            if (roadPos != vector.Zero)
                            {
                                IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, roadPos);
                            }
                            else
                            {
                                IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, randomPos);
                            }
                        }
                    }
                    
                    g.UpdateVehicleOrders();
                }
                continue;
            }
            
            // Random order refresh time between 35-55 seconds instead of fixed 45
            int randomOrderRefresh = 35 + IA_Game.rng.RandInt(0, 20);
            if (g.TimeSinceLastOrder() >= randomOrderRefresh)
            {
                g.RemoveAllOrders();
            }
			if (!g.HasActiveWaypoint())
            {
                    vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 1.0, m_area.GetOrigin());
                    g.AddOrder(pos, IA_AiOrder.Patrol);
                
            }
        }
    }

    private void AiAttackersTask()
    {
		    if (!m_area)
        {
            return;
        }
        if (!m_aiAttackers)
            return;
        if (m_aiAttackers.IsInitializing())
        {
            // Add slight random delay to spread out spawning
            if (IA_Game.rng.RandInt(0, 100) < 60) // 60% chance to process on any given cycle
            {
                vector pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 1.2, m_area.GetRadius() * 1.3, m_area.GetOrigin());
                m_aiAttackers.SpawnNextGroup(pos);
            }
            return;
        }
        if (m_aiAttackers.IsAllGroupsDead())
        {
            IA_Faction f = m_aiAttackers.GetFaction();
            m_attackingFactions.RemoveItem(f);
            delete m_aiAttackers;
            m_aiAttackers = null;
            return;
        }
        
        // Add randomness to attacker processing
        if (IA_Game.rng.RandInt(0, 100) < 80) // 80% chance to process on any given cycle
        {
            m_aiAttackers.ProcessNextGroup();
        }
    }

    void GenerateRandomAiGroups(int number, bool insideArea)
    {
        // Apply player scaling to number of groups
        int scaledNumber = Math.Round(number * m_aiScaleFactor);
        if (scaledNumber < 1) scaledNumber = 1; // Ensure at least one group
        
        //Print("[PLAYER_SCALING] GenerateRandomAiGroups: Original=" + number + ", Scaled=" + scaledNumber + " (scale factor: " + m_aiScaleFactor + ")", LogLevel.NORMAL);
        
        int strCounter = m_strength;
        for (int i = 0; i < scaledNumber; i = i + 1)
        {
			    if (!m_area)
    {
        //Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
            IA_SquadType st = IA_GetRandomSquadType();
            vector pos = vector.Zero;
            if (insideArea)
                pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
            else
                pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 0.95, m_area.GetRadius() * 1.05, m_area.GetOrigin());
            
            // --- BEGIN NEW LOGIC ---
            int unitCount = IA_SquadCount(st, m_faction); // Determine unit count based on squad type
            
            // Apply scaling to unit count
            int scaledUnitCount = Math.Round(unitCount * m_aiScaleFactor);
            if (scaledUnitCount < 1) scaledUnitCount = 1; // Ensure at least one unit
            
            if (scaledUnitCount <= 0) 
            {
                //Print("[IA_AreaInstance.GenerateRandomAiGroups] Invalid unit count (" + scaledUnitCount + ") for squad type " + st + ", skipping group.", LogLevel.WARNING);
                continue;
            }
            
            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(pos, m_faction, scaledUnitCount);
            if (!grp)
            {
                //Print("[IA_AreaInstance.GenerateRandomAiGroups] Failed to create military group from units.", LogLevel.WARNING);
                continue;
            }
            
            // Add the group to the military list
            AddMilitaryGroup(grp);
            
            // Update strength counter
            strCounter = strCounter + grp.GetAliveCount();
        }
        
        // Update overall strength
        OnStrengthChange(strCounter);
    }

    private void VehicleReinforcementsTask()
    {
        if (m_vehicleReinforcements != IA_ReinforcementState.Countdown)
            return;
            
        m_vehicleReinforcementTimer = m_vehicleReinforcementTimer + 1;
        
        // Vehicles arrive faster when under attack
        int countdownLimit = 300; // Normal: 5 minutes
        
        // Speed up vehicle reinforcements if area is under heavy attack
        if (IsUnderAttack() && m_attackingFactions.Count() > 0)
        {
            countdownLimit = 180; // Under attack: 3 minutes
            
            // Further speed up if we have no vehicles left
            if (m_areaVehicles.Count() == 0)
            {
                countdownLimit = 120; // Urgent: 2 minutes
            }
        }
        
        if (m_vehicleReinforcementTimer > countdownLimit)
        {
            m_vehicleReinforcements = IA_ReinforcementState.Done;
            SpawnVehicleReinforcements();
        }
    }
    
    // Request priority vehicle reinforcements (used during combat situations)
    void RequestPriorityVehicleReinforcement()
    {
        // Only process for active military factions
        if (m_faction == IA_Faction.NONE || m_faction == IA_Faction.CIV)
            return;
            
        // If we're already at max vehicles, don't add more
        if (m_areaVehicles.Count() >= m_maxVehicles)
            return;
            
        // If we've recently lost a vehicle, don't immediately spawn another
        int currentTime = System.GetUnixTime();
        if (currentTime - m_lastVehicleLostTime < 60) // 60-second cooldown
            return;
            
        // Reset the timer if we're in countdown, or set it to done if not started
        if (m_vehicleReinforcements == IA_ReinforcementState.Countdown)
        {
            // Force the timer to near completion
            m_vehicleReinforcementTimer = 290; // Almost at the normal 300 limit
        }
        else if (m_vehicleReinforcements == IA_ReinforcementState.NotDone)
        {
            // Set to Done and spawn immediately
            m_vehicleReinforcements = IA_ReinforcementState.Done;
            SpawnVehicleReinforcements();
        }
    }
    
    void SpawnVehicleReinforcements()
    {
        if (!m_area)
            return;
            
        if (m_faction == IA_Faction.NONE || m_faction == IA_Faction.CIV)
            return;
            
        //Print("[DEBUG] IA_AreaInstance.SpawnVehicleReinforcements: Spawning vehicle reinforcements for area " + m_area.GetName(), LogLevel.NORMAL);
        
        // Get number of vehicles to spawn (1-2), scaled with player count
        int baseVehiclesToSpawn = IA_Game.rng.RandInt(1, 3);
        int playerScaledVehicles = Math.Round(baseVehiclesToSpawn * m_aiScaleFactor);
        if (playerScaledVehicles < 1) playerScaledVehicles = 1;
        
       //Print("[PLAYER_SCALING] SpawnVehicleReinforcements: Base vehicles=" + baseVehiclesToSpawn + 
//              ", Scaled=" + playerScaledVehicles + " (scale factor: " + m_aiScaleFactor + 
//              ", player count: " + m_currentPlayerCount + ")", LogLevel.NORMAL);
        
        // Get active group from area markers
        int activeGroup = -1;
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        if (markers && !markers.IsEmpty())
        {
            IA_AreaMarker firstMarker = markers.Get(0);
            if (firstMarker)
                activeGroup = firstMarker.m_areaGroup;
        }
        
        // If we couldn't get the group from markers, use the vehicle manager
        if (activeGroup < 0)
        {
            activeGroup = IA_VehicleManager.GetActiveGroup();
        }
        
        // Calculate spawn positions around the area perimeter
        for (int i = 0; i < playerScaledVehicles; i++)
        {
            // Check if we're under max vehicle limit
            if (m_areaVehicles.Count() >= m_maxVehicles)
            {
                //Print("[DEBUG] IA_AreaInstance.SpawnVehicleReinforcements: At vehicle limit. Removing oldest vehicle.", LogLevel.NORMAL);
                if (!m_areaVehicles.IsEmpty())
                {
                    Vehicle oldVehicle = m_areaVehicles[0];
                    if (oldVehicle)
                    {
                        IA_VehicleManager.DespawnVehicle(oldVehicle);
                    }
                    m_areaVehicles.Remove(0);
                }
            }
            
            // Try to find a road position near the perimeter for more realistic spawning
            float spawnDistance = m_area.GetRadius() * 1.2;
            float randomAngle = IA_Game.rng.RandInt(0, 360);
            float rad = randomAngle * Math.PI / 180.0;
            
            vector origin = m_area.GetOrigin();
            vector perimeterPoint;
            perimeterPoint[0] = origin[0] + Math.Cos(rad) * spawnDistance;
            perimeterPoint[1] = origin[1];
            perimeterPoint[2] = origin[2] + Math.Sin(rad) * spawnDistance;
            perimeterPoint[1] = GetGame().GetWorld().GetSurfaceY(perimeterPoint[0], perimeterPoint[2]);
            
            // Try to find a road near this perimeter point
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(perimeterPoint, 150, activeGroup);
            vector spawnPos;
            
            // If a road position was found, use it, otherwise use the perimeter point
            if (roadPos != vector.Zero)
            {
                spawnPos = roadPos;
                //Print("[DEBUG] SpawnVehicleReinforcements: Found road location near perimeter at " + spawnPos, LogLevel.NORMAL);
            }
            else
            {
                spawnPos = perimeterPoint;
                //Print("[DEBUG] SpawnVehicleReinforcements: No road found, using perimeter position at " + spawnPos, LogLevel.WARNING);
            }
            
            // Use military vehicles only
            Vehicle spawnedVehicle = IA_VehicleManager.SpawnRandomVehicle(m_faction, false, true, spawnPos);
            
            if (spawnedVehicle)
            {
                //Print("[DEBUG] IA_AreaInstance.SpawnVehicleReinforcements: Spawned vehicle at " + spawnPos, LogLevel.NORMAL);
                m_areaVehicles.Insert(spawnedVehicle);
                
                // Create AI units and place them in the vehicle
                IA_AiGroup vehicleGroup = IA_VehicleManager.PlaceUnitsInVehicle(spawnedVehicle, m_faction, m_area.GetOrigin(), this);
            }
            else
            {
                //Print("[DEBUG] IA_AreaInstance.SpawnVehicleReinforcements: Failed to spawn vehicle", LogLevel.WARNING);
            }
        }
    }
    
    private void VehicleManagementTask()
    {
        // Process vehicle management once every ~5 seconds
        m_vehicleCheckTimer = m_vehicleCheckTimer + 1;
        if (m_vehicleCheckTimer < 5)
            return;
            
        m_vehicleCheckTimer = 0;
        
        // Clean up destroyed/invalid vehicles and schedule replacements
        for (int i = 0; i < m_areaVehicles.Count(); i++)
        {
            Vehicle vehicle = m_areaVehicles[i];
            if (!vehicle || vehicle.GetDamageManager().IsDestroyed())
            {
                if (vehicle && vehicle.GetDamageManager().IsDestroyed())
                {
                    // Handle the destroyed vehicle with proper cleanup and replacement
                    HandleDestroyedVehicle(vehicle);
                }
                
                // Remove from tracking array
                m_areaVehicles.Remove(i);
                i--;
            }
        }
        
        // Check if we need more vehicles in the area (for active factions)
        if (m_faction != IA_Faction.NONE && m_faction != IA_Faction.CIV && m_areaVehicles.Count() < 1)
        {
            // Periodically spawn new vehicles if we're below minimum
            if (IA_Game.rng.RandInt(0, 100) < 20)  // Increased chance from 10% to 20%
            {
                SpawnVehicleReinforcements();
            }
        }
        
        // Also manage civilian vehicles
        ManageCivilianVehicles();
    }
    
    // Handle a destroyed vehicle with proper cleanup and schedule a replacement
    private void HandleDestroyedVehicle(Vehicle vehicle)
    {
        if (!vehicle)
            return;
            
        //Print("[DEBUG] IA_AreaInstance.HandleDestroyedVehicle: Processing destroyed vehicle in area " + m_area.GetName(), LogLevel.NORMAL);
        
        // Update the last vehicle lost time
        m_lastVehicleLostTime = System.GetUnixTime();
        
        // If this faction is no longer active, don't replace the vehicle
        if (m_faction == IA_Faction.NONE || m_faction == IA_Faction.CIV)
            return;
            
        // Try to find an AI group that was using this vehicle
        IA_AiGroup vehicleGroup = null;
        foreach (IA_AiGroup group : m_military)
        {
            if (group && group.GetReferencedEntity() == vehicle)
            {
                vehicleGroup = group;
                break;
            }
        }
        
        // Release any vehicle reservations in the vehicle manager
        IA_VehicleManager.ReleaseVehicleReservation(vehicle);
        
        // If we found a group using this vehicle, give them new orders
        if (vehicleGroup)
        {
            //Print("[DEBUG] IA_AreaInstance.HandleDestroyedVehicle: Found AI group using the destroyed vehicle", LogLevel.NORMAL);
            
            // Clear the vehicle reference and issue new orders
            vehicleGroup.ClearVehicleReference();
            vehicleGroup.RemoveAllOrders();
            
            // If the group still has alive members, give them defend orders at their current position
            if (vehicleGroup.GetAliveCount() > 0)
            {
                vector groupPos = vehicleGroup.GetOrigin();
                vehicleGroup.AddOrder(groupPos, IA_AiOrder.Defend);
                //Print("[DEBUG] IA_AreaInstance.HandleDestroyedVehicle: Issued defend orders to surviving group members", LogLevel.NORMAL);
            }
        }
        
        // Check if we've already reached the max number of pending replacements
        if (m_replacementsPending >= 2)
        {
            //Print("[DEBUG] IA_AreaInstance.HandleDestroyedVehicle: Already have " + m_replacementsPending + " replacements pending, skipping", LogLevel.NORMAL);
            return;
        }
        
        // Schedule a replacement vehicle with a delay
        int randomDelay = 30000 + IA_Game.rng.RandInt(0, 30000); // 30-60 second delay
        m_replacementsPending++;
        GetGame().GetCallqueue().CallLater(ScheduleVehicleReplacement, randomDelay, false);
        
        //Print("[DEBUG] IA_AreaInstance.HandleDestroyedVehicle: Scheduled replacement vehicle in " + (randomDelay/1000) + " seconds", LogLevel.NORMAL);
    }
    
    // Schedule a replacement vehicle to be spawned
    private void ScheduleVehicleReplacement()
    {
        // Decrement the pending count
        m_replacementsPending--;
        
        // Only replace vehicles for active military factions
        if (m_faction == IA_Faction.NONE || m_faction == IA_Faction.CIV)
            return;
            
        // Check if we're already at the vehicle limit
        if (m_areaVehicles.Count() >= m_maxVehicles)
            return;
            
        //Print("[DEBUG] IA_AreaInstance.ScheduleVehicleReplacement: Spawning replacement vehicle for area " + m_area.GetName(), LogLevel.NORMAL);
        
        // Get active group from area markers
        int activeGroup = -1;
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        if (markers && !markers.IsEmpty())
        {
            IA_AreaMarker firstMarker = markers.Get(0);
            if (firstMarker)
                activeGroup = firstMarker.m_areaGroup;
        }
        
        // If we couldn't get the group from markers, use the vehicle manager
        if (activeGroup < 0)
        {
            activeGroup = IA_VehicleManager.GetActiveGroup();
        }
        
        // Calculate a spawn position on the perimeter of the area
        float randomAngle = IA_Game.rng.RandInt(0, 360);
        float rad = randomAngle * Math.PI / 180.0;
        
        vector origin = m_area.GetOrigin();
        vector perimeterPoint;
        perimeterPoint[0] = origin[0] + Math.Cos(rad) * (m_area.GetRadius() * 1.2);
        perimeterPoint[1] = origin[1];
        perimeterPoint[2] = origin[2] + Math.Sin(rad) * (m_area.GetRadius() * 1.2);
        perimeterPoint[1] = GetGame().GetWorld().GetSurfaceY(perimeterPoint[0], perimeterPoint[2]);
        
        // Try to find a road near this perimeter point
        vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(perimeterPoint, 150, activeGroup);
        vector spawnPos;
        
        // If a road position was found, use it, otherwise use the perimeter point
        if (roadPos != vector.Zero)
        {
            spawnPos = roadPos;
            //Print("[DEBUG] ScheduleVehicleReplacement: Found road location near perimeter at " + spawnPos, LogLevel.NORMAL);
        }
        else
        {
            spawnPos = perimeterPoint;
            //Print("[DEBUG] ScheduleVehicleReplacement: No road found, using perimeter position at " + spawnPos, LogLevel.WARNING);
        }
        
        // Spawn a new military vehicle
        Vehicle replacementVehicle = IA_VehicleManager.SpawnRandomVehicle(m_faction, false, true, spawnPos);
        
        if (replacementVehicle)
        {
            //Print("[DEBUG] IA_AreaInstance.ScheduleVehicleReplacement: Spawned replacement vehicle at " + spawnPos, LogLevel.NORMAL);
            m_areaVehicles.Insert(replacementVehicle);
            
            // Place units in the vehicle
            IA_AiGroup vehicleGroup = IA_VehicleManager.PlaceUnitsInVehicle(replacementVehicle, m_faction, m_area.GetOrigin(), this);
        }
        else
        {
            //Print("[DEBUG] IA_AreaInstance.ScheduleVehicleReplacement: Failed to spawn replacement vehicle", LogLevel.WARNING);
        }
    }
    
    // Integrate a new vehicle with the area's military forces
    void RegisterVehicleWithMilitary(Vehicle vehicle, IA_AiGroup vehicleGroup)
    {
        if (!vehicle || !vehicleGroup)
            return;
            
        // Add the vehicle to our tracked list    
        if (m_areaVehicles.Find(vehicle) == -1)
        {
            m_areaVehicles.Insert(vehicle);
        }
        
        // Add the AI group to our military units
        AddMilitaryGroup(vehicleGroup);
    }
    
    // Get an array of all vehicles in this area
    array<Vehicle> GetAreaVehicles()
    {
        return m_areaVehicles;
    }
    
    // Get an array of all civilian vehicles in this area
    array<Vehicle> GetAreaCivVehicles()
    {
        return m_areaCivVehicles;
    }
    
    // Integrate a new civilian vehicle with the area's civilian groups
    void RegisterCivilianVehicle(Vehicle vehicle, IA_AiGroup vehicleGroup)
    {
        if (!vehicle || !vehicleGroup)
            return;
            
        // Add the vehicle to our tracked list    
        if (m_areaCivVehicles.Find(vehicle) == -1)
        {
            m_areaCivVehicles.Insert(vehicle);
        }
        
        // Ensure the civilian group is in our list
        if (m_civilians.Find(vehicleGroup) == -1)
        {
            m_civilians.Insert(vehicleGroup);
        }
    }
    
    // Spawn initial vehicles when the area is created
    void SpawnInitialVehicles()
    {
        if (m_faction == IA_Faction.NONE || m_faction == IA_Faction.CIV)
            return;
            
        // Get current scaling
        float scaleFactor = IA_Game.GetAIScaleFactor();
        m_aiScaleFactor = scaleFactor;
        m_maxVehicles = IA_Game.GetMaxVehiclesForPlayerCount(3);
        
        // Spawn 1-2 vehicles for military factions, scaled with player count
        int baseVehicles = IA_Game.rng.RandInt(1, 3);
        int scaledVehicles = Math.Round(baseVehicles * m_aiScaleFactor);
        if (scaledVehicles < 1) scaledVehicles = 1;
        
       //Print("[PLAYER_SCALING] SpawnInitialVehicles: Base vehicles=" + baseVehicles + 
//              ", Scaled=" + scaledVehicles + " (scale factor: " + m_aiScaleFactor + ")", LogLevel.NORMAL);
              
        // Get active group from area markers
        int activeGroup = -1;
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        if (markers && !markers.IsEmpty())
        {
            IA_AreaMarker firstMarker = markers.Get(0);
            if (firstMarker)
                activeGroup = firstMarker.m_areaGroup;
        }
        
        // If we couldn't get the group from markers, use the vehicle manager
        if (activeGroup < 0)
        {
            activeGroup = IA_VehicleManager.GetActiveGroup();
        }
        
        for (int i = 0; i < scaledVehicles; i++)
        {
            // Find a road position to spawn the vehicle on
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(m_area.GetOrigin(), m_area.GetRadius() * 0.8, activeGroup);
            vector spawnPos;
            
            // If a road was found, use it; otherwise fall back to random position
            if (roadPos != vector.Zero)
            {
                spawnPos = roadPos;
                //Print("[DEBUG] SpawnInitialVehicles: Found road location at " + spawnPos, LogLevel.NORMAL);
            }
            else
            {
                // Fallback to random position if no road found
                spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 0.7, m_area.GetOrigin());
                //Print("[DEBUG] SpawnInitialVehicles: No road found, using random position at " + spawnPos, LogLevel.WARNING);
            }
            
            // Spawn the vehicle (with military-only vehicles)
            Vehicle vehicle = IA_VehicleManager.SpawnRandomVehicle(m_faction, false, true, spawnPos);
            
            if (vehicle)
            {
                m_areaVehicles.Insert(vehicle);
                
                // Create AI units and place them in the vehicle
                IA_AiGroup vehicleGroup = IA_VehicleManager.PlaceUnitsInVehicle(vehicle, m_faction, m_area.GetOrigin(), this);
            }
        }
    }

    void GenerateCivilians(int number)
    {
        if (!m_area)
        {
            //Print("[ERROR] IA_AreaInstance.GenerateCivilians: m_area is null!", LogLevel.ERROR);
            return;
        }
        
        // Don't apply player scaling to civilians - they're always spawned at the original count
        //Print("[PLAYER_SCALING] Civilians not affected by scaling - using original count: " + number, LogLevel.NORMAL);
        
        m_civilians.Clear();
        for (int i = 0; i < number; i = i + 1)
        {
            vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
            float y = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
            pos[1] = y;
            IA_AiGroup civ = IA_AiGroup.CreateCivilianGroup(pos);
            civ.Spawn();
            m_civilians.Insert(civ);
        }
    }

    // Handle civilian vehicle management
    private void ManageCivilianVehicles()
    {
        // Process civ vehicle management on the same cycle
        m_civVehicleCheckTimer = m_civVehicleCheckTimer + 1;
        if (m_civVehicleCheckTimer < 5)
            return;
            
        m_civVehicleCheckTimer = 0;
        
        // Clean up destroyed/invalid civilian vehicles
        for (int i = 0; i < m_areaCivVehicles.Count(); i++)
        {
            Vehicle vehicle = m_areaCivVehicles[i];
            if (!vehicle || vehicle.GetDamageManager().IsDestroyed())
            {
                // Remove from tracking array
                m_areaCivVehicles.Remove(i);
                i--;
                continue;
            }
            
            // Also check for abandoned civilian vehicles (no occupants)
            bool hasDriver = false;
            SCR_ChimeraCharacter driver = IA_VehicleManager.GetVehicleDriver(vehicle);
            if (driver)
            {
                hasDriver = true;
            }
            
            // If no driver and not recently spawned (10+ minutes), remove the civilian vehicle
            if (!hasDriver)
            {
                // 5% chance to remove each cycle
                if (IA_Game.rng.RandInt(0, 100) < 5)
                {
                    //Print("[DEBUG] ManageCivilianVehicles: Removing abandoned civilian vehicle", LogLevel.NORMAL);
                    IA_VehicleManager.DespawnVehicle(vehicle);
                    m_areaCivVehicles.Remove(i);
                    i--;
                }
            }
        }
        
        // Periodically add new civilian vehicles if we're below the limit
        // First check if this area type should have civilian vehicles
        if (m_area && m_areaCivVehicles.Count() < m_maxCivVehicles)
        {
            // Ensure we're respecting area type restrictions
            IA_AreaType areaType = m_area.GetAreaType();
            bool shouldHaveCivVehicles = false;
            
            switch (areaType)
            {
                case IA_AreaType.City:
                case IA_AreaType.Town:
                case IA_AreaType.Docks:
                case IA_AreaType.Airport:
                    shouldHaveCivVehicles = true;
                    break;
                    
                case IA_AreaType.Military:
                case IA_AreaType.Property:
                    shouldHaveCivVehicles = false;
                    break;
                    
                default:
                    shouldHaveCivVehicles = true;
            }
            
            // Only spawn new vehicles if this area type should have them
            if (shouldHaveCivVehicles)
            {
                // Lower chance than military vehicles (10%)
                if (IA_Game.rng.RandInt(0, 100) < 10)
                {
                    SpawnCivilianVehicle();
                }
            }
        }
    }
    
    // Spawn a single civilian vehicle
    private void SpawnCivilianVehicle()
    {
        if (!m_area)
            return;
            
        //Print("[DEBUG] IA_AreaInstance.SpawnCivilianVehicle: Spawning civilian vehicle in area " + m_area.GetName(), LogLevel.NORMAL);
        
        // Get active group from area markers
        int activeGroup = -1;
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        if (markers && !markers.IsEmpty())
        {
            IA_AreaMarker firstMarker = markers.Get(0);
            if (firstMarker)
                activeGroup = firstMarker.m_areaGroup;
        }
        
        // If we couldn't get the group from markers, use the vehicle manager
        if (activeGroup < 0)
        {
            activeGroup = IA_VehicleManager.GetActiveGroup();
        }
        
        // Find a road position to spawn the vehicle on
        vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(m_area.GetOrigin(), m_area.GetRadius() * 0.8, activeGroup);
        vector spawnPos;
        
        // If a road was found, use it; otherwise fall back to random position
        if (roadPos != vector.Zero)
        {
            spawnPos = roadPos;
            //Print("[DEBUG] SpawnCivilianVehicle: Found road location at " + spawnPos, LogLevel.NORMAL);
        }
        else
        {
            // Fallback to random position if no road found
            spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 0.7, m_area.GetOrigin());
            //Print("[DEBUG] SpawnCivilianVehicle: No road found, using random position at " + spawnPos, LogLevel.WARNING);
        }
        
        // Spawn the civilian vehicle (using IA_Faction.CIV and civilian-only flag)
        Vehicle vehicle = IA_VehicleManager.SpawnRandomVehicle(IA_Faction.CIV, true, false, spawnPos);
        
        if (vehicle)
        {
            // Create AI units and place them in the vehicle
            IA_AiGroup civGroup = IA_VehicleManager.PlaceUnitsInVehicle(vehicle, IA_Faction.CIV, m_area.GetOrigin(), this);
            
            // Register the vehicle and group with our civilian tracking
            if (civGroup)
            {
                RegisterCivilianVehicle(vehicle, civGroup);
            }
            else
            {
                // If no group was created, just track the vehicle
                m_areaCivVehicles.Insert(vehicle);
            }
            
            //Print("[DEBUG] SpawnCivilianVehicle: Spawned civilian vehicle at " + spawnPos, LogLevel.NORMAL);
        }
    }
    
    // Spawn initial civilian vehicles when the area is created
    void SpawnInitialCivVehicles()
    {
       //Print("[DEBUG_CIV_VEHICLES] SpawnInitialCivVehicles started for area: " + m_area.GetName(), LogLevel.NORMAL);
        
        if (!m_area)
            return;
            
        // Get the area type
        IA_AreaType areaType = m_area.GetAreaType();
        
        // Set maximum civilian vehicles based on area type
        switch (areaType)
        {
            case IA_AreaType.City:
                m_maxCivVehicles = 4 + IA_Game.rng.RandInt(0, 2); // 5-7 vehicles in cities
                break;
                
            case IA_AreaType.Town:
                m_maxCivVehicles = 3 + IA_Game.rng.RandInt(0, 2); // 3-5 vehicles in towns
                break;
                
            case IA_AreaType.Docks:
                m_maxCivVehicles = 1 + IA_Game.rng.RandInt(0, 1); // 2-3 vehicles at docks
                break;
                
            case IA_AreaType.Airport:
                m_maxCivVehicles = 1; // 2 vehicles at airports
                break;
                
            case IA_AreaType.Military:
                m_maxCivVehicles = 0; // 1 vehicle at military bases
                break;
                
            case IA_AreaType.Property:
                m_maxCivVehicles = 0; // 1 vehicle at properties
                break;
                
            default:
                m_maxCivVehicles = 1 + IA_Game.rng.RandInt(0, 2); // Default fallback
        }
        
        // Apply player scaling to max vehicle count
        m_maxCivVehicles = Math.Round(m_maxCivVehicles * m_aiScaleFactor);
        
       //Print("[DEBUG_CIV_VEHICLES] Area type: " + areaType + ", Max civilian vehicles: " + m_maxCivVehicles, LogLevel.NORMAL);
        
        // If there are no vehicles to spawn, exit early
        if (m_maxCivVehicles <= 0)
        {
           //Print("[DEBUG_CIV_VEHICLES] No civilian vehicles will spawn in this area type", LogLevel.NORMAL);
            return;
        }
        
        // Calculate how many to spawn initially (usually fewer than the max)
        int initialCivVehicles = Math.Round(m_maxCivVehicles * 0.7);
        if (initialCivVehicles < 1) initialCivVehicles = 1;
        
       //Print("[DEBUG_CIV_VEHICLES] Attempting to spawn " + initialCivVehicles + " initial civilian vehicles", LogLevel.NORMAL);
        
        int successfulSpawns = 0;
        
        // Get active group from area markers
        int activeGroup = -1;
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        if (markers && !markers.IsEmpty())
        {
            IA_AreaMarker firstMarker = markers.Get(0);
            if (firstMarker)
                activeGroup = firstMarker.m_areaGroup;
        }
        
        // If we couldn't get the group from markers, use the vehicle manager
        if (activeGroup < 0)
        {
            activeGroup = IA_VehicleManager.GetActiveGroup();
        }
        
       //Print("[DEBUG_CIV_VEHICLES] Using area group: " + activeGroup + " for vehicle spawning", LogLevel.NORMAL);
        
        for (int i = 0; i < initialCivVehicles; i++)
        {
            // Find a road position to spawn the vehicle on
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(m_area.GetOrigin(), m_area.GetRadius() * 0.8, activeGroup);
            vector spawnPos;
            
            // If a road was found, use it; otherwise fall back to random position
            if (roadPos != vector.Zero)
            {
                spawnPos = roadPos;
               //Print("[DEBUG_CIV_VEHICLES] Found road location at " + spawnPos + " for vehicle " + (i+1), LogLevel.NORMAL);
            }
            else
            {
                // Fallback to random position if no road found
                spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 0.7, m_area.GetOrigin());
               //Print("[DEBUG_CIV_VEHICLES] No road found, using random position at " + spawnPos + " for vehicle " + (i+1), LogLevel.WARNING);
            }
            
            // Spawn the civilian vehicle (using IA_Faction.CIV and civilian-only flag)
           //Print("[DEBUG_CIV_VEHICLES] Attempting to spawn civilian vehicle " + (i+1) + " at position: " + spawnPos, LogLevel.NORMAL);
            Vehicle vehicle = IA_VehicleManager.SpawnRandomVehicle(IA_Faction.CIV, true, false, spawnPos);
            
            if (vehicle)
            {
               //Print("[DEBUG_CIV_VEHICLES] Successfully spawned civilian vehicle at " + vehicle.GetOrigin().ToString(), LogLevel.NORMAL);
                successfulSpawns++;
                
                // Create AI units and place them in the vehicle
                IA_AiGroup civGroup = IA_VehicleManager.PlaceUnitsInVehicle(vehicle, IA_Faction.CIV, m_area.GetOrigin(), this);
                
                // Register the vehicle and group with our civilian tracking
                if (civGroup)
                {
                   //Print("[DEBUG_CIV_VEHICLES] Created civilian AI group for vehicle, registering vehicle with group", LogLevel.NORMAL);
                    RegisterCivilianVehicle(vehicle, civGroup);
                }
                else
                {
                    // If no group was created, just track the vehicle
                   //Print("[DEBUG_CIV_VEHICLES] No civilian AI group created for vehicle, tracking vehicle only", LogLevel.NORMAL);
                    m_areaCivVehicles.Insert(vehicle);
                }
            }
            else
            {
               //Print("[DEBUG_CIV_VEHICLES] Failed to spawn civilian vehicle at position: " + spawnPos, LogLevel.WARNING);
            }
        }
        
       //Print("[DEBUG_CIV_VEHICLES] SpawnInitialCivVehicles completed - Successfully spawned " + successfulSpawns + " of " + initialCivVehicles + " civilian vehicles", LogLevel.NORMAL);
    }

    // New task for processing AI reactions
    private void AIReactionsTask()
    {
        // Process reactions for all military groups
        foreach (IA_AiGroup group : m_military)
        {
            if (group && group.IsSpawned())
            {
                group.ProcessReactions();
            }
        }
        
        // Process reactions for civilian groups if needed
        foreach (IA_AiGroup group : m_civilians)
        {
            if (group && group.IsSpawned())
            {
                group.ProcessReactions();
            }
        }
    }
}