///////////////////////////////////////////////////////////////////////
// IA_DefendMission - Secondary Objective System
///////////////////////////////////////////////////////////////////////
class IA_DefendMission
{
    private static const float PEAK_MULT_CAP = 1.75;
    private static const int BASE_WAVE_INTERVAL_MS = 25000;
    private static const int MIN_WAVE_INTERVAL_MS = 12000;
    private static const int MAX_WAVE_INTERVAL_MS = 35000;
    private static const int LULL_END_GUARD_MS = 120000;

    private vector m_defendPoint;
    private int m_startTime;
    private int m_duration = Math.RandomInt(12, 17) * 60000; // 12-16 minutes
    private int m_durationMinutes = 0;
    private int m_baseTargetAICount;
    private bool m_isActive = false;
    private int m_groupID;
    private ref array<IA_AreaInstance> m_affectedAreas = {};
    private int m_lastWaveSpawnTime = 0;
    private int m_waveSpawnInterval = BASE_WAVE_INTERVAL_MS;
    private Faction m_defendFaction = null; // Store the faction once for all waves
    private string m_defendMarkerName = ""; // Store the marker name for notifications
    private bool m_vehicleBeatSpawned = false;
    private int m_lastVehicleBeatAttempt = 0;
    private ref IA_AreaGroupManager m_ownedQrfManager; // kept alive for truck arrival CallLaters if needed
    private ref IA_EnhancedDefendDirector m_Enhanced;
    private bool m_bEnhancedClockOn;
    private IA_AreaInstance m_ExplicitHost;
    private int m_iActivationSerial;
    private bool m_bPreparationAlreadyComplete;
    private bool m_bDynamicBase;
    private ref IA_Config m_DefenseConfig;

    private float m_fStartMult = 0.75;
    private float m_fPeakMult = 1.75;
    private float m_fPeakMinutes = 6.0;
    private int m_surgeStartMs = 180000;
    private int m_surgeEndMs = 240000;
    private bool m_bHasLull = false;
    private int m_lullStartMs = 0;
    private int m_lullEndMs = 0;
    private float m_fVehicleBeatFrac = 0.4;
    
