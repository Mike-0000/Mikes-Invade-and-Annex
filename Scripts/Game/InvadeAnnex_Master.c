// ====================================================================================
// File: IA_InvadeAnnexZoneEntity_SpawnTest.c
// Now integrates random logic from the example code. 
// We replicate identical resource arrays (US, USSR, Civ) 
// with minimal changes, no "TAB_" naming, and no "tag_" calls.
// ====================================================================================

class IA_InvadeAnnexZoneEntityClass : ScriptedGameTriggerEntityClass {}

// Our own local enumerations to replicate the example mod’s approach
enum IA_Faction
{
	NONE = 0,	// If you need an “unused” slot
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

class IA_InvadeAnnexZoneEntity : ScriptedGameTriggerEntity
{
	//----------------------------------------------------------------
	// These arrays replicate the second mod's resource references 
	// so we can do "identical" random US/USSR squads, plus civilians.
	//----------------------------------------------------------------

	// For a more “complete” approach, you might also add 
	// “FIA” if you want that from the second mod, or skip it.
	
	//------------------------------------------------------
	// US Squad Prefabs
	//------------------------------------------------------
	protected static string IA_SquadResourceName_US(IA_SquadType squadType)
	{
		switch (squadType)
		{
			case IA_SquadType.Riflemen:
				return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
			case IA_SquadType.Firesquad:
				return "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
			case IA_SquadType.Medic:
				return "{EF62027CC75A7459}Prefabs/Groups/BLUFOR/Group_US_MedicalSection.et";
			case IA_SquadType.Antitank:
				return "{FAEA8B9E1252F56E}Prefabs/Groups/BLUFOR/Group_US_Team_LAT.et";
		}
		// fallback
		return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
	}

	//------------------------------------------------------
	// USSR Squad Prefabs
	//------------------------------------------------------
	protected static string IA_SquadResourceName_USSR(IA_SquadType squadType)
	{
		switch (squadType)
		{
			case IA_SquadType.Riflemen:
				return "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
			case IA_SquadType.Firesquad:
				return "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
			case IA_SquadType.Medic:
				return "{D815658156080328}Prefabs/Groups/OPFOR/Group_USSR_MedicalSection.et";
			case IA_SquadType.Antitank:
				return "{96BAB56E6558788E}Prefabs/Groups/OPFOR/Group_USSR_Team_AT.et";
		}
		// fallback
		return "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
	}

	//------------------------------------------------------
	// Civilians - identical large array from the example mod
	//------------------------------------------------------
	protected static ref array<string> s_CivilianResourceList = {
		// Generic civilians
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

		// Construction workers
		"{6F5A71376479B353}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_1.et",
		"{C97B985FCFC3E4D6}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_2.et",
		"{2CE7D4A6C32673BD}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_3.et",
		"{11D3F19EB64AFA8A}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_4.et",
		"{F44FBD67BAAF6DE1}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_5.et",

		// Businessmen
		"{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et",
		"{A517C72CEF150898}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_2.et",
		"{626874113CAE4387}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_3.et"
	};


	//----------------------------------------------------------------
	// We'll replicate random getters from the second mod – but renamed 
	// "IA_" and minimal references, so we do not rely on the "TAB_" code.
	//----------------------------------------------------------------

	// Return random faction from US or USSR only. 
	// If you want Civ as well, you can handle that with the 
	// "m_bSpawnCiv" check or expand logic as needed.
	protected int IA_RandomInt(int low, int high)
	{
		RandomGenerator rng = new RandomGenerator();
		return rng.RandInt(low, high);
	}
	protected IA_Faction IA_GetRandomFaction()
	{
		// 50% US, 50% USSR
		int n = IA_RandomInt(0, 1); // 0 or 1
		if (n == 0) return IA_Faction.US;
		return IA_Faction.USSR;
	}
	
	// Return random squad type – same approach as the second mod 
	protected IA_SquadType IA_GetRandomSquadType()
	{
		int r = IA_RandomInt(0, 3); // 0..3
		switch (r)
		{
			case 0:  return IA_SquadType.Riflemen;
			case 1:  return IA_SquadType.Firesquad;
			case 2:  return IA_SquadType.Medic;
			case 3:  return IA_SquadType.Antitank;
		}
		// fallback
		return IA_SquadType.Riflemen;
	}

	// Return a random resource name for a given squad & faction 
	protected string IA_GetSquadResource(IA_SquadType squadType, IA_Faction faction)
	{
		switch (faction)
		{
			case IA_Faction.US:
				return IA_SquadResourceName_US(squadType);
			case IA_Faction.USSR:
				return IA_SquadResourceName_USSR(squadType);
			default:
				// fallback if we needed it
				return IA_SquadResourceName_US(squadType);
		}
		return IA_SquadResourceName_US(squadType);
	}

