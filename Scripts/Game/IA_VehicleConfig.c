class IA_VehicleConfig
{
    // US VEHICLE PREFABS
    static ref array<string> s_UsCivilianCarPrefabs = {
        "{47D94E1193A88497}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport.et",
        "{F649585ABB3809F4}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport_covered.et"
    };
    
    static ref array<string> s_UsCivilianTruckPrefabs = {
        "{16A674FE31B0921C}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et",
        "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport_covered.et"
    };
    
    static ref array<string> s_UsMilitaryTransportPrefabs = {
        "{47D94E1193A88497}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport.et",
        "{F649585ABB3809F4}Prefabs/Vehicles/Wheeled/M151A2/M151A2_transport_covered.et",
        "{16A674FE31B0921C}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et"
    };
    
    static ref array<string> s_UsMilitaryPatrolPrefabs = {
        "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", 
        "{DD774D6E1547D820}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB_MERDC.et"
    };
    
    static ref array<string> s_UsMilitaryApcPrefabs = {
        "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et",
        "{DD774D6E1547D820}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB_MERDC.et" 
    };
    
    // USSR VEHICLE PREFABS
    static ref array<string> s_UssrCivilianCarPrefabs = {
        "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et",
        "{5112706CD4157393}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_closed.et"
    };
    
    static ref array<string> s_UssrCivilianTruckPrefabs = {
        "{2BE2949C3729DECE}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et",
        "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_covered.et"
    };
    
    static ref array<string> s_UssrMilitaryTransportPrefabs = {
        "{2BE2949C3729DECE}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et",
        "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_covered.et",
        "{16C950AF870637E3}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et"
    };
    
    static ref array<string> s_UssrMilitaryPatrolPrefabs = {
        "{16C950AF870637E3}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et",
        "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_covered.et"
    };
    
    static ref array<string> s_UssrMilitaryApcPrefabs = {
        "{4AD2AB7C2728D175}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et"
    };
    
    // Get a random vehicle prefab by type and faction
    static string GetRandomVehiclePrefab(IA_VehicleType type, IA_Faction faction)
    {
        array<string> prefabs = GetVehiclePrefabList(type, faction);
        if (prefabs.IsEmpty())
            return "";
            
        int idx = IA_Game.rng.RandInt(0, prefabs.Count());
        return prefabs[idx];
    }
    
    // Get the complete list of vehicle prefabs by type and faction
    static array<string> GetVehiclePrefabList(IA_VehicleType type, IA_Faction faction)
    {
        if (faction == IA_Faction.US)
        {
            if (type == IA_VehicleType.CivilianCar)
                return s_UsCivilianCarPrefabs;
            else if (type == IA_VehicleType.CivilianTruck)
                return s_UsCivilianTruckPrefabs;
            else if (type == IA_VehicleType.MilitaryTransport)
                return s_UsMilitaryTransportPrefabs;
            else if (type == IA_VehicleType.MilitaryPatrol)
                return s_UsMilitaryPatrolPrefabs;
            else if (type == IA_VehicleType.MilitaryAPC)
                return s_UsMilitaryApcPrefabs;
        }
        else if (faction == IA_Faction.USSR)
        {
            if (type == IA_VehicleType.CivilianCar)
                return s_UssrCivilianCarPrefabs;
            else if (type == IA_VehicleType.CivilianTruck)
                return s_UssrCivilianTruckPrefabs;
            else if (type == IA_VehicleType.MilitaryTransport)
                return s_UssrMilitaryTransportPrefabs;
            else if (type == IA_VehicleType.MilitaryPatrol)
                return s_UssrMilitaryPatrolPrefabs;
            else if (type == IA_VehicleType.MilitaryAPC)
                return s_UssrMilitaryApcPrefabs;
        }
        else if (faction == IA_Faction.CIV)
        {
            // Use US civilian vehicles for civilian faction
            if (type == IA_VehicleType.CivilianCar)
                return s_UsCivilianCarPrefabs;
            else if (type == IA_VehicleType.CivilianTruck)
                return s_UsCivilianTruckPrefabs;
            // No military vehicles for civilian faction
        }
        
        // Default empty array if no match
        return {};
    }
    
    // Get all available vehicles matching a specific filter
    static array<string> GetVehiclesByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary)
    {
        array<string> result = {};
        
        if (allowCivilian)
        {
            array<string> civilianCars = GetVehiclePrefabList(IA_VehicleType.CivilianCar, faction);
            array<string> civilianTrucks = GetVehiclePrefabList(IA_VehicleType.CivilianTruck, faction);
            
            foreach (string prefab : civilianCars)
                result.Insert(prefab);
                
            foreach (string prefab : civilianTrucks)
                result.Insert(prefab);
        }
        
        if (allowMilitary)
        {
            array<string> militaryTransports = GetVehiclePrefabList(IA_VehicleType.MilitaryTransport, faction);
            array<string> militaryPatrols = GetVehiclePrefabList(IA_VehicleType.MilitaryPatrol, faction);
            array<string> militaryApcs = GetVehiclePrefabList(IA_VehicleType.MilitaryAPC, faction);
            
            foreach (string prefab : militaryTransports)
                result.Insert(prefab);
                
            foreach (string prefab : militaryPatrols)
                result.Insert(prefab);
                
            foreach (string prefab : militaryApcs)
                result.Insert(prefab);
        }
        
        return result;
    }
}; 