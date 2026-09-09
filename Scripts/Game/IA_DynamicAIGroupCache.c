// Repeated cache/wake cycles of the current infantry survivors. The owner stays alive.
class IA_DynamicAIGroupCache
{
	protected IA_AiGroup m_Owner;
	protected ref array<ref IA_DynamicAIUnit> m_aUnits = {};
	protected ref IA_DynamicAIActivityGate m_ActivityGate = new IA_DynamicAIActivityGate();
	protected int m_iNextUnit;
	protected bool m_bCached;
	protected bool m_bWaking;
	protected bool m_bFinished;
	protected bool m_bUrgentWake;
	protected int m_iWakeRequestedMs;
	protected int m_iLastSnapshotMs;
	protected int m_iLastRemovalMs;
	protected ref array<IEntity> m_aDeletionAudit;
	protected int m_iAuditActiveBefore;
	protected int m_iAuditBudgetBefore;
	protected string m_sLiveStatus = "awaiting scan";
	protected string m_sSnapshotBlockReason;

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

	protected bool IsTransactionLive()
	{
		return m_bCached && !m_bFinished && IsOwnerLive();
	}

	string GetDiagnosticStatus()
	{
		if (IsWaking())
			return "restoring";
		if (m_bCached)
			return "cached";
		if (m_sLiveStatus == "quiet period" && m_sSnapshotBlockReason != "")
			return m_sSnapshotBlockReason;
		return m_sLiveStatus;
	}

	protected bool BlockSnapshot(string reason)
	{
		m_sLiveStatus = reason;
		m_sSnapshotBlockReason = reason;
		ResetQuietPeriod();
		return false;
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
		if (!entity)
			return;
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

	void RequestWake(bool urgent = true)
	{
		if (!m_bCached || m_bFinished)
			return;
		if (!m_bWaking)
			m_iWakeRequestedMs = System.GetTickCount();
		m_bWaking = true;
		m_bUrgentWake = m_bUrgentWake || urgent;
		IA_DynamicAISpawning.EnqueueWake(this);
	}

	int GetWakeRequestedMs()
	{
		return m_iWakeRequestedMs;
	}

	bool HasUrgentWake()
	{
		return m_bUrgentWake;
	}

	int GetCandidateSoldierCount()
	{
		if (!IsOwnerLive())
			return 0;
		return m_Owner.GetSCR_AIGroup().GetAgentsCount();
	}

	int GetLastSnapshotMs()
	{
		return m_iLastSnapshotMs;
	}

	int GetLastRemovalMs()
	{
		return m_iLastRemovalMs;
	}

	void CheckWake(array<vector> players, bool enabled)
	{
		if (!m_bCached)
			return;
		if (!enabled)
			RequestWake(false);
		// A background OFF request must still become urgent when players approach.
		if (m_bUrgentWake)
			return;
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
		m_sLiveStatus = m_Owner.GetDynamicAIRoleBlockReason();
		// Most exclusions need no member/component traversal.
		if (m_sLiveStatus != "" || !m_Owner.IsDynamicAICacheReady())
		{
			if (m_sLiveStatus == "")
				m_sLiveStatus = "initialization";
			ResetQuietPeriod();
			return false;
		}
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		bool blocked;
		m_sLiveStatus = "quiet period";
		foreach (AIAgent member : agents)
		{
			if (blocked)
				break;
			if (!member || !member.GetControlledEntity())
			{
				blocked = true;
				m_sLiveStatus = "missing character";
				break;
			}
			vector transform[4];
			member.GetControlledEntity().GetWorldTransform(transform);
			if (IA_SpawnPlacement.IsNearAnyPlayer(transform[3], players, IA_DynamicAISpawning.CACHE_DISTANCE_M))
			{
				blocked = true;
				m_sLiveStatus = "player nearby";
				break;
			}
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(member.GetControlledEntity().FindComponent(SCR_AICombatComponent));
			if (combat && combat.GetCurrentTarget() && combat.GetCurrentTarget().GetTimeSinceSeen() < IA_DynamicAISpawning.COMBAT_QUIET_SEC)
			{
				blocked = true;
				m_sLiveStatus = "recent combat";
				break;
			}
		}
		// The addon's engaged-faction flag is historical; recent danger can expire.
		int lastDanger = m_Owner.GetLastDangerEventTime();
		if (!blocked && lastDanger > 0 && System.GetUnixTime() - lastDanger < IA_DynamicAISpawning.COMBAT_QUIET_SEC)
			m_sLiveStatus = "recent combat";
		if (!m_ActivityGate.CanCache(blocked, now, m_Owner.GetLastDangerEventTime(), System.GetUnixTime()))
			return false;

		m_sLiveStatus = "queued to despawn";
		// The scan queues references only. The worker repeats this entire check
		// with fresh player positions and the current roster before any mutation.
		if (!allowCapture)
			return !agents.IsEmpty();
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager || agents.IsEmpty())
			return false;
		int snapshotStarted;
		if (IA_Log.IsDebugEnabled())
			snapshotStarted = System.GetTickCount();
		m_sSnapshotBlockReason = "";
		ref array<ref IA_DynamicAIUnit> survivors = {};
		ref array<SCR_CharacterDamageManagerComponent> downed = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				return BlockSnapshot("missing character");
			SCR_ChimeraCharacter pawn = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!pawn || pawn.GetParent() || pawn.IsInVehicle() || manager.GetPlayerIdFromControlledEntity(pawn) > 0 || agent.GetPermanentLOD() != -1)
				return BlockSnapshot("ownership, attachment or forced LOD");
			CharacterControllerComponent controller = pawn.GetCharacterController();
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(pawn.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!controller || !damage)
				return BlockSnapshot("missing character state");
			// Only the current living roster can become a new spawn record.
			if (controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;
			if (controller.IsUnconscious())
			{
				downed.Insert(damage);
				continue;
			}
			if (!CanRecreateHealthyInfantry(pawn, manager))
				return BlockSnapshot("injury or status (last snapshot check)");
			ref IA_DynamicAIUnit record = new IA_DynamicAIUnit();
			if (!record.Capture(pawn))
				return BlockSnapshot("prefab unavailable (last snapshot check)");
			if (pawn == group.GetLeaderEntity())
				survivors.InsertAt(record, 0);
			else
				survivors.Insert(record);
		}

