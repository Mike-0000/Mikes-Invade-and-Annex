"""Stolen AO/QRF hulls near players must survive ForceFinish group cleanup.

QRF trucks are never registered in m_areaVehicles, so DespawnGroupAndPassengers
is their only GC path. Dumped crews keep IsDriving with the hull while on foot
at the old AO; AI origin can be >600 m away while the stolen truck sits with
the player.

Run: python -m unittest discover -s tools -p "test_stolen_vehicle_forcefinish.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class StolenVehicleForceFinishTests(unittest.TestCase):
    def test_group_cleanup_keeps_occupied_or_near_player_hulls(self):
        area = source("IA_AreaInstance.c")
        self.assertRegex(
            area,
            r"DespawnGroupAndPassengers\(IA_AiGroup group,\s*array<vector> players,\s*bool checkPlayers\)",
        )
        body = method(area, "DespawnGroupAndPassengers")
        self.assertIn("IA_VehicleManager.IsVehicleOccupied(vehicle)", body)
        self.assertIn("ShouldKeepNearPlayers(vehicle.GetOrigin(), players)", body)
        self.assertIn("m_areaVehicles.Insert(vehicle)", body)
        self.assertLess(
            body.index("IsVehicleOccupied(vehicle)"),
            body.index("IA_VehicleManager.DespawnVehicle(vehicle)"),
        )
        self.assertLess(
            body.index("ShouldKeepNearPlayers(vehicle.GetOrigin(), players)"),
            body.index("IA_VehicleManager.DespawnVehicle(vehicle)"),
        )

    def test_callers_forward_player_positions(self):
        area = source("IA_AreaInstance.c")
        attackers = method(area, "DespawnAttackersAwayFromPlayers")
        groups = method(area, "DespawnGroupListAwayFromPlayers")
        self.assertIn("DespawnGroupAndPassengers(group, players, checkPlayers)", attackers)
        self.assertIn("DespawnGroupAndPassengers(group, players, checkPlayers)", groups)


if __name__ == "__main__":
    unittest.main()