    private void IA_DefendMission(vector defendPoint, int groupID, string markerName = "")
    {
        m_defendPoint = defendPoint;
        m_groupID = groupID;
        m_defendMarkerName = markerName;
        m_durationMinutes = Math.Round(m_duration / 60000.0);
        if (m_durationMinutes < 1)
            m_durationMinutes = 1;
        m_baseTargetAICount = CalculateTargetAICount();
        
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] Created defend mission at %1 (%2) for group %3, base AI cap: %4, duration: %5 min", 
                m_defendPoint.ToString(), m_defendMarkerName, m_groupID, m_baseTargetAICount, m_durationMinutes), LogLevel.NORMAL);
        }
    }
    
    static IA_DefendMission Create(vector defendPoint, int groupID, string markerName = "")
    {
        IA_DefendMission mission = new IA_DefendMission(defendPoint, groupID, markerName);
        IA_Config cfg = mission.GetDefenseConfig();
        if (cfg && !cfg.UseLegacyDefense())
            mission.m_Enhanced = IA_EnhancedDefendDirector.Create(mission);
        return mission;
    }

    static IA_DefendMission CreateForDynamicBase(vector defendPoint, int groupId, string displayName, IA_AreaInstance host, Faction enemyFaction, int activationSerial, IA_Config defenseConfigSnapshot)
    {
        IA_DefendMission mission = new IA_DefendMission(defendPoint, groupId, displayName);
        mission.m_ExplicitHost = host;
        mission.m_defendFaction = enemyFaction;
        mission.m_iActivationSerial = activationSerial;
        mission.m_bPreparationAlreadyComplete = true;
        mission.m_bDynamicBase = true;
        mission.m_DefenseConfig = defenseConfigSnapshot;
        IA_Config cfg = mission.GetDefenseConfig();
        if (cfg && !cfg.UseLegacyDefense())
            mission.m_Enhanced = IA_EnhancedDefendDirector.Create(mission);
        return mission;
    }

    IA_Config GetDefenseConfig()
    {
        if (m_DefenseConfig)
            return m_DefenseConfig;
        return IA_MissionInitializer.GetGlobalConfig();
    }

    bool IsPreparedDynamicBase()
    {
        return m_bPreparationAlreadyComplete;
    }

    int GetActivationSerial()
    {
        return m_iActivationSerial;
    }
    
    void StartDefendMission()
    {
        if (m_isActive)
            return;

        if (m_Enhanced)
        {
            m_Enhanced.Start();
            return;
        }
            
        m_isActive = true;
        m_startTime = System.GetTickCount();
        m_lastWaveSpawnTime = m_startTime;
        RollPressureProfile();
        
        IA_Log.Info(string.Format("[IA_DefendMission] Starting defend mission at %1", m_defendPoint.ToString()));
        
        // Get all area instances for this group
        CollectAffectedAreas();
        
        // Create defend task for players
        CreateDefendTask();
        
        // Set all existing AI to defend mode
        SetAllAIToDefendMode();

        PublishDefendHud(IA_DefendHudState.Active);
        
        // Spawn initial wave
        SpawnDefendWave();
        RollNextWaveInterval();
    }
    
    void UpdateDefendMission()
    {
        if (!m_isActive)
            return;

        if (m_Enhanced)
        {
            m_Enhanced.Update();
            return;
        }
            
        int currentTime = System.GetTickCount();
        
        // Check if mission duration is complete
        if (currentTime - m_startTime >= m_duration)
        {
            if (IA_Log.IsDebugEnabled())
            {
                Print(string.Format("[IA_DefendMission] %1 minute duration complete - ending mission", m_durationMinutes), LogLevel.NORMAL);
            }
            EndDefendMission();
            return;
        }

        PublishDefendHud(IA_DefendHudState.Active);
        
        // Mid-hold vehicle / QRF beat. Retry every 30s until success.
        if (!m_vehicleBeatSpawned)
        {
            int beatThreshold = Math.Round(m_duration * m_fVehicleBeatFrac);
            if (currentTime - m_startTime >= beatThreshold)
            {
                if (m_lastVehicleBeatAttempt == 0 || currentTime - m_lastVehicleBeatAttempt >= 30000)
                {
                    m_lastVehicleBeatAttempt = currentTime;
                    if (TrySpawnVehicleBeat())
                        m_vehicleBeatSpawned = true;
                }
            }
        }

        // Spawn new waves if needed
        if (currentTime - m_lastWaveSpawnTime >= m_waveSpawnInterval)
        {
            int currentAICount = GetCurrentAICount();
            if (currentAICount < GetEffectiveTargetAICount())
            {
                SpawnDefendWave();
                m_lastWaveSpawnTime = currentTime;
                RollNextWaveInterval();
            }
        }
    }
    
    bool IsActive()
    {
        return m_isActive;
    }

    bool IsEnhanced()
    {
        return m_Enhanced != null;
    }

    int GetDurationMs()
    {
        return m_duration;
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
            
        IA_Log.Info("[IA_DefendMission] Ending defend mission");
        if (m_Enhanced)
            m_Enhanced.CleanupEvents();
        m_isActive = false;
        
        // Complete the defend task (that path already sends TaskCompleted).
        bool didNotify = CompleteDefendTask();

        PublishDefendHud(IA_DefendHudState.Complete);
        
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (!didNotify && initializer)
        {
            string taskTitle;
            if (m_defendMarkerName.IsEmpty())
                taskTitle = "Defend Position";
            else
                taskTitle = "Defend " + m_defendMarkerName;
            initializer.TriggerGlobalNotification("TaskCompleted", taskTitle);
        }
        
        // Return all AI to normal mode
        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (area)
            {
                area.SetDefendMode(false);
            }
        }
        
        m_affectedAreas.Clear();
        if (m_ownedQrfManager)
            m_ownedQrfManager.Shutdown();
        m_ownedQrfManager = null;
        
        IA_Game gameInstance = IA_Game.Instantiate();
        if (gameInstance && gameInstance.GetActiveDefendMission() == this)
            gameInstance.SetActiveDefendMission(null);

        if (m_bDynamicBase)
        {
            IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
            if (init)
            {
                IA_DynamicObjectiveDirector director = init.GetDynamicObjectiveDirector();
                if (director && director.GetObjective())
                    director.GetObjective().OnDefenseEnded(m_iActivationSerial, true);
            }
            return;
        }

        if (initializer)
            initializer.OnDefendMissionComplete();
    }

    void AbortDefendMission()
    {
        if (!m_isActive && !m_Enhanced)
        {
            m_isActive = false;
            return;
        }

        Print("[IA_DefendMission] Aborting defend mission without success.", LogLevel.WARNING);
        if (m_Enhanced)
            m_Enhanced.CleanupEvents();
        m_isActive = false;
        DismissDefendTask();
        IA_MissionInitializer.PublishDefendHud("", IA_DefendHudState.Hidden, 0, 0, 0, IA_DefendHudPhase.None);

        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (area)
                area.SetDefendMode(false);
        }
        m_affectedAreas.Clear();
        if (m_ownedQrfManager)
            m_ownedQrfManager.Shutdown();
        m_ownedQrfManager = null;

        IA_Game gameInstance = IA_Game.Instantiate();
        if (gameInstance && gameInstance.GetActiveDefendMission() == this)
            gameInstance.SetActiveDefendMission(null);
    }

    protected void DismissDefendTask()
    {
        if (m_affectedAreas.IsEmpty())
            return;
        IA_AreaInstance firstArea = m_affectedAreas[0];
        if (firstArea)
            firstArea.DismissOpenTasks();
    }
    
    private int CalculateTargetAICount()
    {
        float scaleFactor = IA_Game.GetAIScaleFactor();
        float scaled = scaleFactor * 1.9;
        int targetCount = Math.Round(9 * (scaled * scaled) * 1.6);
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] Calculated base target AI count: %1 (scale factor: %2)", targetCount, scaleFactor), LogLevel.NORMAL);
        }
        
        return targetCount;
    }

    private void RollPressureProfile()
    {
        m_fStartMult = IA_Game.rng.RandFloatXY(0.65, 0.85);
        m_fPeakMult = IA_Game.rng.RandFloatXY(1.55, PEAK_MULT_CAP);
        if (m_fPeakMult > PEAK_MULT_CAP)
            m_fPeakMult = PEAK_MULT_CAP;
        m_fPeakMinutes = IA_Game.rng.RandFloatXY(5.0, 7.0);
        m_fVehicleBeatFrac = IA_Game.rng.RandFloatXY(0.35, 0.55);

        int surgeDurationMs = Math.Round(IA_Game.rng.RandFloatXY(50.0, 80.0) * 1000.0);
        m_surgeStartMs = 180000 + Math.Round(IA_Game.rng.RandFloatXY(0.0, 20000.0));
        m_surgeEndMs = m_surgeStartMs + surgeDurationMs;

        m_bHasLull = IA_Game.rng.RandFloat01() < 0.5;
        m_lullStartMs = 0;
        m_lullEndMs = 0;
        if (m_bHasLull)
        {
            if (!TryPlaceLull())
                m_bHasLull = false;
        }

        int lullFlag = 0;
        if (m_bHasLull)
            lullFlag = 1;
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] Pressure profile start=%1 peak=%2 at %3 min surge=%4-%5s lull=%6 vehicleBeat=%7",
                m_fStartMult, m_fPeakMult, m_fPeakMinutes, m_surgeStartMs / 1000, m_surgeEndMs / 1000, lullFlag, m_fVehicleBeatFrac), LogLevel.NORMAL);
        }
    }

    private bool TryPlaceLull()
    {
        int lullDurationMs = Math.Round(IA_Game.rng.RandFloatXY(30.0, 45.0) * 1000.0);
        int latestStart = m_duration - LULL_END_GUARD_MS - lullDurationMs;
        int earliestAfterSurge = m_surgeEndMs + 10000;
        if (earliestAfterSurge <= latestStart)
        {
            m_lullStartMs = Math.Round(IA_Game.rng.RandFloatXY(earliestAfterSurge, latestStart));
            m_lullEndMs = m_lullStartMs + lullDurationMs;
            return true;
        }

        int earliestBefore = 60000;
        int latestBefore = m_surgeStartMs - 10000 - lullDurationMs;
        if (earliestBefore <= latestBefore)
        {
            m_lullStartMs = Math.Round(IA_Game.rng.RandFloatXY(earliestBefore, latestBefore));
            m_lullEndMs = m_lullStartMs + lullDurationMs;
            return true;
        }

        return false;
    }

    private float GetIntensityMultiplier()
    {
        int elapsedMs = GetElapsedMs();
        float tMin = elapsedMs / 60000.0;
        float u = 0;
        if (m_fPeakMinutes > 0.01)
            u = tMin / m_fPeakMinutes;
        if (u < 0)
            u = 0;
        if (u > 1)
            u = 1;
        u = SmoothStep01(u);

        float mult = m_fStartMult + u * (m_fPeakMult - m_fStartMult);
        if (elapsedMs >= m_surgeStartMs && elapsedMs < m_surgeEndMs)
        {
            mult = mult + 0.20;
            if (mult > PEAK_MULT_CAP)
                mult = PEAK_MULT_CAP;
        }

        if (m_bHasLull && elapsedMs >= m_lullStartMs && elapsedMs < m_lullEndMs)
            mult = mult * 0.75;

        return mult;
    }

    private float SmoothStep01(float t)
    {
        return t * t * (3.0 - 2.0 * t);
    }

    private int GetElapsedMs()
    {
        if (!m_isActive && m_startTime == 0)
            return 0;
        int elapsed = System.GetTickCount() - m_startTime;
        if (elapsed < 0)
            return 0;
        return elapsed;
    }

    private int GetEffectiveTargetAICount()
    {
        float mult = GetIntensityMultiplier();
        int cap = Math.Round(m_baseTargetAICount * mult);
        int peakCap = Math.Round(m_baseTargetAICount * PEAK_MULT_CAP);
        if (cap > peakCap)
            cap = peakCap;
        if (cap < 2)
            cap = 2;

        int alive = GetCurrentAICount();
        if (cap < alive)
            cap = alive;
        return cap;
    }

    private void RollNextWaveInterval()
    {
        float mult = GetIntensityMultiplier();
        if (mult < 0.01)
            mult = 0.01;

        int interval = Math.Round(BASE_WAVE_INTERVAL_MS / mult);
        interval = ClampInt(interval, MIN_WAVE_INTERVAL_MS, MAX_WAVE_INTERVAL_MS);
        float jitter = IA_Game.rng.RandFloatXY(0.85, 1.15);
        interval = Math.Round(interval * jitter);
        m_waveSpawnInterval = ClampInt(interval, MIN_WAVE_INTERVAL_MS, MAX_WAVE_INTERVAL_MS);
    }

    private int ClampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    private float GetHudTimeLeft01()
    {
        if (m_duration <= 0)
            return 0;
        float left = 1.0 - (GetElapsedMs() / m_duration);
        if (left < 0)
            return 0;
        if (left > 1)
            return 1;
        return left;
    }

    private int GetHudRemainingSec()
    {
        int remainingMs = m_duration - GetElapsedMs();
        if (remainingMs < 0)
            remainingMs = 0;
        int sec = Math.Ceil(remainingMs / 1000.0);
        if (sec < 0)
            sec = 0;
        return sec;
    }

    private float GetHudPressure01()
    {
        float span = m_fPeakMult - m_fStartMult;
        if (span < 0.01)
            return 1;

        float pressure = (GetIntensityMultiplier() - m_fStartMult) / span;
        if (pressure < 0)
            return 0;
        if (pressure > 1)
            return 1;
        return pressure;
    }

    private void PublishDefendHud(IA_DefendHudState state)
    {
        string areaName = m_defendMarkerName;
        if (areaName.IsEmpty())
            areaName = "Position";

        float timeLeft = GetHudTimeLeft01();
        int remainingSec = GetHudRemainingSec();
        float pressure = GetHudPressure01();
        if (state == IA_DefendHudState.Complete)
        {
            timeLeft = 0;
            remainingSec = 0;
        }

        IA_MissionInitializer.PublishDefendHud(areaName, state, timeLeft, remainingSec, pressure, IA_DefendHudPhase.None);
    }
    
    private void CollectAffectedAreas()
    {
        if (m_bDynamicBase)
        {
            m_affectedAreas.Clear();
            if (!m_ExplicitHost || m_ExplicitHost.IsShutDown() || m_ExplicitHost.GetAreaGroup() != m_groupID)
            {
                Print("[IA_DefendMission] Explicit dynamic-base host is not live for this group.", LogLevel.ERROR);
                return;
            }
            m_affectedAreas.Clear();
            m_affectedAreas.Insert(m_ExplicitHost);
            return;
        }

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
        
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] CollectAffectedAreas: Found %1 total areas, looking for area containing defend point %2", 
                allAreas.Count(), m_defendPoint.ToString()), LogLevel.NORMAL);
        }
            
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
            if (IA_Log.IsDebugEnabled())
            {
                Print(string.Format("[IA_DefendMission] Found defend area: %1 (group %2) at distance %3 from defend point", 
                    defendArea.m_area.GetName(), defendArea.GetAreaGroup(), closestDistance), LogLevel.NORMAL);
            }
        }
        else
        {
            Print("[IA_DefendMission] Failed to find any area for defend point!", LogLevel.ERROR);
        }
        
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] CollectAffectedAreas: Collected %1 areas for defend mission", 
                m_affectedAreas.Count()), LogLevel.NORMAL);
        }
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
        string taskDesc = string.Format("Hold the position for %1 minutes against enemy attacks. Enemy pressure will increase.", m_durationMinutes);
        
        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] Creating defend task for area %1 at position %2", 
                firstArea.m_area.GetName(), m_defendPoint.ToString()), LogLevel.NORMAL);
        }

        firstArea.DismissOpenTasks();
        firstArea.QueueTask(taskTitle, taskDesc, m_defendPoint);
    }
    
    private bool CompleteDefendTask()
    {
        if (m_affectedAreas.IsEmpty())
            return false;
            
        IA_AreaInstance firstArea = m_affectedAreas[0];
        if (!firstArea)
            return false;
            
        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA_DefendMission] CompleteDefendTask: Attempting to complete defend task", LogLevel.NORMAL);
        }
        
        string taskTitle;
        if (m_defendMarkerName.IsEmpty())
            taskTitle = "Defend Position";
        else
            taskTitle = "Defend " + m_defendMarkerName;
        bool taskCompleted = firstArea.CompleteTaskByTitle(taskTitle);
        
        if (taskCompleted)
        {
            if (IA_Log.IsDebugEnabled())
            {
                Print("[IA_DefendMission] Successfully completed defend task", LogLevel.NORMAL);
            }
            return true;
        }

        Print("[IA_DefendMission] Could not find defend task to complete - it may have already been completed", LogLevel.WARNING);
        return false;
    }
    
    private void SetAllAIToDefendMode()
    {
        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA_DefendMission] Setting all existing AI to defend mode - this should only happen once!", LogLevel.NORMAL);
        }
        
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
                if (group && group.IsSpawned() && group.GetAliveCount() > 0 && !group.IsHoldingPost())
                {
                    if (IA_Log.IsDebugEnabled())
                    {
                        Print(string.Format("[IA_DefendMission] Setting defend mode for existing group at %1", group.GetOrigin().ToString()), LogLevel.NORMAL);
                    }
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
        if (targetArea.IsShutDown())
        {
            Print(string.Format("[IA_DefendMission] Cannot spawn wave: host area %1 is shut down", targetArea.m_area.GetName()), LogLevel.ERROR);
            return;
        }
        
        float scaleFactor = IA_Game.GetAIScaleFactor();
        int baseBudget = IA_GetDefendWaveUnitBudget(scaleFactor);
        float mult = GetIntensityMultiplier();
        int unitBudget = Math.Round(baseBudget * mult);
        float jitter = IA_Game.rng.RandFloatXY(0.85, 1.15);
        unitBudget = Math.Round(unitBudget * jitter);
        if (unitBudget < 8)
            unitBudget = 8;

        int room = GetEffectiveTargetAICount() - GetCurrentAICount();
        if (room < unitBudget)
            unitBudget = room;
        if (unitBudget < 2)
        {
            if (IA_Log.IsDebugEnabled())
            {
                Print(string.Format("[IA_DefendMission] Skipping wave: only %1 unit slots under cap", unitBudget), LogLevel.NORMAL);
            }
            return;
        }

        if (IA_Log.IsDebugEnabled())
        {
            Print(string.Format("[IA_DefendMission] Spawning defend wave: budget %1 units from area %2 (cap room %3, mult %4, nextInterval %5ms)", 
                unitBudget, targetArea.m_area.GetName(), room, mult, m_waveSpawnInterval), LogLevel.NORMAL);
        }
            
        // --- BEGIN MODIFIED: Use stored faction or get it once ---
        // If we haven't set the faction for this defend mission yet, get it now and store it
        if (!m_defendFaction)
        {
            IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
            if (initializer)
            {
                m_defendFaction = initializer.GetRandomEnemyFaction();
                if (IA_Log.IsDebugEnabled())
                {
                    Print(string.Format("[IA_DefendMission] Setting defend faction for all waves: %1", m_defendFaction), LogLevel.NORMAL);
                }
            }
        }
        
        if (m_defendFaction)
        {
            // First arg is unit budget when forDefendMission == true
            targetArea.SpawnReinforcementWave(unitBudget, m_defendFaction, true);
        }
        else
        {
            Print("[IA_DefendMission] Failed to get or store enemy faction for reinforcement spawn", LogLevel.WARNING);
        }
        // --- END MODIFIED ---
    }

    private bool TrySpawnVehicleBeat()
    {
        if (m_affectedAreas.IsEmpty())
        {
            Print("[IA_DefendMission] Vehicle beat skipped: no affected areas", LogLevel.WARNING);
            return false;
        }

        IA_AreaInstance targetArea = m_affectedAreas[0];
        if (!targetArea)
            return false;

        if (!m_defendFaction)
        {
            IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
            if (initializer)
                m_defendFaction = initializer.GetRandomEnemyFaction();
        }
        if (!m_defendFaction)
        {
            Print("[IA_DefendMission] Vehicle beat skipped: no enemy faction", LogLevel.WARNING);
            return false;
        }

        IA_AreaGroupManager qrfManager = GetOrCreateQrfManager(targetArea);

        // Fallback if the group manager was cleared: keep a owned manager alive for vehicle CallLaters
        if (!qrfManager)
        {
            if (!m_ownedQrfManager)
            {
                ref array<ref IA_AreaInstance> areas = new array<ref IA_AreaInstance>();
                areas.Insert(targetArea);
                m_ownedQrfManager = new IA_AreaGroupManager(areas);
            }
            qrfManager = m_ownedQrfManager;
            if (IA_Log.IsDebugEnabled())
            {
                Print("[IA_DefendMission] Vehicle beat using owned AreaGroupManager", LogLevel.NORMAL);
            }
        }

        bool spawned = qrfManager.SpawnDefendVehicleBeat(targetArea, m_defendPoint, m_defendFaction);
        if (spawned)
        {
            if (IA_Log.IsDebugEnabled())
            {
                Print("[IA_DefendMission] Mid-hold vehicle QRF beat spawned", LogLevel.NORMAL);
            }
        }
        else
            Print("[IA_DefendMission] Mid-hold vehicle QRF beat failed; will retry", LogLevel.WARNING);

        return spawned;
    }
    
    //! Counts only wave-spawned defend attackers; leftover garrison does not fill the cap.
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
                    if (group && group.IsSpawned() && group.IsDefendWaveGroup())
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

    IA_AreaInstance GetHostArea()
    {
        foreach (IA_AreaInstance area : m_affectedAreas)
        {
            if (area && !area.IsShutDown())
                return area;
        }

        if (m_affectedAreas.IsEmpty())
            return null;
        return m_affectedAreas[0];
    }

    bool IsHostingArea(IA_AreaInstance area)
    {
        if (!area)
            return false;

        foreach (IA_AreaInstance hosted : m_affectedAreas)
        {
            if (hosted == area)
                return true;
        }
        return false;
    }
    
    int GetGroupID()
    {
        return m_groupID;
    }

    string GetMarkerName()
    {
        if (m_defendMarkerName.IsEmpty())
            return "Position";
        return m_defendMarkerName;
    }

    void OnHostAreaForceFinish()
    {
        if (!m_isActive)
            return;
        if (m_bDynamicBase)
        {
            AbortDefendMission();
            IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
            if (init)
            {
                IA_DynamicObjectiveDirector director = init.GetDynamicObjectiveDirector();
                if (director && director.GetObjective())
                    director.GetObjective().OnDefenseEnded(m_iActivationSerial, false);
            }
            return;
        }
        EndDefendMission();
    }

    void BeginEnhancedHold()
    {
        if (m_isActive)
            return;
        m_isActive = true;
        m_startTime = System.GetTickCount();
        m_bEnhancedClockOn = false;
        CollectAffectedAreas();
        CreateDefendTask();
        SetAllAIToDefendMode();
    }

    void StartEnhancedClock()
    {
        m_startTime = System.GetTickCount();
        m_bEnhancedClockOn = true;
        m_lastWaveSpawnTime = m_startTime;
    }

    void SetDurationMs(int durationMs)
    {
        if (durationMs < 60000)
            durationMs = 60000;
        m_duration = durationMs;
        m_durationMinutes = Math.Round(m_duration / 60000.0);
        if (m_durationMinutes < 1)
            m_durationMinutes = 1;
    }

    int GetRemainingMs()
    {
        int remainingMs = m_duration - GetElapsedMs();
        if (remainingMs < 0)
            return 0;
        return remainingMs;
    }

    float GetElapsedClock01()
    {
        if (m_duration <= 0)
            return 1;
        float u = GetElapsedMs() / m_duration;
        if (u < 0)
            return 0;
        if (u > 1)
            return 1;
        return u;
    }

    void AdjustRemainingMs(int deltaMs, int minRemainingMs)
    {
        int current = GetRemainingMs();
        int left = current + deltaMs;
        if (deltaMs < 0)
        {
            if (minRemainingMs < 60000)
                minRemainingMs = 60000;
            if (current <= minRemainingMs)
                left = current;
            else if (left < minRemainingMs)
                left = minRemainingMs;
        }
        else if (left < 60000)
        {
            left = 60000;
        }

        m_duration = GetElapsedMs() + left;
        m_durationMinutes = Math.Round(m_duration / 60000.0);
        if (m_durationMinutes < 1)
            m_durationMinutes = 1;
    }

    bool HasPlayerContact()
    {
        IA_AreaInstance host = GetHostArea();
        if (!host)
            return false;

        ref array<vector> players = new array<vector>();
        IA_SpawnPlacement.CollectPlayerPositions(players);

        array<ref IA_AiGroup> groups = host.GetMilitaryGroups();
        if (!groups)
            return false;

        int nowUnix = System.GetUnixTime();
        int count = groups.Count();
        int i;
        for (i = 0; i < count; i++)
        {
            IA_AiGroup group = groups[i];
            if (!group || !group.IsSpawned())
                continue;
            if (!group.IsDefendWaveGroup())
                continue;

            int danger = group.GetLastDangerEventTime();
            if (danger > 0 && (nowUnix - danger) < 15)
                return true;
            if (IA_SpawnPlacement.IsNearAnyPlayer(group.GetOrigin(), players, 90))
                return true;
        }
        return false;
    }

    void NotifyPlayers(string messageType, string title)
    {
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (initializer)
            initializer.TriggerGlobalNotification(messageType, title);
    }

    void SpawnDirectorWave(int unitBudget, bool tightStagger)
    {
        if (m_affectedAreas.IsEmpty())
            return;
        IA_AreaInstance targetArea = m_affectedAreas[0];
        if (!targetArea || !targetArea.m_area || targetArea.IsShutDown())
            return;

        EnsureDefendFaction();
        if (!m_defendFaction)
            return;
        if (unitBudget < 2)
            unitBudget = 8;
        targetArea.SpawnReinforcementWave(unitBudget, m_defendFaction, true, tightStagger);
    }

    bool SpawnDoctrineBeat(IA_QRFType type)
    {
        IA_AreaInstance host = GetHostArea();
        if (!host)
            return false;
        EnsureDefendFaction();
        IA_AreaGroupManager mgr = GetOrCreateQrfManager(host);
        if (!mgr)
            return false;
        return mgr.SpawnDefendDoctrineBeat(type, host, m_defendPoint, m_defendFaction);
    }

    bool SpawnAirborneBeat(bool preferHotDrop)
    {
        IA_AreaInstance host = GetHostArea();
        if (!host)
            return false;
        EnsureDefendFaction();
        IA_AreaGroupManager mgr = GetOrCreateQrfManager(host);
        if (!mgr)
            return false;
        return mgr.SpawnDefendAirborneDrop(host, m_defendPoint, m_defendFaction, preferHotDrop);
    }

    IA_AiGroup SpawnEventConvoyVehicle(vector pos)
    {
        IA_AreaInstance host = GetHostArea();
        if (!host)
            return null;
        EnsureDefendFaction();
        if (!m_defendFaction)
            return null;

        IA_AreaGroupManager mgr = GetOrCreateQrfManager(host);
        if (!mgr)
            return null;
        return mgr.SpawnDefendConvoyTruck(host, pos, m_defendPoint, m_defendFaction);
    }

    IA_AiGroup SpawnEventGroup(vector pos, int count, bool elite, bool hvt, bool hold)
    {
        IA_AreaInstance host = GetHostArea();
        if (!host)
            return null;
        EnsureDefendFaction();
        if (!m_defendFaction)
            return null;
        if (count < 1)
            count = 1;

        IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(pos, IA_Faction.USSR, count, m_defendFaction, hvt, hvt, false, elite);
        if (!grp)
            return null;

        grp.SetAssignedArea(host.GetArea());
        grp.Spawn();
        if (hold)
            grp.SetDefendMode(true, pos);
        else
            grp.EnableInboundSimulation(host.GetArea().GetOrigin());
        host.AddMilitaryGroup(grp);
        if (elite)
            GetGame().GetCallqueue().CallLater(grp.ApplyEliteCombatProfile, 2000, false);
        return grp;
    }

    int CountWaveAI()
    {
        return GetCurrentAICount();
    }

    int GetPhaseTargetAI(float pressure01)
    {
        if (m_baseTargetAICount <= 0)
            m_baseTargetAICount = CalculateTargetAICount();
        if (pressure01 < 0)
            pressure01 = 0;
        if (pressure01 > 1)
            pressure01 = 1;
        float mult = 0.70 + (pressure01 * 0.90);
        int cap = Math.Round(m_baseTargetAICount * mult);
        if (cap < 4)
            cap = 4;
        return cap;
    }

    void PublishEnhancedHud(IA_DefendHudState state, int phase, float timeLeft, int remainingSec, float pressure)
    {
        string areaName = GetMarkerName();
        if (state == IA_DefendHudState.Complete)
        {
            timeLeft = 0;
            remainingSec = 0;
        }
        IA_MissionInitializer.PublishDefendHud(areaName, state, timeLeft, remainingSec, pressure, phase);
    }

    protected void EnsureDefendFaction()
    {
        if (m_defendFaction)
            return;
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (initializer)
            m_defendFaction = initializer.GetRandomEnemyFaction();
    }

    protected IA_AreaGroupManager GetOrCreateQrfManager(IA_AreaInstance targetArea)
    {
        IA_AreaGroupManager qrfManager = null;
        IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
        if (init)
            qrfManager = init.GetCurrentAreaGroupManager();
        if (qrfManager && qrfManager.ContainsLiveArea(targetArea))
            return qrfManager;

        if (!m_ownedQrfManager && targetArea)
        {
            ref array<ref IA_AreaInstance> areas = new array<ref IA_AreaInstance>();
            areas.Insert(targetArea);
            m_ownedQrfManager = new IA_AreaGroupManager(areas);
        }
        return m_ownedQrfManager;
    }
}
