"""Source-contract: GM-placed RadioTower compositions must spawn.

Auto Support only auto-places extra mortar-pit markers. A RadioTower marker
the GM already put in Staging/Live still needs its destroyable composition,
or CheckCurrentZoneComplete waits forever on a required zone at score 0.

Run: python -m unittest tools.test_radio_tower_spawn -v
"""
import re
import unittest

from test_dynamic_base_flow import method, source


def brace_block_after(text, needle):
    idx = text.find(needle)
    if idx < 0:
        raise AssertionError(f"Missing {needle!r}")
    start = text.find("{", idx)
    if start < 0:
        raise AssertionError(f"No '{{' after {needle!r}")
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
    raise AssertionError(f"Unclosed block after {needle!r}")


class RadioTowerSpawnTests(unittest.TestCase):
    def test_radio_tower_is_a_required_zone(self):
        complete = method(source("IA_MissionInitializer.c"), "CheckCurrentZoneComplete")
        self.assertIn("if (marker_counter.GetAreaType() != IA_AreaType.MortarPit)", complete)
        self.assertIn("amountOfRequiredZones++", complete)
        self.assertNotIn("IA_AreaType.RadioTower", complete.split("amountOfRequiredZones++")[0].split("amountOfRequiredZones = 0")[-1])

    def test_auto_support_does_not_gate_radio_composition_spawn(self):
        proceed = method(source("IA_MissionInitializer.c"), "ProceedToNextZone")
        support = brace_block_after(proceed, "if (placeSupport)")
        self.assertIn("IA_MortarPitPlacer.EnsureForGroup(currentGroup)", support)
        self.assertNotIn("EnsureRadioTowersForGroup", support)
        self.assertIn("IA_AreaMarker.EnsureRadioTowersForGroup(currentGroup)", proceed)
        self.assertGreater(
            proceed.find("IA_AreaMarker.EnsureRadioTowersForGroup(currentGroup)"),
            proceed.find("if (placeSupport)"),
        )

    def test_hotadd_spawns_radio_composition(self):
        append = method(source("IA_MissionInitializer.c"), "ServerAppendLiveSite")
        spawn_at = append.find("SpawnAreaFromMarker(marker")
        ensure_at = append.find("marker.EnsureRadioTowerSpawned()")
        self.assertGreaterEqual(spawn_at, 0)
        self.assertGreater(ensure_at, spawn_at)

    def test_live_frame_spawns_radio_composition(self):
        frame = method(source("IA_AreaMarker.c"), "EOnFrame")
        mortar = brace_block_after(frame, "if (GetAreaType() == IA_AreaType.MortarPit)")
        self.assertIn("SpawnMortarPitComposition()", mortar)
        self.assertRegex(
            frame,
            r"else if \(GetAreaType\(\) == IA_AreaType\.RadioTower\)\s*\{\s*EnsureRadioTowerSpawned\(\);",
        )

    def test_ensure_is_idempotent_and_radio_only(self):
        ensure = method(source("IA_AreaMarker.c"), "EnsureRadioTowerSpawned")
        self.assertIn("if (GetAreaType() != IA_AreaType.RadioTower)", ensure)
        self.assertIn("if (m_prefabSpawned || m_spawnedEntity || m_isDestroyed)", ensure)
        self.assertIn("SpawnPrefabEntity()", ensure)


if __name__ == "__main__":
    unittest.main()
