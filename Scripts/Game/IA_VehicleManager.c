enum IA_VehicleType
{
    None,
    CivilianCar,
    CivilianTruck,
    MilitaryTransport,
    MilitaryPatrol,
    MilitaryAPC
};

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
    
    static string GetVehicleResourceName(IA_VehicleType type, IA_Faction faction)
    {
		Print("GetVehicleResourceName Called", LogLevel.NORMAL);
        string resourceName = IA_VehicleCatalog.GetRandomVehiclePrefab(type, faction);
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
    static Vehicle SpawnVehicle(IA_VehicleType type, IA_Faction faction, vector position)
    {
        Print("[DEBUG] IA_VehicleManager.SpawnVehicle called with type " + type + ", faction " + faction, LogLevel.NORMAL);
        
        // Get vehicle resource name using our helper method
        string resourceName = GetVehicleResourceName(type, faction);
        if (resourceName.IsEmpty())
        {
            Print("[IA_VehicleManager.SpawnVehicle] Couldn't find any vehicles of type " + type + " for faction " + faction, LogLevel.ERROR);
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
    static Vehicle SpawnVehicleInAreaGroup(IA_VehicleType type, IA_Faction faction, int groupNumber)
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
        return SpawnVehicle(type, faction, spawnPos);
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
        if (!vehicle || !reservingGroup)
            return false;
            
        // Check if vehicle is already reserved
        if (IsVehicleReserved(vehicle) && !IsReservedByGroup(vehicle, reservingGroup))
            return false;
            
        // Reserve the vehicle
        m_vehicleReservations.Set(vehicle, reservingGroup);
        Print("[DEBUG] Vehicle reserved by group: " + vehicle.GetOrigin().ToString(), LogLevel.NORMAL);
        return true;
    }
    
    // Check if a vehicle is already reserved
    static bool IsVehicleReserved(Vehicle vehicle)
    {
        if (!vehicle)
            return false;
            
        return m_vehicleReservations.Contains(vehicle);
    }
    
    // Check if a vehicle is reserved by a specific group
    static bool IsReservedByGroup(Vehicle vehicle, IA_AiGroup group)
    {
        if (!vehicle || !group)
            return false;
            
        if (!m_vehicleReservations.Contains(vehicle))
            return false;
            
        return m_vehicleReservations.Get(vehicle) == group;
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
    
    // Modified version that only finds unreserved vehicles in a group
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
        return SpawnVehicle(IA_VehicleType.CivilianCar, faction, position);
    }
    
    // Spawn a civilian truck
    static Vehicle SpawnCivilianTruck(IA_Faction faction, vector position)
    {
        return SpawnVehicle(IA_VehicleType.CivilianTruck, faction, position);
    }
    
    // Spawn a military transport vehicle
    static Vehicle SpawnMilitaryTransport(IA_Faction faction, vector position)
    {
        return SpawnVehicle(IA_VehicleType.MilitaryTransport, faction, position);
    }
    
    // Spawn a military patrol vehicle
    static Vehicle SpawnMilitaryPatrol(IA_Faction faction, vector position)
    {
        return SpawnVehicle(IA_VehicleType.MilitaryPatrol, faction, position);
    }
    
    // Spawn a military APC
    static Vehicle SpawnMilitaryAPC(IA_Faction faction, vector position)
    {
        return SpawnVehicle(IA_VehicleType.MilitaryAPC, faction, position);
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
        Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Spawning vehicles for group " + m_currentActiveGroup, LogLevel.NORMAL);
        
        // If no active group, exit
        if (m_currentActiveGroup < 0)
        {
            Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: No active group", LogLevel.WARNING);
            return;
        }
        
        // Get all spawn points for this group
        array<IA_VehicleSpawnPoint> spawnPoints = IA_VehicleSpawnPoint.GetSpawnPointsByGroup(m_currentActiveGroup);
        if (spawnPoints.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: No spawn points found for group " + m_currentActiveGroup, LogLevel.WARNING);
            return;
        }
        
        Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Found " + spawnPoints.Count() + " spawn points", LogLevel.NORMAL);
        
        // Spawn a vehicle at each spawn point
        foreach (IA_VehicleSpawnPoint spawnPoint : spawnPoints)
        {
            if (!spawnPoint.CanSpawnVehicle())
                continue;
                
            // Decide what type of vehicle to spawn based on spawn point settings
            IA_VehicleType vehicleType;
            if (spawnPoint.m_allowMilitary)
            {
                // Choose a random military vehicle type
                int typeRand = IA_Game.rng.RandInt(0, 3);
                if (typeRand == 0)
                    vehicleType = IA_VehicleType.MilitaryTransport;
                else if (typeRand == 1)
                    vehicleType = IA_VehicleType.MilitaryPatrol;
                else
                    vehicleType = IA_VehicleType.MilitaryAPC;
            }
            else
            {
                // Choose a random civilian vehicle type
                int typeRand = IA_Game.rng.RandInt(0, 2);
                if (typeRand == 0)
                    vehicleType = IA_VehicleType.CivilianCar;
                else
                    vehicleType = IA_VehicleType.CivilianTruck;
            }
            
            // Spawn the vehicle
            Vehicle vehicle = spawnPoint.SpawnVehicle(vehicleType, faction);
            if (!vehicle)
            {
                Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Failed to spawn vehicle at spawn point", LogLevel.WARNING);
                continue;
            }
            
            Print("[DEBUG] IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints: Successfully spawned vehicle at " + spawnPoint.GetOrigin().ToString(), LogLevel.NORMAL);
            
            // Get a random destination within the area for the vehicle to head to
            vector randomDestination = IA_Game.rng.GenerateRandomPointInRadius(
                1, 
                IA_AreaMarker.CalculateGroupRadius(m_currentActiveGroup), 
                IA_AreaMarker.CalculateGroupCenterPoint(m_currentActiveGroup)
            );
            
            // Directly place units inside the vehicle and set them in motion
            PlaceUnitsInVehicle(vehicle, faction, randomDestination);
        }
    }
    
    // Helper method to place AI units directly inside a vehicle and set them on a path
    static void PlaceUnitsInVehicle(Vehicle vehicle, IA_Faction faction, vector destination)
    {
        if (!vehicle)
            return;
            
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Placing units in vehicle", LogLevel.NORMAL);
        
        // Find out how many seats the vehicle has
        BaseCompartmentManagerComponent compartmentManager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
        if (!compartmentManager)
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: No compartment manager found", LogLevel.WARNING);
            return;
        }
        
        // Get all compartments in the vehicle
        array<BaseCompartmentSlot> compartments = {};
        compartmentManager.GetCompartments(compartments);
        
        if (compartments.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: No compartments found", LogLevel.WARNING);
            return;
        }
        
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Found " + compartments.Count() + " compartments", LogLevel.NORMAL);

        // Calculate the number of usable seats (Pilot + Cargo/Turret)
        int seatCount = 0;
        foreach (BaseCompartmentSlot compartment : compartments)
        {
            ECompartmentType type = compartment.GetType();
            if (type == ECompartmentType.PILOT || type == ECompartmentType.CARGO || type == ECompartmentType.TURRET)
            {
                seatCount++;
            }
        }

        if (seatCount == 0)
        {
             Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: No usable seats found in vehicle.", LogLevel.WARNING);
             return;
        }
        
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Vehicle has " + seatCount + " usable seats.", LogLevel.NORMAL);
        
        // Get catalog entries matching the faction
        array<SCR_EntityCatalogEntry> entries = {};
        SCR_EntityCatalog catalog = SCR_EntityCatalog.GetInstance();
        if (!catalog) {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Failed to get entity catalog", LogLevel.ERROR);
            return;
        }

        // Get faction manager to help with catalog queries
        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Failed to get faction manager", LogLevel.ERROR);
            return;
        }

        // Get the actual faction for catalog query
        Faction actualFaction;
        if (faction == IA_Faction.US)
            actualFaction = factionManager.GetFactionByKey("US");
        else if (faction == IA_Faction.USSR)
            actualFaction = factionManager.GetFactionByKey("USSR");
        else {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Unsupported faction for vehicle crew: " + faction, LogLevel.ERROR);
            return;
        }

        // Query the catalog for character prefabs of this faction
        array<SCR_EntityCatalogEntry> characterEntries = {};
        catalog.GetEntries(characterEntries, "", actualFaction, EEntityCatalogType.CHARACTER);
        if (characterEntries.IsEmpty()) {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: No character entries found for faction", LogLevel.ERROR);
            return;
        }

        // Find a suitable rifleman/basic soldier entry
        SCR_EntityCatalogEntry soldierEntry;
        foreach (SCR_EntityCatalogEntry entry : characterEntries) {
            string prefabPath = entry.GetPrefab();
            if (prefabPath.Contains("rifleman") || prefabPath.Contains("Rifleman")) {
                soldierEntry = entry;
                break;
            }
        }

        // Fallback to first entry if no specific rifleman found
        if (!soldierEntry)
            soldierEntry = characterEntries[0];

        string charPrefabPath = soldierEntry.GetPrefab();
        Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Will use character prefab: " + charPrefabPath, LogLevel.NORMAL);
        
        // Instead of creating the group directly, use the factory method from IA_AiGroup
        IA_AiGroup aiGroup = IA_AiGroup.CreateMilitaryGroup(vehicle.GetOrigin(), IA_SquadType.Riflemen, faction);
        if (!aiGroup) {
            Print("[DEBUG] IA_VehicleManager.PlaceUnitsInVehicle: Failed to create AI group", LogLevel.ERROR);
            return;
        }
        
        // Spawn the group to initialize it properly
        aiGroup.Spawn();
        
        // Defer placement logic to allow units to fully spawn
        GetGame().GetCallqueue().CallLater(this._PlaceSpawnedUnitsInVehicle, 1000, false, vehicle, aiGroup, destination);
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

        // Fill the remaining seats with passengers
        int passengerIndex = 1;
        for (int i = 0; i < compartments.Count() && passengerIndex < characters.Count(); i++)
        {
            BaseCompartmentSlot compartment = compartments[i];

            // Skip the driver compartment
            if (compartment == driverCompartment)
                continue;

            // Only use passenger compartments (Adjusting the check as per previous context - assuming non-pilot means passenger/gunner/etc)
            // If ECompartmentType.Cargo exists, that might be better? Or checking slot flags?
            // For now, sticking to the logic that seemed implied before: anything NOT pilot is passenger/cargo.
            // if (compartment.GetType() == ECompartmentType.PILOT) { // Original incorrect logic commented out
            //     Print("Pilot Slot found for passenger placement - skipping",LogLevel.NORMAL);
            //     continue;
            // }
             if (compartment.GetType() == ECompartmentType.CARGO || compartment.GetType() == ECompartmentType.TURRET){ // More explicit check for typical passenger/gunner slots
                 // Place a character in this compartment
                 if (passengerIndex >= characters.Count()) break; // Ensure we don't go out of bounds

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
             } else {
                 Print("[DEBUG] IA_VehicleManager._PlaceSpawnedUnitsInVehicle: Skipping compartment type " + compartment.GetType() + " for passenger index " + passengerIndex, LogLevel.NORMAL);
             }
        }

        // Create waypoint for the vehicle to drive to using the AI group we already created
        // This needs to happen *after* units are successfully placed, especially the driver.
        CreateWaypointForVehicleUsingGroup(vehicle, destination, aiGroup);
    }
    
    // Helper method to create a waypoint for a vehicle to drive to using a known AI group
    static void CreateWaypointForVehicleUsingGroup(Vehicle vehicle, vector destination, IA_AiGroup aiGroup)
    {
        if (!vehicle || !aiGroup)
            return;
            
        Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicleUsingGroup: Creating waypoint at " + destination.ToString(), LogLevel.NORMAL);
        
        // Create a move waypoint at the destination
        ResourceName waypointResource = "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
        Resource res = Resource.Load(waypointResource);
        if (!res)
        {
            Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicleUsingGroup: Failed to load waypoint resource", LogLevel.ERROR);
            return;
        }
        
        EntitySpawnParams params = EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
        params.Transform[3] = destination;
        
        IEntity waypointEntity = GetGame().SpawnEntityPrefab(res, null, params);
        if (!waypointEntity)
        {
            Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicleUsingGroup: Failed to spawn waypoint entity", LogLevel.ERROR);
            return;
        }
        
        // Add the waypoint to the group
        SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(waypointEntity);
        if (waypoint)
        {
            aiGroup.AddWaypoint(waypoint);
            Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicleUsingGroup: Added waypoint to group", LogLevel.NORMAL);
        }
    }
    
    // Helper method to create a waypoint for a vehicle to drive to
    static void CreateWaypointForVehicle(Vehicle vehicle, SCR_ChimeraCharacter driver, vector destination)
    {
        Print("[DEBUG] IA_VehicleManager.CreateWaypointForVehicle: This method is deprecated. Use CreateWaypointForVehicleUsingGroup instead", LogLevel.WARNING);
        return;
    }
}; 