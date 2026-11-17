"""QRF/defend-convoy crews must leave a wreck and assault on foot.

QRF hulls never enter m_areaVehicles, so HandleDestroyedVehicle never
ClearVehicleReference. UpdateVehicleOrders then sees empty seats on the wreck
and plants GetInVehicle (Autocomplete 0) forever instead of attacking.

Run: python -m unittest discover -s tools -p "test_qrf_wreck_remount.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class QrfWreckRemountTests(unittest.TestCase):
    def test_unusable_hulls_are_fire_immobile_or_destroyed(self):
        body = method(source("IA_VehicleManager.c"), "IsVehicleUnusable")
        self.assertIn("SCR_AIVehicleUsability.VehicleIsOnFire(vehicle)", body)
        self.assertIn("SCR_AIVehicleUsability.VehicleCanMove(vehicle)", body)
        self.assertIn("damage.IsDestroyed()", body)

    def test_update_vehicle_orders_abandons_before_getin(self):
        body = method(source("IA_AI_Group.c"), "UpdateVehicleOrders")
        self.assertIn("IA_VehicleManager.IsVehicleUnusable(vehicle)", body)
        self.assertIn("AbandonUnusableVehicleAndContinueOnFoot()", body)
        self.assertLess(
            body.index("IA_VehicleManager.IsVehicleUnusable(vehicle)"),
            body.index("IA_AiOrder.GetInVehicle"),
        )

    def test_abandon_dumps_passengers_clears_crew_role_and_assaults(self):
        abandon = method(source("IA_AI_Group.c"), "AbandonUnusableVehicleAndContinueOnFoot")
        self.assertIn("DumpPassengersAndAssault(assault)", abandon)
        self.assertIn("ClearVehicleReference()", abandon)
        self.assertIn("m_isVehicleCrewGroup = false", abandon)
        self.assertIn("m_pendingSeatTeleport = false", abandon)
        self.assertLess(
            abandon.index("DumpPassengersAndAssault(assault)"),
            abandon.index("ClearVehicleReference()"),
        )
        finish = method(source("IA_AI_Group.c"), "FinishCrewWreckAssault")
        self.assertIn("ForceEjectSeatedMembers()", finish)
        self.assertIn("IA_AiOrder.SearchAndDestroy", finish)

    def test_delayed_seat_teleport_does_not_board_a_wreck(self):
        body = method(source("IA_VehicleManager.c"), "_PlaceSpawnedUnitsInVehicle")
        self.assertIn("IsVehicleUnusable(vehicle)", body)
        self.assertLess(
            body.index("IsVehicleUnusable(vehicle)"),
            body.index("SeatCharactersInSlots"),
        )
        self.assertIn("AbandonUnusableVehicleAndContinueOnFoot(destination)", body)

    def test_drive_after_getin_refuses_unusable_hull(self):
        body = method(source("IA_AI_Group.c"), "DriveAfterGetInClear")
        self.assertIn("IA_VehicleManager.IsVehicleUnusable(vehicle)", body)
        self.assertLess(
            body.index("IA_VehicleManager.IsVehicleUnusable(vehicle)"),
            body.index("m_isDriving = true"),
        )

    def test_qrf_arrival_poll_stops_on_unusable_hull(self):
        body = method(source("IA_AreaGroupManager.c"), "QRF_PollTruckArrival")
        self.assertIn("IA_VehicleManager.IsVehicleUnusable(vehicle)", body)
        self.assertLess(
            body.index("IA_VehicleManager.IsVehicleUnusable(vehicle)"),
            body.index("HasVehicleReachedDestination"),
        )
        self.assertIn("AbandonUnusableVehicleAndContinueOnFoot(areaOrigin)", body)
        self.assertNotIn(
            "CallLater(this.QRF_PollTruckArrival",
            body[body.index("IsVehicleUnusable(vehicle)") : body.index("HasVehicleReachedDestination")],
        )

    def test_qrf_place_units_never_registers_area_vehicles(self):
        place = method(source("IA_VehicleManager.c"), "PlaceUnitsInVehicle")
        self.assertIn("areaInstance.AddMilitaryGroup(crewGroup)", place)
        self.assertNotIn("m_areaVehicles", place)


if __name__ == "__main__":
    unittest.main()
