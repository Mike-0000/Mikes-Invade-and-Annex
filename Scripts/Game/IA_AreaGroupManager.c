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
    private bool m_isArtilleryStrikeActive = false;
    private int m_lastArtilleryStrikeCheckTime = 0;
    private int m_lastArtilleryStrikeEndTime = 0;
    private vector m_artilleryStrikeCenter = vector.Zero;
    private int m_artilleryStrikeSmokeTime = 0; // Time when smoke was spawned
    private bool m_artillerySmokeSpawned = false;
    private const int ARTILLERY_CHECK_INTERVAL = 60; // seconds
    private const int ARTILLERY_COOLDOWN = 300; // 5+ minutes
    private const float ARTILLERY_STRIKE_CHANCE = 0.18; // 25% chance per check
    private const ResourceName RED_SMOKE_EFFECT_PREFAB = "{002FEEDB0213777D}Prefabs/EffectsModuleEntities/EffectModule_Particle_Smoke_Red.et";
    private const ResourceName ARTILLERY_STRIKE_PREFAB = "{11B2A636F321AD68}PrefabsEditable/EffectsModules/Mortar/IA_EffectModule_Zoned_MortarBarrage_Large.et";

    void IA_AreaGroupManager(array<ref IA_AreaInstance> instances)
    {
        m_areaInstances = instances;
        Print(string.Format("[AreaGroupManager] Created for a group with %1 area instances.", m_areaInstances.Count()), LogLevel.NORMAL);
    }

    // --- QRF (Quick Reaction Force) System ---

    // Check cadence
    private const int QRF_CHECK_INTERVAL = 60; // seconds
    private int m_lastQRFCheckTime = 0;

    // Single cooldown and chance for the whole QRF system
    private const int QRF_COOLDOWN = 300; // 300 seconds
    private const float QRF_CHANCE = 0.2;
    private int m_lastQRFTime = 0;

    // Entry point called periodically (from MissionInitializer) to evaluate spawning of QRFs for the whole area group
    void QRFTask()
    {
        if (!Replication.IsServer())
            return; // QRF is server-authoritative only
        int currentTime = System.GetUnixTime();
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

        bool spawned = SpawnQRF(selectedType, finalTarget);
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
        if (!Replication.IsServer())
            return false;

        // Resolve faction context
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (!initializer)
            return false;
        Faction enemyGameFaction = initializer.GetRandomEnemyFaction();
        if (!enemyGameFaction)
            return false;

        // Find the nearest area instance to the final target to integrate groups
        IA_AreaInstance targetAreaInst = null;
        ResolveClosestAreaTarget(targetPos, targetAreaInst);
        if (!targetAreaInst)
            return false;

        string areaName = targetAreaInst.GetArea().GetName();

        // Compute a shared spawn anchor so all units in this QRF spawn together
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        vector areaOrigin = targetAreaInst.GetArea().GetOrigin();
        vector qrfAnchor = FindSafeRoadSpawnPos(areaOrigin, activeGroup, 300, 450);
        if (qrfAnchor == vector.Zero)
        {
            // Fallback to area origin if no road found
            qrfAnchor = areaOrigin;
            qrfAnchor[1] = GetGame().GetWorld().GetSurfaceY(qrfAnchor[0], qrfAnchor[2]);
        }

        bool success = false;
        switch (type)
        {
            case IA_QRFType.Infantry:
            {
                // Minimum 2 infantry groups
                bool s1 = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(qrfAnchor, 0));
                bool s2 = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(qrfAnchor, 1));
                success = (s1 || s2);
                break;
            }
            case IA_QRFType.Motorized:
            {
                // Minimum: 1 filled truck and 1 infantry
                bool v = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, /*preferAPC*/ false, /*allowTrucks*/ true, /*armourOnly*/ false, ComputeClusterPos(qrfAnchor, 0));
                bool inf = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(qrfAnchor, 1));
                success = (v || inf);
                break;
            }
            case IA_QRFType.Mechanized:
            {
                // Minimum: 1 APC and 1 filled truck
                bool apc = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, /*preferAPC*/ true, /*allowTrucks*/ false, /*armourOnly*/ false, ComputeClusterPos(qrfAnchor, 0));
                bool truck = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, /*preferAPC*/ false, /*allowTrucks*/ true, /*armourOnly*/ false, ComputeClusterPos(qrfAnchor, 1));
                success = (apc || truck);
                break;
            }
            case IA_QRFType.Armoured:
            {
                // Minimum: 2 APCs/Armor and 1 Infantry
                bool a1 = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, /*preferAPC*/ true, /*allowTrucks*/ false, /*armourOnly*/ true, ComputeClusterPos(qrfAnchor, 0));
                bool a2 = SpawnVehicleQRF(targetAreaInst, enemyGameFaction, targetPos, /*preferAPC*/ true, /*allowTrucks*/ false, /*armourOnly*/ true, ComputeClusterPos(qrfAnchor, 1));
                bool inf = SpawnInfantryQRF(targetAreaInst, enemyGameFaction, targetPos, ComputeClusterPos(qrfAnchor, 2));
                success = (a1 || a2 || inf);
                break;
            }
        }

        if (success)
        {
            // Notify players (reuse existing ReinforcementsCalled notification channel)
            string notif = QRFTypeToString(type) + " inbound at " + areaName + "!";
            IA_Game.S_TriggerGlobalNotification("ReinforcementsCalled", notif);
            Print(string.Format("[QRF] %1 spawned towards %2 at %3", QRFTypeToString(type), areaName, targetPos.ToString()), LogLevel.NORMAL);
        }
        else
        {
            Print(string.Format("[QRF] Spawn failed for %1 at area %2 (target %3)", QRFTypeToString(type), areaName, targetPos.ToString()), LogLevel.WARNING);
        }
        return success;
    }

    private bool SpawnInfantryQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, vector preferredSpawn)
    {
        float scale = IA_Game.GetAIScaleFactor();
        int unitCount = Math.Clamp(Math.Round(6 * scale), 4, 10);

        // Spawn position near a road close to target
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        // Spawn relative to the area's origin (final target), not the raw danger position
        vector anchor = preferredSpawn;
        if (anchor == vector.Zero)
            anchor = areaInst.GetArea().GetOrigin();
        vector spawnPos = FindSafeRoadSpawnPos(anchor, activeGroup, 300, 400);
        if (spawnPos == vector.Zero)
        {
            Print(string.Format("[QRF] Infantry fallback: no safe road found near %1; aborting.", areaInst.GetArea().GetOrigin()), LogLevel.WARNING);
            return false;
        }

        IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(spawnPos, IA_Faction.USSR, unitCount, enemyGameFaction);
        if (!grp)
            return false;

        grp.SetAssignedArea(areaInst.GetArea());
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
                    w.SetPriorityLevel(30);
                    grp.AddWaypoint(w);
                }
            }
        }
        areaInst.AddMilitaryGroup(grp);
        grp.Spawn();
        // Lock S&D order to this threat for the lifetime of this reinforcement
        IA_LockGroupToSearchAndDestroy(areaInst, grp, targetPos);
        return true;
    }

    private bool SpawnVehicleQRF(IA_AreaInstance areaInst, Faction enemyGameFaction, vector targetPos, bool preferAPC, bool allowTrucks, bool armourOnly, vector preferredSpawn)
    {
        // Determine a road-based spawn near the target
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        vector anchor = preferredSpawn;
        if (anchor == vector.Zero)
            anchor = areaInst.GetArea().GetOrigin();
        vector spawnPos = FindSafeRoadSpawnPos(anchor, activeGroup, 300, 450);
        if (spawnPos == vector.Zero)
        {
            Print(string.Format("[QRF] Vehicle fallback: no safe road found near %1; aborting.", areaInst.GetArea().GetOrigin()), LogLevel.WARNING);
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

        // Destination: pick a road near the objective to drive to (APC/Armor must use road move waypoints)
        int driveGroup = IA_VehicleManager.GetActiveGroup();
        vector areaOrigin = areaInst.GetArea().GetOrigin();
        vector driveTarget = IA_VehicleManager.FindRandomRoadPointForVehiclePatrol(areaOrigin, 400, driveGroup);
        if (driveTarget == vector.Zero)
            driveTarget = areaOrigin;

        IA_AiGroup vehicleGroup = IA_VehicleManager.PlaceUnitsInVehicle(selectedVehicle, IA_Faction.USSR, driveTarget, areaInst, enemyGameFaction);
        if (!vehicleGroup)
        {
            Print("[QRF] Failed to create AI group for vehicle QRF.", LogLevel.WARNING);
            return false;
        }

        // Ensure assigned area for proper integration
        vehicleGroup.SetAssignedArea(areaInst.GetArea());

        // Branch behavior by vehicle class: trucks dismount and defend; APC/Armor lock to S&D
        bool vIsTruck = false; bool vIsAPC = false; bool vIsArmored = false;
        DetermineVehicleClass(selectedVehicle, vIsTruck, vIsAPC, vIsArmored);
        if (vIsTruck && !vIsAPC && !vIsArmored)
        {
            // Replace any existing vehicle waypoint with a high-priority Move waypoint (priority 2000)
            vehicleGroup.RemoveAllOrders(true);
            ResourceName moveWpRes = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
            Resource moveRes = Resource.Load(moveWpRes);
            if (moveRes)
            {
                EntitySpawnParams wpParams = EntitySpawnParams();
                wpParams.TransformMode = ETransformMode.WORLD;
                wpParams.Transform[3] = driveTarget;
                IEntity wpEnt = GetGame().SpawnEntityPrefab(moveRes, null, wpParams);
                SCR_AIWaypoint wp = SCR_AIWaypoint.Cast(wpEnt);
                if (wp)
                {
                    wp.SetPriorityLevel(100);
                    vehicleGroup.AddWaypoint(wp);
                }
            }

            // Poll until arrival, then order dismount and S&D after 30s
            GetGame().GetCallqueue().CallLater(this.QRF_PollTruckArrival, 3000, false, selectedVehicle, vehicleGroup, areaInst, areaOrigin, driveTarget);
        }
        else
        {
            // APC/Armor: keep only move waypoints; set priority to 15 for the move waypoint
            SCR_AIWaypoint currentWp = null;
            if (!vehicleGroup.HasActiveWaypoint())
            {
                // Create one if missing
                ResourceName moveWpRes2 = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
                Resource moveRes2 = Resource.Load(moveWpRes2);
                if (moveRes2)
                {
                    EntitySpawnParams wpParams2 = EntitySpawnParams();
                    wpParams2.TransformMode = ETransformMode.WORLD;
                    wpParams2.Transform[3] = driveTarget;
                    IEntity wpEnt2 = GetGame().SpawnEntityPrefab(moveRes2, null, wpParams2);
                    currentWp = SCR_AIWaypoint.Cast(wpEnt2);
                    if (currentWp) vehicleGroup.AddWaypoint(currentWp);
                }
            }
            else
            {
                // We can't directly get the group's waypoint from here; assume the last added move is active and priority was set.
            }
            if (currentWp)
            {
                currentWp.SetPriorityLevel(40);
            }
        }
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

    // Helper: determine if any player is within radius of a position
    private bool IsNearAnyPlayer(vector position, float radius)
    {
        array<int> playerIDs = {};
        GetGame().GetPlayerManager().GetAllPlayers(playerIDs);
        float rSq = radius * radius;
        foreach (int playerID : playerIDs)
        {
            PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerID);
            if (!pc) continue;
            IEntity ent = pc.GetControlledEntity();
            if (!ent) continue;
            vector p = ent.GetOrigin();
            if (vector.DistanceSq(p, position) <= rSq)
                return true;
        }
        return false;
    }

    // Helper: progressively expand road search radius, skipping positions too close to players
    private vector FindSafeRoadSpawnPos(vector center, int groupNumber, float initialRadius, float minPlayerDistance)
    {
        float radius = initialRadius;
        const float RADIUS_INCREMENT = 150.0;
        const float ABS_MAX_RADIUS = 1200.0;
        while (radius <= ABS_MAX_RADIUS)
        {
            vector roadPos = IA_VehicleManager.FindRandomRoadEntityInZone(center, radius, groupNumber);
            if (roadPos != vector.Zero)
            {
                if (!IsNearAnyPlayer(roadPos, minPlayerDistance))
                {
                    roadPos[1] = GetGame().GetWorld().GetSurfaceY(roadPos[0], roadPos[2]);
                    return roadPos;
                }
                else
                {
                    Print(string.Format("[QRF] Found road at %1 but too close to players (<%2m). Expanding radius...", roadPos.ToString(), minPlayerDistance), LogLevel.NORMAL);
                }
            }
            radius += RADIUS_INCREMENT;
        }
        // Fallback to a navigation-friendly point if no safe road found
        vector fallback = IA_VehicleManager.FindFallbackNavigationPoint(center, initialRadius, groupNumber);
        if (fallback != vector.Zero && !IsNearAnyPlayer(fallback, minPlayerDistance))
        {
            fallback[1] = GetGame().GetWorld().GetSurfaceY(fallback[0], fallback[2]);
            return fallback;
        }
        return vector.Zero;
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
            group.RemoveAllOrders(true);
            group.AddOrder(areaOrigin, IA_AiOrder.GetOutOfVehicle, true);
            // After 30 seconds, add a defend waypoint
            GetGame().GetCallqueue().CallLater(this.QRF_AddDefendAfterDisembark, 30000, false, group, areaOrigin);
            return;
        }
        // Not yet; poll again in 3s
        GetGame().GetCallqueue().CallLater(this.QRF_PollTruckArrival, 3000, false, vehicle, group, areaInst, areaOrigin, driveTarget);
    }

    private void QRF_AddDefendAfterDisembark(IA_AiGroup group, vector defendPos)
    {
        if (!group) return;
        group.AddOrder(defendPos, IA_AiOrder.Defend, true);
        group.SetTacticalState(IA_GroupTacticalState.Defending, defendPos, null, true);
    }

    void ArtilleryStrikeTask()
    {
        int currentTime = System.GetUnixTime();
		Print(string.Format("[ArtilleryStrikeTask] Running for group with %1 areas.", m_areaInstances.Count()), LogLevel.NORMAL);

        // If a strike is active, manage its lifecycle
        if (m_isArtilleryStrikeActive)
        {
            if (m_artillerySmokeSpawned && (currentTime - m_artilleryStrikeSmokeTime >= Math.RandomInt(45, 70)))
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
            return; // Don't try to start a new one
        }

        // --- New Strike Checks ---

        // 1. Check main 60-second timer
        if (currentTime - m_lastArtilleryStrikeCheckTime < ARTILLERY_CHECK_INTERVAL)
		{
			Print(string.Format("[ArtilleryStrike] Check skipped: interval not yet met. %1s remaining.", ARTILLERY_CHECK_INTERVAL - (currentTime - m_lastArtilleryStrikeCheckTime)), LogLevel.NORMAL);
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
		vector groupCenter = vector.Zero;
		if (!m_areaInstances.IsEmpty())
		{
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
				Print(string.Format("[ArtilleryStrike] Calculated area group center: %1", groupCenter), LogLevel.NORMAL);
			}
			else
			{
				 Print("[ArtilleryStrike] Could not calculate area group center, no valid areas found. Aborting strike.", LogLevel.WARNING);
				 return;
			}
		}
		else
		{
			Print("[ArtilleryStrike] Strike aborted, no area instances in group.", LogLevel.WARNING);
			return;
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
                // Only consider recent danger events
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
							Print(string.Format("[ArtilleryStrike] Discarded danger event at %1, too far from group center %2 (Distance: %3m, Max: %4m)", 
								currentDangerPos, groupCenter, vector.Distance(currentDangerPos, groupCenter), MAX_DANGER_EVENT_DISTANCE), LogLevel.NORMAL);
						}
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
        m_artilleryStrikeCenter = IA_Game.rng.GenerateRandomPointInRadius(4, 30, primaryThreatLocation);
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
    }
}; 