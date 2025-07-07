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
    Assassination,
    Military,
    SmallMilitary,
    RadioTower,
    DefendObjective
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

    void IA_Area(string nm, IA_AreaType t, vector org, float rad)
    {
        Print("[IA_Area] Constructor called for area: " + nm, LogLevel.DEBUG);
        m_name   = nm;
        m_type   = t;
        m_origin = org;
        m_radius = rad;
    }

static IA_Area Create(string nm, IA_AreaType t, vector org, float rad)
{
    //Print("[DEBUG] IA_Area.Create called for area: " + nm, LogLevel.NORMAL);
    IA_Area area = new IA_Area(nm, t, org, rad);
    IA_Game.s_allAreas.Insert(area); // <-- Keeps strong ref
    return area;
}

static IA_Area CreateTransient(string nm, IA_AreaType t, vector org, float rad)
{
    // This version does NOT add the area to the global list.
    // It's for temporary, self-contained areas like side objectives.
    IA_Area area = new IA_Area(nm, t, org, rad);
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
            return 3;
        else if (m_type == IA_AreaType.Airport)
            return 6;
        else if (m_type == IA_AreaType.Military)
            return 5;
        else if (m_type == IA_AreaType.SmallMilitary)
            return 3;
        else if (m_type == IA_AreaType.Assassination)
            return 6;
        else if (m_type == IA_AreaType.Property)
            return 1;
        else if (m_type == IA_AreaType.RadioTower)
            return 3; // Medium security for radio tower
        return 0;
    }

    int GetReinforcementGroupQuota()
    {
        // Define total reinforcement groups per area type
        switch (m_type)
        {
            case IA_AreaType.City:       return 6;
            case IA_AreaType.Town:       return 4;
            case IA_AreaType.Military:   return 4;
            case IA_AreaType.SmallMilitary: return 2;
            case IA_AreaType.Airport:    return 4;
            case IA_AreaType.Assassination:    return 4;
            case IA_AreaType.Docks:      return 3;
            case IA_AreaType.Property:   return 1;
            case IA_AreaType.RadioTower: return 2; // Moderate reinforcements for radio tower
            default:                     return 4; // Default fallback
        }
        return 0; // Should not be reached
    }

    int GetCivilianCount()
    {
        if (m_type == IA_AreaType.Town)
            return 9;
        else if (m_type == IA_AreaType.City)
            return 14;
        else if (m_type == IA_AreaType.Docks)
            return 3;
        else if (m_type == IA_AreaType.SmallMilitary)
            return 1;
        else if (m_type == IA_AreaType.Assassination)
            return 2;
        else if (m_type == IA_AreaType.Property)
            return 3;
        else if (m_type == IA_AreaType.RadioTower)
            return 0; // No civilians at radio tower
        return 1;
    }

    bool IsPositionInside(vector pos)
    {
        float distanceSq = vector.DistanceSq(m_origin, pos);
        float radiusSq = m_radius * m_radius;
        return distanceSq <= radiusSq;
    }
};
