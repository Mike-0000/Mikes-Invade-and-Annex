"""Empty stolen AO hulls near players must survive SetActiveGroup cleanup.

ForceFinish keeps empty garrison/QRF trucks within DESPAWN_PLAYER_SAFE_M.
ProceedToNextZone then calls IA_VehicleManager.SetActiveGroup →
CleanupInactiveGroupVehicles. Occupancy-only GC would delete those parked
hulls beside the player during RTB / next-flag advance.

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

    def test_proceed_to_next_zone_triggers_set_active_group(self):
        # AO advance is what flips the inactive group and runs the cleanup.
        proceed = method(source("IA_MissionInitializer.c"), "ProceedToNextZone")
        self.assertIn("IA_VehicleManager.SetActiveGroup(currentGroup)", proceed)

    def test_set_active_group_runs_inactive_cleanup(self):
        body = method(source("IA_VehicleManager.c"), "SetActiveGroup")
        self.assertIn("CleanupInactiveGroupVehicles()", body)
        self.assertLess(
            body.index("m_previousActiveGroup = m_currentActiveGroup"),
            body.index("CleanupInactiveGroupVehicles()"),
        )


if __name__ == "__main__":
    unittest.main()
