///////////////////////////////////////////////////////////////////////
// 1) AI ORDERS ENUM & WAYPOINT RESOURCE
///////////////////////////////////////////////////////////////////////
enum IA_AiOrder
{
    Move,
    Defend,
    Patrol,
    SearchAndDestroy,
    GetInVehicle,
    GetOutOfVehicle
};

ResourceName IA_AiOrderResource(IA_AiOrder order)
{
    Print("[DEBUG] IA_AiOrderResource called with order: " + order, LogLevel.NORMAL);
    if (order == IA_AiOrder.Defend)
    {
        Print("[DEBUG] Returning resource for Defend", LogLevel.NORMAL);
        return "{D9C14ECEC9772CC6}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Defend.et";
    }
    else if (order == IA_AiOrder.Patrol)
    {
        Print("[DEBUG] Returning resource for Patrol", LogLevel.NORMAL);
        return "{C0A9A9B589802A5B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Patrol.et";
    }
    else if (order == IA_AiOrder.SearchAndDestroy)
    {
        Print("[DEBUG] Returning resource for SearchAndDestroy", LogLevel.NORMAL);
        return "{EE9A99488B40628B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_SearchAndDestroy.et";
    }
    else if (order == IA_AiOrder.GetInVehicle)
    {
        Print("[DEBUG] Returning resource for GetInVehicle", LogLevel.NORMAL);
        return "{0A2A37B4A56D74DF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetInNearest.et";
    }
    else if (order == IA_AiOrder.GetOutOfVehicle)
    {
        Print("[DEBUG] Returning resource for GetOutOfVehicle", LogLevel.NORMAL);
        return "{2602CAB8AB74FBBF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetOut.et";
    }
    Print("[DEBUG] Returning default resource for Move", LogLevel.NORMAL);
    return "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
}

///////////////////////////////////////////////////////////////////////
// 2) FACTION & SQUAD ENUMS
///////////////////////////////////////////////////////////////////////
enum IA_Faction
{
    NONE = 0,
    US,
    USSR,
    CIV
};

enum IA_SquadType
{
    Riflemen = 0,
    Firesquad,
    Medic,
    Antitank
};

///////////////////////////////////////////////////////////////////////
// 3) AI GROUP
///////////////////////////////////////////////////////////////////////
class IA_AiGroup
{
    private SCR_AIGroup m_group = null;
    private bool        m_isSpawned = false;
    private vector      m_initialPosition;

    private IA_SquadType m_squadType;
    private IA_Faction   m_faction;
    private bool         m_isCivilian;

    private vector m_lastOrderPosition;
    private int    m_lastOrderTime;

    private bool   m_isDriving     = false;
    private vector m_drivingTarget = vector.Zero;
    private IEntity m_referencedEntity;

    private IA_Faction m_engagedEnemyFaction = IA_Faction.NONE;

