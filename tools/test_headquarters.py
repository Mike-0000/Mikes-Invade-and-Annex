"""Permanent-headquarters authoring regression. Live spawning/RPL/AI are separate."""
from pathlib import Path
from collections import Counter
import json
import re
import sys
import unittest

sys.dont_write_bytecode=True
from author_base_compositions import REF,parse
from author_base_designs import CAPS,RECIPE_EXPANDED_BUDGET,box,overlap
import author_headquarters as hq

ROOT=Path(__file__).resolve().parents[1]
BASE=Path('D:/ReforgerGameSources/data/data007')


def read(path):
    return (ROOT/path).read_text(encoding='utf-8-sig')


class HeadquartersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog=json.loads(read('docs/headquarters-catalog.json'))
        cls.measure=json.loads(read('docs/headquarters-measurements.json'))['assets']
        cls.recipes=json.loads(read('docs/headquarters-designs.json'))

    def test_reproducible_designs(self):
        for path,content in hq.generate_designs().items():
            self.assertEqual(read(path),content,path)

    @unittest.skipUnless(BASE.exists(),'requires extracted vanilla data007')
    def test_reproducible_assets(self):
        outputs=hq.HeadquartersAuthor(BASE).generate()
        guids=set()
        for path,content in outputs.items():
            self.assertEqual(read(path),content,path)
            if not path.endswith('.et'):
                continue
            meta=re.search(r'Name "\{([A-F0-9]+)\}',read(path+'.meta'))[1]
            self.assertNotIn(meta,guids)
            guids.add(meta)
            self.assertEqual(parse(content).prop('ID').strip('"'),meta,path)

    def test_every_catalog_prefab_measured_and_loaded(self):
        # Measurements come from a Workbench Preview run that spawned each prefab.
        self.assertEqual(set(self.catalog),set(self.measure))
        for key,m in self.measure.items():
            self.assertGreater(m['count'],0,key)
            self.assertTrue((ROOT/self.catalog[key]['prefab'].split('}',1)[1]).exists(),key)

    def test_no_stock_destructibles_spawned_directly(self):
        """Walls and buildings are mesh-only; no destruction phases or doors."""
        for key,entry in self.catalog.items():
            root=parse(read(entry['prefab'].split('}',1)[1]))
            components=root.block('components').body
            self.assertEqual(sum(c.head.startswith('RplComponent ') for c in components),1,key)
            self.assertEqual(sum(c.head.startswith('Hierarchy ') for c in components),1,key)
            text=root.render()
            self.assertNotRegex(text,r'SCR_Destructible|DoorComponent|Persistence')
            for ref in REF.finditer(text):
                target=ref[1]
                self.assertFalse('/Houses/' in target and 'BuildingAddons' not in target,(key,target))
                self.assertFalse(target.startswith('Prefabs/Structures/Walls/Concrete/'),(key,target))
        for path in (ROOT/hq.OUT/'Mesh').glob('*.et'):
            root=parse(path.read_text(encoding='utf-8-sig'))
            self.assertEqual(root.head,'StaticModelEntity',path)
            kinds=[c.head.split(' ',1)[0] for c in root.block('components').body]
            self.assertEqual(kinds,['MeshObject','RigidBody'],path)

    def test_wrappers_keep_vanilla_sentinel_posts(self):
        """IA area garrisons look up CoverPost/ObservationPost smart actions."""
        expected={'HQCommand':3,'HQTower':2,'HQGuardBox':1,'HQBarracks':0,'HQShelter':0,'HQPillbox':0}
        for key,count in expected.items():
            self.assertEqual(self.catalog[key]['posts'],count,key)
            text=read(self.catalog[key]['prefab'].split('}',1)[1])
            self.assertEqual(text.count('SCR_AISmartActionSentinelComponent '),count,key)
            self.assertEqual(text.count('"CoverPost"')+text.count('"ObservationPost"'),count,key)

    def test_wrappers_keep_frames_and_pillbox_ladder(self):
        """Door frames stay as mesh trim; the pillbox keeps its only way up."""
        self.assertEqual({k:self.catalog[k]['frames'] for k in ('HQCommand','HQBarracks','HQShelter','HQPillbox','HQTower','HQGuardBox')},
                         {'HQCommand':3,'HQBarracks':10,'HQShelter':1,'HQPillbox':1,'HQTower':0,'HQGuardBox':0})
        for key,entry in self.catalog.items():
            text=read(entry['prefab'].split('}',1)[1])
            ladders=text.count('Ladder_Bunker_SPS_B.et')
            self.assertEqual(ladders,int(key=='HQPillbox'),key)
            self.assertNotIn('Prefabs/Structures/BuildingParts/Doors/',text,key)
        pillbox=read(self.catalog['HQPillbox']['prefab'].split('}',1)[1])
        self.assertIn('PivotID "Socket_Bunker_SPS_Ladder"',pillbox)
        self.assertIn('"Parent Node From Parent Entity" 1',pillbox)

    def test_casemate_geometry(self):
        for key,kind in (('HQCasematePKM',0),('HQCasemateNSV',1)):
            m=self.measure[key]
            self.assertEqual([s['kind'] for s in self.catalog[key]['sockets']],[kind])
            pose=self.catalog[key]['sockets'][0]['chain'][0]['position']
            # Tripod on the 0.656 m parapet top, behind the parapet face.
            self.assertAlmostEqual(pose[1],0.656,3)
            self.assertLess(pose[2],0)
            # Roof and cheeks stay inside the 2-slot wall gap and ~3.2 m tall.
            self.assertLessEqual(max(-m['mins'][0],m['maxs'][0]),hq.PANEL+0.31)
            self.assertGreater(m['mins'][2],-3.3)
            self.assertAlmostEqual(m['maxs'][1],3.216,1)

    def test_wall_panels_tile_exactly(self):
        for key,panels in (('HQWallA',2),('HQWallB',2),('HQWallC',2),('HQWallPanel',1)):
            m=self.measure[key]
            self.assertAlmostEqual(m['maxs'][0],hq.PANEL*panels/2,2,key)
            # A pillar overhangs the left end by 0.474 m (stock panel geometry).
            self.assertAlmostEqual(m['mins'][0],-hq.PANEL*panels/2-0.474,2,key)
            self.assertLess(m['mins'][1],-0.9,key)

    def test_all_sizes_and_archetypes(self):
        self.assertEqual(len(self.recipes),6*hq.HQ_VARIANTS)
        self.assertEqual(Counter(r['size'] for r in self.recipes),Counter({s:hq.HQ_VARIANTS for s in range(6)}))
        for size in range(6):
            archetypes=Counter(r['archetype'] for r in self.recipes if r['size']==size)
            self.assertEqual(set(archetypes),set(hq.ARCHETYPES))
            layouts=[tuple((m['key'],tuple(m['position'])) for m in r['modules']) for r in self.recipes if r['size']==size]
            self.assertEqual(len(set(layouts)),len(layouts),size)

    def test_budgets_guns_and_gates(self):
        for r in self.recipes:
            name=r['name']
            self.assertLessEqual(r['roots'],hq.ROOT_LIMIT,name)
            self.assertLessEqual(r['expanded']+r['guns']*12,RECIPE_EXPANDED_BUDGET,name)
            self.assertTrue(4<=r['guns']<=CAPS[r['size']],name)
            self.assertLessEqual(r['heavy'],int(r['size']<2),name)
            armed={m['side'] for m in r['modules'] if m['key'].startswith('HQCasemate') and m['side']>=0}
            self.assertEqual(armed,{0,1,2,3},name)
            self.assertEqual(r['modules'][0]['key'],'HQCommand',name)
            self.assertEqual(r['modules'][0]['role'],'Hq',name)
            self.assertTrue(any(m['role']=='Barracks' for m in r['modules']),name)
            # Front gate and both side gates stay open for the shared entry points.
            for w in r['walls']+[m for m in r['modules'] if m['side']>=0]:
                a=box(w)
                if w['side']==2:
                    self.assertFalse(a[2]>-4.9 and a[0]<4.9,(name,w))
                if w['side'] in (1,3):
                    self.assertFalse(a[3]>-12.9 and a[1]<-3.1,(name,w))

    def test_walls_close_every_face(self):
        for r in self.recipes:
            for side in range(4):
                horizontal=side in (0,2)
                half=r['wall_half_width'] if horizontal else r['wall_half_depth']
                spans=sorted((w['position'][0 if horizontal else 2]-w['half_width'],w['position'][0 if horizontal else 2]+w['half_width'])
                             for w in r['walls'] if w['side']==side)
                spans+= [(m['position'][0 if horizontal else 2]-hq.PANEL,m['position'][0 if horizontal else 2]+hq.PANEL)
                         for m in r['modules'] if m['side']==side]
                spans.sort()
                edge=-half
                for a,b in spans:
                    gap=a-edge
                    if gap>0.01:
                        # Only the authored gates may be open.
                        mid=(a+edge)/2
                        self.assertTrue((side==2 and abs(mid)<5.5) or (side in (1,3) and -14<mid<-2),(r['name'],side,edge,a))
                    edge=max(edge,b)
                self.assertAlmostEqual(edge,half,2,(r['name'],side))

    def test_interior_clear_of_walls_lane_and_each_other(self):
        for r in self.recipes:
            lim_x=r['wall_half_width']-1.7
            lim_z=r['wall_half_depth']-1.7
            interior=[m for m in r['modules'] if m['side']<0]
            for m in interior:
                a=box(m)
                self.assertTrue(a[0]>=-lim_x-1e-3 and a[2]<=lim_x+1e-3 and a[1]>=-lim_z-1e-3 and a[3]<=lim_z+1e-3,(r['name'],m['key']))
            for i,m in enumerate(interior):
                for n in interior[i+1:]:
                    self.assertFalse(overlap(box(m),box(n),1),(r['name'],m['key'],n['key']))
            lane=(-hq.LANE_HALF,-r['half_depth'],hq.LANE_HALF,r['capture'][2])
            for m in interior:
                if m['role'] not in ('Hq','Tower','Cover'):
                    self.assertFalse(overlap(box(m),lane,1),(r['name'],m['key']))
            for b in r['belt']:
                a=box(b)
                self.assertTrue(a[0]>=-r['half_width'] and a[2]<=r['half_width'] and a[1]>=-r['half_depth'] and a[3]<=r['half_depth'],(r['name'],b))

    def test_doors_open_onto_clear_yard(self):
        """Measured door sockets get a clear apron; flank buildings face the lane."""
        def bare(m):
            pad=0 if m['side']>=0 else 1
            return box(dict(m,half_width=m['half_width']-pad,half_depth=m['half_depth']-pad))
        checked=0
        for r in self.recipes:
            inner_x,inner_z=r['wall_half_width']-0.5,r['wall_half_depth']-0.5
            for m in r['modules']:
                aprons=hq.Plan.aprons(None,m)
                self.assertEqual(len(aprons),len(hq.DOORS.get(m['key'],[])))
                for a,_ in aprons:
                    checked+=1
                    self.assertTrue(a[0]>=-inner_x and a[2]<=inner_x and a[1]>=-inner_z and a[3]<=inner_z,(r['name'],m['key'],a))
                    for n in r['modules']:
                        if n is not m:
                            self.assertFalse(overlap(a,bare(n)),(r['name'],m['key'],n['key']))
                if aprons and m['role'] not in ('Hq','Tower') and m['side']<0:
                    wx,_=aprons[0][1]
                    self.assertLessEqual(wx*m['position'][0],0,(r['name'],m['key']))
            hq_door=hq.Plan.aprons(None,r['modules'][0])[0]
            self.assertEqual(hq_door[1],(0,-1),r['name'])  # command door looks down the lane at the gate
        self.assertGreater(checked,500)

    def test_posts_inside_walls_and_clear(self):
        for r in self.recipes:
            self.assertGreaterEqual(len(r['posts']),16 if r['size']<2 else 6,r['name'])
            for p in r['posts']:
                self.assertLess(abs(p[0]),r['wall_half_width'],r['name'])
                self.assertLess(abs(p[2]),r['wall_half_depth'],r['name'])
                a=(p[0]-1.5,p[2]-1.5,p[0]+1.5,p[2]+1.5)
                self.assertFalse(any(overlap(a,box(m),1) for m in r['modules']),(r['name'],p))

    def test_runtime_selection_wiring(self):
        placer=read('Scripts/Game/IA_DynamicSitePlacer.c')
        self.assertNotIn('IA_BaseDesignRecipes.Create',placer)
        self.assertIn('IA_BaseDesignLibrary.CreateLayout',placer)
        self.assertIn('TryFallbackToScrappy',placer)
        config=read('Scripts/Game/IA_Config.c')
        self.assertIn('int m_iDynamicBaseHeadquartersChancePct = 50;',config)
        library=read('Scripts/Game/IA_BaseDesignLibrary.c')
        self.assertIn(f'HQ_BASE = 100',library)
        recipes=read('Scripts/Game/IA_HeadquartersRecipes.c')
        self.assertIn(f'VARIANTS = {hq.HQ_VARIANTS}',recipes)
        # Admin override pack format is untouched; the field is config-only.
        self.assertNotIn('Headquarters',read('Scripts/Game/IA_AdminOverrides.c'))


if __name__=='__main__':
    unittest.main()
