/*enum IA_VehicleType
{
    CivilianCar,
    CivilianTruck,
    MilitaryTransport,
    MilitaryPatrol,
    MilitaryAPC
};*/

class IA_VehicleManagerClass: GenericEntityClass {};

class IA_VehicleManager: GenericEntity
{
    static private ref array<IEntity> m_vehicles = {};
    static private IA_VehicleManager m_instance;
    static private const float DEFAULT_INITIAL_ROAD_SEARCH_RADIUS = 30.0;
    
    // Group-specific vehicles
    static private ref array<ref array<IEntity>> m_groupVehicles = {};
    static private int m_currentActiveGroup = -1;
    static private int m_previousActiveGroup = -1; // Add tracking for previous active group
    
    // Vehicle reservation system
    static private ref map<IEntity, IA_AiGroup> m_vehicleReservations = new map<IEntity, IA_AiGroup>(); // vehicle -> group that reserved it
    
    void IA_VehicleManager(IEntitySource src, IEntity parent)
    {
        m_instance = this;
        
        // Initialize vehicle groups array (create empty arrays for each potential group)
        for (int i = 0; i < 10; i++)  // Assuming a max of 10 groups
        {
            m_groupVehicles.Insert(new array<IEntity>());
        }
    }
    
    static IA_VehicleManager Instance()
    {
        return m_instance;
    }
    
    static void Reset()
    {
        // Print(("[VEHICLE_DEBUG] IA_VehicleManager.Reset called", LogLevel.NORMAL);
        
        // Clear all tracked vehicles
        m_vehicles.Clear();
        
        // Clear vehicle groups
        m_groupVehicles.Clear();
        
        // Reset active group
        m_currentActiveGroup = -1;
        m_previousActiveGroup = -1;
        
        // Clear all reservations
        ClearAllReservations();
    }
    
    // Set the currently active area group
    static void SetActiveGroup(int groupNumber)
    {
        // Store the previous group before changing
        m_previousActiveGroup = m_currentActiveGroup;
        
        // Validate input
        if (groupNumber < 0)
        {
            //// Print(("[VEHICLE_DEBUG] SetActiveGroup called with invalid group number: " + groupNumber, LogLevel.WARNING);
            m_currentActiveGroup = -1;
            return;
        }
        
        // // Print( debug info about the group change
        // Print(("[VEHICLE_DEBUG] Changing active group from " + m_currentActiveGroup + " to " + groupNumber, LogLevel.NORMAL);
        
        // Ensure we have enough slots in the array
        if (m_groupVehicles.Count() <= groupNumber)
        {
            int originalSize = m_groupVehicles.Count();
            for (int i = originalSize; i <= groupNumber; i++)
            {
                m_groupVehicles.Insert(new array<IEntity>());
            }
            // Print(("[VEHICLE_DEBUG] Expanded group vehicles array from " + originalSize + " to " + m_groupVehicles.Count() + " slots", LogLevel.NORMAL);
        }
        
        // Set the active group
        m_currentActiveGroup = groupNumber;
        
        // Clean up vehicles from inactive groups
        CleanupInactiveGroupVehicles();
    }
    
    // Get the currently active area group
    static int GetActiveGroup()
    {
        return m_currentActiveGroup;
    }
    
    // Clean up vehicles from inactive groups
    static void CleanupInactiveGroupVehicles()
    {
      //  // Print(("[VEHICLE_DEBUG] CleanupInactiveGroupVehicles called", LogLevel.NORMAL);
        
        // Skip if the active group hasn't changed
        if (m_currentActiveGroup == m_previousActiveGroup)
        {
            //// Print(("[VEHICLE_DEBUG] Skipping cleanup - active group unchanged: " + m_currentActiveGroup, LogLevel.NORMAL);
            return;
        }
        
        // Print(("[VEHICLE_DEBUG] Active group changed from " + m_previousActiveGroup + " to " + m_currentActiveGroup + ". Starting cleanup.", LogLevel.NORMAL);
        
        int inactiveGroup = m_previousActiveGroup;
        
        // Get vehicles in the inactive group
        array<Vehicle> inactiveGroupVehicles = GetVehiclesInAreaGroup(inactiveGroup);
        
        if (inactiveGroupVehicles.IsEmpty())
        {
            //// Print(("[VEHICLE_DEBUG] No vehicles found in inactive group " + inactiveGroup, LogLevel.NORMAL);
            return;
        }
        
        // Print(("[VEHICLE_DEBUG] Found " + inactiveGroupVehicles.Count() + " vehicles in inactive group " + inactiveGroup, LogLevel.NORMAL);
        
        // Handle each vehicle in the inactive group
        foreach (Vehicle vehicle : inactiveGroupVehicles)
        {
            // Check if the vehicle is reserved
            if (IsVehicleReserved(vehicle))
            {
                IA_AiGroup reservingGroup = m_vehicleReservations[vehicle];
                
                // If it's reserved and the group is still active, leave it alone
                if (reservingGroup && reservingGroup.IsSpawned() && reservingGroup.GetAliveCount() > 0)
                {
                    // Print(("[VEHICLE_DEBUG] Vehicle " + vehicle + " has active group " + reservingGroup + " - preserving", LogLevel.NORMAL);
                    continue;
                }
                
                // Otherwise release the reservation
                //// Print(("[VEHICLE_DEBUG] Vehicle " + vehicle + " has inactive group - releasing reservation", LogLevel.NORMAL);
                ReleaseVehicleReservation(vehicle);
            }
            
            // If the vehicle is not occupied by players, despawn it
            if (!IsVehicleOccupied(vehicle))
            {
                // Print(("[VEHICLE_DEBUG] Despawning unoccupied vehicle " + vehicle + " from inactive group", LogLevel.NORMAL);
                DespawnVehicle(vehicle);
            }
            else
            {
                // Print(("[VEHICLE_DEBUG] Vehicle " + vehicle + " is occupied - not despawning", LogLevel.NORMAL);
            }
        }
        
        // Print(("[VEHICLE_DEBUG] Inactive group cleanup complete", LogLevel.NORMAL);
    }
    
    static string GetVehicleResourceName(IA_Faction faction, Faction AreaFaction)
    {
		// Print(("GetVehicleResourceName Called", LogLevel.NORMAL);
        string resourceName = IA_VehicleCatalog.GetRandomVehiclePrefab(faction, AreaFaction);
		// Print(("resourceName Received as " + resourceName, LogLevel.NORMAL);
        if (resourceName.IsEmpty())
        {
            // Fallback in case no prefab is found
            return "{47D94E1193A88497}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport.et";
        }
        return resourceName;
    }
    