    private void IA_AiGroup(vector initialPos, IA_SquadType squad, IA_Faction fac)
    {
        Print("[DEBUG] IA_AiGroup constructor called with pos: " + initialPos + ", squad: " + squad + ", faction: " + fac, LogLevel.NORMAL);
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
	    Print("[DEBUG] CreateMilitaryGroup called", LogLevel.NORMAL);
	    IA_AiGroup grp = new IA_AiGroup(initialPos,squadType,faction);
	    grp.m_lastOrderPosition = initialPos;
	    grp.m_initialPosition   = initialPos;
	    grp.m_lastOrderTime     = 0;
	    grp.m_squadType         = squadType;
	    grp.m_faction           = faction;
	    grp.m_isCivilian        = false;
	    return grp;
	}

	
	bool HasActiveWaypoint()
	{
		Print("Checking if Active Waypoints",LogLevel.NORMAL);
	    if (!m_group)
	        return false;
	
	    array<AIWaypoint> wps = {};
	    m_group.GetWaypoints(wps);
		
	    foreach (AIWaypoint wp : wps)
	    {
	        SCR_AIWaypoint scrWp = SCR_AIWaypoint.Cast(wp);
	        if (!scrWp)
	            continue;
			
	        if (!scrWp.IsDeleted()) // ✅ Only count non-finished ones
	            return true;
	    }
	
	    return false;
	}

	
    static IA_AiGroup CreateCivilianGroup(vector initialPos)
    {
        Print("[DEBUG] CreateCivilianGroup called with pos: " + initialPos, LogLevel.NORMAL);
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV);
        grp.m_isCivilian = true;
        return grp;
    }

    IA_Faction GetEngagedEnemyFaction()
    {
        Print("[DEBUG] GetEngagedEnemyFaction called. Returning: " + m_engagedEnemyFaction, LogLevel.NORMAL);
        return m_engagedEnemyFaction;
    }

    bool IsEngagedWithEnemy()
    {
        bool engaged = (m_engagedEnemyFaction != IA_Faction.NONE);
        Print("[DEBUG] IsEngagedWithEnemy called. Engaged: " + engaged, LogLevel.NORMAL);
        return engaged;
    }

    void MarkAsUnengaged()
    {
        Print("[DEBUG] MarkAsUnengaged called. Resetting engaged enemy faction.", LogLevel.NORMAL);
        m_engagedEnemyFaction = IA_Faction.NONE;
    }

    array<SCR_ChimeraCharacter> GetGroupCharacters()
    {
        Print("[DEBUG] GetGroupCharacters called.", LogLevel.NORMAL);
        array<SCR_ChimeraCharacter> chars = {};
        if (!m_group || !m_isSpawned)
        {
            Print("[DEBUG] Group not spawned or m_group is null.", LogLevel.NORMAL);
            return chars;
        }

        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        Print("[DEBUG] Found " + agents.Count() + " agents in the group.", LogLevel.NORMAL);
        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                Print("[IA_AiGroup] Cast to SCR_ChimeraCharacter failed!", LogLevel.WARNING);
                continue;
            }
            Print("[DEBUG] Successfully cast agent to SCR_ChimeraCharacter.", LogLevel.NORMAL);
            chars.Insert(ch);
        }
        return chars;
    }

    void AddOrder(vector origin, IA_AiOrder order, bool topPriority = false)
    {
        Print("[DEBUG] AddOrder called with origin: " + origin + ", order: " + order + ", topPriority: " + topPriority, LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            Print("[IA_AiGroup.AddOrder] Group not spawned!", LogLevel.WARNING);
            return;
        }
        if (!m_group)
        {
            Print("[DEBUG] m_group is null in AddOrder.", LogLevel.NORMAL);
            return;
        }

        // Adjust spawn height based on terrain
        float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
        Print("[DEBUG] Surface Y calculated: " + y, LogLevel.NORMAL);
        origin[1] = y + 0.5;

        ResourceName rname = IA_AiOrderResource(order);
        Resource res = Resource.Load(rname);
        if (!res)
        {
            Print("[IA_AiGroup.AddOrder] Missing resource: " + rname, LogLevel.ERROR);
            return;
        }
        Print("[DEBUG] Resource loaded successfully: " + rname, LogLevel.NORMAL);

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        Print("[DEBUG] Spawned waypoint entity.", LogLevel.NORMAL);
        SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
        if (!w)
        {
            Print("[IA_AiGroup.AddOrder] Could not cast to AIWaypoint: " + rname, LogLevel.ERROR);
            return;
        }
        Print("[DEBUG] Successfully cast entity to SCR_AIWaypoint.", LogLevel.NORMAL);

        if (m_isDriving || topPriority)
        {
            w.SetPriorityLevel(2000);
            Print("[DEBUG] Set waypoint priority level to 2000.", LogLevel.NORMAL);
        }

        m_group.AddWaypoint(w);
        Print("[DEBUG] Waypoint added to group.", LogLevel.NORMAL);
        m_lastOrderPosition = origin;
        m_lastOrderTime     = System.GetUnixTime();
        Print("[DEBUG] Order timestamp updated to " + m_lastOrderTime, LogLevel.NORMAL);
    }

    void SetInitialPosition(vector pos)
    {
        Print("[DEBUG] SetInitialPosition called with pos: " + pos, LogLevel.NORMAL);
        m_initialPosition = pos;
    }

    int TimeSinceLastOrder()
    {
        int delta = System.GetUnixTime() - m_lastOrderTime;
        Print("[DEBUG] TimeSinceLastOrder called. Delta: " + delta, LogLevel.NORMAL);
        return delta;
    }

    bool HasOrders()
    {
        if (!m_group)
        {
            Print("[DEBUG] HasOrders called but m_group is null.", LogLevel.NORMAL);
            return false;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        Print("[DEBUG] HasOrders found " + wps.Count() + " waypoints.", LogLevel.NORMAL);
        return !wps.IsEmpty();
    }

    bool IsDriving()
    {
        Print("[DEBUG] IsDriving called. Returning: " + m_isDriving, LogLevel.NORMAL);
        return m_isDriving;
    }

    void RemoveAllOrders(bool resetLastOrderTime = false)
    {
        Print("[DEBUG] RemoveAllOrders called. resetLastOrderTime: " + resetLastOrderTime, LogLevel.NORMAL);
        if (!m_group)
        {
            Print("[DEBUG] m_group is null in RemoveAllOrders.", LogLevel.NORMAL);
            return;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        Print("[DEBUG] Removing " + wps.Count() + " waypoints.", LogLevel.NORMAL);
        foreach (AIWaypoint w : wps)
        {
            m_group.RemoveWaypoint(w);
        }
        m_isDriving = false;
        if (resetLastOrderTime)
        {
            m_lastOrderTime = 0;
            Print("[DEBUG] Last order time reset.", LogLevel.NORMAL);
        }
    }

    int GetAliveCount()
    {
        Print("[DEBUG] GetAliveCount called.", LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            if (m_isCivilian)
            {
                Print("[DEBUG] Group is civilian and not spawned. Returning count 1.", LogLevel.NORMAL);
                return 1;
            }
            else
            {
                int count = IA_SquadCount(m_squadType, m_faction);
                Print("[DEBUG] Group is military and not spawned. Returning squad count: " + count, LogLevel.NORMAL);
                return count;
            }
        }
        if (!m_group)
        {
            Print("[DEBUG] m_group is null in GetAliveCount. Returning 0.", LogLevel.NORMAL);
            return 0;
        }
        int aliveCount = m_group.GetPlayerAndAgentCount();
        Print("[DEBUG] Alive count from group: " + aliveCount, LogLevel.NORMAL);
        return aliveCount;
    }

    vector GetOrigin()
    {
        if (!m_group)
        {
            Print("[DEBUG] GetOrigin called but m_group is null. Returning vector.Zero.", LogLevel.NORMAL);
            return vector.Zero;
        }
        vector origin = m_group.GetOrigin();
        Print("[DEBUG] GetOrigin returning: " + origin, LogLevel.NORMAL);
        return origin;
    }

    bool IsSpawned()
    {
        Print("[DEBUG] IsSpawned called. Returning: " + m_isSpawned, LogLevel.NORMAL);
        return m_isSpawned;
    }

void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
{
    Print("[DEBUG] Spawn called with initialOrder: " + initialOrder + ", orderPos: " + orderPos, LogLevel.NORMAL);
    if (!PerformSpawn())
    {
        Print("[DEBUG] PerformSpawn returned false. Aborting Spawn.", LogLevel.NORMAL);
        return;
    }

    vector posToUse;
    if (orderPos != vector.Zero)
    {
        posToUse = orderPos;
    }
    else
    {
        posToUse = IA_Game.rng.GenerateRandomPointInRadius(5, 25, m_initialPosition); // <-- ADD VARIANCE!
    }

    AddOrder(posToUse, initialOrder);
}


	
	
	
    private bool PerformSpawn()
    {
        Print("[DEBUG] PerformSpawn called.", LogLevel.NORMAL);
        if (IsSpawned())
        {
            Print("[DEBUG] Already spawned. Returning false.", LogLevel.NORMAL);
            return false;
        }
        m_isSpawned = true;

        string resourceName;
        if (m_isCivilian)
        {
            resourceName = IA_RandomCivilianResourceName();
            Print("[DEBUG] Spawning civilian. Resource: " + resourceName, LogLevel.NORMAL);
        }
        else
        {
            resourceName = IA_SquadResourceName(m_squadType, m_faction);
            Print("[DEBUG] Spawning military. Resource: " + resourceName, LogLevel.NORMAL);
        }

        Resource res = Resource.Load(resourceName);
        if (!res)
        {
            Print("[IA_AiGroup.PerformSpawn] No resource: " + resourceName, LogLevel.ERROR);
            return false;
        }

        IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
        Print("[DEBUG] Spawned main entity for group.", LogLevel.NORMAL);
        if (m_isCivilian)
        {
            // For civilians, spawn an extra group prefab and add the character
            Resource groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
            IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
            m_group = SCR_AIGroup.Cast(groupEnt);
            Print("[DEBUG] Spawned civilian group entity.", LogLevel.NORMAL);

            if (!m_group.AddAIEntityToGroup(entity))
            {
                Print("[IA_AiGroup.PerformSpawn] Could not add civ to group: " + resourceName, LogLevel.ERROR);
            }
            else
            {
                Print("[DEBUG] Civilian added to group successfully.", LogLevel.NORMAL);
            }
        }
        else
        {
            m_group = SCR_AIGroup.Cast(entity);
            Print("[DEBUG] Cast main entity to SCR_AIGroup.", LogLevel.NORMAL);
        }
	if (m_group)
	{
	    vector groundPos = m_initialPosition;
	    float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
	    groundPos[1] = groundY;
	    

	    
	    // Also update the origin if that function exists.
	    m_group.SetOrigin(groundPos);
	    
	    Print("[DEBUG] Group position forced to ground level: " + groundPos, LogLevel.NORMAL);
	}

        if (!m_group)
        {
            Print("[IA_AiGroup.PerformSpawn] Could not cast to AIGroup", LogLevel.ERROR);
            return false;
        }

        SetupDeathListener();
        WaterCheck();
        Print("[DEBUG] PerformSpawn completed successfully.", LogLevel.NORMAL);
        return true;
    }

    void Despawn()
    {
        Print("[DEBUG] Despawn called.", LogLevel.NORMAL);
        if (!IsSpawned())
        {
            Print("[DEBUG] Group is not spawned. Nothing to despawn.", LogLevel.NORMAL);
            return;
        }
        m_isSpawned = false;
        m_isDriving = false;

        if (m_group)
        {
            Print("[DEBUG] Deactivating all members of the group.", LogLevel.NORMAL);
            m_group.DeactivateAllMembers();
        }
        RemoveAllOrders();
        IA_Game.AddEntityToGc(m_group);
        m_group = null;
        Print("[DEBUG] Group despawned successfully.", LogLevel.NORMAL);
    }

    IA_Faction GetFaction()
    {
        Print("[DEBUG] GetFaction called. Returning: " + m_faction, LogLevel.NORMAL);
        return m_faction;
    }

    private void SetupDeathListener()
    {
        Print("[DEBUG] SetupDeathListener called.", LogLevel.NORMAL);
        if (!m_group)
        {
            Print("[DEBUG] m_group is null in SetupDeathListener.", LogLevel.NORMAL);
            return;
        }
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);

        if (agents.IsEmpty())
        {
            Print("[DEBUG] No agents found. Retrying SetupDeathListener in 1000 ms.", LogLevel.NORMAL);
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }

        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                Print("[DEBUG] Agent does not have a valid SCR_ChimeraCharacter.", LogLevel.NORMAL);
                continue;
            }
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
            {
                Print("[IA_AiGroup.SetupDeathListener] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
                continue;
            }
            Print("[DEBUG] Inserting death callback for agent.", LogLevel.NORMAL);
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
        }
    }

    private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {
        Print("[DEBUG] OnMemberDeath called.", LogLevel.NORMAL);
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
            Print("[DEBUG] Member killed by player.", LogLevel.NORMAL);
            // If a player kills one of our guys and we’re not yet “engaged,” 
            // set some placeholder for engaged faction (CIV is used as a placeholder).
            if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
            {
                Print("[IA_AiGroup.OnMemberDeath] Player killed enemy, group engaged with CIV as placeholder");
                m_engagedEnemyFaction = IA_Faction.CIV; 
            }
        }
        else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
        {
            Print("[DEBUG] Member killed by AI.", LogLevel.NORMAL);
            SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
            if (!c)
            {
                Print("[IA_AiGroup.OnMemberDeath] cast killerEntity to character failed!", LogLevel.ERROR);
            }
            else
            {
                string fKey = c.GetFaction().GetFactionKey();
                Print("[DEBUG] Killer faction key: " + fKey, LogLevel.NORMAL);
                if (fKey == "US")
                {
                    m_engagedEnemyFaction = IA_Faction.US;
                }
                else if (fKey == "USSR")
                {
                    m_engagedEnemyFaction = IA_Faction.USSR;
                }
                else if (fKey == "CIV")
                {
                    m_engagedEnemyFaction = IA_Faction.CIV;
                }
                else
                {
                    Print("[IA_AiGroup.OnMemberDeath] unknown faction key " + fKey, LogLevel.WARNING);
                }
                // If friendly fire, reset engagement.
                if (m_engagedEnemyFaction == m_faction)
                {
                    Print("[IA_AiGroup.OnMemberDeath] Friendly kill => resetting engagement");
                    m_engagedEnemyFaction = IA_Faction.NONE;
                }
            }
        }
    }

    private void WaterCheck()
    {
        Print("[DEBUG] WaterCheck called (placeholder).", LogLevel.NORMAL);
        // No extra water check here because safe areas are pre-defined.
    }
};

