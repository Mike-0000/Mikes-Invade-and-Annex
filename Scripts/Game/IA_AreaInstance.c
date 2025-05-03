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

    // --- BEGIN ADDED: Centralized State Tracking ---
    private ref map<IA_AiGroup, IA_GroupTacticalState> m_assignedGroupStates = new map<IA_AiGroup, IA_GroupTacticalState>();
    // New stability tracking
    private ref map<IA_AiGroup, int> m_stateStability = new map<IA_AiGroup, int>();
    // --- END ADDED ---

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
                                // --- BEGIN ADDED: Remove from state map ---
                                if (m_assignedGroupStates.Contains(group))
                                    m_assignedGroupStates.Remove(group);
                                // --- END ADDED ---
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
	
	    // Initialize central reaction manager
	    inst.m_centralReactionManager = new IA_AIReactionManager();
	    inst.m_lastReactionProcessTime = System.GetUnixTime();
	    
	    // --- BEGIN ADDED: Initialize attrition tracking ---
	    // Set initial max historical strength to current strength
	    inst.m_maxHistoricalStrength = inst.m_strength;
	    inst.m_initialTotalUnits = inst.m_strength;
	    // --- END ADDED ---

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
            
            // --- BEGIN ADDED: Assign initial state ---
            // Assign a default state when group is added
            IA_GroupTacticalState initialState = IA_GroupTacticalState.DefendPatrol;
            if (IsUnderAttack()) // If area is already under attack, start defending/attacking
            {
                if (Math.RandomFloat01() < 0.5) 
                    initialState = IA_GroupTacticalState.Defending; 
                else 
                    initialState = IA_GroupTacticalState.Attacking;
            }
            m_assignedGroupStates.Insert(group, initialState);
            // Enforce this initial state immediately (might be redundant if group handles it)
            group.SetTacticalState(initialState, group.GetOrigin(), null, true); // Added true for fromAuthority parameter
            Print(string.Format("[AreaInstance.AddMilitaryGroup] Assigned initial state %1 to group at %2", 
                typename.EnumToString(IA_GroupTacticalState, initialState), group.GetOrigin().ToString()), LogLevel.NORMAL);
            // --- END ADDED ---
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
        
        // --- BEGIN ADDED: Update max historical strength ---
        if (newVal > m_maxHistoricalStrength)
        {
            m_maxHistoricalStrength = newVal;
            Print(string.Format("[AreaInstance.OnStrengthChange] new maximum strength: %1", m_maxHistoricalStrength), LogLevel.DEBUG);
        }
        // --- END ADDED ---
        
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
        
        // --- BEGIN ADDED: Update max historical strength if needed ---
        if (totalCount > m_maxHistoricalStrength)
        {
            m_maxHistoricalStrength = totalCount;
        }
        
        // Calculate attrition ratio and determine criticality
        float attritionRatio = 0;
        if (m_maxHistoricalStrength > 0)
        {
            attritionRatio = 1.0 - (totalCount / (float)m_maxHistoricalStrength);
            
            // Update critical state based on attrition
            bool wasCritical = m_criticalState;
            m_criticalState = (attritionRatio >= m_unitLossThreshold);
            
            // Log when transitioning into or out of critical state
            if (m_criticalState != wasCritical)
            {
                string transitionType;
                if (m_criticalState)
                {
                    transitionType = "ENTERING";
                }
                else
                {
                    transitionType = "EXITING";
                }
                
                Print(string.Format("[AreaInstance.StrengthUpdateTask] Area '%1' %2 CRITICAL STATE: Current=%3, Max=%4, Attrition=%5",
                    m_area.GetName(),
                    transitionType,
                    totalCount,
                    m_maxHistoricalStrength,
                    attritionRatio), LogLevel.WARNING);
            }
        }
        // --- END ADDED ---
        
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
        if (!this || !m_area || !m_canSpawn || m_military.IsEmpty())
            return;

        // Get current time for duration calculations
        int currentTime = System.GetUnixTime();
        
        // Update reassignment cooldown
        if (m_reassignmentCooldown > 0)
            m_reassignmentCooldown--;

        // --- NEW SECTION: Check for pending state change requests from groups ---
        // Handle any pending state change requests before doing normal processing
        foreach (IA_AiGroup g : m_military)
        {
            if (!g || g.GetAliveCount() == 0) 
                continue;
                
            // Check if this group has a pending state change request
            if (g.HasPendingStateRequest())
            {
                IA_GroupTacticalState requestedState = g.GetRequestedState();
                vector requestedPosition = g.GetRequestedStatePosition();
                IEntity requestedEntity = g.GetRequestedStateEntity();
                
                // Get the group's current state for comparison
                IA_GroupTacticalState currentState = g.GetTacticalState();
                
                // Print debug info about the request
                Print(string.Format("[AreaInstance.MilitaryTask] Processing state change request from Group %1: %2 -> %3", 
                    g.GetOrigin().ToString(),
                    typename.EnumToString(IA_GroupTacticalState, currentState),
                    typename.EnumToString(IA_GroupTacticalState, requestedState)), LogLevel.NORMAL);
                
                // Decide whether to approve or deny the request
                bool approveRequest = false;
                
                // Critical state changes (retreating when in danger, etc.) should generally be approved
                if (requestedState == IA_GroupTacticalState.Retreating || 
                    requestedState == IA_GroupTacticalState.LastStand)
                {
                    // Almost always approve retreat/last stand requests as they're likely critical
                    approveRequest = true;
                    Print(string.Format("[AreaInstance.MilitaryTask] APPROVED critical state change to %1 for Group %2", 
                        typename.EnumToString(IA_GroupTacticalState, requestedState),
                        g.GetOrigin().ToString()), LogLevel.WARNING);
                }
                else if (requestedState == IA_GroupTacticalState.Defending && 
                        (currentState == IA_GroupTacticalState.Attacking || currentState == IA_GroupTacticalState.Flanking))
                {
                    // Usually approve requests to fall back to defense when currently in offensive mode
                    // This indicates the group is likely under heavy pressure
                    approveRequest = true;
                    Print(string.Format("[AreaInstance.MilitaryTask] APPROVED tactical withdrawal to defensive position for Group %1", 
                        g.GetOrigin().ToString()), LogLevel.NORMAL);
                }
                else if (m_forceDefend && requestedState != IA_GroupTacticalState.Defending)
                {
                    // If area is in emergency defense mode, reject non-defense requests
                    approveRequest = false;
                    Print(string.Format("[AreaInstance.MilitaryTask] DENIED state change to %1 for Group %2 - Area is in emergency defense mode", 
                        typename.EnumToString(IA_GroupTacticalState, requestedState),
                        g.GetOrigin().ToString()), LogLevel.WARNING);
                }
                else
                {
                    // For other cases, approve with a moderate probability
                    // This balances unit autonomy with central coordination
                    approveRequest = (Math.RandomFloat(0, 1) < 0.7);
                    
                    if (approveRequest)
                    {
                        Print(string.Format("[AreaInstance.MilitaryTask] APPROVED standard state change to %1 for Group %2", 
                            typename.EnumToString(IA_GroupTacticalState, requestedState),
                            g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                    else
                    {
                        Print(string.Format("[AreaInstance.MilitaryTask] DENIED standard state change to %1 for Group %2 - maintaining area cohesion", 
                            typename.EnumToString(IA_GroupTacticalState, requestedState),
                            g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                }
                
                // If approved, apply the requested state
                if (approveRequest)
                {
                    // Apply the state change with the authority flag
                    g.SetTacticalState(requestedState, requestedPosition, requestedEntity, true);
                    
                    // Update our state tracking for this group
                    if (m_assignedGroupStates.Contains(g))
                    {
                        m_assignedGroupStates.Set(g, requestedState);
                        m_stateStability.Set(g, 0); // Reset stability when state changes
                        m_stateStartTimes.Set(g, currentTime); // Reset start time
                    }
                    else
                    {
                        m_assignedGroupStates.Insert(g, requestedState);
                        m_stateStability.Insert(g, 0);
                        m_stateStartTimes.Insert(g, currentTime);
                    }
                    
                    // Reset investigation tracking when state changes
                    if (m_hasInvestigatedThreat.Contains(g))
                    {
                        m_hasInvestigatedThreat.Set(g, false);
                    }
                    else
                    {
                        m_hasInvestigatedThreat.Insert(g, false);
                    }
                    
                    if (m_investigationProgress.Contains(g))
                    {
                        m_investigationProgress.Set(g, 0.0);
                    }
                    else
                    {
                        m_investigationProgress.Insert(g, 0.0);
                    }
                }
                
                // Clear the request whether approved or denied
                g.ClearStateRequest();
            }
        }
        // --- END NEW SECTION ---

        // --- Stage 1: Threat Assessment ---
        bool isUnderAttack = IsUnderAttack();
        vector primaryThreatLocation = m_area.GetOrigin(); // Default if no specific threat found
        bool validThreatLocation = false;
        float globalThreatLevel = 0.0;

        if (isUnderAttack)
        {
            // Check if AI attackers are present and engaged
            bool attackersEngaged = false;
            if (m_aiAttackers)
            {
                attackersEngaged = m_aiAttackers.IsAnyEngaged(); 
            }

            if (attackersEngaged)
            {
                // We know attackers are present, but can't get average pos easily.
                // Use the fallback logic to find threat based on own groups' danger.
                Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK by AI Attackers. Assessing threat from own groups.", m_area.GetName()), LogLevel.DEBUG);
                // Fallthrough to the next block
            }
            
            vector dangerSum = vector.Zero;
            int dangerCount = 0;
            float dangerLevelSum = 0.0;
            int dangerLevelCount = 0;
            vector currentDangerPos;
            foreach (IA_AiGroup g : m_military) {
                if (g && g.IsEngagedWithEnemy()) {
                    currentDangerPos = g.GetLastDangerPosition(); // Use getter
                    if(currentDangerPos != vector.Zero)
                    {
                         // Basic check to filter out danger likely *inside* the base when defending
                         if (vector.DistanceSq(currentDangerPos, m_area.GetOrigin()) > 50*50)
                         {
                            dangerSum += currentDangerPos;
                            dangerCount++;
                         }
                    }
                    
                    // Track global threat level
                    float groupDanger = g.GetCurrentDangerLevel();
                    if (groupDanger > 0) {
                        dangerLevelSum += groupDanger;
                        dangerLevelCount++;
                    }
                }
            }
            
            // Calculate global threat level for the area
            if (dangerLevelCount > 0) {
                globalThreatLevel = dangerLevelSum / dangerLevelCount;
            }
            float attritionRatio = GetAttritionRatio();
            
            // --- BEGIN MODIFIED: Use m_criticalState as an additional factor for force_defend decision ---
            // Determine if area is in severe danger and should force defensive posture
            // Higher threshold for non-critical state, lower threshold for critical state
            float dangerThreshold;
            if (m_criticalState)
            {
                dangerThreshold = 0.6;
            }
            else
            {
                dangerThreshold = 0.75;
            }
            
            if ((globalThreatLevel > dangerThreshold) && (attritionRatio >= m_unitLossThreshold)) {
                m_forceDefend = true;
                Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL THREAT LEVEL (%1): Area '%2' is forcing defensive posture (Critical: %3)", 
                    globalThreatLevel, m_area.GetName(), m_criticalState), LogLevel.WARNING);
            } else {
                m_forceDefend = false;
            }
            
            // When in critical state, be more conservative about flanking
            float flankingThreshold;
            if (m_criticalState)
            {
                flankingThreshold = 0.5;
            }
            else
            {
                flankingThreshold = 0.6;
            }
            m_allowFlankingOperations = !m_forceDefend && globalThreatLevel < flankingThreshold;
            // --- END MODIFIED ---
            
            if (dangerCount > 0) {
                primaryThreatLocation = dangerSum / dangerCount;
                validThreatLocation = true;
                Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK. Threat assessed from own groups near: %2", m_area.GetName(), primaryThreatLocation.ToString()), LogLevel.DEBUG);
            } else {
                 Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK. No specific threat location found, using origin.", m_area.GetName()), LogLevel.DEBUG);
            }
        } else {
             Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' SECURE.", m_area.GetName()), LogLevel.DEBUG);
             m_forceDefend = false;
             m_allowFlankingOperations = true;
        }

        // --- Stage 2: Role Calculation ---
        int totalMilitaryGroups = m_military.Count();
        int targetDefenders = 0;
        int targetAttackers = 0;
        int targetFlankers = 0;
        IA_GroupTacticalState defaultStateForArea;

        if (!isUnderAttack || totalMilitaryGroups == 0) // Also handle case with 0 groups
        {
            defaultStateForArea = IA_GroupTacticalState.DefendPatrol;
            targetDefenders = totalMilitaryGroups; // All patrol or 0
            targetAttackers = 0;
            targetFlankers = 0;
             Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Secure/Empty): Default State: DefendPatrol. Def=%1", targetDefenders), LogLevel.DEBUG);
        }
        else if (m_forceDefend) // Emergency defensive mode - most units defend
        {
            defaultStateForArea = IA_GroupTacticalState.Defending;
            
            // --- BEGIN MODIFIED: Adjust defender percentage based on critical state ---
            // In emergency mode + critical state, 90% defend instead of 80%
            float defenderPercentage;
            if (m_criticalState)
            {
                defenderPercentage = 0.9;
            }
            else
            {
                defenderPercentage = 0.8;
            }
            targetDefenders = Math.Max(1, Math.Round(totalMilitaryGroups * defenderPercentage));
            // --- END MODIFIED ---
            
            targetAttackers = totalMilitaryGroups - targetDefenders;
            targetFlankers = 0; // No flanking in emergency mode
            
            Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (EMERGENCY): Def=%1, Att=%2, Flk=0, Critical=%3", 
                targetDefenders, targetAttackers, m_criticalState), LogLevel.WARNING);
        }
        else // Area is under attack AND has groups - normal combat mode
        {
            defaultStateForArea = IA_GroupTacticalState.Attacking; // Default response

            // --- BEGIN MODIFIED: Adjust defender percentage based on critical state ---
            // 1. Calculate Defenders - more defenders when in critical state
            float defenderPercentage;
            if (m_criticalState)
            {
                defenderPercentage = 0.6;
            }
            else
            {
                defenderPercentage = 0.45; // 60% vs 45%
            }
            targetDefenders = Math.Max(1, Math.Round(totalMilitaryGroups * defenderPercentage));
            // --- END MODIFIED ---

            // 2. Calculate available Offensive groups
            int offensiveGroups = totalMilitaryGroups - targetDefenders;

            // 3. Calculate Flankers and Attackers from Offensive groups based on 1:2 ratio
            if (offensiveGroups > 0 && validThreatLocation && m_allowFlankingOperations)
            {
                // --- BEGIN MODIFIED: Adjust flanker percentage based on critical state ---
                // Reduce flankers when in critical state
                float flankerRatio;
                if (m_criticalState)
                {
                    flankerRatio = 0.2;
                }
                else
                {
                    flankerRatio = 0.33; // 1:4 vs 1:2 ratio
                }
                targetFlankers = Math.Round(offensiveGroups * flankerRatio);
                // --- END MODIFIED ---

                // If we have few offensive units (e.g., 1 or 2), ensure proper allocation
                if (offensiveGroups < 3)
                {
                    if (offensiveGroups == 2)
                    {
                        // With 2 offensive units, assign 1 flanker and 1 attacker (1:1)
                        // Unless in critical state, then both attack
                        if (m_criticalState)
                        {
                            targetFlankers = 0;
                        }
                        else
                        {
                            targetFlankers = 1;
                        }
                    }
                    else // offensiveGroups == 1
                    {
                        // With just 1 offensive unit, make it an attacker
                        targetFlankers = 0;
                    }
                }

                targetAttackers = offensiveGroups - targetFlankers;

                // Calculate ratio for debug message without ternary operator
                int attackerToFlankerRatio = 0;
                if (targetFlankers > 0)
                {
                    attackerToFlankerRatio = Math.Round(targetAttackers / targetFlankers);
                }
                
                Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Under Attack - Offensive): Off=%1 -> Flank=%2, Attack=%3 (1:%4 ratio), Critical=%5",
                    offensiveGroups, targetFlankers, targetAttackers, attackerToFlankerRatio, m_criticalState), LogLevel.DEBUG);
            }
            else // Not enough offensive groups or no valid threat location for flanking
            {
                targetFlankers = 0;
                targetAttackers = offensiveGroups; // Assign all remaining as attackers (could be 0)
                Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Under Attack - No Flank): Off=%1 -> Flank=0, Attack=%2 (ThreatValid=%3)",
                    offensiveGroups, targetAttackers, validThreatLocation), LogLevel.DEBUG);
            }

            // Final Sanity Check - Adjust attackers if counts don't match total
            if (targetDefenders + targetAttackers + targetFlankers != totalMilitaryGroups) {
                 Print(string.Format("[AreaInstance.MilitaryTask] Role Calc WARNING: Role counts don't match total! Total=%1, Def=%2, Att=%3, Flk=%4",
                    totalMilitaryGroups, targetDefenders, targetAttackers, targetFlankers), LogLevel.ERROR);
                 // Adjust attackers to fix discrepancy as a fallback
                 targetAttackers = totalMilitaryGroups - targetDefenders - targetFlankers;
            }

            Print(string.Format("[AreaInstance.MilitaryTask] Role Calc Final (Under Attack): Total=%1, Def=%2, Flank=%3, Attack=%4, Critical=%5",
                totalMilitaryGroups, targetDefenders, targetFlankers, targetAttackers, m_criticalState), LogLevel.DEBUG);
        }

        // --- Stage 3: Authority Check - Only perform major reassignments at intervals ---
        bool canPerformGlobalReassignment = false;
        
        // Check if enough time has passed since the last global authority check
        if (currentTime - m_lastGlobalStateAuthority >= AREA_AUTHORITY_INTERVAL && m_reassignmentCooldown == 0)
        {
            canPerformGlobalReassignment = true;
            m_lastGlobalStateAuthority = currentTime;
            m_reassignmentCooldown = 3; // Set cooldown for 3 cycles
            Print(string.Format("[AreaInstance.MilitaryTask] GLOBAL AUTHORITY CHECK: Authorizing potential reassignments", m_area.GetName()), LogLevel.NORMAL);
        }
        
        // --- Stage 4: Pre-computation & Peril Checks ---
        // Store current assignments and identify groups needing role change due to peril
        map<IA_AiGroup, IA_GroupTacticalState> currentAssignments = new map<IA_AiGroup, IA_GroupTacticalState>();
        map<IA_AiGroup, bool> needsRoleUpdate = new map<IA_AiGroup, bool>(); // Tracks groups whose state MUST change this cycle
        const int STATE_STABILITY_THRESHOLD = 10; // Only for reference in this logic

        foreach (IA_AiGroup g : m_military)
        {
            if (!g || g.GetAliveCount() == 0) continue;
            if (g.IsDriving()) continue; // Skip driving groups for role assignment

            // Get current state information
            IA_GroupTacticalState currentAssignedState = IA_GroupTacticalState.Neutral;
            if (!m_assignedGroupStates.Find(g, currentAssignedState))
            {
                // Group somehow not in map, assign default and mark for update
                currentAssignedState = defaultStateForArea;
                m_assignedGroupStates.Insert(g, currentAssignedState);
                m_stateStability.Insert(g, 0); // New state, so stability is 0
                m_stateStartTimes.Insert(g, currentTime); // Record state start time
                needsRoleUpdate.Insert(g, true);
                Print(string.Format("[AreaInstance.MilitaryTask] WARNING: Group %1 not in map, assigned %2.",
                    g.GetOrigin().ToString(), 
                    typename.EnumToString(IA_GroupTacticalState, currentAssignedState)), LogLevel.WARNING);
            }
            else
            {
                // Increase stability counter for groups with existing state
                int stability = 0;
                m_stateStability.Find(g, stability);
                m_stateStability.Set(g, stability + 1);
                
                // Check if the state duration is sufficient to allow changes
                int stateStartTime = currentTime;
                if (!m_stateStartTimes.Find(g, stateStartTime))
                {
                    // No start time recorded, set it now
                    m_stateStartTimes.Insert(g, currentTime);
                }
                
                // Determine if the state has been active long enough to consider changes
                int stateDuration = currentTime - stateStartTime;
                bool stateTimeLocked = stateDuration < MINIMUM_STATE_DURATION;
                
                // If state is time-locked, log it and prevent further changes
                if (stateTimeLocked && !needsRoleUpdate.Contains(g))
                {
                    int remainingLockTime = MINIMUM_STATE_DURATION - stateDuration;
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 state LOCKED for %2 more seconds", 
                        g.GetOrigin().ToString(), remainingLockTime), LogLevel.DEBUG);
                }
            }
            
            currentAssignments.Insert(g, currentAssignedState); // Store current assignment

            // --- Peril Checks (these always override time locks) ---
            bool perilDetected = false;
            IA_GroupTacticalState stateAfterPeril = currentAssignedState;

            // Defender Peril Check: High danger AND significant losses?
            if (currentAssignedState == IA_GroupTacticalState.Defending)
            {
                float danger = g.GetCurrentDangerLevel();
                float healthPercent = 1.0;
                int initialCount = g.GetInitialUnitCount(); // Use getter
                if (initialCount > 0) healthPercent = (float)g.GetAliveCount() / initialCount; // Use getter

                // Condition: Danger > 0.7 AND health < 50%
                if (danger > 0.7 && healthPercent < 0.5)
                {
                    perilDetected = true;
                    stateAfterPeril = IA_GroupTacticalState.Defending; // Force defend near origin
                    Print(string.Format("[AreaInstance.MilitaryTask] PERIL (Defender): Group %1 (Danger: %2, Health: %3). Forcing Defend at Origin.", g.GetOrigin().ToString(), danger, healthPercent), LogLevel.WARNING);
                }
            }
            // Attacker/Flanker Peril Check: Critically low members?
            else if (currentAssignedState == IA_GroupTacticalState.Attacking || currentAssignedState == IA_GroupTacticalState.Flanking)
            {
                 // Condition: Only 1 unit left
                if (g.GetAliveCount() <= 1 && totalMilitaryGroups > 1) // Don't retreat last group
                {
                     perilDetected = true;
                     stateAfterPeril = IA_GroupTacticalState.Defending; // Force defend near origin
                     Print(string.Format("[AreaInstance.MilitaryTask] PERIL (Attacker/Flanker): Group %1 (Units: %2). Forcing Defend at Origin.", g.GetOrigin().ToString(), g.GetAliveCount()), LogLevel.WARNING);
                }
            }

            // Emergency defense check overrides other peril checks
            if (m_forceDefend && (currentAssignedState == IA_GroupTacticalState.Flanking))
            {
                perilDetected = true;
                stateAfterPeril = IA_GroupTacticalState.Defending;
                Print(string.Format("[AreaInstance.MilitaryTask] EMERGENCY MODE: Converting Flanking Group %1 to Defender", 
                    g.GetOrigin().ToString()), LogLevel.WARNING);
            }

            if (perilDetected)
            {
                m_assignedGroupStates.Set(g, stateAfterPeril); // Update assignment map immediately
                currentAssignments.Set(g, stateAfterPeril); // Update local map for this cycle
                m_stateStability.Set(g, 0);            // Reset stability counter for peril-induced change
                m_stateStartTimes.Set(g, currentTime); // Reset state start time
                needsRoleUpdate.Insert(g, true);        // Mark that state *must* be updated
                
                // Reset investigation tracking when role changes in an emergency
                m_hasInvestigatedThreat.Set(g, false);
                m_investigationProgress.Set(g, 0.0);
            }
        } // End of peril check loop

        // --- Stage 5: Role Distribution Accounting ---
        // Count current roles after peril adjustments
        int currentDefenders = 0;
        int currentAttackers = 0;
        int currentFlankers = 0;
        array<IA_AiGroup> availableGroups = {}; // Groups that could potentially change state

        // --- BEGIN ADDED: Track unit counts by role ---
        int totalAliveUnits = 0;
        int attackingAliveUnits = 0;
        int flankingAliveUnits = 0;
        int defendingAliveUnits = 0;
        // --- END ADDED ---

        foreach (IA_AiGroup g, IA_GroupTacticalState assignedState : currentAssignments)
        {
            // Get unit count for this group
            int groupAliveCount = g.GetAliveCount();
            totalAliveUnits += groupAliveCount;

            // Skip groups already marked for updates due to peril
            if (needsRoleUpdate.Contains(g)) 
            {
                if (assignedState == IA_GroupTacticalState.Defending || assignedState == IA_GroupTacticalState.DefendPatrol) 
                {
                    currentDefenders++;
                    defendingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Attacking) 
                {
                    currentAttackers++;
                    attackingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Flanking) 
                {
                    currentFlankers++;
                    flankingAliveUnits += groupAliveCount;
                }
                continue;
            }
            
            // Check if state is time-locked (must maintain for minimum duration)
            int stateStartTime = currentTime;
            m_stateStartTimes.Find(g, stateStartTime);
            int stateDuration = currentTime - stateStartTime;
            
            if (stateDuration < MINIMUM_STATE_DURATION)
            {
                // State is time-locked, count but don't allow changes
                if (assignedState == IA_GroupTacticalState.Defending || assignedState == IA_GroupTacticalState.DefendPatrol) 
                {
                    currentDefenders++;
                    defendingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Attacking) 
                {
                    currentAttackers++;
                    attackingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Flanking) 
                {
                    currentFlankers++;
                    flankingAliveUnits += groupAliveCount;
                }
            }
            else if (!canPerformGlobalReassignment)
            {
                // Not time for global reassignment, count but don't change
                if (assignedState == IA_GroupTacticalState.Defending || assignedState == IA_GroupTacticalState.DefendPatrol) 
                {
                    currentDefenders++;
                    defendingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Attacking) 
                {
                    currentAttackers++;
                    attackingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Flanking) 
                {
                    currentFlankers++;
                    flankingAliveUnits += groupAliveCount;
                }
            }
            else
            {
                // Group can be considered for reassignment during global authority check
                availableGroups.Insert(g);
                
                // Still count them in their current role for now
                if (assignedState == IA_GroupTacticalState.Defending || assignedState == IA_GroupTacticalState.DefendPatrol) 
                {
                    currentDefenders++;
                    defendingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Attacking) 
                {
                    currentAttackers++;
                    attackingAliveUnits += groupAliveCount;
                }
                else if (assignedState == IA_GroupTacticalState.Flanking) 
                {
                    currentFlankers++;
                    flankingAliveUnits += groupAliveCount;
                }
            }
        }
        
        // --- BEGIN ADDED: Check offensive force strength and adjust targets if needed ---
        // Calculate current offensive force percentage
        float offensiveUnitPercentage = 0;
        int offensiveAliveUnits = attackingAliveUnits + flankingAliveUnits;
        
        if (totalAliveUnits > 0)
        {
            offensiveUnitPercentage = offensiveAliveUnits / (float)totalAliveUnits;
        }
        
        // If we're under attack and offensive force is too weak, adjust target roles
        if (isUnderAttack && !m_forceDefend && totalAliveUnits >= 6) // Only rebalance for areas with sufficient units
        {
            // Define minimum offensive force percentage (at least 30% of total force)
            const float MIN_OFFENSIVE_PERCENTAGE = 0.30;
            
            // If offensive percentage is too low, adjust target distribution
            if (offensiveUnitPercentage < MIN_OFFENSIVE_PERCENTAGE)
            {
                Print(string.Format("[AreaInstance.MilitaryTask] FORCE BALANCE WARNING: Offensive units (%1) too low (%2% of total %3). Adjusting targets.", 
                    offensiveAliveUnits, Math.Round(offensiveUnitPercentage * 100), totalAliveUnits), LogLevel.WARNING);
                
                // Original targets before adjustment
                int originalDefenders = targetDefenders;
                int originalAttackers = targetAttackers;
                int originalFlankers = targetFlankers;
                
                // Calculate how many units we need to shift to reach minimum percentage
                int desiredOffensiveUnits = Math.Ceil(totalAliveUnits * MIN_OFFENSIVE_PERCENTAGE);
                int missingOffensiveUnits = desiredOffensiveUnits - offensiveAliveUnits;
                
                // If adjustment needed, shift some groups from defenders to attackers/flankers
                if (missingOffensiveUnits > 0 && defendingAliveUnits > 0 && currentDefenders > 1)
                {
                    // Calculate how many groups to shift based on average group size
                    float avgGroupSize = totalAliveUnits / (float)totalMilitaryGroups;
                    if (avgGroupSize <= 0) avgGroupSize = 1;
                    
                    // Determine how many defender groups to convert
                    int groupsToConvert = Math.Ceil(missingOffensiveUnits / avgGroupSize);
                    
                    // Limit conversion to maintain at least one defender and not exceed total defenders
                    groupsToConvert = Math.Min(groupsToConvert, currentDefenders - 1);
                    
                    // Adjust target numbers
                    targetDefenders = Math.Max(1, targetDefenders - groupsToConvert);
                    
                    // Distribute the converted groups between attackers and flankers (2/3 attackers, 1/3 flankers)
                    int additionalAttackers = Math.Ceil(groupsToConvert * 0.67);
                    int additionalFlankers = groupsToConvert - additionalAttackers;
                    
                    // If flanking not allowed, all become attackers
                    if (!m_allowFlankingOperations || !validThreatLocation)
                    {
                        additionalAttackers = groupsToConvert;
                        additionalFlankers = 0;
                    }
                    
                    targetAttackers += additionalAttackers;
                    targetFlankers += additionalFlankers;
                    
                    Print(string.Format("[AreaInstance.MilitaryTask] FORCE REBALANCE: Adjusting targets from Def:%1/Att:%2/Flk:%3 to Def:%4/Att:%5/Flk:%6", 
                        originalDefenders, originalAttackers, originalFlankers, 
                        targetDefenders, targetAttackers, targetFlankers), LogLevel.WARNING);
                }
            }
        }
        // --- END ADDED ---
        
        // Count current vs target role differences for logging
        int defenderDiff = targetDefenders - currentDefenders;
        int attackerDiff = targetAttackers - currentAttackers;
        int flankerDiff = targetFlankers - currentFlankers;
        
        Print(string.Format("[AreaInstance.MilitaryTask] Role Balance: Current Def=%1/%2, Att=%3/%4, Flk=%5/%6 (Available for changes: %7)",
            currentDefenders, targetDefenders, currentAttackers, targetAttackers, 
            currentFlankers, targetFlankers, availableGroups.Count()), LogLevel.DEBUG);

        // --- Stage 6: Selective Role Reassignment (Only During Authority Checks) ---
        // Only perform role balancing during global authority checks and when we have available groups
        if (canPerformGlobalReassignment && !availableGroups.IsEmpty())
        {
            // Sort groups by stability (less stable first) using a manual bubble sort approach
            int groupCount = availableGroups.Count();
            for (int i = 0; i < groupCount - 1; i++)
            {
                for (int j = 0; j < groupCount - i - 1; j++)
                {
                    IA_AiGroup group1 = availableGroups[j];
                    IA_AiGroup group2 = availableGroups[j + 1];
                    
                    int stability1 = 0;
                    int stability2 = 0;
                    m_stateStability.Find(group1, stability1);
                    m_stateStability.Find(group2, stability2);
                    
                    if (stability1 > stability2)
                    {
                        // Swap to put less stable first
                        availableGroups.SwapItems(j, j + 1);
                    }
                }
            }
            
            // Recalculate current role counts removing available groups
            // This gives us the "fixed" counts after removing groups available for assignment
            int fixedDefenders = currentDefenders;
            int fixedAttackers = currentAttackers;
            int fixedFlankers = currentFlankers;
            
            foreach (IA_AiGroup g : availableGroups)
            {
                IA_GroupTacticalState currentRole = currentAssignments.Get(g);
                
                if (currentRole == IA_GroupTacticalState.Defending || currentRole == IA_GroupTacticalState.DefendPatrol)
                    fixedDefenders--;
                else if (currentRole == IA_GroupTacticalState.Attacking)
                    fixedAttackers--;
                else if (currentRole == IA_GroupTacticalState.Flanking)
                    fixedFlankers--;
            }
            
            // Maximum number of groups to reassign in a single authority check
            // to prevent sudden mass role changes
            int maxReassignments = Math.Max(1, Math.Round(groupCount * 0.3)); // Max 30% of available groups
            int reassignmentCount = 0;
            
            // Determine gaps to fill
            int defenderGap = targetDefenders - fixedDefenders;
            int attackerGap = targetAttackers - fixedAttackers;
            int flankerGap = targetFlankers - fixedFlankers;
            
            Print(string.Format("[AreaInstance.MilitaryTask] Authority Balancing: Needed Def=%1, Att=%2, Flk=%3 (max changes: %4)",
                defenderGap, attackerGap, flankerGap, maxReassignments), LogLevel.NORMAL);

            // Iterate through groups needing assignment and fill roles based on targets
            // Prioritize keeping existing roles if possible
            foreach (IA_AiGroup g : availableGroups)
            {
                // If we've reached max reassignments for this cycle, stop
                if (reassignmentCount >= maxReassignments)
                    break;
                    
                IA_GroupTacticalState currentRole = currentAssignments.Get(g);
                IA_GroupTacticalState finalRole = currentRole; // Assume keeping current role initially
                bool roleChanged = false;
                
                // Check if this group is currently investigating a threat
                bool hasInvestigated = true; // Default to true (investigation complete)
                m_hasInvestigatedThreat.Find(g, hasInvestigated);
                
                // If the group is in attack/flank mode but hasn't completed their investigation,
                // don't reassign them yet
                if ((currentRole == IA_GroupTacticalState.Attacking || currentRole == IA_GroupTacticalState.Flanking) && !hasInvestigated)
                {
                    float progress = 0.0;
                    m_investigationProgress.Find(g, progress);
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 EXEMPT from reassignment - investigation in progress (%2)", 
                        g.GetOrigin().ToString(), progress), LogLevel.DEBUG);
                    continue; // Skip this group for reassignment
                }
                
                // Skip this group if current role already matches a needed role
                if ((currentRole == IA_GroupTacticalState.Defending || currentRole == IA_GroupTacticalState.DefendPatrol) && defenderGap > 0)
                {
                    defenderGap--;
                    continue; // Already in the right role
                }
                else if (currentRole == IA_GroupTacticalState.Attacking && attackerGap > 0)
                {
                    attackerGap--;
                    continue; // Already in the right role
                }
                else if (currentRole == IA_GroupTacticalState.Flanking && flankerGap > 0 && m_allowFlankingOperations)
                {
                    flankerGap--;
                    continue; // Already in the right role
                }
                
                // Get the group's current strength information for role decisions
                int aliveCount = g.GetAliveCount();
                int initialUnitCount = g.GetInitialUnitCount();
                float strengthRatio;
                if (initialUnitCount > 0) {
                    strengthRatio = aliveCount / (float)initialUnitCount;
                } else {
                    strengthRatio = 1.0;
                }

                // Special check: If we need attacker/flanker replacements and this is a defender with good strength,
                // it's a candidate for promotion to an offensive role
                bool isPromotionCandidate = (currentRole == IA_GroupTacticalState.Defending || 
                                            currentRole == IA_GroupTacticalState.DefendPatrol) && 
                                           aliveCount >= 3 && strengthRatio >= 0.65;
                
                // Now reassign based on priorities
                // 1. First priority: Replace lost attackers if needed, but only with strong defenders
                if ((attackerGap > 0 || (flankerGap > 0 && m_allowFlankingOperations)) && isPromotionCandidate)
                {
                    // If we need both attackers and flankers, prioritize attackers unless this unit is at full strength
                    if (attackerGap > 0 && (flankerGap == 0 || strengthRatio < 0.8 || !m_allowFlankingOperations))
                    {
                        finalRole = IA_GroupTacticalState.Attacking;
                        attackerGap--;
                        roleChanged = true;
                        Print(string.Format("[AreaInstance.MilitaryTask] PROMOTING Group %1 from defender to attacker (strength: %2%)", 
                            g.GetOrigin().ToString(), Math.Round(strengthRatio * 100)), LogLevel.WARNING);
                    }
                    else if (flankerGap > 0 && m_allowFlankingOperations)
                    {
                        finalRole = IA_GroupTacticalState.Flanking;
                        flankerGap--;
                        roleChanged = true;
                        Print(string.Format("[AreaInstance.MilitaryTask] PROMOTING Group %1 from defender to flanker (strength: %2%)", 
                            g.GetOrigin().ToString(), Math.Round(strengthRatio * 100)), LogLevel.WARNING);
                    }
                }
                // 2. Emergency defense needs
                else if (m_forceDefend && currentRole != IA_GroupTacticalState.Defending && 
                        currentRole != IA_GroupTacticalState.DefendPatrol && defenderGap > 0)
                {
                    finalRole = IA_GroupTacticalState.Defending;
                    defenderGap--;
                    roleChanged = true;
                }
                // 3. High priority to defense if under target
                else if (defenderGap > 0 && 
                        (currentRole != IA_GroupTacticalState.Defending && currentRole != IA_GroupTacticalState.DefendPatrol))
                {
                    finalRole = IA_GroupTacticalState.Defending;
                    defenderGap--;
                    roleChanged = true;
                }
                // 4. Next priority to flanking if enabled and needed
                else if (flankerGap > 0 && currentRole != IA_GroupTacticalState.Flanking && 
                         m_allowFlankingOperations && validThreatLocation)
                {
                    // Only assign flanking to groups with sufficient strength
                    if (aliveCount >= 3 && strengthRatio >= 0.6)
                    {
                        finalRole = IA_GroupTacticalState.Flanking;
                        flankerGap--;
                        roleChanged = true;
                    }
                    else
                    {
                        // Not strong enough for flanking, fall through to next option
                        Print(string.Format("[AreaInstance.MilitaryTask] Group %1 not strong enough for flanking (strength: %2%)", 
                            g.GetOrigin().ToString(), Math.Round(strengthRatio * 100)), LogLevel.DEBUG);
                    }
                }
                // 5. Last priority to attackers if needed
                else if (attackerGap > 0 && currentRole != IA_GroupTacticalState.Attacking)
                {
                    // Only assign attacking to groups with reasonable strength
                    if (aliveCount >= 2 && strengthRatio >= 0.4)
                    {
                        finalRole = IA_GroupTacticalState.Attacking;
                        attackerGap--;
                        roleChanged = true;
                    }
                    else
                    {
                        // Not strong enough for attacking, keep as defender
                        Print(string.Format("[AreaInstance.MilitaryTask] Group %1 not strong enough for attacking (strength: %2%)", 
                            g.GetOrigin().ToString(), Math.Round(strengthRatio * 100)), LogLevel.DEBUG);
                        finalRole = IA_GroupTacticalState.Defending;
                        defenderGap--;
                        roleChanged = true;
                    }
                }
                
                // If we determined a role change is needed
                if (roleChanged)
                {
                    m_assignedGroupStates.Set(g, finalRole); // Update map
                    needsRoleUpdate.Insert(g, true); // Mark for state enforcement
                    m_stateStability.Set(g, 0); // Reset stability on role change
                    m_stateStartTimes.Set(g, currentTime); // Reset state start time
                    reassignmentCount++; // Count this reassignment
                    
                    Print(string.Format("[AreaInstance.MilitaryTask] AUTHORITY REASSIGNMENT: Group %1 (%2) -> %3", 
                        g.GetOrigin().ToString(), 
                        typename.EnumToString(IA_GroupTacticalState, currentRole), 
                        typename.EnumToString(IA_GroupTacticalState, finalRole)), LogLevel.WARNING);
                }
            }
            
            if (reassignmentCount > 0)
            {
                Print(string.Format("[AreaInstance.MilitaryTask] Authority Check reassigned %1 groups", reassignmentCount), LogLevel.NORMAL);
            }
            else
            {
                Print(string.Format("[AreaInstance.MilitaryTask] Authority Check found no groups requiring reassignment", reassignmentCount), LogLevel.NORMAL);
            }
        }

        // --- Stage 7: Enforcement & Idle Handling ---
        foreach (IA_AiGroup g : m_military)
        {
             if (!g || g.GetAliveCount() == 0) continue;
             if (g.IsDriving()) continue; // Already handled

            IA_GroupTacticalState finalAssignedState = m_assignedGroupStates.Get(g); // Get final role for this cycle
            IA_GroupTacticalState actualState = g.GetTacticalState();
            vector targetForState = primaryThreatLocation; // Default

            // Determine correct target based on FINAL assigned state
            if (finalAssignedState == IA_GroupTacticalState.Defending || finalAssignedState == IA_GroupTacticalState.DefendPatrol)
            {
                 targetForState = m_area.GetOrigin(); // Defend/Patrol around origin
                 
                 // If peril forced defend, use group origin for defenders converting from other roles
                 if(needsRoleUpdate.Contains(g) && (actualState == IA_GroupTacticalState.Attacking || actualState == IA_GroupTacticalState.Flanking)){
                    targetForState = g.GetOrigin(); // Retreating attacker/flanker defends where they are initially
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 Peril Defend Target OVERRIDE to Group Origin.", g.GetOrigin().ToString()), LogLevel.DEBUG);
                 }
            }
            else if (finalAssignedState == IA_GroupTacticalState.Flanking && !validThreatLocation)
            {
                // Fallback if assigned Flank but threat is gone: Attack towards origin
                finalAssignedState = IA_GroupTacticalState.Attacking;
                targetForState = m_area.GetOrigin();
                Print(string.Format("[AreaInstance.MilitaryTask] Group %1 fallback Flank -> Attack Origin", g.GetOrigin().ToString()), LogLevel.DEBUG);
                m_assignedGroupStates.Set(g, finalAssignedState); // Correct map assignment
                needsRoleUpdate.Insert(g, true); // Mark as needing enforcement
            }
            else {
                targetForState = primaryThreatLocation; // Attack/Flank known threat
            }

            // --- MODIFIED LOGIC ---
            // Only enforce state if the assigned state doesn't match the actual state OR
            // if the group is specifically marked as needing an update (e.g., due to peril or reassignment).
            // This prevents redundant SetTacticalState calls if the state is already correct.
            bool needsEnforcement = (actualState != finalAssignedState) && needsRoleUpdate.Contains(g);
            
            if (needsEnforcement)
            {
                 Print(string.Format("[AreaInstance.MilitaryTask] Enforcing state for Group %1: Assigned=%2, Actual=%3. Setting state with target %4...",
                    g.GetOrigin().ToString(),
                    typename.EnumToString(IA_GroupTacticalState, finalAssignedState),
                    typename.EnumToString(IA_GroupTacticalState, actualState),
                    targetForState.ToString()), LogLevel.DEBUG);

                // When setting to attack or flank, register this as start of investigation
                if (finalAssignedState == IA_GroupTacticalState.Attacking || finalAssignedState == IA_GroupTacticalState.Flanking)
                {
                    m_hasInvestigatedThreat.Set(g, false);
                    m_assignedThreatPosition.Set(g, targetForState);
                    m_investigationProgress.Set(g, 0.0);
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 starting threat investigation at %2", 
                        g.GetOrigin().ToString(), targetForState.ToString()), LogLevel.DEBUG);
                }

                g.SetAssignedArea(m_area); // Set the assigned area
                g.SetTacticalState(finalAssignedState, targetForState);
                continue; // Skip idle/timeout for this cycle
            }

            // Track investigation progress for attacking/flanking groups regardless of state enforcement this cycle
            if ((actualState == IA_GroupTacticalState.Attacking || actualState == IA_GroupTacticalState.Flanking))
            {
                // Get the group's current position
                vector groupPosition = g.GetOrigin();
                vector threatPosition = vector.Zero;
                bool hasTarget = m_assignedThreatPosition.Find(g, threatPosition);
                bool hasInvestigated = false;
                m_hasInvestigatedThreat.Find(g, hasInvestigated);
                
                // If we're tracking a threat position for this group
                if (hasTarget && threatPosition != vector.Zero && !hasInvestigated)
                {
                    // Calculate distance to threat
                    float distanceToThreat = vector.Distance(groupPosition, threatPosition);
                    
                    // Determine if group has reached threat location
                    bool reachedThreat = distanceToThreat <= INVESTIGATION_COMPLETE_DISTANCE;
                    
                    // Calculate investigation progress based on distance
                    float startDistance = vector.Distance(m_area.GetOrigin(), threatPosition);
                    // Cap at a minimum start distance to avoid division issues
                    if (startDistance < 50) startDistance = 50;
                    
                    float progress = 1.0 - (distanceToThreat / startDistance);
                    // Clamp progress to 0.0-1.0 range
                    if (progress < 0.0) progress = 0.0;
                    if (progress > 1.0) progress = 1.0;
                    
                    // Check time spent in investigation
                    int stateStartTime = currentTime;
                    m_stateStartTimes.Find(g, stateStartTime);
                    int timeSpentInvestigating = currentTime - stateStartTime;
                    
                    // Update investigation progress
                    m_investigationProgress.Set(g, progress);
                    
                    // Investigation is complete if:
                    // 1. The group has reached the threat location OR
                    // 2. Made substantial progress (>70%) AND spent minimum time investigating
                    if (reachedThreat || (progress > 0.7 && timeSpentInvestigating >= MINIMUM_INVESTIGATION_TIME))
                    {
                        m_hasInvestigatedThreat.Set(g, true); // Mark investigation as complete
                        Print(string.Format("[AreaInstance.MilitaryTask] Group %1 has completed investigation at %2 (Reached: %3, Progress: %4, Time: %5s)",
                            g.GetOrigin().ToString(), threatPosition.ToString(), reachedThreat, progress, timeSpentInvestigating), LogLevel.NORMAL);
                    }
                }
            }

            // --- MODIFIED LOGIC ---
            // If the actual state doesn't match our assigned state AND it wasn't just enforced above,
            // it means the group autonomously changed state and we might need to correct it.
            // We now also check if the group has completed its investigation for attacking/flanking.
            if (actualState != finalAssignedState && !needsEnforcement)
            {
                int stateStartTime = currentTime;
                m_stateStartTimes.Find(g, stateStartTime);
                int stateDuration = currentTime - stateStartTime;

                // Check if the state has been active long enough to allow autonomous changes
                bool stateDurationMet = stateDuration >= MINIMUM_STATE_DURATION;
                
                bool allowAutonomousChange = false;

                // Special case: Attacking/Flanking units changing to Defending *after* investigation is complete
                if ((finalAssignedState == IA_GroupTacticalState.Attacking || finalAssignedState == IA_GroupTacticalState.Flanking)
                    && actualState == IA_GroupTacticalState.Defending)
                {
                    bool hasInvestigated = false;
                    m_hasInvestigatedThreat.Find(g, hasInvestigated);

                    // If investigation is complete, allow them to switch to defensive
                    if (hasInvestigated)
                    {
                        allowAutonomousChange = true;
                        Print(string.Format("[AreaInstance.MilitaryTask] Allowing state change to Defending for Group %1 - investigation complete",
                            g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                }

                // If the state duration is met or the specific autonomous change is allowed, accept it.
                if (stateDurationMet || allowAutonomousChange)
                {
                    // If the state has been active long enough, accept the autonomous change
                    Print(string.Format("[AreaInstance.MilitaryTask] Accepting autonomous state change for Group %1: %2 -> %3 (state served min duration or change allowed)",
                        g.GetOrigin().ToString(),
                        typename.EnumToString(IA_GroupTacticalState, finalAssignedState),
                        typename.EnumToString(IA_GroupTacticalState, actualState)), LogLevel.NORMAL);
                        
                    // Update our records to match the new reality
                    m_assignedGroupStates.Set(g, actualState);
                    m_stateStartTimes.Set(g, currentTime);
                    m_stateStability.Set(g, 0);
                    finalAssignedState = actualState; // Update for use in the rest of this method
                    // Do NOT set needsEnforcement here - we just accepted the state change
                }
                else
                {
                    // If the state should be locked by duration and the change is not specifically allowed, correct it.
                    Print(string.Format("[AreaInstance.MilitaryTask] CORRECTING AUTONOMOUS STATE CHANGE: Group %1 changed from %2 to %3 WITHOUT AUTHORITY. Enforcing original state.", // Keep original warning
                        g.GetOrigin().ToString(),
                        typename.EnumToString(IA_GroupTacticalState, finalAssignedState), // Assigned state before correction
                        typename.EnumToString(IA_GroupTacticalState, actualState)), LogLevel.WARNING);

                    g.SetTacticalState(finalAssignedState, targetForState);
                    needsEnforcement = true; // Mark that we enforced state this cycle
                    continue; // Skip idle handling since we just enforced state
                }
            }

            // --- MODIFIED LOGIC ---
            // Only check for idle and apply orders if the state was NOT enforced/corrected in this cycle.
            // This prevents immediately clearing new orders.
            if (!needsEnforcement)
            {
                // Timeout Logic - significantly increased timeout
                int randomOrderRefresh = 120 + IA_Game.rng.RandInt(0, 60); // Increase timeout to 120-180 seconds
                if (g.TimeSinceLastOrder() >= randomOrderRefresh)
                {
                     Print(string.Format("[AreaInstance.MilitaryTask] Group %1 timed out (State: %2). Removing orders.", g.GetOrigin().ToString(), g.GetTacticalStateAsString()), LogLevel.DEBUG);
                    g.RemoveAllOrders(true);
                }

                // Idle Handling
                // Apply orders if group has no active waypoint AND their current state matches the ASSIGNED state
                // AND the state duration is met (they've been in this state long enough to justify re-issuing orders if idle)
                bool stateDurationMet = (currentTime - m_stateStartTimes.Get(g)) >= MINIMUM_STATE_DURATION;
                
                if (!g.HasActiveWaypoint())
                {
                    if (actualState == finalAssignedState)
                    {
                        // Only re-issue orders if they've been idle for some time (not immediately)
                        if (g.TimeSinceLastOrder() > 5)
                        {
                            Print(string.Format("[AreaInstance.MilitaryTask] Group at %1 is Idle (State: %2). Applying orders for state.",
                                g.GetOrigin().ToString(), actualState.ToString()), LogLevel.DEBUG);
                            g.SetAssignedArea(m_area); // Set the assigned area
                            g.ApplyTacticalStateOrders(); 
                        }
                    }
                    else if (stateDurationMet) 
                    {
                        // If actual state doesn't match assigned but duration met, apply orders for current state
                        Print(string.Format("[AreaInstance.MilitaryTask] Group %1 idle with state mismatch but duration met. Using current state %2.",
                            g.GetOrigin().ToString(), actualState.ToString()), LogLevel.DEBUG);
                        g.SetAssignedArea(m_area);
                        g.ApplyTacticalStateOrders();
                    }
                    else 
                    {
                        // Waiting for state/timing to sync up
                        Print(string.Format("[AreaInstance.MilitaryTask] Group %1 idle, waiting for state (%2) to match assigned (%3) or minimum duration (%4s left).",
                            g.GetOrigin().ToString(), actualState.ToString(), finalAssignedState.ToString(), 
                            MINIMUM_STATE_DURATION - (currentTime - m_stateStartTimes.Get(g))), LogLevel.DEBUG);
                    }
                }
            }
        } // End of group loop
    }

    private void CivilianOrderTask()
    {
        if (!m_area)
        {
            return;
        }
        if (!m_canSpawn || m_civilians.IsEmpty())
            return;
        
        // Get current time for state tracking
        int currentTime = System.GetUnixTime();
        
        foreach (IA_AiGroup g : m_civilians)
        {
            if (!g.IsSpawned() || g.GetAliveCount() == 0)
                continue;
            
            // --- NEW SECTION: Check for pending state change requests from civilian groups ---
            // Check if this civilian group has a pending state change request
            if (g.HasPendingStateRequest())
            {
                IA_GroupTacticalState requestedState = g.GetRequestedState();
                vector requestedPosition = g.GetRequestedStatePosition();
                
                // Almost always approve civilian state change requests (they're typically for safety)
                bool approveRequest = (Math.RandomFloat(0, 1) < 0.9);
                
                if (approveRequest)
                {
                    // Apply the requested state with authority flag
                    g.SetTacticalState(requestedState, requestedPosition, null, true);
                    Print(string.Format("[AreaInstance.CivilianOrderTask] APPROVED state change to %1 for civilian group", 
                        typename.EnumToString(IA_GroupTacticalState, requestedState)), LogLevel.DEBUG);
                }
                
                // Clear the request whether approved or denied
                g.ClearStateRequest();
                
                // If approved a retreat or defend state, skip order generation this cycle
                if (approveRequest && (requestedState == IA_GroupTacticalState.Retreating || 
                                      requestedState == IA_GroupTacticalState.Defending))
                {
                    continue;
                }
            }
            // --- END NEW SECTION ---
            
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
                // For civilians, use Neutral or DefendPatrol state instead of direct patrol order
                g.SetTacticalState(IA_GroupTacticalState.DefendPatrol, pos, null, true); // Added authority flag
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
            
            // Make sure the group has a tactical state
            if (grp.IsSpawned())
            {
                // Default state based on area status
                IA_GroupTacticalState defaultState;
                if (IsUnderAttack())
                    defaultState = IA_GroupTacticalState.Attacking;
                else
                    defaultState = IA_GroupTacticalState.DefendPatrol;
                    
                // Set tactical state explicitly
                grp.SetTacticalState(defaultState, pos, null, true); // Added fromAuthority parameter
            }
            
            // Add the group to the military list (will also assign initial state)
            AddMilitaryGroup(grp);
            
            // Update strength counter
            strCounter = strCounter + grp.GetAliveCount();
        }
        
        // Update overall strength
        OnStrengthChange(strCounter);
        
        // --- BEGIN ADDED: Update max historical strength if this is the initial spawn ---
        if (m_initialTotalUnits == 0)
        {
            m_initialTotalUnits = strCounter;
            m_maxHistoricalStrength = strCounter;
        }
        // --- END ADDED ---
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
                
                // Use tactical state system instead of direct orders
                vehicleGroup.SetTacticalState(IA_GroupTacticalState.Defending, groupPos);
                
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
        // Only process reactions at the defined interval
        int currentTime = System.GetUnixTime();
        if (currentTime - m_lastReactionProcessTime < REACTION_PROCESS_INTERVAL)
            return;
        
        m_lastReactionProcessTime = currentTime;
        string areaName;
        if (m_area)
        {
            areaName = m_area.GetName();
        }
        else
        {
            areaName = "unknown";
        }
        Print(string.Format("[AreaInstance.AIReactionsTask] Processing reactions for Area %1", areaName), LogLevel.NORMAL);
        
        // Clear any existing reactions in the central manager
        m_centralReactionManager.ClearAllReactions();
        
        // Process all military groups
        foreach (IA_AiGroup group : m_military)
        {
            if (!group || !group.IsSpawned() || group.GetAliveCount() == 0)
                continue;
            
            // Skip civilian groups and vehicles
            if (group.IsDriving())
                continue;
            
            // 1. Collect danger information from the group
            float dangerLevel = group.GetCurrentDangerLevel();
            vector dangerPosition = group.GetLastDangerPosition();
            IA_Faction enemyFaction = group.GetEngagedEnemyFaction();
            IA_GroupTacticalState currentState = group.GetTacticalState();
            vector groupPosition = group.GetOrigin();
            
            // 2. Determine appropriate reaction based on collected data
            if (dangerLevel > 0.5 && dangerPosition != vector.Zero)
            {
                // Determine reaction type based on danger level
                IA_AIReactionType reactionType = IA_AIReactionType.None;
                
                if (dangerLevel >= 0.7)
                {
                    reactionType = IA_AIReactionType.UnderFire;
                    Print(string.Format("[AreaInstance.AIReactionsTask] Group at %1 - HIGH danger (%2) - triggering UnderFire reaction", 
                        groupPosition.ToString(), dangerLevel), LogLevel.NORMAL);
                }
                else if (dangerLevel >= 0.3)
                {
                    reactionType = IA_AIReactionType.EnemySpotted;
                    Print(string.Format("[AreaInstance.AIReactionsTask] Group at %1 - MEDIUM danger (%2) - triggering EnemySpotted reaction", 
                        groupPosition.ToString(), dangerLevel), LogLevel.NORMAL);
                }
                
                // Apply the reaction if valid
                if (reactionType != IA_AIReactionType.None)
                {
                    // If no enemy faction is set but we have danger, use opposing faction
                    if (enemyFaction == IA_Faction.NONE)
                    {
                        if (m_faction == IA_Faction.USSR)
                            enemyFaction = IA_Faction.US;
                        else if (m_faction == IA_Faction.US)
                            enemyFaction = IA_Faction.USSR;
                        else
                            enemyFaction = IA_Faction.US; // Default
                    }
                    
                    // Apply distance-based intensity scaling like in HandleUnderFireReaction
                    float finalIntensity = dangerLevel;
                    if (reactionType == IA_AIReactionType.UnderFire && dangerPosition != vector.Zero)
                    {
                        float distanceToDanger = vector.Distance(groupPosition, dangerPosition);
                        const float MAX_INTENSITY_RANGE = 25.0;
                        const float MIN_INTENSITY_RANGE = 150.0;
                        const float MIN_INTENSITY_SCALE = 0.3;
                        
                        float distanceFactor = 1.0;
                        if (distanceToDanger > MAX_INTENSITY_RANGE)
                        {
                            float range = MIN_INTENSITY_RANGE - MAX_INTENSITY_RANGE;
                            if (range > 0)
                            {
                                float progress = (distanceToDanger - MAX_INTENSITY_RANGE) / range;
                                distanceFactor = Math.Clamp(1.0 - progress, MIN_INTENSITY_SCALE, 1.0);
                            }
                            else
                            {
                                distanceFactor = MIN_INTENSITY_SCALE;
                            }
                        }
                        if(distanceToDanger >= MIN_INTENSITY_RANGE)
                            distanceFactor = MIN_INTENSITY_SCALE;

                        finalIntensity = dangerLevel * distanceFactor;
                    }
                    
                    // Trigger the reaction in the central manager
                    m_centralReactionManager.TriggerReaction(
                        reactionType,
                        finalIntensity,
                        dangerPosition,
                        null,
                        enemyFaction
                    );
                }
            }
        }
        
        // 3. Process the reactions and apply the appropriate tactical state to each group
        if (m_centralReactionManager.HasActiveReaction())
        {
            IA_AIReactionState reaction = m_centralReactionManager.GetCurrentReaction();
            if (reaction)
            {
                IA_AIReactionType reactionType = reaction.GetReactionType();
                float intensity = reaction.GetIntensity();
                vector sourcePos = reaction.GetSourcePosition();
                IA_Faction sourceFaction = reaction.GetSourceFaction();
                
                Print(string.Format("[AreaInstance.AIReactionsTask] Processing central reaction: Type=%1, Intensity=%2, SourcePos=%3", 
                    typename.EnumToString(IA_AIReactionType, reactionType),
                    intensity,
                    sourcePos.ToString()), LogLevel.NORMAL);
                
                // Apply the reaction to all relevant groups
                foreach (IA_AiGroup group : m_military)
                {
                    if (!group || !group.IsSpawned() || group.GetAliveCount() == 0 || group.IsDriving())
                        continue;
                    
                    // Apply appropriate behavior based on reaction type
                    if (reactionType == IA_AIReactionType.UnderFire)
                    {
                        // Apply logic similar to HandleUnderFireReaction
                        ApplyUnderFireReactionToGroup(group, reaction);
                    }
                    else if (reactionType == IA_AIReactionType.EnemySpotted)
                    {
                        // Apply logic similar to HandleEnemySpottedReaction
                        ApplyEnemySpottedReactionToGroup(group, reaction);
                    }
                    else if (reactionType == IA_AIReactionType.GroupMemberKilled)
                    {
                        // Apply logic similar to HandleGroupMemberKilledReaction
                        ApplyGroupMemberKilledReactionToGroup(group, reaction);
                    }
                }
                
                // Clear the reaction after processing to prevent continuous reapplication
                m_centralReactionManager.ClearAllReactions();
            }
        }
        
        // Also periodically evaluate tactical state for all groups
        foreach (IA_AiGroup group : m_military)
        {
            if (group && group.IsSpawned())
            {
                group.EvaluateTacticalState();
            }
        }
    }

    // --- Add these helper methods for reaction processing ---
    private void ApplyUnderFireReactionToGroup(IA_AiGroup group, IA_AIReactionState reaction)
    {
        if (!group || !reaction)
            return;
        
        IA_Faction sourceFaction = reaction.GetSourceFaction();
        if (sourceFaction != IA_Faction.NONE && !group.IsEngagedWithEnemy())
        {
            // Update engagement state if not already engaged
            if (group)
            {
                group.m_engagedEnemyFaction = sourceFaction;
            }
        }
        
        // Skip if group is driving
        if (group.IsDriving())
            return;
        
        vector sourcePos = reaction.GetSourcePosition();
        float intensity = reaction.GetIntensity();
        int aliveCount = group.GetAliveCount();
        
        if (sourcePos != vector.Zero)
        {
            float distanceToThreat = vector.Distance(group.GetOrigin(), sourcePos);
            
            // Define distance thresholds from original code
            const float CLOSE_DISTANCE = 50.0;
            const float SHORT_DISTANCE = 125.0;
            const float MEDIUM_DISTANCE = 225.0;
            const float FAR_DISTANCE = 350.0;

            // Check current flanking state
            IA_GroupTacticalState currentState = group.GetTacticalState();
            bool isInFlankingPhase = false;
            
            if (currentState == IA_GroupTacticalState.Flanking)
            {
                // If already flanking, generally continue with it
                return;
            }
            
            // Decision logic based on distance, intensity, and unit count
            Print(string.Format("[ApplyUnderFireReactionToGroup] Group %1 | Threat Pos: %2 | Intensity: %3 | Alive: %4 | Distance: %5m", 
                group, sourcePos.ToString(), intensity, aliveCount, distanceToThreat), LogLevel.NORMAL);
                
            // Handle special case for low unit count (<=2)
            if (aliveCount <= 2)
            {
                if (group.GetInitialUnitCount() > 0)
                {
                    float remainingPercentage = (float)aliveCount / group.GetInitialUnitCount();
                    if (remainingPercentage >= 0.80)
                    {
                        if (Math.RandomFloat(0, 1) < 0.5) {
                            group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                        } else {
                            group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
                        }
                        return;
                    }
                }
                
                float desperationFactor = 0;
                
                // Intensity component
                desperationFactor += Math.Clamp(intensity, 0, 1) * 0.5;
                
                // Alive count component
                if (aliveCount == 1) desperationFactor += 0.5;
                else desperationFactor += 0.25;
                
                // Distance component
                if (distanceToThreat < SHORT_DISTANCE) {
                    desperationFactor += (1.0 - (distanceToThreat / SHORT_DISTANCE)) * 0.25;
                }
                
                // Loss percentage factor
                if (group.GetInitialUnitCount() > 0 && aliveCount < group.GetInitialUnitCount())
                {
                    float lossPercentage = 1.0 - ((float)aliveCount / group.GetInitialUnitCount());
                    desperationFactor += Math.Clamp(lossPercentage, 0, 1) * 0.25;
                }
                
                // Calculate final chance for Last Stand
                float baseLastStandChance = 0.1;
                float maxLastStandChance = 0.9;
                float lastStandChance = Math.Clamp(baseLastStandChance + (desperationFactor * 0.8), baseLastStandChance, maxLastStandChance);
                
                if (Math.RandomFloat(0, 1) < lastStandChance)
                {
                    Print(string.Format("[ApplyUnderFireReactionToGroup] UnderFire (Desperate): RND Check PASSED -> Setting state to LastStand", distanceToThreat), LogLevel.NORMAL);
                    group.SetTacticalState(IA_GroupTacticalState.LastStand, group.GetOrigin(), null, true); // Set state as authority
                }
                else
                {
                    Print(string.Format("[ApplyUnderFireReactionToGroup] UnderFire (Desperate): RND Check FAILED -> Setting state to Retreating", distanceToThreat), LogLevel.NORMAL);
                    group.SetTacticalState(IA_GroupTacticalState.Retreating, sourcePos, null, true); // Set state as authority
                }
                return;
            }
            
            // Logic for groups with > 2 members
            if (distanceToThreat < CLOSE_DISTANCE)
            {
                // Close range
                group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
            }
            else if (distanceToThreat < SHORT_DISTANCE)
            {
                // Short range
                float randomChoice = Math.RandomFloat(0, 1);
                if (randomChoice < 0.5) {
                    group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                } else {
                    vector defendPoint = group.GetOrigin();
                    group.SetTacticalState(IA_GroupTacticalState.Defending, defendPoint, null, true); // Set state as authority
                }
            }
            else if (distanceToThreat < MEDIUM_DISTANCE)
            {
                // Medium range
                float randomChoice = Math.RandomFloat(0, 1);
                if (distanceToThreat >= 100.0 && aliveCount >= 3 && randomChoice < 0.005) {
                    group.SetTacticalState(IA_GroupTacticalState.Flanking, sourcePos, null, true); // Set state as authority
                } else if (randomChoice < 0.5) {
                    group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                } else {
                    vector defendPoint = group.GetOrigin();
                    group.SetTacticalState(IA_GroupTacticalState.Defending, defendPoint, null, true); // Set state as authority
                }
            }
            else if (distanceToThreat < FAR_DISTANCE)
            {
                // Long range
                float randomChoice = Math.RandomFloat(0, 1);
                if (distanceToThreat >= 100.0 && aliveCount >= 3 && randomChoice < 0.02) {
                    group.SetTacticalState(IA_GroupTacticalState.Flanking, sourcePos, null, true); // Set state as authority
                } else if (randomChoice < 0.08) {
                    group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                } else {
                    vector defendPoint = group.GetOrigin();
                    group.SetTacticalState(IA_GroupTacticalState.Defending, defendPoint, null, true); // Set state as authority
                }
            }
            else
            {
                // Far range
                vector defendPoint = group.GetOrigin();
                group.SetTacticalState(IA_GroupTacticalState.Defending, defendPoint, null, true); // Set state as authority
            }
        }
        else
        {
            // Fallback for unknown source position
            if (aliveCount <= 2)
            {
                if (Math.RandomFloat(0,1) < 0.3 + (intensity * 0.5))
                {
                    group.SetTacticalState(IA_GroupTacticalState.LastStand, group.GetOrigin(), null, true); // Set state as authority
                } else {
                    group.SetTacticalState(IA_GroupTacticalState.Retreating, group.GetOrigin(), null, true); // Set state as authority
                }
            }
            else
            {
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
            }
        }
    }

    private void ApplyEnemySpottedReactionToGroup(IA_AiGroup group, IA_AIReactionState reaction)
    {
        if (!group || !reaction)
            return;
        
        // Skip for civilians and vehicles
        if (group.IsDriving())
            return;
        
        float intensity = reaction.GetIntensity();
        vector sourcePos = reaction.GetSourcePosition();
        int aliveCount = group.GetAliveCount();
        
        if (sourcePos != vector.Zero)
        {
            float distanceToThreat = vector.Distance(group.GetOrigin(), sourcePos);
            
            // Define distance thresholds
            const float CLOSE_DISTANCE = 150.0;
            const float MEDIUM_DISTANCE = 300.0;
            
            // Decision logic based on distance
            if (distanceToThreat < CLOSE_DISTANCE)
            {
                Print(string.Format("[ApplyEnemySpottedReactionToGroup] EnemySpotted (Close: %1m): Decided state = Attacking", distanceToThreat), LogLevel.NORMAL);
                group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
            }
            else if (distanceToThreat < MEDIUM_DISTANCE)
            {
                if (aliveCount >= 4)
                {
                    float tacticalRoll = Math.RandomFloat(0, 1);
                    if (tacticalRoll < 0.01) {
                        group.SetTacticalState(IA_GroupTacticalState.Flanking, sourcePos, null, true); // Set state as authority
                    } else if (tacticalRoll < 0.7) {
                        group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                    } else {
                        group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
                    }
                }
                else
                {
                    if (Math.RandomFloat(0, 1) < 0.6)
                    {
                        group.SetTacticalState(IA_GroupTacticalState.Attacking, sourcePos, null, true); // Set state as authority
                    }
                    else
                    {
                        group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
                    }
                }
            }
            else
            {
                group.SetTacticalState(IA_GroupTacticalState.DefendPatrol, sourcePos, null, true); // Set state as authority
            }
        }
        else
        {
            group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
        }
    }

    private void ApplyGroupMemberKilledReactionToGroup(IA_AiGroup group, IA_AIReactionState reaction)
    {
        if (!group || !reaction)
            return;
        
        // Update legacy engagement system
        if (!group.IsEngagedWithEnemy() && !group.IsDriving())
        {
            IA_Faction sourceFaction = reaction.GetSourceFaction();
            if (sourceFaction != IA_Faction.NONE)
            {
                group.m_engagedEnemyFaction = sourceFaction;
            }
            else
            {
                group.m_engagedEnemyFaction = IA_Faction.CIV;
            }
        }
        
        // New approach: Use tactical state system
        if (!group.IsDriving())
        {
            vector threatPos = reaction.GetSourcePosition();
            int aliveCount = group.GetAliveCount();
            
            // Decide tactical behavior based on remaining strength
            if (aliveCount <= 1)
            {
                if (Math.RandomFloat(0, 1) < 0.3)
                {
                    group.SetTacticalState(IA_GroupTacticalState.LastStand, group.GetOrigin(), null, true); // Set state as authority
                }
                else
                {
                    group.SetTacticalState(IA_GroupTacticalState.Retreating, threatPos, null, true); // Set state as authority
                }
            }
            else if (aliveCount <= 2)
            {
                group.SetTacticalState(IA_GroupTacticalState.Retreating, threatPos, null, true); // Set state as authority
            }
            else
            {
                if (Math.RandomFloat(0, 1) < 0.7)
                {
                    group.SetTacticalState(IA_GroupTacticalState.Attacking, threatPos, null, true); // Set state as authority
                }
                else
                {
                    group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin(), null, true); // Set state as authority
                }
            }
        }
        else if (reaction.GetIntensity() > 0.5)
        {
            // Vehicle handling
            vector threatPos = reaction.GetSourcePosition();
            if (threatPos != vector.Zero)
            {
                vector awayDir = group.GetOrigin() - threatPos;
                awayDir = awayDir.Normalized();
                vector retreatPos = group.GetOrigin() + awayDir * 20;
                
                group.RemoveAllOrders();
                group.AddOrder(retreatPos, IA_AiOrder.Defend, true);
            }
        }
    }

    // --- Add these class member variables after other private member variables ---
    private ref IA_AIReactionManager m_centralReactionManager = new IA_AIReactionManager();
    private int m_lastReactionProcessTime = 0;
    private const int REACTION_PROCESS_INTERVAL = 30; // Process reactions every 30 seconds

    // --- Near the top, before any methods, add these class member variables
    // State timing and duration tracking
    private ref map<IA_AiGroup, int> m_stateStartTimes = new map<IA_AiGroup, int>();
    private const int MINIMUM_STATE_DURATION = 180; // 3 minutes (in seconds)
    private const int AREA_AUTHORITY_INTERVAL = 60; // Only perform major state reassignments every 60 seconds
    private int m_lastGlobalStateAuthority = 0; // Last time we performed a global authority check
    
    // Threat investigation tracking
    private ref map<IA_AiGroup, bool> m_hasInvestigatedThreat = new map<IA_AiGroup, bool>();
    private ref map<IA_AiGroup, vector> m_assignedThreatPosition = new map<IA_AiGroup, vector>();
    private ref map<IA_AiGroup, float> m_investigationProgress = new map<IA_AiGroup, float>(); // 0.0 to 1.0 progress
    private const float INVESTIGATION_COMPLETE_DISTANCE = 30.0; // Must get within 30m of threat location
    private const int MINIMUM_INVESTIGATION_TIME = 60; // At least 60 seconds of investigation

    // State control flags - very specific conditions where units can change states
    private bool m_forceDefend = false; // Emergency defensive mode for the entire area
    private bool m_allowFlankingOperations = true; // Whether flanking is tactically viable now
    private int m_reassignmentCooldown = 0; // Cooldown for next batch of reassignments

    // --- Unit Attrition Tracking ---
    private int m_initialTotalUnits = 0;       // Initial number of units when area was created
    private int m_maxHistoricalStrength = 0;   // Highest strength value ever recorded for this area
    private float m_unitLossThreshold = 0.8;     // Percentage of units lost to trigger critical state (80%)
    private bool m_criticalState = false;      // Whether area is in a critical state due to high losses

    // Add a method to get the attrition ratio for debugging
    float GetAttritionRatio()
    {
        if (m_maxHistoricalStrength <= 0)
            return 0;
            
        return 1.0 - (m_strength / (float)m_maxHistoricalStrength);
    }
    
    // Add a method to get critical state
    bool IsInCriticalState()
    {
        return m_criticalState;
    }
}