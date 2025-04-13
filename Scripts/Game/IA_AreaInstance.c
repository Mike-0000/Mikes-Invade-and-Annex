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

    // --- Dynamic Task Variables ---
    private SCR_TriggerTask m_currentTaskEntity;
    private ref array<SCR_TriggerTask> m_taskQueue = {};

	
	
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
	
	    int groupCount = area.GetMilitaryAiGroupCount();
	    inst.GenerateRandomAiGroups(groupCount, true);
	
	    int civCount = area.GetCivilianCount();
	    inst.GenerateCivilians(civCount);
	
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
            // Placeholder cycle
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
            
        // Count how many vehicles we're using to limit vehicle usage
        int vehiclesInUse = 0;
        int maxVehiclesAllowed = 2; // Maximum number of vehicles to use at once
        
        Print("[DEBUG] MilitaryOrderTask processing " + m_military.Count() + " military groups", LogLevel.NORMAL);
        
        foreach (IA_AiGroup g : m_military)
        {
            if(!g)
                continue;
            if (g.GetAliveCount() == 0)
                continue;
            
            // VALIDATION: If a unit has a referenced entity but IsDriving is false, clear the reference
            if (g.GetReferencedEntity() && !g.IsDriving())
            {
                Print("[DEBUG_VEHICLE_REPAIR] Found inconsistency: Group has referenced entity but IsDriving=false. Clearing reference.", LogLevel.WARNING);
                g.ClearVehicleReference();
            }
            
            Print("[DEBUG_VEHICLE_TRACKING] Group state: " + g.GetStateDescription() + ", IsDriving: " + g.IsDriving() + ", HasReferencedEntity: " + (g.GetReferencedEntity() != null), LogLevel.NORMAL);
			
            // Check if the group is already using a vehicle
            if (g.IsDriving())
            {
                Print("[DEBUG_VEHICLE_TRACKING] Group is marked as driving", LogLevel.NORMAL);
                // Existing logic for already driving groups (handled by UpdateVehicleOrders)
                Vehicle vehicle = Vehicle.Cast(g.GetReferencedEntity());
                if (vehicle)
                {
                    Print("[DEBUG_VEHICLE_TRACKING] Successfully cast referenced entity to Vehicle", LogLevel.NORMAL);
                    vector vehiclePos = vehicle.GetOrigin();
                    vector drivingTarget = g.GetDrivingTarget();
                    float distance = vector.Distance(vehiclePos, drivingTarget);
                    
                    Print("[DEBUG_VEHICLES] Vehicle at " + vehiclePos.ToString() + ", target: " + drivingTarget.ToString() + ", distance: " + distance, LogLevel.NORMAL);
                    
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
                        Print("[DEBUG_VEHICLE_TRACKING] No active waypoint, finding random road in area", LogLevel.NORMAL);
                        vector roadTarget = IA_VehicleManager.FindRandomRoadEntityInZone(centerPoint, radiusToUse, currentActiveGroup);
                        
                        if (roadTarget != vector.Zero)
                        {
                            Print("[DEBUG_VEHICLE_TRACKING] Found random road at: " + roadTarget, LogLevel.NORMAL);
                            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, roadTarget);
                        }
                        else
                        {
                            // Fallback to random position if no road found
                            vector randomPos = IA_Game.rng.GenerateRandomPointInRadius(1, radiusToUse, centerPoint);
                            Print("[DEBUG_VEHICLE_TRACKING] No road found, using random position: " + randomPos, LogLevel.WARNING);
                            IA_VehicleManager.UpdateVehicleWaypoint(vehicle, g, randomPos);
                        }
                    }
                    
                    g.UpdateVehicleOrders(); 
                }
                else
                {
                    // If we can't cast to Vehicle, clear the incorrect driving state
                    Print("[DEBUG_VEHICLE_REPAIR] IsDriving=true but failed to cast referenced entity to Vehicle. Clearing driving state.", LogLevel.ERROR);
                    g.ClearVehicleReference();
                }
                vehiclesInUse++;
                continue; // Skip to next group if already driving
            }
                
            // Logic for groups NOT currently driving
            // Random order refresh time between 30-60 seconds instead of fixed 45
            int randomOrderRefresh = 30 + IA_Game.rng.RandInt(0, 30);
            if (g.TimeSinceLastOrder() >= randomOrderRefresh)
                g.RemoveAllOrders();
                
            if (!g.HasActiveWaypoint()) // Needs a new order
            {
                // Randomize vehicle assignment chance between 5-15% instead of fixed 10%
                int vehicleAssignmentChance = 5 + IA_Game.rng.RandInt(0, 10);
                bool tryAssignVehicle = (IA_Game.rng.RandInt(0, 100) < vehicleAssignmentChance && vehiclesInUse < maxVehiclesAllowed);
                Print("[DEBUG_VEHICLE_TRACKING] Trying to assign vehicle: " + tryAssignVehicle + ", vehicles in use: " + vehiclesInUse + "/" + maxVehiclesAllowed, LogLevel.NORMAL);
                
                Vehicle vehicleToAssign = null;
                if (tryAssignVehicle)
                {
                    int currentGroup = -1;
                    IA_MissionInitializer missionInit = IA_AreaMarker.s_missionInitializer;
                    if (missionInit) currentGroup = missionInit.GetCurrentIndex();
                    Print("[DEBUG_VEHICLE_TRACKING] Current group index: " + currentGroup, LogLevel.NORMAL);
                    
                    if (currentGroup >= 0)
                    {
                        // Try finding an existing nearby unreserved vehicle
                        vehicleToAssign = IA_VehicleManager.GetClosestUnreservedVehicleInGroup(g.GetOrigin(), currentGroup, 100);
                        Print("[DEBUG_VEHICLE_TRACKING] Found existing vehicle: " + vehicleToAssign, LogLevel.NORMAL);
                        
                        if (vehicleToAssign && IA_VehicleManager.IsVehicleOccupied(vehicleToAssign))
                        {
                            Print("[DEBUG_VEHICLE_TRACKING] Vehicle is occupied, not using it", LogLevel.NORMAL);
                            vehicleToAssign = null; // Don't use occupied vehicles
                        }
                        
                        // If no existing one, try spawning (check limit again)
                        if (!vehicleToAssign && vehiclesInUse < maxVehiclesAllowed)
                        {
                             // Try spawning at designated points
                             array<IA_VehicleSpawnPoint> spawnPoints = IA_VehicleSpawnPoint.GetSpawnPointsByGroup(currentGroup);
                             Print("[DEBUG_VEHICLE_TRACKING] Found " + spawnPoints.Count() + " spawn points for group " + currentGroup, LogLevel.NORMAL);
                             
                             foreach (IA_VehicleSpawnPoint point : spawnPoints)
                             {
                                 if (point.CanSpawnVehicle())
                                 {
                                     Print("[DEBUG_VEHICLE_TRACKING] Attempting to spawn vehicle at point", LogLevel.NORMAL);
                                     vehicleToAssign = point.SpawnRandomVehicle(m_faction);
                                     if (vehicleToAssign) 
                                     {
                                         Print("[DEBUG_VEHICLE_TRACKING] Successfully spawned vehicle: " + vehicleToAssign, LogLevel.NORMAL);
                                         break; // Found one
                                     }
                                 }
                             }
                            
                             // If still no vehicle, try spawning randomly in area (last resort, randomized chance 30-70%)
                             if (!vehicleToAssign && vehiclesInUse < maxVehiclesAllowed)
                             {
                                int randomSpawnChance = 30 + IA_Game.rng.RandInt(0, 40);
                                if (IA_Game.rng.RandInt(0, 100) < randomSpawnChance)
                                {
                                    Print("[DEBUG_VEHICLE_TRACKING] Attempting to spawn random vehicle in area", LogLevel.NORMAL);
                                    vehicleToAssign = IA_VehicleManager.SpawnRandomVehicleInAreaGroup(
                                        m_faction, true, true, currentGroup
                                    );
                                    if (vehicleToAssign)
                                    {
                                        Print("[DEBUG_VEHICLE_TRACKING] Successfully spawned random vehicle in area", LogLevel.NORMAL);
                                    }
                                }
                             }
                        }
                    }
                }
                
                // --- Assign Order --- 
                if (vehicleToAssign) // Assign vehicle order
                {
                    Print("[DEBUG_VEHICLE_TRACKING] About to assign vehicle: " + vehicleToAssign, LogLevel.NORMAL);
                    
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
                    
                    // Directly find a random road entity in the area
                    vector roadTargetPos = IA_VehicleManager.FindRandomRoadEntityInZone(centerPoint, radiusToUse, currentActiveGroup);
                    
                    // Fallback if no road found
                    if (roadTargetPos == vector.Zero)
                    {
                        roadTargetPos = IA_Game.rng.GenerateRandomPointInRadius(1, radiusToUse, centerPoint);
                        Print("[DEBUG_VEHICLES] No road found, using random point: " + roadTargetPos, LogLevel.WARNING);
                    }
                    else
                    {
                        Print("[DEBUG_VEHICLES] Found random road target: " + roadTargetPos, LogLevel.NORMAL);
                    }
                    
                    Print("[DEBUG_VEHICLES] Assigning vehicle to group. Target: " + roadTargetPos, LogLevel.NORMAL);
                    
                    // Call this method and check its result
                    g.AssignVehicle(vehicleToAssign, roadTargetPos);
                    
                    // Check if assignment was successful
                    Print("[DEBUG_VEHICLE_TRACKING] After assignment - IsDriving: " + g.IsDriving() + ", ReferencedEntity: " + g.GetReferencedEntity(), LogLevel.NORMAL);
                    
                    // If assignment failed but we have a valid vehicle, try manually forcing the driving state as a test
                    if (!g.IsDriving() && vehicleToAssign)
                    {
                        Print("[DEBUG_VEHICLE_TRACKING] Vehicle assignment didn't set driving state, trying manual state forcing", LogLevel.WARNING);
                        g.ForceDrivingState(true);
                        Print("[DEBUG_VEHICLE_TRACKING] After forcing - IsDriving: " + g.IsDriving(), LogLevel.NORMAL);
                    }
                    
                    vehiclesInUse++;
                }
                else // Assign regular infantry order
                {    
                    // Generate a random position for infantry
                    vector potentialTargetPos;
                    int infantryGroupNumber = IA_VehicleManager.GetActiveGroup();
                    vector infantryGroupCenter = IA_AreaMarker.CalculateGroupCenterPoint(infantryGroupNumber);
                    float infantryGroupRadius = IA_AreaMarker.CalculateGroupRadius(infantryGroupNumber);
                    
                    // Use appropriate center point and radius
                    vector infantryCenterPoint;
                    if (infantryGroupCenter != vector.Zero)
                        infantryCenterPoint = infantryGroupCenter;
                    else
                        infantryCenterPoint = m_area.GetOrigin();
                    
                    float infantryRadiusToUse;
                    if (infantryGroupRadius > 0)
                        infantryRadiusToUse = infantryGroupRadius * 0.6;
                    else
                        infantryRadiusToUse = m_area.GetRadius() * 0.6;
                    
                    // Generate the random position
                    potentialTargetPos = IA_Game.rng.GenerateRandomPointInRadius(1, infantryRadiusToUse, infantryCenterPoint);
                    
                    Print("[DEBUG] Assigning infantry order to group. Target: " + potentialTargetPos + ", Order: " + infantryOrder, LogLevel.NORMAL);
                    
                    g.AddOrder(potentialTargetPos, infantryOrder); 
                }
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
            
            // Random order refresh time between 35-55 seconds instead of fixed 45
            int randomOrderRefresh = 35 + IA_Game.rng.RandInt(0, 20);
            if (g.TimeSinceLastOrder() >= randomOrderRefresh)
            {
                g.RemoveAllOrders();
            }
			if (!g.HasActiveWaypoint())
            {
                // Instead of just using m_area's radius, consider the entire group
                int civGroupForRadius = IA_VehicleManager.GetActiveGroup();
                vector civGroupCenter = IA_AreaMarker.CalculateGroupCenterPoint(civGroupForRadius);
                float civGroupRadius = IA_AreaMarker.CalculateGroupRadius(civGroupForRadius);
                
                // Use group center and radius if available, otherwise fall back to current area
                vector civOriginPoint;
                if (civGroupCenter != vector.Zero)
                    civOriginPoint = civGroupCenter;
                else
                    civOriginPoint = m_area.GetOrigin();
                
                float civRadiusToUse;
                if (civGroupRadius > 0)
                    civRadiusToUse = civGroupRadius;
                else
                    civRadiusToUse = m_area.GetRadius();
                
			    vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, civRadiusToUse, civOriginPoint);
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
        int strCounter = m_strength;
        for (int i = 0; i < number; i = i + 1)
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
            if (unitCount <= 0) 
            {
                Print("[IA_AreaInstance.GenerateRandomAiGroups] Invalid unit count (" + unitCount + ") for squad type " + st + ", skipping group.", LogLevel.WARNING);
                continue;
            }
            
            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(pos, m_faction, unitCount);
            if (!grp)
            {
                Print("[IA_AreaInstance.GenerateRandomAiGroups] Failed to create military group from units.", LogLevel.WARNING);
                continue;
            }
            // Note: CreateMilitaryGroupFromUnits already handles marking as spawned and water checks
            // --- END NEW LOGIC ---

            // --- OLD LOGIC (Commented out/Removed) ---
            // IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroup(pos, st, m_faction);
            // grp.Spawn();
            // --- END OLD LOGIC ---

            strCounter = strCounter + grp.GetAliveCount();
            m_military.Insert(grp);
        }
        OnStrengthChange(strCounter);
    }

    void GenerateCivilians(int number)
    {
		    if (!m_area)
    {
        //Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
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
};



