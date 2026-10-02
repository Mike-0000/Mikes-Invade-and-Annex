//------------------------------------------------------------------------------------------------
//! Seize-phase air-raid drill for a dynamic base garrison. While a hostile
//! crewed aircraft is airborne near the base, the nearest garrison group to
//! each camouflaged bunker re-anchors its Defend post on that bunker, so the
//! existing soldiers take its overhead cover posts. Home posts return after
//! the all-clear. Spawns nothing and keeps the normal combat priority, so the
//! sheltered groups still fight infantry that reaches them.
//------------------------------------------------------------------------------------------------
class IA_BaseAirRaidCover
{
	static const float DETECT_RADIUS_M = 1500;
	static const float MIN_AGL_M = 8;
	static const int SCAN_INTERVAL_MS = 2000;
	static const int ALL_CLEAR_MS = 45000;
	static const float SHELTER_RADIUS_M = 6;
	static const float MAX_ASSIGN_DISTANCE_M = 70;

	protected IA_DynamicSiteInstance m_Site;
	protected Faction m_GarrisonFaction;
	protected bool m_bAlert;
	protected bool m_bThreatFound;
	protected vector m_vThreatPos;
	protected int m_iNextScanMs;
	protected int m_iLastThreatMs;

	// One entry per manned bunker. Site shelter and garrison lists only grow
	// until cleanup, so their indices stay stable for the whole Seize phase.
	protected ref array<int> m_aMannedShelter;
	protected ref array<int> m_aMannedGroup;
	protected ref array<vector> m_aHomePost;
	protected ref array<float> m_aHomeRadius;
	// Bunkers whose crew died this raid are not refilled until the all-clear.
	protected ref array<int> m_aCompromised;

	//------------------------------------------------------------------------------------------------
	void IA_BaseAirRaidCover()
	{
		m_aMannedShelter = new array<int>();
		m_aMannedGroup = new array<int>();
		m_aHomePost = new array<vector>();
		m_aHomeRadius = new array<float>();
		m_aCompromised = new array<int>();
	}

