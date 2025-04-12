class IA_VehicleCatalog
{
    // Map the internal vehicle types to appropriate entity labels
    static array<EEditableEntityLabel> GetLabelsForVehicleType(IA_VehicleType type)
    {
        array<EEditableEntityLabel> labels = {};
        
        switch (type)
        {
            case IA_VehicleType.CivilianCar:
                labels.Insert(EEditableEntityLabel.VEHICLE_CAR);
                labels.Insert(EEditableEntityLabel.TRAIT_UNARMED);
                break;
                
            case IA_VehicleType.CivilianTruck:
                labels.Insert(EEditableEntityLabel.VEHICLE_TRUCK);
                labels.Insert(EEditableEntityLabel.TRAIT_UNARMED);
                break;
                
            case IA_VehicleType.MilitaryTransport:
                labels.Insert(EEditableEntityLabel.TRAIT_PASSENGERS_LARGE);
                labels.Insert(EEditableEntityLabel.TRAIT_UNARMED);
                break;
                
            case IA_VehicleType.MilitaryPatrol:
                labels.Insert(EEditableEntityLabel.VEHICLE_CAR);
                labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
                break;
                
            case IA_VehicleType.MilitaryAPC:
                labels.Insert(EEditableEntityLabel.VEHICLE_APC);
                labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
                break;
        }
        
        return labels;
    }
    
    // Get the faction key string from our IA_Faction enum
    static string GetFactionKey(IA_Faction faction)
    {
        switch (faction)
        {
            case IA_Faction.US:
                return "US";
            case IA_Faction.USSR:
                return "USSR";
            case IA_Faction.CIV:
                return "CIV";
            case IA_Faction.FIA:
                return "FIA";
        }
        
        return "US"; // Default fallback
    }
    
    // Get catalog entries for a specific vehicle type and faction
    static array<SCR_EntityCatalogEntry> GetVehicleEntries(IA_VehicleType type, IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries called for type " + type + ", faction " + faction, LogLevel.NORMAL);
        array<SCR_EntityCatalogEntry> result = {};
        
        // Get the faction manager
        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager)
        {
            Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get faction manager", LogLevel.WARNING);
            return result;
        }
        
        // Get the faction by key
        string factionKey = GetFactionKey(faction);
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Looking for faction with key " + factionKey, LogLevel.NORMAL);
        SCR_Faction scrFaction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
        if (!scrFaction)
        {
            Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get faction with key " + factionKey, LogLevel.WARNING);
            // For CIV vehicles, sometimes we might still want US vehicles
            if (faction == IA_Faction.CIV)
            {
                Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Trying US faction as fallback", LogLevel.NORMAL);
                scrFaction = SCR_Faction.Cast(factionManager.GetFactionByKey("US"));
                if (!scrFaction)
                {
                    Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get US faction fallback", LogLevel.WARNING);
                    return result;
                }
            }
            else
            {
                return result;
            }
        }
        
        // Get the entity catalog for vehicles
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Getting vehicle catalog for faction " + factionKey, LogLevel.NORMAL);
        SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
        if (!entityCatalog)
        {
            Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get entity catalog", LogLevel.WARNING);
            return result;
        }
        
        // Setup the filter labels
        array<EEditableEntityLabel> includedLabels = GetLabelsForVehicleType(type);
        array<EEditableEntityLabel> excludedLabels = {};
        
        // For civilian vehicles, exclude military faction vehicles
        if (type == IA_VehicleType.CivilianCar || type == IA_VehicleType.CivilianTruck)
        {
            if (faction != IA_Faction.CIV)
            {
                // If we're looking for civilian vehicles but from a military faction,
                // exclude military-specific traits
                excludedLabels.Insert(EEditableEntityLabel.TRAIT_ARMED);
            }
            else
            {
                // For actual CIV faction, just ensure we're not getting military vehicles
                excludedLabels.Insert(EEditableEntityLabel.FACTION_US);
                excludedLabels.Insert(EEditableEntityLabel.FACTION_USSR);
                excludedLabels.Insert(EEditableEntityLabel.FACTION_FIA);
            }
        }
        
        // Get the filtered list of vehicles
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Getting filtered entity list", LogLevel.NORMAL);
        entityCatalog.GetFullFilteredEntityList(result, includedLabels, excludedLabels);
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Found " + result.Count() + " entries", LogLevel.NORMAL);
        return result;
    }
    
    // Get a random catalog entry for a specific vehicle type and faction
    static SCR_EntityCatalogEntry GetRandomVehicleEntry(IA_VehicleType type, IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry called for type " + type + ", faction " + faction, LogLevel.NORMAL);
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(type, faction);
        if (entries.IsEmpty())
        {
            Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry: No entries found", LogLevel.WARNING);
            return null;
        }
            
        int idx = IA_Game.rng.RandInt(0, entries.Count());
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry: Found " + entries.Count() + " entries, selected index " + idx, LogLevel.NORMAL);
        return entries[idx];
    }
    
    // Get a random vehicle prefab resource name as a string
    static string GetRandomVehiclePrefab(IA_VehicleType type, IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab called for type " + type + ", faction " + faction, LogLevel.NORMAL);
        SCR_EntityCatalogEntry entry = GetRandomVehicleEntry(type, faction);
        if (!entry)
        {
            Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: No entry found", LogLevel.WARNING);
            return "";
        }
            
        string prefab = entry.GetPrefab();
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: Selected prefab " + prefab, LogLevel.NORMAL);
        return prefab;
    }
    
    // Get all vehicle prefab resource names for a specific type and faction
    static array<string> GetVehiclePrefabList(IA_VehicleType type, IA_Faction faction)
    {
        array<string> result = {};
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(type, faction);
        
        foreach (SCR_EntityCatalogEntry entry : entries)
        {
            result.Insert(entry.GetPrefab());
        }
        
        return result;
    }
    
    // Get all vehicle catalog entries matching a filter
    static array<SCR_EntityCatalogEntry> GetVehicleEntriesByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary)
    {
        array<SCR_EntityCatalogEntry> result = {};
        
        if (allowCivilian)
        {
            array<SCR_EntityCatalogEntry> civilianCars = GetVehicleEntries(IA_VehicleType.CivilianCar, faction);
            array<SCR_EntityCatalogEntry> civilianTrucks = GetVehicleEntries(IA_VehicleType.CivilianTruck, faction);
            
            foreach (SCR_EntityCatalogEntry entry : civilianCars)
                result.Insert(entry);
                
            foreach (SCR_EntityCatalogEntry entry : civilianTrucks)
                result.Insert(entry);
        }
        
        if (allowMilitary)
        {
            array<SCR_EntityCatalogEntry> militaryTransports = GetVehicleEntries(IA_VehicleType.MilitaryTransport, faction);
            array<SCR_EntityCatalogEntry> militaryPatrols = GetVehicleEntries(IA_VehicleType.MilitaryPatrol, faction);
            array<SCR_EntityCatalogEntry> militaryApcs = GetVehicleEntries(IA_VehicleType.MilitaryAPC, faction);
            
            foreach (SCR_EntityCatalogEntry entry : militaryTransports)
                result.Insert(entry);
                
            foreach (SCR_EntityCatalogEntry entry : militaryPatrols)
                result.Insert(entry);
                
            foreach (SCR_EntityCatalogEntry entry : militaryApcs)
                result.Insert(entry);
        }
        
        return result;
    }
    
    // Get all vehicle prefabs matching a filter
    static array<string> GetVehiclesByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary)
    {
        array<string> result = {};
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntriesByFilter(faction, allowCivilian, allowMilitary);
        
        foreach (SCR_EntityCatalogEntry entry : entries)
        {
            result.Insert(entry.GetPrefab());
        }
        
        return result;
    }
}; 