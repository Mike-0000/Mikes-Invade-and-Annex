#ifdef WORKBENCH
// Native waypoint configuration and arrival-policy checks. Live paths need a mission.
[WorkbenchPluginAttribute(name: "IA building garrison regression", wbModules: {"ResourceManager"})]
class IA_BuildingGarrisonTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IABuildingGarrisonTest");
		BaseWorld world = preview.GetRef();
		Check(IA_BuildingGarrison.OrderFor(false, false) == IA_AiOrder.DefendSmall, "defend team walks on Defend");
		Check(IA_BuildingGarrison.OrderFor(true, false) == IA_AiOrder.DefendSmall, "hold team walks on Defend");
		Check(IA_BuildingGarrison.OrderFor(false, true) == IA_AiOrder.DefendSmall, "defend team stays Defend");
		Check(IA_BuildingGarrison.OrderFor(true, true) == IA_AiOrder.Hold, "hold only after arrival");
		int holds = 0;
		for (int assignment = 0; assignment < 9; assignment++)
		{
			if (IA_BuildingGarrison.UsesHold(assignment))
				holds++;
			Check(holds == (assignment + 1) / 2, "balanced teams including odd counts");
		}
		vector post = Vector(100, 10, 100);
		Check(IA_BuildingGarrison.NearPost(post + Vector(2, 0, 0), post, 5), "member near interior post");
		Check(!IA_BuildingGarrison.NearPost(post + Vector(0, 3, 0), post, 5), "wrong floor cannot trigger Hold");
		Check(!IA_BuildingGarrison.NearPost(post + Vector(8, 0, 0), post, 5), "outdoor approach distance cannot trigger Hold");
		Check(!IA_BuildingGarrison.WithinBounds(Vector(5, 1, 0), Vector(-5, 0, -5), Vector(5, 6, 5)), "exterior wall is not inside");
		Check(!IA_BuildingGarrison.HasReachedOpenPost(null, post, 5), "open cover posts still require a living roster");
		Check(!IA_BuildingGarrison.HasReachedInterior(null, null, post, 5), "missing group never triggers Hold");

		Resource resource = Resource.Load(IA_AiOrderResource(IA_AiOrder.DefendSmall));
		Check(resource != null, "load stock DefendSmall");
		if (resource)
		{
			SCR_DefendWaypoint waypoint = SCR_DefendWaypoint.Cast(GetGame().SpawnEntityPrefab(resource, world, IA_CreateSimpleSpawnParams(post)));
			Check(waypoint != null, "spawn native Defend waypoint");
			if (waypoint)
			{
				waypoint.SetFastInit(true);
				IA_BuildingGarrison.ConfigureDefend(waypoint);
				Check(!waypoint.GetFastInit(), "explicitly disable vanilla teleport even if prefab enables it");
				SCR_DefendWaypointPreset preset = waypoint.GetCurrentDefendPreset();
				Check(preset != null, "valid Defend preset");
				if (preset)
				{
					Check(!preset.GetUseTurrets(), "building teams stay on foot");
					Check(preset.GetFractionOfSA() == 1.0, "use available building posts");
					ref array<string> tags = {};
					preset.GetTagsForSearch(tags);
					Check(tags.Contains("CoverPost") && tags.Contains("ObservationPost"), "cover and observation posts enabled");
				}
				delete waypoint;
			}
		}
		Print(string.Format("[IA][BuildingGarrisonTest] completed failures=%1", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Check(bool passed, string label)
	{
		if (passed)
			return;
		m_iFailures++;
		Print("[IA][BuildingGarrisonTest] FAIL " + label, LogLevel.ERROR);
	}
}
#endif