///////////////////////////////////////////////////////////////////////
// 4) AREA TYPE
///////////////////////////////////////////////////////////////////////
enum IA_AreaType
{
    Town,
    City,
    Property,
    Airport,
    Docks,
    Military
};

///////////////////////////////////////////////////////////////////////
// 5) IA_Area - NEW: Predefined safe areas
///////////////////////////////////////////////////////////////////////
class IA_Area
{
    // For AI spawns relative to an area.
    static const float SpawnAiAreaRadius = 500;

    private string      m_name;
    private IA_AreaType m_type;
    private vector      m_origin;
    private float       m_radius;
    private bool        m_instantiated = false;

    private void IA_Area(string nm, IA_AreaType t, vector org, float rad)
    {
        Print("[DEBUG] IA_Area constructor called with name: " + nm + ", type: " + t + ", origin: " + org + ", radius: " + rad, LogLevel.NORMAL);
        m_name   = nm;
        m_type   = t;
        m_origin = org;
        m_radius = rad;
    }

static IA_Area Create(string nm, IA_AreaType t, vector org, float rad)
{
    Print("[DEBUG] IA_Area.Create called for area: " + nm, LogLevel.NORMAL);
    IA_Area area = new IA_Area(nm, t, org, rad);
    IA_Game.s_allAreas.Insert(area); // <-- Keeps strong ref
    return area;
}


    string GetName()
    {
        return m_name;
    }
    IA_AreaType GetAreaType()
    {
        return m_type;
    }
    vector GetOrigin()
    {
        return m_origin;
    }
    float GetRadius()
    {
        return m_radius;
    }

    bool IsInstantiated()
    {
        return m_instantiated;
    }
    void SetInstantiated(bool val)
    {
        m_instantiated = val;
    }

    int GetMilitaryAiGroupCount()
    {
        // Customize based on area type
        if (m_type == IA_AreaType.Town)
            return 4;
        else if (m_type == IA_AreaType.City)
            return 8;
        else if (m_type == IA_AreaType.Docks)
            return 2;
        else if (m_type == IA_AreaType.Airport)
            return 6;
        else if (m_type == IA_AreaType.Military)
            return 3;
        else if (m_type == IA_AreaType.Property)
            return 1;
        return 0;
    }

    int GetCivilianCount()
    {
        if (m_type == IA_AreaType.Town)
            return 6;
        else if (m_type == IA_AreaType.City)
            return 12;
        else if (m_type == IA_AreaType.Docks)
            return 3;
        else if (m_type == IA_AreaType.Property)
            return 2;
        return 0;
    }
};

///////////////////////////////////////////////////////////////////////
// 6) AREA ATTACKERS
///////////////////////////////////////////////////////////////////////
class IA_AreaAttackers
{
    private static const int ORDER_FREQ_SECS = 60;

