
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
    //Print("[DEBUG] IA_AiOrderResource called with order: " + order, LogLevel.NORMAL);
    if (order == IA_AiOrder.Defend)
    {
        //Print("[DEBUG] Returning resource for Defend", LogLevel.NORMAL);
        return "{0AB63F524C44E0D2}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_DefendSmall.et";
    }
    else if (order == IA_AiOrder.Patrol)
    {
        //Print("[DEBUG] Returning resource for Patrol", LogLevel.NORMAL);
        return "{C0A9A9B589802A5B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Patrol.et";
    }
    else if (order == IA_AiOrder.SearchAndDestroy)
    {
        //Print("[DEBUG] Returning resource for SearchAndDestroy", LogLevel.NORMAL);
        return "{EE9A99488B40628B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_SearchAndDestroy.et";
    }
    else if (order == IA_AiOrder.GetInVehicle)
    {
        //Print("[DEBUG] Returning resource for GetInVehicle", LogLevel.NORMAL);
        return "{0A2A37B4A56D74DF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetInNearest.et";
    }
    else if (order == IA_AiOrder.GetOutOfVehicle)
    {
        //Print("[DEBUG] Returning resource for GetOutOfVehicle", LogLevel.NORMAL);
        return "{2602CAB8AB74FBBF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetOut.et";
    }
    //Print("[DEBUG] Returning default resource for Move", LogLevel.NORMAL);
    return "{FFF9518F73279473}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Move.et";
}


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
// 7) REINFORCEMENT STATE
///////////////////////////////////////////////////////////////////////
enum IA_ReinforcementState
{
    NotDone,
    Countdown,
    Done
};


///////////////////////////////////////////////////////////////////////
// 11) HELPER FACTION / SQUAD UTILS
///////////////////////////////////////////////////////////////////////
string IA_SquadResourceName_US(IA_SquadType st)
{
    //Print("[DEBUG] IA_SquadResourceName_US called with squad type: " + st, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_SquadResourceName_USSR called with squad type: " + st, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_RandomCivilianResourceName chosen index: " + idx, LogLevel.NORMAL);
    return s_IA_CivList[idx];
}

string IA_SquadResourceName(IA_SquadType st, IA_Faction fac)
{
    //Print("[DEBUG] IA_SquadResourceName called with squad type: " + st + ", faction: " + fac, LogLevel.NORMAL);
    if (fac == IA_Faction.US)
        return IA_SquadResourceName_US(st);
    else if (fac == IA_Faction.USSR)
        return IA_SquadResourceName_USSR(st);
    return IA_SquadResourceName_US(st);
}

int IA_SquadCount(IA_SquadType st, IA_Faction fac)
{
    //Print("[DEBUG] IA_SquadCount called with squad type: " + st + ", faction: " + fac, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_GetRandomSquadType generated random number: " + r, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_FactionToInt called with faction: " + f, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_FactionFromInt called with int: " + i, LogLevel.NORMAL);
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
    //Print("[DEBUG] IA_CreateSimpleSpawnParams called with origin: " + origin, LogLevel.NORMAL);
    EntitySpawnParams p = new EntitySpawnParams();
    p.TransformMode = ETransformMode.WORLD;
    p.Transform[3]  = origin;
    return p;
}

EntitySpawnParams IA_CreateSurfaceAdjustedSpawnParams(vector origin)
{
    //Print("[DEBUG] IA_CreateSurfaceAdjustedSpawnParams called with origin: " + origin, LogLevel.NORMAL);
    float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
    origin[1] = y;
    EntitySpawnParams p = new EntitySpawnParams();
    p.TransformMode = ETransformMode.WORLD;
    p.Transform[3]  = origin;
    return p;
}




// ----------------------------------------------------------------------------------------------
//  Basic dictionary: string -> int
// ----------------------------------------------------------------------------------------------
const int IA_DICT_STRING_INT_MAX = 32;
class IA_DictStringInt
{
	private string  m_Keys[IA_DICT_STRING_INT_MAX];
	private int     m_Values[IA_DICT_STRING_INT_MAX];
	private int     m_Count;

	void IA_DictStringInt()
	{
		m_Count = 0;
		for (int i = 0; i < IA_DICT_STRING_INT_MAX; i++)
		{
			m_Keys[i]   = "";
			m_Values[i] = 0;
		}
	}

	// Return true if key exists
	bool Contains(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return true;
		return false;
	}

	// Return value for key (or 0 if not found)
	int Get(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return m_Values[i];
		return 0;
	}

