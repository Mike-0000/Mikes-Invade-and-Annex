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
	if (order == IA_AiOrder.Defend)
	{
		return "{D9C14ECEC9772CC6}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Defend.et";
	}
	else if (order == IA_AiOrder.Patrol)
	{
		return "{C0A9A9B589802A5B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_Patrol.et";
	}
	else if (order == IA_AiOrder.SearchAndDestroy)
	{
		return "{EE9A99488B40628B}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_SearchAndDestroy.et";
	}
	else if (order == IA_AiOrder.GetInVehicle)
	{
		return "{0A2A37B4A56D74DF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetInNearest.et";
	}
	else if (order == IA_AiOrder.GetOutOfVehicle)
	{
		return "{2602CAB8AB74FBBF}PrefabsEditable/Auto/AI/Waypoints/E_AIWaypoint_GetOut.et";
	}
	// Default
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
		m_lastOrderPosition = initialPos;
		m_initialPosition   = initialPos;
		m_lastOrderTime     = 0;
		m_squadType         = squad;
		m_faction           = fac;
	}

	static IA_AiGroup CreateMilitaryGroup(vector initialPos, IA_SquadType squadType, IA_Faction faction)
	{
		IA_AiGroup grp = new IA_AiGroup(initialPos, squadType, faction);
		grp.m_isCivilian = false;
		return grp;
	}

	static IA_AiGroup CreateCivilianGroup(vector initialPos)
	{
		IA_AiGroup grp = new IA_AiGroup(initialPos, IA_SquadType.Riflemen, IA_Faction.CIV);
		grp.m_isCivilian = true;
		return grp;
	}

	IA_Faction GetEngagedEnemyFaction()
	{
		return m_engagedEnemyFaction;
	}

	bool IsEngagedWithEnemy()
	{
		return (m_engagedEnemyFaction != IA_Faction.NONE);
	}

	void MarkAsUnengaged()
	{
		m_engagedEnemyFaction = IA_Faction.NONE;
	}

	array<SCR_ChimeraCharacter> GetGroupCharacters()
	{
		array<SCR_ChimeraCharacter> chars = {};
		if (!m_group || !m_isSpawned)
		{
			return chars;
		}

		array<AIAgent> agents = {};
		m_group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!ch)
			{
				Print("[IA_AiGroup] Cast to SCR_ChimeraCharacter failed!", LogLevel.WARNING);
				continue;
			}
			chars.Insert(ch);
		}
		return chars;
	}

	void AddOrder(vector origin, IA_AiOrder order, bool topPriority = false)
	{
		if (!m_isSpawned)
		{
			Print("[IA_AiGroup.AddOrder] Group not spawned!", LogLevel.WARNING);
			return;
		}
		if (!m_group)
		{
			return;
		}

		float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		origin[1] = y + 0.5;

		ResourceName rname = IA_AiOrderResource(order);
		Resource res = Resource.Load(rname);
		if (!res)
		{
			Print("[IA_AiGroup.AddOrder] Missing resource: " + rname, LogLevel.ERROR);
			return;
		}

		IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(origin));
		SCR_AIWaypoint w = SCR_AIWaypoint.Cast(waypointEnt);
		if (!w)
		{
			Print("[IA_AiGroup.AddOrder] Could not cast to AIWaypoint: " + rname, LogLevel.ERROR);
			return;
		}

		if (m_isDriving || topPriority)
		{
			w.SetPriorityLevel(2000);
		}

		m_group.AddWaypoint(w);
		m_lastOrderPosition = origin;
		m_lastOrderTime     = System.GetUnixTime();
	}

	void SetInitialPosition(vector pos)
	{
		m_initialPosition = pos;
	}

	int TimeSinceLastOrder()
	{
		return System.GetUnixTime() - m_lastOrderTime;
	}

	bool HasOrders()
	{
		if (!m_group)
		{
			return false;
		}
		array<AIWaypoint> wps = {};
		m_group.GetWaypoints(wps);
		return !wps.IsEmpty();
	}

	bool IsDriving()
	{
		return m_isDriving;
	}

	void RemoveAllOrders(bool resetLastOrderTime = false)
	{
		if (!m_group)
		{
			return;
		}
		array<AIWaypoint> wps = {};
		m_group.GetWaypoints(wps);
		foreach (AIWaypoint w : wps)
		{
			m_group.RemoveWaypoint(w);
		}
		m_isDriving = false;
		if (resetLastOrderTime)
		{
			m_lastOrderTime = 0;
		}
	}

	int GetAliveCount()
	{
		if (!m_isSpawned)
		{
			if (m_isCivilian)
			{
				return 1;
			}
			else
			{
				return IA_SquadCount(m_squadType, m_faction);
			}
		}
		if (!m_group)
		{
			return 0;
		}
		return m_group.GetPlayerAndAgentCount();
	}

	vector GetOrigin()
	{
		if (!m_group)
		{
			return vector.Zero;
		}
		return m_group.GetOrigin();
	}

	bool IsSpawned()
	{
		return m_isSpawned;
	}

	void Spawn(IA_AiOrder initialOrder = IA_AiOrder.Patrol, vector orderPos = vector.Zero)
	{
		if (!PerformSpawn())
		{
			return;
		}
		if (orderPos != vector.Zero)
		{
			AddOrder(orderPos, initialOrder);
		}
		else
		{
			AddOrder(m_initialPosition, initialOrder);
		}
	}

	private bool PerformSpawn()
	{
		if (IsSpawned())
		{
			return false;
		}
		m_isSpawned = true;

		string resourceName;
		if (m_isCivilian)
		{
			resourceName = IA_RandomCivilianResourceName();
		}
		else
		{
			resourceName = IA_SquadResourceName(m_squadType, m_faction);
		}

		Resource res = Resource.Load(resourceName);
		if (!res)
		{
			Print("[IA_AiGroup.PerformSpawn] No resource: " + resourceName, LogLevel.ERROR);
			return false;
		}

		IEntity entity = GetGame().SpawnEntityPrefab(res, null, IA_CreateSurfaceAdjustedSpawnParams(m_initialPosition));
		if (m_isCivilian)
		{
			// For civs, spawn an extra group prefab and add the character
			Resource groupRes = Resource.Load("{2D4A93612FA7C24D}Prefabs/Groups/TAB_Group_CIV.et");
			IEntity groupEnt = GetGame().SpawnEntityPrefab(groupRes, null, IA_CreateSimpleSpawnParams(m_initialPosition));
			m_group = SCR_AIGroup.Cast(groupEnt);

			if (!m_group.AddAIEntityToGroup(entity))
			{
				Print("[IA_AiGroup.PerformSpawn] Could not add civ to group: " + resourceName, LogLevel.ERROR);
			}
		}
		else
		{
			m_group = SCR_AIGroup.Cast(entity);
		}
		if (!m_group)
		{
			Print("[IA_AiGroup.PerformSpawn] Could not cast to AIGroup", LogLevel.ERROR);
			return false;
		}

		SetupDeathListener();
		WaterCheck();
		return true;
	}

	void Despawn()
	{
		if (!IsSpawned())
		{
			return;
		}
		m_isSpawned = false;
		m_isDriving = false;

		if (m_group)
		{
			m_group.DeactivateAllMembers();
		}
		RemoveAllOrders();
		IA_Game.AddEntityToGc(m_group);
		m_group = null;
	}

	IA_Faction GetFaction()
	{
		return m_faction;
	}

	private void SetupDeathListener()
	{
		if (!m_group)
		{
			return;
		}
		array<AIAgent> agents = {};
		m_group.GetAgents(agents);

		if (agents.IsEmpty())
		{
			GetGame().GetCallqueue().CallLater(SetupDeathListener, 1000);
			return;
		}

		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!ch)
			{
				continue;
			}
			SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(ch.FindComponent(SCR_CharacterControllerComponent));
			if (!ccc)
			{
				Print("[IA_AiGroup.SetupDeathListener] Missing SCR_CharacterControllerComponent in AI", LogLevel.WARNING);
				continue;
			}
			ccc.GetOnPlayerDeathWithParam().Insert(OnMemberDeath);
		}
	}

	private void OnMemberDeath(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
	{
		if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
		{
			if (!IsEngagedWithEnemy() && !m_isCivilian && m_faction != IA_Faction.CIV)
			{
				Print("[IA_AiGroup.OnMemberDeath] Player killed enemy, group engaged with CIV as placeholder");
				m_engagedEnemyFaction = IA_Faction.CIV; 
			}
		}
		else if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_AI)
		{
			SCR_ChimeraCharacter c = SCR_ChimeraCharacter.Cast(killerEntity);
			if (!c)
			{
				Print("[IA_AiGroup.OnMemberDeath] cast killerEntity to character failed!", LogLevel.ERROR);
			}
			else
			{
				string fKey = c.GetFaction().GetFactionKey();
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
		// placeholder
	}
}

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
// 5) IA_Area
///////////////////////////////////////////////////////////////////////
class IA_Area
{
	static const float SpawnAiAreaRadius = 500;

	private string      m_name;
	private IA_AreaType m_type;
	private vector      m_origin;
	private float       m_radius;
	private bool        m_instantiated = false;

	private void IA_Area(string nm, IA_AreaType t, vector org, float rad)
	{
		m_name = nm;
		m_type = t;
		m_origin = org;
		m_radius = rad;
	}

	static IA_Area Create(string nm, IA_AreaType t, vector org, float rad)
	{
		return new IA_Area(nm, t, org, rad);
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
		if (m_type == IA_AreaType.Town)
		{
			return 4;
		}
		else if (m_type == IA_AreaType.City)
		{
			return 8;
		}
		else if (m_type == IA_AreaType.Docks)
		{
			return 2;
		}
		else if (m_type == IA_AreaType.Airport)
		{
			return 6;
		}
		else if (m_type == IA_AreaType.Military)
		{
			return 3;
		}
		else if (m_type == IA_AreaType.Property)
		{
			return 1;
		}
		Print("[IA_Area] fallback=0 " + m_name);
		return 0;
	}

	int GetCivilianCount()
	{
		if (m_type == IA_AreaType.Town)
		{
			return 6;
		}
		else if (m_type == IA_AreaType.City)
		{
			return 12;
		}
		else if (m_type == IA_AreaType.Docks)
		{
			return 3;
		}
		else if (m_type == IA_AreaType.Property)
		{
			return 2;
		}
		return 0;
	}
}

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
		if (m_groups.Count() == m_groupCount)
		{
			return;
		}

		IA_SquadType st = IA_GetRandomSquadType();
		IA_AiGroup grp  = IA_AiGroup.CreateMilitaryGroup(origin, st, m_faction);
		m_groups.Insert(grp);
		grp.Spawn();

		if (m_groups.Count() == m_groupCount)
		{
			m_initializing = false;
		}
	}

	void ProcessNextGroup()
	{
		if (m_groupToProcess >= m_groups.Count())
		{
			m_groupToProcess = 0;
		}
		if (m_groups.IsEmpty())
		{
			return;
		}

		IA_AiGroup g = m_groups[m_groupToProcess];
		if (g.GetAliveCount() == 0)
		{
			m_groups.Remove(m_groupToProcess);
			return;
		}
		if (g.TimeSinceLastOrder() >= ORDER_FREQ_SECS)
		{
			g.RemoveAllOrders();
		}
		if (!g.HasOrders())
		{
			vector pos;
			if (m_initialOrderGiven)
			{
				pos = m_area.GetOrigin();
			}
			else
			{
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
		{
			return false;
		}
		if (m_groups.IsEmpty())
		{
			return true;
		}
		return false;
	}

	bool IsAnyEngaged()
	{
		foreach (IA_AiGroup g : m_groups)
		{
			if (g.IsEngagedWithEnemy())
			{
				return true;
			}
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
}

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
// 8) IA_AreaInstance - rewritten for dynamic mission/task handling
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

	// --- NEW: Dynamic Task Variables ---
	private SCR_TriggerTask m_currentTaskEntity;
	private ref array<SCR_TriggerTask> m_taskQueue = {};

	// Constructor
	void IA_AreaInstance(IA_Area area, IA_Faction faction, int startStrength = 0)
	{
		m_area     = area;
		m_faction  = faction;
		m_strength = startStrength;

		m_area.SetInstantiated(true);

		int groupCount = m_area.GetMilitaryAiGroupCount();
		GenerateRandomAiGroups(groupCount, true);

		int civCount = m_area.GetCivilianCount();
		GenerateCivilians(civCount);
	}

	// --- Task Queue Methods ---
	// QueueTask: If no active task, create it immediately; otherwise add to the queue.
	void QueueTask(string title, string description, vector spawnOrigin)
	{
		if (!m_currentTaskEntity)
		{
			CreateTask(title, description, spawnOrigin);
		}
		else
		{
			Resource res = Resource.Load("{33DA4D098C409420}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
			IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
			SCR_TriggerTask task = SCR_TriggerTask.Cast(taskEnt);
			task.SetTitle(title);
			task.SetDescription(description);
			// Do not activate now; store it in the queue.
			m_taskQueue.Insert(task);
		}
	}

	// CreateTask: Immediately spawn and activate a task.
	private void CreateTask(string title, string description, vector spawnOrigin)
	{
		Resource res = Resource.Load("{33DA4D098C409420}Prefabs/Tasks/CTF_TriggerTask_Capture.et");
		IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(spawnOrigin));
		m_currentTaskEntity = SCR_TriggerTask.Cast(taskEnt);
		m_currentTaskEntity.SetTitle(title);
		m_currentTaskEntity.SetDescription(description);
		m_currentTaskEntity.Create(false);
	}

	// CompleteCurrentTask: Finishes the active task and, if available, activates the next queued task.
	void CompleteCurrentTask()
	{
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
		}
	}

	// UpdateTask: Checks whether the current task has finished and, if so, completes it.
	void UpdateTask()
	{
		// (Here you could check more specific conditions; we assume IsFinished() exists.)
		if (m_currentTaskEntity && m_currentTaskEntity.GetTaskState() == SCR_TaskState.FINISHED)
		{
			CompleteCurrentTask();
		}
	}

	// --- End New Task Methods ---

	// RunNextTask: This method is called from the periodic game task loop.
	void RunNextTask()
	{
		m_currentTask = m_currentTask + 1;
		if (m_currentTask == 1)
		{
			// (Placeholder: do nothing or process non-despawn logic)
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
			// (Placeholder: spawn or remove if needed)
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
		// NEW: update the current task (if any) in the dynamic task queue.
		UpdateTask();
	}

	// --- Existing Methods (unchanged from your original IA_AreaInstance) ---
	bool IsUnderAttack()
	{
		return (!m_attackingFactions.IsEmpty());
	}

	void OnAttacked(IA_Faction attacker)
	{
		if (attacker == m_faction)
		{
			Print("[IA_AreaInstance.OnAttacked] ignoring same faction attack");
			return;
		}
		Print(m_area.GetName() + " is under attack by faction=" + attacker);

		if (m_reinforcements == IA_ReinforcementState.NotDone)
		{
			m_reinforcements = IA_ReinforcementState.Countdown;
			Print("[IA_AreaInstance.OnAttacked] set reinforcements countdown");
		}
		else
		{
			Print("[IA_AreaInstance.OnAttacked] no new reinforcements");
		}
	}

	void OnFactionChange(IA_Faction newFaction)
	{
		m_faction = newFaction;
		m_attackingFactions.Clear();
		m_strength = 0;
		IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
		if (rep)
		{
			rep.SetFaction(m_area.GetName(), newFaction);
		}
		Print("[IA_AreaInstance.OnFactionChange] " + m_area.GetName() + " => new faction=" + newFaction);
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
		if (rep)
		{
			rep.SetStrength(m_area.GetName(), newVal);
		}
		Print("[IA_AreaInstance.OnStrengthChange] " + m_area.GetName() + " => new strength=" + newVal);
	}

	private void ReinforcementsTask()
	{
		if (m_reinforcements != IA_ReinforcementState.Countdown)
		{
			return;
		}
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
				IA_Faction newOwner = m_attackingFactions[0];
				OnFactionChange(newOwner);
			}
			else if (m_attackingFactions.Count() > 1 && m_faction != IA_Faction.NONE)
			{
				Print("[IA_AreaInstance.StrengthUpdateTask] contested => faction=NONE");
				m_faction = IA_Faction.NONE;
			}
		}
	}

	private void MilitaryOrderTask()
	{
		if (!m_canSpawn)
		{
			return;
		}
		if (m_military.IsEmpty())
		{
			return;
		}
		IA_AiOrder order = IA_AiOrder.Patrol;
		if (IsUnderAttack())
		{
			order = IA_AiOrder.SearchAndDestroy;
		}
		foreach (IA_AiGroup g : m_military)
		{
			if (!g.IsSpawned())
			{
				continue;
			}
			if (g.GetAliveCount() == 0)
			{
				continue;
			}
			if (g.TimeSinceLastOrder() >= 60)
			{
				g.RemoveAllOrders();
			}
			if (!g.HasOrders())
			{
				vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
				g.AddOrder(pos, order);
			}
		}
	}

	private void CivilianOrderTask()
	{
		if (!m_canSpawn)
		{
			return;
		}
		if (m_civilians.IsEmpty())
		{
			return;
		}
		foreach (IA_AiGroup g : m_civilians)
		{
			if (!g.IsSpawned())
			{
				continue;
			}
			if (g.GetAliveCount() == 0)
			{
				continue;
			}
			if (g.TimeSinceLastOrder() >= 60)
			{
				g.RemoveAllOrders();
				vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius(), m_area.GetOrigin());
				g.AddOrder(pos, IA_AiOrder.Patrol);
			}
		}
	}

	private void AiAttackersTask()
	{
		if (!m_aiAttackers)
		{
			return;
		}
		if (m_aiAttackers.IsInitializing())
		{
			vector pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 1.2,
			                                                      m_area.GetRadius() * 1.3,
			                                                      m_area.GetOrigin());
			m_aiAttackers.SpawnNextGroup(pos);
			return;
		}
		if (m_aiAttackers.IsAllGroupsDead())
		{
			IA_Faction f = m_aiAttackers.GetFaction();
			m_attackingFactions.RemoveItem(f);
			delete m_aiAttackers;
			m_aiAttackers = null;
			Print("[IA_AreaInstance.AiAttackersTask] Attackers from faction " + f + " defeated in " + m_area.GetName());
			return;
		}
		m_aiAttackers.ProcessNextGroup();
	}

	void GenerateRandomAiGroups(int number, bool insideArea)
	{
		int strCounter = m_strength;
		int i;
		for (i = 0; i < number; i = i + 1)
		{
			IA_SquadType st = IA_GetRandomSquadType();
			vector pos = vector.Zero;
			if (insideArea)
			{
				pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
			}
			else
			{
				pos = IA_Game.rng.GenerateRandomPointInRadius(m_area.GetRadius() * 0.95, m_area.GetRadius() * 1.05, m_area.GetOrigin());
			}
			IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroup(pos, st, m_faction);
			strCounter = strCounter + grp.GetAliveCount();
			m_military.Insert(grp);
		}
		OnStrengthChange(strCounter);
	}

	void GenerateCivilians(int number)
	{
		m_civilians.Clear();
		int i;
		for (i = 0; i < number; i = i + 1)
		{
			vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, m_area.GetRadius() / 3, m_area.GetOrigin());
			float y = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
			pos[1] = y;
			IA_AiGroup civ = IA_AiGroup.CreateCivilianGroup(pos);
			m_civilians.Insert(civ);
		}
	}
}


