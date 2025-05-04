///////////////////////////////////////////////////////////////////////
// 8) IA_AreaInstance - dynamic mission/task handling (integrates area)
///////////////////////////////////////////////////////////////////////

// Add this class definition after the FactionManager include statement at the top of the file, before the IA_AreaInstance class definition

// --- BEGIN ADDED: Reinforcement State Enum ---
// (Ensure this is defined appropriately - here or globally)
enum IA_ReinforcementState
{
    NotDone,
    Countdown,
    SpawningWaves, // New state for wave spawning
    Done
};
// --- END ADDED ---

// Class to track pending tactical state change requests
class IA_PendingStateRequest
{
    IA_AiGroup group;
    IA_GroupTacticalState requestedState;
    vector requestedPosition;
    IEntity requestedEntity;
    IA_GroupTacticalState currentState;
}

class IA_AreaInstance
{
    IA_Area m_area;
    IA_Faction m_faction;
    int m_strength;
    private ref array<ref IA_AiGroup> m_military  = {};
    private ref array<ref IA_AiGroup> m_civilians = {};
    private ref array<IA_Faction>     m_attackingFactions = {};

    // Target role counts for military groups
    private int targetDefenders = 0;
    private int targetAttackers = 0;
    private int targetFlankers = 0;

    // --- BEGIN ADDED: Centralized State Tracking ---
    private ref map<IA_AiGroup, IA_GroupTacticalState> m_assignedGroupStates = new map<IA_AiGroup, IA_GroupTacticalState>();
    // New stability tracking
    private ref map<IA_AiGroup, int> m_stateStability = new map<IA_AiGroup, int>();
    // State timing and duration tracking
    private ref map<IA_AiGroup, int> m_stateStartTimes = new map<IA_AiGroup, int>();
    private const int MINIMUM_STATE_DURATION = 180; // 3 minutes (in seconds)
    private const int AREA_AUTHORITY_INTERVAL = 60; // Only perform major state reassignments every 60 seconds
    private int m_lastGlobalStateAuthority = 0; // Last time we performed a global authority check
    
    // Tracking variables for state change requests
    private int currentDefenders_PreRequest = 0;
    private int currentAttackers_PreRequest = 0; 
    private int currentFlankers_PreRequest = 0;
    
    // Tracking for pending state change requests
    private ref array<ref IA_PendingStateRequest> m_pendingStateRequests = {};
    // --- END ADDED ---

    private ref IA_AreaAttackers m_aiAttackers = null;

    private IA_ReinforcementState m_reinforcements = IA_ReinforcementState.NotDone;
    private int m_reinforcementTimer = 0;
    private int m_currentTask = 0;
    private bool m_canSpawn   = true;
    
    // --- BEGIN ADDED: Reinforcement Wave Variables ---
    private int m_totalReinforcementQuota = 0;        // Max groups for this area type
    private int m_reinforcementGroupsSpawned = 0;     // Groups spawned in this attack cycle
    private int m_reinforcementWaveDelayTimer = 0;    // Ticks until next wave
    private const int INITIAL_REINFORCEMENT_DELAY_TICKS = 15;
    private const int REINFORCEMENT_WAVE_DELAY_TICKS = 15;   
    // --- END ADDED ---

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
	    
	    // --- BEGIN ADDED: Initialize Reinforcement Quota ---
	    inst.m_totalReinforcementQuota = area.GetReinforcementGroupQuota();
	    Print(string.Format("[AreaInstance.Create] Area %1 initialized with reinforcement quota: %2", area.GetName(), inst.m_totalReinforcementQuota), LogLevel.NORMAL);
	    // --- END ADDED ---

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

