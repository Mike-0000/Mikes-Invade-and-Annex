"""Source-contract: defense waves must not fail-closed on a 280 m player floor.

63a2d15 stopped spawning in the 80-180 m ring players already occupy. The first
cut used the same 280 m as both the hold annulus min and the player floor, with
no relax, so a spread fireteam around a town hold returned vector.Zero. Director
and radio-tower callers have no airborne fallback.

Run: python -m unittest discover -s tools -p "test_defend_wave_spawn.py"
"""
from pathlib import Path
import math
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


def const_float(text, name):
    match = re.search(r"static const float " + re.escape(name) + r"\s*=\s*([0-9.]+)", text)
    if not match:
        raise AssertionError(f"Missing const: {name}")
    return float(match.group(1))


class DefendWaveSpawnTests(unittest.TestCase):
    def test_wave_origin_relaxes_player_floor_without_reopening_inner_ring(self):
        placement = source("IA_SpawnPlacement.c")
        body = method(placement, "FindDefendWaveInfantryOrigin")
        self.assertIn("TryDefendWaveOriginWithPlayerMin", body)
        self.assertIn("DEFEND_WAVE_PLAYER_MIN_M", body)
        self.assertIn("DEFEND_WAVE_PLAYER_RELAX_M", body)
        self.assertIn("DEFEND_WAVE_PLAYER_FLOOR_M", body)
        self.assertNotIn("REINF_MIN_M", body)
        self.assertNotIn("REINF_MAX_M", body)

        helper = method(placement, "TryDefendWaveOriginWithPlayerMin")
        self.assertIn("DEFEND_WAVE_MIN_M", helper)
        self.assertIn("DEFEND_WAVE_MAX_M", helper)
        self.assertNotIn("CENTER_MIN_M", helper)

        self.assertGreaterEqual(const_float(placement, "DEFEND_WAVE_MIN_M"), 280.0)
        self.assertGreaterEqual(const_float(placement, "DEFEND_WAVE_PLAYER_RELAX_M"), 100.0)
        self.assertLess(const_float(placement, "DEFEND_WAVE_PLAYER_RELAX_M"), 280.0)
        self.assertGreaterEqual(const_float(placement, "DEFEND_WAVE_PLAYER_FLOOR_M"), 80.0)
        self.assertLess(
            const_float(placement, "DEFEND_WAVE_PLAYER_FLOOR_M"),
            const_float(placement, "DEFEND_WAVE_PLAYER_RELAX_M"),
        )

    def test_spread_players_can_cover_a_280m_player_floor(self):
        # Six players at 200 m, 60 deg apart: a 400 m bisector sample is ~248 m
        # from the nearest pawn, inside a 280 m player floor and outside 160 m.
        player_r = 200.0
        sample_r = 400.0
        half_step = math.pi / 6.0
        dist = math.sqrt(
            sample_r * sample_r
            + player_r * player_r
            - 2.0 * sample_r * player_r * math.cos(half_step)
        )
        self.assertLess(dist, 280.0)
        self.assertGreater(dist, 160.0)

    def test_air_assault_lz_relaxes_instead_of_dropping_on_the_hold(self):
        placement = source("IA_SpawnPlacement.c")
        body = method(placement, "TryFindDefendDropLz")
        self.assertNotIn("TryFindDropLz(", body)
        self.assertIn("DEFEND_DROP_PLAYER_MIN_M", body)
        self.assertIn("DEFEND_DROP_PLAYER_RELAX_M", body)
        self.assertIn("DEFEND_DROP_PLAYER_FLOOR_M", body)
        self.assertGreaterEqual(const_float(placement, "DEFEND_DROP_PLAYER_FLOOR_M"), 80.0)
        self.assertLess(
            const_float(placement, "DEFEND_DROP_PLAYER_FLOOR_M"),
            const_float(placement, "DEFEND_DROP_PLAYER_MIN_M"),
        )

    def test_defend_enactor_has_no_airborne_fallback_on_zero_origin(self):
        area = source("IA_AreaInstance.c")
        body = method(area, "SpawnReinforcementEnactor")
        self.assertIn("FindDefendWaveInfantryOrigin", body)
        self.assertIn("if (spawnPos == vector.Zero)", body)
        self.assertIn("return false", body)
        self.assertNotIn("ScheduleAirborneQRF", body)
        self.assertNotIn("SpawnAirborneQRF", body)
