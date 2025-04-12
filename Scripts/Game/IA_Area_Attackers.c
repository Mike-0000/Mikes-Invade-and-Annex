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
    private IA_AiOrder m_currentOrder = IA_AiOrder.Patrol;

    void IA_AreaAttackers(IA_Faction fac, IA_Area area)
    {
        //Print("[DEBUG] IA_AreaAttackers constructor called with faction: " + fac + " and area: " + area.GetName(), LogLevel.NORMAL);
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
        //Print("[DEBUG] SpawnNextGroup called with origin: " + origin, LogLevel.NORMAL);
        if (m_groups.Count() == m_groupCount)
            return;

        IA_SquadType st = IA_GetRandomSquadType();
        //Print("[DEBUG] Random squad type chosen: " + st, LogLevel.NORMAL);
        IA_AiGroup grp  = IA_AiGroup.CreateMilitaryGroup(origin, st, m_faction);
        m_groups.Insert(grp);
        //Print("[DEBUG] New military group created and inserted.", LogLevel.NORMAL);
        grp.Spawn();

        if (m_groups.Count() == m_groupCount)
        {
            m_initializing = false;
            //Print("[DEBUG] All groups spawned. Initializing complete.", LogLevel.NORMAL);
        }
    }

    void ProcessNextGroup()
    {
        // Check if the zone is under attack
        array<IA_AreaMarker> markers = IA_AreaMarker.GetAreaMarkersForArea(m_area.GetName());
        bool zoneUnderAttack = false;
        
        // Check if any marker for this area is under attack
        foreach (IA_AreaMarker marker : markers)
        {
            if (marker && marker.IsZoneUnderAttack())
            {
                zoneUnderAttack = true;
                break;
            }
        }
        
        // If the zone is under attack, switch to Search and Destroy mode
        if (zoneUnderAttack)
        {
            m_currentOrder = IA_AiOrder.SearchAndDestroy;
        }
        
        // Process the next group based on the current order
        if (m_groupToProcess >= m_groups.Count())
        {
            m_groupToProcess = 0;
        }
        if (m_groups.IsEmpty())
            return;

        IA_AiGroup g = m_groups[m_groupToProcess];
        if (g.GetAliveCount() == 0)
        {
            m_groups.Remove(m_groupToProcess);
            m_groupToProcess++; // stay on same index
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
                pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
                m_initialOrderGiven = true;
            }
            
            // Use the appropriate order based on whether the zone is under attack
            g.AddOrder(pos, m_currentOrder);
        }
        m_groupToProcess = m_groupToProcess + 1;
    }

    bool IsAllGroupsDead()
    {
        if (m_initializing)
            return false;
        if (m_groups.IsEmpty()){
            //Print("AllGroupsDead GOING TO TRUE!!!",LogLevel.ERROR);
            return true;
        }
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