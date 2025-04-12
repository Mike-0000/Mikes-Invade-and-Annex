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
	void ProceedToNextZone()
	{
	    if (m_currentIndex >= groupsArray.Count())
	    {
	        Print("GAME FINISHED!!!", LogLevel.WARNING);
	        return;
		}
		
		m_currentAreaInstances.Clear();
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		
		foreach(IA_AreaMarker marker : markers){
	//		IA_AreaMarker marker = m_shuffledMarkers[m_currentIndex];
		    if (!marker)
		    {
		        //m_currentIndex++;
		        //ProceedToNextZone();
		        continue;
		    }
			if(marker.m_areaGroup != groupsArray[m_currentIndex])
				continue;
		    vector pos = marker.GetOrigin();
		    string name = marker.GetAreaName();
		    float radius = marker.GetRadius();
			
		    IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
			IA_AreaInstance m_currentAreaInstance = IA_Game.Instantiate().AddArea(area, IA_Faction.USSR, 0);
		    m_currentAreaInstances.Insert(m_currentAreaInstance);
		
		    string taskTitle = "Capture " + area.GetName();
		    string taskDesc = "Eliminate enemy presence and secure " + area.GetName();
		    m_currentAreaInstance.QueueTask(taskTitle, taskDesc, pos);
		
		    //Print("[INFO] Initialized area: " + name, LogLevel.NORMAL);
		
		    // Monitor completion
		}
		

	    GetGame().GetCallqueue().CallLater(CheckCurrentZoneComplete, 5000, true);
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
    Print("[DEBUG] IA_MissionInitializer: InitializeNow started.", LogLevel.NORMAL);
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
        Print("[WARNING] No IA_AreaMarkers found!", LogLevel.WARNING);
        return;
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
		//Print("Running CheckCurrentZoneComplete",LogLevel.NORMAL);
		if (!m_currentAreaInstances)
	        return;
		//Print("Running CheckCurrentZoneComplete 1",LogLevel.NORMAL);
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		array<bool> zoneCompletionStatus = {}; // Track completion for each zone in current group
		
		// First, find all markers for current group and their initial completion status
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != groupsArray[m_currentIndex])
				continue;
				
			// Add this zone to our tracking array
			zoneCompletionStatus.Insert(false);
		}
		//Print("Running CheckCurrentZoneComplete 2",LogLevel.NORMAL);
		// If no markers found for this group, something is wrong
		if(zoneCompletionStatus.Count() == 0) {
			Print("[ERROR] No markers found for group " + groupsArray[m_currentIndex], LogLevel.ERROR);
			return;
		}
		//Print("Running CheckCurrentZoneComplete 3",LogLevel.NORMAL);
		// Now check each marker in the current group
		int currentZoneIndex = 0;
		int amountOfZones = 0;
		int currentScore = 0;
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != groupsArray[m_currentIndex])
				continue;
			amountOfZones++
		}
		foreach(IA_AreaMarker marker : markers) {
			if(!marker || marker.m_areaGroup != groupsArray[m_currentIndex])
				continue;
				//Print("Running CheckCurrentZoneComplete 3.5",LogLevel.NORMAL);
			// Find corresponding area instance
			if(currentZoneIndex >= m_currentAreaInstances.Count()) {
				Print("[ERROR] More markers than area instances for group " + groupsArray[m_currentIndex], LogLevel.ERROR);
				return;
			}
			//Print("Running CheckCurrentZoneComplete 3.75",LogLevel.NORMAL);
			// Skip if no task entity
			if(!m_currentAreaInstances[currentZoneIndex].GetCurrentTaskEntity()){
				currentScore++;
				currentZoneIndex++;
				continue;
			}
				
			int factionScore = marker.USFactionScore;
			Print("Zone " + currentZoneIndex + " score is " + factionScore, LogLevel.NORMAL);
			//Print("Running CheckCurrentZoneComplete 3.8",LogLevel.NORMAL);
			// Mark this zone as complete if score threshold met
			if(factionScore >= 5) {
				zoneCompletionStatus[currentZoneIndex] = true;
				m_currentAreaInstances[currentZoneIndex].GetCurrentTaskEntity().Finish();
				currentScore++;
			}
			
			currentZoneIndex++;
		}
		
/*
		Print("Running CheckCurrentZoneComplete 4",LogLevel.NORMAL);
		// Check if ALL zones in the group are complete
		bool allComplete = true;
		for(int i = 0; i < zoneCompletionStatus.Count(); i++) {
			Print("Running CheckCurrentZoneComplete 4.5",LogLevel.NORMAL);
			if(!zoneCompletionStatus[i]) {
				allComplete = false;
				break;
			}
		}
		Print("Running CheckCurrentZoneComplete 5 " + allComplete, LogLevel.NORMAL);
		// If all zones complete, move to next group
		*/
		Print("Current Score " + currentScore + " amountOfZones " + amountOfZones);
		if(currentScore >= amountOfZones){
			Print("[INFO] All zones in group " + groupsArray[m_currentIndex] + " complete. Proceeding to next.", LogLevel.NORMAL);
			m_currentIndex++;
			m_currentAreaInstances.Clear();
			GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete);
			ProceedToNextZone();
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
            Print("[IA_MissionInitializer] WARNING: No IA_ReplicationWorkaround entity found!", LogLevel.ERROR);
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
        Print("[INFO] Randomized " + shuffledMarkers.Count() + " markers into " + groupCount + " groups.", LogLevel.NORMAL);
    }
	
	
	*/
	
	
	
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }
}
