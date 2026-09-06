"""Composition/source regression. Live navigation, firing and JIP are separate."""
from pathlib import Path
from collections import Counter
import json
import math
import re
import sys
import unittest

sys.dont_write_bytecode=True
from author_base_compositions import Author,REF,parse
from author_base_designs import generate,box,overlap,CAPS,perimeter_walls,mesh_box,WALL_INSET

ROOT=Path(__file__).resolve().parents[1]
BASE=Path('D:/ReforgerGameSources/data/data007')


def read(path):
    return (ROOT/path).read_text(encoding='utf-8-sig')


class CompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog=json.loads(read('docs/base-composition-catalog.json'))
        cls.recipes=json.loads(read('docs/base-composition-designs.json'))
        cls.measure=json.loads(read('docs/base-composition-measurements.json'))['assets']

    def test_reproducible_designs(self):
        for path,content in generate().items():
            self.assertEqual(read(path),content,path)

    @unittest.skipUnless(BASE.exists(),'requires extracted vanilla data007')
    def test_reproducible_assets_and_clean_inheritance(self):
        author=Author(BASE)
        outputs=author.generate()
        guids=set()
        for path,content in outputs.items():
            self.assertEqual(read(path),content,path)
            if not path.endswith('.et'):
                continue
            self.assertNotRegex(content,r'(?m)^\s*(?:SCR_\w*(?:Campaign|ServicePoint|SupportStation|ResourceComponent)\w*|ScriptedRadioComponent)\s|/Systems/|/Arsenal/|/Weapons/')
            root=parse(content)
            components=root.block('components').body
            self.assertEqual(sum(c.head.startswith('RplComponent ') for c in components),1,path)
            self.assertEqual(sum(c.head.startswith('Hierarchy ') for c in components),1,path)
            for ref in REF.finditer(content):
                target=ref[1]
                if target.startswith('Prefabs/BaseCompositions/'):
                    self.assertIn(target,outputs)
                else:
                    self.assertFalse(author.needs_clean(target),target)
            meta=read(path+'.meta')
            guid=re.search(r'Name "\{([A-F0-9]+)\}',meta)[1]
            self.assertNotIn(guid,guids)
            guids.add(guid)

    def test_all_sizes_themes_and_nonduplicated_recipes(self):
        self.assertEqual(len(self.recipes),120)
        used={m['key'] for r in self.recipes for m in r['modules']}
        self.assertEqual(used,set(self.catalog)-{'LivingLarge'}) # wall budget takes priority over the 518-entity cluster
        for size in range(6):
            recipes=[r for r in self.recipes if r['size']==size]
            self.assertEqual(len(recipes),20)
            signatures={json.dumps(r['modules'],sort_keys=True) for r in recipes}
            self.assertEqual(len(signatures),20)
            self.assertEqual(len({r['theme'] for r in recipes}),5)

    def test_geometry_budgets_lanes_and_guard_posts(self):
        for r in self.recipes:
            modules=r['modules']; W,D=r['half_width'],r['half_depth']
            self.assertEqual(modules[0]['role'],'Hq')
            self.assertTrue(any(m['role']=='Barracks' and m['required'] for m in modules))
            self.assertLessEqual(r['expanded']+r['guns']*12,820)
            self.assertLessEqual(len(modules)+len(r['walls'])+r['guns'],256)
            self.assertLessEqual(r['guns'],CAPS[r['size']])
            self.assertLessEqual(r['heavy'],int(r['size']<2))
            counts=Counter(m['side'] for m in modules)
            self.assertTrue(all(counts[s]>=1 for s in range(4)))
            lanes=[(-4,-D,4,r['capture'][2]),(-W,-12,W,-4)]
            for i,m in enumerate(modules):
                a=box(m)
                if m['side']>=0:
                    mb=mesh_box(m,self.measure)
                    self.assertGreaterEqual(mb[0],-W-0.6)
                    self.assertGreaterEqual(mb[1],-D-0.6)
                    self.assertLessEqual(mb[2],W+0.6)
                    self.assertLessEqual(mb[3],D+0.6)
                else:
                    self.assertGreaterEqual(a[0],-W+1)
                    self.assertGreaterEqual(a[1],-D+1)
                    self.assertLessEqual(a[2],W-1)
                    self.assertLessEqual(a[3],D-1)
                for other in modules[i+1:]:
                    self.assertFalse(overlap(a,box(other),1),(r['name'],m['key'],other['key']))
                if m['role']!='Hq':
                    self.assertFalse(any(overlap(a,lane,1) for lane in lanes),(r['name'],m['key']))
                for p in r['posts']:
                    self.assertFalse(overlap(a,(p[0]-1.5,p[2]-1.5,p[0]+1.5,p[2]+1.5),1))
            for heading in range(0,360,15):
                a=math.radians(heading)
                for p in r['posts']:
                    x=p[0]*math.cos(a)+p[2]*math.sin(a)
                    z=-p[0]*math.sin(a)+p[2]*math.cos(a)
                    self.assertAlmostEqual(math.hypot(x,z),math.hypot(p[0],p[2]),places=5)

    def test_wall_infill_gates_and_accounting(self):
        for r in self.recipes:
            walls=r['walls']
            self.assertEqual(walls,perimeter_walls(r['half_width'],r['half_depth'],r['modules'],self.catalog,self.measure))
            self.assertGreater(len(walls),30)
            self.assertEqual(r['expanded'],sum(m['expanded'] for m in r['modules'])+len(walls))
            styles={w.get('style',0) for w in walls}
            self.assertIn(3,styles,(r['name'],styles))
            if r['size']<3:
                self.assertTrue(len(styles)>=3,(r['name'],styles))
            for w in walls:
                x,z=w['position'][0],w['position'][2]
                if w['side']==2:
                    self.assertTrue(x+1.483<=-5.0+0.001 or x-1.483>=5.0-0.001)
                elif w['side'] in (1,3):
                    self.assertTrue(z+1.483<=-13+0.001 or z-1.483>=-3-0.001)
                a=(x-1.47,z-.62,x+1.47,z+.62) if w['side'] in (0,2) else (x-.62,z-1.47,x+.62,z+1.47)
                self.assertFalse(any(overlap(a,mesh_box(m,self.measure)) for m in r['modules'] if m['side']==w['side']),(r['name'],w))
        source=read('Scripts/Game/IA_ComposedSiteLayout.c')
        self.assertIn('AddCover("infill_"',source)
        self.assertIn('m_bRequired = true',source)
        self.assertIn('int style = 0',source)

    def test_fighting_positions_sit_on_the_wall_line(self):
        for r in self.recipes:
            W,D=r['half_width'],r['half_depth']
            for m in r['modules']:
                if m['side']<0:
                    continue
                mb=mesh_box(m,self.measure)
                if m['side']==0:
                    self.assertAlmostEqual(mb[3],D-WALL_INSET,places=2)
                elif m['side']==1:
                    self.assertAlmostEqual(mb[2],W-WALL_INSET,places=2)
                elif m['side']==2:
                    self.assertAlmostEqual(mb[1],-(D-WALL_INSET),places=2)
                else:
                    self.assertAlmostEqual(mb[0],-(W-WALL_INSET),places=2)

    def test_shared_gun_budget_includes_checkpoints(self):
        self.assertEqual(self.catalog['CheckpointM']['sockets'][0]['kind'],0)
        self.assertEqual(self.catalog['CheckpointL']['sockets'][0]['kind'],1)
        self.assertEqual(self.catalog['AA']['sockets'][0]['kind'],3)
        for r in self.recipes:
            sockets=[s for m in r['modules'] for s in self.catalog[m['key']]['sockets']]
            self.assertEqual(len(sockets),r['guns'])
            self.assertEqual(sum(s['kind']>0 for s in sockets),r['heavy'])

    def test_native_supporting_geometry_not_old_tripod_fit(self):
        source=read('Scripts/Game/IA_CompositionGunBuilder.c')
        self.assertIn('m_Spec.m_Socket.Transform(parent, m_Mat)',source)
        self.assertNotIn('uneven_feet',source)
        self.assertNotIn('m_aFeet',source)
        self.assertIn('CollectTree(m_Record.m_Root, exclusions)',source)
        self.assertNotIn('CollectTree(m_Assembly',source)
        self.assertIn('m_iHeavyInstalled >= 1',source)
        self.assertIn('m_iSample >= 5',source)
        self.assertIn('m_iDeadlineMs',source)
        self.assertNotIn('CallLater',source)
        self.assertNotIn('EOnFrame',source)
        placer=read('Scripts/Game/IA_DynamicSitePlacer.c')
        self.assertIn('IA_BaseDesignRecipes.Create(layoutId, m_iDesignVariant)',placer)
        self.assertIn('m_bEmplacementPhaseDone = !m_Settings || !m_Settings.m_bEmplacementsEnabled',placer)
        self.assertIn('m_bAuthoredAccess',placer)
        self.assertIn('m_bFollowTerrainPlane',placer)
        site=read('Scripts/Game/IA_DynamicSiteInstance.c')
        self.assertIn('if (!DeleteRoots())',site)

    def test_aa_retains_vanilla_limits_without_campaign_disassembly(self):
        aa=read('Prefabs/Emplacements/IA_Emplacement_AA.et')
        self.assertIn('Tripod_6T7_NSV_SPP.et',aa)
        self.assertNotIn('LimitsHoriz',aa)
        self.assertNotIn('LimitsVert',aa)
        self.assertIn('IA_StaticGunComponent',aa)
        self.assertIn('"Tag categories" 0 0',aa)
        self.assertIn('m_bDeleteAfterDestroyed 0',aa)


if __name__=='__main__':
    unittest.main()
