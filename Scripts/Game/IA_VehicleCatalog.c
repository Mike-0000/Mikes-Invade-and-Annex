class IA_VehicleCatalog
{
    // Get random vehicle labels based on faction and random selection
    static array<EEditableEntityLabel> GetRandomVehicleLabels(IA_Faction faction)
    {
        array<EEditableEntityLabel> labels = {};
        
        // Add faction label
        switch (faction)
        {
            case IA_Faction.US:
                labels.Insert(EEditableEntityLabel.FACTION_US);
                break;
            case IA_Faction.USSR:
                labels.Insert(EEditableEntityLabel.FACTION_USSR);
                break;
            case IA_Faction.CIV:
                labels.Insert(EEditableEntityLabel.FACTION_CIV);
                break;
            case IA_Faction.FIA:
                labels.Insert(EEditableEntityLabel.FACTION_FIA);
                break;
        }
        
        // Generate random vehicle type based on faction
        int randInt = IA_Game.rng.RandInt(0, 3);
        if (faction == IA_Faction.USSR)
        {
            switch (randInt)
            {
                case 0: // Unarmed Transport
                	labels.Insert(EEditableEntityLabel.TRAIT_PASSENGERS_LARGE);
					labels.Insert(EEditableEntityLabel.VEHICLE_TRUCK);
                    break;
                case 1:
					labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
					labels.Insert(EEditableEntityLabel.VEHICLE_CAR);
					break;
                case 2: // Armed Vic
					labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
                    break;
                case 3: // Armor Vic
                    labels.Insert(EEditableEntityLabel.TRAIT_ARMOR);
                    break;
            }
        }
        else if (faction == IA_Faction.US)
        {
			randInt = IA_Game.rng.RandInt(0, 4);
            switch (randInt)
            {
                case 0: // Unarmed Transport
                    labels.Insert(EEditableEntityLabel.VEHICLE_TRUCK);
                    labels.Insert(EEditableEntityLabel.TRAIT_PASSENGERS_LARGE);
                    labels.Insert(EEditableEntityLabel.TRAIT_UNARMED);
                    break;
                case 1:
                case 2:
                case 3: // Armed Vic
                    labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
                    break;
                case 4: // Armor Vic
                    labels.Insert(EEditableEntityLabel.TRAIT_ARMOR);
                    break;
            }
        }
        else if (faction == IA_Faction.CIV)
        {
            switch (randInt)
            {
                case 0:
                case 1: // Civilian Car
                case 2:
                case 3:
                case 4: // Civilian Truck
                    labels.Insert(EEditableEntityLabel.VEHICLE_CAR);
                    break;
            }
        }
        else if (faction == IA_Faction.FIA)
        {
            switch (randInt)
            {
                case 0: // Unarmed Transport
                    labels.Insert(EEditableEntityLabel.VEHICLE_TRUCK);
                    labels.Insert(EEditableEntityLabel.TRAIT_PASSENGERS_LARGE);
                    labels.Insert(EEditableEntityLabel.TRAIT_UNARMED);
                    break;
                case 1:
                case 2:
                case 3: // Armed Vic
                    labels.Insert(EEditableEntityLabel.TRAIT_ARMED);
                    break;
                case 4: // Armor Vic
                    labels.Insert(EEditableEntityLabel.TRAIT_ARMOR);
                    break;
            }
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
    
    // Get catalog entries for a specific faction with random vehicle type
    static array<SCR_EntityCatalogEntry> GetVehicleEntries(IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries called for faction " + faction, LogLevel.NORMAL);
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
        array<EEditableEntityLabel> includedLabels = GetRandomVehicleLabels(faction);
        array<EEditableEntityLabel> excludedLabels = {EEditableEntityLabel.VEHICLE_HELICOPTER};
        
        // For civilian vehicles, exclude military faction vehicles
        if (faction == IA_Faction.CIV)
        {
            // For actual CIV faction, just ensure we're not getting military vehicles
            excludedLabels.Insert(EEditableEntityLabel.FACTION_US);
            excludedLabels.Insert(EEditableEntityLabel.FACTION_USSR);
            excludedLabels.Insert(EEditableEntityLabel.FACTION_FIA);
        }
        
        // Get the filtered list of vehicles
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Getting filtered entity list", LogLevel.NORMAL);
        entityCatalog.GetFullFilteredEntityList(result, includedLabels, excludedLabels);
        Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Found " + result.Count() + " entries", LogLevel.NORMAL);
        return result;
    }
    
    // Get a random catalog entry for a specific faction
    static SCR_EntityCatalogEntry GetRandomVehicleEntry(IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry called for faction " + faction, LogLevel.NORMAL);
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction);
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
    static string GetRandomVehiclePrefab(IA_Faction faction)
    {
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab called for faction " + faction, LogLevel.NORMAL);
        SCR_EntityCatalogEntry entry = GetRandomVehicleEntry(faction);
        if (!entry)
        {
            Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: No entry found", LogLevel.WARNING);
            return "";
        }
            
        string prefab = entry.GetPrefab();
        Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: Selected prefab " + prefab, LogLevel.NORMAL);
        return prefab;
    }
    
    // Get all vehicle prefab resource names for a specific faction
    static array<string> GetVehiclePrefabList(IA_Faction faction)
    {
        array<string> result = {};
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction);
        
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
        
        // For civilian vehicles
        if (allowCivilian && faction == IA_Faction.CIV)
        {
            array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction);
            foreach (SCR_EntityCatalogEntry entry : entries)
                result.Insert(entry);
        }
        
        // For military vehicles
        if (allowMilitary && faction != IA_Faction.CIV)
        {
            array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction);
            foreach (SCR_EntityCatalogEntry entry : entries)
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