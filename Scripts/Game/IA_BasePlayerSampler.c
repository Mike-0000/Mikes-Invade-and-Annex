//------------------------------------------------------------------------------------------------
//! Eligible-player roster and command-zone sampling for BaseAssault.
//! Does not change ordinary IA_AreaMarker capture scoring.
//------------------------------------------------------------------------------------------------
class IA_BasePlayerSampler
{
	protected static const int DEPLOY_RECORD_INTERVAL_SEC = 5;
	protected static const int DEPLOY_WINDOW_SEC = 600;
	protected static const int CASUALTY_GRACE_SEC = 120;
	protected static const float GROUND_SLACK_M = 3.0;

	protected int m_iGroupId = -1;
	protected int m_iLastRecordUnix;
	protected ref map<int, ref IA_BaseRosterEntry> m_Seen;
	protected ref array<ref IA_BaseRosterEntry> m_Frozen;
	protected int m_iInitialTarget;

	//------------------------------------------------------------------------------------------------
	void IA_BasePlayerSampler()
	{
		m_Seen = new map<int, ref IA_BaseRosterEntry>();
		m_Frozen = new array<ref IA_BaseRosterEntry>();
	}

	//------------------------------------------------------------------------------------------------
	void BeginAo(int groupId)
	{
		m_iGroupId = groupId;
		m_iLastRecordUnix = 0;
		m_Seen.Clear();
		m_Frozen.Clear();
		m_iInitialTarget = 0;
	}

