"""Source-contract: occupying infantry origin must not fail closed when players
already stand on the next AO flag (OCCUPY_MAX 250 < PLAYER_MIN 280)."""
from pathlib import Path
import math
import re
import sys
import unittest

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "Scripts/Game/IA_SpawnPlacement.c").read_text(encoding="utf-8-sig")


def const_float(name: str) -> float:
    match = re.search(rf"static const float {name} = ([0-9.]+);", SOURCE)
    if not match:
        raise AssertionError(f"missing const {name}")
    return float(match.group(1))


class OccupyingSpawnGeometry(unittest.TestCase):
    def test_constants_still_conflict_on_flag(self):
        # The bug is the hard floor vs ring max. Relaxation must exist because
        # a player at the AO origin makes every 80-250 sample < PLAYER_MIN.
        occupy_max = const_float("OCCUPY_MAX_M")
        player_min = const_float("PLAYER_MIN_M")
        self.assertLess(occupy_max, player_min)

        center = (0.0, 0.0)
        player = (0.0, 0.0)
        samples = []
        for i in range(24):
            ang = (2.0 * math.pi * i) / 24.0
            for radius in (80.0, 160.0, 250.0):
                samples.append((center[0] + math.cos(ang) * radius, center[1] + math.sin(ang) * radius))

        strict_hits = 0
        for x, z in samples:
            dist = math.hypot(x - player[0], z - player[1])
            if occupy_max >= dist >= 80.0 and dist >= player_min:
                strict_hits += 1
        self.assertEqual(strict_hits, 0)

        expanded_hits = 0
        hard_cap = const_float("HARD_CAP_FROM_CENTER_M")
        for i in range(24):
            ang = (2.0 * math.pi * i) / 24.0
            for radius in (player_min, 400.0, hard_cap):
                x = center[0] + math.cos(ang) * radius
                z = center[1] + math.sin(ang) * radius
                dist = math.hypot(x - player[0], z - player[1])
                if player_min <= dist <= hard_cap and dist >= player_min:
                    expanded_hits += 1
        self.assertGreater(expanded_hits, 0)

    def test_occupying_origin_relaxes_then_expands(self):
        body = SOURCE.split("static vector FindOccupyingInfantryOrigin", 1)[1]
        body = body.split("static vector FindHoldInfantryOrigin", 1)[0]
        self.assertIn("OCCUPY_MIN_M, OCCUPY_MAX_M, PLAYER_MIN_M", body)
        self.assertIn("OCCUPY_MIN_M, OCCUPY_MAX_M, REINF_PLAYER_MIN_M", body)
        self.assertIn("PLAYER_MIN_M, HARD_CAP_FROM_CENTER_M, PLAYER_MIN_M", body)
        self.assertIn("OCCUPY_MIN_M, HARD_CAP_FROM_CENTER_M, REINF_PLAYER_MIN_M", body)
        # Still not the defend-wave helper (different callers / ring).
        self.assertNotIn("FindDefendWaveInfantryOrigin", body)

    def test_async_path_still_fail_closes_only_after_origin_search(self):
        group = (ROOT / "Scripts/Game/IA_AI_Group.c").read_text(encoding="utf-8-sig")
        search = group.split("static void PerformNextRoadSearch", 1)[1]
        search = search.split("static void CompleteAsyncGroupCreation", 1)[0]
        self.assertIn("FindOccupyingInfantryOrigin", search)
        self.assertIn("m_originRetry", search)
        self.assertIn("OnAsyncGroupCreated", group)


if __name__ == "__main__":
    unittest.main()
