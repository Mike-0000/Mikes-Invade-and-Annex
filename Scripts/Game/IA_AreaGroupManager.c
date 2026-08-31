// QRF type enum must be at global scope in Enfusion/Enforce Script
enum IA_QRFType
{
    Infantry,
    Motorized,
    Mechanized,
    Armoured,
    Airborne
}

class IA_AreaGroupManager
{
    private ref array<ref IA_AreaInstance> m_areaInstances;
    private bool m_bShutDown = false;

    // --- Artillery Strike System ---
    private int m_lastArtilleryStrikeCheckTime = 0;
    private int m_lastArtilleryStrikeEndTime = 0;
    private vector m_artilleryStrikeCenter = vector.Zero;
    private int m_artilleryStrikeSmokeTime = 0;
    private int m_artilleryStrikeImpactDelay = 0;
    private bool m_artillerySmokeSpawned = false;
    private const int ARTILLERY_CHECK_INTERVAL = 60; // seconds
    private const int ARTILLERY_COOLDOWN = 300; // 5+ minutes
    private const float ARTILLERY_STRIKE_CHANCE = 0.18; // chance per check
    private const ResourceName RED_SMOKE_EFFECT_PREFAB = "{002FEEDB0213777D}Prefabs/EffectsModuleEntities/EffectModule_Particle_Smoke_Red.et";
    private const int ARTILLERY_MIN_SHOTS = 6;
    private const int ARTILLERY_MAX_SHOTS = 12;
    private const int ARTILLERY_PIT_DEFENSE_COOLDOWN = 15;
    private int m_lastPitDefenseFireTime = 0;

    void IA_AreaGroupManager(array<ref IA_AreaInstance> instances)
    {
        m_areaInstances = instances;
        Print(string.Format("[AreaGroupManager] Created for a group with %1 area instances.", m_areaInstances.Count()), LogLevel.NORMAL);
    }

    // --- QRF (Quick Reaction Force) System ---

    // Check cadence
    private const int QRF_CHECK_INTERVAL = 40; // seconds
    private const int DEFEND_QRF_CHECK_INTERVAL = 30; // seconds
    private int m_lastQRFCheckTime = 0;

    // Single cooldown and chance for the whole QRF system
    private const int QRF_COOLDOWN = 220; // seconds
    private const float QRF_CHANCE = 0.13;
    private const int DEFEND_QRF_COOLDOWN = 150; // seconds
    private const float DEFEND_QRF_CHANCE = 0.45;
    private const float AIRBORNE_CHANCE = 0.12;
    private const float DEFEND_AIRBORNE_CHANCE = 0.15;
    private int m_lastQRFTime = 0;
    private bool m_qrfRetryPending = false;
    private IA_QRFType m_qrfRetryType;
    private vector m_qrfRetryTarget = vector.Zero;
    private IA_AreaInstance m_qrfRetryArea;
    private Faction m_qrfRetryFaction;
    private bool m_qrfRetryDefend = false;

    private const int AIRBORNE_QRF_DELAY_MIN_MS = 20000;
    private const int AIRBORNE_QRF_DELAY_MAX_MS = 40000;
    private bool m_bAirbornePending = false;
    private vector m_airborneTarget = vector.Zero;
    private IA_AreaInstance m_airborneArea;
    private Faction m_airborneFaction;
    private bool m_airborneDefend = false;
    private bool m_airborneHotDrop = false;
    private ref array<vector> m_recentDropLzs = new array<vector>();

    // Entry point called periodically (from MissionInitializer) to evaluate spawning of QRFs for the whole area group
    void Shutdown()
    {
        m_bShutDown = true;
        m_qrfRetryPending = false;
        m_bAirbornePending = false;
        if (m_recentDropLzs)
            m_recentDropLzs.Clear();
        ScriptCallQueue queue = GetGame().GetCallqueue();
        if (queue)
        {
            queue.Remove(OnQRFRetry);
            queue.Remove(OnAirborneQRFSpawn);
            queue.Remove(OnAirborneQRFSpawnRetry);
            queue.Remove(QRF_PollTruckArrival);
            queue.Remove(QRF_AddDefendAfterDisembark);
        }
    }

    void QRFTask()
    {
        if (m_bShutDown)
            return;
        if (!Replication.IsServer())
            return; // QRF is server-authoritative only

        IA_DefendMission defend = GetActiveDefendMission();
        if (defend && defend.IsEnhanced())
            return;

        bool forDefend = false;
        if (defend)
            forDefend = true;

        int currentTime = System.GetUnixTime();
        if (!forDefend && IA_MissionInitializer.IsQRFDisabled())
            return; // QRF globally disabled (legacy defend holds still get QRF)

        if (!forDefend && IA_GmDirector.IsAutoQrfOff())
            return;

        int checkInterval = QRF_CHECK_INTERVAL;
        int cooldown = QRF_COOLDOWN;
        float chance = QRF_CHANCE;
        if (forDefend)
        {
            checkInterval = DEFEND_QRF_CHECK_INTERVAL;
            cooldown = DEFEND_QRF_COOLDOWN;
            chance = DEFEND_QRF_CHANCE;
        }

        if (currentTime - m_lastQRFCheckTime < checkInterval)
            return;
        m_lastQRFCheckTime = currentTime;
        Print(string.Format("[QRF] Running for group with %1 areas.", m_areaInstances.Count()), LogLevel.NORMAL);

        IA_AreaInstance closestArea = null;
        vector finalTarget = vector.Zero;
        if (forDefend)
        {
            closestArea = defend.GetHostArea();
            finalTarget = defend.GetDefendPoint();
            if (!closestArea || closestArea.IsShutDown() || finalTarget == vector.Zero)
            {
                Print("[QRF] Defend QRF aborted: no live host area or defend point.", LogLevel.WARNING);
                return;
            }
        }
        else
        {
            bool groupUnderAttack = false;
            foreach (IA_AreaInstance instance : m_areaInstances)
            {
                if (instance && !instance.IsShutDown() && instance.IsUnderAttack())
                {
                    groupUnderAttack = true;
                    break;
                }
            }
            if (!groupUnderAttack)
            {
                Print("[QRF] Check failed: No area in the group is under attack.", LogLevel.NORMAL);
                return;
            }

            vector targetPos;
            bool hasTarget = ComputeGroupThreatTarget(targetPos);
            if (!hasTarget || targetPos == vector.Zero)
            {
                Print("[QRF] Aborted: No valid recent danger events to target.", LogLevel.NORMAL);
                return;
            }

            finalTarget = ResolveClosestAreaTarget(targetPos, closestArea);
            if (finalTarget == vector.Zero || !closestArea)
            {
                Print("[QRF] Failed to resolve closest area target; aborting QRF.", LogLevel.WARNING);
                return;
            }
        }

        if (currentTime - m_lastQRFTime < cooldown)
        {
            int remaining = cooldown - (currentTime - m_lastQRFTime);
            Print(string.Format("[QRF] Global cooldown active: %1s remaining.", remaining), LogLevel.NORMAL);
            return;
        }

        float roll = IA_Game.rng.RandFloat01();
        if (roll > chance)
        {
            Print(string.Format("[QRF] Global chance failed (roll %1 > %2).", roll, chance), LogLevel.NORMAL);
            return;
        }

        IA_QRFType selectedType = SelectQRFType(forDefend);

        Print(string.Format("[QRF] Selected %1; closest area '%2'; final target %3. Attempting spawn...",
            QRFTypeToString(selectedType), closestArea.GetArea().GetName(), finalTarget.ToString()), LogLevel.NORMAL);

        bool spawned = SpawnQRFForTarget(selectedType, finalTarget, closestArea, null, forDefend, false);
        if (spawned)
        {
            m_lastQRFTime = currentTime;
        }
    }

