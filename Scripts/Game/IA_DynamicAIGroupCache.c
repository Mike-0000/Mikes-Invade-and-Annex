// Repeated cache/wake cycles of the current garrison survivors. The owner stays alive.
class IA_DynamicAIGroupCache
{
	protected IA_AiGroup m_Owner;
	protected ref array<ref IA_DynamicAIUnit> m_aUnits = {};
	protected ref IA_DynamicAIActivityGate m_ActivityGate = new IA_DynamicAIActivityGate();
	protected int m_iNextUnit;
	protected bool m_bCached;
	protected bool m_bWaking;
	protected bool m_bFinished;
	protected ref array<IEntity> m_aDeletionAudit;
	protected int m_iAuditActiveBefore;
	protected int m_iAuditBudgetBefore;

	void Init(IA_AiGroup owner)
	{
		m_Owner = owner;
	}

	void ResetQuietPeriod()
	{
		m_ActivityGate.Reset();
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
		SCR_AIGroup group = m_Owner.GetSCR_AIGroup();
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		bool blocked = !m_Owner.IsDynamicAICacheReady();
		foreach (AIAgent member : agents)
		{
			if (!member || !member.GetControlledEntity())
			{
				blocked = true;
				break;
			}
			vector transform[4];
			member.GetControlledEntity().GetWorldTransform(transform);
			if (IA_SpawnPlacement.IsNearAnyPlayer(transform[3], players, IA_DynamicAISpawning.CACHE_DISTANCE_M))
			{
				blocked = true;
				break;
			}
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(member.GetControlledEntity().FindComponent(SCR_AICombatComponent));
			if (combat && combat.GetCurrentTarget() && combat.GetCurrentTarget().GetTimeSinceSeen() < IA_DynamicAISpawning.COMBAT_QUIET_SEC)
			{
				blocked = true;
				break;
			}
		}
		// The addon's engaged-faction flag is historical; recent danger can expire.
		if (!m_ActivityGate.CanCache(blocked, now, m_Owner.GetLastDangerEventTime(), System.GetUnixTime()))
			return false;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!allowCapture || !manager || agents.IsEmpty())
			return false;
		ref array<ref IA_DynamicAIUnit> survivors = {};
		ref array<SCR_CharacterDamageManagerComponent> downed = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
			{
				ResetQuietPeriod();
				return false;
			}
			SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!pawn || pawn.GetParent() || pawn.IsInVehicle() || manager.GetPlayerIdFromControlledEntity(pawn) > 0 || agent.GetPermanentLOD() != -1)
			{
				ResetQuietPeriod();
				return false;
			}
			CharacterControllerComponent controller = pawn.GetCharacterController();
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(pawn.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!controller || !damage)
			{
				ResetQuietPeriod();
				return false;
			}
			// Only the current living roster can become a new spawn record.
			if (controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;
			if (controller.IsUnconscious())
			{
				downed.Insert(damage);
				continue;
			}
			if (!CanRecreateHealthyInfantry(pawn, manager))
			{
				ResetQuietPeriod();
				return false;
			}
			ref IA_DynamicAIUnit record = new IA_DynamicAIUnit();
			if (!record.Capture(pawn))
			{
				ResetQuietPeriod();
				return false;
			}
			if (pawn == group.GetLeaderEntity())
				survivors.InsertAt(record, 0);
			else
				survivors.Insert(record);
		}

		// Publish the logical roster before any member/empty-group callbacks.
		m_aUnits = survivors;
		m_iNextUnit = 0;
		m_bCached = true;
		foreach (SCR_CharacterDamageManagerComponent casualty : downed)
		{
			// Use the original instigator and normal death path exactly once.
			if (casualty)
				casualty.Kill(casualty.GetInstigator());
		}
		if (!IsOwnerLive() || !m_bCached)
			return false;
		foreach (SCR_CharacterDamageManagerComponent checkedCasualty : downed)
		{
			if (!checkedCasualty)
				continue;
			SCR_ChimeraCharacter casualtyPawn = SCR_ChimeraCharacter.Cast(checkedCasualty.GetOwner());
			if (casualtyPawn && casualtyPawn.GetCharacterController() && casualtyPawn.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD)
			{
				// Damage can be disabled externally. Keep every unremoved survivor
				// live rather than dropping a still-living downed unit from accounting.
				m_bCached = false;
				m_aUnits.Clear();
				ResetQuietPeriod();
				return false;
			}
		}
		if (m_aUnits.IsEmpty())
		{
			m_bFinished = true;
			m_bCached = false;
			m_Owner.ResumeAfterDynamicAI();
			return false;
		}
		if (IA_Log.IsDebugEnabled())
		{
			// Weak entity references survive in the array only while the originals exist.
			m_aDeletionAudit = new array<IEntity>();
			foreach (IA_DynamicAIUnit auditUnit : m_aUnits)
			{
				m_aDeletionAudit.Insert(auditUnit.m_Entity);
			}
			m_iAuditActiveBefore = GetActiveAICount();
			m_iAuditBudgetBefore = GetEditorAIBudget();
		}
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
			GetGame().GetCallqueue().CallLater(ReportDeletionAudit, 250, false);
		}
		return true;
	}

	// The cache record is not evidence of physical removal. Check the old entities
	// after deletion callbacks and editor budget updates have had time to run.
	protected void ReportDeletionAudit()
	{
		if (IA_Log.IsDebugEnabled())
		{
			if (m_aDeletionAudit)
			{
				int remaining;
				foreach (IEntity original : m_aDeletionAudit)
				{
					if (original && !original.IsDeleted())
						remaining++;
				}
				Print(string.Format("[IA][DynamicAI] Removal audit: originals=%1 remaining=%2; activeAI=%3->%4; editorAIBudget=%5->%6. Global counts can include concurrent spawns.", m_aDeletionAudit.Count(), remaining, m_iAuditActiveBefore, GetActiveAICount(), m_iAuditBudgetBefore, GetEditorAIBudget()), LogLevel.NORMAL);
			}
		}
		m_aDeletionAudit = null;
	}

	protected int GetActiveAICount()
	{
		AIWorld world = GetGame().GetAIWorld();
		if (!world)
			return -1;
		return world.GetCurrentNumOfActiveAIs();
	}

	protected int GetEditorAIBudget()
	{
		SCR_EditableEntityCore core = SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore));
		SCR_EditableEntityCoreBudgetSetting budget;
		if (!core || !core.GetBudget(EEditableEntityBudget.AI, budget) || !budget)
			return -1;
		return budget.GetCurrentBudget();
	}

	protected bool CanRecreateHealthyInfantry(SCR_ChimeraCharacter pawn, PlayerManager manager)
	{
		if (!pawn || pawn.GetParent() || pawn.IsInVehicle() || manager.GetPlayerIdFromControlledEntity(pawn) > 0)
			return false;
		CharacterControllerComponent controller = pawn.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE || controller.IsUnconscious())
			return false;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(pawn.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return false;
		// Injury/status serialization is intentionally deferred; never heal a survivor
		// merely by recreating its prefab. These teams remain live until recovered.
		ref array<ref SCR_PersistentDamageEffect> effects = {};
		damage.GetPersistentEffects(effects);
		if (!effects.IsEmpty())
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
		ResetQuietPeriod();
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
