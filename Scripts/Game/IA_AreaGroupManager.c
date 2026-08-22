// QRF type enum must be at global scope in Enfusion/Enforce Script
enum IA_QRFType
{
    Infantry,
    Motorized,
    Mechanized,
    Armoured
}

class IA_AreaGroupManager
{
    private ref array<ref IA_AreaInstance> m_areaInstances;

    // --- Artillery Strike System ---
    private int m_lastArtilleryStrikeCheckTime = 0;
    private int m_lastArtilleryStrikeEndTime = 0;
    private const int ARTILLERY_CHECK_INTERVAL = 60; // seconds
    private const int ARTILLERY_COOLDOWN = 300; // 5+ minutes
    private const float ARTILLERY_STRIKE_CHANCE = 0.18; // chance per check
    private const int ARTILLERY_MIN_SHOTS = 6;
    private const int ARTILLERY_MAX_SHOTS = 12;

    void IA_AreaGroupManager(array<ref IA_AreaInstance> instances)
    {
        m_areaInstances = instances;
        Print(string.Format("[AreaGroupManager] Created for a group with %1 area instances.", m_areaInstances.Count()), LogLevel.NORMAL);
    }

    // --- QRF (Quick Reaction Force) System ---

    // Check cadence
    private const int QRF_CHECK_INTERVAL = 30; // seconds
    private int m_lastQRFCheckTime = 0;

    // Single cooldown and chance for the whole QRF system
    private const int QRF_COOLDOWN = 120; // seconds
    private const float QRF_CHANCE = 0.2;
    private int m_lastQRFTime = 0;
    private bool m_qrfRetryPending = false;
    private IA_QRFType m_qrfRetryType;
    private vector m_qrfRetryTarget = vector.Zero;
    private IA_AreaInstance m_qrfRetryArea;
    private Faction m_qrfRetryFaction;
    private bool m_qrfRetryDefend = false;