    private IA_DefendMission GetActiveDefendMission()
    {
        IA_Game game = IA_Game.Instantiate();
        if (!game)
            return null;

        IA_DefendMission defend = game.GetActiveDefendMission();
        if (!defend || !defend.IsActive())
            return null;
        return defend;
    }

    //! Capture QRF: 20% airborne. Defend QRF: 25% airborne.
    //! Remaining weight keeps non-airborne relative shares (mech 3, motorized 2, infantry 1, armour 1).
    private IA_QRFType SelectQRFType(bool forDefend)
    {
        float airborneChance = AIRBORNE_CHANCE;
        if (forDefend)
            airborneChance = DEFEND_AIRBORNE_CHANCE;

        if (IA_Game.rng.RandFloat01() < airborneChance)
            return IA_QRFType.Airborne;

        int idx = Math.RandomInt(0, 7);
        switch (idx)
        {
            case 0: return IA_QRFType.Infantry;
            case 1: return IA_QRFType.Armoured;
            case 2:
            case 3: return IA_QRFType.Motorized;
        }
        return IA_QRFType.Mechanized;
    }

    private string QRFTypeToString(IA_QRFType type)
    {
        switch (type)
        {
            case IA_QRFType.Infantry:   return "Infantry QRF";
            case IA_QRFType.Motorized:  return "Motorized QRF";
            case IA_QRFType.Mechanized: return "Mechanized QRF";
            case IA_QRFType.Armoured:   return "Armoured QRF";
            case IA_QRFType.Airborne:   return "Airborne QRF";
        }
        return "QRF";
    }

    bool ForceSpawnQRF(IA_QRFType type)
    {
        if (m_bShutDown)
            return false;
        if (!Replication.IsServer())
            return false;
        if (!m_areaInstances || m_areaInstances.IsEmpty())
        {
            Print("[IA][Admin] Force QRF failed: no area instances", LogLevel.WARNING);
            return false;
        }

        IA_AreaInstance closestArea = null;
        vector targetPos = vector.Zero;
        if (!ComputeGroupThreatTarget(targetPos) || targetPos == vector.Zero)
        {
            int i;
            int count = m_areaInstances.Count();
            for (i = 0; i < count; i++)
            {
                IA_AreaInstance inst = m_areaInstances[i];
                if (!inst || inst.IsShutDown() || !inst.GetArea())
                    continue;
                targetPos = inst.GetArea().GetOrigin();
                closestArea = inst;
                break;
            }
        }

        if (targetPos == vector.Zero)
        {
            Print("[IA][Admin] Force QRF failed: no target area", LogLevel.WARNING);
            return false;
        }

        if (!closestArea)
            targetPos = ResolveClosestAreaTarget(targetPos, closestArea);
        if (!closestArea)
        {
            Print("[IA][Admin] Force QRF failed: could not resolve area", LogLevel.WARNING);
            return false;
        }

        bool spawned = SpawnQRFForTarget(type, targetPos, closestArea, null, false, false);
        if (spawned)
            m_lastQRFTime = System.GetUnixTime();
        else
            Print("[IA][Admin] Force QRF spawn failed for " + QRFTypeToString(type), LogLevel.WARNING);
        return spawned;
    }

    bool ForceSpawnQRFAt(IA_QRFType type, vector pos)
    {
        if (m_bShutDown)
            return false;
        if (!Replication.IsServer())
            return false;
        if (!m_areaInstances || m_areaInstances.IsEmpty())
        {
            Print("[IA][Admin] Force QRF at point failed: no area instances", LogLevel.WARNING);
            return false;
        }

        IA_AreaInstance closestArea = null;
        vector resolved = ResolveClosestAreaTarget(pos, closestArea);
        if (!closestArea)
        {
            Print("[IA][Admin] Force QRF at point failed: could not resolve area", LogLevel.WARNING);
            return false;
        }

        vector targetPos = pos;
        if (targetPos == vector.Zero)
            targetPos = resolved;

        bool spawned = SpawnQRFForTarget(type, targetPos, closestArea, null, false, false);
        if (spawned)
            m_lastQRFTime = System.GetUnixTime();
        else
            Print("[IA][Admin] Force QRF at point failed for " + QRFTypeToString(type), LogLevel.WARNING);
        return spawned;
    }

    void AddInstance(IA_AreaInstance inst)
    {
        if (!inst)
            return;
        if (!m_areaInstances)
            m_areaInstances = new array<ref IA_AreaInstance>();
        if (m_areaInstances.Find(inst) == -1)
            m_areaInstances.Insert(inst);
    }

