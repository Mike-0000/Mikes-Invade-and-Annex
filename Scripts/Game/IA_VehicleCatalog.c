class IA_VehicleCatalog
{
    // Get random vehicle labels based on faction and random selection
    static array<EEditableEntityLabel> GetRandomVehicleLabels(IA_Faction faction)
    {
        array<EEditableEntityLabel> labels = {};

        // Generate random vehicle type based on faction
        int randInt = IA_Game.rng.RandInt(0, 4);
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
                case 3:
				case 4: // Armor Vic
                    labels.Insert(EEditableEntityLabel.VEHICLE_APC);
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
			 // Dont add any labels right now for most diversity.
            switch (randInt)
            {
                case 0:
                case 1: 
                case 2:
                case 3:
                case 4: // Civilian Truck
                    //labels.Insert(EEditableEntityLabel.VEHICLE_CAR);
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
    static array<SCR_EntityCatalogEntry> GetVehicleEntries(IA_Faction faction, Faction AreaFaction)
    {
       //Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries called for faction " + faction, LogLevel.NORMAL);
        array<SCR_EntityCatalogEntry> result = {};
        
        // Get the faction manager
        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager)
        {
           //Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get faction manager", LogLevel.WARNING);
            return result;
        }
		
		array<EEditableEntityLabel> includedLabels = GetRandomVehicleLabels(faction);
        array<EEditableEntityLabel> excludedLabels = {EEditableEntityLabel.VEHICLE_HELICOPTER};
		
		// Check for config overrides first - they take precedence over AreaFaction
		IA_Config config = IA_MissionInitializer.GetGlobalConfig();
		array<Faction> EnemyFactions = {};
		
		if (config && config.m_sDesiredEnemyVehicleFactionKeys && !config.m_sDesiredEnemyVehicleFactionKeys.IsEmpty()) {
			Print("[IA_VehicleCatalog] Using config override for enemy vehicle factions", LogLevel.NORMAL);
			
			foreach (string factionKey : config.m_sDesiredEnemyVehicleFactionKeys) {
				Faction configFaction = factionManager.GetFactionByKey(factionKey);
				if (configFaction) {
					// Verify faction has vehicles before adding
					SCR_Faction scrConfigFaction = SCR_Faction.Cast(configFaction);
					if (scrConfigFaction) {
						SCR_EntityCatalog configEntityCatalog = scrConfigFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
						if (configEntityCatalog) {
							array<EEditableEntityLabel> configExcludedLabels = {};
							array<EEditableEntityLabel> configIncludedLabels = {};
							array<SCR_EntityCatalogEntry> vehicleEntries = {};
							configEntityCatalog.GetFullFilteredEntityList(vehicleEntries, configIncludedLabels, configExcludedLabels);
							
							if (vehicleEntries.Count() >= 1) {
								EnemyFactions.Insert(configFaction);
								Print("[IA_VehicleCatalog] Added vehicle faction '" + factionKey + "' from config", LogLevel.NORMAL);
							} else {
								Print("[IA_VehicleCatalog] Skipping vehicle faction '" + factionKey + "' - insufficient vehicles (" + vehicleEntries.Count() + ")", LogLevel.WARNING);
							}
						}
					}
				} else {
					Print("[IA_VehicleCatalog] Warning: Vehicle faction key '" + factionKey + "' not found", LogLevel.WARNING);
				}
			}
			
			if (EnemyFactions.IsEmpty()) {
				Print("[IA_VehicleCatalog] No valid vehicle factions from config, falling back to default behavior", LogLevel.WARNING);
			}
		}
		
		// Default behavior when no config, config is empty, or no valid config factions
		if (EnemyFactions.IsEmpty()) {
			// First try AreaFaction if it has vehicles
			if(AreaFaction && faction != IA_Faction.CIV){
				SCR_Faction scr_AreaFaction = SCR_Faction.Cast(AreaFaction);
				if (scr_AreaFaction) {
					SCR_EntityCatalog areaFactionEntityCatalog = scr_AreaFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
					if (areaFactionEntityCatalog) {
						array<SCR_EntityCatalogEntry> areaFactionEntityCatalogEntryArray = {};
						areaFactionEntityCatalog.GetFullFilteredEntityList(areaFactionEntityCatalogEntryArray, includedLabels, excludedLabels);
						if (areaFactionEntityCatalogEntryArray.Count() > 0){
							Print("[IA_VehicleCatalog] No config override, using Area Faction for vehicle query", LogLevel.NORMAL);
							return areaFactionEntityCatalogEntryArray;
						}
					}
				}
			}
			
			// Fallback to general enemy faction detection
			Print("[IA_VehicleCatalog] Using default enemy vehicle faction detection", LogLevel.NORMAL);
			SCR_Faction USFaction = SCR_Faction.Cast(factionManager.GetFactionByKey("US"));
			array<Faction> AllFactions = {};
			factionManager.GetFactionsList(AllFactions);
			
			foreach(Faction currentFaction : AllFactions){
				if(USFaction.IsFactionEnemy(currentFaction))
					EnemyFactions.Insert(currentFaction);
			}
		}
		array<SCR_EntityCatalog> finalEntityCatalogArray = {};
        foreach(Faction currentFaction : EnemyFactions){
	        // Get the faction by key
	        string factionKey = GetFactionKey(faction);
	       //Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Looking for faction with key " + factionKey, LogLevel.NORMAL);
	        SCR_Faction scrFaction = SCR_Faction.Cast(currentFaction);
	        if (!scrFaction)
	        {
	            continue;
	        }
		
			finalEntityCatalogArray.Insert(scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true));
	        
		
		
		
		}
        if (finalEntityCatalogArray.IsEmpty())
	        {
	           Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Failed to get entity catalogs", LogLevel.WARNING);
	            return result;
	        }
        // Get the entity catalog for vehicles
       //Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Getting vehicle catalog for faction " + factionKey, LogLevel.NORMAL);
        
        
        // Setup the filter labels

        array<SCR_EntityCatalogEntry> finalEntityCatalogEntries = {};
        // For civilian vehicles, exclude military faction vehicles
        if (faction == IA_Faction.CIV)
        {
            // For actual CIV faction, just ensure we're not getting military vehicles
            excludedLabels.Insert(EEditableEntityLabel.FACTION_US);
            excludedLabels.Insert(EEditableEntityLabel.FACTION_USSR);
            excludedLabels.Insert(EEditableEntityLabel.FACTION_FIA);
			SCR_Faction scr_CIVFaction = SCR_Faction.Cast(factionManager.GetFactionByKey("CIV"));

			SCR_EntityCatalog CIVEntityCatalog = scr_CIVFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
			
			CIVEntityCatalog.GetFullFilteredEntityList(finalEntityCatalogEntries, includedLabels, excludedLabels);
			return finalEntityCatalogEntries;
        }
        
        // Get the filtered list of vehicles
       //Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Getting filtered entity list", LogLevel.NORMAL);
		
		foreach(SCR_EntityCatalog currentEntityCatalog : finalEntityCatalogArray){
			array<SCR_EntityCatalogEntry> currentEntityCatalogEntries = {};
        	currentEntityCatalog.GetFullFilteredEntityList(currentEntityCatalogEntries, includedLabels, excludedLabels);
			finalEntityCatalogEntries.InsertAll(currentEntityCatalogEntries);
       		//Print("[DEBUG] IA_VehicleCatalog.GetVehicleEntries: Found " + result.Count() + " entries", LogLevel.NORMAL);
			
		}
        return finalEntityCatalogEntries;
    }
    
    // Get a random catalog entry for a specific faction
    static SCR_EntityCatalogEntry GetRandomVehicleEntry(IA_Faction faction, Faction AreaFaction)
    {
       //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry called for faction " + faction, LogLevel.NORMAL);
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction, AreaFaction);
        if (entries.IsEmpty())
        {
           //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry: No entries found", LogLevel.WARNING);
            return null;
        }
            
        int idx = IA_Game.rng.RandInt(0, entries.Count());
       //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehicleEntry: Found " + entries.Count() + " entries, selected index " + idx, LogLevel.NORMAL);
        return entries[idx];
    }
    
    // Get a random vehicle prefab resource name as a string
    static string GetRandomVehiclePrefab(IA_Faction faction, Faction AreaFaction)
    {
       //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab called for faction " + faction, LogLevel.NORMAL);
        SCR_EntityCatalogEntry entry = GetRandomVehicleEntry(faction, AreaFaction);
        if (!entry)
        {
           //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: No entry found", LogLevel.WARNING);
            return "";
        }
            
        string prefab = entry.GetPrefab();
       //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefab: Selected prefab " + prefab, LogLevel.NORMAL);
        return prefab;
    }
    
    // Get all vehicle prefab resource names for a specific faction
    static array<string> GetVehiclePrefabList(IA_Faction faction, Faction AreaFaction)
    {
        array<string> result = {};
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction, AreaFaction);
        
        foreach (SCR_EntityCatalogEntry entry : entries)
        {
            result.Insert(entry.GetPrefab());
        }
        
        return result;
    }
    
    // Get all vehicle catalog entries matching a filter
    static array<SCR_EntityCatalogEntry> GetVehicleEntriesByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary, Faction AreaFaction)
    {
        array<SCR_EntityCatalogEntry> result = {};
        
        // For civilian vehicles
        if (allowCivilian && faction == IA_Faction.CIV)
        {
            array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction, AreaFaction);
            foreach (SCR_EntityCatalogEntry entry : entries)
                result.Insert(entry);
        }
        
        // For military vehicles
        if (allowMilitary && faction != IA_Faction.CIV)
        {
            array<SCR_EntityCatalogEntry> entries = GetVehicleEntries(faction, AreaFaction);
            foreach (SCR_EntityCatalogEntry entry : entries)
                result.Insert(entry);
        }
        
        return result;
    }
    
    // Get all vehicle prefabs matching a filter
    static array<string> GetVehiclesByFilter(IA_Faction faction, bool allowCivilian, bool allowMilitary, Faction AreaFaction)
    {
        array<string> result = {};
        array<SCR_EntityCatalogEntry> entries = GetVehicleEntriesByFilter(faction, allowCivilian, allowMilitary, AreaFaction);
        
        foreach (SCR_EntityCatalogEntry entry : entries)
        {
            result.Insert(entry.GetPrefab());
        }
        
        return result;
    }
    
    // Get a random vehicle prefab resource name as a string by specific labels
    static string GetRandomVehiclePrefabBySpecificLabels(EEditableEntityLabel factionLabel, array<EEditableEntityLabel> explicitIncludedTraitLabels, array<EEditableEntityLabel> explicitExcludedTraitLabels)
    {
        //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels called. Faction: %1, Included Traits: %2, Excluded Traits: %3", typename.EnumToString(EEditableEntityLabel, factionLabel), explicitIncludedTraitLabels.Count(), explicitExcludedTraitLabels.Count()), LogLevel.NORMAL);

        array<SCR_EntityCatalogEntry> result = {};
        
        SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager)
        {
            //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Failed to get faction manager", LogLevel.WARNING);
            return "";
        }
        
        // Convert IA_Faction enum style (if it were passed, but we use EEditableEntityLabel for faction now) to key
        // For EEditableEntityLabel, we need to derive the faction key somewhat differently or ensure the label IS a faction key label.
        // Assuming factionLabel is one of EEditableEntityLabel.FACTION_US, EEditableEntityLabel.FACTION_USSR, etc.
        string factionKey = "";
        switch (factionLabel)
        {
            case EEditableEntityLabel.FACTION_US:
                factionKey = "US";
                break;
            case EEditableEntityLabel.FACTION_USSR:
                factionKey = "USSR";
                break;
            case EEditableEntityLabel.FACTION_CIV:
                factionKey = "CIV";
                break;
            case EEditableEntityLabel.FACTION_FIA:
                factionKey = "FIA";
                break;
            default:
                //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Invalid or non-faction label provided as factionLabel: %1", typename.EnumToString(EEditableEntityLabel, factionLabel)), LogLevel.WARNING);
                return ""; // Not a valid faction label
        }
        array<EEditableEntityLabel> includedLabels = {};
        foreach (EEditableEntityLabel traitLabel : explicitIncludedTraitLabels)
        {
            if (traitLabel != 0) // Ensure no "none" or default labels are accidentally included if 0 is used for that
                includedLabels.Insert(traitLabel);
        }
        
        // Use the explicitly passed excluded labels. Ensure it's not null if passed from attribute.
        array<EEditableEntityLabel> finalExcludedLabels = {};
        if (explicitExcludedTraitLabels)
        {
            finalExcludedLabels.InsertAll(explicitExcludedTraitLabels);
        }
		array<Faction> friendlyFactions = {};
        //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Looking for faction with key %1", factionKey), LogLevel.NORMAL);
        SCR_Faction scrFaction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
		scrFaction.GetFriendlyFactions(friendlyFactions, true);
		Faction civFaction = factionManager.GetFactionByKey("CIV");
		foreach(Faction currentFaction : friendlyFactions){
			if(factionKey != "CIV" && currentFaction == civFaction)
				continue;
			SCR_Faction scrCurrentFaction = SCR_Faction.Cast(currentFaction);
			SCR_EntityCatalog entityCatalog = scrCurrentFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
	        if (!entityCatalog)
	        {
	            //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Failed to get vehicle entity catalog for faction %1", factionKey), LogLevel.WARNING);
	            continue;
	        }
       		array<SCR_EntityCatalogEntry> preResult = {};
			entityCatalog.GetFullFilteredEntityList(preResult, includedLabels, finalExcludedLabels);
			result.InsertAll(preResult);
		}
        
		if(result.IsEmpty()){
			SCR_Faction newScrCurrentFaction = SCR_Faction.Cast(factionManager.GetFactionByKey("US"));

			SCR_EntityCatalog newEntityCatalog = newScrCurrentFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.VEHICLE, true);
			newEntityCatalog.GetFullFilteredEntityList(result, includedLabels, finalExcludedLabels);
			
		}
        
        

        
        //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Getting full filtered entity list.");
        //Print("[DEBUG] Final Included Labels Count: " + includedLabels.Count());
        //foreach (EEditableEntityLabel lbl : includedLabels) Print("[DEBUG] Incl: " + typename.EnumToString(EEditableEntityLabel, lbl));
        //Print("[DEBUG] Final Excluded Labels Count: " + finalExcludedLabels.Count());
        //foreach (EEditableEntityLabel lbl : finalExcludedLabels) Print("[DEBUG] Excl: " + typename.EnumToString(EEditableEntityLabel, lbl));

        
        
        //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Found %1 entries after filtering.", result.Count()), LogLevel.NORMAL);

        if (result.IsEmpty())
        {
            //Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: No entries found matching the specific labels.", LogLevel.WARNING);
            return "";
        }
            
        int idx = IA_Game.rng.RandInt(0, result.Count()); // result.Count() is exclusive, so 0 to Count-1
        SCR_EntityCatalogEntry entry = result[idx];
        
        if (!entry)
        {
        	//Print("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Selected entry is null? This should not happen if list is not empty.", LogLevel.ERROR);
        	return "";
        }
        
        string prefab = entry.GetPrefab();
        //Print(string.Format("[DEBUG] IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels: Selected prefab %1", prefab), LogLevel.NORMAL);
        return prefab;
    }
}; 