    // Entry point called periodically (from MissionInitializer) to evaluate spawning of QRFs for the whole area group
    void QRFTask()
    {
        if (!Replication.IsServer())
            return; // QRF is server-authoritative only
        int currentTime = System.GetUnixTime();
        if (IA_MissionInitializer.IsQRFDisabled())
            return; // QRF globally disabled
        Print(string.Format("[QRF] Running for group with %1 areas.", m_areaInstances.Count()), LogLevel.NORMAL);

        // 1) Check interval
        if (currentTime - m_lastQRFCheckTime < QRF_CHECK_INTERVAL)
        {
            Print(string.Format("[QRF] Check skipped: interval not yet met. %1s remaining.", QRF_CHECK_INTERVAL - (currentTime - m_lastQRFCheckTime)), LogLevel.NORMAL);
            return;
        }
        m_lastQRFCheckTime = currentTime;

        // 2) Verify at least one area in this group is under attack
        bool groupUnderAttack = false;
        foreach (IA_AreaInstance instance : m_areaInstances)
        {
            if (instance && instance.IsUnderAttack())
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

        // 3) Determine target location using the same logic as Artillery (median of recent danger events, clamped by distance)
        vector targetPos;
        bool hasTarget = ComputeGroupThreatTarget(targetPos);
        if (!hasTarget || targetPos == vector.Zero)
        {
            Print("[QRF] Aborted: No valid recent danger events to target.", LogLevel.NORMAL);
            return;
        }

        // 4) Global cooldown and chance gate for QRF
        if (currentTime - m_lastQRFTime < QRF_COOLDOWN)
        {
            int remaining = QRF_COOLDOWN - (currentTime - m_lastQRFTime);
            Print(string.Format("[QRF] Global cooldown active: %1s remaining.", remaining), LogLevel.NORMAL);
            return;
        }

        float roll = IA_Game.rng.RandFloat01();
        if (roll > QRF_CHANCE)
        {
            Print(string.Format("[QRF] Global chance failed (roll %1 > %2).", roll, QRF_CHANCE), LogLevel.NORMAL);
            return;
        }

        // 5) Randomly select ONE QRF type to spawn
        int idx = Math.RandomInt(0, 7); 
        IA_QRFType selectedType;
        switch (idx)
        {
            case 0: selectedType = IA_QRFType.Infantry; break;
            case 1: selectedType = IA_QRFType.Armoured; break;
			case 4:
			case 5:
            case 2: selectedType = IA_QRFType.Mechanized; break;
            default: selectedType = IA_QRFType.Motorized; break;  // 3,6,7
        }

        // Resolve final target as the closest area's origin to the computed danger position
        IA_AreaInstance closestArea = null;
        vector finalTarget = ResolveClosestAreaTarget(targetPos, closestArea);
        if (finalTarget == vector.Zero || !closestArea)
        {
            Print("[QRF] Failed to resolve closest area target; aborting QRF.", LogLevel.WARNING);
            return;
        }

        Print(string.Format("[QRF] Selected %1; closest area '%2'; final target %3. Attempting spawn...",
            QRFTypeToString(selectedType), closestArea.GetArea().GetName(), finalTarget.ToString()), LogLevel.NORMAL);

        bool spawned = SpawnQRFForTarget(selectedType, finalTarget, closestArea, null, false, false);
        if (spawned)
        {
            m_lastQRFTime = currentTime;
        }
    }

    private string QRFTypeToString(IA_QRFType type)
    {
        switch (type)
        {
            case IA_QRFType.Infantry:   return "Infantry QRF";
            case IA_QRFType.Motorized:  return "Motorized QRF";
            case IA_QRFType.Mechanized: return "Mechanized QRF";
            case IA_QRFType.Armoured:   return "Armoured QRF";
        }
        return "QRF";
    }

    //! One mid-hold vehicle pulse for Defend missions. Biased truck/APC; rare armour. No pure infantry.
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

        // Motorized ~55%, Mechanized ~35%, Armoured ~10%
        float roll = IA_Game.rng.RandFloat01();
        IA_QRFType type;
        if (roll < 0.55)
            type = IA_QRFType.Motorized;
        else if (roll < 0.90)
            type = IA_QRFType.Mechanized;
        else
            type = IA_QRFType.Armoured;

        Print(string.Format("[QRF] Defend vehicle beat selected %1 toward %2",
            QRFTypeToString(type), defendPoint.ToString()), LogLevel.NORMAL);

        return SpawnQRFForTarget(type, defendPoint, areaInst, enemyFaction, true, false);
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
        if (!Replication.IsServer())
            return false;
        if (!targetAreaInst)
            return false;

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
                bool s2 = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(infAnchor, 1), forDefendMission);
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
        if (!m_qrfRetryArea)
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
        if (forDefendMission)
            grp.SetDefendMode(true, targetPos);
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
        grp.Spawn();
        grp.EnableInboundSimulation(targetPos);
        // Lock S&D order to this threat for the lifetime of this reinforcement
        IA_LockGroupToSearchAndDestroy(areaInst, grp, targetPos);
        return true;
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
        if (!vehicle || !group || !areaInst) return;
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
        if (!group) return;
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
        int currentTime = System.GetUnixTime();
        float strikeChance = ARTILLERY_STRIKE_CHANCE;
        int cooldown = ARTILLERY_COOLDOWN;

        IA_Config config = IA_MissionInitializer.GetGlobalConfig();
        if (config)
        {
            strikeChance = config.m_fArtilleryStrikeChance;
            cooldown = config.m_iArtilleryCooldown;
        }

        IA_AreaInstance mortarPit = FindMortarPitInstance();
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

        int shotCount = Math.RandomInt(ARTILLERY_MIN_SHOTS, ARTILLERY_MAX_SHOTS + 1);
        bool fired = mortarPit.IssueMortarFireMission(targetPos, shotCount);
        if (!fired)
        {
            Print("[ArtilleryStrike] Fire mission skipped: pit captured, crew dead, or mortar unavailable.", LogLevel.WARNING);
            return;
        }

        m_lastArtilleryStrikeEndTime = currentTime;
        Print(string.Format("[ArtilleryStrike] Fire mission issued: %1 rounds at %2. Cooldown started for %3 seconds.", shotCount, targetPos, cooldown), LogLevel.NORMAL);
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