    private bool m_initialOrderGiven = false;
    private IA_Faction   m_faction;
    private ref IA_Area  m_area;
    private ref array<ref IA_AiGroup> m_groups = {};
    private bool m_initializing = true;
    private int  m_groupCount;
    private int  m_groupToProcess = 0;

    void IA_AreaAttackers(IA_Faction fac, IA_Area area)
    {
        Print("[DEBUG] IA_AreaAttackers constructor called with faction: " + fac + " and area: " + area.GetName(), LogLevel.NORMAL);
        m_faction    = fac;
        m_area       = area;
        m_groupCount = area.GetMilitaryAiGroupCount();
    }

    bool IsInitializing()
    {
        return m_initializing;
    }

    void SpawnNextGroup(vector origin)
    {
        Print("[DEBUG] SpawnNextGroup called with origin: " + origin, LogLevel.NORMAL);
        if (m_groups.Count() == m_groupCount)
            return;

        IA_SquadType st = IA_GetRandomSquadType();
        Print("[DEBUG] Random squad type chosen: " + st, LogLevel.NORMAL);
        IA_AiGroup grp  = IA_AiGroup.CreateMilitaryGroup(origin, st, m_faction);
        m_groups.Insert(grp);
        Print("[DEBUG] New military group created and inserted.", LogLevel.NORMAL);
        grp.Spawn();

        if (m_groups.Count() == m_groupCount)
        {
            m_initializing = false;
            Print("[DEBUG] All groups spawned. Initializing complete.", LogLevel.NORMAL);
        }
    }

    void ProcessNextGroup()
    {
        Print("[DEBUG] ProcessNextGroup called.", LogLevel.NORMAL);
        if (m_groupToProcess >= m_groups.Count())
        {
            m_groupToProcess = 0;
            Print("[DEBUG] Resetting m_groupToProcess to 0.", LogLevel.NORMAL);
        }
        if (m_groups.IsEmpty())
            return;

        IA_AiGroup g = m_groups[m_groupToProcess];
        if (g.GetAliveCount() == 0)
        {
            m_groups.Remove(m_groupToProcess);
            return;
        }
        if (g.TimeSinceLastOrder() >= ORDER_FREQ_SECS)
            g.RemoveAllOrders();
        if (!g.HasOrders())
        {
            vector pos;
            if (m_initialOrderGiven)
                pos = m_area.GetOrigin();
            else
            {
                // Use the IA_Game’s random generator to generate a point within the safe area.
                pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
                m_initialOrderGiven = true;
            }
            g.AddOrder(pos, IA_AiOrder.SearchAndDestroy);
        }
        m_groupToProcess = m_groupToProcess + 1;
    }

    bool IsAllGroupsDead()
    {
        if (m_initializing)
            return false;
        if (m_groups.IsEmpty())
            return true;
        return false;
    }

    bool IsAnyEngaged()
    {
        foreach (IA_AiGroup g : m_groups)
        {
            if (g.IsEngagedWithEnemy())
                return true;
        }
        return false;
    }

    void DespawnAll()
    {
        foreach (IA_AiGroup g : m_groups)
        {
            g.Despawn();
        }
    }

    IA_Faction GetFaction()
    {
        return m_faction;
    }

    array<ref IA_AiGroup> GetGroups()
    {
        return m_groups;
    }
};

///////////////////////////////////////////////////////////////////////
// 7) REINFORCEMENT STATE
///////////////////////////////////////////////////////////////////////
enum IA_ReinforcementState
{
    NotDone,
    Countdown,
    Done
};

///////////////////////////////////////////////////////////////////////
// 8) IA_AreaInstance - dynamic mission/task handling (integrates area)
///////////////////////////////////////////////////////////////////////
class IA_AreaInstance
{
    IA_Area m_area;
    IA_Faction m_faction;
    int m_strength;

    private ref array<ref IA_AiGroup> m_military  = {};
    private ref array<ref IA_AiGroup> m_civilians = {};
    private ref array<IA_Faction>     m_attackingFactions = {};

    private ref IA_AreaAttackers m_aiAttackers = null;

    private IA_ReinforcementState m_reinforcements = IA_ReinforcementState.NotDone;
    private int m_reinforcementTimer = 0;
    private int m_currentTask = 0;
    private bool m_canSpawn   = true;

