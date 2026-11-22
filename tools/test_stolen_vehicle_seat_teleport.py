"""Source-contract: delayed PlaceUnitsInVehicle teleport must not seat AI
into a hull a player already stole.

Run: python -m unittest discover -s tools -p "test_stolen_vehicle_seat_teleport.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class StolenVehicleSeatTeleportTests(unittest.TestCase):
    def test_player_occupancy_helper_fail_closes_and_checks_seats(self):
        helper = method(source("IA_VehicleManager.c"), "VehicleHasPlayerOccupantOrTransition")
        self.assertIn("GetGame().GetPlayerManager()", helper)
        self.assertRegex(helper, r"if \(!pm\)\s*return true;")
        self.assertIn("CollectOccupiedCompartmentSlots(vehicle, occupied)", helper)
        self.assertIn("GetPlayerIdFromControlledEntity(occupant) > 0", helper)
        self.assertIn("access.IsGettingIn()", helper)

    def test_delayed_teleport_aborts_before_force_get_in(self):
        place = method(source("IA_VehicleManager.c"), "_PlaceSpawnedUnitsInVehicle")
        abort_at = place.index("VehicleHasPlayerOccupantOrTransition(vehicle)")
        seat_at = place.index("SeatCharactersInSlots(")
        drive_at = place.index("DriveAfterGetInClear(")
        self.assertLess(abort_at, seat_at)
        self.assertLess(abort_at, drive_at)
        self.assertLess(
            place.index("passengerGroup.AbortPendingSeatTeleport(destination)"),
            place.index("crewGroup.AbortPendingSeatTeleport(destination)"),
        )
        abort_block = place[abort_at:seat_at]
        self.assertIn("return;", abort_block)
        self.assertNotIn("SeatCharactersInSlots", abort_block)
        self.assertNotIn("DriveAfterGetInClear", abort_block)

    def test_abort_releases_reservation_and_pending_teleport(self):
        abort = method(source("IA_AI_Group.c"), "AbortPendingSeatTeleport")
        self.assertIn("m_pendingSeatTeleport = false", abort)
        self.assertIn("DumpPassengersAndAssault(assaultTarget)", abort)
        self.assertIn("ClearVehicleReference()", abort)
        self.assertIn("m_isVehicleCrewGroup = false", abort)
        self.assertIn("IA_GroupTacticalState.Attacking", abort)
        self.assertLess(abort.index("DumpPassengersAndAssault"), abort.index("ClearVehicleReference()"))