	// Return random civilian from the big array 
	protected string IA_GetRandomCivilianResource()
	{
		int idx = IA_RandomInt(0, s_CivilianResourceList.Count() - 1);
		return s_CivilianResourceList[idx];
	}


	// ---------------------------------------------------------------------------------
	// The rest is basically your original code, but with the random picks replaced 
	// by the calls above. 
	// ---------------------------------------------------------------------------------

	protected ref IA_DictStringInt  m_FactionCounts;
	protected ref IA_DictIntString  m_CharacterFactionMapping;

	// Frame counters, etc.
	protected int   m_iFrameCount      = 0;
	protected float m_fTimeAccumulator = 0.0;

	// How many groups to spawn each cycle
	protected const int GROUPS_PER_SPAWN_CYCLE = 2;

	[Attribute(defvalue: "10.0", UIWidgets.EditBox, "Zone radius for OnActivate and sphere queries", category: "Zone", params: "0.1 99999")]
	protected float m_fZoneRadius;

	// If true, spawns random civilians. If false, spawns random US/USSR squads.
	[Attribute(defvalue: "0", desc: "Spawn civilian AI? (Otherwise spawn military squads)")]
	protected bool m_bSpawnCiv;

	// Optional default attributes if you want fixed faction or squad
	[Attribute(defvalue: "Riflemen", desc: "Default squad type name (for resource lookups if not randomized)")]
	protected string m_sSquadTypeName;

	[Attribute(defvalue: "USSR", desc: "Default faction name (for resource lookups if not randomized)")]
	protected string m_sFactionName;

	protected RplComponent m_RplComponent;

	// ----------------------------------------------------------------
	// Constructor
	void IA_InvadeAnnexZoneEntity(IEntitySource src, IEntity parent)
	{
		m_FactionCounts           = new IA_DictStringInt();
		m_CharacterFactionMapping = new IA_DictIntString();

		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	// ----------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
	}

	// ----------------------------------------------------------------
	// When an entity enters the trigger
	override protected void OnActivate(IEntity ent)
	{
		if (IsProxy()) return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		if (!character) return;

		string factionKey = GetFactionOfCharacter(character);
		int    charHash   = GetCharacterHash(character);

		m_CharacterFactionMapping.Set(charHash, factionKey);

		if (m_FactionCounts.Contains(factionKey))
		{
			int oldCount = m_FactionCounts.Get(factionKey);
			m_FactionCounts.Set(factionKey, oldCount + 1);
		}
		else
		{
			m_FactionCounts.Set(factionKey, 1);
		}

		Print("[TEST] Character ENTER zone. faction=" + factionKey 
		      + " newCount=" + m_FactionCounts.Get(factionKey));
	}

	// ----------------------------------------------------------------
	// When an entity leaves the trigger
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

