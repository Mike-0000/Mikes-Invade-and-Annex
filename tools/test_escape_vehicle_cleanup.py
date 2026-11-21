"""Stolen assassination escape hulls near players must survive objective end.

Escape vehicles are raw SpawnEntityPrefab (not VehicleManager / m_areaVehicles).
Complete and _CleanupObjective used to AddEntityToGc unconditionally, so a
player who stole the empty truck, parked it at HQ, and got out lost it the
moment the HVT died. Occupied deletes are the same site (#31); empty-near-player
matches ForceFinish / SetActiveGroup keep policy (#39 / #40).

Run: python -m unittest discover -s tools -p "test_escape_vehicle_cleanup.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class EscapeVehicleCleanupTests(unittest.TestCase):
    def test_discard_keeps_occupied_or_near_player_before_gc(self):
        body = method(source("IA_SideObjective.c"), "DiscardEscapeVehicle")
        self.assertIn("IA_VehicleManager.IsVehicleOccupied(veh)", body)
        self.assertIn("IA_SpawnPlacement.CollectPlayerPositions(players)", body)
        self.assertIn(
            "IA_SpawnPlacement.IsNearAnyPlayer(veh.GetOrigin(), players, IA_SpawnPlacement.DESPAWN_PLAYER_SAFE_M)",
            body,
        )
        self.assertLess(
            body.index("IsVehicleOccupied(veh)"),
            body.index("AddEntityToGc(m_EscapeVehicle)"),
        )
        self.assertLess(
            body.index("IsNearAnyPlayer(veh.GetOrigin(), players, IA_SpawnPlacement.DESPAWN_PLAYER_SAFE_M)"),
            body.index("AddEntityToGc(m_EscapeVehicle)"),
        )

    def test_complete_and_cleanup_use_discard(self):
        side = source("IA_SideObjective.c")
        # Base IA_SideObjective also has Complete(); target the assassination override.
        assassinate = side[side.index("class IA_AssassinationObjective") :]
        complete = method(assassinate, "Complete")
        cleanup = method(assassinate, "_CleanupObjective")
        self.assertIn("DiscardEscapeVehicle()", complete)
        self.assertIn("DiscardEscapeVehicle()", cleanup)
        self.assertNotIn("AddEntityToGc(m_EscapeVehicle)", complete)
        self.assertNotIn("AddEntityToGc(m_EscapeVehicle)", cleanup)

    def test_escape_spawn_bypasses_vehicle_manager_lists(self):
        body = method(source("IA_SideObjective.c"), "SpawnEscapeVehicleAt")
        self.assertIn("SpawnEntityPrefab(vehRes", body)
        self.assertNotIn("IA_VehicleManager.Spawn", body)
        self.assertNotIn("m_areaVehicles", body)
        self.assertNotIn("PlaceUnitsInVehicle", body)


if __name__ == "__main__":
    unittest.main()