	// Insert or update key with given value
	void Set(string key, int value)
	{
		// If key already exists, update
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				m_Values[i] = value;
				return;
			}
		}

		// Otherwise add a new entry if there's space
		if (m_Count < IA_DICT_STRING_INT_MAX)
		{
			m_Keys[m_Count]   = key;
			m_Values[m_Count] = value;
			m_Count++;
		}
	}

	// Remove entry by key
	void Remove(string key)
	{
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				// shift everything down
				for (int j = i; j < m_Count - 1; j++)
				{
					m_Keys[j]   = m_Keys[j + 1];
					m_Values[j] = m_Values[j + 1];
				}
				// Clear the last
				m_Keys[m_Count - 1]   = "";
				m_Values[m_Count - 1] = 0;
				m_Count--;
				return;
			}
		}
	}

	// Clear entire dictionary
	void Clear()
	{
		for (int i = 0; i < m_Count; i++)
		{
			m_Keys[i]   = "";
			m_Values[i] = 0;
		}
		m_Count = 0;
	}

	// Get number of valid key-value pairs
	int GetCount()
	{
		return m_Count;
	}

	// Access the i-th entry (for loops)
	void GetPair(int index, out string key, out int value)
	{
		if (index < 0 || index >= m_Count)
		{
			key   = "";
			value = 0;
			return;
		}
		key   = m_Keys[index];
		value = m_Values[index];
	}
}

// ----------------------------------------------------------------------------------------------
//  For storing (RplId hash -> string). 
//  We cannot store SCR_ChimeraCharacter references without 'ref', so we store an int hash.
// ----------------------------------------------------------------------------------------------
const int IA_DICT_INT_STRING_MAX = 64;
class IA_DictIntString
{
	private int     m_Keys[IA_DICT_INT_STRING_MAX];
	private string  m_Values[IA_DICT_INT_STRING_MAX];
	private int     m_Count;

	void IA_DictIntString()
	{
		m_Count = 0;
		for (int i = 0; i < IA_DICT_INT_STRING_MAX; i++)
		{
			m_Keys[i]   = 0;
			m_Values[i] = "";
		}
	}

	bool Contains(int key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return true;
		return false;
	}

	string Get(int key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return m_Values[i];
		return "";
	}

	void Set(int key, string value)
	{
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				m_Values[i] = value;
				return;
			}
		}
		// Insert new if there's space
		if (m_Count < IA_DICT_INT_STRING_MAX)
		{
			m_Keys[m_Count]   = key;
			m_Values[m_Count] = value;
			m_Count++;
		}
	}

	void Remove(int key)
	{
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				for (int j = i; j < m_Count - 1; j++)
				{
					m_Keys[j]   = m_Keys[j + 1];
					m_Values[j] = m_Values[j + 1];
				}
				m_Keys[m_Count - 1]   = 0;
				m_Values[m_Count - 1] = "";
				m_Count--;
				return;
			}
		}
	}

	void Clear()
	{
		for (int i = 0; i < m_Count; i++)
		{
			m_Keys[i]   = 0;
			m_Values[i] = "";
		}
		m_Count = 0;
	}
}

// ----------------------------------------------------------------------------------------------
//  For storing (string -> float)
// ----------------------------------------------------------------------------------------------
const int IA_DICT_STRING_FLOAT_MAX = 32;
class IA_DictStringFloat
{
	private string  m_Keys[IA_DICT_STRING_FLOAT_MAX];
	private float   m_Values[IA_DICT_STRING_FLOAT_MAX];
	private int     m_Count;

	void IA_DictStringFloat()
	{
		m_Count = 0;
		for (int i = 0; i < IA_DICT_STRING_FLOAT_MAX; i++)
		{
			m_Keys[i]   = "";
			m_Values[i] = 0.0;
		}
	}

	bool Contains(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return true;
		return false;
	}

	float Get(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return m_Values[i];
		return 0.0;
	}

	void Set(string key, float value)
	{
		// If key already exists, update
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				m_Values[i] = value;
				return;
			}
		}
		// Otherwise add new if there's space
		if (m_Count < IA_DICT_STRING_FLOAT_MAX)
		{
			m_Keys[m_Count]   = key;
			m_Values[m_Count] = value;
			m_Count++;
		}
	}

	void Remove(string key)
	{
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				for (int j = i; j < m_Count - 1; j++)
				{
					m_Keys[j]   = m_Keys[j + 1];
					m_Values[j] = m_Values[j + 1];
				}
				m_Keys[m_Count - 1]   = "";
				m_Values[m_Count - 1] = 0.0;
				m_Count--;
				return;
			}
		}
	}

	void Clear()
	{
		for (int i = 0; i < m_Count; i++)
		{
			m_Keys[i]   = "";
			m_Values[i] = 0.0;
		}
		m_Count = 0;
	}

	int GetCount()
	{
		return m_Count;
	}

	void GetPair(int index, out string key, out float value)
	{
		if (index < 0 || index >= m_Count)
		{
			key   = "";
			value = 0.0;
			return;
		}
		key   = m_Keys[index];
		value = m_Values[index];
	}
}



