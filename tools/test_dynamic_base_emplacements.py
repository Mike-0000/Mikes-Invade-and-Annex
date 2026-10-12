"""Source/authoring regression; not a substitute for live engine assertions."""
from pathlib import Path
import hashlib
import json
import math
import re
import sys
import unittest

sys.dont_write_bytecode = True
from author_base_emplacements import generate, NAMES, envelope
from check_dynamic_base_layouts import read_layout

ROOT = Path(__file__).resolve().parents[1]


def text(path):
    return (ROOT/path).read_text(encoding='utf-8-sig')


class EmplacementTests(unittest.TestCase):
    def test_authoring_is_reproducible(self):
        for path, content in generate().items():
            self.assertEqual(path.read_text(encoding='utf-8'), content, path)

    def test_structural_walls_and_guard_positions_are_unchanged(self):
        source = text('Scripts/Game/IA_DynamicSiteLayout.c')
        declarations = '\n'.join(line.strip() for line in source.splitlines()
                                 if any(s in line for s in ('layout.AddModule(', 'layout.AddCover(', 'm_aGuardPosts.Insert(')))
        self.assertEqual(hashlib.sha256(declarations.encode()).hexdigest(),
                         '241fde22717cba1ab37f486183d018fe158fa92822fc25b70ce09b353fc90429')

    def test_candidates_all_headings(self):
        data = json.loads(text('docs/dynamic-base-emplacements.json'))
        self.assertEqual(list(data['scenes']), NAMES)
        for name, stations in data['scenes'].items():
            self.assertLessEqual(len(stations), 8)
            self.assertEqual(len({s['id'] for s in stations}), len(stations))
            _, _, _, _, _, _, boxes, _ = read_layout(name)
            panel_names = {n for n, _ in boxes}
            for s in stations:
                self.assertIn(s['panel'], panel_names)
                self.assertIn(s['setback'], (1.5, 2.0, 2.5))
                if s['heavy']:
                    self.assertIn(name, ('Full', 'Compact'))
                    self.assertEqual(s['side'], 1)
            for heading in range(0, 360, 15):
                a = math.radians(heading)
                world = [(s['position'][0]*math.cos(a)+s['position'][2]*math.sin(a),
                          -s['position'][0]*math.sin(a)+s['position'][2]*math.cos(a)) for s in stations]
                for i, p in enumerate(world):
                    for q in world[i+1:]:
                        self.assertGreaterEqual(math.dist(p,q), 12-1e-6)

    def test_variants_are_armed_not_scenery_or_deployable(self):
        for kind, guid, parent in [('PKM','A80E963CB15F40D1','723870DBB19D30B0'),
                                   ('NSV','A80E963CB15F40D2','29F0CC704A582154')]:
            path = f'Prefabs/Emplacements/IA_Emplacement_{kind}.et'
            prefab = text(path)
            self.assertTrue(prefab.startswith(f'Turret : "{{{parent}}}'))
            self.assertIn(guid, text(path+'.meta'))
            self.assertNotIn('Deployable', prefab)
            self.assertIn('IA_StaticGunComponent', prefab)
            self.assertIn('"Tag categories" 0 0', prefab)
            self.assertEqual(prefab.count('LimitsHoriz -30 30'), 2)
            self.assertEqual(prefab.count('LimitsVert -5 20'), 2)
            self.assertIn('m_bDeleteAfterDestroyed 0', prefab)
            self.assertIn('m_bDetachAfterDestroyed 0', prefab)

    def test_no_per_gun_frame_or_call_queue(self):
        for path in (ROOT/'Scripts/Game').glob('IA_StaticGun*.c'):
            source = path.read_text(encoding='utf-8')
            self.assertNotIn('EOnFrame', source)
            self.assertNotIn('CallLater(', source)
            self.assertNotIn('StartCommand_Vehicle', source)
        assignment = text('Scripts/Game/IA_StaticGunAssignment.c')
        self.assertIn('m_bAccepted || now < m_iNextAttemptMs', assignment)
        self.assertIn('m_iAttempts >= 3', assignment)
        self.assertIn('m_Seat.IsReservedBy(m_Pawn)', assignment)
        self.assertIn('AllowMaxLOD()', assignment)
        self.assertIn('GetReloadDuration() * 1000', assignment)
        self.assertIn('EGetOutType.TELEPORT', assignment)
        self.assertIn('GetOutVehicle_NoDoor', assignment)
        self.assertIn('ShouldDeferGroupDespawn', assignment)
        self.assertNotIn('Never force a pawn pose', assignment)
        self.assertNotIn('Normal host cleanup owns the remaining AI', assignment)

    def test_exit_timeout_force_ejects_instead_of_abandoning_the_seat(self):
        assignment = text('Scripts/Game/IA_StaticGunAssignment.c')
        finish = assignment.split('protected void FinishExit(int now)', 1)[1]
        self.assertIn('EGetOutType.ANIMATED', finish)
        self.assertLess(finish.index('EGetOutType.ANIMATED'), finish.index('RequestAiEject'))
        self.assertIn('RequestAiEject(access, character)', finish)
        # Timeout must keep ticking until the pawn is unbound; do not drop restore just because ANIMATED failed.
        after_animated = finish.split('EGetOutType.ANIMATED', 1)[1]
        self.assertNotIn('m_bRestoreDefense = false', after_animated)

    def test_site_ownership_and_budget_exclude_occupants(self):
        site = text('Scripts/Game/IA_DynamicSiteInstance.c')
        self.assertIn('ChimeraCharacter.Cast(ent)', site)
        self.assertIn('HasEmplacementPlayerOccupant()', site)
        self.assertIn('HasEmplacementOccupant()', site)
        delete_roots = site.split('bool DeleteRoots()', 1)[1].split('protected void RequestSavedNavRebuild', 1)[0]
        self.assertIn('HasEmplacementOccupant()', delete_roots)
        group = text('Scripts/Game/IA_AI_Group.c')
        despawn = group.split('void Despawn()', 1)[1].split('void SetTacticalState', 1)[0]
        self.assertIn('ShouldDeferGroupDespawn()', despawn)
        record = text('Scripts/Game/IA_StaticGunRecord.c')
        self.assertIn('bool HasOccupant()', record)
        builder = text('Scripts/Game/IA_EmplacementBuilder.c')
        self.assertIn('m_Site.AddEmplacement(m_Record)', builder)
        self.assertIn('remainingEntities + m_Profile.m_iExpanded', builder)
        self.assertNotIn('CollectOwnedEntities(', builder)
        self.assertNotIn('Random', builder)

    def test_capture_and_failed_paths_stop_assignment(self):
        objective = text('Scripts/Game/IA_BaseAssaultObjective.c')
        defense = objective.split('protected void StartBaseDefense()',1)[1].split('protected Faction',1)[0]
        self.assertLess(defense.index('StopEmplacementAssignments(true)'), defense.index('CreateForDynamicBase('))
        failure = objective.split('"defense_abort"',1)[0]
        self.assertIn('StopEmplacementAssignments(true)', failure)
        hook = text('Scripts/Game/IA_StaticGunCombat.c')
        self.assertIn('IA_StaticGunComponent.Find(m_CurrentCompartmentSlot.GetOwner())', hook)
        self.assertIn('addGetIn = false', hook)
        self.assertIn('super.TryAddDismountTurretActions', hook)

    def test_persistence_and_snapshot_paths_carry_setting(self):
        for file in ('IA_Config.c','IA_AdminOverrides.c','IA_DynamicObjectiveTypes.c'):
            self.assertIn('m_bDynamicBaseEmplacementsEnabled', text('Scripts/Game/'+file))
        cfg = text('Scripts/Game/IA_Config.c')
        self.assertIn('int emplacementsI = 1;', cfg)
        self.assertIn('version != 2 || parts.Count() != 11', cfg)
        self.assertIn('version != 1 || parts.Count() != 10', cfg)


if __name__ == '__main__':
    unittest.main()
