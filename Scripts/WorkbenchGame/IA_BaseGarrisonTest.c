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

	protected void CheckArea(IA_DynamicSiteLayout layout, vector post)
	{
		vector center;
		float radius;
		Check(IA_BaseGarrisonArea.Resolve(layout, post, center, radius), "valid post has defensive room");
		Check(center == vector.Zero, "every group shares the base centre");
		float halfWidth = layout.m_fHalfWidthM;
		float halfDepth = layout.m_fHalfDepthM;
		Check(Math.AbsFloat(radius * radius - halfWidth * halfWidth - halfDepth * halfDepth) < 0.01, "area reaches the whole footprint without extra radius inflation");
		Check(vector.Distance(center, post) < radius, "spawn post lies inside shared area");
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
