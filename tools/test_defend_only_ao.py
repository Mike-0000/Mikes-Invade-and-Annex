"""Defend-only AO must start the authored hold instead of stalling forever.

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
        after = body.index("////Print(\"Running CheckCurrentZoneComplete 3\"", zero)
        fragment = body[zero:after]
        self.assertIn("HasDefendObjectiveForGroup(currentGroup)", fragment)
        self.assertIn("m_bForceAuthoredOnFallback = true", fragment)
        self.assertIn("ContinueWithoutDynamicBase(m_iAoActivationSerial, currentGroup)", fragment)
        # Empty primary circles make dynamic-base placement fail into admin-wait.
        self.assertNotIn("TryBeginTerminalObjective", fragment)

    def test_hotadd_defend_does_not_hijack_live_group(self):
        body = method(source("IA_MissionInitializer.c"), "ServerAppendLiveSite")
        defend = body.index("IA_AreaType.DefendObjective")
        insert = body.index("groupsArray.Insert(liveGroup)")
        self.assertLess(defend, insert)
        fragment = body[defend:insert]
        self.assertIn("return;", fragment)
        self.assertNotIn("BeginDynamicObjectiveAo", fragment)
        self.assertNotIn("CallLater(CheckCurrentZoneComplete", fragment)


if __name__ == "__main__":
    unittest.main()
