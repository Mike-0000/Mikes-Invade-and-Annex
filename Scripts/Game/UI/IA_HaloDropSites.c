//------------------------------------------------------------------------------------------------
//! I&A named HALO drops: current objective group, plus map location names
//! (church, village, landmark, …) inside a radius around each AO.
//!
//! Consumer: MHJ_DropSiteCatalog.Collect via IA_HaloDropCatalog. Do not call from HUD.
//------------------------------------------------------------------------------------------------
class IA_HaloDropSites
{
	protected static const int MAX_SITES = 14;
	protected static const int MAX_QUERY_HITS = 48;
	protected static const float DEDUP_M = 180;
	protected static const float SAME_POINT_M = 45;
	protected static const float DEFAULT_SEARCH_M = 650;
	protected static const float SEARCH_MIN_M = 500;
	protected static const float SEARCH_MAX_M = 1100;
	protected static const float TASK_MARKER_M = 90;

	protected ref array<IEntity> m_aHits;

	//------------------------------------------------------------------------------------------------
	static void Fill(notnull array<ref MHJ_DropSite> outSites)
	{
		ref IA_HaloDropSites worker = new IA_HaloDropSites();
		worker.FillInternal(outSites);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillInternal(notnull array<ref MHJ_DropSite> outSites)
	{
		ref array<vector> centers = new array<vector>();
		ref array<float> radii = new array<float>();
		ref array<string> labels = new array<string>();
		CollectCenters(centers, radii, labels);

		int centerCount = centers.Count();
		int c;
		for (c = 0; c < centerCount; c++)
		{
			vector origin = centers[c];
			AddUnique(outSites, labels[c], origin[0], origin[2], DEDUP_M);
		}

		for (c = 0; c < centerCount; c++)
			CollectNamedAround(outSites, centers[c], radii[c]);

		CollectFromOpenMap(outSites, centers, radii);

		Print(string.Format("[IA][HALO] Drop sites: %1 (centers %2)", outSites.Count(), centerCount), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectCenters(notnull array<vector> centers, notnull array<float> radii, notnull array<string> labels)
	{
		int group = ResolveActiveGroup();
		array<IA_AreaMarker> markers = {};
		if (group >= 0)
			markers = IA_AreaMarker.GetAreaMarkersByGroup(group);

		int markerCount = 0;
		if (markers)
			markerCount = markers.Count();

		int m;
		for (m = 0; m < markerCount; m++)
		{
			IA_AreaMarker marker = markers[m];
			if (!marker)
				continue;

			string name = marker.GetAreaName();
			if (name.IsEmpty())
				continue;

			vector origin = marker.GetOrigin();
			centers.Insert(origin);
			radii.Insert(SearchRadiusForMarker(marker));
			labels.Insert(name);
		}

		AppendTaskCenters(centers, radii, labels);
	}

	//------------------------------------------------------------------------------------------------
	protected int ResolveActiveGroup()
	{
		if (IA_MissionInitializer.s_instance)
		{
			int fromInit = IA_MissionInitializer.s_instance.GetActiveGroup();
			if (fromInit >= 0)
				return fromInit;
		}

		return IA_Game.GetActiveGroupID();
	}

	//------------------------------------------------------------------------------------------------
	protected float SearchRadiusForMarker(notnull IA_AreaMarker marker)
	{
		float r = marker.GetRadius();
		if (r < 80)
			r = 80;

		float search = r * 1.35 + 280;
		if (search < SEARCH_MIN_M)
			search = SEARCH_MIN_M;
		if (search > SEARCH_MAX_M)
			search = SEARCH_MAX_M;
		return search;
	}

	//------------------------------------------------------------------------------------------------
	protected void AppendTaskCenters(notnull array<vector> centers, notnull array<float> radii, notnull array<string> labels)
	{
		SCR_TaskSystem taskSys = SCR_TaskSystem.GetInstance();
		if (!taskSys)
			return;

		array<SCR_Task> tasks = {};
		taskSys.GetTasks(tasks);
		int taskCount = tasks.Count();
		int t;
		for (t = 0; t < taskCount; t++)
		{
			SCR_Task task = tasks[t];
			if (!IsLiveTask(task))
				continue;

			vector pos = task.GetTaskPosition();
			if (IsCovered(centers, pos, TASK_MARKER_M))
				continue;

			string name = task.GetTaskName();
			if (name.Contains("#"))
				name = WidgetManager.Translate(name);
			if (name.IsEmpty())
				continue;

			centers.Insert(pos);
			radii.Insert(DEFAULT_SEARCH_M);
			labels.Insert(name);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLiveTask(SCR_Task task)
	{
		if (!task)
			return false;

		SCR_ETaskState state = task.GetTaskState();
		if (state == SCR_ETaskState.CREATED)
			return true;
		if (state == SCR_ETaskState.ASSIGNED)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCovered(notnull array<vector> centers, vector pos, float radiusM)
	{
		float lim = radiusM * radiusM;
		int count = centers.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (vector.DistanceSqXZ(centers[i], pos) <= lim)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectNamedAround(notnull array<ref MHJ_DropSite> outSites, vector origin, float radiusM)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_aHits = new array<IEntity>();
		world.QueryEntitiesBySphere(origin, radiusM, OnNamedEntity, FilterNamed, EQueryEntitiesFlags.STATIC);

		float radiusSq = radiusM * radiusM;
		int hitCount = m_aHits.Count();
		int i;
		for (i = 0; i < hitCount; i++)
		{
			IEntity e = m_aHits[i];
			if (!e)
				continue;
			if (vector.DistanceSqXZ(e.GetOrigin(), origin) > radiusSq)
				continue;

			string name = ReadPlaceName(e);
			if (!IsUsableName(name))
				continue;

			vector p = e.GetOrigin();
			AddUnique(outSites, name, p[0], p[2], DEDUP_M);
			if (outSites.Count() >= MAX_SITES)
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool FilterNamed(IEntity e)
	{
		if (!e)
			return false;

		SCR_MapDescriptorComponent desc = SCR_MapDescriptorComponent.Cast(e.FindComponent(SCR_MapDescriptorComponent));
		if (!desc)
			return false;

		return IsNamedDropType(desc.GetBaseType());
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnNamedEntity(IEntity e)
	{
		if (!m_aHits)
			return false;
		if (m_aHits.Count() >= MAX_QUERY_HITS)
			return false;

		m_aHits.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectFromOpenMap(notnull array<ref MHJ_DropSite> outSites, notnull array<vector> centers, notnull array<float> radii)
	{
		if (outSites.Count() >= MAX_SITES)
			return;
		if (centers.IsEmpty())
			return;

		SCR_MapEntity mapEnt = SCR_MapEntity.GetMapInstance();
		if (!mapEnt)
			return;
		if (!mapEnt.IsOpen())
			return;

		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_LOCAL, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_VILLAGE, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_TOWN, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_CITY, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_SETTLEMENT, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_GENERIC, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_NAME_HILL, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_CHURCH, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_CHAPEL, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_LANDMARK, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_HOSPITAL, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_AIRPORT, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_PORT, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_BASE, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_TRANSMITTER, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_RADIO, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_TOWER, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_LIGHTHOUSE, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_MONUMENT, centers, radii);
		AddMapType(outSites, mapEnt, EMapDescriptorType.MDT_RUIN, centers, radii);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMapType(notnull array<ref MHJ_DropSite> outSites, notnull SCR_MapEntity mapEnt, int type, notnull array<vector> centers, notnull array<float> radii)
	{
		if (outSites.Count() >= MAX_SITES)
			return;

		array<MapItem> items = {};
		int count = mapEnt.GetByType(items, type);
		int i;
		for (i = 0; i < count; i++)
		{
			MapItem item = items[i];
			if (!item)
				continue;

			string name = item.GetDisplayName();
			if (name.Contains("#"))
				name = WidgetManager.Translate(name);
			if (!IsUsableName(name))
				continue;

			vector pos = item.GetPos();
			if (!IsInsideAny(centers, radii, pos))
				continue;

			AddUnique(outSites, name, pos[0], pos[2], DEDUP_M);
			if (outSites.Count() >= MAX_SITES)
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInsideAny(notnull array<vector> centers, notnull array<float> radii, vector pos)
	{
		int count = centers.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			float r = radii[i];
			if (vector.DistanceSqXZ(centers[i], pos) <= r * r)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected string ReadPlaceName(notnull IEntity e)
	{
		string name;
		SCR_MapDescriptorComponent desc = SCR_MapDescriptorComponent.Cast(e.FindComponent(SCR_MapDescriptorComponent));
		if (desc)
		{
			MapItem item = desc.Item();
			if (item)
				name = item.GetDisplayName();
		}

		if (name.IsEmpty())
		{
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(e.FindComponent(SCR_EditableEntityComponent));
			if (editable)
				name = editable.GetDisplayName();
		}

		if (name.Contains("#"))
			name = WidgetManager.Translate(name);

		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUsableName(string name)
	{
		if (name.IsEmpty())
			return false;
		if (name.Length() < 2)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddUnique(notnull array<ref MHJ_DropSite> outSites, string name, float x, float z, float dedupM)
	{
		if (outSites.Count() >= MAX_SITES)
			return;
		if (!IsUsableName(name))
			return;

		float sameSq = SAME_POINT_M * SAME_POINT_M;
		float dedupSq = dedupM * dedupM;
		int count = outSites.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			MHJ_DropSite existing = outSites[i];
			if (!existing)
				continue;

			float dx = existing.m_fX - x;
			float dz = existing.m_fZ - z;
			float distSq = dx * dx + dz * dz;
			if (distSq <= sameSq)
				return;
			if (existing.m_sName == name && distSq <= dedupSq)
				return;
		}

		ref MHJ_DropSite site = new MHJ_DropSite();
		site.m_sName = name;
		site.m_fX = x;
		site.m_fZ = z;
		outSites.Insert(site);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsNamedDropType(int type)
	{
		if (type == EMapDescriptorType.MDT_CHURCH)
			return true;
		if (type == EMapDescriptorType.MDT_CHAPEL)
			return true;
		if (type == EMapDescriptorType.MDT_HOSPITAL)
			return true;
		if (type == EMapDescriptorType.MDT_LANDMARK)
			return true;
		if (type == EMapDescriptorType.MDT_AIRPORT)
			return true;
		if (type == EMapDescriptorType.MDT_PORT)
			return true;
		if (type == EMapDescriptorType.MDT_BASE)
			return true;
		if (type == EMapDescriptorType.MDT_TOWER)
			return true;
		if (type == EMapDescriptorType.MDT_VIEWTOWER)
			return true;
		if (type == EMapDescriptorType.MDT_LIGHTHOUSE)
			return true;
		if (type == EMapDescriptorType.MDT_TRANSMITTER)
			return true;
		if (type == EMapDescriptorType.MDT_RADIO)
			return true;
		if (type == EMapDescriptorType.MDT_MONUMENT)
			return true;
		if (type == EMapDescriptorType.MDT_RUIN)
			return true;
		if (type == EMapDescriptorType.MDT_FUELSTATION)
			return true;
		if (type == EMapDescriptorType.MDT_POLICE)
			return true;
		if (type == EMapDescriptorType.MDT_FIREDEP)
			return true;
		if (type == EMapDescriptorType.MDT_HOTEL)
			return true;
		if (type == EMapDescriptorType.MDT_PUB)
			return true;
		if (type == EMapDescriptorType.MDT_VIEWPOINT)
			return true;
		if (type == EMapDescriptorType.MDT_CAVE)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_GENERIC)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_CITY)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_VILLAGE)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_TOWN)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_SETTLEMENT)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_HILL)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_LOCAL)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_ISLAND)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_RIDGE)
			return true;
		if (type == EMapDescriptorType.MDT_NAME_VALLEY)
			return true;
		return false;
	}
}
