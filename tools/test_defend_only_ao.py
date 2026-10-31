"""Defend-only AO must start the hold instead of stalling forever.

Run: python -m unittest discover -s tools -p "test_defend_only_ao.py"
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


class DefendOnlyAoTests(unittest.TestCase):
    def test_zero_capture_zones_with_defend_starts_hold(self):
        body = method(source("IA_MissionInitializer.c"), "CheckCurrentZoneComplete")
        zero = body.index("if(amountOfZones == 0)")
        self.assertIn("HasDefendObjectiveForGroup(currentGroup)", body[zero:])
        self.assertIn("m_bForceAuthoredOnFallback = true", body[zero:])
        self.assertIn("ContinueWithoutDynamicBase(m_iAoActivationSerial, currentGroup)", body[zero:])
        self.assertIn("TryBeginTerminalObjective(false)", body[zero:])
        # Must not be a bare return for the defend-only case.
        fragment = body[zero : zero + 900]
        self.assertIn("if (!HasDefendObjectiveForGroup(currentGroup))", fragment)

    def test_hotadd_defend_only_starts_completion_poll(self):
        body = method(source("IA_MissionInitializer.c"), "ServerAppendLiveSite")
        defend = body.index("IA_AreaType.DefendObjective")
        fragment = body[defend:]
        self.assertIn("CallLater(CheckCurrentZoneComplete", fragment)
        self.assertIn("BeginDynamicObjectiveAo(liveGroup)", fragment)
        # Group registration must happen before the defend early-return.
        insert = body.index("groupsArray.Insert(liveGroup)")
        self.assertLess(insert, defend)


if __name__ == "__main__":
    unittest.main()
