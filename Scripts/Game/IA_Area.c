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
        //Print("[DEBUG] IA_Area constructor called with name: " + nm + ", type: " + t + ", origin: " + org + ", radius: " + rad, LogLevel.NORMAL);
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

    int GetReinforcementGroupQuota()
    {
        // Define total reinforcement groups per area type
        switch (m_type)
        {
            case IA_AreaType.City:       return 8;
            case IA_AreaType.Town:       return 6;
            case IA_AreaType.Military:   return 6;
            case IA_AreaType.Airport:    return 5;
            case IA_AreaType.Docks:      return 4;
            case IA_AreaType.Property:   return 3;
            default:                     return 4; // Default fallback
        }
        return 0; // Should not be reached
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
