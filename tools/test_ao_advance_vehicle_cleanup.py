"""Empty stolen AO hulls near players must survive SetActiveGroup cleanup.

ForceFinish can leave empty garrison/QRF trucks in m_groupVehicles (vehicle-list
keep, or dead-crew skip of DespawnGroupAndPassengers). SuccessZoneComplete then
CallLater ProceedToNextZone → SetActiveGroup → CleanupInactiveGroupVehicles.
Occupancy-only GC would delete those parked hulls beside the player during RTB.

Run: python -m unittest discover -s tools -p "test_ao_advance_vehicle_cleanup.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class AoAdvanceVehicleCleanupTests(unittest.TestCase):
    def test_inactive_group_cleanup_keeps_occupied_or_near_player_hulls(self):
        body = method(source("IA_VehicleManager.c"), "CleanupInactiveGroupVehicles")
        self.assertIn("IA_SpawnPlacement.CollectPlayerPositions(players)", body)
        self.assertIn("IsVehicleOccupied(vehicle)", body)
        self.assertIn(
            "IA_SpawnPlacement.IsNearAnyPlayer(vehicle.GetOrigin(), players, IA_SpawnPlacement.DESPAWN_PLAYER_SAFE_M)",
            body,
        )
        self.assertLess(
            body.index("IsVehicleOccupied(vehicle)"),
            body.index("DespawnVehicle(vehicle)"),
        )
        self.assertLess(
            body.index("IsNearAnyPlayer(vehicle.GetOrigin(), players, IA_SpawnPlacement.DESPAWN_PLAYER_SAFE_M)"),
            body.index("DespawnVehicle(vehicle)"),
        )

    def test_success_zone_complete_forcefinishes_then_advances(self):
        success = method(source("IA_MissionInitializer.c"), "SuccessZoneComplete")
        self.assertIn("ForceFinishAllCurrentAreaInstances()", success)
        self.assertIn("CallLater(ProceedToNextZone, finalDelay, false)", success)
        self.assertLess(
            success.index("ForceFinishAllCurrentAreaInstances()"),
            success.index("CallLater(ProceedToNextZone, finalDelay, false)"),
        )

    def test_proceed_to_next_zone_triggers_set_active_group(self):
        proceed = method(source("IA_MissionInitializer.c"), "ProceedToNextZone")
        self.assertIn("IA_VehicleManager.SetActiveGroup(currentGroup)", proceed)

    def test_set_active_group_runs_inactive_cleanup(self):
        body = method(source("IA_VehicleManager.c"), "SetActiveGroup")
        self.assertIn("CleanupInactiveGroupVehicles()", body)
        self.assertLess(
            body.index("m_previousActiveGroup = m_currentActiveGroup"),
            body.index("CleanupInactiveGroupVehicles()"),
        )

    def test_qrf_and_garrison_spawns_enter_group_vehicle_list(self):
        mgr = source("IA_VehicleManager.c")
        for name in ("SpawnRandomVehicle", "SpawnVehicle"):
            body = method(mgr, name)
            self.assertIn("m_groupVehicles[m_currentActiveGroup].Insert(entity)", body)
        place = method(mgr, "PlaceUnitsInVehicle")
        self.assertIn("areaInstance.AddMilitaryGroup(crewGroup)", place)
        self.assertNotIn("m_areaVehicles", place)

    def test_dead_crew_forcefinish_skips_vehicle_despawn(self):
        groups = method(source("IA_AreaInstance.c"), "DespawnGroupListAwayFromPlayers")
        dead = groups[groups.index("GetAliveCount() <= 0") :]
        dead = dead[: dead.index("continue;") + len("continue;")]
        self.assertIn("group.Despawn()", dead)
        self.assertNotIn("DespawnGroupAndPassengers", dead)
        self.assertNotIn("DespawnVehicle", dead)


if __name__ == "__main__":
    unittest.main()
