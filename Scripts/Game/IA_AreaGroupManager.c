class IA_AreaGroupManager
{
    private ref array<ref IA_AreaInstance> m_areaInstances;

    // --- Artillery Strike System ---
    private bool m_isArtilleryStrikeActive = false;
    private int m_lastArtilleryStrikeCheckTime = 0;
    private int m_lastArtilleryStrikeEndTime = 0;
    private vector m_artilleryStrikeCenter = vector.Zero;
    private int m_artilleryStrikeSmokeTime = 0; // Time when smoke was spawned
    private bool m_artillerySmokeSpawned = false;
    private const int ARTILLERY_CHECK_INTERVAL = 60; // seconds
    private const int ARTILLERY_COOLDOWN = 300; // 5 minutes
    private const int ARTILLERY_SMOKE_TO_IMPACT_DELAY = 90; // 90 seconds
    private const float ARTILLERY_STRIKE_CHANCE = 0.25; // 25% chance per check
    private const ResourceName RED_SMOKE_EFFECT_PREFAB = "{002FEEDB0213777D}Prefabs/EffectsModuleEntities/EffectModule_Particle_Smoke_Red.et";
    private const ResourceName ARTILLERY_STRIKE_PREFAB = "{11B2A636F321AD68}PrefabsEditable/EffectsModules/Mortar/IA_EffectModule_Zoned_MortarBarrage_Large.et";

    void IA_AreaGroupManager(array<ref IA_AreaInstance> instances)
    {
        m_areaInstances = instances;
        Print(string.Format("[AreaGroupManager] Created for a group with %1 area instances.", m_areaInstances.Count()), LogLevel.NORMAL);
    }

    void ArtilleryStrikeTask()
    {
        int currentTime = System.GetUnixTime();
		Print(string.Format("[ArtilleryStrikeTask] Running for group with %1 areas.", m_areaInstances.Count()), LogLevel.DEBUG);

        // If a strike is active, manage its lifecycle
        if (m_isArtilleryStrikeActive)
        {
            if (m_artillerySmokeSpawned && (currentTime - m_artilleryStrikeSmokeTime >= ARTILLERY_SMOKE_TO_IMPACT_DELAY))
            {
                // Time for impact!
                Print(string.Format("[ArtilleryStrike] Firing artillery at %1 for area group.", m_artilleryStrikeCenter), LogLevel.NORMAL);
                
                Resource res = Resource.Load(ARTILLERY_STRIKE_PREFAB);
                if (res)
                {
                    GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(m_artilleryStrikeCenter));
                }
                else
                {
                    Print(string.Format("[ArtilleryStrike] FAILED to load artillery prefab: %1", ARTILLERY_STRIKE_PREFAB), LogLevel.ERROR);
                }

                // Strike is finished, reset state and start cooldown
                m_isArtilleryStrikeActive = false;
                m_artillerySmokeSpawned = false;
                m_artilleryStrikeCenter = vector.Zero;
                m_lastArtilleryStrikeEndTime = currentTime;
				Print(string.Format("[ArtilleryStrike] Strike finished. Cooldown started for %1 seconds.", ARTILLERY_COOLDOWN), LogLevel.NORMAL);
            }
			else if (m_artillerySmokeSpawned)
			{
				Print(string.Format("[ArtilleryStrike] Strike active, waiting for impact. %1s remaining.", ARTILLERY_SMOKE_TO_IMPACT_DELAY - (currentTime - m_artilleryStrikeSmokeTime)), LogLevel.DEBUG);
			}
            return; // Don't try to start a new one
        }

        // --- New Strike Checks ---

        // 1. Check main 60-second timer
        if (currentTime - m_lastArtilleryStrikeCheckTime < ARTILLERY_CHECK_INTERVAL)
		{
			Print(string.Format("[ArtilleryStrike] Check skipped: interval not yet met. %1s remaining.", ARTILLERY_CHECK_INTERVAL - (currentTime - m_lastArtilleryStrikeCheckTime)), LogLevel.DEBUG);
            return; // Too soon for another check
		}
        m_lastArtilleryStrikeCheckTime = currentTime;
		Print("[ArtilleryStrike] Passed 60-second interval check.", LogLevel.NORMAL);

        // 2. Check if any area in the group is under attack
        bool groupUnderAttack = false;
		string attackedAreaName;
        foreach (IA_AreaInstance instance : m_areaInstances)
        {
            if (instance && instance.IsUnderAttack())
            {
                groupUnderAttack = true;
				attackedAreaName = instance.GetArea().GetName();
                break;
            }
        }
        if (!groupUnderAttack)
		{
			Print("[ArtilleryStrike] Check failed: No area in the group is under attack.", LogLevel.NORMAL);
            return;
		}
		Print(string.Format("[ArtilleryStrike] Passed 'Under Attack' check. Area '%1' is under attack.", attackedAreaName), LogLevel.NORMAL);

        // 3. Check 5-minute cooldown
        if (currentTime - m_lastArtilleryStrikeEndTime < ARTILLERY_COOLDOWN)
		{
			Print(string.Format("[ArtilleryStrike] Check failed: On cooldown. %1 seconds remaining.", ARTILLERY_COOLDOWN - (currentTime - m_lastArtilleryStrikeEndTime)), LogLevel.NORMAL);
            return;
		}
		Print("[ArtilleryStrike] Passed cooldown check.", LogLevel.NORMAL);

        // 4. Random chance
		float randomRoll = IA_Game.rng.RandFloat01();
        if (randomRoll > ARTILLERY_STRIKE_CHANCE)
		{
			Print(string.Format("[ArtilleryStrike] Check failed: Random chance not met (Rolled %1, needed <= %2).", randomRoll, ARTILLERY_STRIKE_CHANCE), LogLevel.NORMAL);
            return;
		}
		Print(string.Format("[ArtilleryStrike] Passed random chance (Rolled %1, needed <= %2). Initiating strike.", randomRoll, ARTILLERY_STRIKE_CHANCE), LogLevel.NORMAL);
			
        // 5. Determine target location from *all* danger events in the group
        array<vector> relevantPositions = {};
        
        foreach (IA_AreaInstance instance : m_areaInstances)
        {
            if (!instance) continue;
            
            array<ref IA_AiGroup> military = instance.GetMilitaryGroups();
            foreach (IA_AiGroup group : military)
            {
                int timeSinceLastDanger = currentTime - group.GetLastDangerEventTime();
                // Only consider recent danger events
                if (group && group.GetLastDangerEventTime() > 0 && timeSinceLastDanger < 90)
                {
                    vector currentDangerPos = group.GetLastDangerPosition();
                    if (currentDangerPos != vector.Zero)
                    {
                        relevantPositions.Insert(currentDangerPos);
                    }
                }
            }
        }
        
        // BUG FIX: If no valid, recent danger events are found, abort the strike.
        if (relevantPositions.IsEmpty())
        {
            Print("[ArtilleryStrike] Strike aborted for area group. No recent danger events found.", LogLevel.NORMAL);
            return;
        }
        
		Print(string.Format("[ArtilleryStrike] Found %1 recent danger events to calculate target.", relevantPositions.Count()), LogLevel.NORMAL);
		
        // Calculate median position
        IA_VectorUtils.SortVectorsByX(relevantPositions);
        int medianIndex = relevantPositions.Count() / 2;
        vector primaryThreatLocation = relevantPositions[medianIndex];
        
        // Add 25m randomization
        m_artilleryStrikeCenter = IA_Game.rng.GenerateRandomPointInRadius(1, 25, primaryThreatLocation);
		m_artilleryStrikeCenter[1] = GetGame().GetWorld().GetSurfaceY(m_artilleryStrikeCenter[0], m_artilleryStrikeCenter[2]);
        Print(string.Format("[ArtilleryStrike] Target determined (Median): %1 for area group.", m_artilleryStrikeCenter), LogLevel.NORMAL);
    
        // 6. Spawn warning smoke
        Print(string.Format("[ArtilleryStrike] Spawning 6 warning smoke markers around %1.", m_artilleryStrikeCenter), LogLevel.NORMAL);
        Resource smokeRes = Resource.Load(RED_SMOKE_EFFECT_PREFAB);
        if (smokeRes)
        {
            for (int i = 0; i < 6; i++)
            {
                vector smokePos = IA_Game.rng.GenerateRandomPointInRadius(1, 100, m_artilleryStrikeCenter);
				smokePos[1] = GetGame().GetWorld().GetSurfaceY(smokePos[0], smokePos[2]);
                GetGame().SpawnEntityPrefab(smokeRes, null, IA_CreateSimpleSpawnParams(smokePos));
            }
        }
        else
        {
            Print(string.Format("[ArtilleryStrike] FAILED to load smoke prefab: %1", RED_SMOKE_EFFECT_PREFAB), LogLevel.ERROR);
        }
    
        // 7. Update state
        m_isArtilleryStrikeActive = true;
        m_artillerySmokeSpawned = true;
        m_artilleryStrikeSmokeTime = currentTime;
		Print(string.Format("[ArtilleryStrike] Strike is now active. Impact in %1 seconds.", ARTILLERY_SMOKE_TO_IMPACT_DELAY), LogLevel.NORMAL);
    }
}; 