///////////////////////////////////////////////////////////////////////
// 9) IA_Game SINGLETON - includes zone flow logic
///////////////////////////////////////////////////////////////////////
class IA_Game
{
	static private ref IA_Game m_instance = null;
	static bool HasInstance()
	{
		if (m_instance)
			return true;
		return false;
	}

	static ref RandomGenerator rng = new RandomGenerator();

	private bool m_periodicTaskActive = false;
	private ref array<ref IA_AreaInstance> m_areas = {};

	static private ref array<IEntity> m_entityGc = {};

	private bool m_hasInit = false;

	// ---------------------------
	// ZoneFlow data
	// ---------------------------
	private bool m_flowActive = false;
	private int  m_currentZoneIndex = 0;
	private ref array<IA_InvadeAnnexZoneEntity> m_zoneFlowList = {};

	void Init()
	{
		if (m_hasInit)
			return;

		m_hasInit = true;
		ActivatePeriodicTask();
	}

	static IA_Game Instantiate()
	{
		if (m_instance)
		{
			Print("[IA_Game] Deleting old instance");
			delete m_instance;
		}
		Print("[IA_Game] Creating new IA_Game");
		m_instance = new IA_Game();
		m_instance.Init();
		return m_instance;
	}

	void ~IA_Game()
	{
		m_areas.Clear();
		m_entityGc.Clear();
	}

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
		GetGame().GetCallqueue().CallLater(PeriodicalGameTask, 200, true);
		GetGame().GetCallqueue().CallLater(EntityGcTask, 250, true);

