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

	ref array<int> groupsArray;
    // Simple flag to ensure we only run once.
    protected bool m_bInitialized = false;
	//ref array<IA_AreaMarker> m_shuffledMarkers = {};
	int m_currentIndex = 0;
	ref array<IA_AreaInstance> m_currentAreaInstances = null;

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
	void ProceedToNextZone()
	{
	    if (!groupsArray || groupsArray.IsEmpty())
	    {
	        ////Print("[ERROR] IA_MissionInitializer.ProceedToNextZone: groupsArray is null or empty!", LogLevel.ERROR);
	        return;
	    }
	    
	    if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
	    {
			Print("GAME FINISHED!!! Waiting 30 seconds and ending gamemode.", LogLevel.WARNING);
	        GetGame().GetCallqueue().CallLater(FinishGame, 30000);
	        return;
	    }
	    
	    int currentGroup = groupsArray[m_currentIndex];
	    ////Print("[DEBUG_ZONE_GROUP] Proceeding to zone group " + currentGroup + " (index " + m_currentIndex + " of " + groupsArray.Count() + ")", LogLevel.WARNING);
	    
	    // Update the active group in the vehicle manager
	    IA_VehicleManager.SetActiveGroup(currentGroup);
	    
	    m_currentAreaInstances.Clear();
	    array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
	    
	    int zonesInGroup = 0;
	    foreach(IA_AreaMarker marker : markers){
	        if (!marker)
	        {
	            continue;
	        }
	        
	        if(marker.m_areaGroup != currentGroup)
	            continue;
	            
	        zonesInGroup++;
	        vector pos = marker.GetOrigin();
	        string name = marker.GetAreaName();
	        float radius = marker.GetRadius();
	        
	        ////Print("[DEBUG_ZONE_GROUP] Initializing zone: " + name + " in group " + currentGroup, LogLevel.NORMAL);
	        
	        IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
	        IA_AreaInstance m_currentAreaInstance = IA_Game.Instantiate().AddArea(area, IA_Faction.USSR, 0);
	        m_currentAreaInstances.Insert(m_currentAreaInstance);
	        
	        // Set the current area instance in IA_Game before spawning vehicles
	        IA_Game.SetCurrentAreaInstance(m_currentAreaInstance);
	    
	        string taskTitle = "Capture " + area.GetName();
	        string taskDesc = "Eliminate enemy presence and secure " + area.GetName();
	        m_currentAreaInstance.QueueTask(taskTitle, taskDesc, pos);
	    
	        ////Print("[DEBUG_ZONE_GROUP] Initialized area: " + name + " with task: " + taskTitle, LogLevel.NORMAL);
	    }
	    
	    ////Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " contains " + zonesInGroup + " zones to capture", LogLevel.WARNING);
	    
	    // Ensure CurrentAreaInstance is valid before spawning vehicles (it should be the last one from the loop)
	    if (IA_Game.CurrentAreaInstance)
	    {
	        IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints(IA_Faction.USSR);
	        ////Print("[DEBUG_ZONE_GROUP] Spawned vehicles for zone group " + currentGroup, LogLevel.NORMAL);
	    }
	    else
	    {
	        ////Print("[WARNING] No current area instance set after processing markers for group " + currentGroup + ". Cannot spawn vehicles.", LogLevel.WARNING);
	    }

	    GetGame().GetCallqueue().CallLater(CheckCurrentZoneComplete, 5000, true);
	    ////Print("[DEBUG_ZONE_GROUP] Started monitoring completion for group " + currentGroup, LogLevel.NORMAL);
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
    ////Print("[DEBUG] IA_MissionInitializer: InitializeNow started.", LogLevel.NORMAL);
	groupsArray = new array<int>;
	m_currentAreaInstances = new array<IA_AreaInstance>;
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
		
		////Print("Running CheckCurrentZoneComplete",LogLevel.NORMAL);
		if (!m_currentAreaInstances)
		{
			//Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: m_currentAreaInstances is null!", LogLevel.ERROR);
			return;
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
			if(!marker_counter || marker_counter.m_areaGroup != currentGroup)
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
			if(!marker || marker.m_areaGroup != currentGroup)
				continue;
			
			////Print("Running CheckCurrentZoneComplete 3.5 for marker: " + marker.GetAreaName(),LogLevel.NORMAL);
			// Ensure currentZoneIndex is within bounds for both m_currentAreaInstances and zoneCompletionStatus
			if(currentZoneIndex >= m_currentAreaInstances.Count() || currentZoneIndex >= zoneCompletionStatus.Count()) {
				//Print("[ERROR] Index out of bounds. Marker/Instance/StatusArray desync for group " + currentGroup + " at index " + currentZoneIndex, LogLevel.ERROR);
				return; // Stop processing to prevent crash
			}
			
			////Print("Running CheckCurrentZoneComplete 3.75",LogLevel.NORMAL);
			IA_AreaInstance instance = m_currentAreaInstances[currentZoneIndex];
			if(!instance) {
			    //Print("[WARNING] Null IA_AreaInstance at index " + currentZoneIndex + " for group " + currentGroup, LogLevel.WARNING);
			    currentZoneIndex++; // Increment to process next marker correctly
			    continue;
			}
				
			// Get US faction score (now on 0-100 scale)
			float factionScore = marker.GetFactionScore("US");
			//Print("[DEBUG_ZONE_SCORE_DEBUG] Marker name: " + marker.GetAreaName() + ", US faction score: " + factionScore + "/100", LogLevel.WARNING);
			
			if(factionScore >= 100) {
				//Print("[DEBUG_ZONE_GROUP] Zone " + marker.GetAreaName() + " (idx " + currentZoneIndex + ") in group " + currentGroup + " IS complete (Score: " + factionScore + ").", LogLevel.WARNING);
				actualCompletedZones++;
				zoneCompletionStatus[currentZoneIndex] = true; // Update status array
					
				// If there's an active task for this completed zone, finish it.
				if (instance.GetCurrentTaskEntity()) {
				    //Print("[DEBUG_ZONE_GROUP] Finishing task for completed zone: " + marker.GetAreaName(), LogLevel.NORMAL);
					instance.GetCurrentTaskEntity().Finish();
				}
			}
			else { // factionScore < 100
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
				    instance.QueueTask(taskTitle, taskDesc, pos);
				}
			}
			
			currentZoneIndex++;
		}
		
		//Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " progress: " + actualCompletedZones + "/" + amountOfZones + " zones completed.", LogLevel.WARNING);
		
		if(actualCompletedZones >= amountOfZones){ // Use the accurate count
			//Print("[INFO] All " + amountOfZones + " zones in group " + currentGroup + " complete. Proceeding to next.", LogLevel.WARNING);
			m_currentIndex++;
			if (m_currentAreaInstances) m_currentAreaInstances.Clear(); // Clear instances for the completed group
			GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete); // Stop checking this group
			ProceedToNextZone(); // Start next group
		}
		else {
			//Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " still needs " + (amountOfZones - actualCompletedZones) + " more zones to complete. Will re-check.", LogLevel.NORMAL);
			// Callqueue will call this method again.
		}
	}

	void InitDelayed(IEntity owner){
	

        // Only execute on the server
        if (!Replication.IsServer()) return;

        // Prevent re-initialization
        if (m_bInitialized) return;
	    m_bInitialized = true;

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
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }


}