    //! One mid-hold QRF pulse for Defend missions. Uses defend type weights (25% airborne).
    bool SpawnDefendVehicleBeat(IA_AreaInstance areaInst, vector defendPoint, Faction enemyFaction)
    {
        if (!Replication.IsServer())
            return false;
        if (!areaInst)
            return false;

        if (!enemyFaction)
        {
            IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
            if (initializer)
                enemyFaction = initializer.GetRandomEnemyFaction();
        }
        if (!enemyFaction)
        {
            Print("[QRF] Defend vehicle beat aborted: no enemy faction.", LogLevel.WARNING);
            return false;
        }

        IA_QRFType type = SelectQRFType(true);

        Print(string.Format("[QRF] Defend mid-hold beat selected %1 toward %2",
            QRFTypeToString(type), defendPoint.ToString()), LogLevel.NORMAL);

        return SpawnQRFForTarget(type, defendPoint, areaInst, enemyFaction, true, false);
    }

    //! Enhanced Defense: fire a specific QRF type through the existing defend path.
    bool SpawnDefendDoctrineBeat(IA_QRFType type, IA_AreaInstance areaInst, vector defendPoint, Faction enemyFaction)
    {
        if (!Replication.IsServer())
            return false;
        if (!areaInst)
            return false;
        if (!enemyFaction)
            return false;
        return SpawnQRFForTarget(type, defendPoint, areaInst, enemyFaction, true, false);
    }

    //! One truck + crew/cargo at a chosen road site, driving to the defend point.
    IA_AiGroup SpawnDefendConvoyTruck(IA_AreaInstance areaInst, vector spawnPos, vector defendPoint, Faction enemyFaction)
    {
        if (!Replication.IsServer())
            return null;
        if (!areaInst || areaInst.IsShutDown())
            return null;
        if (!enemyFaction)
            return null;

        vector site = spawnPos;
        if (site == vector.Zero)
            site = IA_SpawnPlacement.FindInboundVehicleSpawn(defendPoint, IA_VehicleManager.GetActiveGroup(), -1);
        if (site == vector.Zero)
            return null;

        Vehicle truck = null;
        int attempt;
        for (attempt = 0; attempt < 6; attempt++)
        {
            Vehicle candidate = IA_VehicleManager.SpawnRandomVehicle(IA_Faction.USSR, false, true, site, enemyFaction);
            if (!candidate)
                continue;
            if (DoesVehicleMatchQRFType(candidate, false, true, false))
            {
                truck = candidate;
                break;
            }
            IA_VehicleManager.DespawnVehicle(candidate);
        }

        if (!truck)
            truck = IA_VehicleManager.SpawnRandomVehicle(IA_Faction.USSR, false, true, site, enemyFaction);
        if (!truck)
            return null;

        vector driveTarget = IA_VehicleManager.FindRoadInAnnulus(defendPoint, 80, 150, IA_VehicleManager.GetActiveGroup());
        if (driveTarget == vector.Zero)
            driveTarget = defendPoint;

        IA_AiGroup crew = IA_VehicleManager.PlaceUnitsInVehicle(truck, IA_Faction.USSR, driveTarget, areaInst, enemyFaction);
        if (!crew)
            return null;

        crew.EnableInboundSimulation(defendPoint);
        crew.SetAssignedArea(areaInst.GetArea());
        crew.EnableDefendModeTracking(true, defendPoint);
        GetGame().GetCallqueue().CallLater(this.QRF_PollTruckArrival, 3000, false, truck, crew, areaInst, defendPoint, driveTarget);
        return crew;
    }

    //! Enhanced Defense: schedule an airborne drop, optionally preferring a hot LZ.
    bool SpawnDefendAirborneDrop(IA_AreaInstance areaInst, vector defendPoint, Faction enemyFaction, bool preferHotDrop)
    {
        if (!Replication.IsServer())
            return false;
        if (!areaInst)
            return false;
        if (!enemyFaction)
            return false;
        m_airborneHotDrop = preferHotDrop;
        return ScheduleAirborneQRF(areaInst, enemyFaction, defendPoint, true);
    }

    // Determine the target position for QRF using the same logic and constraints as the artillery system
    private bool ComputeGroupThreatTarget(out vector outTarget)
    {
        outTarget = vector.Zero;
        int currentTime = System.GetUnixTime();

        // Calculate group center for distance clamping
        vector groupCenter = vector.Zero;
        if (!m_areaInstances || m_areaInstances.IsEmpty())
            return false;

        vector totalPos = vector.Zero;
        int count = 0;
        foreach (IA_AreaInstance inst : m_areaInstances)
        {
            if (inst && inst.GetArea())
            {
                totalPos += inst.GetArea().GetOrigin();
                count++;
            }
        }
        if (count > 0)
        {
            groupCenter = totalPos / count;
            Print(string.Format("[QRF] Calculated area group center: %1", groupCenter), LogLevel.NORMAL);
        }
        else
        {
            Print("[QRF] Could not calculate area group center, no valid areas found.", LogLevel.WARNING);
            return false;
        }

        array<vector> relevantPositions = {};
        const float MAX_DANGER_EVENT_DISTANCE = 1600.0;

        foreach (IA_AreaInstance instance : m_areaInstances)
        {
            if (!instance) continue;

            array<ref IA_AiGroup> military = instance.GetMilitaryGroups();
            foreach (IA_AiGroup group : military)
            {
                int timeSinceLastDanger = currentTime - group.GetLastDangerEventTime();
                if (group && group.GetLastDangerEventTime() > 0 && timeSinceLastDanger < 90)
                {
                    vector currentDangerPos = group.GetLastDangerPosition();
                    if (currentDangerPos != vector.Zero)
                    {
                        if (vector.DistanceSq(currentDangerPos, groupCenter) <= (MAX_DANGER_EVENT_DISTANCE * MAX_DANGER_EVENT_DISTANCE))
                        {
                            relevantPositions.Insert(currentDangerPos);
                        }
                        else
                        {
                            Print(string.Format("[QRF] Discarded danger event at %1, too far from group center %2 (Distance: %3m, Max: %4m)", currentDangerPos, groupCenter, vector.Distance(currentDangerPos, groupCenter), MAX_DANGER_EVENT_DISTANCE), LogLevel.NORMAL);
                        }
                    }
                }
            }
        }

        if (relevantPositions.IsEmpty())
        {
            Print("[QRF] No recent danger events found across area group.", LogLevel.NORMAL);
            return false;
        }

        // Median selection by X as in artillery, then apply small randomization like artillery
        IA_VectorUtils.SortVectorsByX(relevantPositions);
        int medianIndex = relevantPositions.Count() / 2;
        vector primaryThreatLocation = relevantPositions[medianIndex];
        outTarget = IA_Game.rng.GenerateRandomPointInRadius(4, 30, primaryThreatLocation);
        outTarget[1] = GetGame().GetWorld().GetSurfaceY(outTarget[0], outTarget[2]);
        Print(string.Format("[QRF] Target determined (Median+jitter): %1 for area group.", outTarget), LogLevel.NORMAL);
        return true;
    }

