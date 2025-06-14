class IA_AreaMarkerClass : ScriptedGameTriggerEntityClass
{
};

class IA_AreaMarker : ScriptedGameTriggerEntity
{
	bool InitCalled = false;
	float USFactionScore;
	// -- Instead of 'ref map<string,int>', we store: faction->count
    protected ref IA_DictStringInt   m_FactionCounts;

    // -- Instead of 'ref map<SCR_ChimeraCharacter,string>', store: charHash->faction
    protected ref IA_DictIntString   m_CharacterFactionMapping;
		
    // -- Instead of 'ref map<string,float>', store: faction->score
    ref IA_DictStringFloat m_FactionScores;

    // -- Throttle sphere queries
    protected int m_iFrameCount = 0;
	protected float score_limiter_ratio = 0.02;
	protected float m_fTimeAccumulator = 0.0;
	
    // -- Sphere radius
	[Attribute(defvalue: "10.0", UIWidgets.Auto, "Area Group Key", category: "Number")]
    int m_areaGroup;
    [Attribute(defvalue: "10.0", UIWidgets.EditBox, "Radius of sphere query", category: "Zone", params: "0.1 99999")]
    protected float m_fZoneRadius;
    
    // -- Radio Tower specific attributes
    [Attribute("", UIWidgets.ResourceNamePicker, "Prefab to spawn for Radio Tower area type", category: "Radio Tower", params: "et")]
    protected ResourceName m_prefabToSpawn;
    
    protected IEntity m_spawnedEntity;
    protected bool m_isDestroyed = false;
    protected bool m_prefabSpawned = false; // Flag to track if prefab has been spawned

    // -- For authority/proxy check
    protected RplComponent m_RplComponent;

    protected bool m_IsActive = false;
    protected int m_LastScoreUpdate = 0;
    protected const int SCORE_UPDATE_INTERVAL = 1; // Update score every second
    protected int m_LastFrameCount = 0; // Track the last frame we updated on
    
    // Static reference to the mission initializer
    static IA_MissionInitializer s_missionInitializer = null;
    
    // Static method to set the mission initializer reference
    static void SetMissionInitializer(IA_MissionInitializer initializer)
    {
        s_missionInitializer = initializer;
    }

    //----------------------------------------------------------------------------------------------
    vector GetZoneCenter()
    {
        vector mat[4];
        GetTransform(mat);
        return mat[3];
    }

    //----------------------------------------------------------------------------------------------
    protected string GetFactionOfCharacter(SCR_ChimeraCharacter character)
    {
        if (!character) return "";
        Faction fac = character.GetFaction();
        if (fac) return fac.GetFactionKey();
        return "";
    }

    //----------------------------------------------------------------------------------------------
    // Convert the character's RplId into a hash for dictionary storage
    protected int GetCharacterHash(SCR_ChimeraCharacter character)
    {
        if (!character) return 0;


        RplId id = Replication.FindId(character);
        return id; 
    }

    //----------------------------------------------------------------------------------------------
    // Public or local queries:
    int GetFactionCount(string factionKey)
    {
        return m_FactionCounts.Get(factionKey);
    }

    float GetFactionScore(string factionKey)
    {
        return m_FactionScores.Get(factionKey);
    }

