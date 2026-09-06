"""Source-contract regressions; Workbench validation/playtests cover engine behavior.

Run: python -m unittest discover -s tools -p "test_dynamic_base_flow.py"
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Scripts/Game"


def source(path):
    return (GAME / path).read_text(encoding="utf-8-sig")


def method(text, name):
    """Extract a named Enforce method body with balanced braces."""
    match = re.search(r"^\s*(?:(?:static|override|protected|private)\s+)*\w+\s+" + re.escape(name) + r"\([^;{}]*\)\s*\{", text, re.MULTILINE)
    if not match:
        raise AssertionError(f"Missing method: {name}")
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if depth == 0:
            return text[start:i]
    raise AssertionError(f"Unclosed method: {name}")


class DynamicBaseFlowTests(unittest.TestCase):
    def test_capture_awards_then_starts_defense_without_extra_stages(self):
        objective = source("IA_BaseAssaultObjective.c")
        seize = method(objective, "TickSeize")
        self.assertRegex(seize, r"if \(m_iCaptureAccMs >= need\)\s*\{\s*AwardCaptureOnce\(\);\s*StartBaseDefense\(\);")
        for retired in ("EnterRegroup", "TickRegroup", "EnterWarning", "TickWarning", "FreezeAtCapture", "WARNING_MS"):
            self.assertNotIn(retired, objective)
        start = method(objective, "StartBaseDefense")
        self.assertIn("if (m_bDefenseStarted)", start)
        self.assertLess(start.index("m_ePhase = IA_BaseObjectivePhase.Defend"), start.index("m_Defend.StartDefendMission()"))

    def test_defense_handoff_has_strong_owners(self):
        self.assertIn("protected ref IA_DefendMission m_Defend;", source("IA_BaseAssaultObjective.c"))
        mission = source("IA_DefendMission.c")
        for factory in ("Create", "CreateForDynamicBase"):
            self.assertIn("ref IA_DefendMission mission = new IA_DefendMission", method(mission, factory))
        self.assertIn("ref IA_DefendMission defendMission = IA_DefendMission.Create", source("IA_MissionInitializer.c"))

    def test_both_factories_use_same_legacy_enhanced_selection(self):
        mission = source("IA_DefendMission.c")
        for factory in ("Create", "CreateForDynamicBase"):
            body = method(mission, factory)
            self.assertIn("mission.GetDefenseConfig()", body)
            self.assertIn("if (cfg && !cfg.UseLegacyDefense())", body)
            self.assertIn("mission.m_Enhanced = IA_EnhancedDefendDirector.Create(mission)", body)
        self.assertIn("mission.m_DefenseConfig = defenseConfigSnapshot", method(mission, "CreateForDynamicBase"))

    def test_enhanced_defense_has_no_base_phase_or_event_shortcut(self):
        enhanced = source("IA_EnhancedDefendDirector.c")
        self.assertNotIn("IsPreparedDynamicBase", enhanced)
        self.assertNotIn("ShiftOffsiteEventsForPreparedBase", enhanced)
        start = method(enhanced, "Start")
        self.assertIn("m_ePhase = IA_DefendPhase.Prepare", start)
        self.assertNotIn("StartClock(", start)
        for step in ("RollDoctrine()", "ApplyDuration()", "ApplyPrepareWindow()", "BuildEventPlan()", "FireFirstBeat()"):
            self.assertIn(step, start)

    def test_base_hud_reuses_chrome_but_not_sector_prediction(self):
        hud = source("UI/IA_BaseObjectiveHud.c")
        self.assertIn("class IA_BaseObjectiveHud : IA_CaptureHud", hud)
        self.assertIn("super.DrawBody(surface", hud)
        phases = method(hud, "ShowsPhase")
        for phase in ("Placing", "Seize", "Failed"):
            self.assertIn("IA_BaseObjectivePhase." + phase, phases)
        for phase in ("Regroup", "Warning", "Defend", "Completed", "Cancelled"):
            self.assertNotIn("IA_BaseObjectivePhase." + phase, phases)
        progress = method(hud, "TickProgress")
        self.assertIn("MUI_Ease.Approach(m_fDisplay, m_fServerProgress", progress)
        self.assertNotIn("super.TickProgress", progress)
        self.assertNotIn("m_iRemainingSec", hud)
        self.assertIn('ApplyServer("", IA_CaptureHudState.Hidden, m_fServerProgress)', hud)

    def test_retired_wire_values_remain_reserved(self):
        enums = source("IA_DynamicObjectiveTypes.c")
        enum = re.search(r"enum IA_BaseObjectivePhase\s*\{([^}]+)\}", enums).group(1)
        enum = re.sub(r"//[^\n]*", "", enum)
        values = [value.strip() for value in enum.split(",")]
        self.assertEqual(values, ["None", "Placing", "Seize", "Regroup", "Warning", "Defend", "Completed", "Cancelled", "Failed"])
        snapshot = method(enums, "SnapshotFrom")
        self.assertNotIn("Regroup", snapshot)
        self.assertIn("IA_Config.PackDefenseExtras(cfg)", snapshot)


if __name__ == "__main__":
    unittest.main()
