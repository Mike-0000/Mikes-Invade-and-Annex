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
    
    // Group-specific vehicles
    static private ref array<ref array<IEntity>> m_groupVehicles = {};
    static private int m_currentActiveGroup = -1;
    
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
        m_vehicles.Clear();
        
        // Clear all group vehicle arrays
        for (int i = 0; i < m_groupVehicles.Count(); i++)
        {
            m_groupVehicles[i].Clear();
        }
    }
    
    // Set the currently active area group
    static void SetActiveGroup(int groupNumber)
    {
        m_currentActiveGroup = groupNumber;
        
        // Clear all reservations when switching groups
        ClearAllReservations();
        
        // Clean up old vehicles from previous groups
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
        // Don't clean up if there's no active group
        if (m_currentActiveGroup < 0)
            return;
            
        for (int groupIdx = 0; groupIdx < m_groupVehicles.Count(); groupIdx++)
        {
            // Skip the current active group
            if (groupIdx == m_currentActiveGroup)
                continue;
                
            array<IEntity> groupVehicles = m_groupVehicles[groupIdx];
            
            // Clean up all vehicles in this group
            for (int i = 0; i < groupVehicles.Count(); i++)
            {
                IEntity vehEntity = groupVehicles[i];
                if (!vehEntity)
                    continue;
                    
                // Remove from main vehicles array too
                int mainIdx = m_vehicles.Find(vehEntity);
                if (mainIdx != -1)
                    m_vehicles.Remove(mainIdx);
                    
                // Add to garbage collection
                IA_Game.AddEntityToGc(vehEntity);
            }
            
            // Clear the group array
            groupVehicles.Clear();
        }
    }
    
    static string GetVehicleResourceName(IA_Faction faction)
    {
		Print("GetVehicleResourceName Called", LogLevel.NORMAL);
        string resourceName = IA_VehicleCatalog.GetRandomVehiclePrefab(faction);
		Print("resourceName Received as " + resourceName, LogLevel.NORMAL);
        if (resourceName.IsEmpty())
        {
            // Fallback in case no prefab is found
            return "{47D94E1193A88497}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport.et";
        }
        return resourceName;
    }
    
    // Get list of all vehicles that match specific filter criteria
    static array<string> GetVehiclePrefabsByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary)
    {
        return IA_VehicleCatalog.GetVehiclesByFilter(faction, allowCivilian, allowMilitary);
    }
    
    // Spawn a random vehicle that matches filter criteria at the specified position
    static Vehicle SpawnRandomVehicle(IA_Faction faction, bool allowCivilian, bool allowMilitary, vector position)
    {
        Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicle called with faction " + faction + ", civilian=" + allowCivilian + ", military=" + allowMilitary, LogLevel.NORMAL);
        
        // Get catalog entries matching the filter
        array<SCR_EntityCatalogEntry> entries = IA_VehicleCatalog.GetVehicleEntriesByFilter(faction, allowCivilian, allowMilitary);
        if (entries.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicle: No matching entries found", LogLevel.WARNING);
            return null;
        }
            
        // Pick a random entry
        SCR_EntityCatalogEntry entry = entries[IA_Game.rng.RandInt(0, entries.Count())];
        string resourceName = entry.GetPrefab();
        Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicle: Selected prefab " + resourceName, LogLevel.NORMAL);
        
        // Load resource and spawn
        Resource resource = Resource.Load(resourceName);
        if (!resource)
        {
            Print("[IA_VehicleManager.SpawnRandomVehicle] No such resource: " + resourceName, LogLevel.ERROR);
            return null;
        }
        
        EntitySpawnParams params = IA_CreateSurfaceAdjustedSpawnParams(position);
        IEntity entity = GetGame().SpawnEntityPrefab(resource, null, params);
        Vehicle vehicle = Vehicle.Cast(entity);
        
        if (vehicle)
        {
            Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicle: Successfully spawned vehicle at " + position.ToString(), LogLevel.NORMAL);
            // Add to main vehicles array
            m_vehicles.Insert(entity);
            
            // Add to group-specific array if active group is valid
            if (m_currentActiveGroup >= 0 && m_currentActiveGroup < m_groupVehicles.Count())
            {
                m_groupVehicles[m_currentActiveGroup].Insert(entity);
            }
        }
        else
        {
            Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicle: Failed to spawn vehicle", LogLevel.WARNING);
        }
            
        return vehicle;
    }
    
    // Spawn a vehicle at the specified position
    static Vehicle SpawnVehicle(IA_Faction faction, vector position)
    {
        Print("[DEBUG] IA_VehicleManager.SpawnVehicle called with faction " + faction, LogLevel.NORMAL);
        
        // Get vehicle resource name using our helper method
        string resourceName = GetVehicleResourceName(faction);
        if (resourceName.IsEmpty())
        {
            Print("[IA_VehicleManager.SpawnVehicle] Couldn't find any vehicles for faction " + faction, LogLevel.ERROR);
            return null;
        }
        
        Resource resource = Resource.Load(resourceName);
        if (!resource)
        {
            Print("[IA_VehicleManager.SpawnVehicle] No such resource: " + resourceName, LogLevel.ERROR);
            return null;
        }
        
        EntitySpawnParams params = IA_CreateSurfaceAdjustedSpawnParams(position);
        IEntity entity = GetGame().SpawnEntityPrefab(resource, null, params);
        Vehicle vehicle = Vehicle.Cast(entity);
        
        if (vehicle)
        {
            Print("[DEBUG] IA_VehicleManager.SpawnVehicle: Successfully spawned vehicle at " + position.ToString(), LogLevel.NORMAL);
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
                Print("[DEBUG] IA_VehicleManager.SpawnVehicle: Automatically creating AI crew for vehicle", LogLevel.NORMAL);
                
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
                vector roadPos = FindRandomRoadEntityInZone(originPoint, radiusToUse, m_currentActiveGroup); 
                
                // If a valid road is found, use it as destination
                if (roadPos != vector.Zero)
                    destination = roadPos;
                else
                {
                    // Fallback to a random position if no road found
                    destination = IA_Game.rng.GenerateRandomPointInRadius(1, radiusToUse, originPoint);
                }
                
                // Create AI and assign to vehicle
                PlaceUnitsInVehicle(vehicle, faction, destination, IA_Game.CurrentAreaInstance);
            }
        }
        else
        {
            Print("[DEBUG] IA_VehicleManager.SpawnVehicle: Failed to spawn vehicle", LogLevel.WARNING);
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
    static Vehicle SpawnVehicleInAreaGroup(IA_Faction faction, int groupNumber)
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
        return SpawnVehicle(faction, spawnPos);
    }
    
    // Spawn a random vehicle within the area group's bounds
    static Vehicle SpawnRandomVehicleInAreaGroup(IA_Faction faction, bool allowCivilian, bool allowMilitary, int groupNumber)
    {
        Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup called with faction " + faction + ", civilian=" + allowCivilian + ", military=" + allowMilitary + ", group=" + groupNumber, LogLevel.NORMAL);
        
        // Calculate a position within the area group
        vector centerPoint = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
        float groupRadius = IA_AreaMarker.CalculateGroupRadius(groupNumber);
        
        if (centerPoint == vector.Zero || groupRadius <= 0)
        {
            Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup: Invalid center point or radius", LogLevel.WARNING);
            return null;
        }
            
        // Generate a random position within the group area
        vector spawnPos = IA_Game.rng.GenerateRandomPointInRadius(1, groupRadius * 0.8, centerPoint);
        
        // Adjust to ground level
        float y = GetGame().GetWorld().GetSurfaceY(spawnPos[0], spawnPos[2]);
        spawnPos[1] = y + 0.5; // Slightly above ground
        
        Print("[DEBUG] IA_VehicleManager.SpawnRandomVehicleInAreaGroup: Generated position " + spawnPos.ToString(), LogLevel.NORMAL);
        
        // Spawn the random vehicle
        return SpawnRandomVehicle(faction, allowCivilian, allowMilitary, spawnPos);
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
        Print("[DEBUG_VEHICLE_RESERVATION] ReserveVehicle called, vehicle: " + vehicle + ", group: " + reservingGroup, LogLevel.NORMAL);
        
        if (!vehicle || !reservingGroup)
        {
            Print("[DEBUG_VEHICLE_RESERVATION] ReserveVehicle failed - vehicle or group is null", LogLevel.WARNING);
            return false;
        }
            
        // Check if vehicle is already reserved
        bool alreadyReserved = IsVehicleReserved(vehicle);
        bool reservedByThisGroup = IsReservedByGroup(vehicle, reservingGroup);
        
        Print("[DEBUG_VEHICLE_RESERVATION] Vehicle already reserved: " + alreadyReserved + ", by this group: " + reservedByThisGroup, LogLevel.NORMAL);
        
        if (alreadyReserved && !reservedByThisGroup)
        {
            Print("[DEBUG_VEHICLE_RESERVATION] Vehicle already reserved by another group, reservation failed", LogLevel.WARNING);
            return false;
        }
        
        // Get the previous state of the map
        string previousMapState = "";
        foreach (IEntity veh, IA_AiGroup grp : m_vehicleReservations)
            previousMapState += veh.ToString() + " -> " + grp.ToString() + ", ";
        
        Print("[DEBUG_VEHICLE_RESERVATION] Reservation map before: " + previousMapState, LogLevel.NORMAL);
            
        // Reserve the vehicle
        m_vehicleReservations.Set(vehicle, reservingGroup);
        
        // Verify the reservation was added
        bool verifyReserved = m_vehicleReservations.Contains(vehicle);
        IA_AiGroup verifyGroup = null;
        if (verifyReserved)
            verifyGroup = m_vehicleReservations.Get(vehicle);
            
        Print("[DEBUG_VEHICLE_RESERVATION] Verification after Set - contains vehicle: " + verifyReserved + ", group: " + verifyGroup, LogLevel.NORMAL);
        
        Print("[DEBUG] Vehicle reserved by group: " + vehicle.GetOrigin().ToString(), LogLevel.NORMAL);
        return true;
    }
    
    // Check if a vehicle is already reserved
    static bool IsVehicleReserved(Vehicle vehicle)
    {
        if (!vehicle)
        {
            Print("[DEBUG_VEHICLE_RESERVATION] IsVehicleReserved called with null vehicle", LogLevel.WARNING);
            return false;
        }
        
        bool result = m_vehicleReservations.Contains(vehicle);
        Print("[DEBUG_VEHICLE_RESERVATION] IsVehicleReserved check for " + vehicle + " result: " + result, LogLevel.NORMAL);
        return result;
    }
    
    // Check if a vehicle is reserved by a specific group
    static bool IsReservedByGroup(Vehicle vehicle, IA_AiGroup group)
    {
        Print("[DEBUG_VEHICLE_RESERVATION] IsReservedByGroup called, vehicle: " + vehicle + ", group: " + group, LogLevel.NORMAL);
        
        if (!vehicle || !group)
        {
            Print("[DEBUG_VEHICLE_RESERVATION] IsReservedByGroup early return - vehicle or group is null", LogLevel.WARNING);
            return false;
        }
            
        if (!m_vehicleReservations.Contains(vehicle))
        {
            Print("[DEBUG_VEHICLE_RESERVATION] Vehicle not in reservation map", LogLevel.NORMAL);
            return false;
        }
            
        IA_AiGroup reservingGroup = m_vehicleReservations.Get(vehicle);
        bool result = (reservingGroup == group);
        Print("[DEBUG_VEHICLE_RESERVATION] Vehicle is reserved by group " + reservingGroup + ", matches query group: " + result, LogLevel.NORMAL);
        return result;
    }
    
    // Release a vehicle reservation
    static void ReleaseVehicleReservation(Vehicle vehicle)
    {
        if (!vehicle)
            return;
            
        if (m_vehicleReservations.Contains(vehicle))
        {
            m_vehicleReservations.Remove(vehicle);
            Print("[DEBUG] Vehicle reservation released: " + vehicle.GetOrigin().ToString(), LogLevel.NORMAL);
        }
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
            Print("[DEBUG_VEHICLES] HasVehicleReachedDestination - vehicle is NULL", LogLevel.WARNING);
            return false;
        }
            
        vector vehiclePos = vehicle.GetOrigin();
        float distance = vector.Distance(vehiclePos, destination);
        
        Print("[DEBUG_VEHICLES] HasVehicleReachedDestination - vehicle at " + vehiclePos.ToString() + ", destination: " + destination.ToString() + ", distance: " + distance, LogLevel.NORMAL);
        
        // Consider reached if within 10 meters
        bool hasReached = distance < 10;
        Print("[DEBUG_VEHICLES] HasVehicleReachedDestination - Has reached: " + hasReached, LogLevel.NORMAL);
        return hasReached;
    }
    
    // Create waypoint only if the vehicle doesn't already have an active one or has reached its current destination
    static void UpdateVehicleWaypoint(Vehicle vehicle, IA_AiGroup aiGroup, vector destination)
    {
        Print("[DEBUG_VEHICLES] UpdateVehicleWaypoint called", LogLevel.NORMAL);
        if (!vehicle || !aiGroup)
        {
            Print("[DEBUG_VEHICLES] UpdateVehicleWaypoint - vehicle or aiGroup is NULL", LogLevel.WARNING);
            return;
        }
            
        // Get vehicle position
        vector vehiclePos = vehicle.GetOrigin();
        Print("[DEBUG_VEHICLES] Vehicle at " + vehiclePos.ToString() + ", destination: " + destination.ToString(), LogLevel.NORMAL);
        
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
            Print("[DEBUG_VEHICLES] Destination already appears to be on a road", LogLevel.NORMAL);
            shouldFindRoad = false;
        }
        
        // If the group has no active waypoint or has reached the destination
        bool hasActiveWaypoint = aiGroup.HasActiveWaypoint();
        bool hasReachedDestination = HasVehicleReachedDestination(vehicle, destination);
        
        Print("[DEBUG_VEHICLES] UpdateVehicleWaypoint - hasActiveWaypoint: " + hasActiveWaypoint + ", hasReachedDestination: " + hasReachedDestination, LogLevel.NORMAL);
        
        if (!hasActiveWaypoint || hasReachedDestination)
        {
            Print("[DEBUG_VEHICLES] Vehicle needs new waypoint - creating one at " + destination.ToString(), LogLevel.NORMAL);
            
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
                    
                    Print("[DEBUG_VEHICLES] Searching for road near destination with radius " + searchRadius, LogLevel.NORMAL);
                    
                    // Find a road near the destination
                    vector roadDestination = FindRandomRoadEntityInZone(destination, searchRadius, m_currentActiveGroup);
                    
                    // If we found a valid road position, use it
                    if (roadDestination != vector.Zero)
                    {
                        Print("[DEBUG_VEHICLES] Found road at " + roadDestination.ToString() + ", using as waypoint destination", LogLevel.NORMAL);
                        destination = roadDestination;
                    }
                }
            }
            
            CreateWaypointForVehicleUsingGroup(vehicle, destination, aiGroup);
        }
        else
        {
            Print("[DEBUG_VEHICLES] Vehicle has active waypoint and hasn't reached destination - skipping", LogLevel.NORMAL);
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
    static Vehicle SpawnCivilianCar(IA_Faction faction, vector position)
    {
        return SpawnVehicle(faction, position);
    }
    
    // Spawn a civilian truck
    static Vehicle SpawnCivilianTruck(IA_Faction faction, vector position)
    {
        return SpawnVehicle(faction, position);
    }
    
    // Spawn a military transport vehicle
    static Vehicle SpawnMilitaryTransport(IA_Faction faction, vector position)
    {
        return SpawnVehicle(faction, position);
    }
    
    // Spawn a military patrol vehicle
    static Vehicle SpawnMilitaryPatrol(IA_Faction faction, vector position)
    {
        return SpawnVehicle(faction, position);
    }
    
    // Spawn a military APC
    static Vehicle SpawnMilitaryAPC(IA_Faction faction, vector position)
    {
        return SpawnVehicle(faction, position);
    }
    
    // Spawn a random civilian vehicle (car or truck)
    static Vehicle SpawnRandomCivilianVehicle(IA_Faction faction, vector position)
    {
        return SpawnRandomVehicle(faction, true, false, position);
    }
    
    // Spawn a random military vehicle (transport, patrol, or APC)
    static Vehicle SpawnRandomMilitaryVehicle(IA_Faction faction, vector position)
    {
        return SpawnRandomVehicle(faction, false, true, position);
    }
    
    // Spawn vehicles at all available spawn points for the active group with occupants
    static void SpawnVehiclesAtAllSpawnPoints(IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Spawning vehicles for faction " + faction, LogLevel.NORMAL);
        
        // If an active group is set, only spawn in that group
        int targetGroup = m_currentActiveGroup;
        
        // Get all spawn points that match the group
        array<IA_VehicleSpawnPoint> spawnPoints = IA_VehicleSpawnPoint.GetSpawnPointsByGroup(targetGroup);
        if (spawnPoints.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: No spawn points found in group " + targetGroup, LogLevel.WARNING);
            return;
        }
        
        Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Found " + spawnPoints.Count() + " spawn points in group " + targetGroup, LogLevel.NORMAL);
        
        // Spawn vehicles at each point
        foreach (IA_VehicleSpawnPoint spawnPoint : spawnPoints)
        {
            if (!spawnPoint.CanSpawnVehicle())
                continue;
                
            // Spawn a random vehicle
            Vehicle vehicle = spawnPoint.SpawnRandomVehicle(faction);
            if (!vehicle)
                continue;
                
            Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Successfully spawned vehicle at " + spawnPoint.GetOrigin().ToString(), LogLevel.NORMAL);
            
            // Get the area center point to use as a reference
            vector areaCenter = IA_AreaMarker.CalculateGroupCenterPoint(m_currentActiveGroup);
            float areaRadius = IA_AreaMarker.CalculateGroupRadius(m_currentActiveGroup);
            
            // Directly place units inside the vehicle and set them in motion
            PlaceUnitsInVehicle(vehicle, faction, areaCenter, IA_Game.CurrentAreaInstance);
        }
    }
    
    // Add units to a vehicle and assign orders
    static void PlaceUnitsInVehicle(Vehicle vehicle, IA_Faction faction, vector destination, IA_AreaInstance areaInstance)
    {
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle called for vehicle: " + vehicle + ", faction: " + faction, LogLevel.NORMAL);
        
        if (!vehicle || !areaInstance)
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Missing vehicle or area instance", LogLevel.WARNING);
            return;
        }
        
        // Get compartment count for the vehicle
        BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
        if (!compartmentManager)
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: No compartment manager found", LogLevel.WARNING);
            return;
        }
        
        array<BaseCompartmentSlot> compartments = {};
        compartmentManager.GetCompartments(compartments);
        int compartmentCount = compartments.Count();
        
        if (compartmentCount == 0)
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Vehicle has no compartments", LogLevel.WARNING);
            return;
        }
        
        // Create the AI group for the vehicle with the correct number of units
        IA_AiGroup group = IA_AiGroup.CreateGroupForVehicle(vehicle, faction, compartmentCount);
        if (!group)
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Failed to create group for vehicle", LogLevel.WARNING);
            return;
        }
        
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Created group " + group, LogLevel.NORMAL);
        
        // Add group to area instance using the public method
        areaInstance.AddMilitaryGroup(group);
        
        // Spawn the group if needed
        if (!group.IsSpawned())
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Spawning group", LogLevel.NORMAL);
            group.Spawn();
        }
        
        // First assign the vehicle and destination to the group
        group.AssignVehicle(vehicle, destination);
        
        // Then directly teleport units into the vehicle instead of making them walk to it
        _PlaceSpawnedUnitsInVehicle(vehicle, group, destination);
        
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Vehicle and destination assigned to group", LogLevel.NORMAL);
    }
    
    // Find a random road entity in the zone around the given position
    static vector FindRandomRoadEntityInZone(vector position, float maxDistance = 300, int groupNumber = -1)
    {
        // If groupNumber is provided, use it to calculate a dynamic search radius
        if (groupNumber >= 0)
        {
            // Get the group center point
            vector centerPoint = IA_AreaMarker.CalculateGroupCenterPoint(groupNumber);
            
            // Calculate the radius based on the group's actual size
            float groupRadius = IA_AreaMarker.CalculateGroupRadius(groupNumber);
            
            // Use the group radius if available, or fall back to the provided maxDistance
            if (groupRadius > 0)
            {
                // Use the calculated radius plus a small margin
                maxDistance = groupRadius * 1.2;
                // Use the group center as the position if it's valid
                if (centerPoint != vector.Zero)
                {   
                    position = centerPoint;
                }
                Print("[DEBUG] IA_VehicleManager.FindRandomRoadEntityInZone: Using dynamic radius of " + maxDistance + " based on group " + groupNumber, LogLevel.NORMAL);
            }
        }
        
        Print("[DEBUG] IA_VehicleManager.FindRandomRoadEntityInZone: Searching for ANY road entities within radius " + maxDistance + " from center " + position.ToString(), LogLevel.NORMAL);
        
		// Generate a random point within the radius around the center position
		vector randomVector = IA_Game.rng.GenerateRandomPointInRadius(1, maxDistance, position);
		Print("[DEBUG] IA_VehicleManager.FindRandomRoadEntityInZone: Generated random point at " + randomVector.ToString(), LogLevel.NORMAL);
		
        // Create callback to collect road entities
        array<IEntity> roadEntities = {};
        RoadEntitiesCallback callback = new RoadEntitiesCallback(roadEntities);
        AIWorld aiWorld = GetGame().GetAIWorld();
		SCR_AIWorld scr_aiWorld = SCR_AIWorld.Cast(aiWorld);
		RoadNetworkManager roadMngr = scr_aiWorld.GetRoadNetworkManager();
		array<BaseRoad> Roads = {};
		float distanceFromPoint = 0;
		
		// Calculate AABB min and max vectors with large vertical range to account for mountains and valleys
		// Use a vertical range of ±1000 meters to ensure we capture roads at different elevations
		vector vectorAABBMin = Vector(position[0] - maxDistance, position[1] - 1000, position[2] - maxDistance);
		vector vectorAABBMax = Vector(position[0] + maxDistance, position[1] + 1000, position[2] + maxDistance);
		
		//roadMngr.GetClosestRoad(randomVector, closestRoad, distanceFromPoint);
		roadMngr.GetRoadsInAABB(vectorAABBMin, vectorAABBMax, Roads);
		
        if (Roads.IsEmpty()){
			Print("No Roads Found In AABB!!",LogLevel.FATAL);
            return FindFallbackNavigationPoint(position, maxDistance, groupNumber);
		}
		int randomRoadIndex = Math.RandomInt(0,Roads.Count() - 1);
        array<vector> arrayOfVectors = {};
		BaseRoad randomRoad = Roads[randomRoadIndex];
		randomRoad.GetPoints(arrayOfVectors);
        // Get the road position
        vector roadPosition = arrayOfVectors[0];
        Print("[DEBUG] IA_VehicleManager.FindRandomRoadEntityInZone: Selected random road at " + roadPosition.ToString(), LogLevel.NORMAL);
        
        return roadPosition;
    }
    
    // Filter function for road entities
    static bool FilterRoadEntities(IEntity entity)
    {
        if (!entity)
            return false;
	
            //if(entity.ClassName().Contains("RoadEntity"))
				//Print("Found Entity!" + entity.ClassName(),LogLevel.NORMAL);
        // Check if entity is a road
        return (entity.ClassName() == "RoadEntity");
    }
    
    // Find a fallback point that's suitable for navigation if no roads are found
    static vector FindFallbackNavigationPoint(vector position, float maxDistance = 300, int groupNumber = -1)
    {
        Print("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Finding fallback point near " + position.ToString(), LogLevel.NORMAL);
        
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
                Print("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Using dynamic radius of " + maxDistance + " based on group " + groupNumber, LogLevel.NORMAL);
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
                Print("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Found suitable point at " + randPoint.ToString(), LogLevel.NORMAL);
                return randPoint;
            }
        }
        
        // If all attempts failed, just return a random point in a smaller radius
        vector fallbackPoint = IA_Game.rng.GenerateRandomPointInRadius(1, maxDistance/2, position);
        
        Print("[DEBUG] IA_VehicleManager.FindFallbackNavigationPoint: Using simple fallback at " + fallbackPoint.ToString(), LogLevel.WARNING);
        return fallbackPoint;
    }
    
    // Helper method called after a delay to place units in the vehicle
    private static void _PlaceSpawnedUnitsInVehicle(Vehicle vehicle, IA_AiGroup aiGroup, vector destination)
    {
        if (!vehicle || !aiGroup)
            return;
            
        Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Attempting to place units after delay.", LogLevel.NORMAL);

        // Get the characters from the group
        array<SCR_ChimeraCharacter> characters = aiGroup.GetGroupCharacters();
        if (characters.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: No characters found in AI group after delay.", LogLevel.WARNING);
            // Clean up the group if no characters spawned? Maybe despawn?
            // aiGroup.Despawn(); // Consider adding this if empty groups are an issue
            return;
        }
        
        Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Found " + characters.Count() + " characters after delay.", LogLevel.NORMAL);

        // Find out how many seats the vehicle has (needed again here)
        BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
        if (!compartmentManager)
        {
            Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: No compartment manager found after delay.", LogLevel.WARNING);
            return;
        }
        array<BaseCompartmentSlot> compartments = {};
        compartmentManager.GetCompartments(compartments);
        if (compartments.IsEmpty())
        {
             Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: No compartments found after delay.", LogLevel.WARNING);
             return;
        }
        /*
        // Find the driver compartment first (needed again here)
        BaseCompartmentSlot driverCompartment = null;
        foreach (BaseCompartmentSlot compartment : compartments)
        {
            if (compartment.GetType() == ECompartmentType.PILOT)
            {
                driverCompartment = compartment;
                break;
            }
        }
        if (!driverCompartment)
        {
            Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: No driver compartment found after delay.", LogLevel.WARNING);
            return;
        }

        // Set the driver first
        SCR_ChimeraCharacter driver = characters[0];
        if (driver)
        {
            // Place driver in the vehicle
            CompartmentAccessComponent accessComponent = CompartmentAccessComponent.Cast(driver.FindComponent(CompartmentAccessComponent));
            if (accessComponent)
            {
                accessComponent.GetInVehicle(vehicle, driverCompartment, true, 0, ECloseDoorAfterActions.CLOSE_DOOR, true);
                Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Placed driver in vehicle after delay.", LogLevel.NORMAL);
            }
            else {
                 Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Driver CompartmentAccessComponent is NULL after delay.", LogLevel.WARNING);
            }
        }
        else {
            Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Driver character is NULL after delay.", LogLevel.WARNING);
        }
*/
        // Fill the remaining seats with passengers
        int passengerIndex = 0;
        for (int i = 0; i < compartments.Count() && passengerIndex < characters.Count(); i++)
        {
            BaseCompartmentSlot compartment = compartments[i];

            // Skip the driver compartment
           // if (compartment == driverCompartment)
           //     continue;

            if (!compartment.GetOccupant())
            {
                // Place a character in this compartment
                if (passengerIndex >= characters.Count()) 
                    break; // Ensure we don't go out of bounds

                SCR_ChimeraCharacter passenger = characters[passengerIndex];
                CompartmentAccessComponent accessComponent = CompartmentAccessComponent.Cast(passenger.FindComponent(CompartmentAccessComponent));
                if (accessComponent)
                {
                    accessComponent.GetInVehicle(vehicle, compartment, true, 0, ECloseDoorAfterActions.CLOSE_DOOR, true);
                    Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Placed passenger in vehicle after delay.", LogLevel.NORMAL);
                }
                else {
                    Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Passenger CompartmentAccessComponent is NULL after delay.", LogLevel.WARNING);
                }
                passengerIndex++;
            }
        }

        // Create waypoint for the vehicle to drive to using the AI group we already created
        // This needs to happen *after* units are successfully placed, especially the driver.
        UpdateVehicleWaypoint(vehicle, aiGroup, destination);
    }
    
    // Helper method to create a waypoint for a vehicle to drive to using a known AI group
    static void CreateWaypointForVehicleUsingGroup(Vehicle vehicle, vector destination, IA_AiGroup aiGroup)
    {
        Print("[DEBUG_VEHICLES] CreateWaypointForVehicleUsingGroup called", LogLevel.NORMAL);
        
        if (!vehicle || !aiGroup)
        {
            Print("[DEBUG_VEHICLES] CreateWaypointForVehicleUsingGroup - vehicle or aiGroup is NULL", LogLevel.WARNING);
            return;
        }
            
        // Find the nearest road to the destination
        vector vehiclePos = vehicle.GetOrigin();
        float searchRadius = vector.Distance(vehiclePos, destination);
        // Ensure minimum search radius
        if (searchRadius < 100)
            searchRadius = 100;
        
        // Find a road near the destination
        vector roadDestination = FindRandomRoadEntityInZone(destination, searchRadius, IA_VehicleManager.GetActiveGroup());
        
        // If we found a valid road position, use it instead of the original destination
        if (roadDestination != vector.Zero)
        {
            Print("[DEBUG_VEHICLES] Using road position for waypoint instead of original destination", LogLevel.NORMAL);
            destination = roadDestination;
        }
        else
        {
            Print("[DEBUG_VEHICLES] No road found near destination, using original destination", LogLevel.WARNING);
        }
        
        Print("[DEBUG_VEHICLES] Creating waypoint at " + destination.ToString(), LogLevel.NORMAL);
        
        // Create a move waypoint at the destination
        ResourceName waypointResource = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
        Resource res = Resource.Load(waypointResource);
        if (!res)
        {
            Print("[DEBUG_VEHICLES] CreateWaypointForVehicleUsingGroup - Failed to load waypoint resource", LogLevel.ERROR);
            return;
        }
        
        EntitySpawnParams params = EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
        params.Transform[3] = destination;
        
        IEntity waypointEntity = GetGame().SpawnEntityPrefab(res, null, params);
        if (!waypointEntity)
        {
            Print("[DEBUG_VEHICLES] CreateWaypointForVehicleUsingGroup - Failed to spawn waypoint entity", LogLevel.ERROR);
            return;
        }
        
        Print("[DEBUG_VEHICLES] Waypoint entity spawned successfully", LogLevel.NORMAL);
        
        // Add the waypoint to the group
        SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(waypointEntity);
        if (waypoint)
        {
            // Set a very high priority to ensure this waypoint takes precedence
            waypoint.SetPriorityLevel(2000);
            Print("[DEBUG_VEHICLES] Setting waypoint priority to 2000", LogLevel.NORMAL);
            
            aiGroup.AddWaypoint(waypoint);
            Print("[DEBUG_VEHICLES] Added waypoint to group successfully", LogLevel.NORMAL);
        }
        else
        {
            Print("[DEBUG_VEHICLES] Failed to cast waypoint entity to SCR_AIWaypoint", LogLevel.ERROR);
        }
    }
    
    // Helper method to create a waypoint for a vehicle to drive to
    static void CreateWaypointForVehicle(Vehicle vehicle, SCR_ChimeraCharacter driver, vector destination)
    {
        Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicle: This method is deprecated. Use CreateWaypointForVehicleUsingGroup instead", LogLevel.WARNING);
        return;
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