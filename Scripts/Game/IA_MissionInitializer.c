///////////////////////////////////////////////////////////////////////
// IA_MissionInitializerClass & IA_MissionInitializer
// Place this as a GenericEntity in your world with the script set to IA_MissionInitializer.
// It runs once on the server to set up IA_Game, replication, and dynamic areas/tasks.
///////////////////////////////////////////////////////////////////////
class IA_MissionInitializerClass : GenericEntityClass
{
};


class IA_MissionInitializer : GenericEntity
{
	
	[Attribute(defvalue: "2", desc: "How many groups to assign randomly.")]
    int m_numberOfGroups;

	[Attribute(defvalue: "1000", desc: "Delay in milliseconds between spawning each area instance")]
    int m_spawnDelayMs;

	[Attribute(defvalue: "", desc: "IA Config file to override faction and vehicle settings", params: "conf")]
	ResourceName m_configResource;

	private ref IA_Config m_config;
	private ref array<int> groupsArray;
    // Simple flag to ensure we only run once.
    protected bool m_bInitialized = false;
	//ref array<IA_AreaMarker> m_shuffledMarkers = {};
	private int m_currentIndex = -1;
	private ref array<ref IA_AreaInstance> m_currentAreaInstances; 
	private int m_initialTotalCiviliansInGroup = 0;
	private bool m_initialCiviliansCounted = false;
	private bool m_civilianRevoltActive = false;
	private bool m_runOnce = false;
	private ref IA_AreaGroupManager m_currentAreaGroupManager;
	
	// --- BEGIN ADDED: Artillery Cooldown ---
    static int s_artilleryDisabledUntil = 0;
	// --- END ADDED ---
	
	[RplProp()]
	private bool m_bEnforceRoleRestrictionsReplicated = false;

	// Static reference for global access
	static IA_MissionInitializer s_instance;

	int GetCurrentIndex(){
		return m_currentIndex;
	}
	
	array<int> GetGroupsArray()
	{
	    return groupsArray;
	}
	
	int GetActiveGroup()
	{
	    if (!groupsArray || groupsArray.IsEmpty())
	    {
	        ////Print("[ERROR] IA_MissionInitializer.GetActiveGroup: groupsArray is null or empty!", LogLevel.ERROR);
	        return -1;
	    }
	        
	    if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
	    {
	        ////Print("[ERROR] IA_MissionInitializer.GetActiveGroup: Invalid m_currentIndex " + m_currentIndex + ", array size: " + groupsArray.Count(), LogLevel.ERROR);
	        return -1;
	    }
	    
	    return groupsArray[m_currentIndex];
	}
	void FinishGame(){
	
		SCR_BaseGameMode scr_gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		SCR_FactionManager factMan = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		array<int> factIntArray = {factMan.GetFactionIndex(factMan.GetFactionByKey("US"))};
		SCR_GameModeEndData gamemodeEndData = SCR_GameModeEndData.Create(EGameOverTypes.VICTORY, null, factIntArray);
		scr_gm.EndGameMode(gamemodeEndData);
	
	}
	
	Faction GetRandomEnemyFaction(){
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
        if (!factionManager) {
            return null;
        }

		// Declare configFactions in function scope for later reference
		array<Faction> configFactions = {};

		// Check if we have config overrides for enemy faction keys
		if (m_config && m_config.m_sDesiredEnemyFactionKeys && !m_config.m_sDesiredEnemyFactionKeys.IsEmpty()) {
			Print("[IA_MissionInitializer] Using config override for enemy factions", LogLevel.NORMAL);
			foreach (string factionKey : m_config.m_sDesiredEnemyFactionKeys) {
				Faction faction = factionManager.GetFactionByKey(factionKey);
				if (faction) {
					// Verify faction has enough characters before adding
					SCR_Faction scrFaction = SCR_Faction.Cast(faction);
					if (scrFaction) {
						SCR_EntityCatalog entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
						if (entityCatalog) {
							array<EEditableEntityLabel> excludedLabels = {};
							array<EEditableEntityLabel> includedLabels = {};
							array<SCR_EntityCatalogEntry> characterEntries = {};
							entityCatalog.GetFullFilteredEntityList(characterEntries, includedLabels, excludedLabels);
							
							if (characterEntries.Count() >= 1) {
								configFactions.Insert(faction);
								Print("[IA_MissionInitializer] Added faction '" + factionKey + "' from config", LogLevel.NORMAL);
							} else {
								Print("[IA_MissionInitializer] Skipping faction '" + factionKey + "' - insufficient characters (" + characterEntries.Count() + ")", LogLevel.ERROR);
							}
						}
					}
				} else {
					Print("[IA_MissionInitializer] Warning: Faction key '" + factionKey + "' not found", LogLevel.WARNING);
				}
			}
			
			if (!configFactions.IsEmpty()) {
				return configFactions[Math.RandomInt(0, configFactions.Count())];
			} else {
				Print("[IA_MissionInitializer] No valid factions from config, falling back to default behavior", LogLevel.WARNING);
			}
		}

		// Default behavior when no config or config is empty
		Print("[IA_MissionInitializer] Using default enemy faction detection", LogLevel.NORMAL);
		Faction USFaction = factionManager.GetFactionByKey("US");
        array<Faction> actualFactions = {};
		array<Faction> factionGet = {};
        factionManager.GetFactionsList(factionGet);
		foreach (Faction currentFaction : factionGet){
			SCR_EntityCatalog entityCatalog;
			SCR_Faction scrFaction = SCR_Faction.Cast(currentFaction);
			entityCatalog = scrFaction.GetFactionEntityCatalogOfType(EEntityCatalogType.CHARACTER, true);
			if (!scrFaction)
	            continue;
			if (!entityCatalog)
				continue;
			array<EEditableEntityLabel> excludedLabels = {};
			array<EEditableEntityLabel> includedLabels = {};
			array<SCR_EntityCatalogEntry> characterEntries = {};
			entityCatalog.GetFullFilteredEntityList(characterEntries, includedLabels, excludedLabels);
				
			if(USFaction.IsFactionEnemy(currentFaction) && currentFaction != factionManager.GetFactionByKey("FIA") && characterEntries.Count() >= 4)
				actualFactions.Insert(currentFaction); // Build List of Enemy Factions
		}
		if(actualFactions.Count() > 1 && configFactions.IsEmpty()){ // If modded content and no config overrides
			if(Math.RandomInt(1,20) > 1) // 95% chance to use a modded faction
				actualFactions.Remove(actualFactions.Find(factionManager.GetFactionByKey("USSR")));
		}
		return actualFactions[Math.RandomInt(0, actualFactions.Count()-1)];
	}
	
