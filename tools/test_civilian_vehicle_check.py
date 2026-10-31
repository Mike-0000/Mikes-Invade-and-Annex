"""Civilian vehicle remount must not infinite-GetIn on wrecks or cancel peers.

Run: python -m unittest discover -s tools -p "test_civilian_vehicle_check.py"
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Scripts/Game"


def source(path):
    return (GAME / path).read_text(encoding="utf-8-sig")


def method(text, name):
    match = re.search(
        r"^\s*(?:(?:static|override|protected|private)\s+)*\w+\s+"
        + re.escape(name)
        + r"\([^;{}]*\)\s*\{",
        text,
        re.MULTILINE,
    )
    if not match:
        raise AssertionError(f"Missing method: {name}")
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if depth == 0:
            return text[start:i]
    raise AssertionError(f"Unclosed method: {name}")


class CivilianVehicleCheckTests(unittest.TestCase):
    def test_civilian_check_is_one_shot_chain_not_repeating_remove_all(self):
        mgr = source("IA_VehicleManager.c")
        place = method(mgr, "_PlaceSpawnedUnitsInVehicle")
        self.assertIn("ScheduleCivilianVehicleCheck(vehicle, crewGroup, destination)", place)
        self.assertNotRegex(
            place,
            r"CallLater\(\s*CheckCiviliansInVehicle\s*,\s*[^,]+,\s*true\s*,",
        )

        check = method(mgr, "CheckCiviliansInVehicle")
        self.assertNotRegex(check, r"GetCallqueue\(\)\s*\.\s*Remove\s*\(\s*CheckCiviliansInVehicle")
        self.assertIn("IsCivilianVehicleDriveable(vehicle)", check)
        self.assertIn("VehicleHasEmptyAccessibleSeat(vehicle)", check)
        self.assertIn("ScheduleCivilianVehicleCheck(vehicle, aiGroup, destination)", check)

        driveable = method(mgr, "IsCivilianVehicleDriveable")
        self.assertIn("IsDestroyed()", driveable)

    def test_destroyed_civilian_vehicles_are_untracked_not_gc_deleted(self):
        body = method(source("IA_AreaInstance.c"), "ManageCivilianVehicles")
        destroyed = body.index("IsDestroyed()")
        fragment = body[destroyed:destroyed + 400]
        self.assertIn("m_areaCivVehicles.Remove(i)", fragment)
        self.assertNotIn("DespawnVehicle(vehicle)", fragment)


if __name__ == "__main__":
    unittest.main()
