///////////////////////////////////////////////////////////////////////
// IA_DefendMission - Secondary Objective System
///////////////////////////////////////////////////////////////////////
class IA_DefendMission
{
    private vector m_defendPoint;
    private int m_startTime;
    private int m_duration = Math.RandomInt(9, 14)*100000;
    private int m_targetAICount;
    private bool m_isActive = false;
    private int m_groupID;
    private ref array<IA_AreaInstance> m_affectedAreas = {};
    private int m_lastWaveSpawnTime = 0;
    private int m_waveSpawnInterval = 15000; // 15 seconds between waves
    private Faction m_defendFaction = null; // Store the faction once for all waves
    private string m_defendMarkerName = ""; // Store the marker name for notifications
    
    private void IA_DefendMission(vector defendPoint, int groupID, string markerName = "")
    {
        m_defendPoint = defendPoint;
        m_groupID = groupID;
        m_defendMarkerName = markerName;
        m_targetAICount = CalculateTargetAICount();
        
        Print(string.Format("[IA_DefendMission] Created defend mission at %1 (%2) for group %3, target AI count: %4", 
            m_defendPoint.ToString(), m_defendMarkerName, m_groupID, m_targetAICount), LogLevel.NORMAL);
    }
    
    static IA_DefendMission Create(vector defendPoint, int groupID, string markerName = "")
    {
        return new IA_DefendMission(defendPoint, groupID, markerName);
    }
    
    void StartDefendMission()
    {
        if (m_isActive)
            return;
            
        m_isActive = true;
        m_startTime = System.GetTickCount();
        m_lastWaveSpawnTime = m_startTime;
        
        Print(string.Format("[IA_DefendMission] Starting defend mission at %1", m_defendPoint.ToString()), LogLevel.NORMAL);
        
        // Get all area instances for this group
        CollectAffectedAreas();
        
        // Create defend task for players
        CreateDefendTask();
        
        // Set all existing AI to defend mode
        SetAllAIToDefendMode();
        
        // Spawn initial wave
        SpawnDefendWave();
    }
    
    void UpdateDefendMission()
    {
        if (!m_isActive)
            return;
            
        int currentTime = System.GetTickCount();
        
        // Check if mission duration is complete
        if (currentTime - m_startTime >= m_duration)
        {
            Print("[IA_DefendMission] 10 minute duration complete - ending mission", LogLevel.NORMAL);
            EndDefendMission();
            return;
        }
        
        // REMOVED: Continuously update all AI to target defend point
        // This was causing waypoint spam - AI should be set once and left alone
        // UpdateAllAIDefendTargets();
        
        // Spawn new waves if needed
        if (currentTime - m_lastWaveSpawnTime >= m_waveSpawnInterval)
        {
            int currentAICount = GetCurrentAICount();
            if (currentAICount < m_targetAICount)
            {
                SpawnDefendWave();
                m_lastWaveSpawnTime = currentTime;
            }
        }
    }
    
    bool IsActive()
    {
        return m_isActive;
    }
    
    bool IsComplete()
    {
        if (!m_isActive)
            return false;
            
        int currentTime = System.GetTickCount();
        return (currentTime - m_startTime >= m_duration);
    }
    
    void EndDefendMission()
    {
        if (!m_isActive)
            return;
            
        Print("[IA_DefendMission] Ending defend mission", LogLevel.NORMAL);
        m_isActive = false;
        
        // Complete the defend task
        CompleteDefendTask();
        
        // --- BEGIN ADDED: Trigger defend mission completion notification ---
        // Trigger notification that defend mission is complete (but don't mention RTB yet)
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (initializer)
        {
            // Use TaskCompleted type instead of AreaGroupCompleted to avoid showing RTB message
            string taskTitle;
            if (m_defendMarkerName.IsEmpty())
                taskTitle = "Defend Position";
            else
                taskTitle = "Defend " + m_defendMarkerName;
            initializer.TriggerGlobalNotification("TaskCompleted", taskTitle);
        }
        // --- END ADDED ---
        
        // Return all AI to normal mode
        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (area)
            {
                area.SetDefendMode(false);
            }
        }
        
        m_affectedAreas.Clear();
        