	void ProceedToNextZone()
	{
	    if (!groupsArray || groupsArray.IsEmpty())
	    {
	        ////Print("[ERROR] IA_MissionInitializer.ProceedToNextZone: groupsArray is null or empty!", LogLevel.ERROR);
	        return;
	    }

        // --- BEGIN ADDED: Clear existing areas from IA_Game ---    
        IA_Game gameInstance = IA_Game.Instantiate();
        if (gameInstance)
        {
            //Print("[IA_MissionInitializer.ProceedToNextZone] Clearing existing game areas before initializing new zone group.", LogLevel.NORMAL);
            gameInstance.ClearAllAreas();
        }
        else
        {
            Print("[IA_MissionInitializer.ProceedToNextZone] Failed to get IA_Game instance, cannot clear areas.", LogLevel.ERROR);
        }
        
        // Reset all area markers for the new zone group
        IA_AreaMarker.ResetAllMarkersForNewGroup();
        // --- END ADDED ---
	    
	    if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
	    {
			Print("GAME FINISHED!!! Waiting 30 seconds and ending gamemode.", LogLevel.WARNING);
	        GetGame().GetCallqueue().CallLater(FinishGame, 30000);
	        return;
	    }
		
		if (m_currentAreaGroupManager)
		{
			delete m_currentAreaGroupManager;
			m_currentAreaGroupManager = null;
		}
		
		m_initialTotalCiviliansInGroup = 0;
		m_initialCiviliansCounted = false;
		m_civilianRevoltActive = false;

	    Faction nextAreaFaction = GetRandomEnemyFaction();
		Print("Next Faction is = " +nextAreaFaction.GetFactionName(), LogLevel.NORMAL);
	    int currentGroup = groupsArray[m_currentIndex];
	    ////Print("[DEBUG_ZONE_GROUP] Proceeding to zone group " + currentGroup + " (index " + m_currentIndex + " of " + groupsArray.Count() + ")", LogLevel.WARNING);
	    
	    // Update the active group in the vehicle manager
	    IA_VehicleManager.SetActiveGroup(currentGroup);
	    
	    // --- BEGIN ADDED: Set Active Group in IA_Game ---
	    IA_Game.SetActiveGroupID(currentGroup);
	    // --- END ADDED ---
	    
	    // m_currentAreaInstances.Clear(); // Clear for the new zone group - MOVED LATER
	    // array<IA_AreaMarker> markersInGroup = {}; - MOVED LATER
        // array<IA_AreaMarker> allMarkers = IA_AreaMarker.GetAllMarkers(); - MOVED LATER
        // foreach(IA_AreaMarker marker : allMarkers) - MOVED LATER
        // { - MOVED LATER
        //     if (marker && marker.m_areaGroup == currentGroup) - MOVED LATER
        //     { - MOVED LATER
        //         markersInGroup.Insert(marker); - MOVED LATER
        //     } - MOVED LATER
        // } - MOVED LATER
	    
	    // int accumulatedDelay = 0; - MOVED LATER
        // int markerIndex = 0; - MOVED LATER
	    // foreach(IA_AreaMarker marker : markersInGroup) - MOVED LATER
        // { - MOVED LATER
	    //     if (!marker) - MOVED LATER
	    //     { - MOVED LATER
	    //         continue; - MOVED LATER
	    //     } - MOVED LATER
	    //     
        //     // Schedule the area instance creation and AI spawning - MOVED LATER
        //     GetGame().GetCallqueue().CallLater(this._SpawnAreaInstanceWithDelay, accumulatedDelay, false, marker, nextAreaFaction, currentGroup); - MOVED LATER
        //     ////Print("[DEBUG_ZONE_GROUP] Scheduled initialization for zone: " + marker.GetAreaName() + " in group " + currentGroup + " with delay " + accumulatedDelay + "ms", LogLevel.NORMAL); - MOVED LATER
        //     accumulatedDelay += m_spawnDelayMs; - MOVED LATER
        //     markerIndex++; - MOVED LATER
	    // } - MOVED LATER

	    // --- BEGIN REORDERED CODE FOR INITIAL SCALING --- 
	    bool isFirstObjectiveGroup = (m_currentIndex == 0);
	    // Disabling logic is now placed before area spawning for the first group.
	    // The EnableInitialObjectiveScaling() is now in IA_Game.Init()
	    if (isFirstObjectiveGroup)
	    {
	        // The accumulatedDelay for disabling scaling will be calculated after scheduling areas.
	        // For now, we just note it's the first group.
	    }

	    m_currentAreaInstances.Clear(); // Clear for the new zone group
	    array<IA_AreaMarker> markersInGroup = {};
        array<IA_AreaMarker> allMarkers = IA_AreaMarker.GetAllMarkers();
        foreach(IA_AreaMarker marker : allMarkers)
        {
            if (marker && marker.m_areaGroup == currentGroup && marker.GetAreaType() != IA_AreaType.DefendObjective)
            {
                markersInGroup.Insert(marker);
            }
        }
	    
	    int accumulatedDelay = 0;
        int markerIndex = 0;
	    foreach(IA_AreaMarker marker : markersInGroup)
        {
	        if (!marker)
	        {
	            continue;
	        }
	        
            // Schedule the area instance creation and AI spawning
            GetGame().GetCallqueue().CallLater(this._SpawnAreaInstanceWithDelay, accumulatedDelay, false, marker, nextAreaFaction, currentGroup);
            ////Print("[DEBUG_ZONE_GROUP] Scheduled initialization for zone: " + marker.GetAreaName() + " in group " + currentGroup + " with delay " + accumulatedDelay + "ms", LogLevel.NORMAL);
            accumulatedDelay += m_spawnDelayMs;
            markerIndex++;
        }
	    // --- END REORDERED CODE FOR INITIAL SCALING --- 
	    
	    // --- BEGIN ADDED: Disable initial objective scaling after first group setup ---
	    // This should be scheduled after all areas in the first group might have started their setup
	    if (isFirstObjectiveGroup) // Check again if it was the first group
	    {
	        GetGame().GetCallqueue().CallLater(IA_Game.DisableInitialObjectiveScaling, accumulatedDelay + 60000, false); // Wait 60s after last area spawn scheduled
	    }
	    // --- END ADDED ---
	    
	    ////Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " contains " + zonesInGroup + " zones to capture", LogLevel.WARNING);
        Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " contains " + markersInGroup.Count() + " zones to capture. Scheduled over " + accumulatedDelay + "ms", LogLevel.WARNING);
	    
