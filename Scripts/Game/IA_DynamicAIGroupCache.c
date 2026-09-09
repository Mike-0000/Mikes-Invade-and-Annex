// Single cache/wake cycle for a pristine stationary garrison. The owner stays alive.
class IA_DynamicAIGroupCache
{
	protected IA_AiGroup m_Owner;
	protected ref array<ref IA_DynamicAIUnit> m_aUnits = {};
	protected int m_iCreatedMs;
	protected int m_iNextUnit;
	protected bool m_bCached;
	protected bool m_bWaking;
	protected bool m_bFinished;

	void Init(IA_AiGroup owner)
	{
		m_Owner = owner;
		m_iCreatedMs = System.GetTickCount();
	}

	IA_AiGroup GetOwner()
	{
		return m_Owner;
	}

	bool IsCached()
	{
		return m_bCached;
	}

	bool IsWaking()
	{
		return m_bCached && m_bWaking;
	}

	bool IsFinished()
	{
		return m_bFinished;
	}

	bool IsOwnerLive()
	{
		return m_Owner && m_Owner.IsDynamicAIOwnerLive();
	}

	int GetUnrestoredCount()
	{
		int count;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (!unit.m_bRestored)
				count++;
		}
		return count;
	}

	int GetLogicalAliveCount()
	{
		int count;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.IsLogicallyAlive())
				count++;
		}
		return count;
	}

	void OnUnitKilled(IEntity entity)
	{
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (unit.m_Entity != entity)
				continue;
			unit.m_bDead = true;
			unit.m_bRestored = true;
			return;
		}
	}

	bool Intersects(vector center, float radius)
	{
		if (!m_bCached)
			return false;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (!unit.m_bRestored && unit.IsInside(center, radius))
				return true;
		}
		return false;
	}

	void RequestWake()
	{
		if (m_bCached)
			m_bWaking = true;
	}

	void CheckWake(array<vector> players, bool enabled)
	{
		if (!m_bCached || m_bWaking)
			return;
		if (!enabled)
		{
			RequestWake();
			return;
		}
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			if (IA_SpawnPlacement.IsNearAnyPlayer(unit.GetPosition(), players, IA_DynamicAISpawning.WAKE_DISTANCE_M))
			{
				RequestWake();
				return;
			}
		}
	}

	// All safety checks affect only entry into the cache. None veto a requested wake.
	bool TryCache(array<vector> players, int now, bool allowCapture)
	{
		if (m_bFinished || m_bCached || !IsOwnerLive())
			return false;
		if (m_Owner.GetLastDangerEventTime() > 0 || m_Owner.IsEngagedWithEnemy())
		{
			m_bFinished = true;
			return false;
		}
		SCR_AIGroup group = m_Owner.GetSCR_AIGroup();
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent member : agents)
		{
			if (!member || !member.GetControlledEntity())
				continue;
			vector transform[4];
			member.GetControlledEntity().GetWorldTransform(transform);
			if (IA_SpawnPlacement.IsNearAnyPlayer(transform[3], players, IA_DynamicAISpawning.CACHE_DISTANCE_M))
			{
				m_bFinished = true;
				return false;
			}
		}
		if (now - m_iCreatedMs < IA_DynamicAISpawning.SETTLE_DELAY_MS)
			return false;
		if (!m_Owner.IsDynamicAICacheReady())
		{
			// Slow/blocked placement stays on the established live path.
			if (now - m_iCreatedMs >= IA_DynamicAISpawning.SETTLE_TIMEOUT_MS)
				m_bFinished = true;
			return false;
		}

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!allowCapture || !manager || agents.IsEmpty())
			return false;
		m_aUnits.Clear();
		foreach (AIAgent agent : agents)
		{
			if (!agent)
			{
				m_aUnits.Clear();
				m_bFinished = true;
				return false;
			}
			SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!IsPristineInfantry(pawn, manager) || agent.GetPermanentLOD() != -1)
			{
				m_aUnits.Clear();
				m_bFinished = true;
				return false;
			}
			ref IA_DynamicAIUnit record = new IA_DynamicAIUnit();
			if (!record.Capture(pawn))
			{
				m_aUnits.Clear();
				m_bFinished = true;
				return false;
			}
			if (pawn == group.GetLeaderEntity())
				m_aUnits.InsertAt(record, 0);
			else
				m_aUnits.Insert(record);
		}

		// Publish the logical roster before any member/empty-group callbacks.
		m_bCached = true;
		m_Owner.SuspendForDynamicAI();
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			IEntity original = unit.m_Entity;
			unit.m_Entity = null;
			RplComponent.DeleteRplEntity(original, false);
		}
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][DynamicAI] Cached %1 soldiers at %2.", m_aUnits.Count(), m_Owner.GetOrigin()), LogLevel.NORMAL);
		}
		return true;
	}

	protected bool IsPristineInfantry(SCR_ChimeraCharacter pawn, PlayerManager manager)
	{
		if (!pawn || pawn.GetParent() || pawn.IsInVehicle() || manager.GetPlayerIdFromControlledEntity(pawn) > 0)
			return false;
		CharacterControllerComponent controller = pawn.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE || controller.IsUnconscious())
			return false;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(pawn.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return false;
		ref array<HitZone> zones = {};
		damage.GetAllHitZones(zones);
		foreach (HitZone zone : zones)
		{
			if (zone && zone.GetHealthScaled() < 0.999)
				return false;
		}
		return true;
	}

	// One attempt per call. Other soldiers/groups still get work when one attempt fails.
	bool RestoreNext(int now)
	{
		if (!IsWaking() || !IsOwnerLive())
			return false;
		if (GetUnrestoredCount() == 0)
		{
			FinishWake();
			return false;
		}
		int count = m_aUnits.Count();
		for (int checked = 0; checked < count; checked++)
		{
			if (m_iNextUnit >= count)
				m_iNextUnit = 0;
			IA_DynamicAIUnit unit = m_aUnits[m_iNextUnit];
			m_iNextUnit++;
			if (unit.m_bRestored || now < unit.m_iNextAttemptMs)
				continue;
			RestoreUnit(unit, now);
			if (GetUnrestoredCount() == 0)
				FinishWake();
			return true;
		}
		return false;
	}

	protected void FinishWake()
	{
		m_bCached = false;
		m_bWaking = false;
		m_bFinished = true;
		m_Owner.ResumeAfterDynamicAI();
		m_aUnits.Clear();
	}

	protected void RestoreUnit(IA_DynamicAIUnit unit, int now)
	{
		if (!unit.m_Entity)
		{
			if (!unit.m_Resource)
				unit.m_Resource = Resource.Load(unit.m_sPrefab);
			if (!unit.m_Resource)
			{
				unit.RecordFailure(now);
				return;
			}
			ref EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixCopy(unit.m_aTransform, params.Transform);
			unit.m_Entity = GetGame().SpawnEntityPrefab(unit.m_Resource, GetGame().GetWorld(), params);
			if (!unit.m_Entity)
			{
				unit.RecordFailure(now);
				return;
			}
			m_Owner.SetupDynamicAIUnit(unit.m_Entity);
		}

		// A soldier killed during attachment is a casualty, never a new spawn attempt.
		ChimeraCharacter pawn = ChimeraCharacter.Cast(unit.m_Entity);
		if (pawn && pawn.GetCharacterController() && pawn.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
		{
			unit.m_bDead = true;
			unit.m_bRestored = true;
			return;
		}
		// An admin can possess a newly created pawn before its agent attaches.
		// The soldier exists: hand it over without stealing control or duplicating it.
		PlayerManager manager = GetGame().GetPlayerManager();
		if (manager && manager.GetPlayerIdFromControlledEntity(unit.m_Entity) > 0)
		{
			unit.m_bRestored = true;
			return;
		}
		SCR_AIGroup group = m_Owner.GetSCR_AIGroup();
		AIControlComponent control = AIControlComponent.Cast(unit.m_Entity.FindComponent(AIControlComponent));
		AIAgent agent;
		if (control)
			agent = control.GetControlAIAgent();
		if (!agent)
		{
			unit.RecordFailure(now);
			return;
		}
		AIGroup previousGroup = agent.GetParentGroup();
		if (previousGroup && previousGroup != group)
			previousGroup.RemoveAgent(agent);
		if (!group.AddAIEntityToGroup(unit.m_Entity) || agent.GetParentGroup() != group)
		{
			unit.RecordFailure(now);
			return;
		}
		unit.m_bRestored = true;
		// Let successfully restored soldiers respond while the remainder is queued.
		group.ActivateAI();
	}

	void Retire()
	{
		if (m_bCached)
		{
			foreach (IA_DynamicAIUnit unit : m_aUnits)
			{
				// Attached/live soldiers belong to ordinary objective cleanup.
				if (unit.m_bRestored || !unit.m_Entity)
					continue;
				PlayerManager manager = GetGame().GetPlayerManager();
				if (manager && manager.GetPlayerIdFromControlledEntity(unit.m_Entity) <= 0)
					RplComponent.DeleteRplEntity(unit.m_Entity, false);
			}
		}
		m_bCached = false;
		m_bWaking = false;
		m_bFinished = true;
		m_aUnits.Clear();
	}
}