    // --- Dynamic Task Variables ---
    private SCR_TriggerTask m_currentTaskEntity;
    private ref array<SCR_TriggerTask> m_taskQueue = {};

	
	
static IA_AreaInstance Create(IA_Area area, IA_Faction faction, int startStrength = 0)
{
	if(!area){
		Print("[DEBUG] area is NULL! ", LogLevel.NORMAL);
		return null;
	}
    IA_AreaInstance inst = new IA_AreaInstance();  // uses implicit no-arg constructor

    inst.m_area = area;
    inst.m_faction = faction;
    inst.m_strength = startStrength;

    Print("[DEBUG] IA_AreaInstance.Create: Initialized with area = " + area.GetName(), LogLevel.NORMAL);

    inst.m_area.SetInstantiated(true);

    int groupCount = area.GetMilitaryAiGroupCount();
    inst.GenerateRandomAiGroups(groupCount, true);

    int civCount = area.GetCivilianCount();
    inst.GenerateCivilians(civCount);

    return inst;
}
/*

    // Constructor
    void IA_AreaInstance(IA_Area area, IA_Faction faction, int startStrength = 0)
    {
        Print("[DEBUG] IA_AreaInstance constructor called for area: " + area.GetName() + ", faction: " + faction + ", startStrength: " + startStrength, LogLevel.NORMAL);
        m_area     = area;
        m_faction  = faction;
        m_strength = startStrength;

        m_area.SetInstantiated(true);

        int groupCount = m_area.GetMilitaryAiGroupCount();
        GenerateRandomAiGroups(groupCount, true);

        int civCount = m_area.GetCivilianCount();
        GenerateCivilians(civCount);
    }
*/
    // --- Task Queue Methods ---
    void QueueTask(string title, string description, vector spawnOrigin)
    {
        Print("[DEBUG] QueueTask called with title: " + title + ", description: " + description + ", spawnOrigin: " + spawnOrigin, LogLevel.NORMAL);
        if (!m_currentTaskEntity)
        {
            CreateTask(title, description, spawnOrigin);
        }
        else
        {
            Resource res = Resource.Load("{33DA4D098C409421}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
            IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
            SCR_TriggerTask task = SCR_TriggerTask.Cast(taskEnt);
            task.SetTitle(title);
            task.SetDescription(description);
            m_taskQueue.Insert(task);
            Print("[DEBUG] Task queued. Queue length: " + m_taskQueue.Count(), LogLevel.NORMAL);
        }
    }

    private void CreateTask(string title, string description, vector spawnOrigin)
    {
        Print("[DEBUG] CreateTask called with title: " + title + ", spawnOrigin: " + spawnOrigin, LogLevel.NORMAL);
        Resource res = Resource.Load("{33DA4D098C409421}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
        IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
        m_currentTaskEntity = SCR_TriggerTask.Cast(taskEnt);
        m_currentTaskEntity.SetTitle(title);
        m_currentTaskEntity.SetDescription(description);
        m_currentTaskEntity.Create(false);
        Print("[DEBUG] Task created and activated.", LogLevel.NORMAL);
    }

    void CompleteCurrentTask()
    {
        Print("[DEBUG] CompleteCurrentTask called.", LogLevel.NORMAL);
        if (m_currentTaskEntity)
        {
            m_currentTaskEntity.Finish();
            m_currentTaskEntity = null;
        }
        if (!m_taskQueue.IsEmpty())
        {
            m_currentTaskEntity = m_taskQueue[0];
            m_taskQueue.Remove(0);
            m_currentTaskEntity.Create(false);
            Print("[DEBUG] Next queued task activated.", LogLevel.NORMAL);
        }
    }

    void UpdateTask()
    {
        if (m_currentTaskEntity && m_currentTaskEntity.GetTaskState() == SCR_TaskState.FINISHED)
        {
            CompleteCurrentTask();
        }
    }

    void RunNextTask()
    {
        m_currentTask = m_currentTask + 1;
        if (m_currentTask == 1)
        {
            // Placeholder cycle
        }
        else if (m_currentTask == 2)
        {
            ReinforcementsTask();
        }
        else if (m_currentTask == 3)
        {
            StrengthUpdateTask();
        }
        else if (m_currentTask == 4)
        {
            // Placeholder cycle
        }
        else if (m_currentTask == 5)
        {
            MilitaryOrderTask();
        }
        else if (m_currentTask == 6)
        {
            CivilianOrderTask();
        }
        else if (m_currentTask == 7)
        {
            AiAttackersTask();
        }
        else if (m_currentTask == 8)
        {
            m_currentTask = 0;
        }
        UpdateTask();
    }

    // --- Other methods remain similar ---
    bool IsUnderAttack()
    {
        return (!m_attackingFactions.IsEmpty());
    }

    void OnAttacked(IA_Faction attacker)
    {
        if (attacker == m_faction)
            return;
        if (m_reinforcements == IA_ReinforcementState.NotDone)
        {
            m_reinforcements = IA_ReinforcementState.Countdown;
        }
    }

    void OnFactionChange(IA_Faction newFaction)
    {
        m_faction = newFaction;
        m_attackingFactions.Clear();
        m_strength = 0;
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep)
            rep.SetFaction(m_area.GetName(), newFaction);
        if (m_aiAttackers && newFaction != IA_Faction.CIV)
        {
            delete m_aiAttackers;
            m_aiAttackers = null;
        }
    }

    void OnStrengthChange(int newVal)
    {
        m_strength = newVal;
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep && m_area)
            rep.SetStrength(m_area.GetName(), newVal);
    }

    private void ReinforcementsTask()
    {
        if (m_reinforcements != IA_ReinforcementState.Countdown)
            return;
        m_reinforcementTimer = m_reinforcementTimer + 1;
        if (m_reinforcementTimer > 600)
        {
            m_reinforcements = IA_ReinforcementState.Done;
            Print("[IA_AreaInstance.ReinforcementsTask] reinforcements would arrive now");
        }
    }

    private void StrengthUpdateTask()
    {
        int totalCount = 0;
        foreach (IA_AiGroup g : m_military)
        {
            totalCount = totalCount + g.GetAliveCount();
            if (g.IsEngagedWithEnemy())
            {
                IA_Faction enemyFac = g.GetEngagedEnemyFaction();
                if (enemyFac != m_faction && enemyFac != IA_Faction.NONE && !m_attackingFactions.Contains(enemyFac))
                {
                    m_attackingFactions.Insert(enemyFac);
                    OnAttacked(enemyFac);
                }
            }
        }
        if (m_aiAttackers && m_aiAttackers.IsAnyEngaged())
        {
            IA_Faction attFac = m_aiAttackers.GetFaction();
            if (!m_attackingFactions.Contains(attFac))
            {
                m_attackingFactions.Insert(attFac);
                OnAttacked(attFac);
            }
        }
        if (totalCount != m_strength)
        {
            OnStrengthChange(totalCount);
        }
        if (m_strength == 0)
        {
            if (m_attackingFactions.Count() == 1)
            {
                OnFactionChange(m_attackingFactions[0]);
            }
            else if (m_attackingFactions.Count() > 1 && m_faction != IA_Faction.NONE)
            {
                m_faction = IA_Faction.NONE;
            }
        }
    }

    private void MilitaryOrderTask()
    {
	if (!this)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: this is null!", LogLevel.ERROR); // Never seems to happen
        return;
    }
		
		
		if (!m_area)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR); // Always seems to happen
        return;
    } else {
			Print("NOT NULL!", LogLevel.NORMAL);

		}
        if (!m_canSpawn || m_military.IsEmpty())
            return;
        IA_AiOrder order = IA_AiOrder.Patrol;
        if (IsUnderAttack())
            order = IA_AiOrder.SearchAndDestroy;
        foreach (IA_AiGroup g : m_military)
        {
            if (!g.IsSpawned() || g.GetAliveCount() == 0)
                continue;
            if (g.TimeSinceLastOrder() >= 60)
                g.RemoveAllOrders();
            if (!g.HasActiveWaypoint())
            {
                vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
                g.AddOrder(pos, order);
            }
        }
    }

    private void CivilianOrderTask()
    {
		    if (!m_area)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
        if (!m_canSpawn || m_civilians.IsEmpty())
            return;
        foreach (IA_AiGroup g : m_civilians)
        {
            if (!g.IsSpawned() || g.GetAliveCount() == 0)
                continue;
            if (g.TimeSinceLastOrder() >= 60)
            {
                g.RemoveAllOrders();
            }
			if (!g.HasActiveWaypoint())
            {
			    vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
                g.AddOrder(pos, IA_AiOrder.Patrol);
			}
        }
    }

    private void AiAttackersTask()
    {
		    if (!m_area)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
        if (!m_aiAttackers)
            return;
        if (m_aiAttackers.IsInitializing())
        {
            vector pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 1.2, m_area.GetRadius() * 1.3, m_area.GetOrigin());
            m_aiAttackers.SpawnNextGroup(pos);
            return;
        }
        if (m_aiAttackers.IsAllGroupsDead())
        {
            IA_Faction f = m_aiAttackers.GetFaction();
            m_attackingFactions.RemoveItem(f);
            delete m_aiAttackers;
            m_aiAttackers = null;
            return;
        }
        m_aiAttackers.ProcessNextGroup();
    }

    void GenerateRandomAiGroups(int number, bool insideArea)
    {
        int strCounter = m_strength;
        for (int i = 0; i < number; i = i + 1)
        {
			    if (!m_area)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
            IA_SquadType st = IA_GetRandomSquadType();
            vector pos = vector.Zero;
            if (insideArea)
                pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
            else
                pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 0.95, m_area.GetRadius() * 1.05, m_area.GetOrigin());
            IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroup(pos, st, m_faction);
            grp.Spawn();
            strCounter = strCounter + grp.GetAliveCount();
            m_military.Insert(grp);
        }
        OnStrengthChange(strCounter);
    }

    void GenerateCivilians(int number)
    {
		    if (!m_area)
    {
        Print("[ERROR] IA_AreaInstance.MilitaryOrderTask: m_area is null!", LogLevel.ERROR);
        return;
    }
        m_civilians.Clear();
        for (int i = 0; i < number; i = i + 1)
        {
            vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
            float y = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
            pos[1] = y;
            IA_AiGroup civ = IA_AiGroup.CreateCivilianGroup(pos);
            civ.Spawn();
            m_civilians.Insert(civ);
        }
    }
};