	    // Schedule the spawning of additional group vehicles after all area instances have been scheduled
        if (!markersInGroup.IsEmpty()) // Only if there are areas to spawn
        {
            GetGame().GetCallqueue().CallLater(this._SpawnGroupVehiclesWithDelay, accumulatedDelay, false, nextAreaFaction);
            ////Print("[DEBUG_ZONE_GROUP] Scheduled SpawnVehiclesAtAllSpawnPoints for zone group " + currentGroup + " after " + accumulatedDelay + "ms", LogLevel.NORMAL);
        }
	    
        // CheckCurrentZoneComplete needs m_currentAreaInstances to be populated.
        // Since population is now delayed, this initial call might run on an empty/partially empty list.
        // Consider scheduling this after all areas are expected to be created, or make CheckCurrentZoneComplete robust to this.
        // For now, keeping original timing, but this is a potential point of failure if it expects synchronous population.
	    GetGame().GetCallqueue().CallLater(CheckCurrentZoneComplete, accumulatedDelay + 5000, true); // Start checking 5s after last area is scheduled
	    ////Print("[DEBUG_ZONE_GROUP] Started monitoring completion for group " + currentGroup + " (check starts in " + (accumulatedDelay + 5000) + "ms)", LogLevel.NORMAL);
	}
    void Shuffle(array<int> arr)
    {
        int n = arr.Count();
        for (int i = n - 1; i > 0; i--)
        {
            int j = Math.RandomInt(0, i + 1);
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
	
	void InitializeNow()
{
    // --- BEGIN ADDED: Clear all area definitions at mission start ---
    IA_Game.ClearAllAreaDefinitions();
    //Print("[IA_MissionInitializer.InitializeNow] Cleared all area definitions from IA_Game.s_allAreas.", LogLevel.NORMAL);
    // --- END ADDED ---

    ////Print("[DEBUG] IA_MissionInitializer: InitializeNow started.", LogLevel.NORMAL);
	groupsArray = new array<int>;
	m_currentAreaInstances = new array<ref IA_AreaInstance>;
	for (int i = 0; i < m_numberOfGroups; i++)
        {
            groupsArray.Insert(i);
        }
//    m_shuffledMarkers = {};
    array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
    if (markers.IsEmpty())
    {
        ////Print("[WARNING] No IA_AreaMarkers found!", LogLevel.WARNING);
        return;
    }
    
    // Check for duplicate marker names
    ////Print("[DEBUG_MARKER_NAMES] Checking for duplicate marker names...", LogLevel.WARNING);
    ref map<string, int> nameCount = new map<string, int>();
    foreach(IA_AreaMarker marker : markers) {
        if (!marker) continue;
        
        string name = marker.GetAreaName();
        if (nameCount.Contains(name)) {
            nameCount[name] = nameCount[name] + 1;
        } else {
            nameCount[name] = 1;
        }
    }
    
    foreach(string name, int count : nameCount) {
        if (count > 1) {
            ////Print("[DEBUG_MARKER_NAMES] WARNING: Found duplicate marker name: " + name + " (appears " + count + " times)", LogLevel.ERROR);
        } else {
            ////Print("[DEBUG_MARKER_NAMES] Marker name: " + name + " (unique)", LogLevel.NORMAL);
        }
    }
    
    // Initialize the vehicle manager
    Resource vehicleManagerRes = Resource.Load("{07F25000A2274994}Components/VehicleManager.et");
    if (vehicleManagerRes)
    {
        EntitySpawnParams params = EntitySpawnParams();
        GetGame().SpawnEntityPrefab(vehicleManagerRes, null, params);
        ////Print("[DEBUG] IA_VehicleManager spawned.", LogLevel.NORMAL);
    }
    else
    {
        ////Print("[WARNING] Failed to load IA_VehicleManager resource!", LogLevel.WARNING);
    }
		
/*
    // Randomize order
    while (!markers.IsEmpty())
    {
        int i = IA_Game.rng.RandInt(0, markers.Count());
        m_shuffledMarkers.Insert(markers[i]);
        markers.Remove(i);
    }*/
	Shuffle(groupsArray);
    m_currentIndex = 0;
    ProceedToNextZone();
}

	void CivilianRevoltInit()
	{
		if (m_civilianRevoltActive)
			return;
		m_civilianRevoltActive = true;
		
		Print("[IA_MissionInitializer] Civilian revolt initiated.", LogLevel.NORMAL);

		// Notification for revolt start (30s delay)
		GetGame().GetCallqueue().CallLater(TriggerGlobalNotification, 30000, false, "CivilianRevoltStarted", "Civilian Revolt");

		if (!m_currentAreaInstances)
			return;

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
		{
			Print("[IA_MissionInitializer] CivilianRevoltInit: Faction manager not found!", LogLevel.ERROR);
			return;
		}
		Faction ussrFaction = factionManager.GetFactionByKey("USSR");
		if (!ussrFaction)
		{
			Print("[IA_MissionInitializer] CivilianRevoltInit: USSR Faction not found!", LogLevel.ERROR);
			return;
		}

		foreach (IA_AreaInstance instance : m_currentAreaInstances)
		{
			if (!instance)
				continue;

			// Trigger a large reinforcement wave for this area with a 180-second delay.
			GetGame().GetCallqueue().CallLater(instance.SpawnCivilianRevoltReinforcements, 180 * 1000, false);

			array<ref IA_AiGroup> civilianGroups = instance.GetCivilianGroups();
			foreach (IA_AiGroup civGroup : civilianGroups)
			{
				if (!civGroup)
					continue;
				
				// Change the IA_AiGroup's underlying SCR_AIGroup faction
				SCR_AIGroup scrAIGroup = civGroup.GetSCR_AIGroup();
				if (scrAIGroup)
				{
					scrAIGroup.SetFaction(ussrFaction);
				}
				
				// Arm the revolting civilians
                civGroup.ArmGroupWithPistols();
			}
		}

		// Notification for reinforcements sighted (180s wave spawn + 30s delay)
		GetGame().GetCallqueue().CallLater(TriggerGlobalNotification, 210000, false, "CivilianRevoltReinforcements", "Civilian Reinforcements");
	}


	void CheckCurrentZoneComplete()
	{
		// Safety check for array access
		if (!groupsArray || groupsArray.IsEmpty())
		{
			//Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: groupsArray is null or empty!", LogLevel.ERROR);
			return;
		}
		
		if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
		{
			//Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: Invalid m_currentIndex " + m_currentIndex + ", array size: " + groupsArray.Count(), LogLevel.ERROR);
			return;
		}
		
		m_initialTotalCiviliansInGroup = 0;
		int totalCiviliansKilledByPlayer = 0;
		if (m_currentAreaInstances)
		{
			foreach (ref IA_AreaInstance instance : m_currentAreaInstances)
			{
				if (instance && !instance.IsForSideObjective())
				{
					m_initialTotalCiviliansInGroup += instance.GetInitialCivilianCount();
					totalCiviliansKilledByPlayer += instance.GetCiviliansKilledByPlayer();
				}
			}
		}

		Print(string.Format("[IA_MissionInitializer] Civilian Revolt Check for Group %1: %2 civilians killed by players out of %3 initial civilians.", 
			groupsArray[m_currentIndex], totalCiviliansKilledByPlayer, m_initialTotalCiviliansInGroup), LogLevel.NORMAL);
		
		if (m_initialTotalCiviliansInGroup > 0)
		{
			float civilianPercentageKilledByPlayer = 0;
			if (m_initialTotalCiviliansInGroup > 0)
				civilianPercentageKilledByPlayer = totalCiviliansKilledByPlayer / (float)m_initialTotalCiviliansInGroup;
			
			Print(string.Format("[IA_MissionInitializer] Civilian kill percentage by player: %1 (Threshold: 0.11)", civilianPercentageKilledByPlayer), LogLevel.NORMAL);
			
			if (civilianPercentageKilledByPlayer >= 0.11)
			{
				CivilianRevoltInit();
				// More than 16% of civilians have been killed by players.
			}
		}
		
		// --- BEGIN ADDED: Check if defend mission is active and return early ---
		IA_Game gameInstance = IA_Game.Instantiate();
		if (gameInstance && gameInstance.HasActiveDefendMission())
		{
			//Print("[IA_MissionInitializer] Defend mission is active, waiting for completion before checking zones", LogLevel.DEBUG);
			return;
		}
		// --- END ADDED ---
		
		////Print("Running CheckCurrentZoneComplete",LogLevel.NORMAL);
		if (!m_currentAreaInstances)
		{
			//Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: m_currentAreaInstances is null!", LogLevel.ERROR);
			return;
		}
		
		if (!m_currentAreaGroupManager && m_currentAreaInstances)
        {
            m_currentAreaGroupManager = new IA_AreaGroupManager(m_currentAreaInstances);
			Print("[AreaGroupManager] Initialized new manager for area group " + groupsArray[m_currentIndex], LogLevel.NORMAL);
        }

        if (m_currentAreaGroupManager)
		{
			Print("[AreaGroupManager] Performing periodic check for area group " + groupsArray[m_currentIndex], LogLevel.NORMAL);
            int currentTime = System.GetUnixTime();
            if (s_artilleryDisabledUntil > 0 && currentTime < s_artilleryDisabledUntil)
            {
                Print(string.Format("[IA_MissionInitializer] Skipping artillery check due to side objective cooldown. %1 seconds remaining.", s_artilleryDisabledUntil - currentTime), LogLevel.DEBUG);
            }
            else 
            {
                m_currentAreaGroupManager.ArtilleryStrikeTask();
            }
		}
		
		////Print("Running CheckCurrentZoneComplete 1",LogLevel.NORMAL);
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		array<bool> zoneCompletionStatus = {}; // Track completion for each zone in current group
		
		int currentGroup = groupsArray[m_currentIndex];
		
		// Debug info for area instances
		
		// Debug info for markers in current group
		
		// First, find all markers for current group, count them, and initialize their completion status tracking
		int amountOfZones = 0;
		foreach(IA_AreaMarker marker_counter : markers) { // Changed loop variable name for clarity
			if(!marker_counter || marker_counter.m_areaGroup != currentGroup || marker_counter.GetAreaType() == IA_AreaType.DefendObjective)
				continue;
				
			// Add this zone to our tracking array, initialized to false
			zoneCompletionStatus.Insert(false);
			amountOfZones++;
		}
		
		////Print("Running CheckCurrentZoneComplete 2",LogLevel.NORMAL);
		// If no markers found for this group, this is an issue based on original logic.
		if(amountOfZones == 0) { // Consistent with original check on zoneCompletionStatus.Count()
			//Print("[ERROR] No markers found for group " + currentGroup + ". Cannot check completion.", LogLevel.ERROR);
			return;
		}
		
		////Print("Running CheckCurrentZoneComplete 3",LogLevel.NORMAL);
		// Now check each marker in the current group
		int currentZoneIndex = 0; // This will be an index for m_currentAreaInstances and zoneCompletionStatus
		int actualCompletedZones = 0; // Accurate count of zones currently meeting completion criteria
		
		////Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " has " + amountOfZones + " zones to complete.", LogLevel.NORMAL);
		
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != currentGroup || marker.GetAreaType() == IA_AreaType.DefendObjective)
				continue;
			
			////Print("Running CheckCurrentZoneComplete 3.5 for marker: " + marker.GetAreaName(),LogLevel.NORMAL);
			// Ensure currentZoneIndex is within bounds for both m_currentAreaInstances and zoneCompletionStatus
			if(currentZoneIndex >= m_currentAreaInstances.Count() || currentZoneIndex >= zoneCompletionStatus.Count()) {
				//Print("[ERROR] Index out of bounds. Marker/Instance/StatusArray desync for group " + currentGroup + " at index " + currentZoneIndex, LogLevel.ERROR);
				return; // Stop processing to prevent crash
			}
			
			////Print("Running CheckCurrentZoneComplete 3.75",LogLevel.NORMAL);
			ref IA_AreaInstance instance = m_currentAreaInstances[currentZoneIndex];
			if(!instance) {
			    //Print("[WARNING] Null IA_AreaInstance at index " + currentZoneIndex + " for group " + currentGroup, LogLevel.WARNING);
			    currentZoneIndex++; // Increment to process next marker correctly
			    continue;
			}
				
			// Get US faction score (now on 0-1000 scale, representing 0-120 seconds)
			float factionScore = marker.GetFactionScore("US");
			//Print("[DEBUG_ZONE_SCORE_DEBUG] Marker name: " + marker.GetAreaName() + ", US faction score: " + factionScore + "/1000", LogLevel.WARNING);
			
			if(factionScore >= 1000) { // Score of 1000 = 120 seconds completed
				//Print("[DEBUG_ZONE_GROUP] Zone " + marker.GetAreaName() + " (idx " + currentZoneIndex + ") in group " + currentGroup + " IS complete (Score: " + factionScore + ").", LogLevel.WARNING);
				actualCompletedZones++;
				zoneCompletionStatus[currentZoneIndex] = true; // Update status array
					
				// If there's an active task for this completed zone, finish it.
				if (instance.GetCurrentTaskEntity()) {
					string completedTaskTitle = instance.m_area.GetName();
					//TriggerGlobalNotification("TaskCompleted", completedTaskTitle);
				    //Print("[DEBUG_ZONE_GROUP] Finishing task for completed zone: " + marker.GetAreaName(), LogLevel.NORMAL);
					instance.GetCurrentTaskEntity().Finish();
				}
				
				// Reset capture scores for next time
				marker.ResetCaptureScores();
			}
			else { // factionScore < 1000
				//Print("[DEBUG_ZONE_GROUP] Zone " + marker.GetAreaName() + " (idx " + currentZoneIndex + ") in group " + currentGroup + " is NOT complete (Score: " + factionScore + ").", LogLevel.NORMAL);
				zoneCompletionStatus[currentZoneIndex] = false; // Update status array
				
				// If score is low AND there's no active task (it might have been finished previously or never created),
				// then (re)create the task.
				if (instance.GetCurrentTaskEntity() == null) {
				    //Print("[DEBUG_ZONE_GROUP] Score low for " + marker.GetAreaName() + " and no active task. (Re)creating task.", LogLevel.WARNING);
				    vector pos = marker.GetOrigin();
				    string areaName = marker.GetAreaName(); 
				    string taskTitle = "Capture " + areaName;
				    string taskDesc = "Eliminate enemy presence and secure " + areaName;
				    
				    // --- BEGIN MODIFIED: Check Area Type for Task Title ---
				    IA_AreaType areaType = marker.GetAreaType(); // Assuming marker has GetAreaType()
				    if (areaType == IA_AreaType.RadioTower)
				    {
				        taskTitle = "Destroy " + areaName;
				        taskDesc = "Destroy the " + areaName + " to disrupt enemy communications.";
				    }
				    // --- END MODIFIED ---
				    
				    instance.QueueTask(taskTitle, taskDesc, pos);
				}
			}
			
			currentZoneIndex++;
		}
		
		//Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " progress: " + actualCompletedZones + "/" + amountOfZones + " zones completed.", LogLevel.WARNING);
		
		if(actualCompletedZones >= amountOfZones){ // Use the accurate count
			//Print("[INFO] All " + amountOfZones + " zones in group " + currentGroup + " complete. Proceeding to next.", LogLevel.WARNING);

			// --- BEGIN ADDED: Schedule civilian cleanup for completed zone instances ---
			if (m_currentAreaInstances)
			{
				Print(string.Format("[IA_MissionInitializer.CheckCurrentZoneComplete] Group %1 completed. Scheduling civilian cleanup for %2 area instances.", 
					currentGroup, m_currentAreaInstances.Count()), LogLevel.NORMAL);
				foreach (ref IA_AreaInstance oldInstance : m_currentAreaInstances)
				{
					if (oldInstance)
					{
						oldInstance.ScheduleCivilianCleanup(50000); // 60 seconds delay
					}
				}
			}
			// --- END ADDED ---

			// --- BEGIN ADDED: Check for defend mission before proceeding ---
			if (CheckAndStartDefendMission(currentGroup))
			{
				// Defend mission started, don't proceed to next zone yet
				Print("[IA_MissionInitializer] Defend mission started for group " + currentGroup + ". Delaying progression.", LogLevel.NORMAL);
				return;
			}
			// --- END ADDED ---

			// --- BEGIN ADDED: Trigger RTB notification only when actually proceeding to next zone ---
			// Only send RTB notification if we're not starting a defend mission
			TriggerGlobalNotification("AreaGroupCompleted", "All objectives in current area");
			// --- END ADDED ---
			
			if (m_currentAreaGroupManager)
			{
				delete m_currentAreaGroupManager;
				m_currentAreaGroupManager = null;
			}

			m_currentIndex++;
			if (m_currentAreaInstances) m_currentAreaInstances.Clear(); // Clear instances for the completed group
			GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete); // Stop checking this group
			GetGame().GetCallqueue().CallLater(ProceedToNextZone, Math.RandomInt(45,90)*1000, false); // Start next group
		}
		else {
			//Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " still needs " + (amountOfZones - actualCompletedZones) + " more zones to complete. Will re-check.", LogLevel.NORMAL);
			// Callqueue will call this method again.
		}
	}

	// --- BEGIN ADDED: Method to trigger global area completed notification ---
	void TriggerGlobalNotification(string messageType, string taskTitle)
	{
		array<int> playerIDs = new array<int>();
		GetGame().GetPlayerManager().GetAllPlayers(playerIDs);
		foreach (int playerID : playerIDs){
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerID);
			if(pc){
			 	SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
				if(character){
			 		character.SetUIOne(messageType, taskTitle, playerID);
				}
			}
		}
	}
	// --- END ADDED ---

	void InitDelayed(IEntity owner){
	

        // Only execute on the server
        if (!Replication.IsServer()) return;

        // Prevent re-initialization
        if (m_bInitialized) return;
	    m_bInitialized = true;

		// Set static instance for global access
		s_instance = this;

		// Load config file if specified
		LoadConfig();

        // Set this instance as the reference for IA_AreaMarker
        IA_AreaMarker.SetMissionInitializer(this);

        // 1) Check that IA_ReplicationWorkaround is present.
        if (!IA_ReplicationWorkaround.Instance())
        {
            ////Print("[IA_MissionInitializer] WARNING: No IA_ReplicationWorkaround entity found!", LogLevel.ERROR);
        }

        // 2) Initialize the IA_Game Singleton.
        IA_Game game = IA_Game.Instantiate();

        // 3) Set the RNG seed.
        int seed = System.GetUnixTime();
        IA_Game.rng.SetSeed(seed);
		
//		RandomizeMarkerGroups(m_numberOfGroups);

        // 4) Retrieve all IA_AreaMarker entities from the static registry.
        //array<IA_AreaMarker> markerMarkers = IA_AreaMarker.GetAllMarkers();
		
		
		
		GetGame().GetCallqueue().CallLater(InitializeNow, 200, false);
		
		
      

	
	}
	
	
	
	
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
		s_instance = this;
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }

    private void _SpawnAreaInstanceWithDelay(IA_AreaMarker marker, Faction nextAreaFaction, int currentGroup)
    {
        if (!marker)
        {
            Print("[ERROR] IA_MissionInitializer._SpawnAreaInstanceWithDelay: marker is null!", LogLevel.ERROR);
            return;
        }

        vector pos = marker.GetOrigin();
        string name = marker.GetAreaName();
        float radius = marker.GetRadius();

        Print("[DEBUG_ZONE_GROUP_DELAYED] Initializing zone: " + name + " in group " + currentGroup, LogLevel.NORMAL);

        IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
		
		/*
		// Sum up initial civilians for the whole area group
        m_initialTotalCiviliansInGroup += area.GetCivilianCount();
		*/
		
        IA_Game game = IA_Game.Instantiate();
        if (!game)
        {
            Print("[ERROR] IA_MissionInitializer._SpawnAreaInstanceWithDelay: Failed to get IA_Game instance for area " + name, LogLevel.ERROR);
            return;
        }
        
        ref IA_AreaInstance currentAreaInstance = game.AddArea(area, IA_Faction.USSR, nextAreaFaction, 0, currentGroup);
        
        if (currentAreaInstance)
        {
            // Add to the main list for tracking
            if (m_currentAreaInstances.Find(currentAreaInstance) == -1)
            {
                 m_currentAreaInstances.Insert(currentAreaInstance);
            }

            // The IA_AreaInstance.Create method already handles its own initial vehicle and AI spawning.
            // So, IA_Game.SetCurrentAreaInstance(currentAreaInstance); might not be strictly needed here if it was for that purpose.
            // If other systems rely on it being set during this phase, it needs careful consideration.
            // For now, assuming IA_AreaInstance.Create is self-contained for its immediate needs.

            string taskTitle;
            string taskDesc;
            if (area.GetAreaType() == IA_AreaType.RadioTower)
            {
                taskTitle = "Destroy " + area.GetName();
                taskDesc = "Destroy the " + area.GetName() + " to disrupt enemy communications.";
            }
            else
            {
                taskTitle = "Capture " + area.GetName();
                taskDesc = "Eliminate enemy presence and secure " + area.GetName();
            }
            currentAreaInstance.QueueTask(taskTitle, taskDesc, pos);
            Print("[DEBUG_ZONE_GROUP_DELAYED] Initialized area: " + name + " with task: " + taskTitle, LogLevel.NORMAL);
        }
        else
        {
            Print("[ERROR] IA_MissionInitializer._SpawnAreaInstanceWithDelay: Failed to create AreaInstance for " + name, LogLevel.ERROR);
        }
    }

    private void _SpawnGroupVehiclesWithDelay(Faction nextAreaFaction)
    {
        // Ensure CurrentAreaInstance is valid or IA_VehicleManager can work without it for group spawns
        // IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints uses IA_VehicleManager.GetActiveGroup()
        // which is set at the beginning of ProceedToNextZone.
        // So, IA_Game.CurrentAreaInstance might not be directly needed for this specific call.
        
        //Print("[DEBUG_ZONE_GROUP_DELAYED] Calling IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints for faction " + nextAreaFaction.GetFactionKey() + " in active group: " + IA_VehicleManager.GetActiveGroup(), LogLevel.NORMAL);
        IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints(IA_Faction.USSR, nextAreaFaction);
    }

	void LoadConfig()
	{
		if (m_configResource.IsEmpty())
		{
			Print("[IA_MissionInitializer] No config resource specified, using default behavior", LogLevel.NORMAL);
			m_bEnforceRoleRestrictionsReplicated = false;
			Replication.BumpMe();
			return;
		}

		Resource configRes = Resource.Load(m_configResource);
		if (!configRes)
		{
			Print("[IA_MissionInitializer] Failed to load config resource: " + m_configResource, LogLevel.ERROR);
			m_bEnforceRoleRestrictionsReplicated = false;
			Replication.BumpMe();
			return;
		}

		m_config = IA_Config.Cast(BaseContainerTools.CreateInstanceFromContainer(configRes.GetResource().ToBaseContainer()));
		if (!m_config)
		{
			Print("[IA_MissionInitializer] Failed to create IA_Config instance from resource", LogLevel.ERROR);
			m_bEnforceRoleRestrictionsReplicated = false;
			Replication.BumpMe();
			return;
		}

		Print("[IA_MissionInitializer] Successfully loaded config: " + m_configResource, LogLevel.NORMAL);
		m_bEnforceRoleRestrictionsReplicated = m_config.m_bEnforceRoleRestrictions;
		Replication.BumpMe();
	}

	IA_Config GetConfig()
	{
		return m_config;
	}

	// Static getter for global access to config
	static IA_MissionInitializer GetInstance()
	{
		return s_instance;
	}

	// Static getter for config
	static IA_Config GetGlobalConfig()
	{
		if (s_instance)
			return s_instance.GetConfig();
		return null;
	}
	
	static bool AreRoleRestrictionsEnforced()
	{
		if (s_instance)
			return s_instance.m_bEnforceRoleRestrictionsReplicated;
		
		// Default to false (not enforced) if instance not available
		return false;
	}
	
	// --- BEGIN ADDED: Method to set artillery cooldown ---
	static void SetArtilleryDisabled(int durationSeconds)
    {
        int currentTime = System.GetUnixTime();
        s_artilleryDisabledUntil = currentTime + durationSeconds;
        Print(string.Format("[IA_MissionInitializer] Artillery disabled by side objective completion for %1 seconds.", durationSeconds), LogLevel.NORMAL);
    }
	// --- END ADDED ---
	
	// --- BEGIN ADDED: Defend Mission Methods ---
	private bool CheckAndStartDefendMission(int completedGroup)
	{
		// Check if a defend mission is already active
		IA_Game gameInstance = IA_Game.Instantiate();
		if (gameInstance && gameInstance.HasActiveDefendMission())
		{
			Print("[IA_MissionInitializer] Defend mission already active, skipping new defend mission", LogLevel.DEBUG);
			return false;
		}
		
		// Check for defend objective markers in the completed group
		array<IA_AreaMarker> allMarkers = IA_AreaMarker.GetAllMarkers();
		array<IA_AreaMarker> defendMarkers = {};
		
		if (!allMarkers)
			return false;
			
		foreach (IA_AreaMarker marker : allMarkers)
		{
			if (marker && marker.m_areaGroup == completedGroup && marker.GetAreaType() == IA_AreaType.DefendObjective)
			{
				defendMarkers.Insert(marker);
			}
		}
		
		// If no defend objectives found, return false
		if (defendMarkers.IsEmpty())
		{
			Print("[IA_MissionInitializer] No defend objectives found for group " + completedGroup, LogLevel.DEBUG);
			return false;
		}
		
		// Random chance (80%) to trigger defend mission
		if (Math.RandomFloat01() > 1)
		{
			Print("[IA_MissionInitializer] Defend objectives found but random chance failed for group " + completedGroup, LogLevel.DEBUG);
			return false;
		}
		
		// Select random defend objective
		int randomIndex = Math.RandomInt(0, defendMarkers.Count());
		IA_AreaMarker selectedMarker = defendMarkers[randomIndex];
		vector defendPoint = selectedMarker.GetOrigin();
		
		// Debug: Check if defend point is valid
		if (defendPoint == vector.Zero)
		{
			Print(string.Format("[IA_MissionInitializer] WARNING: Defend objective marker '%1' has zero position! This will cause issues.", 
				selectedMarker.GetAreaName()), LogLevel.ERROR);
		}
		
		Print(string.Format("[IA_MissionInitializer] Starting defend mission at %1 (%2) for group %3", 
			selectedMarker.GetAreaName(), defendPoint.ToString(), completedGroup), LogLevel.NORMAL);
		
		// Create and start defend mission
		IA_DefendMission defendMission = IA_DefendMission.Create(defendPoint, completedGroup, selectedMarker.GetAreaName());
		if (defendMission)
		{
			defendMission.StartDefendMission();
			
			// Set the defend mission in IA_Game (reuse the gameInstance from above)
			if (gameInstance)
			{
				gameInstance.SetActiveDefendMission(defendMission);
			}
			
			// Trigger notification
			TriggerGlobalNotification("DefendMissionStarted", "Defend " + selectedMarker.GetAreaName());
			
			return true;
		}
		
		return false;
	}
	
	void OnDefendMissionComplete()
	{
		Print("[IA_MissionInitializer] Defend mission completed, proceeding to next zone", LogLevel.NORMAL);
		
		// Trigger RTB notification just like normal area group completion
		TriggerGlobalNotification("AreaGroupCompleted", "All objectives in current area");
		
		// Clean up current area instances
		m_civilianRevoltActive = false;
		m_currentIndex++;
		if (m_currentAreaInstances) 
			m_currentAreaInstances.Clear();
		
		if (m_currentAreaGroupManager)
		{
			delete m_currentAreaGroupManager;
			m_currentAreaGroupManager = null;
		}
		
		// Remove the zone completion check and proceed to next zone
		GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete);
		GetGame().GetCallqueue().CallLater(ProceedToNextZone, Math.RandomInt(45,90)*1000, false);
	}
	// --- END ADDED ---
}