	//------------------------------------------------------------------------------------------------
	void Init(IA_DynamicSiteInstance site)
	{
		m_Site = site;
		m_bAlert = false;
		m_iNextScanMs = 0;
		m_iLastThreatMs = 0;
		m_GarrisonFaction = null;
		if (site)
			m_GarrisonFaction = site.GetEnemyFaction();
		if (!m_GarrisonFaction)
		{
			FactionManager factions = GetGame().GetFactionManager();
			if (factions)
				m_GarrisonFaction = factions.GetFactionByKey("USSR");
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsAlert()
	{
		return m_bAlert;
	}

	//------------------------------------------------------------------------------------------------
	int GetMannedCount()
	{
		return m_aMannedGroup.Count();
	}

	//------------------------------------------------------------------------------------------------
	void Tick(int nowMs)
	{
		if (!m_Site)
			return;
		if (m_Site.GetShelterCount() == 0)
			return;
		if (nowMs < m_iNextScanMs)
			return;
		m_iNextScanMs = nowMs + SCAN_INTERVAL_MS;

		if (ScanForAircraft())
		{
			m_iLastThreatMs = nowMs;
			if (!m_bAlert)
			{
				m_bAlert = true;
				int manned = ManShelters();
				IA_Log.Info(string.Format("[IA][Base] Air raid: hostile aircraft at %1, %2 of %3 bunkers manned", m_vThreatPos, manned, m_Site.GetShelterCount()));
				return;
			}
			ManShelters();
			return;
		}

		if (!m_bAlert)
			return;
		if ((nowMs - m_iLastThreatMs) < ALL_CLEAR_MS)
			return;
		int restored = RestoreHomePosts();
		m_bAlert = false;
		m_aCompromised.Clear();
		IA_Log.Info(string.Format("[IA][Base] Air raid all clear: %1 groups back on their posts", restored));
	}

	//------------------------------------------------------------------------------------------------
	//! Seize ended or was abandoned. Sheltered groups resume their home posts.
	void Release()
	{
		int restored = RestoreHomePosts();
		if (m_bAlert)
			IA_Log.Info(string.Format("[IA][Base] Air raid cover released: %1 groups back on their posts", restored));
		m_bAlert = false;
		m_aCompromised.Clear();
		m_Site = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Query callback; returns false to stop at the first threat.
	bool CheckAircraft(IEntity ent)
	{
		if (m_bThreatFound)
			return false;
		Vehicle vehicle = Vehicle.Cast(ent);
		if (!vehicle)
			return true;
		if (!IsHostileAirborne(vehicle))
			return true;
		m_bThreatFound = true;
		m_vThreatPos = vehicle.GetOrigin();
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Greedy closest-pair assignment on the ground plane. outChoice[t] is the
	//! candidate index given to target t, or -1 when none is within maxDistance.
	static void PairNearest(notnull array<vector> targets, notnull array<vector> candidates, float maxDistance, notnull array<int> outChoice)
	{
		outChoice.Clear();
		int targetCount = targets.Count();
		int candidateCount = candidates.Count();
		int t;
		for (t = 0; t < targetCount; t++)
		{
			outChoice.Insert(-1);
		}
		array<bool> used = {};
		int c;
		for (c = 0; c < candidateCount; c++)
		{
			used.Insert(false);
		}

		float maxSq = maxDistance * maxDistance;
		int pairs = targetCount;
		if (candidateCount < pairs)
			pairs = candidateCount;
		int p;
		for (p = 0; p < pairs; p++)
		{
			int bestTarget = -1;
			int bestCandidate = -1;
			float bestSq = maxSq;
			for (t = 0; t < targetCount; t++)
			{
				if (outChoice[t] != -1)
					continue;
				for (c = 0; c < candidateCount; c++)
				{
					if (used[c])
						continue;
					float sq = vector.DistanceSqXZ(targets[t], candidates[c]);
					if (sq > bestSq)
						continue;
					bestSq = sq;
					bestTarget = t;
					bestCandidate = c;
				}
			}
			if (bestTarget == -1)
				return;
			outChoice[bestTarget] = bestCandidate;
			used[bestCandidate] = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool ScanForAircraft()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		m_bThreatFound = false;
		world.QueryEntitiesBySphere(m_Site.GetOrigin(), DETECT_RADIUS_M, this.CheckAircraft, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.NO_PROXIES);
		return m_bThreatFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHostileAirborne(notnull Vehicle vehicle)
	{
		if (!IsAircraft(vehicle))
			return false;
		if (!IsAirborne(vehicle.GetOrigin()))
			return false;
		SCR_ChimeraCharacter pilot = IA_GetPilotForVehicle(vehicle);
		if (!IA_BasePlayerSampler.IsLivingConsciousPawn(pilot))
			return false;
		return IsHostileToGarrison(pilot);
	}

	//------------------------------------------------------------------------------------------------
	//! Modded aircraft without an AI usage type are still caught by their helicopter controller.
	protected bool IsAircraft(notnull Vehicle vehicle)
	{
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (usage)
		{
			EAIVehicleType type = usage.GetVehicleType();
			if (type == EAIVehicleType.AIRCRAFT_HELICOPTER)
				return true;
			if (type == EAIVehicleType.AIRCRAFT_PLANE)
				return true;
		}
		return vehicle.FindComponent(HelicopterControllerComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! A parked or landing aircraft is not an air raid.
	protected bool IsAirborne(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		float ground = world.GetSurfaceY(pos[0], pos[2]);
		if (world.IsOcean())
			ground = Math.Max(ground, world.GetOceanHeight(pos[0], pos[2]));
		return (pos[1] - ground) > MIN_AGL_M;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHostileToGarrison(notnull IEntity pilot)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(pilot.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			return false;
		if (!m_GarrisonFaction)
			return faction.GetFactionKey() == "US";
		if (faction == m_GarrisonFaction)
			return false;
		return faction.IsFactionEnemy(m_GarrisonFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! Drop dead groups and lost bunkers, then pair empty bunkers with the
	//! nearest free living groups. Returns the number of manned bunkers.
	protected int ManShelters()
	{
		PruneManned();

		array<int> openShelters = {};
		array<vector> shelterPositions = {};
		int shelterCount = m_Site.GetShelterCount();
		int s;
		for (s = 0; s < shelterCount; s++)
		{
			IEntity shelter = m_Site.GetShelter(s);
			if (!shelter)
				continue;
			if (m_aMannedShelter.Find(s) != -1)
				continue;
			if (m_aCompromised.Find(s) != -1)
				continue;
			openShelters.Insert(s);
			shelterPositions.Insert(shelter.GetOrigin());
		}

		array<int> freeGroups = {};
		array<vector> groupPositions = {};
		int groupCount = m_Site.GetGarrisonCount();
		int g;
		for (g = 0; g < groupCount; g++)
		{
			if (m_aMannedGroup.Find(g) != -1)
				continue;
			IA_AiGroup group = m_Site.GetGarrisonGroup(g);
			if (!IsFreeDefender(group))
				continue;
			vector groupPos = group.GetOrigin();
			if (groupPos == vector.Zero)
				continue;
			freeGroups.Insert(g);
			groupPositions.Insert(groupPos);
		}

		array<int> choice = {};
		PairNearest(shelterPositions, groupPositions, MAX_ASSIGN_DISTANCE_M, choice);
		int openCount = openShelters.Count();
		int o;
		for (o = 0; o < openCount; o++)
		{
			if (choice[o] == -1)
				continue;
			ManShelter(openShelters[o], freeGroups[choice[o]]);
		}
		return m_aMannedGroup.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected void ManShelter(int shelterIndex, int groupIndex)
	{
		IEntity shelter = m_Site.GetShelter(shelterIndex);
		IA_AiGroup group = m_Site.GetGarrisonGroup(groupIndex);
		if (!shelter || !group)
			return;

		vector home = group.GetHoldPost();
		float homeRadius = group.GetHoldRadius();
		FreeShelterPosts(shelter, groupIndex);
		if (!group.RepinDefendPost(shelter.GetOrigin(), SHELTER_RADIUS_M))
			return;
		m_aMannedShelter.Insert(shelterIndex);
		m_aMannedGroup.Insert(groupIndex);
		m_aHomePost.Insert(home);
		m_aHomeRadius.Insert(homeRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected void PruneManned()
	{
		int i;
		for (i = m_aMannedGroup.Count() - 1; i >= 0; i--)
		{
			IA_AiGroup group = m_Site.GetGarrisonGroup(m_aMannedGroup[i]);
			if (!IsUsableGroup(group))
			{
				m_aCompromised.Insert(m_aMannedShelter[i]);
				RemoveManned(i);
				continue;
			}
			if (m_Site.GetShelter(m_aMannedShelter[i]))
				continue;
			group.RepinDefendPost(m_aHomePost[i], m_aHomeRadius[i]);
			RemoveManned(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int RestoreHomePosts()
	{
		int restored = 0;
		int i;
		for (i = m_aMannedGroup.Count() - 1; i >= 0; i--)
		{
			IA_AiGroup group = null;
			if (m_Site)
				group = m_Site.GetGarrisonGroup(m_aMannedGroup[i]);
			if (IsUsableGroup(group))
			{
				if (group.RepinDefendPost(m_aHomePost[i], m_aHomeRadius[i]))
					restored++;
			}
			RemoveManned(i);
		}
		return restored;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveManned(int index)
	{
		m_aMannedShelter.Remove(index);
		m_aMannedGroup.Remove(index);
		m_aHomePost.Remove(index);
		m_aHomeRadius.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUsableGroup(IA_AiGroup group)
	{
		if (!group)
			return false;
		if (!group.IsSpawned())
			return false;
		if (!group.GetSCR_AIGroup())
			return false;
		return group.GetAliveCount() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Gun and mortar crews stay on their weapons, and cached groups have no
	//! soldiers to shelter.
	protected bool IsFreeDefender(IA_AiGroup group)
	{
		if (!IsUsableGroup(group))
			return false;
		if (!group.IsDefendPost() || group.HasStaticGunAssignment() || group.IsMortarCrew())
			return false;
		if (group.IsDynamicAICached() || group.IsDynamicAIPaused())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Garrison Defend searches span the whole base, and vanilla keeps every
	//! cover post a group finds locked until that group re-plans. Hand this
	//! bunker's idle posts back so the sheltering group's search can claim them.
	protected void FreeShelterPosts(notnull IEntity shelter, int keeperIndex)
	{
		int groupCount = m_Site.GetGarrisonCount();
		int g;
		for (g = 0; g < groupCount; g++)
		{
			if (g == keeperIndex)
				continue;
			IA_AiGroup group = m_Site.GetGarrisonGroup(g);
			if (!group)
				continue;
			SCR_AIGroup aiGroup = group.GetSCR_AIGroup();
			if (!aiGroup)
				continue;
			array<AISmartActionComponent> allocated;
			aiGroup.GetAllocatedSmartActions(allocated);
			if (!allocated)
				continue;
			// The getter returns the group's own list; release from a copy.
			array<AISmartActionComponent> snapshot = {};
			snapshot.Copy(allocated);
			foreach (AISmartActionComponent action : snapshot)
			{
				if (!action || action.GetUser())
					continue;
				if (!IsUnder(action.GetOwner(), shelter))
					continue;
				aiGroup.ReleaseSmartAction(action);
				action.SetActionAccessible(true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUnder(IEntity ent, IEntity root)
	{
		while (ent)
		{
			if (ent == root)
				return true;
			ent = ent.GetParent();
		}
		return false;
	}
}
