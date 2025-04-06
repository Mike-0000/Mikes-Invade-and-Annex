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
		//Print("Running CheckCurrentZoneComplete", LogLevel.NORMAL);
	    if (!m_currentAreaInstances)
	        return;
	
//	    if (!m_currentAreaInstance.GetCurrentTaskEntity())
//	        return;
		//Print("CheckCurrentZoneComplete DEBUG 1", LogLevel.NORMAL);
//		IA_AreaMarker marker = m_shuffledMarkers[m_currentIndex];
		
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		bool switcher = true;
		int index1 = 0;
		foreach(IA_AreaMarker marker : markers){
			
			if(!marker || marker.m_areaGroup != groupsArray[m_currentIndex]){
				continue;
			}
			int factionScore = marker.USFactionScore;
			Print("The current score from CheckZoneComplete is "+factionScore, LogLevel.NORMAL);
			if(factionScore < 5){
				switcher = false;
				index1++;
				continue;
			}
			if(m_currentAreaInstances[index1])
				m_currentAreaInstances[index1].GetCurrentTaskEntity().Finish();
			index1++;
		}
		if(switcher == true){
				
				Print("[INFO] Zone complete. Proceeding to next.", LogLevel.NORMAL);
	       	 	m_currentIndex++;
	       		m_currentAreaInstances.Clear();
	        	GetGame().GetCallqueue().Remove(CheckCurrentZoneComplete); // Clean up the loop
	       		ProceedToNextZone();
				
			}
	
	}

	void InitDelayed(IEntity owner){
	

        // Only execute on the server
        if (!Replication.IsServer()) return;

        // Prevent re-initialization
        if (m_bInitialized) return;
	    m_bInitialized = true;


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
