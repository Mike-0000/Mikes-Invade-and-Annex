"""Source-contract: multi-pit AOs must bind capture/crew to this area's marker.

Run: python -m unittest discover -s tools -p "test_mortar_pit_marker_identity.py"
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


class MortarPitMarkerIdentityTests(unittest.TestCase):
    def test_own_marker_prefers_area_name_over_group_first(self):
        area = source("IA_AreaInstance.c")
        resolve = method(area, "ResolveOwnMortarPitMarker")
        self.assertIn("GetAreaMarkersForArea(name)", resolve)
        self.assertIn("GetAreaType() == IA_AreaType.MortarPit", resolve)
        self.assertLess(
            resolve.index("GetAreaMarkersForArea"),
            resolve.index("GetMortarPitMarkerForGroup"),
        )

    def test_capture_and_crew_use_own_marker(self):
        area = source("IA_AreaInstance.c")
        for name in ("IsMortarPitCaptured", "SetupMortarPitCrew", "SpawnMortarPitAiAtGuns"):
            body = method(area, name)
            self.assertIn("ResolveOwnMortarPitMarker()", body)
            self.assertNotIn("GetMortarPitMarkerForGroup(", body)


if __name__ == "__main__":
    unittest.main()