    //----------------------------------------------------------------------------------------------
    bool IsProxy()
    {
        // This ensures we always get a fresh pointer
        if (!m_RplComponent)
            m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));
        return (m_RplComponent && m_RplComponent.IsProxy());
    }
	
    // Static array to hold all pre-placed markers
    static ref array<IA_AreaMarker> s_areaMarkers = new array<IA_AreaMarker>();

    // New static function to retrieve all markers.
    static array<IA_AreaMarker> GetAllMarkers()
    {
        // Create a clean copy of the markers array, removing any null entries
        array<IA_AreaMarker> validMarkers = {};
        
        if (!s_areaMarkers)
        {
            // Print(("[IA_AreaMarker.GetAllMarkers] s_areaMarkers is NULL, initializing empty array", LogLevel.WARNING);
            s_areaMarkers = new array<IA_AreaMarker>();
            return validMarkers;
        }
        
        // Clean up the static array while building the return array
        for (int i = 0; i < s_areaMarkers.Count(); i++)
        {
            IA_AreaMarker marker = s_areaMarkers[i];
            if (!marker)
            {
                // Print(("[IA_AreaMarker.GetAllMarkers] Found NULL marker at index " + i + ", removing it", LogLevel.WARNING);
                s_areaMarkers.Remove(i);
                i--;
                continue;
            }
            
            validMarkers.Insert(marker);
        }
        
        // Print(("[DEBUG] GetAllMarkers returning " + validMarkers.Count() + " valid markers of " + s_areaMarkers.Count() + " total", LogLevel.NORMAL);
        return validMarkers;
    }

    // Attributes to set in the editor
    [Attribute(defvalue: "CaptureZone", desc: "Name of the capture area.")]
    protected string m_areaName;
    
    [Attribute(defvalue: "City", desc: "Type of area (Town, City, Property, Airport, Docks, Military).")]
    protected string m_areaType;
    
    [Attribute(defvalue: "500", desc: "Radius of the area.")]
    protected float m_radius;
    
    // We store the marker's origin once initialized.
    protected vector m_origin;

	
	 
    //----------------------------------------------------------------------------------------------
    // Constructor
    void IA_AreaMarker(IEntitySource src, IEntity parent)
    {
        // Instantiate the dictionaries
        m_FactionCounts            = new IA_DictStringInt();
        m_CharacterFactionMapping  = new IA_DictIntString();
        m_FactionScores            = new IA_DictStringFloat();

        // We want INIT + FRAME events
        SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
    }
	
	

	
	// EOnFrame: actively scan the area around the zone
   override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner);
	    if (!owner)
	    {
	        //// Print(("[ERROR] owner is NULL", LogLevel.ERROR);
	        return;
	    }
	
	    if (IsProxy())
	    {
	        //// Print(("[DEBUG] Entity is proxy, skipping frame update", LogLevel.NORMAL);
	        return;
	    }
	
	    // Get current frame count
	    //int currentFrame = GetGame().GetFrameCount();
	    
	    // Only update if enough time has passed since last update AND we're on a new frame
	    int currentTime = System.GetUnixTime();
	    if (currentTime - m_LastScoreUpdate < SCORE_UPDATE_INTERVAL)
	        return;
	        
	    m_LastScoreUpdate = currentTime;
	    
	    // Check if this marker belongs to the active group
	    int activeGroup = -1;
	    if (s_missionInitializer)
	    {
	        // Get the current active group from the mission initializer using the accessor method
	        activeGroup = s_missionInitializer.GetActiveGroup();
	    }
	    
	    // Skip score processing if this zone isn't in the active group
	    if (activeGroup != m_areaGroup)
	    {
	        //// Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " (group " + m_areaGroup + ") - Not in active group (" + activeGroup + "), skipping score update", LogLevel.NORMAL);
	        return;
	    }
	    
	    // Spawn Radio Tower prefab if it's the active group, not yet spawned, and not already destroyed
	    if (GetAreaType() == IA_AreaType.RadioTower && !m_prefabSpawned && !m_isDestroyed)
	    {
	        SpawnPrefabEntity();
	        if (m_spawnedEntity) // Check if spawning was successful
	        {
	            m_prefabSpawned = true;
	            // Print("[INFO] Radio Tower prefab " + m_prefabToSpawn + " spawned for active group " + m_areaGroup + " at " + m_origin, LogLevel.NORMAL);
	        }
	        else
	        {
	            // Print("[WARNING] Failed to spawn Radio Tower prefab " + m_prefabToSpawn + " for active group " + m_areaGroup + " in EOnFrame. Will retry next frame.", LogLevel.WARNING);
	        }
	    }
	    
	    // Special handling for Radio Tower
	    if (GetAreaType() == IA_AreaType.RadioTower)
	    {
	        // For Radio Tower, we only care if the prefab is destroyed
	        if (m_isDestroyed)
	        {
                // Keep score at max if already destroyed
                if (!m_FactionScores.Contains("US") || m_FactionScores.Get("US") < 1000)
                {
                    m_FactionScores.Set("US", 1000);
                    USFactionScore = 1000;
                    Print("[DEBUG_ZONE_SCORE] Radio Tower " + m_areaName + " - DESTROYED! Score set to 1000", LogLevel.WARNING);
                }

                // Deactivate defense mode now that tower is gone
                IA_Game game = IA_Game.Instantiate();
                if (game)
                {
                    IA_AreaInstance instance = game.GetAreaInstance(m_areaName);
                    if (instance)
                        instance.SetRadioTowerDefenseActive(false);
                }
	        }
	        return; // Skip standard scoring for Radio Tower
	    }

	    // Get all entities within radius
	    array<IEntity> entities = {};
	    MIKE_QueryCallback callback = new MIKE_QueryCallback(entities);
	    GetGame().GetWorld().QueryEntitiesBySphere(m_origin, m_radius, callback.OnEntityFound, FilterPlayerAndAI, EQueryEntitiesFlags.DYNAMIC);
		
	    // Count entities by faction
	    IA_DictStringInt factionCounts = new IA_DictStringInt();
	    for (int i = 0; i < entities.Count(); i++)
	    {
	        IEntity entity = entities[i];
	        if (!FilterPlayerAndAI(entity)) continue;
	        
	        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
	        if (!character) continue;
	        
	        string factionKey = GetFactionOfCharacter(character);
	        if (factionKey == "") continue;
	        
	        int oldCount = factionCounts.Get(factionKey);
	        factionCounts.Set(factionKey, oldCount + 1);
	    }
	
	    // Get counts for US and USSR factions
	    int usCount = factionCounts.Get("US");
	    int ussrCount = factionCounts.Get("USSR");
	    
	    // If no units of either faction, no score change
	    if (usCount == 0 && ussrCount == 0)
	    {
	        m_IsActive = false;
	        // Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " - No US or USSR units present, no score change", LogLevel.NORMAL);
	        return;
	    }
	    
	    m_IsActive = true;
	    
	    // Get current US score
	    float currentScore = 0.0;
	    if (m_FactionScores.Contains("US"))
	        currentScore = m_FactionScores.Get("US");
	        
	    // Determine which faction has the majority
	    float scoreChange = 0.0;
	    
	    if (usCount > ussrCount) 
	    {
	        // US has majority - increase score
	        int advantage = usCount - ussrCount;
	        float multiplier = 6.0; // Normal multiplier
	        if (usCount < 5) // If US (majority) count is less than 5
	        {
	            multiplier = 15.0; // Use faster multiplier
	        }
	        scoreChange = advantage * multiplier;
	        // Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " - US majority (+" + scoreChange + " points)", LogLevel.NORMAL);
	    }
	    /* Commented out to prevent score decreases
	    else if (ussrCount > usCount)
	    {
	        // USSR has majority - decrease score
	        int advantage = ussrCount - usCount;
	        float multiplier = 3.0; // Normal multiplier
	        if (ussrCount < 5) // If USSR (majority) count is less than 5
	        {
	            multiplier = 5.0; // Use faster multiplier
	        }
	        scoreChange = -advantage * multiplier;
	        // Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " - USSR majority (" + scoreChange + " points)", LogLevel.NORMAL);
	    }

	    */
	    
	    // Calculate new score
	    float newScore = currentScore + scoreChange;
	    
	    // Clamp score between 0-1000
	    if (newScore < 0) newScore = 0;
	    if (newScore > 1000) newScore = 1000;
	    
	    // Update USFactionScore for backward compatibility
	    USFactionScore = newScore;
	    
	    // Store the new score
	    m_FactionScores.Set("US", newScore);
	    
	    // Log score changes
	    
	    // Add threshold warnings
	    if (newScore >= 900 && newScore < 1000)
	    {
	        // Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " - APPROACHING COMPLETION THRESHOLD! Current score: " + newScore + "/100", LogLevel.WARNING);
	    }
	    else if (newScore >= 1000)
	    {
	        // Print(("[DEBUG_ZONE_SCORE] Zone " + m_areaName + " - COMPLETION THRESHOLD REACHED! Score: " + newScore + "/100", LogLevel.WARNING);
	    }
	}
		
		
	override void EOnInit(IEntity owner)
	{
        
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_origin = owner.GetOrigin();
		
		// Print(("[DEBUG] IA_AreaMarker.EOnInit called for " + m_areaName + " at " + m_origin, LogLevel.NORMAL);
		
		// Only add markers on the server side
		if (Replication.IsServer() && InitCalled == false)
		{
			// Prevent null/self being added multiple times
			if (!s_areaMarkers.Contains(this) && this != null)
			{
				/*
				//// Print(("Log1",LogLevel.NORMAL);
				InitCalled = true;
				if(!this || !m_areaName)
					return;
				foreach(IA_AreaMarker tempMarker : s_areaMarkers){
					if(!tempMarker || !tempMarker.m_areaName)
						return;
					if(tempMarker.m_areaName == m_areaName)
					{
						// Remove the duplicate marker from the array
						int index = s_areaMarkers.Find(tempMarker);
						if (index != -1)
						{
							s_areaMarkers.Remove(index);
							// Print(("[DEBUG] Removed duplicate marker: " + m_areaName, LogLevel.NORMAL);
						}
									
					}
				}*/
				
                s_areaMarkers.Insert(this);
				super.EOnInit(owner);
				// Print(("[DEBUG] IA_AreaMarker added to static list: " + m_areaName + " at " + m_origin + " (Total markers: " + s_areaMarkers.Count() + ")", LogLevel.NORMAL);
				
				// Spawn prefab entity if this is a Radio Tower
				// SpawnPrefabEntity(); // Removed from here
			}
			else
			{
				// Print(("[WARNING] IA_AreaMarker duplicate or null skipped: " + m_areaName + " at " + m_origin, LogLevel.WARNING);
			}
		}
	}

    
	 // Called when an entity enters the trigger
    override protected void OnActivate(IEntity ent)
    {
        if (IsProxy()) 
			return;

        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
        if (!character || character.GetDamageManager().GetHealth() < 0.05) 
			return;

        string factionKey = GetFactionOfCharacter(character);
        int    charHash   = GetCharacterHash(character);

        // Store the charHash->faction
        m_CharacterFactionMapping.Set(charHash, factionKey);

        // Increment faction count
        if (m_FactionCounts.Contains(factionKey))
        {
            int oldCount = m_FactionCounts.Get(factionKey);
            m_FactionCounts.Set(factionKey, oldCount + 1);
        }
        else
        {
            m_FactionCounts.Set(factionKey, 1);
        }

        //// Print(("Character ENTER zone. Faction=" + factionKey 
         //   + " newCount=" + m_FactionCounts.Get(factionKey));
    }

    //----------------------------------------------------------------------------------------------
    // Called when an entity leaves the trigger
    override void OnDeactivate(IEntity ent)
    {
        if (IsProxy()) return;

        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
        if (!character) return;

        int charHash = GetCharacterHash(character);
        if (!m_CharacterFactionMapping.Contains(charHash)) return;

        string factionKey = m_CharacterFactionMapping.Get(charHash);
        m_CharacterFactionMapping.Remove(charHash);

        if (m_FactionCounts.Contains(factionKey))
        {
            int oldCount = m_FactionCounts.Get(factionKey);
            int newCount = oldCount - 1;
            if (newCount <= 0)
                m_FactionCounts.Remove(factionKey);
            else
                m_FactionCounts.Set(factionKey, newCount);

            //// Print(("Character LEFT zone. Faction=" + factionKey 
           //     + " newCount=" + GetFactionCount(factionKey));
        }
    }

    //----------------------------------------------------------------------------------------------
    
	
	
    string GetAreaName()
    {
        return m_areaName;
    }
    
    float GetRadius()
    {
        return m_radius;
    }
    
   
	
	 // Convert the string attribute into an IA_AreaType enum.
    IA_AreaType GetAreaType()
    {
        if (m_areaType == "Town")
            return IA_AreaType.Town;
        else if (m_areaType == "City")
            return IA_AreaType.City;
        else if (m_areaType == "Property")
            return IA_AreaType.Property;
        else if (m_areaType == "Airport")
            return IA_AreaType.Airport;
        else if (m_areaType == "Docks")
            return IA_AreaType.Docks;
        else if (m_areaType == "Military")
            return IA_AreaType.Military;
        else if (m_areaType == "SmallMilitary")
            return IA_AreaType.SmallMilitary;
        else if (m_areaType == "RadioTower")
            return IA_AreaType.RadioTower;
        else if (m_areaType == "DefendObjective")
            return IA_AreaType.DefendObjective;
        return IA_AreaType.Property; // Fallback
    }
    

    // New static function following the pattern of your other examples.
    static array<IA_AreaMarker> GetAreaMarkersForArea(string areaName)
    {
        array<IA_AreaMarker> markers = {};
        if (!s_areaMarkers)
        {
            // Print(("[IA_AreaMarker.GetAreaMarkersForArea] s_areaMarkers is NULL - areaName = " + areaName, LogLevel.ERROR);
            return markers;
        }
        
        for (int i = 0; i < s_areaMarkers.Count(); i++)
        {
            IA_AreaMarker marker = s_areaMarkers[i];
            if (!marker)
            {
                // Remove invalid markers and adjust index
                // Print(("[IA_AreaMarker.GetAreaMarkersForArea] Found NULL marker at index " + i + ", removing it", LogLevel.WARNING);
                s_areaMarkers.Remove(i);
                i--;
                continue;
            }
            
            string markerAreaName = marker.GetAreaName();
            if (!markerAreaName)
            {
                // Print(("[IA_AreaMarker.GetAreaMarkersForArea] Marker has NULL area name at index " + i, LogLevel.WARNING);
                continue;
            }
            
            if (markerAreaName == areaName)
            {
                markers.Insert(marker);
                // Print(("[IA_AreaMarker.GetAreaMarkersForArea] Found marker for area: " + areaName, LogLevel.NORMAL);
            }
        }
        
        return markers;
    }
		protected bool FilterPlayerAndAI(IEntity entity) 
	{	
		if (!entity) // only humans
			return false;
		
		if(!SCR_ChimeraCharacter.Cast(entity))
			return false;
		
		SCR_ChimeraCharacter char = SCR_ChimeraCharacter.Cast(entity);
		//// Print(("Character found with "+char.GetDamageManager().GetHealth()+" damage!",LogLevel.NORMAL);
		if(char.GetDamageManager().GetHealth() < 0.01 || char.GetDamageManager().IsDestroyed())
			return false;
		//// Print(("Character is Alive!",LogLevel.NORMAL);
		return true;
	}

    // Check if the zone is under attack by checking if the US faction is gaining points
    bool IsZoneUnderAttack()
    {
        // If the zone is not active, it's not under attack
        if (!m_IsActive)
            return false;
            
        // If the US faction score is increasing, the zone is under attack
        float usScore = m_FactionScores.Get("US");
        if (usScore > 0)
            return true;
            
        return false;
    }

    // Get all markers for a specific group
    static array<IA_AreaMarker> GetAreaMarkersByGroup(int groupNumber)
    {
        array<IA_AreaMarker> groupMarkers = {};
        
        foreach (IA_AreaMarker marker : s_areaMarkers)
        {
            if (!marker)
                continue;
                
            if (marker.m_areaGroup == groupNumber)
                groupMarkers.Insert(marker);
        }
        
        return groupMarkers;
    }
    
    // Get all zone origins for a specific group
    static array<vector> GetZoneOriginsByGroup(int groupNumber)
    {
        array<vector> origins = {};
        array<IA_AreaMarker> markers = GetAreaMarkersByGroup(groupNumber);
        
        foreach (IA_AreaMarker marker : markers)
        {
            origins.Insert(marker.GetOrigin());
        }
        
        return origins;
    }
    
    // Calculate a central point (average) for all zones in a group
    static vector CalculateGroupCenterPoint(int groupNumber)
    {
        array<vector> origins = GetZoneOriginsByGroup(groupNumber);
        if (origins.IsEmpty())
            return vector.Zero;
            
        vector sumPoint = vector.Zero;
        foreach (vector origin : origins)
        {
            sumPoint += origin;
        }
        
        // Calculate average center
        return sumPoint / origins.Count();
    }
    
    // Calculate the radius needed to encompass all zones in a group
    static float CalculateGroupRadius(int groupNumber)
    {
        vector centerPoint = CalculateGroupCenterPoint(groupNumber);
        if (centerPoint == vector.Zero)
            return 0;
            
        array<vector> origins = GetZoneOriginsByGroup(groupNumber);
        float maxRadius = 0;
        
        foreach (vector origin : origins)
        {
            IA_AreaMarker marker = GetMarkerAtPosition(origin);
            if (!marker)
                continue;
                
            // Calculate distance from center to this zone + the zone's own radius
            float zoneRadius = marker.GetRadius();
            float distToCenter = vector.Distance(centerPoint, origin);
            float totalRadius = distToCenter + zoneRadius;
            
            // Update max radius if this is larger
            if (totalRadius > maxRadius)
                maxRadius = totalRadius;
        }
        
        return maxRadius;
    }
    
    // Helper to find a marker at a specific position
    static IA_AreaMarker GetMarkerAtPosition(vector position)
    {
        foreach (IA_AreaMarker marker : s_areaMarkers)
        {
            if (!marker)
                continue;
                
            if (marker.GetOrigin() == position)
                return marker;
        }
        
        return null;
    }

    // Method to spawn the prefab entity for Radio Tower areas
    protected void SpawnPrefabEntity()
    {
        // Skip if not a radio tower or no prefab specified
        if (GetAreaType() != IA_AreaType.RadioTower || m_prefabToSpawn == "")
            return;
            
        // Only spawn on server
        if (!Replication.IsServer())
            return;
            
        // Create the entity from the prefab
        EntitySpawnParams params = new EntitySpawnParams();
        params.Transform[3] = m_origin; // Set position to marker origin
        
        // Try to spawn the entity
        Resource res = Resource.Load(m_prefabToSpawn);
        if (!res)
        {
            Print("[ERROR] Failed to load Radio Tower prefab resource: " + m_prefabToSpawn, LogLevel.ERROR);
            return;
        }
        
        m_spawnedEntity = GetGame().SpawnEntityPrefab(res, null, params);
        if (!m_spawnedEntity)
        {
            Print("[ERROR] Failed to spawn Radio Tower prefab: " + m_prefabToSpawn, LogLevel.ERROR);
            return;
        }
        
        // Set up damage event handlers for the spawned entity
        SCR_DestructibleBuildingComponent destructionComp = SCR_DestructibleBuildingComponent.Cast(m_spawnedEntity.FindComponent(SCR_DestructibleBuildingComponent));
        if (destructionComp)
        {
            // Register for the OnDestroyed event using a member function
            destructionComp.GetOnDamageStateChanged().Insert(OnPrefabDestroyed);
            Print("[INFO] Radio Tower prefab spawned with destruction tracking at " + m_origin, LogLevel.NORMAL);
        }
        else
        {
            Print("[WARNING] Radio Tower prefab does not have a destruction component: " + m_prefabToSpawn, LogLevel.WARNING);
        }
    }
    
    // Event handler for when the prefab is destroyed
    protected void OnPrefabDestroyed(EDamageState state)
    {
        if (state != EDamageState.DESTROYED || m_isDestroyed)
            return;
            
        Print("[INFO] Radio Tower prefab has been destroyed!", LogLevel.NORMAL);
        m_isDestroyed = true;
        
        // --- BEGIN ADDED ---
        // Get the area instance and notify it that the tower is destroyed
        IA_Game game = IA_Game.Instantiate();
        if (game)
        {
            IA_AreaInstance instance = game.GetAreaInstance(m_areaName);
            if (instance)
            {
                instance.SetRadioTowerDestroyed();
            }
        }
        // --- END ADDED ---
        
        // --- BEGIN MODIFIED ---
        // Notify players that the objective is complete and waves have stopped
        IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
        if (initializer)
            initializer.TriggerGlobalNotification("TaskCompleted", "Radio Tower " + m_areaName + " Destroyed. Reinforcements halted.");
        // --- END MODIFIED ---

        // Set the US faction score to 1000 (maximum) to indicate completion
        m_FactionScores.Set("US", 1000);
        USFactionScore = 1000;
    }

    // New method to check if a position is within this marker's radius
    bool IsPositionInside(vector pos)
    {
        if (!this) return false; // Should not happen, but good check
        float distanceSq = vector.DistanceSq(GetOrigin(), pos);
        float radiusSq = m_radius * m_radius;
        return distanceSq <= radiusSq;
    }
};