    // Get list of all vehicles that match specific filter criteria
    static array<string> GetVehiclePrefabsByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary, Faction AreaFaction)
    {
        return IA_VehicleCatalog.GetVehiclesByFilter(faction, allowCivilian, allowMilitary, AreaFaction);
    }
    
    // Spawn a random vehicle that matches filter criteria at the specified position
    static Vehicle SpawnRandomVehicle(IA_Faction faction, bool allowCivilian, bool allowMilitary, vector position, Faction AreaFaction)
    {
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle called with faction " + faction + ", civilian=" + allowCivilian + ", military=" + allowMilitary, LogLevel.NORMAL);
        
        // Get catalog entries matching the filter
        array<SCR_EntityCatalogEntry> entries = IA_VehicleCatalog.GetVehicleEntriesByFilter(faction, allowCivilian, allowMilitary, AreaFaction);
        if (entries.IsEmpty())
        {
           //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: No matching entries found in catalog", LogLevel.WARNING);
            return null;
        }
            
        // Pick a random entry
        int randomIndex = IA_Game.rng.RandInt(0, entries.Count());
        SCR_EntityCatalogEntry entry = entries[randomIndex];
        string resourceName = entry.GetPrefab();
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Selected prefab " + resourceName + " (index " + randomIndex + " of " + entries.Count() + ")", LogLevel.NORMAL);
        
        // Load resource and spawn
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Loading resource " + resourceName, LogLevel.NORMAL);
        Resource resource = Resource.Load(resourceName);
        if (!resource)
        {
           //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Failed to load resource: " + resourceName, LogLevel.ERROR);
            return null;
        }
        
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Creating spawn params at position " + position.ToString(), LogLevel.NORMAL);
        EntitySpawnParams params = IA_CreateSurfaceAdjustedSpawnParams(position);
        
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Spawning entity prefab", LogLevel.NORMAL);
        IEntity entity = GetGame().SpawnEntityPrefab(resource, null, params);
        
        if (!entity)
        {
           //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Failed to spawn entity prefab", LogLevel.ERROR);
            return null;
        }
        
       //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Casting to Vehicle", LogLevel.NORMAL);
        Vehicle vehicle = Vehicle.Cast(entity);
        
        if (vehicle)
        {
           //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Successfully spawned vehicle at " + vehicle.GetOrigin().ToString(), LogLevel.NORMAL);
            // Add to main vehicles array
            m_vehicles.Insert(entity);
            
            // Add to group-specific array if active group is valid
            if (m_currentActiveGroup >= 0 && m_currentActiveGroup < m_groupVehicles.Count())
            {
                m_groupVehicles[m_currentActiveGroup].Insert(entity);
               //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Added to group " + m_currentActiveGroup, LogLevel.NORMAL);
            }
        }
        else
        {
           //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Failed to cast to Vehicle, entity type: " + entity.ClassName(), LogLevel.WARNING);
            // Clean up the entity if it's not a vehicle
            if (entity)
            {
                IA_Game.AddEntityToGc(entity);
               //// Print(("[DEBUG_VEHICLE_SPAWN] SpawnRandomVehicle: Added non-vehicle entity to garbage collection", LogLevel.NORMAL);
            }
        }
            
        return vehicle;
    }
    
    // Spawn a vehicle at the specified position
    static Vehicle SpawnVehicle(IA_Faction faction, vector position, Faction AreaFaction)
    {
        //// Print(("[DEBUG] IA_VehicleManager.SpawnVehicle called with faction " + faction, LogLevel.NORMAL);
        
        // Get vehicle resource name using our helper method
        string resourceName = GetVehicleResourceName(faction, AreaFaction);
        if (resourceName.IsEmpty())
        {
           //// Print(("[IA_VehicleManager.SpawnVehicle] Couldn't find any vehicles for faction " + faction, LogLevel.ERROR);
            return null;
        }
        
        Resource resource = Resource.Load(resourceName);
        if (!resource)
        {
           //// Print(("[IA_VehicleManager.SpawnVehicle] No such resource: " + resourceName, LogLevel.ERROR);
            return null;
        }
        
        EntitySpawnParams params = IA_CreateSurfaceAdjustedSpawnParams(position);
        IEntity entity = GetGame().SpawnEntityPrefab(resource, null, params);
        Vehicle vehicle = Vehicle.Cast(entity);
        
        if (vehicle)
        {
            //// Print(("[DEBUG] IA_VehicleManager.SpawnVehicle: Successfully spawned vehicle at " + position.ToString(), LogLevel.NORMAL);
            // Add to main vehicles array
            m_vehicles.Insert(entity);
            
            // Add to group-specific array if active group is valid
            if (m_currentActiveGroup >= 0 && m_currentActiveGroup < m_groupVehicles.Count())
            {
                m_groupVehicles[m_currentActiveGroup].Insert(entity);
            }
            
            // Automatically create AI units for the vehicle
            if (faction != IA_Faction.CIV && IA_Game.CurrentAreaInstance) // Only for military vehicles
            {
                //// Print(("[DEBUG] IA_VehicleManager.SpawnVehicle: Automatically creating AI crew for vehicle", LogLevel.NORMAL);
                
                // Declare destination variable
                vector destination = position;
                
                // Use group center and radius if available, otherwise fall back to current area
                vector originPoint;
                vector groupCenter = IA_AreaMarker.CalculateGroupCenterPoint(m_currentActiveGroup);
                float groupRadius = IA_AreaMarker.CalculateGroupRadius(m_currentActiveGroup);
                if (groupCenter != vector.Zero)
                    originPoint = groupCenter;
                else
                    originPoint = IA_Game.CurrentAreaInstance.m_area.GetOrigin();
                
                float radiusToUse;
                if (groupRadius > 0)
                    radiusToUse = groupRadius * 0.8;
                else
                    radiusToUse = IA_Game.CurrentAreaInstance.m_area.GetRadius() * 0.8;
                
                // Directly find a random road entity in the area
                vector roadPos = FindRandomRoadPointForVehiclePatrol(originPoint, radiusToUse, m_currentActiveGroup); 
                
                // If a valid road is found, use it as destination
                if (roadPos != vector.Zero)
                    destination = roadPos;
                else
                {
                    // Fallback to a random position if no road found
                    destination = IA_Game.rng.GenerateRandomPointInRadius(1, radiusToUse, originPoint);
                }
                
                // Create AI and assign to vehicle
                PlaceUnitsInVehicle(vehicle, faction, destination, IA_Game.CurrentAreaInstance, AreaFaction);
            }
        }
        else
        {
            //// Print(("[DEBUG] IA_VehicleManager.SpawnVehicle: Failed to spawn vehicle", LogLevel.WARNING);
        }
            
        return vehicle;
    }
    
    static Vehicle GetClosestVehicle(vector position, float maxDistance = 100)
    {
        Vehicle closest = null;
        float minDist = maxDistance;
        
        foreach (IEntity entity : m_vehicles)
        {
            Vehicle vehicle = Vehicle.Cast(entity);
            if (!vehicle)
                continue;
                
            float dist = vector.Distance(position, vehicle.GetOrigin());
            if (dist < minDist)
            {
                minDist = dist;
                closest = vehicle;
            }
        }
        
        return closest;
    }
    
    static bool IsVehicleOccupied(Vehicle vehicle)
    {
        if (!vehicle)
            return false;
        return vehicle.IsOccupied();
    }
    
    static SCR_ChimeraCharacter GetVehicleDriver(Vehicle vehicle)
    {
        if (!vehicle)
            return null;
        return SCR_ChimeraCharacter.Cast(vehicle.GetPilot());
    }
    
    static Vehicle GetCharacterVehicle(SCR_ChimeraCharacter character)
    {
        if (!character || !character.IsInVehicle())
            return null;
        
        CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
        if (!access)
            return null;
        
        return Vehicle.Cast(access.GetCompartment().GetOwner());
    }
    
    static void DespawnVehicle(Vehicle vehicle)
    {
        if (!vehicle)
            return;
            
        int idx = m_vehicles.Find(vehicle);
        if (idx != -1)
            m_vehicles.Remove(idx);
            
        IA_Game.AddEntityToGc(vehicle);
    }
    
    // Spawn a vehicle randomly within the area group's bounds
    static Vehicle SpawnVehicleInAreaGroup(IA_Faction faction, int groupNumber, Faction AreaFaction)
    {
        // Calculate a position within the area group
        vector centerPoint = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
        float groupRadius = IA_AreaMarker.CalculateGroupRadius(groupNumber);
        
        if (centerPoint == vector.Zero || groupRadius <= 0)
            return null;
            
        // Generate a random position within the group area
        vector spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, groupRadius * 0.8, centerPoint);
        
        // Adjust to ground level
        float y = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
        spawnPos[1] = y + 0.5; // Slightly above ground
        
        // Spawn the vehicle
        return SpawnVehicle(faction, spawnPos, AreaFaction);
    }
    
    // Spawn a random vehicle within the area group's bounds
    static Vehicle SpawnRandomVehicleInAreaGroup(IA_Faction faction, bool allowCivilian, bool allowMilitary, int groupNumber, Faction AreaFaction)
    {
        //// Print(("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup called with faction " + faction + ", civilian=" + allowCivilian + ", military=" + allowMilitary + ", group=" + groupNumber, LogLevel.NORMAL);
        
        // Calculate a position within the area group
        vector centerPoint = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
        float groupRadius = IA_AreaMarker.CalculateGroupRadius(groupNumber);
        
        if (centerPoint == vector.Zero || groupRadius <= 0)
        {
            //// Print(("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup: Invalid center point or radius", LogLevel.WARNING);
            return null;
        }
            
        // Generate a random position within the group area
        vector spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, groupRadius * 0.8, centerPoint);
        
        // Adjust to ground level
        float y = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
        spawnPos[1] = y + 0.5; // Slightly above ground
        
        //// Print(("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup: Generated position " + spawnPos.ToString(), LogLevel.NORMAL);
        
        // Spawn the random vehicle
        return SpawnRandomVehicle(faction, allowCivilian, allowMilitary, spawnPos, AreaFaction);
    }
    
    // Get all active vehicles in a specific area group
    static array<Vehicle> GetVehiclesInAreaGroup(int groupNumber)
    {
        array<Vehicle> groupVehicles = {};
        
        // If group index is valid
        if (groupNumber >= 0 && groupNumber < m_groupVehicles.Count())
        {
            // Get vehicles from the specific group array
            foreach (IEntity entity : m_groupVehicles[groupNumber])
            {
                if (!entity)
                    continue;
                    
                Vehicle vehicle = Vehicle.Cast(entity);
                if (vehicle)
                    groupVehicles.Insert(vehicle);
            }
        }
        
        return groupVehicles;
    }
    
    // Get closest vehicle within a specific area group
    static Vehicle GetClosestVehicleInGroup(vector position, int groupNumber, float maxDistance = 100)
    {
        Vehicle closest = null;
        float minDist = maxDistance;
        
        array<Vehicle> groupVehicles = GetVehiclesInAreaGroup(groupNumber);
        foreach (Vehicle vehicle : groupVehicles)
        {
            float dist = vector.Distance(position, vehicle.GetOrigin());
            if (dist < minDist)
            {
                minDist = dist;
                closest = vehicle;
            }
        }
        
        return closest;
    }
    
    // Reserve a vehicle for a specific group
    static bool ReserveVehicle(Vehicle vehicle, IA_AiGroup reservingGroup)
    {
        // Print(("[VEHICLE_DEBUG] ReserveVehicle called. Vehicle: " + vehicle + ", Group: " + reservingGroup, LogLevel.NORMAL);
        
        if (!vehicle || !reservingGroup)
        {
            // Print(("[VEHICLE_DEBUG] ReserveVehicle - vehicle or group is null", LogLevel.WARNING);
            return false;
        }

        // If the vehicle is already reserved, only allow if it's the same group making the reservation
        if (m_vehicleReservations.Contains(vehicle))
        {
            IA_AiGroup currentGroup = m_vehicleReservations[vehicle];
            if (currentGroup == reservingGroup)
            {
                // Already reserved by this group, just update the timestamp
                // Print(("[VEHICLE_DEBUG] Vehicle already reserved by same group", LogLevel.NORMAL);
                return true;
            }
            else
            {
                // Print(("[VEHICLE_DEBUG] Vehicle already reserved by different group: " + currentGroup, LogLevel.WARNING);
                return false;
            }
        }

        // Record current reservations for debugging
        string previousMapState = "Before reservation: ";
        foreach (IEntity veh, IA_AiGroup grp : m_vehicleReservations)
            previousMapState += veh.ToString() + " -> " + grp.ToString() + ", ";
        // Print(("[VEHICLE_DEBUG] " + previousMapState, LogLevel.NORMAL);

        // Make the reservation
        m_vehicleReservations[vehicle] = reservingGroup;
        
        // Debug the new state
        // Print(("[VEHICLE_DEBUG] Vehicle reserved successfully. Total reservations: " + m_vehicleReservations.Count(), LogLevel.NORMAL);
        return true;
    }
    
    // Check if a vehicle is reserved by any group
    static bool IsVehicleReserved(Vehicle vehicle)
    {
        if (!vehicle)
            return false;
            
        bool isReserved = m_vehicleReservations.Contains(vehicle);
        if (isReserved)
        {
            IA_AiGroup reservedBy = m_vehicleReservations[vehicle];
            // Print(("[VEHICLE_DEBUG] IsVehicleReserved: Vehicle " + vehicle + " is reserved by group " + reservedBy, LogLevel.NORMAL);
        }
        
        return isReserved;
    }
    
    // Check if a vehicle is reserved by a specific group
    static bool IsReservedByGroup(Vehicle vehicle, IA_AiGroup group)
    {
        // Print(("[VEHICLE_DEBUG] IsReservedByGroup called, vehicle: " + vehicle + ", group: " + group, LogLevel.NORMAL);
        
        if (!vehicle || !group)
        {
            // Print(("[VEHICLE_DEBUG] IsReservedByGroup early return - vehicle or group is null", LogLevel.WARNING);
            return false;
        }
            
        if (!m_vehicleReservations.Contains(vehicle))
        {
            // Print(("[VEHICLE_DEBUG] Vehicle not in reservation map", LogLevel.NORMAL);
            return false;
        }
            
        IA_AiGroup reservingGroup = m_vehicleReservations[vehicle];
        bool result = (reservingGroup == group);
        // Print(("[VEHICLE_DEBUG] Vehicle is reserved by group " + reservingGroup + ", matches query group: " + result, LogLevel.NORMAL);
        return result;
    }
    
    // Release a vehicle's reservation
    static void ReleaseVehicleReservation(Vehicle vehicle)
    {
        // Print(("[VEHICLE_DEBUG] ReleaseVehicleReservation called for vehicle: " + vehicle, LogLevel.NORMAL);
        
        if (!vehicle)
        {
            // Print(("[VEHICLE_DEBUG] ReleaseVehicleReservation - vehicle is null", LogLevel.WARNING);
            return;
        }
            
        if (!m_vehicleReservations.Contains(vehicle))
        {
            // Print(("[VEHICLE_DEBUG] ReleaseVehicleReservation - vehicle not in reservation map", LogLevel.WARNING);
            return;
        }
            
        // Record group before removal for debugging
        IA_AiGroup group = m_vehicleReservations[vehicle];
        
        // Remove the reservation
        m_vehicleReservations.Remove(vehicle);
        
       //Print("[VEHICLE_DEBUG] Released reservation for vehicle " + vehicle + " from group " + group + ". Remaining reservations: " + m_vehicleReservations.Count(), LogLevel.NORMAL);
    }
    
    // Get count of units targeting a specific vehicle
    static int GetVehicleTargetCount(Vehicle vehicle)
    {
        // Implementation depends on how you want to track this
        // For now, just check if it's reserved
        if (IsVehicleReserved(vehicle))
            return 1;
        else
            return 0;
    }
    
    // Find the closest unreserved vehicle
    static Vehicle GetClosestUnreservedVehicle(vector position, float maxDistance = 100)
    {
        Vehicle closest = null;
        float minDist = maxDistance;
        
        foreach (IEntity entity : m_vehicles)
        {
            Vehicle vehicle = Vehicle.Cast(entity);
            if (!vehicle)
                continue;
                
            // Skip if already reserved
            if (IsVehicleReserved(vehicle))
                continue;
                
            float dist = vector.Distance(position, vehicle.GetOrigin());
            if (dist < minDist)
            {
                minDist = dist;
                closest = vehicle;
            }
        }
        
        return closest;
    }
    
    // Find the closest unreserved vehicle in a specific area group
    static Vehicle GetClosestUnreservedVehicleInGroup(vector position, int groupNumber, float maxDistance = 100)
    {
        Vehicle closest = null;
        float minDist = maxDistance;
        
        array<Vehicle> groupVehicles = GetVehiclesInAreaGroup(groupNumber);
        foreach (Vehicle vehicle : groupVehicles)
        {
            // Skip if already reserved
            if (IsVehicleReserved(vehicle))
                continue;
                
            float dist = vector.Distance(position, vehicle.GetOrigin());
            if (dist < minDist)
            {
                minDist = dist;
                closest = vehicle;
            }
        }
        
        return closest;
    }
    
    // Check if a vehicle has reached its destination waypoint (within 10m)
    static bool HasVehicleReachedDestination(Vehicle vehicle, vector destination)
    {
        if (!vehicle)
        {
            // Print(("[VEHICLE_DEBUG] HasVehicleReachedDestination - vehicle is NULL", LogLevel.WARNING);
            return false;
        }
            
        vector vehiclePos = vehicle.GetOrigin();
        float distance = vector.Distance(vehiclePos, destination);
        
        // Print(("[VEHICLE_DEBUG] HasVehicleReachedDestination - vehicle at " + vehiclePos.ToString() + ", destination: " + destination.ToString() + ", distance: " + distance, LogLevel.NORMAL);
        
        // Consider reached if within 10 meters
        bool hasReached = distance < 10;
        // Print(("[VEHICLE_DEBUG] HasVehicleReachedDestination - Has reached: " + hasReached, LogLevel.NORMAL);
        return hasReached;
    }
    
    // Create waypoint only if the vehicle doesn't already have an active one or has reached its current destination
    static void UpdateVehicleWaypoint(Vehicle vehicle, IA_AiGroup aiGroup, vector destination)
    {
         //Print("[VEHICLE_DEBUG] UpdateVehicleWaypoint called", LogLevel.NORMAL);
        if (!vehicle || !aiGroup)
        {
             //Print("[VEHICLE_DEBUG] UpdateVehicleWaypoint - vehicle or aiGroup is NULL", LogLevel.WARNING);
            return;
        }
            
        // Get vehicle position
        vector vehiclePos = vehicle.GetOrigin();
         //Print("[VEHICLE_DEBUG] Vehicle at " + vehiclePos.ToString() + ", destination: " + destination.ToString(), LogLevel.NORMAL);
        
        // Check if we need to find a road for this destination
        bool shouldFindRoad = true;
        
        // We might already have a road-based destination if it was set during vehicle spawning
        // Check the distance to verify if this destination seems to be a road
        array<IEntity> roadEntities = {};
        RoadEntitiesCallback callback = new RoadEntitiesCallback(roadEntities);
        
        // Quick check to see if destination is already on a road by checking a small radius
        GetGame().GetWorld().QueryEntitiesBySphere(destination, 10, callback.OnEntityFound, FilterRoadEntities, EQueryEntitiesFlags.STATIC);
        if (!roadEntities.IsEmpty())
        {
            // Destination seems to already be on a road
             //Print("[VEHICLE_DEBUG] Destination already appears to be on a road", LogLevel.NORMAL);
            shouldFindRoad = false;
        }
        
        // If the group has no active waypoint or has reached the destination
        bool hasActiveWaypoint = aiGroup.HasActiveWaypoint();
        bool hasReachedDestination = HasVehicleReachedDestination(vehicle, destination);
        
         //Print("[VEHICLE_DEBUG] UpdateVehicleWaypoint - hasActiveWaypoint: " + hasActiveWaypoint + ", hasReachedDestination: " + hasReachedDestination, LogLevel.NORMAL);
        
        if (!hasActiveWaypoint || hasReachedDestination)
        {
            //Print("[VEHICLE_DEBUG] Vehicle needs new waypoint - creating one at " + destination.ToString(), LogLevel.NORMAL);
            
            // If we need to find a road, update the destination
            if (shouldFindRoad)
            {
                float searchRadius = 300;
                
                if (destination != vector.Zero)
                {
                    // Set search radius based on distance to destination
                    float distToDest = vector.Distance(vehiclePos, destination);
                    
                    // Use either a minimum of 100m or half the distance to destination
                    searchRadius = distToDest * 0.5;
                    if (searchRadius < 100)
                        searchRadius = 100;
                    
                    //Print("[VEHICLE_DEBUG] Searching for road near destination with radius " + searchRadius, LogLevel.NORMAL);
                    
                    // Find a road near the destination
                    vector roadDestination = FindRandomRoadPointForVehiclePatrol(destination, searchRadius, m_currentActiveGroup);
                    
                    // If we found a valid road position, use it
                    if (roadDestination != vector.Zero)
                    {
                        //Print("[VEHICLE_DEBUG] Found road at " + roadDestination.ToString() + ", using as waypoint destination", LogLevel.NORMAL);
                        destination = roadDestination;
                    }
                    else
                    {
                        //Print("[VEHICLE_DEBUG] No valid road found near destination, using original destination", LogLevel.WARNING);
                    }
                }
            }
            
            CreateWaypointForVehicleUsingGroup(vehicle, destination, aiGroup);
        }
        else
        {
            //Print("[VEHICLE_DEBUG] Vehicle has active waypoint and hasn't reached destination - skipping", LogLevel.NORMAL);
        }
    }
    
    // Clear all vehicle reservations
    static void ClearAllReservations()
    {
        m_vehicleReservations.Clear();
    }
    
    // Clear reservations for a specific group
    static void ClearGroupReservations(IA_AiGroup group)
    {
        if (!group)
            return;
            
        array<IEntity> vehiclesToClear = {};
        
        // Find all vehicles reserved by this group
        foreach (IEntity vehicle, IA_AiGroup reservingGroup : m_vehicleReservations)
        {
            if (reservingGroup == group)
                vehiclesToClear.Insert(vehicle);
        }
        
        // Remove the reservations
        foreach (IEntity vehicle : vehiclesToClear)
        {
            m_vehicleReservations.Remove(vehicle);
        }
    }
    
    // Helper methods to spawn specific vehicle types
    
    // Spawn a civilian car
    static Vehicle SpawnCivilianCar(IA_Faction faction, vector position, Faction civFaction)
    {
        return SpawnVehicle(faction, position, civFaction);
    }
    
    // Spawn a civilian truck
    static Vehicle SpawnCivilianTruck(IA_Faction faction, vector position, Faction civFaction)
    {
        return SpawnVehicle(faction, position, civFaction);
    }
    
    // Spawn a military transport vehicle
    static Vehicle SpawnMilitaryTransport(IA_Faction faction, vector position, Faction AreaFaction)
    {
        return SpawnVehicle(faction, position, AreaFaction);
    }
    
    // Spawn a military patrol vehicle
    static Vehicle SpawnMilitaryPatrol(IA_Faction faction, vector position, Faction AreaFaction)
    {
        return SpawnVehicle(faction, position, AreaFaction);
    }
    
    // Spawn a military APC
    static Vehicle SpawnMilitaryAPC(IA_Faction faction, vector position, Faction AreaFaction)
    {
        return SpawnVehicle(faction, position, AreaFaction);
    }
    
    // Spawn a random civilian vehicle (car or truck)
    static Vehicle SpawnRandomCivilianVehicle(IA_Faction faction, vector position, Faction AreaFaction)
    {
        return SpawnRandomVehicle(faction, true, false, position, AreaFaction);
    }
    
    // Spawn a random military vehicle (transport, patrol, or APC)
    static Vehicle SpawnRandomMilitaryVehicle(IA_Faction faction, vector position, Faction AreaFaction)
    {
        return SpawnRandomVehicle(faction, false, true, position, AreaFaction);
    }
    
    // Spawn vehicles at all available spawn points for the active group with occupants
    static void SpawnVehiclesAtAllSpawnPoints(IA_Faction faction, Faction AreaFaction)
    {
        //// Print(("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Spawning vehicles for faction " + faction, LogLevel.NORMAL);
        
        // If an active group is set, only spawn in that group
        int targetGroup = m_currentActiveGroup;
        
        // Get all spawn points that match the group
        array<IA_VehicleSpawnPoint> spawnPoints = IA_VehicleSpawnPoint.GetSpawnPointsByGroup(targetGroup);
        if (spawnPoints.IsEmpty())
        {
            //// Print(("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: No spawn points found in group " + targetGroup, LogLevel.WARNING);
            return;
        }
        
        //// Print(("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Found " + spawnPoints.Count() + " spawn points in group " + targetGroup, LogLevel.NORMAL);
        
        // Spawn vehicles at each point
        foreach (IA_VehicleSpawnPoint spawnPoint : spawnPoints)
        {
            if (!spawnPoint.CanSpawnVehicle())
                continue;
                
            // Spawn a random vehicle
            Vehicle vehicle = spawnPoint.SpawnRandomVehicle(faction, AreaFaction);
            if (!vehicle)
                continue;
                
            //// Print(("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Successfully spawned vehicle at " + spawnPoint.GetOrigin().ToString(), LogLevel.NORMAL);
            
            // Get the area center point to use as a reference
            vector areaCenter = IA_AreaMarker.CalculateGroupCenterPoint(m_currentActiveGroup);
            float areaRadius = IA_AreaMarker.CalculateGroupRadius(m_currentActiveGroup);
            
            // Directly place units inside the vehicle and set them in motion
            PlaceUnitsInVehicle(vehicle, faction, areaCenter, IA_Game.CurrentAreaInstance, AreaFaction);
        }
    }
    
    // GetCompartments is not recursive. Child-entity turrets (technicals) are
    // missed unless we walk the hierarchy. Fill order is Pilot, Turret, Cargo.
    static void CollectVehicleCrewSeats(IEntity vehicle, notnull array<BaseCompartmentSlot> outSlots, bool skipOccupied)
    {
        outSlots.Clear();
        if (!vehicle)
            return;

        ref array<BaseCompartmentSlot> pilots = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> turrets = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> cargo = new array<BaseCompartmentSlot>();
        CollectVehicleCrewSeatsRecursive(vehicle, pilots, turrets, cargo, skipOccupied);

        int i;
        int n;
        n = pilots.Count();
        for (i = 0; i < n; i++)
        {
            outSlots.Insert(pilots[i]);
        }
        n = turrets.Count();
        for (i = 0; i < n; i++)
        {
            outSlots.Insert(turrets[i]);
        }
        n = cargo.Count();
        for (i = 0; i < n; i++)
        {
            outSlots.Insert(cargo[i]);
        }
    }

    static void CollectVehicleSeatsByRole(IEntity vehicle, notnull array<BaseCompartmentSlot> pilots, notnull array<BaseCompartmentSlot> turrets, notnull array<BaseCompartmentSlot> cargo, bool skipOccupied)
    {
        pilots.Clear();
        turrets.Clear();
        cargo.Clear();
        if (!vehicle)
            return;

        CollectVehicleCrewSeatsRecursive(vehicle, pilots, turrets, cargo, skipOccupied);
    }

    static bool VehicleCrewSeatsHaveTurret(notnull array<BaseCompartmentSlot> slots)
    {
        int n = slots.Count();
        int i;
        for (i = 0; i < n; i++)
        {
            BaseCompartmentSlot slot = slots[i];
            if (slot && slot.GetType() == ECompartmentType.TURRET)
                return true;
        }

        return false;
    }

    protected static void CollectVehicleCrewSeatsRecursive(IEntity ent, notnull array<BaseCompartmentSlot> pilots, notnull array<BaseCompartmentSlot> turrets, notnull array<BaseCompartmentSlot> cargo, bool skipOccupied)
    {
        if (!ent)
            return;

        BaseCompartmentManagerComponent mgr = BaseCompartmentManagerComponent.Cast(ent.FindComponent(BaseCompartmentManagerComponent));
        if (mgr)
        {
            array<BaseCompartmentSlot> slots = {};
            mgr.GetCompartments(slots);
            int n = slots.Count();
            int i;
            for (i = 0; i < n; i++)
            {
                BaseCompartmentSlot slot = slots[i];
                if (!slot)
                    continue;
                if (skipOccupied && slot.GetOccupant())
                    continue;
                if (!slot.IsCompartmentAccessible())
                    continue;

                ECompartmentType type = slot.GetType();
                if (type == ECompartmentType.PILOT)
                    pilots.Insert(slot);
                else if (type == ECompartmentType.TURRET)
                    turrets.Insert(slot);
                else if (type == ECompartmentType.CARGO)
                    cargo.Insert(slot);
            }
        }

        IEntity child = ent.GetChildren();
        while (child)
        {
            CollectVehicleCrewSeatsRecursive(child, pilots, turrets, cargo, skipOccupied);
            child = child.GetSibling();
        }
    }

    // Add units to a vehicle and assign orders
    static IA_AiGroup PlaceUnitsInVehicle(Vehicle vehicle, IA_Faction faction, vector destination, IA_AreaInstance areaInstance, Faction AreaFaction)
    {
       //// Print(("[DEBUG_VEHICLE_UNITS] PlaceUnitsInVehicle called for vehicle: " + vehicle + ", faction: " + faction, LogLevel.NORMAL);
        
        if (!vehicle || !areaInstance)
        {
           //// Print(("[DEBUG_VEHICLE_UNITS] PlaceUnitsInVehicle: Missing vehicle or area instance", LogLevel.WARNING);
            return null;
        }
        
        array<BaseCompartmentSlot> usableCompartments = {};
        CollectVehicleCrewSeats(vehicle, usableCompartments, true);
        int compartmentCount = usableCompartments.Count();
        
        if (compartmentCount == 0)
        {
           //// Print(("[DEBUG_VEHICLE_UNITS] PlaceUnitsInVehicle: Vehicle has no compartments", LogLevel.WARNING);
            return null;
        }
        
        float aiScaleFactor = IA_Game.GetAIScaleFactor();
        int scaledCompartmentCount = Math.Round(compartmentCount * aiScaleFactor);
        if (scaledCompartmentCount < 1)
            scaledCompartmentCount = 1;

        if (faction == IA_Faction.CIV)
        {
            scaledCompartmentCount = scaledCompartmentCount * 0.6;
            if (scaledCompartmentCount < 1)
                scaledCompartmentCount = 1;
        }
        else if (VehicleCrewSeatsHaveTurret(usableCompartments) && scaledCompartmentCount < 2 && compartmentCount >= 2)
        {
            scaledCompartmentCount = 2;
        }
        
        if (scaledCompartmentCount > compartmentCount)
            scaledCompartmentCount = compartmentCount;

        IA_AiGroup crewGroup;
        IA_AiGroup passengerGroup;

        if (faction == IA_Faction.CIV)
        {
            crewGroup = IA_AiGroup.CreateCivilianGroupForVehicle(vehicle, scaledCompartmentCount);
            if (!crewGroup)
                return null;

            if (!crewGroup.IsSpawned())
                crewGroup.Spawn();

            crewGroup.AssignVehicle(vehicle, destination);
            GetGame().GetCallqueue().CallLater(_PlaceSpawnedUnitsInVehicle, 12500, false, vehicle, crewGroup, passengerGroup, destination);
            return crewGroup;
        }

        ref array<BaseCompartmentSlot> pilots = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> turrets = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> cargo = new array<BaseCompartmentSlot>();
        CollectVehicleSeatsByRole(vehicle, pilots, turrets, cargo, true);

        int crewSeats = pilots.Count() + turrets.Count();
        int cargoSeats = cargo.Count();
        int crewUnits = crewSeats;
        if (crewUnits < 1)
            crewUnits = 1;
        if (crewUnits > scaledCompartmentCount && cargoSeats == 0)
            crewUnits = scaledCompartmentCount;
        if (crewUnits > compartmentCount)
            crewUnits = compartmentCount;

        int cargoUnits = 0;
        if (cargoSeats > 0)
        {
            cargoUnits = scaledCompartmentCount - crewUnits;
            if (cargoUnits < 0)
                cargoUnits = 0;
            if (cargoUnits > cargoSeats)
                cargoUnits = cargoSeats;
        }

        crewGroup = IA_AiGroup.CreateGroupForVehicle(vehicle, faction, crewUnits, AreaFaction, false);
        if (!crewGroup)
            return null;

        crewGroup.MarkAsVehicleCrew();
        areaInstance.AddMilitaryGroup(crewGroup);
        if (!crewGroup.IsSpawned())
            crewGroup.Spawn();
        crewGroup.AssignVehicle(vehicle, destination);

        if (cargoUnits > 0)
        {
            passengerGroup = IA_AiGroup.CreateGroupForVehicle(vehicle, faction, cargoUnits, AreaFaction, true);
            if (passengerGroup)
            {
                passengerGroup.ConfigureAsVehiclePassengers(crewGroup, vehicle, destination);
                areaInstance.AddMilitaryGroup(passengerGroup);
                if (!passengerGroup.IsSpawned())
                    passengerGroup.Spawn();
                crewGroup.SetLinkedPassengerGroup(passengerGroup);
            }
        }

        GetGame().GetCallqueue().CallLater(_PlaceSpawnedUnitsInVehicle, 12500, false, vehicle, crewGroup, passengerGroup, destination);
        return crewGroup;
    }
    
    // Find a random road entity in the zone around the given position
    static vector FindRandomRoadEntityInZone(vector position, float maxDistance = DEFAULT_INITIAL_ROAD_SEARCH_RADIUS, int groupNumber = -1, float minSearchRadius = 20.0)
    {
        // Print(('[VEHICLE_DEBUG] FindRandomRoadEntityInZone called with position: " + position + ", maxDist: " + maxDistance + ", groupNum: " + groupNumber + ", minRadius: " + minSearchRadius, LogLevel.NORMAL);
        
        vector originalPosition = position; 
        float initialMaxDistance = maxDistance; 

        if (position == vector.Zero)
        {
            if (groupNumber >= 0)
            {
                vector groupCenter = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
                if (groupCenter != vector.Zero)
                {
                    position = groupCenter; 
                }
                else
                {
                    return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
                }
            }
            else
            {
                return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
            }
        }
        
        vector searchCenter = position;
        if (searchCenter != vector.Zero) 
        {
            vector randomOffset = IA_Game.rng.GenerateRandomPointInRadius(1, 20.0, vector.Zero); 
            searchCenter[0] = searchCenter[0] + randomOffset[0];
            searchCenter[2] = searchCenter[2] + randomOffset[2];
        }

        float currentSearchRadius = initialMaxDistance; 
        currentSearchRadius = Math.Max(currentSearchRadius, minSearchRadius); // Ensure initial radius respects minSearchRadius
        
        // Adjust initial searchRadius based on groupNumber and attack status (existing logic)
        if (groupNumber >= 0)
        {
            float groupRadiusCalc = IA_AreaMarker.CalculateGroupRadius(groupNumber);
            if (groupRadiusCalc > 0)
            {
                // Check against DEFAULT_INITIAL_ROAD_SEARCH_RADIUS to see if maxDistance was the default
                if (initialMaxDistance == DEFAULT_INITIAL_ROAD_SEARCH_RADIUS || currentSearchRadius <= 10) 
                {
                    currentSearchRadius = Math.Max(currentSearchRadius, groupRadiusCalc * 0.8);
                }
            }
        }
        if (IA_Game.CurrentAreaInstance && IA_Game.CurrentAreaInstance.IsUnderAttack())
        {
            vector areaOrigin = IA_Game.CurrentAreaInstance.m_area.GetOrigin();
            if (areaOrigin != vector.Zero) 
            {
                float defensiveRadius = IA_Game.CurrentAreaInstance.m_area.GetRadius() * 0.5; 
                if (defensiveRadius < 100) defensiveRadius = 100; 
                if (defensiveRadius < currentSearchRadius) 
                {
                    currentSearchRadius = defensiveRadius;
                }
            }
        }
        currentSearchRadius = Math.Max(currentSearchRadius, minSearchRadius); // Re-ensure minSearchRadius after adjustments

        int maxRetries = 5; 
        float radiusIncrement = 60; 
        float absoluteMaxSearchRadius = 1200;

        // Print(string.Format("[VEHICLE_DEBUG] Initial attempt to find road with searchCenter: %1, initial adjusted radius: %.1f (min requested: %.1f)", searchCenter.ToString(), currentSearchRadius, minSearchRadius), LogLevel.DEBUG);

        for (int attempt = 0; attempt <= maxRetries; attempt++)
        {
            if (attempt > 0) 
            {
                currentSearchRadius += radiusIncrement*(attempt+1);
                currentSearchRadius = Math.Max(currentSearchRadius, minSearchRadius); // Ensure incremented radius also respects min
                if (currentSearchRadius > absoluteMaxSearchRadius)
                {
                    break; 
                }
            }
            
            AIWorld aiWorld = GetGame().GetAIWorld();
            SCR_AIWorld scr_aiWorld = SCR_AIWorld.Cast(aiWorld);
            if (!scr_aiWorld) {
                 if (attempt == maxRetries) break; 
                 continue;
            }
            RoadNetworkManager roadMngr = scr_aiWorld.GetRoadNetworkManager();
            if (!roadMngr) {
                if (attempt == maxRetries) break; 
                continue;
            }
            
            array<BaseRoad> Roads = {};
            vector vectorAABBMin = Vector(searchCenter[0] - currentSearchRadius, searchCenter[1] - 1000, searchCenter[2] - currentSearchRadius);
            vector vectorAABBMax = Vector(searchCenter[0] + currentSearchRadius, searchCenter[1] + 1000, searchCenter[2] + currentSearchRadius);
            roadMngr.GetRoadsInAABB(vectorAABBMin, vectorAABBMax, Roads);
            
            if (Roads.IsEmpty()){
                if (attempt == maxRetries) break; 
                continue; 
            }
            
            array<vector> validPoints = {};
            array<vector> pointsOnCurrentRoad = {};

            foreach (BaseRoad road : Roads)
            {
                pointsOnCurrentRoad.Clear();
                road.GetPoints(pointsOnCurrentRoad);
                
                if (pointsOnCurrentRoad.IsEmpty())
                    continue;

                foreach (vector point : pointsOnCurrentRoad)
                {
                    float distSq = vector.DistanceSq(searchCenter, point);
                    float currentRadiusSq = currentSearchRadius * currentSearchRadius;
                    if (distSq <= currentRadiusSq) 
                    {
                        validPoints.Insert(point);
                    }
                }
            }

            if (!validPoints.IsEmpty())
            {
                int randomIndex = Math.RandomInt(0, validPoints.Count() - 1);
                vector selectedPoint = validPoints[randomIndex];
                return selectedPoint;
            }
            if (attempt == maxRetries) break; 
        }
        return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
    }
    
    // Filter function for road entities
    static bool FilterRoadEntities(IEntity entity)
    {
        if (!entity)
            return false;
	
            //if(entity.ClassName().Contains("RoadEntity"))
				//// Print(("Found Entity!" + entity.ClassName(),LogLevel.NORMAL);
        // Check if entity is a road
        return (entity.ClassName() == "RoadEntity");
    }
    
    // Find a fallback point that's suitable for navigation if no roads are found
    static vector FindFallbackNavigationPoint(vector position, float maxDistance = 300, int groupNumber = -1)
    {
        //// Print(("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Finding fallback point near " + position.ToString(), LogLevel.NORMAL);
        
        // If groupNumber is provided, use it to calculate a dynamic search radius
        if (groupNumber >= 0)
        {
            // Calculate the radius based on the group's actual size
            float groupRadius = IA_AreaMarker.CalculateGroupRadius(groupNumber);
            
            // Use the group radius if available, or fall back to the provided maxDistance
            if (groupRadius > 0)
            {
                // Use the calculated radius plus a small margin
                maxDistance = groupRadius * 1.2;
                //// Print(("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Using dynamic radius of " + maxDistance + " based on group " + groupNumber, LogLevel.NORMAL);
            }
        }
        
        // Try up to 5 times to find a relatively flat area
        for (int i = 0; i < 5; i++)
        {
            // Generate a random point in the radius
            vector randPoint = IA_Game.rng.GenerateRandomPointInRadius(1, maxDistance, position);
            
            // Check if the slope is reasonable for vehicles
            // Sample 4 points around the position to calculate slope
            float distCheck = 5.0; // Check 5m in each direction
            float y1 = GetGame().GetWorld().GetSurfaceY(randPoint[0] + distCheck, randPoint[2]);
            float y2 = GetGame().GetWorld().GetSurfaceY(randPoint[0] - distCheck, randPoint[2]);
            float y3 = GetGame().GetWorld().GetSurfaceY(randPoint[0], randPoint[2] + distCheck);
            float y4 = GetGame().GetWorld().GetSurfaceY(randPoint[0], randPoint[2] - distCheck);
            
            // Calculate absolute differences
            float diff1 = y1 - y2;
            if (diff1 < 0) diff1 = -diff1; // Manual absolute value
            
            float diff2 = y3 - y4;
            if (diff2 < 0) diff2 = -diff2; // Manual absolute value
            
            // Calculate maximum slope
            float slope1 = diff1 / (2 * distCheck);
            float slope2 = diff2 / (2 * distCheck);
            float maxSlope;
            if (slope1 > slope2)
                maxSlope = slope1;
            else
                maxSlope = slope2;
            
            // If slope is reasonable (less than 30 degrees, approximately 0.577 in tangent)
            if (maxSlope < 0.4)
            {
                //// Print(("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Found suitable point at " + randPoint.ToString(), LogLevel.NORMAL);
                return randPoint;
            }
        }
        
        // If all attempts failed, just return a random point in a smaller radius
        vector fallbackPoint = IA_Game.rng.GenerateRandomPointInRadius(1, maxDistance/2, position);
        
        //// Print(("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Using simple fallback at " + fallbackPoint.ToString(), LogLevel.WARNING);
        return fallbackPoint;
    }
    
    protected static void SeatCharactersInSlots(Vehicle vehicle, notnull array<SCR_ChimeraCharacter> characters, notnull array<BaseCompartmentSlot> slots)
    {
        int charIndex = 0;
        int charCount = characters.Count();
        int slotCount = slots.Count();
        int i;
        for (i = 0; i < slotCount && charIndex < charCount; i++)
        {
            BaseCompartmentSlot compartment = slots[i];
            if (!compartment || compartment.GetOccupant())
                continue;

            SCR_ChimeraCharacter occupant = characters[charIndex];
            charIndex = charIndex + 1;
            if (!occupant)
                continue;

            CompartmentAccessComponent accessComponent = CompartmentAccessComponent.Cast(occupant.FindComponent(CompartmentAccessComponent));
            if (accessComponent)
                accessComponent.GetInVehicle(vehicle, compartment, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, true);
        }
    }

    // Helper method called after a delay to place units in the vehicle
    private static void _PlaceSpawnedUnitsInVehicle(Vehicle vehicle, IA_AiGroup crewGroup, IA_AiGroup passengerGroup, vector destination)
    {
        if (!vehicle || !crewGroup)
            return;

        array<SCR_ChimeraCharacter> crewCharacters = crewGroup.GetGroupCharacters();
        if (crewCharacters.IsEmpty())
            return;

        if (crewGroup.GetFaction() == IA_Faction.CIV || !passengerGroup)
        {
            array<BaseCompartmentSlot> usableCompartments = {};
            CollectVehicleCrewSeats(vehicle, usableCompartments, true);
            SeatCharactersInSlots(vehicle, crewCharacters, usableCompartments);

            if (crewGroup.GetFaction() == IA_Faction.CIV)
            {
                crewGroup.ForceDrivingState(true);
                GetGame().GetCallqueue().CallLater(CheckCiviliansInVehicle, 5000, true, vehicle, crewGroup, destination);
            }

            UpdateVehicleWaypoint(vehicle, crewGroup, destination);
            return;
        }

        ref array<BaseCompartmentSlot> pilots = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> turrets = new array<BaseCompartmentSlot>();
        ref array<BaseCompartmentSlot> cargo = new array<BaseCompartmentSlot>();
        CollectVehicleSeatsByRole(vehicle, pilots, turrets, cargo, true);

        ref array<BaseCompartmentSlot> crewSlots = new array<BaseCompartmentSlot>();
        int n;
        int i;
        n = pilots.Count();
        for (i = 0; i < n; i++)
        {
            crewSlots.Insert(pilots[i]);
        }
        n = turrets.Count();
        for (i = 0; i < n; i++)
        {
            crewSlots.Insert(turrets[i]);
        }

        SeatCharactersInSlots(vehicle, crewCharacters, crewSlots);

        array<SCR_ChimeraCharacter> passengerCharacters = passengerGroup.GetGroupCharacters();
        if (!passengerCharacters.IsEmpty())
            SeatCharactersInSlots(vehicle, passengerCharacters, cargo);

        passengerGroup.IssuePassengerMountHold();
        UpdateVehicleWaypoint(vehicle, crewGroup, destination);
    }
    
    // Helper method to periodically check if civilians are still in their vehicle and order them back in if not
    private static void CheckCiviliansInVehicle(Vehicle vehicle, IA_AiGroup aiGroup, vector destination)
    {
        // Stop checking if the vehicle or group is gone
        if (!vehicle || !aiGroup || !aiGroup.IsSpawned())
        {
            // Cancel this recurring check
            GetGame().GetCallqueue().Remove(CheckCiviliansInVehicle);
            return;
        }
        
        // Only process for civilian groups
        if (aiGroup.GetFaction() != IA_Faction.CIV)
            return;
        
        // Get all characters in the group
        array<SCR_ChimeraCharacter> characters = aiGroup.GetGroupCharacters();
        if (characters.IsEmpty())
            return;
        
        // Check if any are outside the vehicle
        bool anyOutside = false;
        foreach (SCR_ChimeraCharacter character : characters)
        {
            if (!character.IsInVehicle())
            {
                anyOutside = true;
                break;
            }
        }
        
        // If anyone is outside, tell them to get back in
        if (anyOutside)
        {
            // Make sure driving state is still correct
            aiGroup.ForceDrivingState(true);
            
            // Clear orders and create a GetInVehicle order
            aiGroup.RemoveAllOrders();
            aiGroup.AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle, true);
            
            // After a delay, update the waypoint again so they continue to their destination
            GetGame().GetCallqueue().CallLater(UpdateWaypointAfterEntry, 5000, false, vehicle, aiGroup, destination);
        }
    }
    
    // After civilians re-enter the vehicle, restore their waypoint
    private static void UpdateWaypointAfterEntry(Vehicle vehicle, IA_AiGroup aiGroup, vector destination)
    {
        if (!vehicle || !aiGroup || !aiGroup.IsSpawned())
            return;
        
        // Ensure they're still flagged as driving
        aiGroup.ForceDrivingState(true);
        
        // Update their waypoint to continue the journey
        UpdateVehicleWaypoint(vehicle, aiGroup, destination);
    }
    
    // Helper method to create a waypoint for a vehicle to drive to using a known AI group
    static void CreateWaypointForVehicleUsingGroup(Vehicle vehicle, vector destination, IA_AiGroup aiGroup)
    {
        //Print("[VEHICLE_DEBUG] CreateWaypointForVehicleUsingGroup called", LogLevel.NORMAL);
        
        if (!vehicle || !aiGroup)
        {
            //Print("[VEHICLE_DEBUG] CreateWaypointForVehicleUsingGroup - vehicle or aiGroup is NULL", LogLevel.WARNING);
            return;
        }
            
        // Find the nearest road to the destination
        vector vehiclePos = vehicle.GetOrigin();
        float searchRadius = vector.Distance(vehiclePos, destination);
        // Ensure minimum search radius
        if (searchRadius < 100)
            searchRadius = 100;
        
        // Find a road near the destination
        vector roadDestination = FindRandomRoadPointForVehiclePatrol(destination, searchRadius, IA_VehicleManager.GetActiveGroup());
        
        // If we found a valid road position, use it instead of the original destination
        if (roadDestination != vector.Zero)
        {
            //Print("[VEHICLE_DEBUG] Using road position for waypoint instead of original destination", LogLevel.NORMAL);
            destination = roadDestination;
        }
        else
        {
            //Print("[VEHICLE_DEBUG] No road found near destination, using original destination", LogLevel.WARNING);
        }
        
         //Print("[VEHICLE_DEBUG] Creating waypoint at " + destination.ToString(), LogLevel.NORMAL);
        
        // Create a move waypoint at the destination
        ResourceName waypointResource = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
        Resource res = Resource.Load(waypointResource);
        if (!res)
        {
            //Print("[VEHICLE_DEBUG] CreateWaypointForVehicleUsingGroup - Failed to load waypoint resource", LogLevel.ERROR);
            return;
        }
        
        EntitySpawnParams params = EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
        params.Transform[3] = destination;
        
        IEntity waypointEntity = GetGame().SpawnEntityPrefab(res, null, params);
        if (!waypointEntity)
        {
            //Print("[VEHICLE_DEBUG] CreateWaypointForVehicleUsingGroup - Failed to spawn waypoint entity", LogLevel.ERROR);
            return;
        }
        
        //Print("[VEHICLE_DEBUG] Waypoint entity spawned successfully", LogLevel.NORMAL);
        
        // Add the waypoint to the group
        SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(waypointEntity);
        if (waypoint)
        {
            waypoint.SetPriorityLevel(IA_AiGroup.WP_PRIORITY_DRIVE);
            // Print(("[VEHICLE_DEBUG] Setting waypoint priority to 2000", LogLevel.NORMAL);
            
            // Check if the group is in defending state before adding vehicle waypoint
            if (aiGroup.GetTacticalState() == IA_GroupTacticalState.Defending)
            {
                Print(string.Format("[VEHICLE_DEBUG] WARNING: Adding vehicle Move waypoint to group %1 that is in Defending state!", aiGroup), LogLevel.WARNING);
            }
            
            aiGroup.AddWaypoint(waypoint);
            // Print(("[VEHICLE_DEBUG] Added waypoint to group successfully", LogLevel.NORMAL);
        }
        else
        {
            // Print(("[VEHICLE_DEBUG] Failed to cast waypoint entity to SCR_AIWaypoint", LogLevel.ERROR);
        }
    }
    
    // Helper method to create a waypoint for a vehicle to drive to
    static void CreateWaypointForVehicle(Vehicle vehicle, SCR_ChimeraCharacter driver, vector destination)
    {
        //// Print(("[DEBUG] IA_VehicleManager.CreateWaypointForVehicle: This method is deprecated. Use CreateWaypointForVehicleUsingGroup instead", LogLevel.WARNING);
        return;
    }
    
    // Check if a player can pilot a specific vehicle compartment (e.g. helicopter pilot seat)
    static bool CanPlayerPilotVehicle(IEntity user, BaseCompartmentSlot compartment, out string reason)
    {
		// Get the config and check if role restrictions are enforced
        if (!IA_MissionInitializer.AreRoleRestrictionsEnforced())
        {
            return true; // Role restrictions are disabled, so always allow
        }
		
        reason = ""; // Default to no reason
		SCR_ChimeraCharacter chimeraCharacter = SCR_ChimeraCharacter.Cast(user);
        // Check if it's a pilot seat
        if (compartment.GetType() == ECompartmentType.PILOT)
        {
            IEntity vehicleEntity = SCR_EntityHelper.GetMainParent(compartment.GetOwner(), true);
            if (vehicleEntity)
            {
                SCR_EditableVehicleComponent editableVehicle = SCR_EditableVehicleComponent.Cast(vehicleEntity.FindComponent(SCR_EditableVehicleComponent));
                if (editableVehicle)
                {
                    SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
                    if (!factionManager)
                    {
                        //Print("No Faction Manager Found in CanPlayerPilotVehicle!", LogLevel.ERROR);
                        reason = "Internal error: Faction Manager not found.";
                        return false; // Cannot proceed without faction manager
                    }

                    SCR_EntityCatalogEntry catalogEntry;
                    array<Faction> arrayOfFactions = {};
                    factionManager.GetFactionsList(arrayOfFactions);
                    foreach(Faction thisFaction : arrayOfFactions)
                    {
                        SCR_Faction scrFaction = SCR_Faction.Cast(thisFaction);
                        if(!scrFaction)
                            continue;
                        SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
                        catalogEntry = entityCatalog.GetEntryWithPrefab(editableVehicle.GetPrefab());
                        if(catalogEntry)
                        {
                            break;
                        }
                    }
                    
                    if(!catalogEntry)
                    {
                        reason = "Internal error: Vehicle catalog entry not found.";
                        return false; // Cannot proceed without catalog entry
                    }
                            
                    array<EEditableEntityLabel> vehicleLabels = {}; 
                    catalogEntry.GetEditableEntityLabels(vehicleLabels);
                    bool isHelicopter = false;
					bool isArmor = false;
                    foreach (EEditableEntityLabel label : vehicleLabels)
                    {
                        if (label == EEditableEntityLabel.VEHICLE_HELICOPTER)
                        {
                            isHelicopter = true;
                            break;
                        } else if (label == EEditableEntityLabel.VEHICLE_APC)
                        {
                            isArmor = true;
                            break;
                        }
                    }

                    if (isHelicopter || isArmor)
                    {
                        PlayerManager playerManager = GetGame().GetPlayerManager();
                        if (!playerManager)
                        {
                            reason = "Internal error: Player Manager not found.";
                            return false; // Should not happen
                        }
                            
                        int playerId = playerManager.GetPlayerIdFromControlledEntity(user);
                        if (playerId == 0) // PlayerID 0 is invalid
                        {
                            reason = "Internal error: Invalid Player ID.";
                            return false; 
                        }
                            
                        IA_RoleManager roleManager = IA_RoleManager.GetInstance();
                        if (roleManager)
                        {
							IA_PlayerRole playerRole = chimeraCharacter.m_eReplicatedRole;

							if(isHelicopter){
	                            if (playerRole != IA_PlayerRole.PILOT)
	                            {
	                                reason = "Must be a Pilot to Fly!";
	                                return false; // Not a pilot, cannot enter helicopter pilot seat
	                            }
							} else if(isArmor){
	                            if (playerRole != IA_PlayerRole.CREWMAN)
	                            {
	                                reason = "Must be a Crewman to Drive Armored Vehicles!";
	                                return false; // Not a pilot, cannot enter helicopter pilot seat
	                            }
							}
                        }
                        else
                        {
                            reason = "Internal error: Role Manager not found.";
                            return false; // Cannot check role without role manager
                        }
                    }
                }
            }
        }
        
        // If all checks pass or don't apply
        return true;
    }

    // Find a random road point specifically for vehicle patrols (duplicates FindRandomRoadEntityInZone initially)
    static vector FindRandomRoadPointForVehiclePatrol(vector position, float maxDistance = DEFAULT_INITIAL_ROAD_SEARCH_RADIUS, int groupNumber = -1)
    {
        // Print(('[VEHICLE_DEBUG] FindRandomRoadPointForVehiclePatrol called with position: " + position + ", maxDist: " + maxDistance + ", groupNum: " + groupNumber, LogLevel.NORMAL);
        
        vector originalPosition = position; 
        float initialMaxDistance = maxDistance; 

        if (position == vector.Zero)
        {
            if (groupNumber >= 0)
            {
                vector groupCenter = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
                if (groupCenter != vector.Zero)
                {
                    position = groupCenter; 
                }
                else
                {
                    return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
                }
            }
            else
            {
                return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
            }
        }
        
        vector searchCenter = position;
        if (searchCenter != vector.Zero) 
        {
            vector randomOffset = IA_Game.rng.GenerateRandomPointInRadius(1, 20.0, vector.Zero); 
            searchCenter[0] = searchCenter[0] + randomOffset[0];
            searchCenter[2] = searchCenter[2] + randomOffset[2];
        }

        float currentSearchRadius = initialMaxDistance; 
        
        if (groupNumber >= 0)
        {
            float groupRadiusCalc = IA_AreaMarker.CalculateGroupRadius(groupNumber);
            if (groupRadiusCalc > 0)
            {
                if (currentSearchRadius <= 10 || currentSearchRadius == DEFAULT_INITIAL_ROAD_SEARCH_RADIUS) 
                {
                    currentSearchRadius = Math.Max(currentSearchRadius, groupRadiusCalc * 0.8);
                }
            }
        }
        if (IA_Game.CurrentAreaInstance && IA_Game.CurrentAreaInstance.IsUnderAttack())
        {
            vector areaOrigin = IA_Game.CurrentAreaInstance.m_area.GetOrigin();
            if (areaOrigin != vector.Zero) 
            {
                float defensiveRadius = IA_Game.CurrentAreaInstance.m_area.GetRadius() * 0.5; 
                if (defensiveRadius < 100) defensiveRadius = 100; 
                if (defensiveRadius < currentSearchRadius) 
                {
                    currentSearchRadius = defensiveRadius;
                }
            }
        }

        int maxRetries = 3; 
        float radiusIncrement = 150; 
        float absoluteMaxSearchRadius = 1200;

        for (int attempt = 0; attempt <= maxRetries; attempt++)
        {
            if (attempt > 0) 
            {
                currentSearchRadius += radiusIncrement;
                if (currentSearchRadius > absoluteMaxSearchRadius)
                {
                    break; 
                }
            }
            
            AIWorld aiWorld = GetGame().GetAIWorld();
            SCR_AIWorld scr_aiWorld = SCR_AIWorld.Cast(aiWorld);
            if (!scr_aiWorld) {
                 if (attempt == maxRetries) break; 
                 continue;
            }
            RoadNetworkManager roadMngr = scr_aiWorld.GetRoadNetworkManager();
            if (!roadMngr) {
                if (attempt == maxRetries) break; 
                continue;
            }
            
            array<BaseRoad> Roads = {};
            vector vectorAABBMin = Vector(searchCenter[0] - currentSearchRadius, searchCenter[1] - 1000, searchCenter[2] - currentSearchRadius);
            vector vectorAABBMax = Vector(searchCenter[0] + currentSearchRadius, searchCenter[1] + 1000, searchCenter[2] + currentSearchRadius);
            roadMngr.GetRoadsInAABB(vectorAABBMin, vectorAABBMax, Roads);
            
            if (Roads.IsEmpty()){
                if (attempt == maxRetries) break; 
                continue; 
            }
            
            array<vector> validPoints = {};
            array<vector> pointsOnCurrentRoad = {};

            foreach (BaseRoad road : Roads)
            {
                pointsOnCurrentRoad.Clear();
                road.GetPoints(pointsOnCurrentRoad);
                
                if (pointsOnCurrentRoad.IsEmpty())
                    continue;

                foreach (vector point : pointsOnCurrentRoad)
                {
                    float distSq = vector.DistanceSq(searchCenter, point);
                    float currentRadiusSq = currentSearchRadius * currentSearchRadius;
                    if (distSq <= currentRadiusSq) 
                    {
                        validPoints.Insert(point);
                    }
                }
            }

            if (!validPoints.IsEmpty())
            {
                int randomIndex = Math.RandomInt(0, validPoints.Count() - 1);
                vector selectedPoint = validPoints[randomIndex];
                return selectedPoint;
            }
            if (attempt == maxRetries) break; 
        }
        return FindFallbackNavigationPoint(originalPosition, initialMaxDistance, groupNumber);
    }
};

// Callback class for collecting road entities - moved outside the main class
class RoadEntitiesCallback
{
    protected array<IEntity> m_CollectedEntities;
    
    void RoadEntitiesCallback(array<IEntity> collectedEntities)
    {
        m_CollectedEntities = collectedEntities;
    }
    
    bool OnEntityFound(IEntity entity)
    {
        m_CollectedEntities.Insert(entity);
        return true;  // return true to continue searching
    }
} 
