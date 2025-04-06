



class IA_AreaMarkerClass : ScriptedGameTriggerEntityClass
{
};

class IA_AreaMarker : ScriptedGameTriggerEntity
{
	
	int USFactionScore;
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

    // -- For authority/proxy check
    protected RplComponent m_RplComponent;

	 //----------------------------------------------------------------------------------------------
    protected vector GetZoneCenter()
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
        return s_areaMarkers;
    }

    // Attributes to set in the editor
    [Attribute(defvalue: "CaptureZone", desc: "Name of the capture area.")]
    protected string m_areaName;
    
    [Attribute(defvalue: "City", desc: "Type of area (Town, City, Property, Airport, Docks, Military).")]
    protected string m_areaType;
    
    [Attribute(defvalue: "500", desc: "Radius of the area.")]
    protected float m_radius;
    
    // We store the marker’s origin once initialized.
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
	        //Print("[ERROR] owner is NULL", LogLevel.ERROR);
	        return;
	    }
	
	    if (IsProxy())
	    {
	        //Print("[DEBUG] Entity is proxy, skipping frame update", LogLevel.NORMAL);
	        return;
	    }
	
	    // Accumulate the elapsed time every frame
	    m_fTimeAccumulator += timeSlice;
	
	    // Throttle every 200 frames
	    if (m_iFrameCount < 200)
	    {
	        m_iFrameCount++;
	        return;
	    }
	    m_iFrameCount = 0;
	
	    // Use the accumulated time since the last sphere query
	    float elapsedTime = m_fTimeAccumulator;
	    m_fTimeAccumulator = 0.0; // reset accumulator
	
	    // Verify world and radius
	    World world = owner.GetWorld();
	    if (!world)
	    {
	        //Print("[ERROR] World object is NULL", LogLevel.ERROR);
	        return;
	    }
	
	    //Print("[DEBUG] Starting EOnFrame sphere query", LogLevel.NORMAL);
	
	    array<IEntity> entitiesFound = {};
	    MIKE_QueryCallback cb = new MIKE_QueryCallback(entitiesFound);
	
	    vector center = GetZoneCenter();
	
	   // Print("[DEBUG] Zone center=" + center + " Radius=" + m_fZoneRadius, LogLevel.NORMAL);
	
	    world.QueryEntitiesBySphere(center, m_fZoneRadius*6.66, cb.OnEntityFound, FilterPlayerAndAI, EQueryEntitiesFlags.DYNAMIC);
	
	    if (!entitiesFound)
	    {
	        Print("[WARNING] No entities found in sphere query", LogLevel.WARNING);
	        return;
	    }
	
	    IA_DictStringInt localCounts = new IA_DictStringInt();
	
	    foreach (IEntity ent : entitiesFound)
	    {
	        if (!ent)
	        {
	            Print("[WARNING] NULL entity encountered in entitiesFound", LogLevel.WARNING);
	            continue;
	        }
	
	        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
	        if (!character)
	        {
	            //Print("[DEBUG] Entity is not a SCR_ChimeraCharacter, skipping", LogLevel.NORMAL);
	            continue;
	        }
	
	        string factionKey = GetFactionOfCharacter(character);
	        if (factionKey == "" || factionKey == "CIV")
	        {
	            //Print("[DEBUG] Character has empty or Civilian factionKey, skipping", LogLevel.NORMAL);
	            continue;
	        }
	
	        int oldCount = localCounts.Get(factionKey);
	        localCounts.Set(factionKey, oldCount + 1);
	    }
	
	    //Print("[DEBUG] Local counts updated, total factions=" + localCounts.GetCount(), LogLevel.NORMAL);
	
	    m_FactionCounts.Clear();
	    int localTotal = localCounts.GetCount();
	
	    for (int i = 0; i < localTotal; i++)
	    {
	        string fKey;
	        int fVal;
	        localCounts.GetPair(i, fKey, fVal);
	        m_FactionCounts.Set(fKey, fVal);
	
	        //Print("[DEBUG] Faction=" + fKey + " Count=" + fVal, LogLevel.NORMAL);
	    }
	
	    string leadFactionKey = "";
	    int leadCount = 0;
	    int secondCount = 0;
	
	    for (int j = 0; j < localCounts.GetCount(); j++)
	    {
	        string testKey;
	        int testVal;
	        localCounts.GetPair(j, testKey, testVal);
	
	        if (testVal > leadCount)
	        {
	            secondCount = leadCount;
	            leadCount = testVal;
	            leadFactionKey = testKey;
	        }
	        else if (testVal > secondCount)
	        {
	            secondCount = testVal;
	        }
	    }
	
	    //Print("[DEBUG] Leading faction=" + leadFactionKey + " LeadCount=" + leadCount + " SecondCount=" + secondCount, LogLevel.NORMAL);
	
	    if (leadFactionKey == "" || leadCount == secondCount || leadFactionKey != "US")
	    {
	        Print("[DEBUG] No clear leading faction or tie encountered", LogLevel.NORMAL);
	        return;
	    }
	
	    int gap = leadCount - secondCount;
		
	    float oldScore = 0.0;
	    if (m_FactionScores.Contains(leadFactionKey))
	        oldScore = m_FactionScores.Get(leadFactionKey);
		
	    // Use the full elapsed time rather than just the last frame's timeSlice.
	    float newScore = oldScore + ((gap * elapsedTime)*score_limiter_ratio);
	    m_FactionScores.Set(leadFactionKey, newScore);
		USFactionScore = newScore;
	
	    Print("[INFO] Score updated for faction=" + leadFactionKey + " OldScore=" + oldScore + " NewScore=" + newScore + " (gap=" + gap + ")", LogLevel.NORMAL);
	}
		
		
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		m_origin = owner.GetOrigin();
	
		// Prevent null/self being added multiple times
		if (!s_areaMarkers.Contains(this) && this != null)
		{
			s_areaMarkers.Insert(this);
			//Print("[DEBUG] IA_AreaMarker added to static list: " + m_areaName + " at " + m_origin, LogLevel.NORMAL);
		}
		else
		{
			Print("[WARNING] IA_AreaMarker duplicate or null skipped: " + m_areaName, LogLevel.WARNING);
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

        //Print("Character ENTER zone. Faction=" + factionKey 
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

            //Print("Character LEFT zone. Faction=" + factionKey 
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
        return IA_AreaType.Property; // Fallback
    }
    

    // New static function following the pattern of your other examples.
    static array<IA_AreaMarker> GetAreaMarkersForArea(string areaName)
    {
        array<IA_AreaMarker> markers = {};
        if (!s_areaMarkers)
        {
            Print("[IA_AreaMarker.GetAreaMarkersForArea] s_areaMarkers is NULL - areaName = " + areaName, LogLevel.ERROR);
            return markers;
        }
        for (int i = 0; i < s_areaMarkers.Count(); ++i)
        {
            IA_AreaMarker marker = s_areaMarkers[i];
            if (!marker)
            {
                s_areaMarkers.Remove(i);
                --i;
                continue;
            }
            if (marker.GetAreaName() == areaName)
            {
                markers.Insert(marker);
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
		//Print("Character found with "+char.GetDamageManager().GetHealth()+" damage!",LogLevel.NORMAL);
		if(char.GetDamageManager().GetHealth() < 0.01 || char.GetDamageManager().IsDestroyed())
			return false;
		//Print("Character is Alive!",LogLevel.NORMAL);
		return true;
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
				//Print("Player " + playerId + " is inside the zone.");
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
