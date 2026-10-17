"""Occupied AO/QRF hulls must not be GC-deleted under a player.

Run: python -m unittest discover -s tools -p "test_vehicle_despawn_occupied.py"
"""
import unittest

from test_dynamic_base_flow import method, source


class OccupiedVehicleDespawnTests(unittest.TestCase):
    def test_despawn_refuses_a_player_occupied_hull(self):
        manager = source("IA_VehicleManager.c")
        occupied = method(manager, "IsOccupiedByPlayer")
        self.assertIn("GetPlayerIdFromControlledEntity(occupant) > 0", occupied)
        self.assertIn("CollectOccupiedCompartmentSlots(vehicle, occupied)", occupied)
        self.assertIn("GetCharacterVehicle(character) == vehicle", occupied)
        self.assertIn("return vehicle.IsOccupied()", occupied)

        despawn = method(manager, "DespawnVehicle")
        self.assertLess(
            despawn.index("if (IsOccupiedByPlayer(vehicle))"),
            despawn.index("IA_Game.AddEntityToGc(vehicle)"),
        )
        self.assertIn("return false", despawn.split("AddEntityToGc")[0])
        self.assertIn("return true", despawn)

    def test_forcefinish_group_cleanup_does_not_untrack_a_stolen_hull(self):
        area = source("IA_AreaInstance.c")
        group_cleanup = method(area, "DespawnGroupAndPassengers")
        self.assertIn("if (IA_VehicleManager.DespawnVehicle(vehicle))", group_cleanup)
        untrack = group_cleanup[
            group_cleanup.index("if (IA_VehicleManager.DespawnVehicle(vehicle))") :
        ]
        self.assertIn("m_areaVehicles.Remove(milIdx)", untrack)
        self.assertIn("m_areaCivVehicles.Remove(civIdx)", untrack)
        before = group_cleanup[
            : group_cleanup.index("if (IA_VehicleManager.DespawnVehicle(vehicle))")
        ]
        self.assertNotIn("m_areaVehicles.Remove", before)

    def test_deferred_vehicle_list_keeps_player_occupied_hulls(self):
        area = source("IA_AreaInstance.c")
        vehicles = method(area, "DespawnVehicleListAwayFromPlayers")
        self.assertIn("if (IA_VehicleManager.IsOccupiedByPlayer(vehicle))", vehicles)
        occupied = vehicles[vehicles.index("IsOccupiedByPlayer") :]
        self.assertIn("continue", occupied.split("DespawnVehicle")[0])
        self.assertIn("if (!IA_VehicleManager.DespawnVehicle(vehicle))", occupied)
        self.assertLess(
            occupied.index("if (!IA_VehicleManager.DespawnVehicle(vehicle))"),
            occupied.index("vehicles.Remove(i)"),
        )

    def test_dumped_crew_keeps_the_hull_pointer(self):
        group = source("IA_AI_Group.c")
        skip = method(group, "ShouldSkipCrewRemount")
        self.assertIn("if (!m_isVehicleCrewGroup)", skip)
        update = method(group, "UpdateVehicleOrders")
        self.assertIn("if (ShouldSkipCrewRemount())", update)
        skip_branch = update[update.index("if (ShouldSkipCrewRemount())") :]
        skip_branch = skip_branch[: skip_branch.index("return;") + len("return;")]
        self.assertNotIn("ClearVehicleReference", skip_branch)
        self.assertNotIn("m_isDriving = false", skip_branch)