			Print("[TEST] Character LEFT zone. faction=" + factionKey 
			      + " newCount=" + GetFactionCount(factionKey));
		}
	}

	// ----------------------------------------------------------------
	// EOnFrame: every so often, query the area and spawn random AI groups.
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!owner)
		{
			Print("[ERROR] Owner is NULL", LogLevel.ERROR);
			return;
		}

		if (IsProxy())
			return;

		m_fTimeAccumulator += timeSlice;

		// Throttle: e.g. every 200 frames
		if (m_iFrameCount < 500)
		{
			m_iFrameCount++;
			return;
		}
		m_iFrameCount = 0;

		// Reset time accumulator (for debugging/measurement)
		float elapsedTime = m_fTimeAccumulator;
		m_fTimeAccumulator = 0.0;

		World world = owner.GetWorld();
		if (!world)
		{
			Print("[ERROR] World is NULL", LogLevel.ERROR);
			return;
		}

		// Perform a sphere query (for occupant info, if needed)
		array<IEntity> foundEntities = {};
		IA_QueryCallback cb = new IA_QueryCallback(foundEntities);

		vector center = GetZoneCenter();
		world.QueryEntitiesBySphere(center, m_fZoneRadius * 6.66, cb.OnEntityFound, null, EQueryEntitiesFlags.ALL);

		IA_DictStringInt localCounts = new IA_DictStringInt();
		foreach (IEntity ent : foundEntities)
		{
			if (!ent) continue;
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(ent);
			if (!ch) continue;

			string factionKey = GetFactionOfCharacter(ch);
			if (factionKey == "") continue;

			int oldCount = localCounts.Get(factionKey);
			localCounts.Set(factionKey, oldCount + 1);
		}

		// Overwrite occupant dictionary
		m_FactionCounts.Clear();
		int localTotal = localCounts.GetCount();
		for (int i = 0; i < localTotal; i++)
		{
			string fKey;
			int fVal;
			localCounts.GetPair(i, fKey, fVal);
			m_FactionCounts.Set(fKey, fVal);
		}

		// 2) Spawn a few random AI groups
		SpawnAiGroupsForDebug(center, m_fZoneRadius);

		Print("[TEST] Done EOnFrame sphere query. occupant factions=" + localTotal);
	}

	// ----------------------------------------------------------------
	// Spawns a few AI groups at random positions within the zone.
	protected void SpawnAiGroupsForDebug(vector zoneCenter, float zoneRadius)
	{
		for (int i = 0; i < GROUPS_PER_SPAWN_CYCLE; i++)
		{
			SpawnRandomAiGroup(zoneCenter, zoneRadius);
		}
	}

	// ----------------------------------------------------------------
	// This method chooses a random point in the zone and spawns an AI group there,
	// using the new "IA_" random logic from the second mod.
	protected void SpawnRandomAiGroup(vector zoneCenter, float zoneRadius)
	{
		vector spawnPos = IA_GetRandomPointInCircle(zoneCenter, zoneRadius);
		
		if (m_bSpawnCiv)
		{
			// Spawn a random civilian from the big array
			SpawnSingleAiGroupAt(spawnPos, true);
		}
		else
		{
			// Random US or USSR faction
			IA_Faction faction = IA_GetRandomFaction();
			// Random squad type
			IA_SquadType squadType = IA_GetRandomSquadType();
			
			Print(squadType, LogLevel.WARNING);
			SpawnSingleAiGroupAt(spawnPos, false, squadType, faction);
		}
	}

	// ----------------------------------------------------------------
	// Spawns a single AI group at a given position.
	// If "isCivilian" is true, a civilian resource is used.
	// For military groups, we use the new big resource arrays 
	// for squads exactly as in the second mod.
	protected bool SpawnSingleAiGroupAt(vector position, bool isCivilian, IA_SquadType st = IA_SquadType.Riflemen, IA_Faction fac = IA_Faction.US)
	{
		string resourceName;
		if (isCivilian)
		{
			resourceName = IA_GetRandomCivilianResource();
		}
		else
		{
			resourceName = IA_GetSquadResource(st, fac);
		}

		Resource resource = Resource.Load(resourceName);
		if (!resource)
		{
			Print("[IA_InvadeAnnexZone] No such resource: " + resourceName, LogLevel.ERROR);
			return false;
		}

		EntitySpawnParams mainParams = CreateSurfaceAdjustedSpawnParams(position);
		IEntity entity = GetGame().SpawnEntityPrefab(resource, null, mainParams);
		if (!entity)
		{
			Print("[IA_InvadeAnnexZone] Failed to spawn main entity from resource " + resourceName, LogLevel.ERROR);
			return false;
		}

		SCR_AIGroup spawnedGroup = null;
		if (isCivilian)
		{
			// For civilians, create a new AIGroup prefab 
			// and add the character to that group 
			// (same trick from your original).
			Resource groupRes = Resource.Load("{2D4A93612FA7C24D}Prefabs/Groups/TAB_Group_CIV.et"); // or any group you'd want for civ
			if (!groupRes)
			{
				Print("[IA_InvadeAnnexZone] Could not load AIGroup resource for civilians!", LogLevel.ERROR);
				return false;
			}
			EntitySpawnParams groupParams = CreateSimpleEntitySpawnParams(position);
			IEntity groupEntity = GetGame().SpawnEntityPrefab(groupRes, null, groupParams);
			spawnedGroup = SCR_AIGroup.Cast(groupEntity);

			if (!spawnedGroup)
			{
				Print("[IA_InvadeAnnexZone] Casting to SCR_AIGroup failed for civ group prefab!", LogLevel.ERROR);
				return false;
			}

			if (!spawnedGroup.AddAIEntityToGroup(entity))
			{
				Print("[IA_InvadeAnnexZone] Failed to add civilian AI entity to group " + resourceName, LogLevel.ERROR);
			}
		}
		else
		{
			// For US/USSR squads, the prefab itself is the group 
			spawnedGroup = SCR_AIGroup.Cast(entity);
		}

		if (!spawnedGroup)
		{
			Print("[IA_InvadeAnnexZone] Final AIGroup was null after spawn!", LogLevel.ERROR);
			return false;
		}

		// Optionally set up additional listeners, water checks, etc.
		SetupDeathListener(spawnedGroup);
		WaterCheck(spawnedGroup);

		Print("[IA_InvadeAnnexZone] Spawned group at " + position, LogLevel.NORMAL);
		return true;
	}

	// ----------------------------------------------------------------
	// Helper: Returns a random point inside a circle.
	protected vector IA_GetRandomPointInCircle(vector center, float radius)
	{
		float r = Math.RandomFloat01() * radius;
		float a = Math.RandomFloat01() * 360.0 * Math.DEG2RAD;
		float offsetX = r * Math.Cos(a);
		float offsetZ = r * Math.Sin(a);
		return Vector(center[0] + offsetX, center[1], center[2] + offsetZ);
	}

	// ----------------------------------------------------------------
	// Helper: Creates spawn parameters adjusted to the world surface.
	protected EntitySpawnParams CreateSurfaceAdjustedSpawnParams(vector pos)
	{
		World w = GetGame().GetWorld();
		float y = w.GetSurfaceY(pos[0], pos[2]);
		pos[1] = y;

		EntitySpawnParams p = EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		p.Transform[3] = pos;
		return p;
	}

	// ----------------------------------------------------------------
	// Helper: Creates basic spawn parameters.
	protected EntitySpawnParams CreateSimpleEntitySpawnParams(vector pos)
	{
		EntitySpawnParams p = EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		p.Transform[3] = pos;
		return p;
	}

	// ----------------------------------------------------------------
	// Get the center of this zone trigger.
	protected vector GetZoneCenter()
	{
		vector mat[4];
		GetTransform(mat);
		return mat[3];
	}

	// ----------------------------------------------------------------
	// Returns the faction key of a character.
	protected string GetFactionOfCharacter(SCR_ChimeraCharacter character)
	{
		if (!character) return "";
		Faction fac = character.GetFaction();
		if (fac) return fac.GetFactionKey();
		return "";
	}

	// ----------------------------------------------------------------
	// Returns a hash for the character (used for occupant dictionary).
	protected int GetCharacterHash(SCR_ChimeraCharacter character)
	{
		if (!character) return 0;
		RplId id = Replication.FindId(character);
		return id;
	}

	int GetFactionCount(string factionKey)
	{
		return m_FactionCounts.Get(factionKey);
	}

	bool IsProxy()
	{
		if (!m_RplComponent)
			m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));
		return (m_RplComponent && m_RplComponent.IsProxy());
	}

	// ----------------------------------------------------------------
	// Dummy placeholders for hooking AI group events
	protected void SetupDeathListener(SCR_AIGroup group)
	{
		// e.g. if you want each agent's death event, do it here
	}

	protected void WaterCheck(SCR_AIGroup group)
	{
		// e.g. if any AI is in water, move them
	}
}