        // --- BEGIN ADDED: Reset reinforcements on faction change ---
        ResetReinforcementState();
        // Also reset vehicle reinforcements (optional, but likely desired)
        // m_vehicleReinforcements = IA_ReinforcementState.NotDone; // Consider if needed
        // m_vehicleReinforcementTimer = 0;
        // --- END ADDED ---

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
            Print(string.Format("[AreaInstance.OnStrengthChange] new maximum strength: %1", m_maxHistoricalStrength), LogLevel.NORMAL);
        }
        // --- END ADDED ---
        
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep && m_area)
            rep.SetStrength(m_area.GetName(), newVal);
    }

    // --- BEGIN ADDED: Reinforcement Reset Helper ---
    private void ResetReinforcementState()
    {
        Print(string.Format("[AreaInstance.ResetReinforcementState] Area %1 resetting reinforcement state.", m_area.GetName()), LogLevel.NORMAL);
        m_reinforcements = IA_ReinforcementState.NotDone;
        m_reinforcementTimer = 0;
        m_reinforcementGroupsSpawned = 0;
        m_reinforcementWaveDelayTimer = 0;
    }
    // --- END ADDED ---

    // --- MODIFIED: ReinforcementsTask ---
    private void ReinforcementsTask()
    {
        // Stop immediately if area is no longer under attack
        if (!IsUnderAttack())
        {
            if (m_reinforcements != IA_ReinforcementState.NotDone) // Only reset if it wasn't already NotDone
            {
               ResetReinforcementState();
            }
            return;
        }

        // If already done, do nothing
        if (m_reinforcements == IA_ReinforcementState.Done)
            return;

        // --- BEGIN ADDED: Calculate scaled groups per wave ---
        float scaleFactor = IA_Game.GetAIScaleFactor();
        const int baseGroupsPerWave = 1;
        int scaledGroupsToAttempt = Math.Max(1, Math.Round(baseGroupsPerWave * (2*scaleFactor)));
        // --- END ADDED ---

        // Handle initial countdown
        if (m_reinforcements == IA_ReinforcementState.Countdown)
        {
            m_reinforcementTimer++;
			Print("Timer: " + m_reinforcementTimer + " Requirement: " + INITIAL_REINFORCEMENT_DELAY_TICKS,LogLevel.ERROR);
            if (m_reinforcementTimer > INITIAL_REINFORCEMENT_DELAY_TICKS)
            {
				Print("LOG 5",LogLevel.ERROR);
                Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 initial delay (%2 ticks) complete. Attempting first wave (scaled groups: %3).",
                    m_area.GetName(), INITIAL_REINFORCEMENT_DELAY_TICKS, scaledGroupsToAttempt), LogLevel.NORMAL);

                bool initialWaveSpawned = SpawnReinforcementWave(scaledGroupsToAttempt); // Use scaled value

                if (initialWaveSpawned)
                {
                     // First wave spawned successfully
                     m_reinforcements = IA_ReinforcementState.SpawningWaves;
                     m_reinforcementWaveDelayTimer = REINFORCEMENT_WAVE_DELAY_TICKS; // Set delay for next wave
                }
                else
                {
                     // Initial wave failed (e.g., no safe spot).
                     // DO NOT set state to Done. Transition to SpawningWaves state
                     // and set the timer to retry after the standard wave delay.
                     Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 failed to spawn INITIAL wave (e.g., no safe spot). Will retry after delay (%2 ticks).",
                         m_area.GetName(), REINFORCEMENT_WAVE_DELAY_TICKS), LogLevel.WARNING);
                     m_reinforcements = IA_ReinforcementState.SpawningWaves; // Still transition state
                     m_reinforcementWaveDelayTimer = REINFORCEMENT_WAVE_DELAY_TICKS; // Set timer to retry
                }
                m_reinforcementTimer = 0; // Reset initial countdown timer regardless
            }
        }
        // Handle subsequent waves
        else if (m_reinforcements == IA_ReinforcementState.SpawningWaves)
        {
            m_reinforcementWaveDelayTimer--;
            if (m_reinforcementWaveDelayTimer <= 0)
            {
                // Check quota BEFORE attempting to spawn
                if (m_reinforcementGroupsSpawned >= m_totalReinforcementQuota)
                {
                    Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 quota met (%2/%3) before spawning next wave. Reinforcements Done.",
                         m_area.GetName(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota), LogLevel.NORMAL);
                    m_reinforcements = IA_ReinforcementState.Done;
                }
                else // Quota is not yet met, attempt to spawn
                {
                    Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 attempting subsequent wave (scaled groups: %2).",
                        m_area.GetName(), scaledGroupsToAttempt), LogLevel.NORMAL);
                    
                    bool waveSpawnedSuccessfully = SpawnReinforcementWave(scaledGroupsToAttempt); // Use scaled value

                    if (waveSpawnedSuccessfully)
                    {
                         Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 successfully spawned reinforcement wave. Groups spawned: %2/%3. Resetting delay (%4 ticks).",
                             m_area.GetName(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota, REINFORCEMENT_WAVE_DELAY_TICKS), LogLevel.NORMAL);
                         m_reinforcementWaveDelayTimer = REINFORCEMENT_WAVE_DELAY_TICKS; // Reset delay for the *next* wave
                    }
                    else
                    {
                         // SpawnReinforcementWave returned false. This means no group was spawned in this attempt (e.g., safe spot not found).
                         // DO NOT set state to Done here. Just reset the timer to try again later.
                         Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 failed to spawn group in current wave (e.g., no safe spot). Groups spawned: %2/%3. Resetting delay (%4 ticks) to retry.",
                             m_area.GetName(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota, REINFORCEMENT_WAVE_DELAY_TICKS), LogLevel.WARNING);
                         m_reinforcementWaveDelayTimer = REINFORCEMENT_WAVE_DELAY_TICKS; // Reset delay to try again later

                         // An edge case: if quota was *just* met by the failed spawn attempt (shouldn't happen with current logic, but for safety)
                         if (m_reinforcementGroupsSpawned >= m_totalReinforcementQuota) {
                             Print(string.Format("[AreaInstance.ReinforcementsTask] Area %1 quota met (%2/%3) after failed spawn attempt. Reinforcements Done.",
                                 m_area.GetName(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota), LogLevel.NORMAL);
                             m_reinforcements = IA_ReinforcementState.Done;
                         }
                    }
                }
            }
        }
        
    }
    // --- END MODIFIED ---

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
        
        // --- BEGIN ADDED: Check for attacker replacement needs ---
        // Call the new function to prioritize attacker replacement if needed
        if (IsUnderAttack())
        {
            // Only run occasionally (15% chance per strength update)
            if (Math.RandomFloat01() < 0.15)
            {
                PrioritizeAttackerReplacement();
            }
        }
        // --- END ADDED ---
        
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
                    attritionRatio), LogLevel.NORMAL);
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
            
        // Initialize counters for this cycle
        currentDefenders_PreRequest = 0;
        currentAttackers_PreRequest = 0;
        currentFlankers_PreRequest = 0;

        // --- BEGIN MOVED SECTION: Stages 1, 2 & Pre-Request Count ---
        // --- Stage 1: Threat Assessment ---
        bool isUnderAttack = IsUnderAttack();
        vector primaryThreatLocation = m_area.GetOrigin(); // Default if no specific threat found
        bool validThreatLocation = false;
        float globalThreatLevel = 0.0;

        if (isUnderAttack)
        {
            vector dangerSum = vector.Zero;
            int dangerCount = 0;
            float dangerLevelSum = 0.0;
            int dangerLevelCount = 0;
            vector currentDangerPos;
            int latestDangerTime = 0; // Track latest danger time used in calculation
            const int MAX_THREAT_STALENESS_SECONDS = 90; // If danger info is older than this, consider it stale

            foreach (IA_AiGroup g_threat : m_military) {
                int groupLastDangerTime = g_threat.GetLastDangerEventTime();
                int timeSinceLastDanger = System.GetUnixTime() - groupLastDangerTime;

                // Only consider groups with relatively recent danger info for position calculation
                if (g_threat && groupLastDangerTime > 0 && timeSinceLastDanger < MAX_THREAT_STALENESS_SECONDS )
                {
                    currentDangerPos = g_threat.GetLastDangerPosition();
                    if(currentDangerPos != vector.Zero)
                    {
                         // Only consider danger points reasonably within or near the area
                         if (vector.DistanceSq(currentDangerPos, m_area.GetOrigin()) < (m_area.GetRadius() * 1.5) * (m_area.GetRadius() * 1.5)) // Increased radius slightly
                         {
                            dangerSum += currentDangerPos;
                            dangerCount++;
                            if (groupLastDangerTime > latestDangerTime) {
                                latestDangerTime = groupLastDangerTime; // Update latest time stamp
                            }
                         }
                    }
                }
                // Still calculate overall danger level based on all groups reporting any danger (less strict time limit?)
                float groupDanger = g_threat.GetCurrentDangerLevel();
                 if (groupDanger > 0) {
                     dangerLevelSum += groupDanger;
                     dangerLevelCount++;
                 }
            }

            if (dangerLevelCount > 0) {
                globalThreatLevel = dangerLevelSum / dangerLevelCount;
            }

            // Check if we calculated a position AND if the data isn't too stale
            if (dangerCount > 0)
            {
                primaryThreatLocation = dangerSum / dangerCount;
                validThreatLocation = true; // Position calculated from recent data
                Print(string.Format("[AreaInstance.MilitaryTask] Threat assessed from recent group danger near: %1 (Latest event: %2s ago)",
                    primaryThreatLocation.ToString(), System.GetUnixTime() - latestDangerTime), LogLevel.NORMAL);
            } else {
                 Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK. No RECENT (<%2s) threat location found via danger events, using origin as fallback.",
                    m_area.GetName(), MAX_THREAT_STALENESS_SECONDS), LogLevel.NORMAL);
                 primaryThreatLocation = m_area.GetOrigin(); // Fallback to origin
                 validThreatLocation = false; // Mark as invalid since it's stale/fallback
            }

            // --- Existing Force Defend / Flanking Allowance Logic ---
            // (Based on globalThreatLevel calculated from group danger)
            float attritionRatio = GetAttritionRatio();
            float dangerThreshold;
            if (m_criticalState)
            {
                dangerThreshold = 1.1;
            }
            else
            {
                dangerThreshold = 1.1;
            }
            if ((globalThreatLevel > dangerThreshold) && (attritionRatio >= m_unitLossThreshold)) {
                m_forceDefend = true;
                Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL THREAT LEVEL (%1): Area '%2' is forcing defensive posture (Critical: %3) - THREAT OVERDRIVE ACTIVE",
                    globalThreatLevel, m_area.GetName(), m_criticalState), LogLevel.NORMAL);
            } else {
                m_forceDefend = false;
            }
            
            // --- BEGIN MODIFIED: Make flanking more available ---
            // Original code:
            /*
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
            */
            
            // New more permissive logic - allow flanking in most circumstances
            // Only restrict flanking in extreme critical situations (very high threat + force defend active)
            if (m_forceDefend && globalThreatLevel > 1.5) {
                m_allowFlankingOperations = false;
                Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL: Flanking operations disabled due to extreme threat (%1)", globalThreatLevel), LogLevel.NORMAL);
            } else {
                m_allowFlankingOperations = true;
                Print(string.Format("[AreaInstance.MilitaryTask] Flanking operations allowed (Threat: %1, ForceDefend: %2)", globalThreatLevel, m_forceDefend), LogLevel.NORMAL);
            }
            // --- END MODIFIED ---
            
            if (dangerCount > 0) {
                primaryThreatLocation = dangerSum / dangerCount;
                validThreatLocation = true;
                Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK. Threat assessed from own groups near: %2", m_area.GetName(), primaryThreatLocation.ToString()), LogLevel.NORMAL);
            } else {
                 Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' UNDER ATTACK. No specific threat location found, using origin.", m_area.GetName()), LogLevel.NORMAL);
            }
        } else {
             Print(string.Format("[AreaInstance.MilitaryTask] Area '%1' SECURE.", m_area.GetName()), LogLevel.NORMAL);
             m_forceDefend = false;
             m_allowFlankingOperations = true;
        }

        // --- Stage 2: Role Calculation ---
        int totalMilitaryGroups = m_military.Count();
        // --- Renamed local variables to avoid conflict with class members ---
        int localTargetDefenders = 0;
        int localTargetAttackers = 0;
        int localTargetFlankers = 0;
        IA_GroupTacticalState defaultStateForArea;

        if (!isUnderAttack || totalMilitaryGroups == 0)
        {
            defaultStateForArea = IA_GroupTacticalState.DefendPatrol;
            // --- Use renamed local variable ---
            localTargetDefenders = totalMilitaryGroups; // All patrol or 0
            localTargetAttackers = 0;
            localTargetFlankers = 0;
             Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Secure/Empty): Default State: DefendPatrol. Def=%1", localTargetDefenders), LogLevel.NORMAL);
        }
        else if (m_forceDefend)
        {
            defaultStateForArea = IA_GroupTacticalState.Defending;
            float defenderPercentage;
            if (m_criticalState)
            {
                defenderPercentage = 0.9;
            }
            else
            {
                defenderPercentage = 0.8;
            }
            // --- Use renamed local variable ---
            localTargetDefenders = Math.Max(1, Math.Round(totalMilitaryGroups * defenderPercentage));
            // --- Use renamed local variables ---
            localTargetAttackers = totalMilitaryGroups - localTargetDefenders;
            localTargetFlankers = 0; // No flanking in emergency mode
            Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (EMERGENCY): Def=%1, Att=%2, Flk=0, Critical=%3",
                // --- Use renamed local variables ---
                localTargetDefenders, localTargetAttackers, m_criticalState), LogLevel.NORMAL);
        }
        else // This block handles standard "under attack" (not emergency forceDefend)
        {
            defaultStateForArea = IA_GroupTacticalState.Attacking; // Default response
            float defenderPercentage;
            if (m_criticalState)
            {
                defenderPercentage = 0.75; // NEW - 75% defenders if critical
            }
            else
            {
                defenderPercentage = 0.65; // NEW - 65% defenders if normal
            }
            // --- Use renamed local variable ---
            localTargetDefenders = Math.Max(1, Math.Round(totalMilitaryGroups * defenderPercentage));
            // --- Use renamed local variable ---
            int offensiveGroups = totalMilitaryGroups - localTargetDefenders;
            if (offensiveGroups > 0 && validThreatLocation && m_allowFlankingOperations)
            {
                float flankerRatio;
                if (m_criticalState)
                {
                    flankerRatio = 0.35; // Was 0.2 - now 35% of offensive units will be flankers in critical state
                }
                else
                {
                    flankerRatio = 0.45; // Was 0.33 - now 45% of offensive units will be flankers in normal state
                }
                // --- Use renamed local variable ---
                localTargetFlankers = Math.Round(offensiveGroups * flankerRatio);
                if (offensiveGroups < 3)
                {
                    if (offensiveGroups == 2)
                    {
                        // --- Use renamed local variable ---
                        localTargetFlankers = 1; // Always have 1 flanker when we have 2 offensive groups
                    }
                    else // offensiveGroups == 1
                    {
                         // --- Use renamed local variable ---
                        localTargetFlankers = 0;
                    }
                }
                // --- Use renamed local variables ---
                localTargetAttackers = offensiveGroups - localTargetFlankers;
                int attackerToFlankerRatio = 0;
                // --- Use renamed local variable ---
                if (localTargetFlankers > 0)
                {
                    // --- Use renamed local variables ---
                    attackerToFlankerRatio = Math.Round(localTargetAttackers / localTargetFlankers);
                }
                Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Under Attack - Offensive): Off=%1 -> Flank=%2, Attack=%3 (1:%4 ratio), Critical=%5",
                    // --- Use renamed local variables ---
                    offensiveGroups, localTargetFlankers, localTargetAttackers, attackerToFlankerRatio, m_criticalState), LogLevel.NORMAL);
            }
            else // Not enough offensive groups or no valid threat location for flanking
            {
                // --- Use renamed local variables ---
                localTargetFlankers = 0;
                localTargetAttackers = offensiveGroups; // Assign all remaining as attackers (could be 0)
                Print(string.Format("[AreaInstance.MilitaryTask] Role Calc (Under Attack - No Flank): Off=%1 -> Flank=0, Attack=%2 (ThreatValid=%3)",
                    // --- Use renamed local variables ---
                    offensiveGroups, localTargetAttackers, validThreatLocation), LogLevel.NORMAL);
            }
            // --- Use renamed local variables ---
            if (localTargetDefenders + localTargetAttackers + localTargetFlankers != totalMilitaryGroups) {
                 Print(string.Format("[AreaInstance.MilitaryTask] Role Calc NORMAL: Role counts don't match total! Total=%1, Def=%2, Att=%3, Flk=%4",
                    // --- Use renamed local variables ---
                    totalMilitaryGroups, localTargetDefenders, localTargetAttackers, localTargetFlankers), LogLevel.ERROR);
                 // --- Use renamed local variables ---
                 localTargetAttackers = totalMilitaryGroups - localTargetDefenders - localTargetFlankers;
            }
            Print(string.Format("[AreaInstance.MilitaryTask] Role Calc Final (Under Attack): Total=%1, Def=%2, Flank=%3, Attack=%4, Critical=%5",
                // --- Use renamed local variables ---
                totalMilitaryGroups, localTargetDefenders, localTargetFlankers, localTargetAttackers, m_criticalState), LogLevel.NORMAL);
        }

        // --- BEGIN ADDED: Update defender state when under attack ---
        // When area is under attack, explicitly switch defenders to Defending state instead of DefendPatrol
        if (isUnderAttack)
        {
            // For each group currently assigned as DefendPatrol, update to Defending
            foreach (IA_AiGroup g : m_military)
            {
                if (!g || g.GetAliveCount() == 0)
                    continue;
                    
                // Get current state
                IA_GroupTacticalState currentState;
                if (m_assignedGroupStates.Find(g, currentState))
                {
                    // If this is a defender in patrol state, explicitly change to defending
                    if (currentState == IA_GroupTacticalState.DefendPatrol)
                    {
                        m_assignedGroupStates.Set(g, IA_GroupTacticalState.Defending);
                        Print(string.Format("[AreaInstance.MilitaryTask] AREA UNDER ATTACK: Converting defender from patrol to defensive stance at %1", 
                            g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                }
            }
        }
        // --- END ADDED ---

        // --- MOVED: Store pending state change requests rather than processing immediately ---
        m_pendingStateRequests.Clear(); // Clear previous requests
        
        // Collect all pending state change requests
        foreach (IA_AiGroup g : m_military)
        {
            if (!g || g.GetAliveCount() == 0)
                continue;

            if (g.HasPendingStateRequest())
            {
                IA_GroupTacticalState requestedState = g.GetRequestedState();
                vector requestedPosition = g.GetRequestedStatePosition();
                IEntity requestedEntity = g.GetRequestedStateEntity();
                IA_GroupTacticalState currentState = g.GetTacticalState(); // Get actual current state from group
                
                // Create a new pending request and store it for later processing
                IA_PendingStateRequest pendingRequest = new IA_PendingStateRequest();
                pendingRequest.group = g;
                pendingRequest.requestedState = requestedState;
                pendingRequest.requestedPosition = requestedPosition;
                pendingRequest.requestedEntity = requestedEntity;
                pendingRequest.currentState = currentState;
                
                // Process CRITICAL requests immediately (retreat, last stand)
                if (requestedState == IA_GroupTacticalState.Retreating || 
                    requestedState == IA_GroupTacticalState.LastStand || 
                    (requestedState == IA_GroupTacticalState.Defending && 
                    (currentState == IA_GroupTacticalState.Attacking || currentState == IA_GroupTacticalState.Flanking)))
                {
                    Print(string.Format("[AreaInstance.MilitaryTask] Processing CRITICAL state change request from Group %1: %2 -> %3",
                        g.GetOrigin().ToString(),
                        typename.EnumToString(IA_GroupTacticalState, currentState),
                        typename.EnumToString(IA_GroupTacticalState, requestedState)), LogLevel.NORMAL);
                        
                    // Critical requests are always approved
                    ProcessAndApproveStateChange(g, requestedState, requestedPosition, requestedEntity, currentState, currentTime, true);
                    
                    // Update pre-request counts immediately for CRITICAL requests
                    IA_GroupTacticalState stateBeforeChange = IA_GroupTacticalState.Neutral;
                    if (m_assignedGroupStates.Find(g, stateBeforeChange))
                    {
                        if (stateBeforeChange == IA_GroupTacticalState.Defending || stateBeforeChange == IA_GroupTacticalState.DefendPatrol) currentDefenders_PreRequest--;
                        else if (stateBeforeChange == IA_GroupTacticalState.Attacking) currentAttackers_PreRequest--;
                        else if (stateBeforeChange == IA_GroupTacticalState.Flanking) currentFlankers_PreRequest--;
                        
                        if (requestedState == IA_GroupTacticalState.Defending || requestedState == IA_GroupTacticalState.DefendPatrol) currentDefenders_PreRequest++;
                        else if (requestedState == IA_GroupTacticalState.Attacking) currentAttackers_PreRequest++;
                        else if (requestedState == IA_GroupTacticalState.Flanking) currentFlankers_PreRequest++;
                    }
                    
                    g.ClearStateRequest();
                }
                else
                {
                    // Non-critical requests get stored for processing after target calculation
                    m_pendingStateRequests.Insert(pendingRequest);
                }
            }
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
                Print(string.Format("[AreaInstance.MilitaryTask] NORMAL: Group %1 not in map, assigned %2.",
                    g.GetOrigin().ToString(), 
                    typename.EnumToString(IA_GroupTacticalState, currentAssignedState)), LogLevel.NORMAL);
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
                        g.GetOrigin().ToString(), remainingLockTime), LogLevel.NORMAL);
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
                    Print(string.Format("[AreaInstance.MilitaryTask] PERIL (Defender): Group %1 (Danger: %2, Health: %3). Forcing Defend at Origin.", g.GetOrigin().ToString(), danger, healthPercent), LogLevel.NORMAL);
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
                     Print(string.Format("[AreaInstance.MilitaryTask] PERIL (Attacker/Flanker): Group %1 (Units: %2). Forcing Defend at Origin.", g.GetOrigin().ToString(), g.GetAliveCount()), LogLevel.NORMAL);
                }
            }

            // Emergency defense check overrides other peril checks
            if (m_forceDefend && (currentAssignedState == IA_GroupTacticalState.Flanking))
            {
                perilDetected = true;
                stateAfterPeril = IA_GroupTacticalState.Defending;
                Print(string.Format("[AreaInstance.MilitaryTask] EMERGENCY MODE: Converting Flanking Group %1 to Defender", 
                    g.GetOrigin().ToString()), LogLevel.NORMAL);
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
                Print(string.Format("[AreaInstance.MilitaryTask] FORCE BALANCE NORMAL: Offensive units (%1) too low (%2% of total %3). Adjusting targets.", 
                    offensiveAliveUnits, Math.Round(offensiveUnitPercentage * 100), totalAliveUnits), LogLevel.NORMAL);
                
                // Original targets before adjustment
                int originalDefenders = localTargetDefenders;
                int originalAttackers = localTargetAttackers;
                int originalFlankers = localTargetFlankers;
                
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
                    localTargetDefenders = Math.Max(1, localTargetDefenders - groupsToConvert);
                    
                    // Distribute the converted groups between attackers and flankers (2/3 attackers, 1/3 flankers)
                    int additionalAttackers = Math.Ceil(groupsToConvert * 0.67);
                    int additionalFlankers = groupsToConvert - additionalAttackers;
                    
                    // If flanking not allowed, all become attackers
                    if (!m_allowFlankingOperations || !validThreatLocation)
                    {
                        additionalAttackers = groupsToConvert;
                        additionalFlankers = 0;
                    }
                    
                    localTargetAttackers += additionalAttackers;
                    localTargetFlankers += additionalFlankers;
                    
                    Print(string.Format("[AreaInstance.MilitaryTask] FORCE REBALANCE: Adjusting targets from Def:%1/Att:%2/Flk:%3 to Def:%4/Att:%5/Flk:%6", 
                        originalDefenders, originalAttackers, originalFlankers, 
                        localTargetDefenders, localTargetAttackers, localTargetFlankers), LogLevel.NORMAL);
                }
            }
        }
        // --- END ADDED ---
        
        // --- BEGIN ADDED: Cap maximum offensive roles ---
        int totalOffensiveTarget = localTargetAttackers + localTargetFlankers;
        int maxAllowedOffensive = Math.Round(totalMilitaryGroups * MAX_OFFENSIVE_ROLE_PERCENTAGE);

        if (totalOffensiveTarget > maxAllowedOffensive && totalMilitaryGroups > 1) // Apply cap if needed and more than 1 group exists
        {
            Print(string.Format("[AreaInstance.MilitaryTask] OFFENSIVE CAP APPLIED: Target Offensive (%1) exceeds max allowed (%2). Capping...",
                totalOffensiveTarget, maxAllowedOffensive), LogLevel.NORMAL);

            // Calculate desired ratio (0.4-0.6 ratio of flankers to attackers)
            const float flankerRatio = Math.RandomFloat(0.3, 0.6); 
            // Calculate new numbers maintaining ratio
            int newFlankers = Math.Round(maxAllowedOffensive * flankerRatio);
            int newAttackers = maxAllowedOffensive - newFlankers;
            
            // Ensure we have at least 1 attacker if we have any offensive units
            if (maxAllowedOffensive > 0 && newAttackers < 1)
            {
                newAttackers = 1;
                newFlankers = maxAllowedOffensive - newAttackers;
            }
            
            // If we only have 1-2 offensive slots, just make them attackers
            if (maxAllowedOffensive <= 2)
            {
                newAttackers = maxAllowedOffensive;
                newFlankers = 0;
            }
            
            // Update the targets
            localTargetFlankers = newFlankers;
            localTargetAttackers = newAttackers;

            // Increase defenders by the reduced amount
            localTargetDefenders = totalMilitaryGroups - localTargetAttackers - localTargetFlankers;

            Print(string.Format("[AreaInstance.MilitaryTask] OFFENSIVE CAP RESULT: New targets Def:%1/Att:%2/Flk:%3 (Maintaining %4/%5 ratio)",
                localTargetDefenders, localTargetAttackers, localTargetFlankers, 
                (1.0 - flankerRatio), flankerRatio), LogLevel.NORMAL);
        }
        // --- END ADDED ---
        
        // --- MOVED: Now process the non-critical state change requests using the FINAL capped targets ---
        // Track current role counts (after applying the caps) for request decisions
        int finalTargetDefenders = localTargetDefenders/1.4;
        int finalTargetAttackers = localTargetAttackers;
        int finalTargetFlankers = localTargetFlankers;
        int currentDefendersAfterCap = currentDefenders;
        int currentAttackersAfterCap = currentAttackers;
        int currentFlankersAfterCap = currentFlankers;
        
        Print(string.Format("[AreaInstance.MilitaryTask] After Cap - Role Counts: Def=%1/%2, Att=%3/%4, Flk=%5/%6",
            currentDefendersAfterCap, finalTargetDefenders,
            currentAttackersAfterCap, finalTargetAttackers,
            currentFlankersAfterCap, finalTargetFlankers), LogLevel.NORMAL);
        
        // Now process standard state change requests using the FINAL target values
        foreach (IA_PendingStateRequest request : m_pendingStateRequests)
        {
            IA_AiGroup g = request.group;
            if (!g || g.GetAliveCount() == 0)
                continue;
                
            // Skip if the request is no longer valid
            if (!g.HasPendingStateRequest())
                continue;
                
            // Reference request data for readability
            IA_GroupTacticalState requestedState = request.requestedState;
            vector requestedPosition = request.requestedPosition;
            IEntity requestedEntity = request.requestedEntity;
            IA_GroupTacticalState currentState = g.GetTacticalState(); // Get actual current state from group
                
            Print(string.Format("[AreaInstance.MilitaryTask] Processing state change request from Group %1: %2 -> %3",
                g.GetOrigin().ToString(),
                typename.EnumToString(IA_GroupTacticalState, currentState), // Log actual current state
                typename.EnumToString(IA_GroupTacticalState, requestedState)), LogLevel.NORMAL);

            bool approveRequest = false;
            string reason = "";

            // State change decision based on final target numbers after capping
            if (requestedState == IA_GroupTacticalState.Attacking)
            {
                if (currentAttackersAfterCap >= finalTargetAttackers)
                {
                    approveRequest = false;
                    reason = string.Format("Area already has %1/%2 attackers (after cap)", currentAttackersAfterCap, finalTargetAttackers);
                }
                else 
                {
                    // --- BEGIN ENHANCED: Higher approval chance when low on attackers ---
                    // Check how critically we need attackers
                    float approvalProbability = 0.5; // Base 50% chance
                    
                    // If we have no attackers at all, almost always approve (90%)
                    if (currentAttackersAfterCap == 0)
                    {
                        approvalProbability = 0.9;
                        reason = "Critical attacker shortage - high priority approval";
                        Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL ATTACKER SHORTAGE (0 attackers). Prioritizing approval for Group %1", 
                            g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                    // If we're well below target, increase chance (75%)
                    else if (currentAttackersAfterCap < finalTargetAttackers * 0.5)
                    {
                        approvalProbability = 0.75;
                        reason = "Significant attacker shortage - increased approval chance";
                        Print(string.Format("[AreaInstance.MilitaryTask] ATTACKER SHORTAGE (%1/%2). Increasing approval chance for Group %3", 
                            currentAttackersAfterCap, finalTargetAttackers, g.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
                    // --- END ENHANCED ---
                    // Approve based on the calculated probability
                    approveRequest = (Math.RandomFloat(0, 1) < approvalProbability);
                    
                    // --- BEGIN ADDED: Update counters on approval ---
                    if (approveRequest)
                    {
                        currentAttackersAfterCap++;
                        // Determine which role the group is coming from and decrement accordingly
                        IA_GroupTacticalState roleBeforeChange = IA_GroupTacticalState.Neutral;
                        if (m_assignedGroupStates.Find(g, roleBeforeChange))
                        {
                            if (roleBeforeChange == IA_GroupTacticalState.Defending || roleBeforeChange == IA_GroupTacticalState.DefendPatrol)
                                currentDefendersAfterCap--;
                            else if (roleBeforeChange == IA_GroupTacticalState.Flanking)
                                currentFlankersAfterCap--;
                            // No need to decrement if it was already Attacking
                        }
                    }
                    // --- END ADDED ---
                }
            }
            else if (requestedState == IA_GroupTacticalState.Flanking)
            {
                if (!m_allowFlankingOperations)
                {
                    approveRequest = false;
                    reason = "Flanking operations not allowed in current tactical situation";
                }
                else if (currentFlankersAfterCap >= finalTargetFlankers)
                {


                        approveRequest = false;
                        reason = string.Format("Area already has %1/%2 flankers (after cap)", currentFlankersAfterCap, finalTargetFlankers);
                    
                }
                else
                {
                    // --- BEGIN ADDED: Log defender check values ---
                    Print(string.Format("[DEFENDER CHECK (Flanking)] Request from %1: currentDefendersAfterCap=%2, finalTargetDefenders=%3",
                        g.GetOrigin().ToString(), currentDefendersAfterCap, finalTargetDefenders), LogLevel.NORMAL);
                    // --- END ADDED ---

                    // NEW CHECK: Prevent defender count from going too low
                    IA_GroupTacticalState roleBeforeChange = IA_GroupTacticalState.Neutral; // Keep the original declaration here
                    if (m_assignedGroupStates.Find(g, roleBeforeChange) &&
                        (roleBeforeChange == IA_GroupTacticalState.Defending || roleBeforeChange == IA_GroupTacticalState.DefendPatrol) &&
                        currentDefendersAfterCap <= finalTargetDefenders)
                    {

                            approveRequest = false;
                            reason = string.Format("Cannot reduce defender count below minimum (%1/%2)",
                                currentDefendersAfterCap, finalTargetDefenders);
                            Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL DEFENDER CHECK: Prevented change to Flanking. Defenders: %1/%2",
                                currentDefendersAfterCap, finalTargetDefenders), LogLevel.NORMAL);
                       
                    }
                    else
                    {
                        // Normal approval - already have enough defenders and under flanker cap
                        approveRequest = true;
                    }
                }
            }
            else if (requestedState == IA_GroupTacticalState.Defending || requestedState == IA_GroupTacticalState.DefendPatrol)
            {
                if (currentDefendersAfterCap >= finalTargetDefenders && !m_forceDefend)
                {
                    // Only deny if we're not in force defend mode
                    approveRequest = false;
                    reason = string.Format("Area already has %1/%2 defenders (after cap)", currentDefendersAfterCap, finalTargetDefenders);
                }
                else
                {
                    // Higher probability of approval for defensive role (75%)
                    approveRequest = (Math.RandomFloat(0, 1) < 0.75);
                    if (approveRequest)
                    {
                        currentDefendersAfterCap++;
                        // Determine which role the group is coming from and decrement accordingly
                        IA_GroupTacticalState roleBeforeChange = IA_GroupTacticalState.Neutral;
                        if (m_assignedGroupStates.Find(g, roleBeforeChange))
                        {
                            if (roleBeforeChange == IA_GroupTacticalState.Attacking)
                                currentAttackersAfterCap--;
                            else if (roleBeforeChange == IA_GroupTacticalState.Flanking)
                                currentFlankersAfterCap--;
                        }
                    }
                }
            }
            else // Another state (retreat, last stand, etc.)
            {
                // Usually approve non-standard states with a moderate probability
                approveRequest = (Math.RandomFloat(0, 1) < 0.6);
            }

            // Emergency defense override - always approve defensive state changes in emergency
            if (m_forceDefend && (requestedState == IA_GroupTacticalState.Defending || requestedState == IA_GroupTacticalState.DefendPatrol))
            {
                approveRequest = true;
                reason = "Emergency defense mode - defensive requests approved";
            }

            if (approveRequest)
            {
                 Print(string.Format("[AreaInstance.MilitaryTask] APPROVED standard state change to %1 for Group %2 (Post-Cap Counts: Def=%3/%4, Att=%5/%6, Flk=%7/%8)",
                     typename.EnumToString(IA_GroupTacticalState, requestedState),
                     g.GetOrigin().ToString(),
                     currentDefendersAfterCap, finalTargetDefenders,
                     currentAttackersAfterCap, finalTargetAttackers,
                     currentFlankersAfterCap, finalTargetFlankers), LogLevel.NORMAL);

                ProcessAndApproveStateChange(g, requestedState, requestedPosition, requestedEntity, currentState, currentTime, false);
            }
            else
            {
                if (reason != "")
                {
                    Print(string.Format("[AreaInstance.MilitaryTask] DENIED state change to %1 for Group %2 - %3",
                        typename.EnumToString(IA_GroupTacticalState, requestedState),
                        g.GetOrigin().ToString(), reason), LogLevel.NORMAL);
                }
                else
                {
                    Print(string.Format("[AreaInstance.MilitaryTask] DENIED standard state change to %1 for Group %2 - baseline probability failed",
                        typename.EnumToString(IA_GroupTacticalState, requestedState),
                        g.GetOrigin().ToString()), LogLevel.NORMAL);
                }
            }

            g.ClearStateRequest();
        }
        // --- END MOVED ---

        // --- BEGIN ADDED: Log state distribution after reassignment ---
        int postReassignmentDefenders = 0;
        int postReassignmentAttackers = 0;
        int postReassignmentFlankers = 0;
        int postReassignmentOther = 0;
        foreach (IA_AiGroup g_log : m_military)
        {
            if (!g_log || g_log.GetAliveCount() == 0 || g_log.IsDriving()) continue;
            IA_GroupTacticalState finalState = IA_GroupTacticalState.Neutral;
            if (m_assignedGroupStates.Find(g_log, finalState)) {
                if (finalState == IA_GroupTacticalState.Defending || finalState == IA_GroupTacticalState.DefendPatrol)
                    postReassignmentDefenders++;
                else if (finalState == IA_GroupTacticalState.Attacking)
                    postReassignmentAttackers++;
                else if (finalState == IA_GroupTacticalState.Flanking)
                    postReassignmentFlankers++;
                else postReassignmentOther++;
            } else postReassignmentOther++; // Should not happen
        }
        Print(string.Format("[AreaInstance.MilitaryTask] Post-Reassignment State Map: Def=%1, Att=%2, Flk=%3, Other=%4 (Targets: Def=%5, Att=%6, Flk=%7)",
            postReassignmentDefenders, postReassignmentAttackers, postReassignmentFlankers, postReassignmentOther,
            localTargetDefenders, localTargetAttackers, localTargetFlankers), LogLevel.NORMAL);
        // --- END ADDED ---

        // --- Stage 7: Enforcement & Idle Handling ---
        foreach (IA_AiGroup g : m_military)
        {
             if (!g || g.GetAliveCount() == 0) continue;
             if (g.IsDriving()) continue; // Already handled

            IA_GroupTacticalState finalAssignedState = m_assignedGroupStates.Get(g); // Get final role for this cycle
            IA_GroupTacticalState actualState = g.GetTacticalState();
            vector targetForState = primaryThreatLocation; // Default
            
            // --- BEGIN ADDED: Special handling for attacker reinforcement ---
            // Check if we're critically short on attackers and this group could help
            if (g.GetAliveCount() >= 3 && isUnderAttack && actualState != IA_GroupTacticalState.Attacking)
            {
                // Count how many attackers we currently have
                int currentAttackerCount = 0;
                foreach (IA_AiGroup atg : m_military)
                {
                    if (atg && atg.GetAliveCount() > 0 && !atg.IsDriving())
                    {
                        IA_GroupTacticalState ats = atg.GetTacticalState();
                        if (ats == IA_GroupTacticalState.Attacking)
                            currentAttackerCount++;
                    }
                }
                
                // If we have no active attackers but we're under attack, force this group to attack
                // if they're in good condition and not already in a critical state
                if (currentAttackerCount == 0 && g.GetAliveCount() >= 4 && 
                    actualState != IA_GroupTacticalState.Retreating && 
                    actualState != IA_GroupTacticalState.LastStand)
                {
                    finalAssignedState = IA_GroupTacticalState.Attacking;
                    m_assignedGroupStates.Set(g, finalAssignedState);
                    Print(string.Format("[AreaInstance.MilitaryTask] EMERGENCY ATTACKER ASSIGNMENT: No active attackers! Group %1 assigned attacker role.",
                        g.GetOrigin().ToString()), LogLevel.NORMAL);
                }
            }
            // --- END ADDED ---

            // --- BEGIN ADDED: Threat position update for attackers and flankers ---
            // Check if this group is currently an attacker or flanker with valid threat data
            if ((actualState == IA_GroupTacticalState.Attacking || actualState == IA_GroupTacticalState.Flanking) && 
                isUnderAttack && validThreatLocation)
            {
                // Get the currently assigned threat position
                vector currentAssignedThreat = vector.Zero;
                bool hasAssignedThreat = m_assignedThreatPosition.Find(g, currentAssignedThreat);
                
                // Check if this group has already investigated its current threat
                bool hasInvestigated = false;
                m_hasInvestigatedThreat.Find(g, hasInvestigated);
                
                // Check when this position was last updated
                int lastThreatUpdateTime = 0;
                if (!m_lastThreatUpdateTime.Find(g, lastThreatUpdateTime))
                {
                    // If not recorded yet, assume it was a while ago
                    lastThreatUpdateTime = currentTime - 120;
                }
                
                // Determine if we should update the threat position:
                // 1. If current threat has been investigated, OR
                // 2. If enough time has passed since last update, OR
                // 3. If the new threat is significantly different from the current one
                bool shouldUpdateThreat = false;
                
                // Calculate the time since last threat update
                int timeSinceLastUpdate = currentTime - lastThreatUpdateTime;
                
                // If already investigated or enough time has passed, consider updating
                if (hasInvestigated || timeSinceLastUpdate > THREAT_UPDATE_INTERVAL) 
                {
                    shouldUpdateThreat = true;
                }
                // If the new threat position is significantly different from the current position
                else if (hasAssignedThreat && primaryThreatLocation != vector.Zero && 
                         vector.Distance(primaryThreatLocation, currentAssignedThreat) > THREAT_POSITION_UPDATE_THRESHOLD)
                {
                    shouldUpdateThreat = true;
                }
                
                // Update the threat position if needed
                if (shouldUpdateThreat) 
                {
                    // Update the stored threat position
                    m_assignedThreatPosition.Set(g, primaryThreatLocation);
                    m_lastThreatUpdateTime.Set(g, currentTime);
                    
                    // Reset investigation for a new threat position
                    m_hasInvestigatedThreat.Set(g, false);
                    m_investigationProgress.Set(g, 0.0);
                    
                    // If the group doesn't have orders, immediately issue new orders to the updated position
                    if (!g.HasActiveWaypoint())
                    {
                        if (actualState == IA_GroupTacticalState.Attacking)
                        {
                            // Issue direct attack orders to new threat position
                            Print(string.Format("[AreaInstance.MilitaryTask] UPDATING ATTACKER TARGET: Group %1 redirected to new threat at %2 (moved %3m)", 
                                g.GetOrigin().ToString(), primaryThreatLocation.ToString(), 
                                vector.Distance(currentAssignedThreat, primaryThreatLocation)), LogLevel.NORMAL);
                            
                            g.RemoveAllOrders();
                            g.AddOrder(primaryThreatLocation, IA_AiOrder.SearchAndDestroy, true);
                        }
                        else if (actualState == IA_GroupTacticalState.Flanking)
                        {
                            // Calculate new flanking position for the updated threat
                            vector newFlankPos = g.CalculateFlankingPosition(primaryThreatLocation, g.GetOrigin());
                            
                            Print(string.Format("[AreaInstance.MilitaryTask] UPDATING FLANKER TARGET: Group %1 redirected to flank new threat at %2", 
                                g.GetOrigin().ToString(), primaryThreatLocation.ToString()), LogLevel.NORMAL);
                            
                            g.RemoveAllOrders();
                            g.AddOrder(newFlankPos, IA_AiOrder.Move, true);
                            // Then add a secondary order to attack the original position once flanking is complete
                            g.AddOrder(primaryThreatLocation, IA_AiOrder.SearchAndDestroy, false);
                        }
                    }
                }
            }
            // --- END ADDED ---

            // Determine correct target based on FINAL assigned state
            if (finalAssignedState == IA_GroupTacticalState.Defending || finalAssignedState == IA_GroupTacticalState.DefendPatrol)
            {
                if (finalAssignedState == IA_GroupTacticalState.Defending && isUnderAttack)
                {
                    // When under attack use a random position within the inner ~60% of the area for defenders
                    float defenseRadius = m_area.GetRadius() * 0.4;
                    
                    // Units should be distributed throughout the inner area instead of all at origin
                    targetForState = IA_Game.rng.GenerateRandomPointInRadius(defenseRadius * 0.2, defenseRadius, m_area.GetOrigin());
                    
                }
                else if (finalAssignedState == IA_GroupTacticalState.DefendPatrol)
                {
                    // For patrol state, use wider area (up to 80% of area radius)
                    float patrolRadius = m_area.GetRadius() * 0.8;
                    targetForState = IA_Game.rng.GenerateRandomPointInRadius(1, patrolRadius, m_area.GetOrigin());
                }
                else
                {
                    // Default fallback to area origin for other cases
                    targetForState = m_area.GetOrigin();
                }
                
                // If peril forced defend, use group origin for defenders converting from other roles
                if(needsRoleUpdate.Contains(g) && (actualState == IA_GroupTacticalState.Attacking || actualState == IA_GroupTacticalState.Flanking)){
                    targetForState = g.GetOrigin(); // Retreating attacker/flanker defends where they are initially
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 Peril Defend Target OVERRIDE to Group Origin.", g.GetOrigin().ToString()), LogLevel.NORMAL);
                }
            }
            else if (finalAssignedState == IA_GroupTacticalState.Flanking && !validThreatLocation)
            {
                // Fallback if assigned Flank but threat is gone: Attack towards origin
                finalAssignedState = IA_GroupTacticalState.Attacking;
                targetForState = m_area.GetOrigin();
                Print(string.Format("[AreaInstance.MilitaryTask] Group %1 fallback Flank -> Attack Origin", g.GetOrigin().ToString()), LogLevel.NORMAL);
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
            bool needsEnforcement = (actualState != finalAssignedState) || needsRoleUpdate.Contains(g);
            
            if (needsEnforcement)
            {
                 // --- BEGIN MODIFIED --- Added Logging
                 Print(string.Format("[AreaInstance.MilitaryTask] State ENFORCEMENT for Group %1: Assigned=%2, Actual=%3 -> NeedsUpdate=%4. Setting state to %2 with target %5",
                     g.GetOrigin().ToString(),
                     typename.EnumToString(IA_GroupTacticalState, finalAssignedState),
                     needsRoleUpdate.Contains(g),
                     typename.EnumToString(IA_GroupTacticalState, actualState),
                     targetForState.ToString()), LogLevel.NORMAL);

                // When setting to attack or flank, register this as start of investigation
                if (finalAssignedState == IA_GroupTacticalState.Attacking || finalAssignedState == IA_GroupTacticalState.Flanking)
                {
                    m_hasInvestigatedThreat.Set(g, false);
                    m_assignedThreatPosition.Set(g, targetForState);
                    m_investigationProgress.Set(g, 0.0);
                    Print(string.Format("[AreaInstance.MilitaryTask] Group %1 starting threat investigation at %2", 
                        g.GetOrigin().ToString(), targetForState.ToString()), LogLevel.NORMAL);
                }

                g.SetAssignedArea(m_area); // Set the assigned area
                g.SetTacticalState(finalAssignedState, targetForState, null, true); // Always use authority flag
                continue; // Skip idle/timeout for this cycle
            }

            // --- MOVED --- Set the assigned area regardless of enforcement need
            g.SetAssignedArea(m_area); // Set the assigned area

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
                    Print(string.Format("[AreaInstance.MilitaryTask] ACCEPTING autonomous state change for Group %1: Assigned=%2 -> Actual=%3 (Duration %4s >= %5s OR Change Allowed %6)",
                        g.GetOrigin().ToString(),
                        typename.EnumToString(IA_GroupTacticalState, finalAssignedState),
                        typename.EnumToString(IA_GroupTacticalState, actualState),
                        stateDuration, MINIMUM_STATE_DURATION, allowAutonomousChange), LogLevel.NORMAL);
                        
                    // Update our records to match the new reality
                    m_assignedGroupStates.Set(g, actualState);
                    m_stateStartTimes.Set(g, currentTime);
                    m_stateStability.Set(g, 0);
                    finalAssignedState = actualState; // Update for use in the rest of this method
                    // Do NOT set needsEnforcement here - we just accepted the state change
                }
                else
                {
                    // NEW: We need to check specific cases for attacker/defender balance
                    
                    // Get the current composition stats to influence decisions
                    int totalAttackers = 0;
                    int totalDefenders = 0;
                    foreach (IA_AiGroup checkGroup : m_military)
                    {
                        if (!checkGroup || checkGroup.GetAliveCount() == 0) continue;
                        IA_GroupTacticalState checkState = checkGroup.GetTacticalState();
                        
                        if (checkState == IA_GroupTacticalState.Attacking)
                            totalAttackers++;
                        else if (checkState == IA_GroupTacticalState.Defending || checkState == IA_GroupTacticalState.DefendPatrol)
                            totalDefenders++;
                    }
                    
                    // Calculate if we're critically low on attackers - less than 10% of groups are attackers
                    bool criticalAttackerShortage = (float)totalAttackers / (totalAttackers + totalDefenders + 1) < 0.1;
                    
                    // Special case: Attacker changing to something else - this might be a problem for force balance 
                    if (finalAssignedState == IA_GroupTacticalState.Attacking && totalAttackers <= localTargetAttackers)
                    {
                        // We're at or below target attackers - assess the current danger level for this unit
                        float dangerLevel = g.GetCurrentDangerLevel();
                        
                        // If the group has high danger (>0.85) and <= 3 members, allow retreat to defensive stance
                        if (dangerLevel > 0.85 && g.GetAliveCount() <= 3)
                        {
                            // Accept the change if unit in severe danger
                            Print(string.Format("[AreaInstance.MilitaryTask] ACCEPTING Attacker->Other change for Group %1: Assigned=%2 -> Actual=%3 due to SEVERE DANGER (Lvl: %4, Units: %5)",
                                g.GetOrigin().ToString(), typename.EnumToString(IA_GroupTacticalState, finalAssignedState), typename.EnumToString(IA_GroupTacticalState, actualState),
                                dangerLevel, g.GetAliveCount()), LogLevel.NORMAL);

                            m_assignedGroupStates.Set(g, actualState);
                            m_stateStartTimes.Set(g, currentTime);
                            m_stateStability.Set(g, 0);
                            finalAssignedState = actualState;
                            continue;
                        }
                        
                        // If we're critically short on attackers and this isn't a high-danger situation, enforce attacker role
                        if (criticalAttackerShortage && dangerLevel < 0.75)
                        {
                            // --- BEGIN MODIFIED --- Added Logging
                            Print(string.Format("[AreaInstance.MilitaryTask] CORRECTING UNAUTHORIZED CHANGE (Attacker Shortage): Group %1 changed Assigned=%2 -> Actual=%3. Enforcing %2.",
                                g.GetOrigin().ToString(), typename.EnumToString(IA_GroupTacticalState, finalAssignedState), typename.EnumToString(IA_GroupTacticalState, actualState)), LogLevel.NORMAL);
                            // --- END MODIFIED ---

                            g.SetTacticalState(finalAssignedState, targetForState, null, true);
                            needsEnforcement = true;
                            continue;
                        }
                    }
                    
                    // Default case - enforce the original state with authority flag
                    Print(string.Format("[AreaInstance.MilitaryTask] CORRECTING UNAUTHORIZED STATE CHANGE (Default): Group %1 changed Assigned=%2 -> Actual=%3 WITHOUT AUTHORITY. Enforcing %2.",
                        g.GetOrigin().ToString(),
                        typename.EnumToString(IA_GroupTacticalState, finalAssignedState),
                        typename.EnumToString(IA_GroupTacticalState, actualState)), LogLevel.NORMAL);

                    g.SetTacticalState(finalAssignedState, targetForState, null, true);
                    needsEnforcement = true; // Mark that we enforced state this cycle
                    continue; // Skip idle handling since we just enforced state
                }
            }

            
            // Random order refresh time between 35-55 seconds instead of fixed 45
            int randomOrderRefresh = 35 + IA_Game.rng.RandInt(0, 20);
            if (g.TimeSinceLastOrder() >= randomOrderRefresh)
            {
                g.RemoveAllOrders();
            }
            
            // If the group's assigned role is patrol and it has no orders, give it a new patrol target
            if ((finalAssignedState == IA_GroupTacticalState.DefendPatrol) && !g.HasActiveWaypoint())
            {
                vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() * 1.0, m_area.GetOrigin());
                // For civilians, use Neutral or DefendPatrol state instead of direct patrol order
                g.SetTacticalState(IA_GroupTacticalState.DefendPatrol, pos, null, true); // Added authority flag
            }
        }

        // --- BEGIN ADDED: Attacker Replacement Logic ---
        // After Stage 5: Role Distribution Accounting, around line 1522 after the post-reassignment log

        // --- BEGIN ADDED: Detect and replace lost attackers ---
        // Check if we previously had attackers but now have significantly fewer
        if (isUnderAttack && currentAttackers < localTargetAttackers && totalMilitaryGroups > 1) {
            // We're losing attackers while under attack - need to reinforce
            int missingAttackers = localTargetAttackers - currentAttackers;
            
            // If we've lost most of our attacking force, this is critical
            if (missingAttackers >= 2 || (localTargetAttackers > 0 && currentAttackers == 0)) {
                Print(string.Format("[AreaInstance.MilitaryTask] CRITICAL ATTACKER LOSS DETECTED: %1/%2 attackers remaining. Forcing attacker replacement.", 
                    currentAttackers, localTargetAttackers), LogLevel.NORMAL);
                
                // Find suitable defenders to convert
                int defendersToConvert = Math.Min(missingAttackers, Math.Max(1, currentDefenders - 1));
                int convertCount = 0;
                
                // Find healthy defenders to convert to attackers
                foreach (IA_AiGroup g : m_military) {
                    if (!g || g.GetAliveCount() < 3 || g.IsDriving())
                        continue;
                        
                    IA_GroupTacticalState groupState;
                    if (!m_assignedGroupStates.Find(g, groupState))
                        continue;
                        
                    // Only convert defenders with sufficient strength
                    if ((groupState == IA_GroupTacticalState.Defending || groupState == IA_GroupTacticalState.DefendPatrol) 
                        && g.GetAliveCount() >= 3 && convertCount < defendersToConvert) {
                        
                        // Check if state is time-locked
                        int stateStartTime = currentTime;
                        m_stateStartTimes.Find(g, stateStartTime);
                        int stateDuration = currentTime - stateStartTime;
                        
                        // Even override time locks in critical situations
                        if (currentAttackers == 0 || stateDuration >= MINIMUM_STATE_DURATION / 2) {
                            // Immediately change to attacker role
                            m_assignedGroupStates.Set(g, IA_GroupTacticalState.Attacking);
                            m_stateStartTimes.Set(g, currentTime);
                            m_stateStability.Set(g, 0);
                            
                            Print(string.Format("[AreaInstance.MilitaryTask] EMERGENCY CONVERSION: Group %1 converted from defender to attacker", 
                                g.GetOrigin().ToString()), LogLevel.NORMAL);
                                
                            convertCount++;
                            currentAttackers++;
                            currentDefenders--;
                        }
                    }
                }
            }
        }
        // --- END ADDED ---

        // --- BEGIN ADDED: Check for attackers/flankers that have been out of contact too long ---
        // This check now runs regardless of the overall area attack status
        currentTime = System.GetUnixTime(); // Ensure currentTime is up-to-date

        foreach (IA_AiGroup g : m_military)
        {
            if (!g || g.GetAliveCount() == 0 || g.IsDriving()) continue;
            
            // Get the current state and check for attacking/flanking groups
            IA_GroupTacticalState currentGrpState = g.GetTacticalState();
            bool isOffensive = (currentGrpState == IA_GroupTacticalState.Attacking || currentGrpState == IA_GroupTacticalState.Flanking);
            
            if (isOffensive)
            {
                // Check when this group was last under fire
                int lastFireTime = 0;
                if (!m_lastUnderFireTime.Find(g, lastFireTime))
                {
                    // If no record exists, use area's last fire time or a default
                    // We might need a better fallback if area time is also stale
                    lastFireTime = m_areaLastUnderFireTime; 
                }
                
                // Calculate time since last contact for this specific group
                int timeSinceContact = currentTime - lastFireTime;
                
                // Determine which timeout to use based on whether it's a flanking unit or attacker
                int timeoutToUse;
                if (currentGrpState == IA_GroupTacticalState.Flanking)
                {
                    timeoutToUse = MAX_FLANKING_WITHOUT_CONTACT; // Use longer timeout for flanking units
                }
                else
                {
                    timeoutToUse = MAX_ATTACK_WITHOUT_CONTACT; // Standard timeout for regular attackers
                }
                
                // If it's been too long without contact for this group, convert to defending
                if (timeSinceContact > timeoutToUse)
                {
                    Print(string.Format("[AreaInstance.MilitaryTask] INDIVIDUAL CONTACT TIMEOUT: Group %1 in %2 state for %3s without contact (max %4s). Converting to defender.", 
                        g.GetOrigin().ToString(), 
                        typename.EnumToString(IA_GroupTacticalState, currentGrpState),
                        timeSinceContact,
                        timeoutToUse), LogLevel.NORMAL);
                    
                    // Update to Defending state
                    m_assignedGroupStates.Set(g, IA_GroupTacticalState.Defending);
                    m_stateStartTimes.Set(g, currentTime);
                    m_stateStability.Set(g, 0);
                    
                    // Reset investigation tracking since we're abandoning the offensive operation
                    m_hasInvestigatedThreat.Set(g, false);
                    m_investigationProgress.Set(g, 0.0);
                    
                    // Mark that this group needs state enforcement
                    needsRoleUpdate.Insert(g, true); // Ensure the state change is applied later
                }
            }
        }
        // --- END MOVED ---

        // --- Area-wide check: Only perform if the area is considered secure ---
        bool areaHasRecentContact = (currentTime - m_areaLastUnderFireTime) < MAX_AREA_WITHOUT_CONTACT;

        // Only perform contact checks if the area is not currently under attack
        if (!isUnderAttack && !m_forceDefend)
        {
            // Area-wide check to return to patrol mode (remains inside the condition)
            if (!areaHasRecentContact)
            {
                Print(string.Format("[AreaInstance.MilitaryTask] AREA PACIFICATION: Area '%1' has been peaceful for %2 seconds. Converting defenders to patrol state.", 
                    m_area.GetName(), currentTime - m_areaLastUnderFireTime), LogLevel.NORMAL);
                
                foreach (IA_AiGroup g : m_military)
                {
                    if (!g || g.GetAliveCount() == 0 || g.IsDriving()) continue;
                    
                    IA_GroupTacticalState currentGrpState = g.GetTacticalState();
                    
                    // Convert Defending units to DefendPatrol
                    if (currentGrpState == IA_GroupTacticalState.Defending)
                    {
                        m_assignedGroupStates.Set(g, IA_GroupTacticalState.DefendPatrol);
                        m_stateStartTimes.Set(g, currentTime);
                        m_stateStability.Set(g, 0);
                        
                        // Mark that this group needs state enforcement
                        needsRoleUpdate.Insert(g, true);
                    }
                }
                
                // Reset the area's last fire time to prevent repeated patrol conversions
                m_areaLastUnderFireTime = currentTime - MAX_AREA_WITHOUT_CONTACT + 60; // Wait another minute before next check
            }
        }
        // --- END ADDED --- // This comment seems misplaced, likely from the original position. Removing it.
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
                //Print("[IA_AreaInstance.GenerateRandomAiGroups] Invalid unit count (" + scaledUnitCount + ") for squad type " + st + ", skipping group.", LogLevel.NORMAL);
                continue;
            }
            
            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(pos, m_faction, scaledUnitCount);
            if (!grp)
            {
                //Print("[IA_AreaInstance.GenerateRandomAiGroups] Failed to create military group from units.", LogLevel.NORMAL);
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
                //Print("[DEBUG] SpawnVehicleReinforcements: No road found, using perimeter position at " + spawnPos, LogLevel.NORMAL);
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
                //Print("[DEBUG] IA_AreaInstance.SpawnVehicleReinforcements: Failed to spawn vehicle", LogLevel.NORMAL);
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
            //Print("[DEBUG] ScheduleVehicleReplacement: No road found, using perimeter position at " + spawnPos, LogLevel.NORMAL);
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
            //Print("[DEBUG] IA_AreaInstance.ScheduleVehicleReplacement: Failed to spawn replacement vehicle", LogLevel.NORMAL);
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
                //Print("[DEBUG] SpawnInitialVehicles: No road found, using random position at " + spawnPos, LogLevel.NORMAL);
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
            //Print("[DEBUG] SpawnCivilianVehicle: No road found, using random position at " + spawnPos, LogLevel.NORMAL);
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
               //Print("[DEBUG_CIV_VEHICLES] No road found, using random position at " + spawnPos + " for vehicle " + (i+1), LogLevel.NORMAL);
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
               //Print("[DEBUG_CIV_VEHICLES] Failed to spawn civilian vehicle at position: " + spawnPos, LogLevel.NORMAL);
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
        m_centralReactionManager.ClearAllReactions(); // Use ClearAllReactions instead of Clear
        
        // Track highest danger group for central reaction
        IA_AiGroup highestDangerGroup = null;
        float highestDanger = 0.0;
        vector highestDangerPos = vector.Zero;
        
        // Create a map to track which groups already have reactions this cycle
        ref map<IA_AiGroup, bool> processedGroups = new map<IA_AiGroup, bool>();
        
        // Collect and process individual group reactions first
        array<IA_AiGroup> allGroups = {};
        // Copy military groups to allGroups
        foreach (IA_AiGroup group : m_military)
        {
            allGroups.Insert(group);
        }
        
        // Process only military groups
        foreach (IA_AiGroup group : allGroups)
        {
            if (!group || !group.IsSpawned() || group.GetAliveCount() == 0 || group.IsDriving())
                continue;
                
            // Check if the group already has active orders and has been stable in its state
            // If it's been in a tactical state for a while, we may want to avoid disrupting it
            int timeSinceLastChange = currentTime - group.GetLastStateChangeTime();
            
            // Get the current state
            IA_GroupTacticalState currentState = group.GetTacticalState();
            bool isAttacker = (currentState == IA_GroupTacticalState.Attacking);
            bool isFlanker = (currentState == IA_GroupTacticalState.Flanking);
            
            // Give attackers and flankers more stability - they need longer periods without interruption
            bool isStableState = (timeSinceLastChange > 15);  // 15 seconds stability for regular groups
            if (isAttacker || isFlanker) {
                isStableState = (timeSinceLastChange > 30);  // 30 seconds stability for offensive groups
            }
            
            // Skip very stable groups that already have orders, let them continue their current behavior
            if (isStableState && group.HasOrders()) {
                Print(string.Format("[AreaInstance.AIReactionsTask] Group at %1 - SKIPPING (stable state for %2 seconds)", 
                    group.GetOrigin().ToString(), timeSinceLastChange), LogLevel.NORMAL);
                processedGroups.Insert(group, true);
                continue;
            }
            
            // Get danger level
            float dangerLevel = group.GetDangerLevel();
            
            if (dangerLevel > highestDanger)
            {
                highestDanger = dangerLevel;
                highestDangerGroup = group;
                highestDangerPos = group.GetOrigin();
            }
            
            float underFireThreshold = 0.98;
            float enemySpottedThreshold = 0.8;
            
            // Apply reactions based on danger level
            if (dangerLevel > underFireThreshold)
            {
                Print(string.Format("[AreaInstance.AIReactionsTask] Group at %1 - HIGH danger (%2) - triggering UnderFire reaction", 
                    group.GetOrigin().ToString(), dangerLevel), LogLevel.NORMAL);
                    
                // Add to central reaction manager for later group-wide processing
                m_centralReactionManager.TriggerReaction(IA_AIReactionType.UnderFire, dangerLevel, group.GetOrigin()); // Use TriggerReaction
                
                // Mark this group as processed
                processedGroups.Insert(group, true);
            }
            else if (dangerLevel > enemySpottedThreshold)
            {
                Print(string.Format("[AreaInstance.AIReactionsTask] Group at %1 - MEDIUM danger (%2) - triggering EnemySpotted reaction", 
                    group.GetOrigin().ToString(), dangerLevel), LogLevel.NORMAL);
                    
                // Only process individual EnemySpotted if the group isn't already part of a UnderFire reaction
                if (!processedGroups.Contains(group))
                {
                    // Create a reaction state and apply it
                    ApplyEnemySpottedReactionToGroup(group, group.GetOrigin(), dangerLevel);
                    
                    // Mark this group as processed
                    processedGroups.Insert(group, true);
                }
            }
        }
        
        // Process central reactions after individual ones
        m_centralReactionManager.ProcessReactions(); // Let the manager update its internal state
        
        if (m_centralReactionManager && m_centralReactionManager.HasActiveReaction())
        {
            IA_AIReactionState currentCentralReaction = m_centralReactionManager.GetCurrentReaction();
            IA_AIReactionType reactionType = currentCentralReaction.GetReactionType();
            vector reactionPos = currentCentralReaction.GetSourcePosition();
            float reactionIntensity = currentCentralReaction.GetIntensity();

            Print(string.Format("[AreaInstance.AIReactionsTask] Processing CENTRAL reaction: Type=%1, Intensity=%2, SourcePos=%3", 
                typename.EnumToString(IA_AIReactionType, reactionType), reactionIntensity, reactionPos.ToString()), LogLevel.NORMAL);
                
            // Apply the current central reaction to all groups that weren't already processed
            foreach (IA_AiGroup group : allGroups)
            {
                // Skip groups that were already processed individually
                if (processedGroups.Contains(group) && processedGroups.Get(group))
                    continue;
                
                if (!group.IsSpawned() || group.GetAliveCount() == 0)
                    continue;
                
                // Apply reaction based on type
                switch (reactionType)
                {
                    case IA_AIReactionType.UnderFire:
                        ApplyUnderFireReactionToGroup(group, reactionPos, reactionIntensity);
                        break;
                    
                    case IA_AIReactionType.EnemySpotted:
                        ApplyEnemySpottedReactionToGroup(group, reactionPos, reactionIntensity);
                        break;
                    
                    // Add cases for other reaction types if needed
                    
                    default:
                        break;
                }
            }
        }
    }

    // --- Add these helper methods for reaction processing ---
    private void ApplyUnderFireReactionToGroup(IA_AiGroup group, vector sourcePos, float intensity)
    {
        if (!group)
            return;
        
        // Update engagement state if not already engaged
        if (!group.IsEngagedWithEnemy())
        {
            // Try to determine a reasonable faction based on this group's faction
            IA_Faction sourceFaction = IA_Faction.NONE;
            if (group.GetFaction() == IA_Faction.US)
            {
                sourceFaction = IA_Faction.USSR;
            }
            else if (group.GetFaction() == IA_Faction.USSR)
            {
                sourceFaction = IA_Faction.US;
            }
            
            // Update engagement if we determined a sensible enemy faction
            if (sourceFaction != IA_Faction.NONE && group)
            {
                group.m_engagedEnemyFaction = sourceFaction;
            }
        }
        
        // --- BEGIN ADDED: Update "under fire" timestamps ---
        int currentTime = System.GetUnixTime();
        // Update the group's last under fire time
        m_lastUnderFireTime.Set(group, currentTime);
        // Update the area's last under fire time
        m_areaLastUnderFireTime = currentTime;
        Print(string.Format("[ApplyUnderFireReactionToGroup] UNDER FIRE timestamp updated for Group %1: %2", 
            group.GetOrigin().ToString(), currentTime), LogLevel.NORMAL);
        // --- END ADDED ---
        
        // Skip if group is driving
        if (group.IsDriving())
            return;
        
        int aliveCount = group.GetAliveCount();
        
        // Check if this group is already assigned as an attacker or flanker
        IA_GroupTacticalState currentState = group.GetTacticalState();
        bool isAssignedAttacker = (currentState == IA_GroupTacticalState.Attacking);
        bool isAssignedFlanker = (currentState == IA_GroupTacticalState.Flanking);
        
        // Debug logging
        Print(string.Format("[ApplyUnderFireReactionToGroup] Group %1 | Threat Pos: %2 | Intensity: %3 | Alive: %4 | Distance: %5m", 
            group, sourcePos.ToString(), intensity, aliveCount, vector.Distance(sourcePos, group.GetOrigin())), LogLevel.NORMAL);
        
        // If the group already has waypoints and is in an appropriate state, don't change anything
        bool hasAppropriateState = false;
        if (group.HasOrders()) {
            // For attackers and flankers - if they already have orders, let them keep them
            if (isAssignedAttacker || isAssignedFlanker) {
                hasAppropriateState = true;
            }
            
            // For defenders - if they're already defending, don't change
            if (currentState == IA_GroupTacticalState.Defending) {
                hasAppropriateState = true;
            }
        }
        
        // If the group already has appropriate orders, don't change them
        if (hasAppropriateState) {
            return;
        }
        
        // Determine appropriate reaction based on state and distance
        float distanceToSource = vector.Distance(group.GetOrigin(), sourcePos);
        
        // Attackers should generally advance toward the threat
        if (isAssignedAttacker)
        {
            // Even attackers should maintain some distance from big threats
            if (intensity > 0.8 && distanceToSource < 50 && aliveCount < 4)
            {
                // Hold position if too close with small numbers
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin());
            }
            else
            {
                // Otherwise attack toward the source
                group.RemoveAllOrders();
                group.AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy);
            }
        }
        // Flankers should move around to flanking positions
        else if (isAssignedFlanker)
        {
            // --- BEGIN MODIFIED: Use the improved flanking position calculation ---
            // Original simple perpendicular vector approach:
            // vector flankDir = Vector(-sourcePos[2], 0, sourcePos[0]).Normalized();
            // vector flankPos = sourcePos + flankDir * 80;
            
            // Use the improved flanking position calculation method
            vector flankPos = group.CalculateFlankingPosition(sourcePos, group.GetOrigin());
            
            group.RemoveAllOrders();
            group.AddOrder(flankPos, IA_AiOrder.Move);
            // --- END MODIFIED ---
        }
        // Default behavior for other groups
        else
        {
            // Defensive reaction for most units
            // If very close to threat, hold position
            if (distanceToSource < 100 && intensity > 0.7)
            {
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin());
            }
            // If medium distance and large group, send some to attack
            else if (distanceToSource < 250 && aliveCount > 5)
            {
                group.RemoveAllOrders();
                group.AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy);
            }
            // Otherwise defend in place
            else
            {
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin());
            }
        }
    }

    private void ApplyEnemySpottedReactionToGroup(IA_AiGroup group, vector sourcePos, float intensity)
    {
        if (!group)
            return;
        
        // Skip for civilians and vehicles
        if (group.IsDriving())
            return;
        
        int aliveCount = group.GetAliveCount();
        
        // Check if this group is already assigned as an attacker or flanker
        IA_GroupTacticalState currentState = group.GetTacticalState();
        bool isAssignedAttacker = (currentState == IA_GroupTacticalState.Attacking);
        bool isAssignedFlanker = (currentState == IA_GroupTacticalState.Flanking);
        
        // If the group already has waypoints and is in an appropriate state, don't change anything
        bool hasAppropriateState = false;
        if (group.HasOrders()) {
            // For attackers and flankers - if they already have orders, let them keep them
            if (isAssignedAttacker || isAssignedFlanker) {
                hasAppropriateState = true;
            }
            
            // For defenders - if they're already defending, don't change
            if (currentState == IA_GroupTacticalState.Defending) {
                hasAppropriateState = true;
            }
        }
        
        // If the group already has appropriate orders, don't change them
        if (hasAppropriateState) {
            return;
        }
        
        // Determine appropriate reaction based on state and distance
        float distanceToSource = vector.Distance(group.GetOrigin(), sourcePos);
        
        // Attackers should generally advance toward the threat
        if (isAssignedAttacker)
        {
            // Order direct attack to source position
            group.RemoveAllOrders();
            group.AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy);
        }
        // Flankers should move around to flanking positions
        else if (isAssignedFlanker)
        {
            // Calculate flanking position
            vector flankDir = Vector(-sourcePos[2], 0, sourcePos[0]).Normalized();
            vector flankPos = sourcePos + flankDir * 80;
            
            group.RemoveAllOrders();
            group.AddOrder(flankPos, IA_AiOrder.Move);
        }
        // Default behavior for other groups
        else
        {
            // EnemySpotted typically leads to less aggressive behavior than UnderFire
            // Large groups at close range might attack
            if (distanceToSource < 200 && aliveCount > 6)
            {
                group.RemoveAllOrders();
                group.AddOrder(sourcePos, IA_AiOrder.SearchAndDestroy);
            }
            // Medium groups would be more cautious
            else if (distanceToSource < 300 && aliveCount > 3)
            {
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin());
            }
            // Small groups would be defensive
            else
            {
                group.SetTacticalState(IA_GroupTacticalState.Defending, group.GetOrigin());
            }
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
    private const int REACTION_PROCESS_INTERVAL = 20; // Process reactions every 30 seconds

    // --- Near the top, before any methods, add these class member variables
    // Threat investigation tracking
    private ref map<IA_AiGroup, bool> m_hasInvestigatedThreat = new map<IA_AiGroup, bool>();
    private ref map<IA_AiGroup, vector> m_assignedThreatPosition = new map<IA_AiGroup, vector>();
    private ref map<IA_AiGroup, float> m_investigationProgress = new map<IA_AiGroup, float>(); // 0.0 to 1.0 progress
    private const float INVESTIGATION_COMPLETE_DISTANCE = 30.0; // Must get within 30m of threat location
    private const int MINIMUM_INVESTIGATION_TIME = 60; // At least 60 seconds of investigation

    // --- BEGIN ADDED: Threat position update tracking ---
    private ref map<IA_AiGroup, int> m_lastThreatUpdateTime = new map<IA_AiGroup, int>(); // Last time each group had its threat position updated
    private const int THREAT_UPDATE_INTERVAL = 30; // Update threat position every 60 seconds if needed
    private const float THREAT_POSITION_UPDATE_THRESHOLD = 50.0; // Update if threat moves more than 50m
    // --- END ADDED ---

    // --- Add tracking for last "under fire" time ---
    private ref map<IA_AiGroup, int> m_lastUnderFireTime = new map<IA_AiGroup, int>(); // Last time each group was under fire
    private int m_areaLastUnderFireTime = 0; // Last time any group in area was under fire
    private const int MAX_ATTACK_WITHOUT_CONTACT = 60; // 1 minute without contact = return to defending
    private const int MAX_AREA_WITHOUT_CONTACT = 90; // 1.5 minutes without area contact = return to patrol

    // --- BEGIN ADDED: Longer timeout for flanking units ---
    private const int MAX_FLANKING_WITHOUT_CONTACT = 180; // 3 minutes without contact for flanking units
    // --- END ADDED ---

    // State control flags - very specific conditions where units can change states
    private bool m_forceDefend = false; // Emergency defensive mode for the entire area
    private bool m_allowFlankingOperations = true; // Whether flanking is tactically viable now
    private int m_reassignmentCooldown = 0; // Cooldown for next batch of reassignments
    private const float MAX_OFFENSIVE_ROLE_PERCENTAGE = 0.2; // Cap offensive roles at 30% of total groups

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

    // Add a method to get the current target attackers for use in state correction logic
    private int getTargetAttackers()
    {
        return this.targetAttackers;
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
                bool approveRequest = (Math.RandomFloat01() < 0.9);
                
                if (approveRequest)
                {
                    // Apply the requested state with authority flag
                    g.SetTacticalState(requestedState, requestedPosition, null, true);
                    Print(string.Format("[AreaInstance.CivilianOrderTask] APPROVED state change to %1 for civilian group", 
                        typename.EnumToString(IA_GroupTacticalState, requestedState)), LogLevel.NORMAL);
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
            int randomOrderRefresh = 35 + Math.RandomInt(0, 20);
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

    // Add ProcessAndApproveStateChange method
    private void ProcessAndApproveStateChange(IA_AiGroup g, IA_GroupTacticalState requestedState, vector requestedPosition, IEntity requestedEntity, IA_GroupTacticalState currentState, int currentTime, bool critical = false)
    {
        if (!g)
            return;
            
        // Update our internal group state map
        m_assignedGroupStates.Set(g, requestedState);
        
        // Reset stability counter when state changes
        m_stateStability.Set(g, 0);
        
        // Reset state start time for time locking
        m_stateStartTimes.Set(g, currentTime);
        
        // --- BEGIN ADDED: Update threat tracking for attacking and flanking assignments ---
        // When assigning attacking or flanking states, update threat tracking
        if (requestedState == IA_GroupTacticalState.Attacking || requestedState == IA_GroupTacticalState.Flanking)
        {
            // Store the threat position
            if (requestedPosition != vector.Zero)
            {
                m_assignedThreatPosition.Set(g, requestedPosition);
                m_lastThreatUpdateTime.Set(g, currentTime);
                
                // Reset investigation tracking
                m_hasInvestigatedThreat.Set(g, false);
                m_investigationProgress.Set(g, 0.0);
                
                Print(string.Format("[AreaInstance.ProcessAndApproveStateChange] THREAT TRACKING: Group %1 assigned target at %2", 
                    g.GetOrigin().ToString(), requestedPosition.ToString()), LogLevel.NORMAL);
            }
        }
        // --- END ADDED ---
        
        // Execute the actual state change on the group
        g.SetTacticalState(requestedState, requestedPosition, requestedEntity, true); // Use authority flag
        
        Print(string.Format("[AreaInstance.ProcessAndApproveStateChange] Changed group %1 state from %2 to %3 (critical: %4)",
            g.GetOrigin().ToString(),
            typename.EnumToString(IA_GroupTacticalState, currentState),
            typename.EnumToString(IA_GroupTacticalState, requestedState),
            critical), LogLevel.NORMAL);
    }

    // Add this function near the end of the class, before the closing bracket
    // Attempts to prioritize rebuilding the attacking force when it has been decimated
    private void PrioritizeAttackerReplacement()
    {
        // Get current time for state tracking
        int currentTime = System.GetUnixTime();

        // Only run this function when we're under attack
        if (!IsUnderAttack())
            return;

        // Count current attackers
        int currentAttackers = 0;
        int totalAvailableGroups = 0;
        // --- BEGIN ADDED: Count current flankers and defenders for cap calculation ---
        int currentFlankers = 0;
        int currentDefenders = 0;
        // --- END ADDED ---

        // --- BEGIN ADDED: Calculate Primary Threat Location ---
        vector primaryThreatLocation = m_area.GetOrigin(); // Default
        bool validThreatLocation = false;
        vector dangerSum = vector.Zero;
        int dangerCount = 0;
        
        foreach (IA_AiGroup g_threat : m_military)
        {
            if (!g_threat || g_threat.GetAliveCount() == 0 || g_threat.IsDriving())
                continue;
                
            totalAvailableGroups++; // Count available groups here
            
            // Check for engagement or recent danger
            if (g_threat.IsEngagedWithEnemy() || (System.GetUnixTime() - g_threat.GetLastDangerEventTime() < 60))
            {
                 vector currentDangerPos = g_threat.GetLastDangerPosition();
                 if(currentDangerPos != vector.Zero)
                 {
                      // Only consider danger points reasonably within or near the area
                      if (vector.DistanceSq(currentDangerPos, m_area.GetOrigin()) < (m_area.GetRadius() * 1.5) * (m_area.GetRadius() * 1.5))
                      {
                         dangerSum += currentDangerPos;
                         dangerCount++;
                      }
                 }
            }

            IA_GroupTacticalState state;
            if (m_assignedGroupStates.Find(g_threat, state) && state == IA_GroupTacticalState.Attacking)
                currentAttackers++;
            // --- BEGIN ADDED: Count flankers and defenders ---
            else if (state == IA_GroupTacticalState.Flanking)
                currentFlankers++;
            else if (state == IA_GroupTacticalState.Defending || state == IA_GroupTacticalState.DefendPatrol)
                currentDefenders++;
            // --- END ADDED ---
        }

        if (dangerCount > 0)
        {
            primaryThreatLocation = dangerSum / dangerCount;
            validThreatLocation = true;
            Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] Threat location calculated: %1", primaryThreatLocation.ToString()), LogLevel.NORMAL);
        }
        else
        {
             Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] No specific threat location found, will use area origin as fallback."), LogLevel.NORMAL);
        }
        // --- END ADDED ---
        
        // If no groups available, we can't do anything
        if (totalAvailableGroups <= 1)
            return;
        
        // Calculate target attackers (similar to MilitaryOrderTask logic)
        float defenderPercentage;
        if (m_criticalState)
        {
            defenderPercentage = 0.9;
        }
        else
        {
            defenderPercentage = 0.65;
        }
        int targetDefenders = Math.Max(1, Math.Round(totalAvailableGroups * defenderPercentage));
        int targetAttackers = totalAvailableGroups - targetDefenders;
        // --- BEGIN ADDED: Calculate target flankers for capping logic ---
        int targetFlankers = 0; // Initialize
		float flankerRatio = 0.2;
        if(targetAttackers > 0 && validThreatLocation && m_allowFlankingOperations) {
             
             if (m_criticalState) flankerRatio = 0.10; else flankerRatio = 0.25;
             targetFlankers = Math.Round(targetAttackers * flankerRatio);
             if (targetAttackers < 3) {
                 if (targetAttackers == 2) targetFlankers = 1; else targetFlankers = 0;
             }
             targetAttackers = targetAttackers - targetFlankers; // Adjust attackers based on flankers
        }
        // --- END ADDED ---

        // --- BEGIN ADDED: Apply offensive cap logic within this function ---
        int totalOffensiveTarget = targetAttackers + targetFlankers;
        int maxAllowedOffensive = Math.Round(totalAvailableGroups * MAX_OFFENSIVE_ROLE_PERCENTAGE);
        int cappedTargetAttackers = targetAttackers; // Start with uncapped values
        int cappedTargetFlankers = targetFlankers;
        int cappedTargetDefenders = targetDefenders;

        if (totalOffensiveTarget > maxAllowedOffensive && totalAvailableGroups > 1)
        {
            Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] OFFENSIVE CAP APPLIED (Internal): Target Offensive (%1) exceeds max (%2). Capping...",
                totalOffensiveTarget, maxAllowedOffensive), LogLevel.NORMAL);


            int newFlankers = Math.Round(maxAllowedOffensive * flankerRatio);
            int newAttackers = maxAllowedOffensive - newFlankers;

            if (maxAllowedOffensive > 0 && newAttackers < 1) { newAttackers = 1; newFlankers = maxAllowedOffensive - newAttackers; }
            if (maxAllowedOffensive <= 2) { newAttackers = maxAllowedOffensive; newFlankers = 0; }

            cappedTargetFlankers = newFlankers;
            cappedTargetAttackers = newAttackers;
            cappedTargetDefenders = totalAvailableGroups - cappedTargetAttackers - cappedTargetFlankers;

            Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] OFFENSIVE CAP RESULT (Internal): New targets Def:%1/Att:%2/Flk:%3",
                cappedTargetDefenders, cappedTargetAttackers, cappedTargetFlankers), LogLevel.NORMAL);
        }
        // --- END ADDED ---


        // If attacker count is severely depleted, force reassignment
        // --- MODIFIED: Use cappedTargetAttackers for the check ---
        if (currentAttackers < cappedTargetAttackers && (currentAttackers == 0 || currentAttackers < cappedTargetAttackers * 0.5))
        {
            // --- MODIFIED: Log using capped target ---
            Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] CRITICAL ATTACKER DEPLETION: %1/%2 attackers (Capped Target). Forcing reassignment.",
                currentAttackers, cappedTargetAttackers), LogLevel.NORMAL);

            // Find suitable defenders to convert (up to the number needed)
            // --- MODIFIED: Use cappedTargetAttackers and currentDefenders for calculation ---
            int neededAttackers = Math.Min(cappedTargetAttackers - currentAttackers, Math.Max(1, currentDefenders - 1)); // Use current defenders count
            // --- END MODIFIED ---
            int convertCount = 0;

            foreach (IA_AiGroup g : m_military)
            {
                if (!g || g.GetAliveCount() < 3 || g.IsDriving() || convertCount >= neededAttackers)
                    continue;
                    
                IA_GroupTacticalState state;
                if (!m_assignedGroupStates.Find(g, state))
                    continue;
                    
                // Only convert defenders with sufficient strength
                if ((state == IA_GroupTacticalState.Defending || state == IA_GroupTacticalState.DefendPatrol) 
                    && g.GetAliveCount() >= 3)
                {
                    // Force change to attacker role
                    m_assignedGroupStates.Set(g, IA_GroupTacticalState.Attacking);
                    m_stateStartTimes.Set(g, currentTime);
                    m_stateStability.Set(g, 0);
                    
                    // Apply the change immediately using the calculated threat location
                    g.SetTacticalState(IA_GroupTacticalState.Attacking, primaryThreatLocation, null, true); // Use primaryThreatLocation
                    
                    Print(string.Format("[AreaInstance.PrioritizeAttackerReplacement] EMERGENCY CONVERSION: Group %1 converted to attacker, targeting %2 (ValidThreat: %3)", 
                        g.GetOrigin().ToString(), primaryThreatLocation.ToString(), validThreatLocation), LogLevel.NORMAL);
                        
                    convertCount++;
                }
            }
        }
    }

    // --- BEGIN ADDED: Spawn Reinforcement Wave Logic ---
    private bool SpawnReinforcementWave(int groupsToSpawn)
    {
        if (m_reinforcementGroupsSpawned >= m_totalReinforcementQuota)
        {
            Print(string.Format("[AreaInstance.SpawnReinforcementWave] Area %1 cannot spawn: Quota met (%2/%3).", 
                m_area.GetName(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota), LogLevel.NORMAL);
            return false; // Quota already met
        }

        int actualSpawnCount = Math.Min(groupsToSpawn, m_totalReinforcementQuota - m_reinforcementGroupsSpawned);
        if (actualSpawnCount <= 0) 
        {
            Print(string.Format("[AreaInstance.SpawnReinforcementWave] Area %1 cannot spawn: Calculated spawn count is zero or negative.", m_area.GetName()), LogLevel.NORMAL);
            return false; // Should not happen if initial check passed, but safety first
        }
        
        Print(string.Format("[AreaInstance.SpawnReinforcementWave] Area %1 attempting to spawn %2 reinforcement groups (Quota: %3/%4).", 
            m_area.GetName(), actualSpawnCount, m_reinforcementGroupsSpawned, m_totalReinforcementQuota), LogLevel.NORMAL);

        bool spawnedAny = false;
        const int MAX_SPAWN_ATTEMPTS = 10; // Max tries to find a safe spot
        const float SAFE_SPAWN_RADIUS_SQ = 300 * 300; // Check within 800m (squared for efficiency)

        // Get player positions once per wave
        array<vector> playerPositions = {};
        GetAllPlayerPositions(playerPositions);

        for (int i = 0; i < actualSpawnCount; i++)
        {
            vector spawnPos = vector.Zero;
            bool safeSpawnFound = false;
            float unsafeAngle = -1.0; // Store the angle that was unsafe in the previous attempt (-1 means no bias)

            for (int attempt = 0; attempt < MAX_SPAWN_ATTEMPTS; attempt++)
            {
                // Print(string.Format("[SpawnReinforcementWave] Attempt %1/%2 to find safe spawn location...", attempt + 1, MAX_SPAWN_ATTEMPTS), LogLevel.NORMAL);
                
                // 1. Calculate potential Spawn Position (inside attempt loop)
                float spawnMinRadius = 200;
                float spawnMaxRadius = 500;
                vector center = m_area.GetOrigin();
                
                // Try finding a road nearby first 
                vector searchPoint = IA_Game.rng.GenerateRandomPointInRadius(spawnMinRadius, spawnMaxRadius, center);
                int activeGroup = -1;
                array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
                if (markers && !markers.IsEmpty() && markers[0]) activeGroup = markers[0].m_areaGroup; 
                if (activeGroup < 0) activeGroup = IA_VehicleManager.GetActiveGroup();
                vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(searchPoint, 150, activeGroup); 
                
                if (roadPos != vector.Zero)
                {
                    spawnPos = roadPos;
                }
                else 
                { // Fallback to random point in the ring
                    float angle;
                    if (unsafeAngle != -1.0) // Check if we need to bias the angle
                    {
                        // Bias away from the unsafe angle (opposite side +/- 45 degrees)
                        float targetAngle = unsafeAngle + Math.PI; // Add 180 degrees
                        angle = targetAngle + IA_Game.rng.RandFloatXY(-Math.PI / 18, Math.PI / 18); // Reduced randomness
                        // Normalize angle to be within 0..2PI
                        while (angle < 0) angle += Math.PI2;
                        while (angle >= Math.PI2) angle -= Math.PI2;
                        //Print(string.Format("[SpawnReinforcementWave Attempt %1] Biasing angle away from %2 towards %3 (final: %4)", attempt + 1, unsafeAngle * Math.RAD2DEG, targetAngle * Math.RAD2DEG, angle * Math.RAD2DEG), LogLevel.NORMAL);
                        // unsafeAngle = -1.0; // REMOVED: Do not reset bias until a safe spot is found or max attempts reached
                    }
                    else
                    {
                        // Standard random angle
                        angle = IA_Game.rng.RandFloat01() * Math.PI2;
                    }
                    
                    float dist = IA_Game.rng.RandFloatXY(spawnMinRadius, spawnMaxRadius);
                    spawnPos[0] = center[0] + Math.Cos(angle) * dist;
                    spawnPos[2] = center[2] + Math.Sin(angle) * dist;
                    spawnPos[1] = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
                }

                Print(string.Format("[SpawnReinforcementWave Attempt %1] Candidate spawnPos: %2", attempt + 1, spawnPos.ToString()), LogLevel.NORMAL);

                // 2. Check distance to players
                bool isSafe = true;
                foreach (vector playerPos : playerPositions)
                {
                    if (vector.DistanceSq(spawnPos, playerPos) < SAFE_SPAWN_RADIUS_SQ)
                    {
                        isSafe = false;
                        // Calculate and store the angle of the player relative to the center
                        vector delta = playerPos - center;
                        unsafeAngle = Math.Atan2(delta[2], delta[0]); // atan2(z, x) gives angle in radians
                        // Normalize angle to 0..2PI for consistency
                        while (unsafeAngle < 0) unsafeAngle += Math.PI2;
                        while (unsafeAngle >= Math.PI2) unsafeAngle -= Math.PI2; 
                        
                        Print(string.Format("[SpawnReinforcementWave Attempt %1] UNSAFE: Spawn candidate %2 is too close to player at %3 (DistSq < %4). Will bias next attempt away from angle %5 deg.", 
                            attempt + 1, spawnPos.ToString(), playerPos.ToString(), SAFE_SPAWN_RADIUS_SQ, unsafeAngle * Math.RAD2DEG), LogLevel.NORMAL);
                        break; // No need to check other players for this spot
                    }
                }

                // 3. If safe, proceed and break attempt loop
                if (isSafe)
                {
                    //Print(string.Format("[SpawnReinforcementWave Attempt %1] SAFE: Found safe spawn location at %2.", attempt + 1, spawnPos.ToString()), LogLevel.NORMAL);
                    safeSpawnFound = true;
                    break; // Found a safe spot, exit attempt loop
                }
                
                // If last attempt failed, log it
                if (attempt == MAX_SPAWN_ATTEMPTS - 1)
                {
                    Print(string.Format("[SpawnReinforcementWave] Failed to find safe spawn location near %1 after %2 attempts. Skipping group.", spawnPos.ToString(), MAX_SPAWN_ATTEMPTS), LogLevel.WARNING);
                }
            } // End of attempt loop

            // If no safe spawn was found after all attempts, skip this group
            if (!safeSpawnFound)
            {
                continue; // Move to the next group index in the wave
            }

            // 4. Create Group (with scaling) - Only if safe spot found
            IA_SquadType st = IA_GetRandomSquadType();
            int unitCount = IA_SquadCount(st, m_faction); 
            int scaledUnitCount = Math.Round(unitCount * m_aiScaleFactor); // Use current scale factor
            if (scaledUnitCount < 1) scaledUnitCount = 1; 

            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(spawnPos, m_faction, scaledUnitCount);

            // 5. Spawn and Integrate
            if (grp)
            {
                grp.Spawn();
                AddMilitaryGroup(grp); // This assigns initial state based on area status
                m_reinforcementGroupsSpawned++;
                spawnedAny = true;

                // Give initial orders towards the general area
                // If primary threat location is known, use it, otherwise use area origin
                vector targetPos = m_area.GetOrigin(); 
                // TODO: Potentially access primaryThreatLocation if available?
                // It might be better to let the standard MilitaryOrderTask handle specific targeting once they integrate.
                grp.SetTacticalState(IA_GroupTacticalState.Attacking, targetPos, null, true); // Send them towards the fight
                
                Print(string.Format("[AreaInstance.SpawnReinforcementWave] Spawned reinforcement group (%1 units) at %2. Total spawned: %3/%4.",
                    scaledUnitCount, spawnPos.ToString(), m_reinforcementGroupsSpawned, m_totalReinforcementQuota), LogLevel.NORMAL);
            }
            else
            {
                Print(string.Format("[AreaInstance.SpawnReinforcementWave] Failed to create reinforcement group of type %1 at %2.", st, spawnPos.ToString()), LogLevel.WARNING);
            }
        }
        
        return spawnedAny;
    }
    // --- END ADDED ---

    // --- BEGIN ADDED: Helper to get player positions ---
    static void GetAllPlayerPositions(out array<vector> playerPositions)
    {
        playerPositions.Clear(); // Ensure the output array is empty
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return;

        array<int> playerIds = {};
        playerManager.GetAllPlayers(playerIds);

        foreach (int playerId : playerIds)
        {
            IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
            if (playerEntity)
            {
                playerPositions.Insert(playerEntity.GetOrigin());
            }
        }
    }
    // --- END ADDED ---
}
