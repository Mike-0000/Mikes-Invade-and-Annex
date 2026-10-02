#ifdef WORKBENCH
// Native utility-score regression plus authored defensive-area geometry.
// This does not simulate perception, pathfinding, or firing in a live mission.
[WorkbenchPluginAttribute(name: "IA base garrison regression", wbModules: {"ResourceManager"})]
class IA_BaseGarrisonTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAGarrisonTest");
		CheckPriorities();
		CheckShelterAssignment();
		CheckShelterFootprint(preview.GetRef());
		ref array<ref IA_DynamicSiteLayout> layouts = {};
		layouts.Insert(IA_DynamicSiteLayout.CreateFull());
		layouts.Insert(IA_DynamicSiteLayout.CreateCompact());
		layouts.Insert(IA_DynamicSiteLayout.CreateCourtyard());
		layouts.Insert(IA_DynamicSiteLayout.CreateRoadside());
		layouts.Insert(IA_DynamicSiteLayout.CreateCommandPost());
		layouts.Insert(IA_DynamicSiteLayout.CreateRallyPost());
		foreach (IA_DynamicSiteLayout layout : layouts)
		{
			foreach (vector post : layout.m_aGuardPosts)
			{
				CheckArea(layout, post);
			}
			foreach (vector station : layout.m_aPerimeterStations)
			{
				vector inward = -station;
				inward[1] = 0;
				inward.Normalize();
				CheckArea(layout, station + inward * 6);
			}
			vector center;
			float radius;
			Check(!IA_BaseGarrisonArea.Resolve(layout, Vector(layout.m_fHalfWidthM, 0, 0), center, radius), "outside post rejected");
			Print(string.Format("[IA][GarrisonTest] layout=%1 area checks complete", layout.m_sName), LogLevel.NORMAL);
		}
		Print(string.Format("[IA][GarrisonTest] completed failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Check(bool passed, string label)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][GarrisonTest] FAIL " + label, LogLevel.ERROR);
	}

	protected void CheckPriorities()
	{
		ref SCR_AIDefendBehavior defend = new SCR_AIDefendBehavior(null, null, null, vector.Zero, 180);
		defend.SetPriorityLevel(20);
		float oldScore = defend.Evaluate();
		defend.SetPriorityLevel(IA_AiGroup.WP_PRIORITY_DEFEND_POST);
		float newScore = defend.Evaluate();
		ref SCR_AIActionBase attack = new SCR_AIActionBase();
		attack.SetPriority(SCR_AIActionBase.PRIORITY_BEHAVIOR_ATTACK_NOT_SELECTED);
		ref SCR_AIActionBase observe = new SCR_AIActionBase();
		observe.SetPriority(SCR_AIActionBase.PRIORITY_BEHAVIOR_OBSERVE_THREATS_HIGH_PRIORITY);
		Check(oldScore == 81, "native Evaluate adds waypoint level to defend behavior");
		Check(oldScore > attack.Evaluate() && oldScore > observe.Evaluate(), "reproduce old combat starvation");
		Check(newScore == 61, "defend post uses vanilla normal priority level");
		Check(newScore < attack.Evaluate() && newScore < observe.Evaluate(), "attack and incoming-fire reaction interrupt defend");
		Print(string.Format("[IA][GarrisonTest] scores oldDefend=%1 newDefend=%2 attack=%3 observe=%4", oldScore, newScore, attack.Evaluate(), observe.Evaluate()), LogLevel.NORMAL);
	}

	// Air-raid pairing: each bunker gets its nearest free group, one group per
	// bunker, and a group too far away stays on its own post.
	protected void CheckShelterAssignment()
	{
		array<vector> shelters = {};
		array<vector> groups = {};
		array<int> choice = {};
		shelters.Insert(Vector(0, 0, 0));
		shelters.Insert(Vector(20, 0, 0));
		groups.Insert(Vector(1, 0, 0));
		groups.Insert(Vector(19, 0, 0));
		groups.Insert(Vector(100, 0, 0));
		IA_BaseAirRaidCover.PairNearest(shelters, groups, IA_BaseAirRaidCover.MAX_ASSIGN_DISTANCE_M, choice);
		Check(choice.Count() == 2 && choice[0] == 0 && choice[1] == 1, "each bunker takes its nearest group");

		// Closest pair first: the group at 6 m belongs to the bunker 4 m away,
		// which leaves the bunker at 0 m to the next group rather than empty.
		shelters.Clear();
		groups.Clear();
		shelters.Insert(Vector(0, 0, 0));
		shelters.Insert(Vector(10, 0, 0));
		groups.Insert(Vector(6, 0, 0));
		groups.Insert(Vector(-20, 0, 0));
		IA_BaseAirRaidCover.PairNearest(shelters, groups, IA_BaseAirRaidCover.MAX_ASSIGN_DISTANCE_M, choice);
		Check(choice.Count() == 2 && choice[0] == 1 && choice[1] == 0, "closest pair wins and no group is shared");

		shelters.Clear();
		groups.Clear();
		shelters.Insert(Vector(0, 0, 0));
		groups.Insert(Vector(100, 0, 0));
		IA_BaseAirRaidCover.PairNearest(shelters, groups, IA_BaseAirRaidCover.MAX_ASSIGN_DISTANCE_M, choice);
		Check(choice.Count() == 1 && choice[0] == -1, "distant group keeps its post");
		Print("[IA][GarrisonTest] shelter assignment checks complete", LogLevel.NORMAL);
	}

	// The layout pad must contain the bunker, its net, and every collider child.
	protected void CheckShelterFootprint(BaseWorld world)
	{
		IEntity shelter = GetGame().SpawnEntityPrefab(Resource.Load(IA_DynamicSiteLayout.PREFAB_SHELTER), world);
		Check(shelter != null, "shelter prefab spawns");
		if (!shelter)
			return;
		vector low = "99999 99999 99999";
		vector high = "-99999 -99999 -99999";
		MeasureHierarchy(shelter, low, high);
		int entities = CountHierarchy(shelter);
		array<AISmartActionComponent> actions = {};
		CollectSmartActions(shelter, actions);
		float extent = IA_DynamicSiteLayout.SHELTER_HALF_EXTENT_M;
		Print(string.Format("[IA][GarrisonTest] shelter mins=%1 maxs=%2 entities=%3 smartActions=%4", low, high, entities, actions.Count()), LogLevel.NORMAL);
		Check(low[0] >= -extent && high[0] <= extent && low[2] >= -extent && high[2] <= extent, "shelter hierarchy fits its layout pad");
		Check(high[1] >= 2, "shelter has overhead cover");
		Check(entities <= IA_DynamicSiteLayout.SHELTER_EXPANDED_ENTITIES, "shelter entity estimate");
		Check(actions.Count() >= 3, "shelter provides cover posts");
		SCR_EntityHelper.DeleteEntityAndChildren(shelter);
	}

	protected int CountHierarchy(IEntity ent)
	{
		if (!ent)
			return 0;
		int total = 1;
		IEntity child = ent.GetChildren();
		while (child)
		{
			total += CountHierarchy(child);
			child = child.GetSibling();
		}
		return total;
	}

	protected void CollectSmartActions(IEntity ent, notnull array<AISmartActionComponent> actions)
	{
		if (!ent)
			return;
		array<Managed> found = {};
		ent.FindComponents(AISmartActionComponent, found);
		foreach (Managed component : found)
		{
			AISmartActionComponent action = AISmartActionComponent.Cast(component);
			if (action)
				actions.Insert(action);
		}
		IEntity child = ent.GetChildren();
		while (child)
		{
			CollectSmartActions(child, actions);
			child = child.GetSibling();
		}
	}

	protected void MeasureHierarchy(IEntity ent, inout vector low, inout vector high)
	{
		if (!ent)
			return;
		if (ent.GetVObject())
		{
			vector mins, maxs, mat[4];
			ent.GetBounds(mins, maxs);
			ent.GetWorldTransform(mat);
			for (int i = 0; i < 8; i++)
			{
				vector corner = mins;
				for (int axis = 0; axis < 3; axis++)
				{
					if (i & (1 << axis))
						corner[axis] = maxs[axis];
				}
				vector p = mat[3] + mat[0] * corner[0] + mat[1] * corner[1] + mat[2] * corner[2];
				for (int axis = 0; axis < 3; axis++)
				{
					low[axis] = Math.Min(low[axis], p[axis]);
					high[axis] = Math.Max(high[axis], p[axis]);
				}
			}
		}
		IEntity child = ent.GetChildren();
		while (child)
		{
			MeasureHierarchy(child, low, high);
			child = child.GetSibling();
		}
	}

	protected void CheckArea(IA_DynamicSiteLayout layout, vector post)
	{
		vector center;
		float radius;
		Check(IA_BaseGarrisonArea.Resolve(layout, post, center, radius), "valid post has defensive room");
		Check(center == vector.Zero, "every group shares the base centre");
		float halfWidth = layout.m_fHalfWidthM;
		float halfDepth = layout.m_fHalfDepthM;
		float halfDiagonal = Math.Sqrt(halfWidth * halfWidth + halfDepth * halfDepth);
		Check(Math.AbsFloat(radius - halfDiagonal * 0.85) < 0.001, "defense radius is trimmed by 15 percent");
		Check(radius > 0 && radius < halfDiagonal, "shared area deliberately excludes footprint corners");
		// Authored edge posts remain valid spawns even outside the smaller circle.
		for (int heading = 0; heading < 24; heading++)
		{
			vector origin = Vector(1000, 100, 1000);
			vector rootMat[4];
			layout.BuildRootTransform(origin, heading * 15, rootMat);
			ref IA_DynamicSiteInstance site = IA_DynamicSiteInstance.Create(0, 0, origin, heading * 15, layout, null);
			vector roundTripPost = site.WorldToLocalFlat(layout.LocalOffsetToWorld(rootMat, post));
			vector rotatedCenter;
			float rotatedRadius;
			Check(IA_BaseGarrisonArea.Resolve(layout, roundTripPost, rotatedCenter, rotatedRadius), "rotated post resolves");
			vector flatCenter = center;
			flatCenter[1] = rotatedCenter[1];
			Check(vector.Distance(flatCenter, rotatedCenter) < 0.001 && Math.AbsFloat(radius - rotatedRadius) < 0.001, "rotation preserves defensive area");
		}
	}
}
#endif