// ====================================================================================
// IA_QueryCallback for sphere queries
// Same as your original code
// ====================================================================================
class IA_QueryCallback
{
	protected array<IEntity> m_Entities;

	void IA_QueryCallback(array<IEntity> arr)
	{
		m_Entities = arr;
	}

	bool OnEntityFound(IEntity e)
	{
		m_Entities.Insert(e);
		return true;
	}
}

// ====================================================================================
// Minimal dictionary classes for occupant tracking: string->int, int->string
// (Unchanged from original)
// ====================================================================================
const int IA_DICT_STRING_INT_MAX = 32;
class IA_DictStringInt
{
	private string m_Keys[IA_DICT_STRING_INT_MAX];
	private int    m_Values[IA_DICT_STRING_INT_MAX];
	private int    m_Count;

	void IA_DictStringInt()
	{
		m_Count = 0;
		for (int i = 0; i < IA_DICT_STRING_INT_MAX; i++)
		{
			m_Keys[i]   = "";
			m_Values[i] = 0;
		}
	}

	bool Contains(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return true;
		return false;
	}

	int Get(string key)
	{
		for (int i = 0; i < m_Count; i++)
			if (m_Keys[i] == key) return m_Values[i];
		return 0;
	}

	void Set(string key, int value)
	{
		for (int i = 0; i < m_Count; i++)
		{
			if (m_Keys[i] == key)
			{
				m_Values[i] = value;
				return;
			}
		}
		if (m_Count < IA_DICT_STRING_INT_MAX)
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
				m_Values[m_Count - 1] = 0;
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
			m_Values[i] = 0;
		}
		m_Count = 0;
	}

	int GetCount()
	{
		return m_Count;
	}

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

const int IA_DICT_INT_STRING_MAX = 64;
class IA_DictIntString
{
	private int    m_Keys[IA_DICT_INT_STRING_MAX];
	private string m_Values[IA_DICT_INT_STRING_MAX];
	private int    m_Count;

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