///////////////////////////////////////////////////////////////////////
// 9) IA_Game SINGLETON
///////////////////////////////////////////////////////////////////////
class IA_Game
{
	static ref array<ref IA_Area> s_allAreas = {};
    static private ref IA_Game m_instance = null;
    static bool HasInstance()
    {
        return (m_instance != null);
    }

    static ref RandomGenerator rng = new RandomGenerator();

    private bool m_periodicTaskActive = false;
    private ref array<IA_AreaInstance> m_areas = {};
	static private bool beenInstantiated = false;
    static private ref array<IEntity> m_entityGc = {};

    private bool m_hasInit = false;

    void Init()
    {
        Print("[DEBUG] IA_Game.Init called.", LogLevel.NORMAL);
        if (m_hasInit)
            return;

        m_hasInit = true;
        ActivatePeriodicTask();
    }
	
	static IA_Game Instantiate()
	{
		if(!Replication.IsServer()){
			return m_instance;
		}
		Print("[DEBUG] IA_Game.Instantiate called.", LogLevel.NORMAL);
		
	    if (beenInstantiated){
			Print("[DEBUG] IA_Game.Instantiate Already Instantiated. = "+beenInstantiated, LogLevel.NORMAL);
	        return m_instance;
		}
		Print("[DEBUG] IA_Game.Instantiate Going.", LogLevel.NORMAL);
		
	    m_instance = new IA_Game();
	    m_instance.Init();
		beenInstantiated = true;
	    return m_instance;
	}
	
/*
    void ~IA_Game()
    {
        m_areas.Clear();
        m_entityGc.Clear();
    }
*/
    static void AddEntityToGc(IEntity e)
    {
        m_entityGc.Insert(e);
    }

    private static void EntityGcTask()
    {
        if (m_entityGc.IsEmpty())
            return;
        delete m_entityGc[0];
        m_entityGc.Remove(0);
    }

private void ActivatePeriodicTask()
{
    if (m_periodicTaskActive)
        return;
    m_periodicTaskActive = true;
    // Use a static wrapper so the instance’s PeriodicalGameTask is called correctly.
    GetGame().GetCallqueue().CallLater(PeriodicalGameTaskWrapper, 200, true);
    GetGame().GetCallqueue().CallLater(EntityGcTask, 250, true);
}

	// Static wrapper that retrieves the singleton instance and calls its PeriodicalGameTask.
	static void PeriodicalGameTaskWrapper()
	{
	    IA_Game gameInstance = IA_Game.Instantiate();
	    if (gameInstance)
	    {
	        gameInstance.PeriodicalGameTask();
	    }
	}


void PeriodicalGameTask()
{
    Print("IA_Game.PeriodicalGameTask: Periodical Game Task has started", LogLevel.NORMAL);
    if (m_areas.IsEmpty())
    {
        // Log that no areas exist yet so you can see that the task is running.
        Print("IA_Game.PeriodicalGameTask: m_areas is empty. Waiting for initialization.", LogLevel.NORMAL);
        return;
    }
    foreach (IA_AreaInstance areaInst : m_areas)
    {
			if(!areaInst.m_area){
				Print("areaInst.m_area is Null!!!",LogLevel.WARNING);
				return;
			}else{
				Print("areaInst.m_area "+ areaInst.m_area.GetName() +"is NOT Null",LogLevel.NORMAL);
			areaInst.RunNextTask();
			}
        
    }
}


    IA_AreaInstance AddArea(IA_Area area, IA_Faction fac, int strength = 0)
    {
		IA_AreaInstance inst = IA_AreaInstance.Create(area, fac, strength);
		if(!inst.m_area){
			Print("IA_AreaInstance inst.m_area is NULL!", LogLevel.ERROR);
		}
		Print("Adding Area " + area.GetName(), LogLevel.WARNING);
        m_areas.Insert(inst);
		Print("m_areas.Count() = " + m_areas.Count(), LogLevel.WARNING);
        IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
        if (rep){
            Print("Adding Area in Replication " + area.GetName(), LogLevel.WARNING);
			rep.AddArea(inst);
		}
        return inst;
    }

    IA_AreaInstance FindAreaInstance(string name)
    {
        foreach (IA_AreaInstance a : m_areas)
        {
            if (a.m_area.GetName() == name)
                return a;
        }
        return null;
    }
};

///////////////////////////////////////////////////////////////////////
// 10) REPLICATION WORKAROUND
///////////////////////////////////////////////////////////////////////
class IA_ReplicationWorkaroundClass : GenericEntityClass {}

class IA_ReplicatedAreaInstance
{
    void IA_ReplicatedAreaInstance(string nm, IA_Faction fac, int strVal)
    {
        Print("[DEBUG] IA_ReplicatedAreaInstance constructor called with name: " + nm + ", faction: " + fac + ", strength: " + strVal, LogLevel.NORMAL);
        name = nm;
        faction = fac;
        strength = strVal;
    }
    string name;
    IA_Faction faction;
    int strength;
};

class IA_ReplicationWorkaround : GenericEntity
{
    static private ref array<ref IA_ReplicatedAreaInstance> m_areaList = {};
    static private ref array<ref IA_AreaInstance> m_areaInstances = {};

    static private IA_ReplicationWorkaround m_instance;

    void IA_ReplicationWorkaround(IEntitySource src, IEntity parent)
    {
        Print("[DEBUG] IA_ReplicationWorkaround constructor called.", LogLevel.NORMAL);
        m_instance = this;
    }

    static IA_ReplicationWorkaround Instance()
    {
        return m_instance;
    }

