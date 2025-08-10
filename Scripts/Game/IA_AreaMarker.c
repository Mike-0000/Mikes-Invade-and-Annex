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
	
	// -- Performance throttling for expensive queries
	protected float m_fQueryTimeAccumulator = 0.0;
	protected const float QUERY_INTERVAL = 1.0; // How often to run the expensive sphere query
	protected int m_iUSCountInZone = 0;
	protected int m_iUSSRCountInZone = 0;
	protected ref array<IEntity> m_entitiesInZone = new array<IEntity>();
	
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
    
    // -- New capture system variables
    protected float m_captureProgress = 0.0; // 0-120 seconds
    protected const float CAPTURE_TIME_SECONDS = 120.0; // Total capture time
    protected bool m_isCapturing = false;
    protected bool m_wasPausedLastFrame = false;
    protected bool m_hasReached50Percent = false;
    protected string m_captureStatus = "Neutral"; // "Capturing", "Paused", "Contested", "Neutral"
    
    // -- Player scoring system
    protected ref map<string, int> m_playerCaptureScores = new map<string, int>(); // PlayerGUID -> score points
    protected float m_lastScoringTime = 0;
    protected const float SCORING_INTERVAL = 1.0; // Award points every 1 second
    protected bool m_isCaptured = false; // Track if zone has been captured
    
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
    
    // Reset all markers for a new zone group
    static void ResetAllMarkersForNewGroup()
    {
        if (!s_areaMarkers)
            return;
            
        foreach (IA_AreaMarker marker : s_areaMarkers)
        {
            if (marker)
            {
                marker.ResetForNewCapture();
            }
        }
        
        Print("[IA_AreaMarker] Reset all markers for new zone group", LogLevel.DEBUG);
    }

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
	
	
	//----------------------------------------------------------------------------------------------
	// This is an expensive, throttled function to check players in the zone
	void UpdatePlayerCountsInZone()
	{
		// Get all entities within radius
		m_entitiesInZone.Clear();
		IA_DictStringInt factionCounts = new IA_DictStringInt();
		CaptureZoneQueryCallback callback = new CaptureZoneQueryCallback(factionCounts, m_entitiesInZone);
		GetGame().GetWorld().QueryEntitiesBySphere(m_origin, m_radius, callback.OnEntityFound, null, EQueryEntitiesFlags.DYNAMIC);
	
		// Update the cached counts
		m_iUSCountInZone = factionCounts.Get("US");
		m_iUSSRCountInZone = factionCounts.Get("USSR");
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
	    /*int currentTime = System.GetUnixTime();
	    if (currentTime - m_LastScoreUpdate < SCORE_UPDATE_INTERVAL)
	        return;
	        
	    m_LastScoreUpdate = currentTime;*/
	    
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
	    
	    // Special handling for Radio Tower and DefendObjective
	    IA_AreaType areaType = GetAreaType();
	    if (areaType == IA_AreaType.RadioTower)
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
            // Apply a 20-minute global QRF cooldown when a radio tower is destroyed
            IA_MissionInitializer.SetQRFDisabled(20 * 60);
	        }
	        return; // Skip standard scoring for Radio Tower
	    }
	    
	    // Skip capture logic for DefendObjective areas
	    if (areaType == IA_AreaType.DefendObjective)
	    {
	        return; // DefendObjective areas are not capturable
	    }
	    
	    // Skip capture logic if already captured
	    if (m_isCaptured)
	    {
	        return; // Zone already captured, no need to process
	    }
		
		// --- Performance: Throttle the expensive entity query ---
		m_fQueryTimeAccumulator += timeSlice;
		if (m_fQueryTimeAccumulator >= QUERY_INTERVAL)
		{
			UpdatePlayerCountsInZone();
			m_fQueryTimeAccumulator = Math.Mod(m_fQueryTimeAccumulator, QUERY_INTERVAL);
		}

	    // Get counts for US and USSR factions from cached data
	    int usCount = m_iUSCountInZone;
	    int ussrCount = m_iUSSRCountInZone;
	    
	    // Get current capture progress (now 0-120 seconds instead of 0-1000 points)
	    float previousProgress = m_captureProgress;
	    bool wasCapturing = m_isCapturing;
	    
	    // Debug logging for zone status
	    if (m_captureProgress > 0 && m_captureProgress < CAPTURE_TIME_SECONDS)
	    {
	        Print(string.Format("[CAPTURE_DEBUG] Zone %1 - US: %2, USSR: %3, wasCapturing: %4, isCapturing: %5, Progress: %6", 
	            m_areaName, usCount, ussrCount, wasCapturing, m_isCapturing, Math.Round(m_captureProgress)), LogLevel.DEBUG);
	    }
	    
	    // Check if capture should be paused when zone becomes empty
	    if (usCount == 0 && ussrCount == 0)
	    {
	        // Check if we need to send pause notification before exiting
	        if (wasCapturing && m_captureProgress > 0 && m_captureProgress < CAPTURE_TIME_SECONDS)
	        {
	            // Zone is now empty but capture was in progress
	            TriggerCaptureNotification("CapturePaused", m_areaName + " capture paused");
	            Print(string.Format("[CAPTURE] Zone %1 - PAUSED (empty zone) - Progress: %2/%3 seconds", 
	                m_areaName, Math.Round(m_captureProgress), CAPTURE_TIME_SECONDS), LogLevel.WARNING);
	        }
	        
	        m_IsActive = false;
	        m_isCapturing = false;
	        m_captureStatus = "Neutral";
	        return;
	    }
	    
	    m_IsActive = true;
	    
	    // Determine capture state based on majority
	    if (usCount > ussrCount) 
	    {
	        // US has majority - capture progresses (only if not already captured)
	        if (!m_isCaptured)
	        {
	            m_isCapturing = true;
	            m_captureStatus = "Capturing";
	            
				// Increment progress and scoring accumulator by timeSlice for frame-rate independence
				m_captureProgress += timeSlice;
				m_fTimeAccumulator += timeSlice;
	            
				// If scoring interval has passed, award points
				if (m_fTimeAccumulator >= SCORING_INTERVAL)
				{
					int pointsToAward = Math.Floor(m_fTimeAccumulator / SCORING_INTERVAL);
					if (pointsToAward > 0)
					{
						AwardCapturePoints(m_entitiesInZone, pointsToAward);
					}
					// Decrement accumulator by the amount of time accounted for
					m_fTimeAccumulator = Math.Mod(m_fTimeAccumulator, SCORING_INTERVAL);
				}
	        }
	        else
	        {
	            // Zone already captured, just maintain captured status
	            m_captureStatus = "Captured";
	        }
	        
	        // Handle notifications
	        if (!wasCapturing)
	        {
	            // Capture just started
	            TriggerCaptureNotification("CaptureStarted", m_areaName + " capture started");
	        }
	        
	        // Check 50% threshold
	        if (!m_hasReached50Percent && m_captureProgress >= CAPTURE_TIME_SECONDS * 0.5)
	        {
	            m_hasReached50Percent = true;
	            TriggerCaptureNotification("Capture50Percent", m_areaName + " 50% captured");
	        }
	    }
	    else if (ussrCount >= usCount)
	    {
	        // USSR has majority or equal - capture pauses
	        m_isCapturing = false;
	        
	        if (ussrCount > usCount)
	            m_captureStatus = "Contested";
	        else
	            m_captureStatus = "Neutral";
	        
	        // Handle pause notification
	        if (wasCapturing && m_captureProgress > 0 && m_captureProgress < CAPTURE_TIME_SECONDS)
	        {
	            // Capture was in progress but now paused
	            TriggerCaptureNotification("CapturePaused", m_areaName + " capture paused");
	        }
	    }
	    
	    // Clamp progress between 0 and capture time
	    m_captureProgress = Math.Clamp(m_captureProgress, 0, CAPTURE_TIME_SECONDS);
	    
	    // Update faction score for compatibility (scale to 0-1000 range)
	    float scaledScore = (m_captureProgress / CAPTURE_TIME_SECONDS) * 1000.0;
	    USFactionScore = scaledScore;
	    m_FactionScores.Set("US", scaledScore);
	    
	    // Debug logging
	    if (m_isCapturing)
	    {
	        int remainingTime = Math.Ceil(CAPTURE_TIME_SECONDS - m_captureProgress);
	        Print(string.Format("[CAPTURE] Zone %1 - Progress: %2/%3 seconds (%4%%) - Time remaining: %5s", 
	            m_areaName, 
	            Math.Round(m_captureProgress), 
	            CAPTURE_TIME_SECONDS,
	            Math.Round((m_captureProgress / CAPTURE_TIME_SECONDS) * 100),
	            remainingTime), LogLevel.DEBUG);
	    }
	    
	    // Check if capture is complete
	    if (m_captureProgress >= CAPTURE_TIME_SECONDS && !m_isCaptured)
	    {
	        m_isCaptured = true; // Mark as captured to prevent repeated logging
	        m_isCapturing = false; // Stop capturing state
	        
	        Print(string.Format("[CAPTURE] Zone %1 - CAPTURED! Top contributors being calculated...", m_areaName), LogLevel.NORMAL);
	        
	        // Get and log top contributors
	        array<string> topContributors = GetTopContributors(3);
	        
	        // --- BEGIN MODIFIED: Queue stats for each contributor ---
	        IA_StatsManager statsManager = IA_StatsManager.GetInstance();
	        if (statsManager)
	        {
	            foreach (string playerGuid, int score : m_playerCaptureScores)
	            {
	                if (score > 0)
	                {
	                    string playerName = GetPlayerNameFromGuid(playerGuid);
	                    statsManager.QueueCaptureContribution(playerGuid, playerName, score);
	                }
	            }
	        }
	        // --- END MODIFIED ---
	        
	        // Build the capture completion message with top contributors
	        string captureMessage = m_areaName + " Captured!";
	        
	        if (!topContributors.IsEmpty())
	        {
	            captureMessage += " Top contributors: ";
	            
	            for (int i = 0; i < topContributors.Count(); i++)
	            {
	                string playerGuid = topContributors[i];
	                string playerName = GetPlayerNameFromGuid(playerGuid);
	                int score = GetPlayerScore(playerGuid);
	                
	                                // Format: "1. PlayerName (60)"
                captureMessage += string.Format("%1. %2 (%3)", i + 1, playerName, score);
	                
	                if (i < topContributors.Count() - 1)
	                    captureMessage += ", ";
	                
	                // Also log to console
	                Print(string.Format("  %1. %2 (GUID: %3): %4 seconds", i + 1, playerName, playerGuid, score), LogLevel.NORMAL);
	            }
	        }
	        
	        // Send notification to all players
	        TriggerCaptureNotification("TaskCompleted", captureMessage);
	        
	        // Note: Score reset will be handled by IA_MissionInitializer when checking completion
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
	/*	protected bool FilterPlayerAndAI(IEntity entity) 
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
	}*/

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
            initializer.TriggerGlobalNotification("TaskCompleted", "Radio Tower " + m_areaName + " Destroyed. Reinforcements halted. QRF disabled for 20 minutes.");
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
    
    // --- Player Scoring Methods ---
    protected void AwardCapturePoints(array<IEntity> entities, int points = 1)
    {
        int playersAwarded = 0;
        
        foreach (IEntity entity : entities)
        {
            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
            if (!character) continue;
            
            // Check if US faction
            string factionKey = GetFactionOfCharacter(character);
            if (factionKey != "US") continue;
            
            // Check if player-controlled
            int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
            if (playerId > 0)
            {
                // Get player GUID
                string playerGuid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
                if (!playerGuid || playerGuid == "")
                {
                    Print(string.Format("[CAPTURE_SCORING] Could not get GUID for player %1", playerId), LogLevel.WARNING);
                    continue;
                }
                
                // Award 1 point
                int currentScore = 0;
                if (m_playerCaptureScores.Contains(playerGuid))
                {
                    currentScore = m_playerCaptureScores[playerGuid];
                }
                
                m_playerCaptureScores[playerGuid] = currentScore + points;
                playersAwarded++;
            }
        }
        
        // Debug logging
        if (playersAwarded > 0)
        {
            Print(string.Format("[CAPTURE_SCORING] Zone %1 - Awarded points to %2 US players", m_areaName, playersAwarded), LogLevel.DEBUG);
        }
    }
    
    array<string> GetTopContributors(int maxCount = 3)
    {
        array<string> sortedPlayers = new array<string>();
        array<int> sortedScores = new array<int>();
        
        // Copy all entries to arrays for sorting
        foreach (string playerGuid, int score : m_playerCaptureScores)
        {
            // Only include if they have at least 5 points (5 seconds of contribution)
            if (score >= 5)
            {
                sortedPlayers.Insert(playerGuid);
                sortedScores.Insert(score);
            }
        }
        
        // Simple bubble sort by score (highest first)
        for (int i = 0; i < sortedScores.Count() - 1; i++)
        {
            for (int j = 0; j < sortedScores.Count() - i - 1; j++)
            {
                if (sortedScores[j] < sortedScores[j + 1])
                {
                    // Swap scores
                    int tempScore = sortedScores[j];
                    sortedScores[j] = sortedScores[j + 1];
                    sortedScores[j + 1] = tempScore;
                    
                    // Swap players
                    string tempPlayer = sortedPlayers[j];
                    sortedPlayers[j] = sortedPlayers[j + 1];
                    sortedPlayers[j + 1] = tempPlayer;
                }
            }
        }
        
        // Return top contributors
        array<string> topContributors = new array<string>();
        for (int i = 0; i < Math.Min(maxCount, sortedPlayers.Count()); i++)
        {
            topContributors.Insert(sortedPlayers[i]);
        }
        
        return topContributors;
    }
    
    // Get score for specific player by GUID
    int GetPlayerScore(string playerGuid)
    {
        if (m_playerCaptureScores.Contains(playerGuid))
            return m_playerCaptureScores[playerGuid];
        return 0;
    }
    
    // Reset scores (call after zone capture)
    void ResetCaptureScores()
    {
        m_playerCaptureScores.Clear();
        // Don't reset m_isCaptured here - that should only be reset when actually starting a new zone group
    }
    
    // Fully reset the zone for a new capture (called when moving to next zone group)
    void ResetForNewCapture()
    {
        m_playerCaptureScores.Clear();
        m_isCapturing = false;
        m_hasReached50Percent = false;
        m_isCaptured = false;
        m_captureProgress = 0.0;
        m_captureStatus = "Neutral";
    }
    
    // Trigger notification to all players
    void TriggerCaptureNotification(string messageType, string message)
    {
        array<int> playerIDs = new array<int>();
        GetGame().GetPlayerManager().GetAllPlayers(playerIDs);
        foreach (int playerID : playerIDs)
        {
            PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerID);
            if (pc)
            {
                SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());
                if (character)
                {
                    character.SetUIOne(messageType, message, playerID);
                }
            }
        }
    }
    
    // Get capture progress percentage
    float GetCapturePercentage()
    {
        return (m_captureProgress / CAPTURE_TIME_SECONDS) * 100.0;
    }
    
    // Get remaining capture time
    int GetRemainingCaptureTime()
    {
        if (m_captureProgress >= CAPTURE_TIME_SECONDS)
            return 0;
        return Math.Ceil(CAPTURE_TIME_SECONDS - m_captureProgress);
    }
    
    // Helper method to get player name from GUID (for future implementation)
    static string GetPlayerNameFromGuid(string playerGuid)
    {
        // You'll need to implement a lookup system here
        // For now, just return a shortened version of the GUID
		return GetGame().GetPlayerManager().GetPlayerNameByIdentity(playerGuid);
    }
    
    // Check if zone is captured (needed by mission initializer)
    bool IsCaptured()
    {
        return m_isCaptured;
    }
};

class CaptureZoneQueryCallback
{
    protected IA_DictStringInt m_FactionCounts;
    protected ref array<IEntity> m_CollectedLiveEntities;

    void CaptureZoneQueryCallback(IA_DictStringInt factionCounts, array<IEntity> collectedLiveEntities)
    {
        m_FactionCounts = factionCounts;
        m_CollectedLiveEntities = collectedLiveEntities;
    }

    bool OnEntityFound(IEntity entity)
    {
        // This is now the filter and the processor combined.
		if (!entity)
			return true;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return true;
		
		if(character.GetDamageManager().GetHealth() < 0.01 || character.GetDamageManager().IsDestroyed())
			return true;
        
        // Entity is a live character, add it to the list for point scoring
        m_CollectedLiveEntities.Insert(entity);

        // Now, get faction and count it.
        string factionKey = "";
        Faction fac = character.GetFaction();
        if (fac) 
            factionKey = fac.GetFactionKey();

        if (factionKey != "")
        {
            int oldCount = m_FactionCounts.Get(factionKey);
            m_FactionCounts.Set(factionKey, oldCount + 1);
        }
        
        return true; // continue searching
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