		if (IA_Log.IsDebugEnabled())
			m_iLastSnapshotMs = System.GetTickCount() - snapshotStarted;
		// Publish the logical roster before any member/empty-group callbacks.
		m_aUnits = survivors;
		m_iNextUnit = 0;
		m_bCached = true;
		foreach (SCR_CharacterDamageManagerComponent casualty : downed)
		{
			// Use the original instigator and normal death path exactly once.
			if (casualty)
				casualty.Kill(casualty.GetInstigator());
			if (!IsTransactionLive())
				return false;
		}
		if (!IsTransactionLive())
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
				m_bWaking = false;
				m_bUrgentWake = false;
				m_aUnits.Clear();
				return BlockSnapshot("downed death refused (last snapshot check)");
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
		int removalStarted;
		if (IA_Log.IsDebugEnabled())
			removalStarted = System.GetTickCount();
		m_Owner.SuspendForDynamicAI();
		if (!IsTransactionLive())
			return false;
		foreach (IA_DynamicAIUnit unit : m_aUnits)
		{
			IEntity original = unit.m_Entity;
			unit.m_Entity = null;
			RplComponent.DeleteRplEntity(original, false);
			if (!IsTransactionLive())
				return false;
		}
		if (IA_Log.IsDebugEnabled())
		{
			m_iLastRemovalMs = System.GetTickCount() - removalStarted;
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
			// Native spawn/member callbacks may retire and detach the whole roster.
			ref IA_DynamicAIUnit unit = m_aUnits[m_iNextUnit];
			m_iNextUnit++;
			if (unit.m_bRestored || now < unit.m_iNextAttemptMs)
				continue;
			RestoreUnit(unit, now);
			if (IsWaking() && IsTransactionLive() && GetUnrestoredCount() == 0)
				FinishWake();
			return true;
		}
		return false;
	}

	protected void FinishWake()
	{
		m_bCached = false;
		m_bWaking = false;
		m_bUrgentWake = false;
		ResetQuietPeriod();
		m_aUnits.Clear();
		if (IsOwnerLive())
			m_Owner.ResumeAfterDynamicAI();
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
			IEntity spawned = GetGame().SpawnEntityPrefab(unit.m_Resource, GetGame().GetWorld(), params);
			if (!IsWaking() || !IsTransactionLive())
			{
				// Retirement ran before Spawn returned, so it could not see this pawn.
				DeletePendingEntity(spawned);
				return;
			}
			unit.m_Entity = spawned;
			if (!spawned)
			{
				unit.RecordFailure(now);
				return;
			}
			m_Owner.SetupDynamicAIUnit(unit.m_Entity);
			if (!IsWaking() || !IsTransactionLive())
				return;
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
		AIAgent agent = GetRestoreAgent(unit);
		if (!agent)
		{
			unit.RecordFailure(now);
			return;
		}
		AIGroup previousGroup = agent.GetParentGroup();
		if (previousGroup && previousGroup != group)
			previousGroup.RemoveAgent(agent);
		if (!CanContinueAttachment(unit, manager))
			return;
		if (!group || m_Owner.GetSCR_AIGroup() != group)
		{
			unit.RecordFailure(now);
			return;
		}
		agent = GetRestoreAgent(unit);
		if (!agent)
		{
			unit.RecordFailure(now);
			return;
		}
		bool attached = group.AddAIEntityToGroup(unit.m_Entity);
		if (!CanContinueAttachment(unit, manager))
			return;
		if (!group || m_Owner.GetSCR_AIGroup() != group)
		{
			unit.RecordFailure(now);
			return;
		}
		agent = GetRestoreAgent(unit);
		if (!attached || !agent || agent.GetParentGroup() != group)
		{
			unit.RecordFailure(now);
			return;
		}
		unit.m_bRestored = true;
		// Let successfully restored soldiers respond while the remainder is queued.
		group.ActivateAI();
	}

	protected AIAgent GetRestoreAgent(IA_DynamicAIUnit unit)
	{
		if (!unit.m_Entity)
			return null;
		AIControlComponent control = AIControlComponent.Cast(unit.m_Entity.FindComponent(AIControlComponent));
		if (control)
			return control.GetControlAIAgent();
		return null;
	}

	protected bool CanContinueAttachment(IA_DynamicAIUnit unit, PlayerManager manager)
	{
		if (!IsWaking() || !IsTransactionLive() || !unit.m_Entity || unit.m_bRestored)
			return false;
		ChimeraCharacter pawn = ChimeraCharacter.Cast(unit.m_Entity);
		if (pawn && pawn.GetCharacterController() && pawn.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
		{
			unit.m_bDead = true;
			unit.m_bRestored = true;
			return false;
		}
		if (manager && manager.GetPlayerIdFromControlledEntity(unit.m_Entity) > 0)
		{
			unit.m_bRestored = true;
			return false;
		}
		return true;
	}

	protected void DeletePendingEntity(IEntity entity)
	{
		if (!entity || !GetGame())
			return;
		PlayerManager manager = GetGame().GetPlayerManager();
		if (manager && manager.GetPlayerIdFromControlledEntity(entity) <= 0)
			RplComponent.DeleteRplEntity(entity, false);
	}

	void Retire()
	{
		if (m_bFinished)
			return;
		bool wasCached = m_bCached;
		// Latch retirement and detach before the first native callback. Keep the
		// detached records alive while recursive notifications see an empty cache.
		m_bCached = false;
		m_bWaking = false;
		m_bUrgentWake = false;
		m_bFinished = true;
		ref array<ref IA_DynamicAIUnit> retiredUnits = m_aUnits;
		m_aUnits = new array<ref IA_DynamicAIUnit>();
		if (wasCached)
		{
			foreach (IA_DynamicAIUnit unit : retiredUnits)
			{
				// Attached/live soldiers belong to ordinary objective cleanup.
				if (unit.m_bRestored || !unit.m_Entity)
					continue;
				DeletePendingEntity(unit.m_Entity);
			}
		}
	}
}
