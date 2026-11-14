"""Civilian-vehicle cleanup must not GC-delete occupied cars.

Run: python -m unittest discover -s tools -p "test_civ_vehicle_cleanup.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class CivVehicleCleanupTests(unittest.TestCase):
    def test_abandoned_cleanup_uses_occupancy_not_driver_only(self):
        body = method(source("IA_AreaInstance.c"), "ManageCivilianVehicles")
        self.assertIn("IA_VehicleManager.IsVehicleOccupied(vehicle)", body)
        self.assertNotIn("GetVehicleDriver", body)
        self.assertIn("DespawnVehicle(vehicle)", body)

    def test_inactive_group_cleanup_also_keeps_occupied_vehicles(self):
        body = method(source("IA_VehicleManager.c"), "CleanupInactiveGroupVehicles")
        self.assertIn("IsVehicleOccupied(vehicle)", body)
        self.assertLess(body.index("IsVehicleOccupied(vehicle)"), body.index("DespawnVehicle(vehicle)"))


if __name__ == "__main__":
    unittest.main()
