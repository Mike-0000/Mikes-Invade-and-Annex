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
	        //Print("[ERROR] IA_MissionInitializer.GetActiveGroup: groupsArray is null or empty!", LogLevel.ERROR);
	        return -1;
	    }
	        
	    if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
	    {
	        //Print("[ERROR] IA_MissionInitializer.GetActiveGroup: Invalid m_currentIndex " + m_currentIndex + ", array size: " + groupsArray.Count(), LogLevel.ERROR);
	        return -1;
	    }
	    
	    return groupsArray[m_currentIndex];
	}
	
	void ProceedToNextZone()
	{
	    if (!groupsArray || groupsArray.IsEmpty())
	    {
	        //Print("[ERROR] IA_MissionInitializer.ProceedToNextZone: groupsArray is null or empty!", LogLevel.ERROR);
	        return;
	    }
	    
	    if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
	    {
	        //Print("[DEBUG_ZONE_GROUP] GAME FINISHED!!! All zone groups completed or invalid index.", LogLevel.WARNING);
	        return;
	    }
	    
	    int currentGroup = groupsArray[m_currentIndex];
	    //Print("[DEBUG_ZONE_GROUP] Proceeding to zone group " + currentGroup + " (index " + m_currentIndex + " of " + groupsArray.Count() + ")", LogLevel.WARNING);
	    
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
	        
	        //Print("[DEBUG_ZONE_GROUP] Initializing zone: " + name + " in group " + currentGroup, LogLevel.NORMAL);
	        
	        IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
	        IA_AreaInstance m_currentAreaInstance = IA_Game.Instantiate().AddArea(area, IA_Faction.USSR, 0);
	        m_currentAreaInstances.Insert(m_currentAreaInstance);
	        
	        // Set the current area instance in IA_Game before spawning vehicles
	        IA_Game.SetCurrentAreaInstance(m_currentAreaInstance);
	    
	        string taskTitle = "Capture " + area.GetName();
	        string taskDesc = "Eliminate enemy presence and secure " + area.GetName();
	        m_currentAreaInstance.QueueTask(taskTitle, taskDesc, pos);
	    
	        //Print("[DEBUG_ZONE_GROUP] Initialized area: " + name + " with task: " + taskTitle, LogLevel.NORMAL);
	    }
	    
	    //Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " contains " + zonesInGroup + " zones to capture", LogLevel.WARNING);
	    
	    // Ensure CurrentAreaInstance is valid before spawning vehicles (it should be the last one from the loop)
	    if (IA_Game.CurrentAreaInstance)
	    {
	        IA_VehicleManager.SpawnVehiclesAtAllSpawnPoints(IA_Faction.USSR);
	        //Print("[DEBUG_ZONE_GROUP] Spawned vehicles for zone group " + currentGroup, LogLevel.NORMAL);
	    }
	    else
	    {
	        //Print("[WARNING] No current area instance set after processing markers for group " + currentGroup + ". Cannot spawn vehicles.", LogLevel.WARNING);
	    }

	    GetGame().GetCallqueue().CallLater(CheckCurrentZoneComplete, 5000, true);
	    //Print("[DEBUG_ZONE_GROUP] Started monitoring completion for group " + currentGroup, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_MissionInitializer: InitializeNow started.", LogLevel.NORMAL);
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
        //Print("[WARNING] No IA_AreaMarkers found!", LogLevel.WARNING);
        return;
    }
    
    // Check for duplicate marker names
    //Print("[DEBUG_MARKER_NAMES] Checking for duplicate marker names...", LogLevel.WARNING);
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
            //Print("[DEBUG_MARKER_NAMES] WARNING: Found duplicate marker name: " + name + " (appears " + count + " times)", LogLevel.ERROR);
        } else {
            //Print("[DEBUG_MARKER_NAMES] Marker name: " + name + " (unique)", LogLevel.NORMAL);
        }
    }
    
    // Initialize the vehicle manager
    Resource vehicleManagerRes = Resource.Load("{07F25000A2274994}Components/VehicleManager.et");
    if (vehicleManagerRes)
    {
        EntitySpawnParams params = EntitySpawnParams();
        GetGame().SpawnEntityPrefab(vehicleManagerRes, null, params);
        //Print("[DEBUG] IA_VehicleManager spawned.", LogLevel.NORMAL);
    }
    else
    {
        //Print("[WARNING] Failed to load IA_VehicleManager resource!", LogLevel.WARNING);
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
			Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: groupsArray is null or empty!", LogLevel.ERROR);
			return;
		}
		
		if (m_currentIndex < 0 || m_currentIndex >= groupsArray.Count())
		{
			Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: Invalid m_currentIndex " + m_currentIndex + ", array size: " + groupsArray.Count(), LogLevel.ERROR);
			return;
		}
		
		//Print("Running CheckCurrentZoneComplete",LogLevel.NORMAL);
		if (!m_currentAreaInstances)
		{
			Print("[ERROR] IA_MissionInitializer.CheckCurrentZoneComplete: m_currentAreaInstances is null!", LogLevel.ERROR);
			return;
		}
		
		//Print("Running CheckCurrentZoneComplete 1",LogLevel.NORMAL);
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		array<bool> zoneCompletionStatus = {}; // Track completion for each zone in current group
		
		int currentGroup = groupsArray[m_currentIndex];
		
		// Debug info for area instances
		Print("[DEBUG_ZONE_MATCH] Current area instances:", LogLevel.WARNING);
		for (int i = 0; i < m_currentAreaInstances.Count(); i++) {
			IA_AreaInstance instance = m_currentAreaInstances[i];
			if (instance && instance.m_area) {
				Print("[DEBUG_ZONE_MATCH] Instance " + i + ": Name=" + instance.m_area.GetName(), LogLevel.WARNING);
			} else {
				Print("[DEBUG_ZONE_MATCH] Instance " + i + ": Invalid or has no area", LogLevel.WARNING);
			}
		}
		
		// Debug info for markers in current group
		Print("[DEBUG_ZONE_MATCH] Markers in current group " + currentGroup + ":", LogLevel.WARNING);
		int markerCount = 0;
		foreach(IA_AreaMarker marker : markers) {
			if (!marker || marker.m_areaGroup != currentGroup)
				continue;
			
			Print("[DEBUG_ZONE_MATCH] Group marker " + markerCount + ": Name=" + marker.GetAreaName() + ", USFactionScore=" + marker.USFactionScore, LogLevel.WARNING);
			markerCount++;
		}
		
		// First, find all markers for current group and their initial completion status
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != currentGroup)
				continue;
				
			// Add this zone to our tracking array
			zoneCompletionStatus.Insert(false);
		}
		
		//Print("Running CheckCurrentZoneComplete 2",LogLevel.NORMAL);
		// If no markers found for this group, something is wrong
		if(zoneCompletionStatus.Count() == 0) {
			Print("[ERROR] No markers found for group " + currentGroup, LogLevel.ERROR);
			return;
		}
		
		//Print("Running CheckCurrentZoneComplete 3",LogLevel.NORMAL);
		// Now check each marker in the current group
		int currentZoneIndex = 0;
		int amountOfZones = 0;
		int currentScore = 0;
		
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != currentGroup)
				continue;
			amountOfZones++;
		}
		
		Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " has " + amountOfZones + " zones to complete", LogLevel.NORMAL);
		
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != currentGroup)
				continue;
			
			//Print("Running CheckCurrentZoneComplete 3.5",LogLevel.NORMAL);
			// Find corresponding area instance
			if(currentZoneIndex >= m_currentAreaInstances.Count()) {
				Print("[ERROR] More markers than area instances for group " + currentGroup, LogLevel.ERROR);
				return;
			}
			
			//Print("Running CheckCurrentZoneComplete 3.75",LogLevel.NORMAL);
			// Skip if no task entity
			IA_AreaInstance instance = m_currentAreaInstances[currentZoneIndex];
			if(!instance || !instance.GetCurrentTaskEntity()){
				Print("[DEBUG_ZONE_GROUP] Zone " + currentZoneIndex + " has no task entity, marking as complete", LogLevel.NORMAL);
				currentScore++;
				currentZoneIndex++;
				continue;
			}
				
			// Replace USFactionScore with dictionary lookup
			float factionScore = marker.GetFactionScore("US");
			Print("[DEBUG_ZONE_SCORE_DEBUG] Marker name: " + marker.GetAreaName() + ", US faction score from dictionary: " + factionScore, LogLevel.WARNING);
			
			Print("[DEBUG_ZONE_GROUP] Zone " + currentZoneIndex + " score is " + factionScore + "/5.0", LogLevel.NORMAL);
			//Print("Running CheckCurrentZoneComplete 3.8",LogLevel.NORMAL);
			// Mark this zone as complete if score threshold met
			if(factionScore >= 5) {
				Print("[DEBUG_ZONE_GROUP] Zone " + currentZoneIndex + " has reached completion threshold!", LogLevel.WARNING);
				if (currentZoneIndex < zoneCompletionStatus.Count())
					zoneCompletionStatus[currentZoneIndex] = true;
					
				if (instance && instance.GetCurrentTaskEntity())
					instance.GetCurrentTaskEntity().Finish();
					
				currentScore++;
			}
			else {
				Print("[DEBUG_ZONE_GROUP] Zone " + currentZoneIndex + " still needs " + (5 - factionScore) + " more points to complete", LogLevel.NORMAL);
			}
			
			currentZoneIndex++;
		}
		
		Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " progress: " + currentScore + "/" + amountOfZones + " zones completed", LogLevel.WARNING);
		
		if(currentScore >= amountOfZones){
			Print("[INFO] All zones in group " + currentGroup + " complete. Proceeding to next.", LogLevel.WARNING);
			m_currentIndex++;
			m_currentAreaInstances.Clear();
			GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete);
			ProceedToNextZone();
		}
		else {
			Print("[DEBUG_ZONE_GROUP] Group " + currentGroup + " still needs " + (amountOfZones - currentScore) + " more zones to complete", LogLevel.NORMAL);
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
            //Print("[IA_MissionInitializer] WARNING: No IA_ReplicationWorkaround entity found!", LogLevel.ERROR);
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
		
		
      

        //Print("[IA_MissionInitializer] Dynamic areas/tasks set up. Initialization complete.", LogLevel.NORMAL);
	
	}
	
	
	
	/*
void RandomizeMarkerGroups(int groupCount)
    {
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
        if (markers.IsEmpty()) return;

        // 1) Shuffle the markers with the same logic you currently use
        array<IA_AreaMarker> shuffledMarkers = {};
        while (!markers.IsEmpty())
        {
            int i = IA_Game.rng.RandInt(0, markers.Count());
            shuffledMarkers.Insert(markers[i]);
            markers.Remove(i);
        }

        // 2) Evenly distribute them among the desired groups
        //    (so if you have 12 markers and 3 groups, each group gets 4).
        int count       = shuffledMarkers.Count();
        int chunkSize   = Math.Ceil(count / groupCount);
        int currentGroup = 1;
        int assignedCountInGroup = 0;

        // Loop the shuffled markers in order and assign them to a group
        for (int x = 0; x < shuffledMarkers.Count(); x++)
        {
            IA_AreaMarker marker = shuffledMarkers[x];
            marker.m_areaGroup = currentGroup;
            assignedCountInGroup++;

            // If we've assigned enough in this group, move to the next
            if (assignedCountInGroup >= chunkSize)
            {
                assignedCountInGroup = 0;
                currentGroup++;

                // Edge case: if we exceed groupCount, clamp to last group.
                if (currentGroup > groupCount)
                    currentGroup = groupCount;
            }
        }

        // At this point, each marker has m_areaGroup 1..N
        //Print("[INFO] Randomized " + shuffledMarkers.Count() + " markers into " + groupCount + " groups.", LogLevel.NORMAL);
    }
	
	
	*/
	
	
	
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }
}
