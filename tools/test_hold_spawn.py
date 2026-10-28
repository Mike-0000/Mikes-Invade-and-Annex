"""Source-contract: hold / CoverPost infantry origin must not fail closed when
players already stand on the post (HOLD_ORIGIN_MAX 80 < PLAYER_MIN 280).

CoverPost-converted occupying slots in GenerateRandomAiGroups share this helper,
so a rushed flag empties those garrison seats the same way #34 emptied the
80-250 occupying ring.
"""
from pathlib import Path
import math
import re
import sys
import unittest

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "Scripts/Game/IA_SpawnPlacement.c").read_text(encoding="utf-8-sig")
AREA = (ROOT / "Scripts/Game/IA_AreaInstance.c").read_text(encoding="utf-8-sig")
GROUP = (ROOT / "Scripts/Game/IA_AI_Group.c").read_text(encoding="utf-8-sig")


def const_float(name: str) -> float:
    match = re.search(rf"static const float {name} = ([0-9.]+);", SOURCE)
    if not match:
        raise AssertionError(f"missing const {name}")
    return float(match.group(1))


def hold_origin_body() -> str:
    body = SOURCE.split("static vector FindHoldInfantryOrigin", 1)[1]
    return body.split("static vector FindReinforcementInfantryOrigin", 1)[0]


class HoldSpawnGeometry(unittest.TestCase):
    def test_constants_still_conflict_on_post(self):
        hold_max = const_float("HOLD_ORIGIN_MAX_M")
        player_min = const_float("PLAYER_MIN_M")
        self.assertLess(hold_max, player_min)

        hold = (0.0, 0.0)
        player = (0.0, 0.0)
        samples = []
        for i in range(24):
            ang = (2.0 * math.pi * i) / 24.0
            for radius in (15.0, 40.0, 80.0):
                samples.append((hold[0] + math.cos(ang) * radius, hold[1] + math.sin(ang) * radius))

        strict_hits = 0
        for x, z in samples:
            dist = math.hypot(x - player[0], z - player[1])
            if hold_max >= dist >= 15.0 and dist >= player_min:
                strict_hits += 1
        self.assertEqual(strict_hits, 0)

        hard_cap = const_float("HARD_CAP_FROM_CENTER_M")
        expanded_hits = 0
        for i in range(24):
            ang = (2.0 * math.pi * i) / 24.0
            for radius in (player_min, 400.0, hard_cap):
                x = hold[0] + math.cos(ang) * radius
                z = hold[1] + math.sin(ang) * radius
                dist = math.hypot(x - player[0], z - player[1])
                if player_min <= dist <= hard_cap:
                    expanded_hits += 1
        self.assertGreater(expanded_hits, 0)

    def test_hold_origin_expands_only_under_player_floor(self):
        body = hold_origin_body()
        self.assertIn("HOLD_ORIGIN_MIN_M, HOLD_ORIGIN_MAX_M, playerMin", body)
        self.assertIn("PLAYER_MIN_M, HARD_CAP_FROM_CENTER_M, PLAYER_MIN_M", body)
        self.assertIn("HOLD_ORIGIN_MIN_M, HARD_CAP_FROM_CENTER_M, PLAYER_MIN_M", body)
        self.assertIn("if (playerMin <= 0)", body)
        # Do not pop-in at REINF_PLAYER_MIN (100 m) inside the hold ring.
        self.assertNotIn("REINF_PLAYER_MIN_M", body)
        self.assertNotIn("FindDefendWaveInfantryOrigin", body)
        self.assertNotIn("FindOccupyingInfantryOrigin", body)

    def test_coverpost_occupying_slots_and_building_holds_share_helper(self):
        # CoverPost seats steal occupying slots via holdPost=true.
        generate = AREA.split("void GenerateRandomAiGroups", 1)[1]
        generate = generate.split("protected void GenerateMortarPitAiGroups", 1)[0]
        self.assertIn("TryTakeGarrisonPost(garrisonPost)", generate)
        self.assertIn("holdPost = true", generate)

        building = AREA.split("protected void SpawnBuildingGarrisonGroups", 1)[1]
        building = building.split("private void _SpawnSingleAiGroupAndAddToArea", 1)[0]
        self.assertIn("_SpawnSingleAiGroupAndAddToArea", building)

        director = (ROOT / "Scripts/Game/IA_GmDirector.c").read_text(encoding="utf-8-sig")
        place = director.split("IA_GmHoldPost PlaceHoldPost", 1)[1]
        place = place.split("IA_SideObjectiveMarker FindNearestSideMarker", 1)[0]
        self.assertIn("liveArea.TrySpawnHoldPost(post)", place)

        search = GROUP.split("static void PerformNextRoadSearch", 1)[1]
        search = search.split("static void CompleteAsyncGroupCreation", 1)[0]
        self.assertIn("FindHoldInfantryOrigin(holdAt)", search)
        self.assertIn("m_originRetry", search)


if __name__ == "__main__":
    unittest.main()