		Print("[IA_Game] Periodic tasks started");
	}

	void PeriodicalGameTask()
	{
		if (m_areas.IsEmpty())
			return;

		foreach (IA_AreaInstance areaInst : m_areas)
		{
			areaInst.RunNextTask();
		}

		if (m_flowActive)
		{
			ProcessFlowLogic();
		}
	}

	IA_AreaInstance AddArea(IA_Area area, IA_Faction fac, int strength = 0)
	{
		IA_AreaInstance inst = new IA_AreaInstance(area, fac, strength);
		m_areas.Insert(inst);

		IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
		if (rep)
		{
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

	//----------------------------------------------------------------
	// FLOW LOGIC - gather all zone entities, randomize, do them one by one
	//----------------------------------------------------------------
	void StartZoneFlow()
	{
		if (m_flowActive)
		{
			Print("[IA_Game.StartZoneFlow] flow already active");
			return;
		}

		// gather zone entities
		//m_zoneFlowList = IA_ZoneFinder.GetAllZones();
		if (m_zoneFlowList.IsEmpty())
		{
			Print("[IA_Game.StartZoneFlow] no zones found");
			
		}

		// randomize
		RandomizeZoneList(m_zoneFlowList);

		m_currentZoneIndex = 0;
		m_flowActive = true;
		Print("[IA_Game.StartZoneFlow] flow started with " + m_zoneFlowList.Count() + " zones");
	}

	private void ProcessFlowLogic()
	{
		if (m_currentZoneIndex >= m_zoneFlowList.Count())
		{
			Print("[IA_Game.ProcessFlowLogic] all zones done!");
			m_flowActive = false;
			return;
		}

		IA_InvadeAnnexZoneEntity currentZone = m_zoneFlowList[m_currentZoneIndex];
		// We'll check if there's an area for it. If not, create one. If it's done => next
		string zoneName = "Zone_" + m_currentZoneIndex; // or something
		IA_AreaInstance inst = FindAreaInstance(zoneName);

		if (!inst)
		{
			// create an area for it
			vector pos = currentZone.GetOrigin();
			float rad = 120.0;
			IA_Area area = IA_Area.Create(zoneName, IA_AreaType.Military, pos, rad);

			// For the "objective," we consider "done" if it gets captured by US
			AddArea(area, IA_Faction.USSR, 0);

			Print("[IA_Game.ProcessFlowLogic] created area for zoneIndex=" + m_currentZoneIndex);
		}
		else
		{
			// Check if area is done => if so, next zone
			bool isDone = IsAreaObjectiveComplete(inst);
			if (isDone)
			{
				Print("[IA_Game.ProcessFlowLogic] zoneIndex=" + m_currentZoneIndex + " is done => moving on");
				m_currentZoneIndex++;
			}
			else
			{
				// keep waiting
			}
		}
	}

	private bool IsAreaObjectiveComplete(IA_AreaInstance inst)
	{
		// Example condition: The area is "done" if it ends up US faction
		if (inst.m_faction == IA_Faction.US)
		{
			return true;
		}
		return false;
	}

	private void RandomizeZoneList(array<IA_InvadeAnnexZoneEntity> list)
	{
		int n = list.Count();
		int i;
		for (i = 0; i < n - 1; i = i + 1)
		{
			int swapIdx = rng.RandInt(i, n);
			IA_InvadeAnnexZoneEntity temp = list[i];
			list[i] = list[swapIdx];
			list[swapIdx] = temp;
		}
	}
}

///////////////////////////////////////////////////////////////////////
// 10) REPLICATION WORKAROUND
///////////////////////////////////////////////////////////////////////
class IA_ReplicationWorkaroundClass : GenericEntityClass {}

class IA_ReplicatedAreaInstance
{
	void IA_ReplicatedAreaInstance(string nm, IA_Faction fac, int strVal)
	{
		name = nm;
		faction = fac;
		strength = strVal;
	}
	string name;
	IA_Faction faction;
	int strength;
}

class IA_ReplicationWorkaround : GenericEntity
{
	static private ref array<ref IA_ReplicatedAreaInstance> m_areaList = {};
	static private ref array<ref IA_AreaInstance> m_areaInstances = {};

	static private IA_ReplicationWorkaround m_instance;

	void IA_ReplicationWorkaround(IEntitySource src, IEntity parent)
	{
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
		m_areaList.Insert(new IA_ReplicatedAreaInstance(areaName, fac, strVal));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	private void RpcDo_SetFaction(string areaName, int facInt)
	{
		if (!IA_Game.HasInstance())
		{
			return;
		}
		IA_Game g = IA_Game.Instantiate();
		IA_AreaInstance inst = g.FindAreaInstance(areaName);
		if (!inst)
		{
			Print("[IA_ReplicationWorkaround] Could not find area " + areaName, LogLevel.ERROR);
			return;
		}
		IA_Faction f = IA_FactionFromInt(facInt);
		inst.OnFactionChange(f);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	private void RpcDo_SetStrength(string areaName, int s)
	{
		if (!IA_Game.HasInstance())
		{
			return;
		}
		IA_Game g = IA_Game.Instantiate();
		IA_AreaInstance inst = g.FindAreaInstance(areaName);
		if (!inst)
		{
			Print("[IA_ReplicationWorkaround] no area " + areaName, LogLevel.ERROR);
			return;
		}
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
		int i;
		for (i = 0; i < c; i = i + 1)
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
		{
			return false;
		}
		int i;
		for (i = 0; i < areaCount; i = i + 1)
		{
			string nm;
			int strVal;
			int facInt;
			bool ok1 = reader.ReadString(nm);
			bool ok2 = reader.Read(strVal, 8);
			bool ok3 = reader.Read(facInt, 8);
			if (!ok1 || !ok2 || !ok3)
			{
				return false;
			}
			IA_Faction ff = IA_FactionFromInt(facInt);
			IA_ReplicatedAreaInstance rep = new IA_ReplicatedAreaInstance(nm, ff, strVal);
			m_areaList.Insert(rep);

			Print("[IA_ReplicationWorkaround] RplLoad => name=" + nm + " fac=" + ff + " str=" + strVal);
		}
		return true;
	}
}

///////////////////////////////////////////////////////////////////////
// 11) HELPER FACTION / SQUAD UTILS
///////////////////////////////////////////////////////////////////////
string IA_SquadResourceName_US(IA_SquadType st)
{
	if (st == IA_SquadType.Riflemen)
	{
		return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
	}
	else if (st == IA_SquadType.Firesquad)
	{
		return "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
	}
	else if (st == IA_SquadType.Medic)
	{
		return "{EF62027CC75A7459}Prefabs/Groups/BLUFOR/Group_US_MedicalSection.et";
	}
	else if (st == IA_SquadType.Antitank)
	{
		return "{FAEA8B9E1252F56E}Prefabs/Groups/BLUFOR/Group_US_Team_LAT.et";
	}
	return "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
}

string IA_SquadResourceName_USSR(IA_SquadType st)
{
	if (st == IA_SquadType.Riflemen)
	{
		return "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
	}
	else if (st == IA_SquadType.Firesquad)
	{
		return "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
	}
	else if (st == IA_SquadType.Medic)
	{
		return "{D815658156080328}Prefabs/Groups/OPFOR/Group_USSR_MedicalSection.et";
	}
	else if (st == IA_SquadType.Antitank)
	{
		return "{96BAB56E6558788E}Prefabs/Groups/OPFOR/Group_USSR_Team_AT.et";
	}
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
	return s_IA_CivList[idx];
}

string IA_SquadResourceName(IA_SquadType st, IA_Faction fac)
{
	if (fac == IA_Faction.US)
	{
		return IA_SquadResourceName_US(st);
	}
	else if (fac == IA_Faction.USSR)
	{
		return IA_SquadResourceName_USSR(st);
	}
	return IA_SquadResourceName_US(st);
}

int IA_SquadCount(IA_SquadType st, IA_Faction fac)
{
	if (fac == IA_Faction.US)
	{
		if (st == IA_SquadType.Riflemen)
		{
			return 9;
		}
		else if (st == IA_SquadType.Firesquad)
		{
			return 4;
		}
		else if (st == IA_SquadType.Medic)
		{
			return 2;
		}
		else if (st == IA_SquadType.Antitank)
		{
			return 4;
		}
		return 9;
	}
	else if (fac == IA_Faction.USSR)
	{
		if (st == IA_SquadType.Riflemen)
		{
			return 6;
		}
		else if (st == IA_SquadType.Firesquad)
		{
			return 4;
		}
		else if (st == IA_SquadType.Medic)
		{
			return 2;
		}
		else if (st == IA_SquadType.Antitank)
		{
			return 4;
		}
		return 6;
	}
	return 1; // civ fallback
}

IA_SquadType IA_GetRandomSquadType()
{
	int r = IA_Game.rng.RandInt(0, 4);
	if (r == 0)
	{
		return IA_SquadType.Riflemen;
	}
	else if (r == 1)
	{
		return IA_SquadType.Firesquad;
	}
	else if (r == 2)
	{
		return IA_SquadType.Medic;
	}
	else if (r == 3)
	{
		return IA_SquadType.Antitank;
	}
	return IA_SquadType.Riflemen;
}

int IA_FactionToInt(IA_Faction f)
{
	if (f == IA_Faction.US)
	{
		return 1;
	}
	else if (f == IA_Faction.USSR)
	{
		return 2;
	}
	else if (f == IA_Faction.CIV)
	{
		return 3;
	}
	return 0;
}

IA_Faction IA_FactionFromInt(int i)
{
	if (i == 1)
	{
		return IA_Faction.US;
	}
	else if (i == 2)
	{
		return IA_Faction.USSR;
	}
	else if (i == 3)
	{
		return IA_Faction.CIV;
	}
	return IA_Faction.NONE;
}

///////////////////////////////////////////////////////////////////////
// 12) HELPER SPAWN PARAMS
///////////////////////////////////////////////////////////////////////
EntitySpawnParams IA_CreateSimpleSpawnParams(vector origin)
{
	EntitySpawnParams p = new EntitySpawnParams();
	p.TransformMode = ETransformMode.WORLD;
	p.Transform[3]  = origin;
	return p;
}

EntitySpawnParams IA_CreateSurfaceAdjustedSpawnParams(vector origin)
{
	float y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
	origin[1] = y;
	EntitySpawnParams p = new EntitySpawnParams();
	p.TransformMode = ETransformMode.WORLD;
	p.Transform[3]  = origin;
	return p;
}

///////////////////////////////////////////////////////////////////////
// 13) ZONE FINDER - gather all zone entities
///////////////////////////////////////////////////////////////////////

// To Remove - Replace with Dynamic Objective Placement

//class IA_ZoneFinder
//{
//	static array<IA_InvadeAnnexZoneEntity> GetAllZones()
//	{
//		array<IA_InvadeAnnexZoneEntity> results = {};
//		array<IEntity> temp = {};
//
//		GetGame().GetWorld().QueryEntitiesBySphere(IA_InvadeAnnexZoneEntity, temp);
//
//		foreach (IEntity e : temp)
//		{
//			IA_InvadeAnnexZoneEntity z = IA_InvadeAnnexZoneEntity.Cast(e);
//			if (z)
//			{
//				results.Insert(z);
//			}
//		}
//		return results;
//	}
//}

///////////////////////////////////////////////////////////////////////
// 14) IA_InvadeAnnexZoneEntity
///////////////////////////////////////////////////////////////////////
class IA_InvadeAnnexZoneEntityClass : ScriptedGameTriggerEntityClass {}

class IA_InvadeAnnexZoneEntity : ScriptedGameTriggerEntity
{
	protected RplComponent m_RplComponent;
	protected bool m_bGameInit = false;

	override void EOnInit(IEntity owner)
	{
		m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));

		if (!IsProxy())
		{
			if (!m_bGameInit)
			{
				m_bGameInit = true;

				// Make sure there's an IA_ReplicationWorkaround entity in the world 
				// (either place it in editor or spawn it if needed).

				IA_Game g = IA_Game.Instantiate();

				int seed = System.GetUnixTime();
				Print("[IA_InvadeAnnexZoneEntity] Using seed=" + seed);
				IA_Game.rng.SetSeed(seed);

				// We won't auto-add an area for THIS zone
				// Instead, we rely on the new "ZoneFlow" system:
				g.StartZoneFlow();

				Print("[IA_InvadeAnnexZoneEntity] done init. This zone can be discovered by IA_ZoneFinder if needed.");
			}
		}
	}

	bool IsProxy()
	{
		if (m_RplComponent && m_RplComponent.IsProxy())
			return true;
		return false;
	}

	override protected void OnActivate(IEntity ent)
	{
		// no-op
	}

	override protected void OnDeactivate(IEntity ent)
	{
		// no-op
	}
}
