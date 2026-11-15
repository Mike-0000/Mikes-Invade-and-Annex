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

    def test_qrf_fill_does_not_register_area_vehicles(self):
        # PlaceUnitsInVehicle is the QRF/defend-convoy crew path. It adds the
        # crew to m_military but never m_areaVehicles, so group cleanup is the
        # only ForceFinish GC for those hulls.
        place = method(source("IA_VehicleManager.c"), "PlaceUnitsInVehicle")
        self.assertIn("areaInstance.AddMilitaryGroup(crewGroup)", place)
        self.assertNotIn("RegisterVehicleWithMilitary", place)

    def test_dumped_crew_keeps_driving_hull_pointer(self):
        group = source("IA_AI_Group.c")
        skip = method(group, "ShouldSkipCrewRemount")
        self.assertIn("if (!m_isVehicleCrewGroup)", skip)
        update = method(group, "UpdateVehicleOrders")
        self.assertIn("if (ShouldSkipCrewRemount())", update)
        skip_branch = update[update.index("if (ShouldSkipCrewRemount())") :]
        skip_branch = skip_branch[: skip_branch.index("return;") + len("return;")]
        self.assertNotIn("ClearVehicleReference", skip_branch)
        self.assertNotIn("m_isDriving = false", skip_branch)

    def test_forcefinish_vehicle_list_runs_after_group_cleanup(self):
        # Kept QRF hulls are inserted into m_areaVehicles during group cleanup
        # so the later vehicle-origin pass can retire them once players leave.
        cleanup = method(source("IA_AreaInstance.c"), "DespawnAssetsAwayFromPlayers")
        self.assertLess(
            cleanup.index("DespawnGroupListAwayFromPlayers(m_military"),
            cleanup.index("DespawnVehicleListAwayFromPlayers(m_areaVehicles"),
        )


if __name__ == "__main__":
    unittest.main()