class MIKE_QueryCallback
{
    protected array<IEntity> m_CollectedEntities;

    // Constructor: pass in the array to fill
    void MIKE_QueryCallback(array<IEntity> collectedEntities)
    {
        m_CollectedEntities = collectedEntities;
    }

    // This is the callback method for each entity found in the query
    bool OnEntityFound(IEntity entity)
    {
        m_CollectedEntities.Insert(entity);
        return true;  // return true to continue searching
    }
}



class ZoneChecker
{
	protected vector m_zoneCenter;
	protected float m_zoneRadius;

	protected PlayerManager m_playerManager;

	void ZoneChecker(vector center, float radius)
	{
		m_zoneCenter = center;
		m_zoneRadius = radius;
		m_playerManager = GetGame().GetPlayerManager();
	}

	int CheckPlayersInZone()
	{
		int numberOfPlayersInZone = 0;
		array<int> playerIds = {};
		m_playerManager.GetAllPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = m_playerManager.GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;

			if (IsPlayerInZone(playerEntity))
			{
				//// Print(("Player " + playerId + " is inside the zone.");
				numberOfPlayersInZone++;
			}
		}
		//GetGame().GetWorld().Get("IA_MissionInitializer")
		//game.getActiveArea();
		return numberOfPlayersInZone;
	}

	bool IsPlayerInZone(IEntity player)
	{
		vector playerPos = player.GetOrigin();
		float distance = vector.Distance(playerPos, m_zoneCenter);
		return distance <= m_zoneRadius;
	}
};
