"""Source-contract: role restrictions must gate TRAIT_ARMOR like VEHICLE_APC.

Run: python3 -m unittest discover -s tools -p "test_vehicle_role_gates.py"
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


class VehicleRoleGateTests(unittest.TestCase):
    def test_crewman_gate_covers_trait_armor_and_apc(self):
        body = method(source("IA_VehicleManager.c"), "CanPlayerPilotVehicle")
        self.assertIn("EEditableEntityLabel.VEHICLE_APC", body)
        self.assertIn("EEditableEntityLabel.TRAIT_ARMOR", body)
        self.assertIn("EEditableEntityLabel.VEHICLE_HELICOPTER", body)
        # HQ armor pads include TRAIT_ARMOR without VEHICLE_APC. Breaking out
        # of the label loop after APC would still miss those hulls if APC is
        # absent, and would skip a later TRAIT_ARMOR if APC were listed first.
        self.assertNotRegex(
            body,
            r"VEHICLE_APC\)\s*\{\s*isArmor\s*=\s*true;\s*break;",
        )
        self.assertIn("IA_PlayerRole.CREWMAN", body)
        self.assertIn("IA_PlayerRole.PILOT", body)

    def test_qrf_armor_classifier_still_uses_both_labels(self):
        body = method(source("IA_AreaGroupManager.c"), "DetermineVehicleClass")
        self.assertIn("EEditableEntityLabel.VEHICLE_APC", body)
        self.assertIn("EEditableEntityLabel.TRAIT_ARMOR", body)

    def test_getin_actions_still_delegate_to_role_gate(self):
        modded = source("IA_GetInUserAction_Modded.c")
        self.assertEqual(modded.count("CanPlayerPilotVehicle"), 2)


if __name__ == "__main__":
    unittest.main()