	//------------------------------------------------------------------------------------------------
	void TickRecord(int nowUnix, vector assemblyCenter, float assemblyRadius)
	{
		if (nowUnix - m_iLastRecordUnix < DEPLOY_RECORD_INTERVAL_SEC)
			return;
		m_iLastRecordUnix = nowUnix;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		int count = ids.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			int playerId = ids[i];
			IEntity pawn = pm.GetPlayerControlledEntity(playerId);
			if (!pawn)
				continue;
			if (!IsFriendlyPlayerPawn(pawn))
				continue;

			vector pos;
			if (!IA_AreaMarker.TryGetPawnWorldPos(pawn, pos))
				continue;

			bool inAo = IsInsideApprovedAo(pos);
			bool inAssembly = HorizontalInside(pos, assemblyCenter, assemblyRadius);
			if (!inAo && !inAssembly)
				continue;

			IA_BaseRosterEntry entry = m_Seen.Get(playerId);
			if (!entry)
			{
				ref IA_BaseRosterEntry created = new IA_BaseRosterEntry();
				created.m_iPlayerId = playerId;
				created.m_sIdentity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
				created.m_bEligible = true;
				m_Seen.Set(playerId, created);
				entry = created;
			}
			entry.m_iLastSeenUnix = nowUnix;
		}
	}

	//------------------------------------------------------------------------------------------------
	void FreezeAtCapture(vector assemblyCenter, float assemblyRadius)
	{
		m_Frozen.Clear();
		int nowUnix = System.GetUnixTime();

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		int count = ids.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			int playerId = ids[i];
			IEntity pawn = pm.GetPlayerControlledEntity(playerId);
			if (!pawn)
				continue;
			if (!IsFriendlyPlayerPawn(pawn))
				continue;

			string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			IA_BaseRosterEntry seen = m_Seen.Get(playerId);
			bool recent = false;
			if (seen && (nowUnix - seen.m_iLastSeenUnix) <= DEPLOY_WINDOW_SEC)
				recent = true;

			vector pos;
			bool atBase = false;
			if (IA_AreaMarker.TryGetPawnWorldPos(pawn, pos))
				atBase = HorizontalInside(pos, assemblyCenter, assemblyRadius);

			if (!recent && !atBase)
				continue;

			ref IA_BaseRosterEntry frozen = new IA_BaseRosterEntry();
			frozen.m_iPlayerId = playerId;
			frozen.m_sIdentity = identity;
			frozen.m_iLastSeenUnix = nowUnix;
			frozen.m_bEligible = true;
			m_Frozen.Insert(frozen);
		}

		int eligible = CountEligibleNow(nowUnix);
		float frac = 0.60;
		IA_Config cfg = IA_MissionInitializer.GetGlobalConfig();
		if (cfg)
			frac = cfg.m_fDynamicBaseRegroupFraction;
		int target = Math.Ceil(eligible * frac);
		if (target < 1)
			target = 1;
		m_iInitialTarget = target;
	}

	//------------------------------------------------------------------------------------------------
	int GetInitialTarget()
	{
		return m_iInitialTarget;
	}

	//------------------------------------------------------------------------------------------------
	int CountEligibleNow(int nowUnix)
	{
		int eligible = 0;
		int count = m_Frozen.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_BaseRosterEntry entry = m_Frozen[i];
			if (!entry)
				continue;
			RefreshEligibility(entry, nowUnix);
			if (entry.m_bEligible)
				eligible = eligible + 1;
		}
		return eligible;
	}

	//------------------------------------------------------------------------------------------------
	int CountEligiblePresent(vector center, float radius)
	{
		int present = 0;
		int count = m_Frozen.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_BaseRosterEntry entry = m_Frozen[i];
			if (!entry || !entry.m_bEligible)
				continue;
			if (IsPlayerGroundedPresent(entry.m_iPlayerId, center, radius))
				present = present + 1;
		}
		return present;
	}

	//------------------------------------------------------------------------------------------------
	int CountAllGroundedPresent(vector center, float radius)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return 0;

		array<int> ids = {};
		pm.GetPlayers(ids);
		int present = 0;
		int count = ids.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (IsPlayerGroundedPresent(ids[i], center, radius))
				present = present + 1;
		}
		return present;
	}

	//------------------------------------------------------------------------------------------------
	bool HasLivingFriendlyInZone(vector center, float radius)
	{
		return CountAllGroundedPresent(center, radius) > 0;
	}

	//------------------------------------------------------------------------------------------------
	bool HasHostileInZone(vector center, float radius)
	{
		return QueryHostiles(center, radius);
	}

	protected bool m_bHostileHit;
	protected vector m_vQueryCenter;
	protected float m_fQueryRadius;

	//------------------------------------------------------------------------------------------------
	bool CollectHostile(IEntity ent)
	{
		if (m_bHostileHit)
			return false;
		if (!IsEffectiveHostile(ent, m_vQueryCenter, m_fQueryRadius))
			return true;
		m_bHostileHit = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool QueryHostiles(vector center, float radius)
	{
		m_bHostileHit = false;
		m_vQueryCenter = center;
		m_fQueryRadius = radius;
		World world = GetGame().GetWorld();
		if (!world)
			return false;
		world.QueryEntitiesBySphere(center, radius + 8, this.CollectHostile, null, EQueryEntitiesFlags.ALL);
		return m_bHostileHit;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshEligibility(notnull IA_BaseRosterEntry entry, int nowUnix)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
		{
			entry.m_bEligible = false;
			return;
		}

		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.Find(entry.m_iPlayerId) == -1)
		{
			entry.m_bEligible = false;
			return;
		}

		string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(entry.m_iPlayerId);
		if (!entry.m_sIdentity.IsEmpty() && identity != entry.m_sIdentity)
		{
			entry.m_bEligible = false;
			return;
		}

		IEntity pawn = pm.GetPlayerControlledEntity(entry.m_iPlayerId);
		if (IsLivingConsciousPawn(pawn))
		{
			entry.m_iGraceStartUnix = 0;
			entry.m_bEligible = true;
			return;
		}

		if (entry.m_iGraceStartUnix <= 0)
			entry.m_iGraceStartUnix = nowUnix;
		if ((nowUnix - entry.m_iGraceStartUnix) >= CASUALTY_GRACE_SEC)
			entry.m_bEligible = false;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlayerGroundedPresent(int playerId, vector center, float radius)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		IEntity pawn = pm.GetPlayerControlledEntity(playerId);
		if (!IsLivingConsciousPawn(pawn))
			return false;
		if (!IsFriendlyPlayerPawn(pawn))
			return false;
		if (IsInAircraft(pawn))
			return false;

		vector pos;
		if (!IA_AreaMarker.TryGetPawnWorldPos(pawn, pos))
			return false;
		if (!HorizontalInside(pos, center, radius))
			return false;

		float supportY = SampleSupportY(pos);
		if ((pos[1] - supportY) > GROUND_SLACK_M)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool HorizontalInside(vector pos, vector center, float radius)
	{
		if (radius <= 0)
			return false;
		float dx = pos[0] - center[0];
		float dz = pos[2] - center[2];
		return (dx * dx + dz * dz) <= radius * radius;
	}

	//------------------------------------------------------------------------------------------------
	static float SampleSupportY(vector pos)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return pos[1];

		float surface = world.GetSurfaceY(pos[0], pos[2]);
		ref TraceParam p = new TraceParam();
		p.Start = Vector(pos[0], pos[1] + 8, pos[2]);
		p.End = Vector(pos[0], surface - 4, pos[2]);
		p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float hit = world.TraceMove(p, null);
		if (hit < 1)
		{
			vector hitPos = p.Start + ((p.End - p.Start) * hit);
			return hitPos[1];
		}
		return surface;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLivingConsciousPawn(IEntity pawn)
	{
		if (!pawn)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pawn);
		if (!character)
			return false;

		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmg && dmg.IsDestroyed())
			return false;

		CharacterControllerComponent ctrl = character.GetCharacterController();
		if (ctrl)
		{
			if (ctrl.GetLifeState() != ECharacterLifeState.ALIVE)
				return false;
			SCR_CharacterControllerComponent scrCtrl = SCR_CharacterControllerComponent.Cast(ctrl);
			if (scrCtrl && scrCtrl.IsUnconscious())
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsFriendlyPlayerPawn(IEntity pawn)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pawn);
		if (!character)
			return false;

		int playerId = 0;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			playerId = pm.GetPlayerIdFromControlledEntity(pawn);
		if (playerId <= 0)
			return false;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return true;
		Faction faction = fac.GetAffiliatedFaction();
		if (!faction)
			return true;
		return faction.GetFactionKey() == "US";
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInAircraft(IEntity pawn)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pawn);
		if (!character)
			return false;

		Vehicle veh = IA_GetVehicleForChimeraCharacter(character);
		if (!veh)
			return false;

		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(veh.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage)
			return false;
		EAIVehicleType t = usage.GetVehicleType();
		if (t == EAIVehicleType.AIRCRAFT_HELICOPTER)
			return true;
		if (t == EAIVehicleType.AIRCRAFT_PLANE)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEffectiveHostile(IEntity ent, vector center, float radius)
	{
		if (!ent)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(ent);
		if (!character)
			return false;
		if (!IsLivingConsciousPawn(character))
			return false;

		int playerId = 0;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			playerId = pm.GetPlayerIdFromControlledEntity(character);
		if (playerId > 0)
			return false;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return false;
		Faction faction = fac.GetAffiliatedFaction();
		if (!faction)
			return false;

		FactionManager fm = GetGame().GetFactionManager();
		if (!fm)
			return false;
		Faction us = fm.GetFactionByKey("US");
		if (!us)
			return false;
		if (!faction.IsFactionEnemy(us))
			return false;

		vector pos;
		if (!IA_AreaMarker.TryGetPawnWorldPos(character, pos))
			return false;
		return HorizontalInside(pos, center, radius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInsideApprovedAo(vector pos)
	{
		array<IA_AreaMarker> markers = IA_AreaMarker.GetAllMarkers();
		if (!markers)
			return false;

		int count = markers.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			if (marker.m_areaGroup != m_iGroupId)
				continue;
			IA_AreaType t = marker.GetAreaType();
			if (t == IA_AreaType.DefendObjective)
				continue;
			if (t == IA_AreaType.MortarPit)
				continue;
			if (t == IA_AreaType.RadioTower)
				continue;
			if (IA_AreaMarker.IsWorldPosInsideCaptureRadius(pos, marker.GetOrigin(), marker.GetRadius() + 350))
				return true;
		}
		return false;
	}
}