    private bool SpawnQRF(IA_QRFType type, vector targetPos)
    {
        IA_AreaInstance targetAreaInst = null;
        ResolveClosestAreaTarget(targetPos, targetAreaInst);
        if (!targetAreaInst)
            return false;

        return SpawnQRFForTarget(type, targetPos, targetAreaInst, null, false, false);
    }

    private bool SpawnQRFForTarget(IA_QRFType type, vector targetPos, IA_AreaInstance targetAreaInst, Faction enemyGameFaction, bool forDefendMission, bool isRetry)
    {
        if (m_bShutDown)
            return false;
        if (targetAreaInst && targetAreaInst.IsShutDown())
            return false;
        if (!Replication.IsServer())
            return false;
        if (!targetAreaInst)
            return false;
        if (targetAreaInst.GetOwningFaction() == IA_Faction.US)
        {
            Print("[QRF] Spawn skipped: target area already captured.", LogLevel.NORMAL);
            return false;
        }

        if (!enemyGameFaction)
        {
            IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
            if (!initializer)
                return false;
            enemyGameFaction = initializer.GetRandomEnemyFaction();
        }
        if (!enemyGameFaction)
            return false;

        string areaName = targetAreaInst.GetArea().GetName();

        // Spawn relative to the defend/fight point when provided, else area origin
        vector spawnCenter = targetPos;
        if (spawnCenter == vector.Zero)
            spawnCenter = targetAreaInst.GetArea().GetOrigin();

        int activeGroup = IA_VehicleManager.GetActiveGroup();
        vector infAnchor = IA_SpawnPlacement.FindInboundInfantrySpawn(spawnCenter, -1);
        vector vehAnchor = IA_SpawnPlacement.FindInboundVehicleSpawn(spawnCenter, activeGroup, -1);

        bool success = false;
        switch (type)
        {
            case IA_QRFType.Infantry:
            {
                bool s1 = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(infAnchor, 0), forDefendMission);
                bool s2 = false;
                if (IA_Game.GetAIScaleFactor() >= 1.0)
                    s2 = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(infAnchor, 1), forDefendMission);
                success = (s1 || s2);
                break;
            }
            case IA_QRFType.Motorized:
            {
                bool v = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, false, true, false, ComputeClusterPos(vehAnchor, 0), forDefendMission);
                bool inf = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(infAnchor, 1), forDefendMission);
                success = (v || inf);
                break;
            }
            case IA_QRFType.Mechanized:
            {
                bool apc = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, true, false, false, ComputeClusterPos(vehAnchor, 0), forDefendMission);
                bool truck = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, false, true, false, ComputeClusterPos(vehAnchor, 1), forDefendMission);
                success = (apc || truck);
                break;
            }
            case IA_QRFType.Armoured:
            {
                bool a1 = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, true, false, true, ComputeClusterPos(vehAnchor, 0), forDefendMission);
                bool a2 = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, true, false, true, ComputeClusterPos(vehAnchor, 1), forDefendMission);
                bool inf = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(infAnchor, 2), forDefendMission);
                success = (a1 || a2 || inf);
                break;
            }
            case IA_QRFType.Airborne:
            {
                success = ScheduleAirborneQRF(targetAreaInst, enemyGameFaction, targetPos, forDefendMission);
                break;
            }
        }

        if (success)
        {
            string notif;
            if (forDefendMission)
                notif = QRFTypeToString(type) + " counterattack inbound!";
            else
                notif = QRFTypeToString(type) + " inbound at " + areaName + "!";
            IA_Game.S_TriggerGlobalNotification("ReinforcementsCalled", notif);
            Print(string.Format("[QRF] %1 spawned towards %2 at %3 (defendBeat=%4)",
                QRFTypeToString(type), areaName, targetPos.ToString(), forDefendMission), LogLevel.NORMAL);
        }
        else
        {
            Print(string.Format("[QRF] Spawn failed for %1 at area %2 (target %3)", QRFTypeToString(type), areaName, targetPos.ToString()), LogLevel.WARNING);
            if (!isRetry && !m_qrfRetryPending)
            {
                m_qrfRetryPending = true;
                m_qrfRetryType = type;
                m_qrfRetryTarget = targetPos;
                m_qrfRetryArea = targetAreaInst;
                m_qrfRetryFaction = enemyGameFaction;
                m_qrfRetryDefend = forDefendMission;
                GetGame().GetCallqueue().CallLater(this.OnQRFRetry, 15000, false);
            }
        }
        return success;
    }

    void OnQRFRetry()
    {
        m_qrfRetryPending = false;
        if (m_bShutDown)
            return;
        if (!m_qrfRetryArea || m_qrfRetryArea.IsShutDown())
            return;
        SpawnQRFForTarget(m_qrfRetryType, m_qrfRetryTarget, m_qrfRetryArea, m_qrfRetryFaction, m_qrfRetryDefend, true);
    }

    private bool SpawnInfantryQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, vector preferredSpawn, bool forDefendMission = false)
    {
        float scale = IA_Game.GetAIScaleFactor();
        int unitCount = Math.Clamp(Math.Round(6 * scale), 4, 10);

        vector spawnPos = preferredSpawn;
        bool preferredUsable = true;
        if (spawnPos == vector.Zero)
            preferredUsable = false;
        else if (vector.Distance(spawnPos, vector.Zero) < 50)
            preferredUsable = false;

        if (!preferredUsable)
            spawnPos = IA_SpawnPlacement.FindInboundInfantrySpawn(areaInst.GetArea().GetOrigin(), -1);

        if (spawnPos == vector.Zero)
        {
            Print(string.Format("[QRF] Infantry miss: no inbound spawn near %1.", areaInst.GetArea().GetOrigin()), LogLevel.WARNING);
            return false;
        }

        IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(spawnPos, IA_Faction.USSR, unitCount, enemyGameFaction, false, true);
        if (!grp)
            return false;

            grp.SetAssignedArea(areaInst.GetArea());
            grp.Spawn();
            if (forDefendMission)
            {
                grp.SetDefendWaveGroup(true);
                grp.SetDefendMode(true, targetPos);
            }
            bool hunter = false;
            if (forDefendMission && IA_Game.rng.RandFloat01() < 0.30)
            {
                hunter = true;
                grp.SetDefendHunter(true);
            }
            grp.SetTacticalState(IA_GroupTacticalState.Attacking, targetPos, null, true);
        // If no waypoint exists yet, add one explicitly
        if (!grp.HasActiveWaypoint())
        {
            ResourceName sadRes = "{EE9A99488B40628B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_SearchAndDestroy.et";
            Resource res = Resource.Load(sadRes);
            if (res)
            {
                EntitySpawnParams p = EntitySpawnParams();
                p.TransformMode = ETransformMode.WORLD;
                p.Transform[3] = targetPos;
                IEntity ent = GetGame().SpawnEntityPrefab(res, null, p);
                SCR_AIWaypoint w = SCR_AIWaypoint.Cast(ent);
                if (w)
                {
                    w.SetPriorityLevel(IA_AiGroup.WP_PRIORITY_FIGHT);
                    grp.AddWaypoint(w);
                }
            }
        }
        areaInst.AddMilitaryGroup(grp);
        grp.EnableInboundSimulation(targetPos);
        if (!hunter)
            IA_LockGroupToSearchAndDestroy(areaInst, grp, targetPos);
        return true;
    }

    private bool ScheduleAirborneQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, bool forDefendMission)
    {
        if (m_bAirbornePending)
        {
            Print("[QRF] Airborne miss: drop already pending.", LogLevel.WARNING);
            return false;
        }
        if (!areaInst || !enemyGameFaction)
            return false;

        m_bAirbornePending = true;
        m_airborneArea = areaInst;
        m_airborneFaction = enemyGameFaction;
        m_airborneTarget = targetPos;
        m_airborneDefend = forDefendMission;

        int span = AIRBORNE_QRF_DELAY_MAX_MS - AIRBORNE_QRF_DELAY_MIN_MS;
        if (span < 0)
            span = 0;
        int delayMs = AIRBORNE_QRF_DELAY_MIN_MS;
        if (IA_Game.rng)
            delayMs = delayMs + IA_Game.rng.RandInt(0, span + 1);
        else
            delayMs = delayMs + Math.RandomInt(0, span + 1);

        GetGame().GetCallqueue().CallLater(this.OnAirborneQRFSpawn, delayMs, false);
        Print(string.Format("[QRF] Airborne inbound, drop in %1s.", (delayMs / 1000).ToString()), LogLevel.NORMAL);
        return true;
    }

    void OnAirborneQRFSpawn()
    {
        m_bAirbornePending = false;
        if (m_bShutDown)
            return;
        if (!m_airborneArea || m_airborneArea.IsShutDown())
            return;
        if (!SpawnAirborneQRF(m_airborneArea, m_airborneFaction, m_airborneTarget, m_airborneDefend))
        {
            Print("[QRF] Airborne drop failed after inbound warning; retrying in 15s.", LogLevel.WARNING);
            GetGame().GetCallqueue().CallLater(this.OnAirborneQRFSpawnRetry, 15000, false);
        }
    }

    void OnAirborneQRFSpawnRetry()
    {
        if (m_bShutDown)
            return;
        if (!m_airborneArea || m_airborneArea.IsShutDown())
            return;
        SpawnAirborneQRF(m_airborneArea, m_airborneFaction, m_airborneTarget, m_airborneDefend);
    }

    private bool SpawnAirborneQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, bool forDefendMission = false)
    {
        if (!areaInst || !enemyGameFaction)
            return false;

        int jumperCount = IA_Game.GetAirborneQRFJumperCount();

        vector attackTarget = targetPos;
        if (attackTarget == vector.Zero)
            attackTarget = areaInst.GetArea().GetOrigin();

        vector dropLz;
        bool foundLz = false;
        if (forDefendMission)
            foundLz = IA_SpawnPlacement.TryFindDefendDropLz(attackTarget, m_airborneHotDrop, dropLz, m_recentDropLzs);
        if (!foundLz)
            foundLz = IA_SpawnPlacement.TryFindDropLz(attackTarget, IA_SpawnPlacement.DROP_LZ_SEARCH_R, dropLz);
        if (!foundLz)
            foundLz = IA_SpawnPlacement.TryFindDropLz(attackTarget, IA_SpawnPlacement.DROP_LZ_SEARCH_WIDE_R, dropLz);
        if (!foundLz)
        {
            Print("[QRF] Airborne miss: no open-sky LZ.", LogLevel.WARNING);
            return false;
        }

        vector release = dropLz;
        vector wind = MHJ_FlightAero.WindWorld(dropLz[1] + MHJ_Constants.AI_DROP_AGL, 0);
        wind[1] = 0;
        if (wind.Length() > 0.2)
        {
            vector upwind = wind * -1;
            upwind.Normalize();
            release = release + upwind * 120;
        }
        release[1] = dropLz[1] + MHJ_Constants.AI_DROP_AGL;

        MHJ_AiDropDirector director = MHJ_AiDropDirector.SpawnStick(dropLz);
        if (!director)
        {
            Print("[QRF] Airborne miss: drop stick refused or missing.", LogLevel.WARNING);
            return false;
        }

        RememberDropLz(dropLz);
        Print(string.Format("[QRF] Airborne LZ %1 attack %2 dist=%3 hot=%4",
            dropLz.ToString(), attackTarget.ToString(), vector.Distance(dropLz, attackTarget), m_airborneHotDrop), LogLevel.NORMAL);

        int remaining = jumperCount;
        int teamIndex = 0;
        bool spawnedAny = false;
        int teamCount = 0;
        int countRemain = jumperCount;
        while (countRemain > 0)
        {
            int previewSize = 5;
            if (countRemain <= 6)
                previewSize = countRemain;
            if (previewSize < 1)
                break;
            teamCount = teamCount + 1;
            countRemain = countRemain - previewSize;
        }

        ref array<int> hunterSlots = new array<int>();
        if (forDefendMission)
            areaInst.PickDefendHunterSlots(teamCount, hunterSlots);

        while (remaining > 0)
        {
            int teamSize = 5;
            if (remaining <= 6)
                teamSize = remaining;
            if (teamSize < 1)
                break;

            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(release, IA_Faction.USSR, teamSize, enemyGameFaction, false, true, true);
            if (!grp)
            {
                remaining = remaining - teamSize;
                continue;
            }

            grp.SetAssignedArea(areaInst.GetArea());
            if (forDefendMission)
                grp.SetDefendMode(true, attackTarget);
            if (forDefendMission && hunterSlots.Find(teamIndex) != -1)
            {
                grp.SetDefendHunter(true);
                grp.SetDefendWaveGroup(true);
            }
            grp.BeginAirborneDrop(director, dropLz, attackTarget, areaInst, 250, 8, 28);
            areaInst.AddMilitaryGroup(grp);

            int delayMs = teamIndex * 150;
            if (delayMs <= 0)
                grp.SpawnNextUnit();
            else
                GetGame().GetCallqueue().CallLater(grp.SpawnNextUnit, delayMs, false);

            spawnedAny = true;
            remaining = remaining - teamSize;
            teamIndex = teamIndex + 1;
        }

        if (!spawnedAny)
            Print("[QRF] Airborne miss: no fireteams created.", LogLevel.WARNING);
        return spawnedAny;
    }

    private void RememberDropLz(vector lz)
    {
        if (lz == vector.Zero)
            return;
        if (!m_recentDropLzs)
            m_recentDropLzs = new array<vector>();
        m_recentDropLzs.Insert(lz);
        while (m_recentDropLzs.Count() > 6)
        {
            m_recentDropLzs.Remove(0);
        }
    }

    private bool SpawnVehicleQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, bool preferAPC, bool allowTrucks, bool armourOnly, vector preferredSpawn, bool forDefendMission = false)
    {
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        vector spawnPos = preferredSpawn;
        bool preferredUsable = true;
        if (spawnPos == vector.Zero)
            preferredUsable = false;
        else if (vector.Distance(spawnPos, vector.Zero) < 50)
            preferredUsable = false;

        if (!preferredUsable)
            spawnPos = IA_SpawnPlacement.FindInboundVehicleSpawn(areaInst.GetArea().GetOrigin(), activeGroup, -1);

        if (spawnPos == vector.Zero)
        {
            Print(string.Format("[QRF] Vehicle miss: no inbound road near %1.", areaInst.GetArea().GetOrigin()), LogLevel.WARNING);
            return false;
        }

        // Try to spawn a matching vehicle using the manager to ensure proper tracking
        Vehicle selectedVehicle = null;

        const int MAX_ATTEMPTS = 6;
        for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
        {
            Vehicle v = IA_VehicleManager.SpawnRandomVehicle(IA_Faction.USSR, false, true, spawnPos, enemyGameFaction);
            if (!v) continue;

            if (DoesVehicleMatchQRFType(v, preferAPC, allowTrucks, armourOnly))
            {
                selectedVehicle = v;
                break;
            }
            else
            {
                // Not desired type; remove and try again
                IA_VehicleManager.DespawnVehicle(v);
            }
        }

        if (!selectedVehicle)
        {
            Print("[QRF] Failed to spawn a vehicle matching desired category.", LogLevel.WARNING);
            return false;
        }

        // Destination: road near the fight/defend point (not just the parent area origin)
        int driveGroup = IA_VehicleManager.GetActiveGroup();
        vector objectivePos = targetPos;
        if (objectivePos == vector.Zero)
            objectivePos = areaInst.GetArea().GetOrigin();
        vector driveTarget = IA_VehicleManager.FindRandomRoadPointForVehiclePatrol(objectivePos, 400, driveGroup);
        if (driveTarget == vector.Zero)
            driveTarget = objectivePos;

        IA_AiGroup vehicleGroup = IA_VehicleManager.PlaceUnitsInVehicle(selectedVehicle, IA_Faction.USSR, driveTarget, areaInst, enemyGameFaction);
        if (!vehicleGroup)
        {
            Print("[QRF] Failed to create AI group for vehicle QRF.", LogLevel.WARNING);
            return false;
        }

        vehicleGroup.EnableInboundSimulation(objectivePos);

        // Ensure assigned area for proper integration
        vehicleGroup.SetAssignedArea(areaInst.GetArea());
        // Track as defend-mode without wiping vehicle drive orders
        if (forDefendMission)
            vehicleGroup.EnableDefendModeTracking(true, objectivePos);

        // Crew keeps the drive Move at WP_PRIORITY_DRIVE. Cargo is a sibling
        // group that dumps on arrival or close contact.
        GetGame().GetCallqueue().CallLater(this.QRF_PollTruckArrival, 3000, false, selectedVehicle, vehicleGroup, areaInst, objectivePos, driveTarget);
        return true;
    }

    private vector ComputeClusterPos(vector anchor, int index)
    {
        // Small offsets forming a tight cluster
        const float OFF = 8.0;
        switch (index % 6)
        {
            case 0: return anchor + Vector(0, 0, 0);
            case 1: return anchor + Vector(OFF, 0, 0);
            case 2: return anchor + Vector(-OFF, 0, 0);
            case 3: return anchor + Vector(0, 0, OFF);
            case 4: return anchor + Vector(0, 0, -OFF);
            default: return anchor + Vector(OFF, 0, OFF);
        }
        return anchor; // Fallback return for compiler
    }

    // Helper: find the closest area's origin to a given position within this group
    private vector ResolveClosestAreaTarget(vector position, out IA_AreaInstance outArea)
    {
        outArea = null;
        float bestDistSq = 3.4e38; // large
        vector best = vector.Zero;
        foreach (IA_AreaInstance inst : m_areaInstances)
        {
            if (!inst || !inst.GetArea()) continue;
            if (inst.IsShutDown()) continue;
            vector origin = inst.GetArea().GetOrigin();
            float dSq = vector.DistanceSq(origin, position);
            if (dSq < bestDistSq)
            {
                bestDistSq = dSq;
                best = origin;
                outArea = inst;
            }
        }
        return best;
    }

    // Helper: register a group as a locked Search & Destroy reinforcement on the area instance
    private void IA_LockGroupToSearchAndDestroy(IA_AreaInstance areaInst, IA_AiGroup group, vector target)
    {
        if (!areaInst || !group) return;
        // Ensure they have the S&D order at the exact target
        group.RemoveAllOrders(true);
        group.AddOrder(target, IA_AiOrder.SearchAndDestroy, true);
        group.SetTacticalState(IA_GroupTacticalState.Attacking, target, null, true);
        // Register with the area instance forced S&D tracking so orders are not removed by normal logic
        areaInst.RegisterForcedReinforcementSND(group, target, true);
    }

    private bool DoesVehicleMatchQRFType(Vehicle vehicle, bool preferAPC, bool allowTrucks, bool armourOnly)
    {
        if (!vehicle) return false;

        bool isTruck = false; bool isAPC = false; bool isArmored = false;
        DetermineVehicleClass(vehicle, isTruck, isAPC, isArmored);

        if (armourOnly)
        {
            return isAPC || isArmored; // Armoured: APCs or armor-labelled vehicles
        }

        if (preferAPC)
        {
            // Mechanized: prefer APC, but allow trucks if APC not present
            if (isAPC) return true;
            if (allowTrucks && isTruck) return true;
            return false;
        }

        // Motorized: trucks only, explicitly not APC
        if (allowTrucks && isTruck && !isAPC)
            return true;

        return false;
    }

    // Determine vehicle class flags from its catalog labels
    private void DetermineVehicleClass(Vehicle vehicle, out bool isTruck, out bool isAPC, out bool isArmored)
    {
        isTruck = false; isAPC = false; isArmored = false;
        if (!vehicle) return;
        SCR_EditableVehicleComponent editableVehicle = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
        if (!editableVehicle) return;
        string prefabPath = editableVehicle.GetPrefab();
        if (prefabPath == string.Empty) return;

        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) return;
        array<Faction> allFactions = {};
        factionManager.GetFactionsList(allFactions);

        array<EEditableEntityLabel> vehicleLabels = {};
        foreach (Faction f : allFactions)
        {
            SCR_Faction scrFaction = SCR_Faction.Cast(f);
            if (!scrFaction) continue;
            SCR_EntityCatalog cat = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
            if (!cat) continue;
            SCR_EntityCatalogEntry entry = cat.GetEntryWithPrefab(prefabPath);
            if (entry)
            {
                entry.GetEditableEntityLabels(vehicleLabels);
                break;
            }
        }

        foreach (EEditableEntityLabel label : vehicleLabels)
        {
            if (label == EEditableEntityLabel.VEHICLE_TRUCK)
                isTruck = true;
            if (label == EEditableEntityLabel.VEHICLE_APC)
                isAPC = true;
            if (label == EEditableEntityLabel.TRAIT_ARMOR)
                isArmored = true;
        }
    }

    // Poller to detect truck arrival at driveTarget, then order dismount and later defend
    private void QRF_PollTruckArrival(Vehicle vehicle, IA_AiGroup group, IA_AreaInstance areaInst, vector areaOrigin, vector driveTarget)
    {
        if (m_bShutDown)
            return;
        if (!vehicle || !group || !areaInst || areaInst.IsShutDown())
            return;
        // If arrived, dismount and schedule defend
        if (IA_VehicleManager.HasVehicleReachedDestination(vehicle, driveTarget))
        {
            IA_AiGroup passengers = group.GetLinkedPassengerGroup();
            if (passengers)
            {
                passengers.DumpPassengersAndAssault(areaOrigin);
                return;
            }

            group.RemoveAllOrders(false);
            group.AddOrder(areaOrigin, IA_AiOrder.GetOutOfVehicle, true);
            GetGame().GetCallqueue().CallLater(this.QRF_AddDefendAfterDisembark, 30000, false, group, areaOrigin);
            return;
        }
        // Not yet; poll again in 3s
        GetGame().GetCallqueue().CallLater(this.QRF_PollTruckArrival, 3000, false, vehicle, group, areaInst, areaOrigin, driveTarget);
    }

    private void QRF_AddDefendAfterDisembark(IA_AiGroup group, vector defendPos)
    {
        if (m_bShutDown || !group)
            return;
        if (group.IsInDefendMode())
        {
            group.AddOrder(defendPos, IA_AiOrder.SearchAndDestroy, true);
            group.SetTacticalState(IA_GroupTacticalState.Attacking, defendPos, null, true);
        }
        else
        {
            group.AddOrder(defendPos, IA_AiOrder.Defend, true);
            group.SetTacticalState(IA_GroupTacticalState.Defending, defendPos, null, true);
        }
    }

    void ArtilleryStrikeTask()
    {
        if (m_bShutDown)
            return;
        if (IA_GmDirector.IsAutoArtyOff())
        {
            ClearPendingArtilleryStrike();
            return;
        }

        int currentTime = System.GetUnixTime();
        float strikeChance = ARTILLERY_STRIKE_CHANCE;
        int cooldown = ARTILLERY_COOLDOWN;
        int minDelay = 45;
        int maxDelay = 70;

        IA_Config config = IA_MissionInitializer.GetGlobalConfig();
        if (config)
        {
            strikeChance = config.m_fArtilleryStrikeChance;
            cooldown = config.m_iArtilleryCooldown;
            minDelay = config.m_iArtilleryMinDelay;
            maxDelay = config.m_iArtilleryMaxDelay;
        }

        IA_AreaInstance mortarPit = FindMortarPitInstance();
        if (mortarPit && mortarPit.CanIssueMortarFireMission())
        {
            vector defensePos;
            if (mortarPit.GetMortarPitDefenseTarget(defensePos))
            {
                if (currentTime - m_lastPitDefenseFireTime >= ARTILLERY_PIT_DEFENSE_COOLDOWN)
                {
                    int defenseShots = Math.RandomInt(ARTILLERY_MIN_SHOTS, ARTILLERY_MAX_SHOTS + 1);
                    if (mortarPit.IssueMortarFireMission(defensePos, defenseShots))
                    {
                        m_lastPitDefenseFireTime = currentTime;
                        Print(string.Format("[ArtilleryStrike] Pit defense fire: %1 rounds at %2", defenseShots, defensePos), LogLevel.NORMAL);
                    }
                }
                return;
            }
        }

        if (m_artillerySmokeSpawned)
        {
            int remaining = m_artilleryStrikeImpactDelay - (currentTime - m_artilleryStrikeSmokeTime);
            if (remaining > 0)
            {
                Print(string.Format("[ArtilleryStrike] Waiting for smoke-to-impact delay. %1s remaining.", remaining), LogLevel.NORMAL);
                return;
            }

            if (!mortarPit || !mortarPit.CanIssueMortarFireMission())
            {
                Print("[ArtilleryStrike] Fire mission skipped: pit captured, crew dead, or mortar unavailable.", LogLevel.WARNING);
                ClearPendingArtilleryStrike();
                m_lastArtilleryStrikeEndTime = currentTime;
                return;
            }

            int shotCount = Math.RandomInt(ARTILLERY_MIN_SHOTS, ARTILLERY_MAX_SHOTS + 1);
            bool fired = mortarPit.IssueMortarFireMission(m_artilleryStrikeCenter, shotCount);
            if (!fired)
                Print("[ArtilleryStrike] Fire mission skipped: pit captured, crew dead, or mortar unavailable.", LogLevel.WARNING);
            else
                Print(string.Format("[ArtilleryStrike] Fire mission issued: %1 rounds at %2. Cooldown started for %3 seconds.", shotCount, m_artilleryStrikeCenter, cooldown), LogLevel.NORMAL);

            ClearPendingArtilleryStrike();
            m_lastArtilleryStrikeEndTime = currentTime;
            return;
        }

        if (!mortarPit || !mortarPit.CanIssueMortarFireMission())
        {
            Print("[ArtilleryStrike] Check failed: No usable mortar pit crew in this AO group.", LogLevel.NORMAL);
            return;
        }

        if (currentTime - m_lastArtilleryStrikeCheckTime < ARTILLERY_CHECK_INTERVAL)
        {
            Print(string.Format("[ArtilleryStrike] Check skipped: interval not yet met. %1s remaining.", ARTILLERY_CHECK_INTERVAL - (currentTime - m_lastArtilleryStrikeCheckTime)), LogLevel.NORMAL);
            return;
        }
        m_lastArtilleryStrikeCheckTime = currentTime;

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

        if (currentTime - m_lastArtilleryStrikeEndTime < cooldown)
        {
            Print(string.Format("[ArtilleryStrike] Check failed: On cooldown. %1 seconds remaining.", cooldown - (currentTime - m_lastArtilleryStrikeEndTime)), LogLevel.NORMAL);
            return;
        }

        float randomRoll = IA_Game.rng.RandFloat01();
        if (randomRoll > strikeChance)
        {
            Print(string.Format("[ArtilleryStrike] Check failed: Random chance not met (Rolled %1, needed <= %2).", randomRoll, strikeChance), LogLevel.NORMAL);
            return;
        }

        vector targetPos;
        if (!ComputeGroupThreatTarget(targetPos))
        {
            Print("[ArtilleryStrike] Strike aborted for area group. No recent danger events found.", LogLevel.NORMAL);
            return;
        }

        SpawnArtilleryWarningSmoke(targetPos);
        m_artilleryStrikeCenter = targetPos;
        m_artillerySmokeSpawned = true;
        m_artilleryStrikeSmokeTime = currentTime;
        m_artilleryStrikeImpactDelay = PickArtilleryImpactDelay(minDelay, maxDelay);
        Print(string.Format("[ArtilleryStrike] Warning smoke spawned at %1. Impact in %2 seconds.", targetPos, m_artilleryStrikeImpactDelay), LogLevel.NORMAL);
    }

    protected void ClearPendingArtilleryStrike()
    {
        m_artillerySmokeSpawned = false;
        m_artilleryStrikeCenter = vector.Zero;
        m_artilleryStrikeSmokeTime = 0;
        m_artilleryStrikeImpactDelay = 0;
    }

    protected int PickArtilleryImpactDelay(int minDelay, int maxDelay)
    {
        if (minDelay < 0)
            minDelay = 0;
        if (maxDelay < minDelay)
            maxDelay = minDelay;
        return Math.RandomInt(minDelay, maxDelay + 1);
    }

    protected void SpawnArtilleryWarningSmoke(vector center)
    {
        Print(string.Format("[ArtilleryStrike] Spawning 6 warning smoke markers around %1.", center), LogLevel.NORMAL);
        ref Resource smokeRes = Resource.Load(RED_SMOKE_EFFECT_PREFAB);
        if (!smokeRes)
        {
            Print(string.Format("[ArtilleryStrike] FAILED to load smoke prefab: %1", RED_SMOKE_EFFECT_PREFAB), LogLevel.ERROR);
            return;
        }

        int i;
        for (i = 0; i < 6; i++)
        {
            vector smokePos = IA_Game.rng.GenerateRandomPointInRadius(1, 100, center);
            smokePos[1] = GetGame().GetWorld().GetSurfaceY(smokePos[0], smokePos[2]);
            GetGame().SpawnEntityPrefab(smokeRes, null, IA_CreateSimpleSpawnParams(smokePos));
        }
    }

    protected IA_AreaInstance FindMortarPitInstance()
    {
        if (!m_areaInstances)
            return null;
        foreach (IA_AreaInstance instance : m_areaInstances)
        {
            if (instance && instance.IsMortarPitArea())
                return instance;
        }
        return null;
    }
}; 