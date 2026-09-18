"""Source integration guards; native policy/config checks live in Workbench.

Live doorway/stair navigation still requires a mission playtest.
"""
import unittest

from test_dynamic_base_flow import method, source


class BuildingGarrisonTests(unittest.TestCase):
    def test_entry_cannot_move_pawns_or_fall_back_to_old_approach(self):
        group = source("IA_AI_Group.c")
        movement = source("IA_BuildingGarrison.c")
        for name in ("SetHoldPost", "TickHoldMarch", "EnterHoldPost"):
            movement += method(group, name)
        self.assertNotRegex(movement, r"\.(?:SetOrigin|SetWorldTransform|Teleport|SetVelocity)\s*\(")
        self.assertNotIn("RelocateHoldUnits", group)
        self.assertNotIn("FindHoldApproach", group)
        self.assertNotIn("HOLD_ENTER_M", source("IA_SpawnPlacement.c"))

    def test_every_building_team_uses_the_outdoor_spawn_pipeline(self):
        area = source("IA_AreaInstance.c")
        spawn = method(area, "_SpawnSingleAiGroupAndAddToArea")
        self.assertIn("this, false, false, true, holdAt, holdRadius", spawn)
        group = source("IA_AI_Group.c")
        self.assertIn("FindHoldInfantryOrigin(holdAt)", method(group, "PerformNextRoadSearch"))
        stagger = method(group, "SpawnNextUnit")
        self.assertRegex(stagger, r"else if \(IsBuildingGarrison\(\)\)[\s\S]*?OutdoorOrOrigin\(m_staggeredSpawnPos, buildingScatter\)")

    def test_balanced_assignment_happens_before_area_can_issue_orders(self):
        callback = method(source("IA_AreaInstance.c"), "OnAsyncGroupCreated")
        self.assertIn("if (grp.IsBuildingGarrison())", callback)
        self.assertIn("IA_BuildingGarrison.UsesHold(m_iBuildingGarrisonAssignments)", callback)
        self.assertLess(callback.index("SetHoldAfterBuildingEntry"), callback.index("AddMilitaryGroup(grp)"))

    def test_actual_member_arrival_is_the_only_entry_trigger(self):
        tick = method(source("IA_AI_Group.c"), "TickHoldMarch")
        self.assertRegex(tick, r"if \(!IsDynamicAICached\(\) && IA_BuildingGarrison.HasReachedInterior\([^\n]+\)\)\s*\{[\s\S]*?EnterHoldPost\(\)")
        arrival = method(source("IA_BuildingGarrison.c"), "HasReachedInterior")
        open_post = method(source("IA_BuildingGarrison.c"), "HasReachedOpenPost")
        self.assertIn("foreach (AIAgent agent : agents)", arrival)
        self.assertIn("!NearPost(pos, post, radius)", arrival)
        self.assertIn("!WithinBounds(pos, mins, maxs)", arrival)
        self.assertIn("roof.ExcludeArray = excluded", arrival)
        self.assertIn("if (hit != building)", arrival)
        self.assertIn("return living > 0", arrival)
        self.assertIn("if (!building)", arrival)
        self.assertIn("return HasReachedOpenPost(group, post, radius)", arrival)
        self.assertIn("NearPost(pawn.GetOrigin(), post, radius)", open_post)
        self.assertIn("return living > 0", open_post)

    def test_defend_to_wait_uses_existing_typed_tree_handoff(self):
        group = source("IA_AI_Group.c")
        add = method(group, "AddOrder")
        self.assertLess(add.index("IA_BuildingGarrison.OrderFor"), add.index("ShouldDeferOrderForTypedTree"))
        self.assertIn("IA_BuildingGarrison.ConfigureDefend(postWaypoint)", add)
        entry = method(group, "EnterHoldPost")
        self.assertLess(entry.index("m_bHoldEntered = true"), entry.index("AddOrder"))
        self.assertIn("wantsDefend != isDefend", method(group, "HasHoldWaypoint"))
        self.assertIn("HasHoldWaypoint() || m_typedClearScheduled", method(group, "SetTacticalState"))

    def test_retiring_a_group_cancels_the_arrival_timer(self):
        group = source("IA_AI_Group.c")
        cleanup = method(group, "CancelPendingUnitSpawns")
        self.assertIn("queue.Remove(this.OnHoldMarchTick)", cleanup)
        self.assertIn("UnpinInboundSimulation()", cleanup)
        self.assertIn("m_bSpawnAborted || !m_group", method(group, "ScheduleHoldMarchTick"))


if __name__ == "__main__":
    unittest.main()