    static void Reset()
    {
        m_areaList.Clear();
        m_areaInstances.Clear();
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcTo_AddArea(string areaName, IA_Faction fac, int strVal)
    {
        Print("[DEBUG] RpcTo_AddArea called for area: " + areaName + ", faction: " + fac + ", strength: " + strVal, LogLevel.NORMAL);
        m_areaList.Insert(new IA_ReplicatedAreaInstance(areaName, fac, strVal));
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcDo_SetFaction(string areaName, int facInt)
    {
		if(!Replication.IsRunning())
			return;
        if (!IA_Game.HasInstance())
            return;
          IA_Game g = IA_Game.Instantiate();
        IA_AreaInstance inst = g.FindAreaInstance(areaName);
        if (!inst)
            return;
        IA_Faction f = IA_FactionFromInt(facInt);
        inst.OnFactionChange(f);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcDo_SetStrength(string areaName, int s)
    {
		if(!Replication.IsRunning())
			return;
        if (!IA_Game.HasInstance())
            return;
        IA_Game g = IA_Game.Instantiate();
        IA_AreaInstance inst = g.FindAreaInstance(areaName);
        if (!inst)
            return;
        inst.OnStrengthChange(s);
    }

    void AddArea(IA_AreaInstance aInst)
    {
        m_areaInstances.Insert(aInst);
        Rpc(RpcTo_AddArea, aInst.m_area.GetName(), aInst.m_faction, aInst.m_strength);
    }

    void SetFaction(string areaName, IA_Faction f)
    {
        int fi = IA_FactionToInt(f);
        Rpc(RpcDo_SetFaction, areaName, fi);
    }

    void SetStrength(string areaName, int val)
    {
        Rpc(RpcDo_SetStrength, areaName, val);
    }

    override bool RplSave(ScriptBitWriter writer)
    {
        int c = m_areaInstances.Count();
        writer.Write(c, 16);
        for (int i = 0; i < c; i = i + 1)
        {
            IA_AreaInstance inst = m_areaInstances[i];
            writer.WriteString(inst.m_area.GetName());
            writer.Write(inst.m_strength, 8);
            int fInt = IA_FactionToInt(inst.m_faction);
            writer.Write(fInt, 8);
        }
        return true;
    }

    override bool RplLoad(ScriptBitReader reader)
    {
        int areaCount;
        if (!reader.Read(areaCount, 16))
            return false;
        for (int i = 0; i < areaCount; i = i + 1)
        {
            string nm;
            int strVal;
            int facInt;
            if (!reader.ReadString(nm) || !reader.Read(strVal, 8) || !reader.Read(facInt, 8))
                return false;
            IA_Faction ff = IA_FactionFromInt(facInt);
            IA_ReplicatedAreaInstance rep = new IA_ReplicatedAreaInstance(nm, ff, strVal);
            m_areaList.Insert(rep);
        }
        return true;
    }
};

///////////////////////////////////////////////////////////////////////
// 11) HELPER FACTION / SQUAD UTILS
///////////////////////////////////////////////////////////////////////
string IA_SquadResourceName_US(IA_SquadType st)
{
    Print("[DEBUG] IA_SquadResourceName_US called with squad type: " + st, LogLevel.NORMAL);
    if (st == IA_SquadType.Riflemen)
        return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
    else if (st == IA_SquadType.Firesquad)
        return "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
    else if (st == IA_SquadType.Medic)
        return "{EF62027CC75A7459}Prefabs/Groups/BLUFOR/Group_US_MedicalSection.et";
    else if (st == IA_SquadType.Antitank)
        return "{FAEA8B9E1252F56E}Prefabs/Groups/BLUFOR/Group_US_Team_LAT.et";
    return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
}

string IA_SquadResourceName_USSR(IA_SquadType st)
{
    Print("[DEBUG] IA_SquadResourceName_USSR called with squad type: " + st, LogLevel.NORMAL);
    if (st == IA_SquadType.Riflemen)
        return "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
    else if (st == IA_SquadType.Firesquad)
        return "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
    else if (st == IA_SquadType.Medic)
        return "{D815658156080328}Prefabs/Groups/OPFOR/Group_USSR_MedicalSection.et";
    else if (st == IA_SquadType.Antitank)
        return "{96BAB56E6558788E}Prefabs/Groups/OPFOR/Group_USSR_Team_AT.et";
    return "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
}

static ref array<string> s_IA_CivList = {
    "{8C7093AF368F496A}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_1.et",
    "{DF7F8D5C05CC1AF6}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_2.et",
    "{408B8BD5E3F09FF3}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_3.et",
    "{035F8F1CEF3B187F}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_4.et",
    "{E6C3C3E5E3DE8F14}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_5.et",
    "{8A97F7055F1A003A}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_CottonShirt_6.et",
    "{11EB9A0D2A5899EA}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_1.et",
    "{3AE3C1A509298D9D}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_2.et",
    "{C943F3CC53D187B6}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Turtleneck_1.et",
    "{1FFE2B88BEF51840}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Turtleneck_2.et",
    "{3A2FBAD5B929AC4B}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Turtleneck_3.et",

    "{6F5A71376479B353}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_1.et",
    "{C97B985FCFC3E4D6}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_2.et",
    "{2CE7D4A6C32673BD}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_3.et",
    "{11D3F19EB64AFA8A}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_4.et",
    "{F44FBD67BAAF6DE1}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_5.et",

    "{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et",
    "{A517C72CEF150898}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_2.et",
    "{626874113CAE4387}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_3.et"
};

string IA_RandomCivilianResourceName()
{
    int idx = IA_Game.rng.RandInt(0, s_IA_CivList.Count());
    Print("[DEBUG] IA_RandomCivilianResourceName chosen index: " + idx, LogLevel.NORMAL);
    return s_IA_CivList[idx];
}

string IA_SquadResourceName(IA_SquadType st, IA_Faction fac)
{
    Print("[DEBUG] IA_SquadResourceName called with squad type: " + st + ", faction: " + fac, LogLevel.NORMAL);
    if (fac == IA_Faction.US)
        return IA_SquadResourceName_US(st);
    else if (fac == IA_Faction.USSR)
        return IA_SquadResourceName_USSR(st);
    return IA_SquadResourceName_US(st);
}

int IA_SquadCount(IA_SquadType st, IA_Faction fac)
{
    Print("[DEBUG] IA_SquadCount called with squad type: " + st + ", faction: " + fac, LogLevel.NORMAL);
    if (fac == IA_Faction.US)
    {
        if (st == IA_SquadType.Riflemen)
            return 9;
        else if (st == IA_SquadType.Firesquad)
            return 4;
        else if (st == IA_SquadType.Medic)
            return 2;
        else if (st == IA_SquadType.Antitank)
            return 4;
        return 9;
    }
    else if (fac == IA_Faction.USSR)
    {
        if (st == IA_SquadType.Riflemen)
            return 6;
        else if (st == IA_SquadType.Firesquad)
            return 4;
        else if (st == IA_SquadType.Medic)
            return 2;
        else if (st == IA_SquadType.Antitank)
            return 4;
        return 6;
    }
    return 1; // civ fallback
}

IA_SquadType IA_GetRandomSquadType()
{
    int r = IA_Game.rng.RandInt(0, 4);
    Print("[DEBUG] IA_GetRandomSquadType generated random number: " + r, LogLevel.NORMAL);
    if (r == 0)
        return IA_SquadType.Riflemen;
    else if (r == 1)
        return IA_SquadType.Firesquad;
    else if (r == 2)
        return IA_SquadType.Medic;
    else if (r == 3)
        return IA_SquadType.Antitank;
    return IA_SquadType.Riflemen;
}

int IA_FactionToInt(IA_Faction f)
{
    Print("[DEBUG] IA_FactionToInt called with faction: " + f, LogLevel.NORMAL);
    if (f == IA_Faction.US)
        return 1;
    else if (f == IA_Faction.USSR)
        return 2;
    else if (f == IA_Faction.CIV)
        return 3;
    return 0;
}

IA_Faction IA_FactionFromInt(int i)
{
    Print("[DEBUG] IA_FactionFromInt called with int: " + i, LogLevel.NORMAL);
    if (i == 1)
        return IA_Faction.US;
    else if (i == 2)
        return IA_Faction.USSR;
    else if (i == 3)
        return IA_Faction.CIV;
    return IA_Faction.NONE;
}

///////////////////////////////////////////////////////////////////////
// 12) HELPER SPAWN PARAMS
///////////////////////////////////////////////////////////////////////
EntitySpawnParams IA_CreateSimpleSpawnParams(vector origin)
{
    Print("[DEBUG] IA_CreateSimpleSpawnParams called with origin: " + origin, LogLevel.NORMAL);
    EntitySpawnParams p = new EntitySpawnParams();
    p.TransformMode = ETransformMode.WORLD;
    p.Transform[3]  = origin;
    return p;
}

EntitySpawnParams IA_CreateSurfaceAdjustedSpawnParams(vector origin)
{
    Print("[DEBUG] IA_CreateSurfaceAdjustedSpawnParams called with origin: " + origin, LogLevel.NORMAL);
    float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
    origin[1] = y;
    EntitySpawnParams p = new EntitySpawnParams();
    p.TransformMode = ETransformMode.WORLD;
    p.Transform[3]  = origin;
    return p;
}

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
    // Simple flag to ensure we only run once.
    protected bool m_bInitialized = false;

	
	
	
	void InitializeNow()
	{
		Print("[DEBUG] IA_MissionInitializer: Delayed initialization running.", LogLevel.NORMAL);
	
		array<IA_AreaMarker> markerMarkers = IA_AreaMarker.GetAllMarkers();
		
		if (markerMarkers.IsEmpty())
		{
			Print("[IA_MissionInitializer] No IA_AreaMarker entities found in the world!", LogLevel.WARNING);
			return;
		}
	
		foreach (IA_AreaMarker marker : markerMarkers)
		{
					
			if (!marker)
			{
				Print("[WARNING] Null IA_AreaMarker detected in markerMarkers list. Skipping.", LogLevel.WARNING);
				continue;
			}
			// Now these values should be set correctly
			vector pos = marker.GetOrigin();
			string name = marker.GetAreaName();
			float radius = marker.GetRadius();
			
			Print("[DEBUG] Processing marker: " + name + " at " + pos.ToString(), LogLevel.NORMAL);
	
			IA_Area area = IA_Area.Create(name, marker.GetAreaType(), pos, radius);
			IA_AreaInstance areaInstance = IA_Game.Instantiate().AddArea(area, IA_Faction.USSR, 0);
	
			string taskTitle = "Capture " + area.GetName();
			string taskDescription = "Eliminate enemy presence and secure " + area.GetName();
			areaInstance.QueueTask(taskTitle, taskDescription, pos);
	
			Print("[DEBUG] Added area and queued task for: " + area.GetName() + " placed at: " + pos.ToString(), LogLevel.NORMAL);
		}
	}

	
	void InitDelayed(IEntity owner){
	
		Print("[DEBUG] IA_MissionInitializer EOnInit called.", LogLevel.NORMAL);

        // Only execute on the server
        if (!Replication.IsServer())
        {
            Print("[IA_MissionInitializer] Client/Proxy detected => skipping area initialization.", LogLevel.NORMAL);
			
            return;
        }

        // Prevent re-initialization
        if (m_bInitialized)
        {
            Print("[DEBUG] IA_MissionInitializer already initialized. Exiting.", LogLevel.NORMAL);
            return;
        }
		
		
		
        m_bInitialized = true;

        Print("[IA_MissionInitializer] Running on Server. Beginning dynamic area setup...", LogLevel.NORMAL);

        // 1) Check that IA_ReplicationWorkaround is present.
        if (!IA_ReplicationWorkaround.Instance())
        {
            Print("[IA_MissionInitializer] WARNING: No IA_ReplicationWorkaround entity found!", LogLevel.ERROR);
            Print("[IA_MissionInitializer] Please place a GenericEntity with the IA_ReplicationWorkaround script in the world!", LogLevel.NORMAL);
        }
       	Print("[IA_MissionInitializer] DEBUG 5!", LogLevel.ERROR);

        // 2) Initialize the IA_Game Singleton.
        IA_Game game = IA_Game.Instantiate();

        // 3) Set the RNG seed.
        int seed = System.GetUnixTime();
        IA_Game.rng.SetSeed(seed);
        Print("[IA_MissionInitializer] RNG Seed set to: " + seed, LogLevel.NORMAL);

        // 4) Retrieve all IA_AreaMarker entities from the static registry.
        array<IA_AreaMarker> markerMarkers = IA_AreaMarker.GetAllMarkers();
		
		
		
		GetGame().GetCallqueue().CallLater(InitializeNow, 200, false);
		
		
      

        Print("[IA_MissionInitializer] Dynamic areas/tasks set up. Initialization complete.", LogLevel.NORMAL);
	
	}
	
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);
		GetGame().GetCallqueue().CallLater(InitDelayed, 5000, false, owner);
        
    }
}


class IA_AreaMarkerClass : GenericEntityClass
{
};

class IA_AreaMarker : GenericEntity
{
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

override void EOnInit(IEntity owner)
{
	super.EOnInit(owner);

	m_origin = owner.GetOrigin();

	// Prevent null/self being added multiple times
	if (!s_areaMarkers.Contains(this) && this != null)
	{
		s_areaMarkers.Insert(this);
		Print("[DEBUG] IA_AreaMarker added to static list: " + m_areaName + " at " + m_origin, LogLevel.NORMAL);
	}
	else
	{
		Print("[WARNING] IA_AreaMarker duplicate or null skipped: " + m_areaName, LogLevel.WARNING);
	}
}

    
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
};
