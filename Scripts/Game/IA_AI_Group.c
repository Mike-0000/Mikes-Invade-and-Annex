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
        //Print("[DEBUG] IA_AiGroup constructor called with pos: " + initialPos + ", squad: " + squad + ", faction: " + fac, LogLevel.NORMAL);
        m_lastOrderPosition = initialPos;
        m_initialPosition   = initialPos;
        m_lastOrderTime     = 0;
        m_squadType         = squad;
        m_faction           = fac;
    }

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
	    //Print("[DEBUG] CreateMilitaryGroup called", LogLevel.NORMAL);
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
		//Print("Checking if Active Waypoints",LogLevel.NORMAL);
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
        //Print("[DEBUG] CreateCivilianGroup called with pos: " + initialPos, LogLevel.NORMAL);
        IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV);
        grp.m_isCivilian = true;
        return grp;
    }

    IA_Faction GetEngagedEnemyFaction()
    {
        //Print("[DEBUG] GetEngagedEnemyFaction called. Returning: " + m_engagedEnemyFaction, LogLevel.NORMAL);
        return m_engagedEnemyFaction;
    }

    bool IsEngagedWithEnemy()
    {
        bool engaged = (m_engagedEnemyFaction != IA_Faction.NONE);
        //Print("[DEBUG] IsEngagedWithEnemy called. Engaged: " + engaged, LogLevel.NORMAL);
        return engaged;
    }

    void MarkAsUnengaged()
    {
        //Print("[DEBUG] MarkAsUnengaged called. Resetting engaged enemy faction.", LogLevel.NORMAL);
        m_engagedEnemyFaction = IA_Faction.NONE;
    }

    array<SCR_ChimeraCharacter> GetGroupCharacters()
    {
        //Print("[DEBUG] GetGroupCharacters called.", LogLevel.NORMAL);
        array<SCR_ChimeraCharacter> chars = {};
        if (!m_group || !m_isSpawned)
        {
            //Print("[DEBUG] Group not spawned or m_group is null.", LogLevel.NORMAL);
            return chars;
        }

        array<AIAgent> agents = {};
        m_group.GetAgents(agents);
        //Print("[DEBUG] Found " + agents.Count() + " agents in the group.", LogLevel.NORMAL);
        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                //Print("[IA_AiGroup] Cast to SCR_ChimeraCharacter failed!", LogLevel.WARNING);
                continue;
            }
            //Print("[DEBUG] Successfully cast agent to SCR_ChimeraCharacter.", LogLevel.NORMAL);
            chars.Insert(ch);
        }
        return chars;
    }

    // Add a pre-existing waypoint to the group
    void AddWaypoint(SCR_AIWaypoint waypoint)
    {
        if (!m_group || !waypoint)
        {
            //Print("[IA_AiGroup.AddWaypoint] Group or waypoint is null.", LogLevel.WARNING);
            return;
        }
        m_group.AddWaypoint(waypoint);
        //Print("[DEBUG] IA_AiGroup.AddWaypoint: Waypoint added to internal SCR_AIGroup.", LogLevel.NORMAL);
    }

    void AddOrder(vector origin, IA_AiOrder order, bool topPriority = false)
    {
        //Print("[DEBUG] AddOrder called with origin: " + origin + ", order: " + order + ", topPriority: " + topPriority, LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            //Print("[IA_AiGroup.AddOrder] Group not spawned!", LogLevel.WARNING);
            return;
        }
        if (!m_group)
        {
            //Print("[DEBUG] m_group is null in AddOrder.", LogLevel.NORMAL);
            return;
        }

        // Adjust spawn height based on terrain
        float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
        //Print("[DEBUG] Surface Y calculated: " + y, LogLevel.NORMAL);
        origin[1] = y + 0.5;

        ResourceName rname = IA_AiOrderResource(order);
        Resource res = Resource.Load(rname);
        if (!res)
        {
            //Print("[IA_AiGroup.AddOrder] Missing resource: " + rname, LogLevel.ERROR);
            return;
        }
        //Print("[DEBUG] Resource loaded successfully: " + rname, LogLevel.NORMAL);

        IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
        //Print("[DEBUG] Spawned waypoint entity.", LogLevel.NORMAL);
        SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
        if (!w)
        {
            //Print("[IA_AiGroup.AddOrder] Could not cast to AIWaypoint: " + rname, LogLevel.ERROR);
            return;
        }
        //Print("[DEBUG] Successfully cast entity to SCR_AIWaypoint.", LogLevel.NORMAL);

        if (m_isDriving || topPriority)
        {
            w.SetPriorityLevel(2000);
            //Print("[DEBUG] Set waypoint priority level to 2000.", LogLevel.NORMAL);
        }

        m_group.AddWaypoint(w);
        //Print("[DEBUG] Waypoint added to group.", LogLevel.NORMAL);
        m_lastOrderPosition = origin;
        m_lastOrderTime     = System.GetUnixTime();
        //Print("[DEBUG] Order timestamp updated to " + m_lastOrderTime, LogLevel.NORMAL);
    }

    void SetInitialPosition(vector pos)
    {
        //Print("[DEBUG] SetInitialPosition called with pos: " + pos, LogLevel.NORMAL);
        m_initialPosition = pos;
    }

    int TimeSinceLastOrder()
    {
        int delta = System.GetUnixTime() - m_lastOrderTime;
        //Print("[DEBUG] TimeSinceLastOrder called. Delta: " + delta, LogLevel.NORMAL);
        return delta;
    }

    bool HasOrders()
    {
        if (!m_group)
        {
            //Print("[DEBUG] HasOrders called but m_group is null.", LogLevel.NORMAL);
            return false;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        //Print("[DEBUG] HasOrders found " + wps.Count() + " waypoints.", LogLevel.NORMAL);
        return !wps.IsEmpty();
    }

    bool IsDriving()
    {
        //Print("[DEBUG] IsDriving called. Returning: " + m_isDriving, LogLevel.NORMAL);
        return m_isDriving;
    }

    void RemoveAllOrders(bool resetLastOrderTime = false)
    {
        //Print("[DEBUG] RemoveAllOrders called. resetLastOrderTime: " + resetLastOrderTime, LogLevel.NORMAL);
        if (!m_group)
        {
            //Print("[DEBUG] m_group is null in RemoveAllOrders.", LogLevel.NORMAL);
            return;
        }
        array<AIWaypoint> wps = {};
        m_group.GetWaypoints(wps);
        //Print("[DEBUG] Removing " + wps.Count() + " waypoints.", LogLevel.NORMAL);
        foreach (AIWaypoint w : wps)
        {
            m_group.RemoveWaypoint(w);
        }
        m_isDriving = false;
        if (resetLastOrderTime)
        {
            m_lastOrderTime = 0;
            //Print("[DEBUG] Last order time reset.", LogLevel.NORMAL);
        }
    }

    int GetAliveCount()
    {
        //Print("[DEBUG] GetAliveCount called.", LogLevel.NORMAL);
        if (!m_isSpawned)
        {
            if (m_isCivilian)
            {
                //Print("[DEBUG] Group is civilian and not spawned. Returning count 1.", LogLevel.NORMAL);
                return 1;
            }
            else
            {
                int count = IA_SquadCount(m_squadType, m_faction);
                //Print("[DEBUG] Group is military and not spawned. Returning squad count: " + count, LogLevel.NORMAL);
                return count;
            }
        }
        if (!m_group)
        {	
            //Print("[DEBUG] m_group is null in GetAliveCount. Returning 0.", LogLevel.NORMAL);
            return 0;
        }
        int aliveCount = m_group.GetPlayerAndAgentCount();
        //Print("[DEBUG] Alive count from group: " + aliveCount, LogLevel.NORMAL);
        return aliveCount;
    }

    vector GetOrigin()
    {
        if (!m_group)
        {
            //Print("[DEBUG] GetOrigin called but m_group is null. Returning vector.Zero.", LogLevel.NORMAL);
            return vector.Zero;
        }
        vector origin = m_group.GetOrigin();
        //Print("[DEBUG] GetOrigin returning: " + origin, LogLevel.NORMAL);
        return origin;
    }

    bool IsSpawned()
    {
		if (GetAliveCount() == 0){
			m_isSpawned = false;
		}
        //Print("[DEBUG] IsSpawned called. Returning: " + m_isSpawned, LogLevel.NORMAL);
        return m_isSpawned;
    }

void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
{
    //Print("[DEBUG] Spawn called with initialOrder: " + initialOrder + ", orderPos: " + orderPos, LogLevel.NORMAL);
    if (!PerformSpawn())
    {
        //Print("[DEBUG] PerformSpawn returned false. Aborting Spawn.", LogLevel.NORMAL);
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
        //Print("[DEBUG] PerformSpawn called.", LogLevel.NORMAL);
        if (IsSpawned())
        {
            //Print("[DEBUG] Already spawned. Returning false.", LogLevel.NORMAL);
            return false;
        }
        m_isSpawned = true;

        string resourceName;
        if (m_isCivilian)
        {
            resourceName = IA_RandomCivilianResourceName();
            //Print("[DEBUG] Spawning civilian. Resource: " + resourceName, LogLevel.NORMAL);
        }
        else
        {
            resourceName = IA_SquadResourceName(m_squadType, m_faction);
            //Print("[DEBUG] Spawning military. Resource: " + resourceName, LogLevel.NORMAL);
        }

        Resource res = Resource.Load(resourceName);
        if (!res)
        {
            //Print("[IA_AiGroup.PerformSpawn] No resource: " + resourceName, LogLevel.ERROR);
            return false;
        }

        IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
        //Print("[DEBUG] Spawned main entity for group.", LogLevel.NORMAL);
        if (m_isCivilian)
        {
            // For civilians, spawn an extra group prefab and add the character
            Resource groupRes = Resource.Load("{71783D1DEDC4E150}Prefabs/Groups/Group_CIV.et");
            IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
            m_group = SCR_AIGroup.Cast(groupEnt);
            //Print("[DEBUG] Spawned civilian group entity.", LogLevel.NORMAL);

            if (!m_group.AddAIEntityToGroup(entity))
            {
                //Print("[IA_AiGroup.PerformSpawn] Could not add civ to group: " + resourceName, LogLevel.ERROR);
            }
            else
            {
                //Print("[DEBUG] Civilian added to group successfully.", LogLevel.NORMAL);
            }
        }
        else
        {
            m_group = SCR_AIGroup.Cast(entity);
            //Print("[DEBUG] Cast main entity to SCR_AIGroup.", LogLevel.NORMAL);
        }
	if (m_group)
	{
	    vector groundPos = m_initialPosition;
	    float groundY = GetGame().GetWorld().GetSurfaceY(groundPos[0], groundPos[2]);
	    groundPos[1] = groundY;
	    

	    
	    // Also update the origin if that function exists.
	    m_group.SetOrigin(groundPos);
	    
	    //Print("[DEBUG] Group position forced to ground level: " + groundPos, LogLevel.NORMAL);
	}

        if (!m_group)
        {
            //Print("[IA_AiGroup.PerformSpawn] Could not cast to AIGroup", LogLevel.ERROR);
            return false;
        }

        SetupDeathListener();
        WaterCheck();
        //Print("[DEBUG] PerformSpawn completed successfully.", LogLevel.NORMAL);
        return true;
    }

    void Despawn()
    {
        //Print("[DEBUG] Despawn called.", LogLevel.NORMAL);
        if (!IsSpawned())
        {
            //Print("[DEBUG] Group is not spawned. Nothing to despawn.", LogLevel.NORMAL);
            return;
        }
        m_isSpawned = false;
        m_isDriving = false;

        if (m_group)
        {
            //Print("[DEBUG] Deactivating all members of the group.", LogLevel.NORMAL);
            m_group.DeactivateAllMembers();
        }
        RemoveAllOrders();
        IA_Game.AddEntityToGc(m_group);
        m_group = null;
        //Print("[DEBUG] Group despawned successfully.", LogLevel.NORMAL);
    }

    IA_Faction GetFaction()
    {
        //Print("[DEBUG] GetFaction called. Returning: " + m_faction, LogLevel.NORMAL);
        return m_faction;
    }

    private void SetupDeathListener()
    {
        //Print("[DEBUG] SetupDeathListener called.", LogLevel.NORMAL);
        if (!m_group)
        {
            //Print("[DEBUG] m_group is null in SetupDeathListener.", LogLevel.NORMAL);
            return;
        }
        array<AIAgent> agents = {};
        m_group.GetAgents(agents);

        if (agents.IsEmpty())
        {
            //Print("[DEBUG] No agents found. Retrying SetupDeathListener in 1000 ms.", LogLevel.NORMAL);
            GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
            return;
        }

        foreach (AIAgent agent : agents)
        {
            SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
            if (!ch)
            {
                //Print("[DEBUG] Agent does not have a valid SCR_ChimeraCharacter.", LogLevel.NORMAL);
                continue;
            }
            SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
            if (!ccc)
            {
                //Print("[IA_AiGroup.SetupDeathListener] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
                continue;
            }
            //Print("[DEBUG] Inserting death callback for agent.", LogLevel.NORMAL);
            ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
        }
    }

    private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {
        //Print("[DEBUG] OnMemberDeath called.", LogLevel.NORMAL);
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
            //Print("[DEBUG] Member killed by player.", LogLevel.NORMAL);
            // If a player kills one of our guys and we're not yet "engaged," 
            // set some placeholder for engaged faction (CIV is used as a placeholder).
            if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
            {
                //Print("[IA_AiGroup.OnMemberDeath] Player killed enemy, group engaged with CIV as placeholder");
                m_engagedEnemyFaction = IA_Faction.CIV; 
            }
        }
        else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
        {
            //Print("[DEBUG] Member killed by AI.", LogLevel.NORMAL);
            SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
            if (!c)
            {
                //Print("[IA_AiGroup.OnMemberDeath] cast killerEntity to character failed!", LogLevel.ERROR);
            }
            else
            {
                string fKey = c.GetFaction().GetFactionKey();
                //Print("[DEBUG] Killer faction key: " + fKey, LogLevel.NORMAL);
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
                    //Print("[IA_AiGroup.OnMemberDeath] unknown faction key " + fKey, LogLevel.WARNING);
                }
                // If friendly fire, reset engagement.
                if (m_engagedEnemyFaction == m_faction)
                {
                    //Print("[IA_AiGroup.OnMemberDeath] Friendly kill => resetting engagement");
                    m_engagedEnemyFaction = IA_Faction.NONE;
                }
            }
        }
    }

    private void WaterCheck()
    {
        //Print("[DEBUG] WaterCheck called (placeholder).", LogLevel.NORMAL);
        // No extra water check here because safe areas are pre-defined.
    }

    void AssignVehicle(Vehicle vehicle, vector destination)
    {
        if (!vehicle || !IsSpawned())
            return;
            
        // Try to reserve the vehicle - if we can't, don't proceed
        if (!IA_VehicleManager.ReserveVehicle(vehicle, this))
            return;
            
        m_referencedEntity = vehicle;
        m_isDriving = true;
        m_drivingTarget = destination;
        m_lastOrderPosition = destination;
        m_lastOrderTime = System.GetUnixTime();
        
        // First order - get to the vehicle
        AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle);
    }
    
    void UpdateVehicleOrders()
    {
        if (!m_isDriving || !m_referencedEntity)
            return;
            
        Vehicle vehicle = Vehicle.Cast(m_referencedEntity);
        if (!vehicle)
        {
            m_isDriving = false;
            m_referencedEntity = null;
            IA_VehicleManager.ReleaseVehicleReservation(vehicle);
            return;
        }
        
        array<SCR_ChimeraCharacter> chars = GetGroupCharacters();
        if (chars.IsEmpty())
            return;
            
        SCR_ChimeraCharacter driver = chars[0];
        Vehicle currentVehicle = IA_VehicleManager.GetCharacterVehicle(driver);
        
        if (currentVehicle)
        {
            if (currentVehicle != vehicle) // Wrong vehicle
            {
                AddOrder(m_drivingTarget, IA_AiOrder.Move);
            }
            else if (vector.Distance(vehicle.GetOrigin(), m_drivingTarget) > 5)
            {
                AddOrder(m_drivingTarget, IA_AiOrder.Move);
            }
            else
            {
                AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetOutOfVehicle);
                m_isDriving = false;
                m_referencedEntity = null;
                IA_VehicleManager.ReleaseVehicleReservation(vehicle);
            }
        }
        else
        {
            AddOrder(vehicle.GetOrigin(), IA_AiOrder.GetInVehicle);
        }
    }
};