        // Notify IA_Game and mission initializer that defend mission is complete
        IA_Game gameInstance = IA_Game.Instantiate();
        if (gameInstance)
        {
            gameInstance.SetActiveDefendMission(null); // Clear the active defend mission
            
            // Notify mission initializer to proceed to next zone
            if (initializer)
            {
                initializer.OnDefendMissionComplete();
            }
        }
    }
    
    private int CalculateTargetAICount()
    {
        // Use existing player scaling logic
        float scaleFactor = IA_Game.GetAIScaleFactor();
        
        int targetCount = Math.Round(5 * ((scaleFactor*1.9) * (scaleFactor*1.9)));
        Print(string.Format("[IA_DefendMission] Calculated target AI count: %1 (scale factor: %2)", targetCount, scaleFactor), LogLevel.NORMAL);
        
        return targetCount;
    }
    
    private void CollectAffectedAreas()
    {
        IA_Game gameInstance = IA_Game.Instantiate();
        if (!gameInstance)
        {
            Print("[IA_DefendMission] CollectAffectedAreas: Failed to get IA_Game instance!", LogLevel.ERROR);
            return;
        }
            
        // Get all area instances and find the one containing the defend point
        array<IA_AreaInstance> allAreas = gameInstance.GetAreaInstances();
        if (!allAreas)
        {
            Print("[IA_DefendMission] CollectAffectedAreas: GetAreaInstances returned null!", LogLevel.ERROR);
            return;
        }
        
        Print(string.Format("[IA_DefendMission] CollectAffectedAreas: Found %1 total areas, looking for area containing defend point %2", 
            allAreas.Count(), m_defendPoint.ToString()), LogLevel.NORMAL);
            
        // Find the specific area that contains the defend point
        IA_AreaInstance defendArea = null;
        float closestDistance = float.MAX;
        
        foreach (IA_AreaInstance area : allAreas)
        {
            if (!area || !area.m_area)
                continue;
                
            // Check if this area contains the defend point or is closest to it
            float distance = vector.Distance(area.m_area.GetOrigin(), m_defendPoint);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                defendArea = area;
            }
        }
        
        if (defendArea)
        {
            m_affectedAreas.Insert(defendArea);
            Print(string.Format("[IA_DefendMission] Found defend area: %1 (group %2) at distance %3 from defend point", 
                defendArea.m_area.GetName(), defendArea.GetAreaGroup(), closestDistance), LogLevel.NORMAL);
        }
        else
        {
            Print("[IA_DefendMission] Failed to find any area for defend point!", LogLevel.ERROR);
        }
        
        Print(string.Format("[IA_DefendMission] CollectAffectedAreas: Collected %1 areas for defend mission", 
            m_affectedAreas.Count()), LogLevel.NORMAL);
    }
    
    private void CreateDefendTask()
    {
        // Use the first affected area to create the task
        if (m_affectedAreas.IsEmpty())
        {
            Print("[IA_DefendMission] CreateDefendTask: No affected areas found!", LogLevel.ERROR);
            return;
        }
            
        IA_AreaInstance firstArea = m_affectedAreas[0];
        if (!firstArea)
        {
            Print("[IA_DefendMission] CreateDefendTask: First area instance is null!", LogLevel.ERROR);
            return;
        }
        
        // Additional safety check
        if (!firstArea.m_area)
        {
            Print("[IA_DefendMission] CreateDefendTask: First area instance has null m_area!", LogLevel.ERROR);
            return;
        }
            
        string taskTitle;
        if (m_defendMarkerName.IsEmpty())
            taskTitle = "Defend Position";
        else
            taskTitle = "Defend " + m_defendMarkerName;
        string taskDesc = "Hold the position for 10 minutes against enemy attacks";
        
        Print(string.Format("[IA_DefendMission] Creating defend task for area %1 at position %2", 
            firstArea.m_area.GetName(), m_defendPoint.ToString()), LogLevel.NORMAL);
            
        firstArea.QueueTask(taskTitle, taskDesc, m_defendPoint);
    }
    
    private void CompleteDefendTask()
    {
        // Complete the task in the first affected area
        if (m_affectedAreas.IsEmpty())
            return;
            
        IA_AreaInstance firstArea = m_affectedAreas[0];
        if (!firstArea)
            return;
            
        Print("[IA_DefendMission] CompleteDefendTask: Attempting to complete defend task", LogLevel.NORMAL);
        
        // Use the new method to find and complete the defend task by title
        string taskTitle;
        if (m_defendMarkerName.IsEmpty())
            taskTitle = "Defend Position";
        else
            taskTitle = "Defend " + m_defendMarkerName;
        bool taskCompleted = firstArea.CompleteTaskByTitle(taskTitle);
        
        if (taskCompleted)
        {
            Print("[IA_DefendMission] Successfully completed defend task", LogLevel.NORMAL);
        }
        else
        {
            Print("[IA_DefendMission] Could not find defend task to complete - it may have already been completed", LogLevel.WARNING);
        }
    }
    
    private void SetAllAIToDefendMode()
    {
        Print("[IA_DefendMission] Setting all existing AI to defend mode - this should only happen once!", LogLevel.NORMAL);
        
        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (!area)
                continue;
                
            // Set area-level defend mode
            area.SetDefendMode(true, m_defendPoint);
            
            // Set individual group defend mode to ensure they get SearchAndDestroy orders immediately
            array<ref IA_AiGroup> militaryGroups = area.GetMilitaryGroups();
            foreach (IA_AiGroup group : militaryGroups)
            {
                if (group && group.IsSpawned() && group.GetAliveCount() > 0)
                {
                    Print(string.Format("[IA_DefendMission] Setting defend mode for existing group at %1", group.GetOrigin().ToString()), LogLevel.NORMAL);
                    // This will give them SearchAndDestroy orders on the defend point and set them to authority-managed
                    group.SetDefendMode(true, m_defendPoint);
                }
            }
        }
    }
    
    private void SpawnDefendWave(IA_AreaInstance spawnArea = null)
    {
        // Use manually passed area or fall back to first available area
        IA_AreaInstance targetArea = spawnArea;
        
        if (!targetArea)
        {
            // If no area was manually passed, use the first available area
            if (m_affectedAreas.IsEmpty())
            {
                Print("[IA_DefendMission] No affected areas available for wave spawn", LogLevel.WARNING);
                return;
            }
            
            targetArea = m_affectedAreas[0];
        }
        
        if (!targetArea || !targetArea.m_area)
        {
            Print("[IA_DefendMission] Invalid area for wave spawn", LogLevel.WARNING);
            return;
        }
        
        float scaleFactor = IA_Game.GetAIScaleFactor();
        //int groupsToSpawn = Math.Max(2, Math.Round(1.85 * ((scaleFactor*1.5) * (scaleFactor*1.5))));
        int groupsToSpawn = 1;

        Print(string.Format("[IA_DefendMission] Spawning defend wave: %1 groups from area %2", 
            groupsToSpawn, targetArea.m_area.GetName()), LogLevel.NORMAL);
            
        // --- BEGIN MODIFIED: Use stored faction or get it once ---
        // If we haven't set the faction for this defend mission yet, get it now and store it
        if (!m_defendFaction)
        {
            IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
            if (initializer)
            {
                m_defendFaction = initializer.GetRandomEnemyFaction();
                Print(string.Format("[IA_DefendMission] Setting defend faction for all waves: %1", m_defendFaction), LogLevel.NORMAL);
            }
        }
        
        if (m_defendFaction)
        {
            targetArea.SpawnReinforcementWave(groupsToSpawn, m_defendFaction, true); // true = forDefendMission
        }
        else
        {
            Print("[IA_DefendMission] Failed to get or store enemy faction for reinforcement spawn", LogLevel.WARNING);
        }
        // --- END MODIFIED ---
    }
    
    private int GetCurrentAICount()
    {
        int totalAI = 0;
        
        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (area)
            {
                array<ref IA_AiGroup> militaryGroups = area.GetMilitaryGroups();
                foreach (IA_AiGroup group : militaryGroups)
                {
                    if (group && group.IsSpawned())
                    {
                        totalAI += group.GetAliveCount();
                    }
                }
            }
        }
        
        return totalAI;
    }
    
    vector GetDefendPoint()
    {
        return m_defendPoint;
    }
    
    int GetGroupID()
    {
        return m_groupID;
    }
} 