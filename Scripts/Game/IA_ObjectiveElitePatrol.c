//------------------------------------------------------------------------------------------------
//! Silent special-forces squad on a capture AO. Same OPFOR infantry map pin as the
//! Enhanced Defense elite event, with no task toast. Patrols until gunfire, then hunts.
//------------------------------------------------------------------------------------------------
class IA_ObjectiveElitePatrol
{
	protected static const int UNIT_COUNT = 8;
	protected static const int MARKER_STALE_MS = 10000;
	protected static const int MARKER_LIVE_MS = 8000;
	protected static const string MARKER_TITLE = "Hunt the elite patrol";

	protected IA_AreaInstance m_Host;
	protected IA_AiGroup m_Group;
	protected ref SCR_MapMarkerBase m_MapMarker;
	protected vector m_vSite;
	protected vector m_vApprox;
	protected vector m_vMarkerPos;
	protected int m_iLastMarkerTick;
	protected bool m_bEngaged;
	protected bool m_bUnitsSeen;
	protected bool m_bDone;

	//------------------------------------------------------------------------------------------------
	static bool CanHost(IA_AreaType areaType)
	{
		if (areaType == IA_AreaType.MortarPit)
			return false;
		if (areaType == IA_AreaType.DefendObjective)
			return false;
		if (areaType == IA_AreaType.Assassination)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static IA_ObjectiveElitePatrol Create(notnull IA_AreaInstance host)
	{
		ref IA_ObjectiveElitePatrol patrol = new IA_ObjectiveElitePatrol();
		if (!patrol.Spawn(host))
			return null;
		return patrol;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Spawn(notnull IA_AreaInstance host)
	{
		IA_Area area = host.GetArea();
		if (!area)
			return false;
		if (!CanHost(area.GetAreaType()))
			return false;

		vector center = area.GetOrigin();
		m_vSite = IA_SpawnPlacement.FindEventSite(center, 350, 600);
		if (m_vSite == vector.Zero)
			m_vSite = IA_SpawnPlacement.FindInboundInfantrySpawn(center, -1);
		if (m_vSite == vector.Zero)
			m_vSite = center + Vector(400, 0, 0);

		IA_AiGroup grp = IA_AiGroup.CreateMilitaryGroupFromUnits(m_vSite, host.GetOwningFaction(), UNIT_COUNT, host.GetAreaFaction(), false, true, false, true);
		if (!grp)
			return false;

		grp.SetEliteProfile(true);
		grp.SetAssignedArea(area);
		grp.Spawn();
		host.AddMilitaryGroup(grp);
		grp.StartSweepPatrol(center, 80, 160, 220, 380);
		GetGame().GetCallqueue().CallLater(grp.ApplyEliteCombatProfile, 2000, false);

		m_Host = host;
		m_Group = grp;
		m_vApprox = Jitter(m_vSite, 80, 150);
		SpawnMapMarker(m_vApprox);

		Print(string.Format("[IA][ElitePatrol] occupying patrol at %1 for %2", m_vSite.ToString(), area.GetName()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Update()
	{
		if (m_bDone)
			return;
		if (!m_Group)
		{
			Finish();
			return;
		}

		NoteUnitsSeen();
		if (m_bUnitsSeen && m_Group.IsSpawned() && m_Group.GetAliveCount() <= 0)
		{
			Finish();
			return;
		}

		if (!m_bEngaged)
		{
			int danger = m_Group.GetLastDangerEventTime();
			if (danger > 0 && (System.GetUnixTime() - danger) < 20)
			{
				m_bEngaged = true;
				vector hunt = vector.Zero;
				if (m_Host)
				{
					IA_Area area = m_Host.GetArea();
					if (area)
						hunt = area.GetOrigin();
				}
				if (hunt != vector.Zero)
					m_Group.SetDefendMode(true, hunt);
			}
		}

		UpdateMarker(System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		RemoveMapMarker();
		m_bDone = true;
		m_Group = null;
		m_Host = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		RemoveMapMarker();
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void NoteUnitsSeen()
	{
		if (m_bUnitsSeen)
			return;
		if (!m_Group)
			return;
		if (!m_Group.IsSpawned())
			return;
		if (m_Group.GetAliveCount() <= 0)
			return;
		m_bUnitsSeen = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateMarker(int now)
	{
		bool exact = m_bEngaged;
		int interval = MARKER_STALE_MS;
		if (exact)
			interval = MARKER_LIVE_MS;
		if (m_iLastMarkerTick != 0 && (now - m_iLastMarkerTick) < interval)
			return;
		m_iLastMarkerTick = now;

		vector pos = m_vApprox;
		if (exact && m_Group)
		{
			vector live = m_Group.GetOrigin();
			if (live != vector.Zero)
				pos = live;
			else
				pos = m_vSite;
		}

		if (pos == vector.Zero)
			return;
		if (m_MapMarker && vector.Distance(pos, m_vMarkerPos) < 25)
			return;

		SpawnMapMarker(pos);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnMapMarker(vector origin)
	{
		if (origin == vector.Zero)
			return;

		RemoveMapMarker();

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr)
			return;

		ref SCR_MapMarkerBase marker = mgr.PrepareMilitaryMarker(EMilitarySymbolIdentity.OPFOR, EMilitarySymbolDimension.LAND, EMilitarySymbolIcon.INFANTRY);
		if (!marker)
		{
			marker = new SCR_MapMarkerBase();
			marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		}

		int worldX = origin[0];
		int worldZ = origin[2];
		marker.SetWorldPos(worldX, worldZ);
		marker.SetCustomText(MARKER_TITLE);
		marker.SetCanBeRemovedByOwner(false);
		mgr.InsertStaticMarker(marker, false, true);
		m_MapMarker = marker;
		m_vMarkerPos = origin;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMapMarker()
	{
		if (!m_MapMarker)
			return;

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (mgr)
			mgr.RemoveStaticMarker(m_MapMarker);

		m_MapMarker = null;
	}

	//------------------------------------------------------------------------------------------------
	protected vector Jitter(vector center, float minR, float maxR)
	{
		float angle = IA_Game.rng.RandFloat01() * Math.PI2;
		float dist = IA_Game.rng.RandFloatXY(minR, maxR);
		vector outPos;
		outPos[0] = center[0] + Math.Cos(angle) * dist;
		outPos[2] = center[2] + Math.Sin(angle) * dist;
		outPos[1] = GetGame().GetWorld().GetSurfaceY(outPos[0], outPos[2]);
		return outPos;
	}
}
