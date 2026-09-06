"""Author small, scenery-only interior vignettes. Sandbag declarations are never edited.

Usage: python tools/author_base_interiors.py D:/ReforgerGameSources/data/data007 --probe
       python tools/author_base_interiors.py D:/ReforgerGameSources/data/data007
"""
from pathlib import Path
import json
import math
import re
import sys
import uuid
import subprocess
from functools import cache

REPO = Path(__file__).resolve().parents[1]
BASE = Path(sys.argv[1])
ASSETS = {
    'board': 'Prefabs/Structures/Civilian/MessageBoard/Filled/MessageBoard_USSR_01_RedStarNews_filled_V1.et',
    'table': 'Prefabs/Props/Military/Furniture/TableMilitary_USSR_01.et',
    'chair': 'Prefabs/Props/Military/Furniture/ChairMilitary_USSR_01.et',
    'crate': 'Prefabs/Props/Crates/CrateWooden_02/CrateWooden_02_1x1x1.et',
    'rations': 'Prefabs/Props/Military/Camps/PalletMRE_01_Soviet.et',
    'barrel': 'Prefabs/Props/Industrial/BarrelMetal_01_military.et',
    'can': 'Prefabs/Props/Civilian/Jerrycan_01.et',
    'bucket': 'Prefabs/Props/Civilian/Bucket_01.et',
    'tools': 'Prefabs/Props/Industrial/ToolBox_01/ToolBox_01_grey.et',
    'wheel': 'Prefabs/Props/VehicleParts/Tires/Wheel_Ural4320.et',
    'cable': 'Prefabs/Props/Construction/CableReel/CableReelWooden_01.et',
    'sacks': 'Prefabs/Props/Civilian/SackUniversal_01_Pile.et',
    'generator': 'Prefabs/Props/Military/Generators/GeneratorPortable_USSR_01.et',
    'floodlight': 'Prefabs/Props/Military/Generators/GeneratorFloodlight_USSR_01.et',
    'lamp': 'Prefabs/Props/Military/Camps/Lamp_Interactive.et',
    'kitchen': 'Prefabs/Props/Military/FieldKitchenTrailerUSSR_01.et',
    'latrine': 'Prefabs/Structures/Civilian/Latrine_01.et',
    'sink': 'Prefabs/Props/Civilian/TableSink_01.et',
    'tank': 'Prefabs/Props/Agriculture/WaterTank_01_v1.et',
    'extinguisher': 'Prefabs/Props/Civilian/FireExtinguisher_01.et',
    'pallet': 'Prefabs/Props/Industrial/Pallets/Pallet_01.et',
    'covered': 'Prefabs/Props/Military/Camps/PalletAmmo_01_Soviet_camo.et',
    'medical': 'Prefabs/Props/EmptyStorages/USSR_MedicalBox_empty.et',
    'bin': 'Prefabs/Props/Garbage/Bins/TrashBin_03/TrashBin_USSR_03_full.et',
    'case': 'Prefabs/Props/Industrial/Pallets/MarsBox_01_green.et',
    'electric': 'Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et',
    'nosmoking': 'Prefabs/Props/Signs/Industrial/SignIndustrial_30x20_1_NoSmoking.et',
}


@cache
def stock_refs():
    pattern = r'\{[A-F0-9]+\}(?:' + '|'.join(re.escape(p) for p in ASSETS.values()) + ')'
    matches = subprocess.check_output(['rg', '--no-filename', '-o', pattern, str(BASE / 'Prefabs')], text=True)
    return {ref.split('}', 1)[1]: ref for ref in matches.splitlines()}


def resource(path):
    measured = REPO / 'docs/dynamic-base-interior-assets.json'
    if measured.exists() and path in json.loads(measured.read_text()):
        return json.loads(measured.read_text())[path]['resource']
    return stock_refs().get(path, path)


def stable_id(name):
    return uuid.uuid5(uuid.NAMESPACE_URL, 'ia-base-interior/' + name).hex[:16].upper()


def write_probe():
    refs = [resource(path) for path in ASSETS.values()]
    generated = sorted((REPO / 'Prefabs/DynamicBase').glob('IA_Dressing_*.et'))
    generated += sorted((REPO / 'Prefabs/DynamicBase').glob('IA_InfrastructureAsset_*.et'))
    expected = [0] * len(refs) + [len(re.findall(r'^  GenericEntity', p.read_text(), re.M)) for p in generated]
    refs += ['{' + stable_id(p.stem) + '}' + p.relative_to(REPO).as_posix() for p in generated]
    items = ',\n'.join('\t\t\t"' + r + '"' for r in refs)
    code = '''#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA interior asset measurement", wbModules: {"ResourceManager"})]
class IA_BaseInteriorProbe : IA_BaseFoundationProbe
{
    override void RunCommandline()
    {
        ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAInteriorProbe");
        BaseWorld world = preview.GetRef();
        array<ResourceName> resources = {
ITEMS
        };
        array<int> expectedChildren = {EXPECTED};
        foreach (int index, ResourceName name : resources)
        {
            IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(name), world);
            if (!ent)
            {
                m_iFailures++;
                Print("[IA][InteriorMeasure] FAILED " + name, LogLevel.ERROR);
                continue;
            }
            vector low = "99999 99999 99999";
            vector high = "-99999 -99999 -99999";
            MeasureHierarchy(ent, low, high);
            int children;
            IEntity child = ent.GetChildren();
            while (child)
            {
                children++;
                child = child.GetSibling();
            }
            Check(children >= expectedChildren[index], "all authored children attached: " + name);
            if (low[0] > high[0])
            {
                m_iFailures++;
                Print("[IA][InteriorMeasure] FAILED empty hierarchy " + name, LogLevel.ERROR);
            }
            Print(string.Format("[IA][InteriorMeasure] %1 mins=%2 maxs=%3", ent.GetPrefabData().GetPrefabName(), low, high), LogLevel.NORMAL);
            SCR_EntityHelper.DeleteEntityAndChildren(ent);
        }
        Print(string.Format("[IA][InteriorMeasure] failures=%1", m_iFailures), LogLevel.NORMAL);
        Workbench.Exit(m_iFailures);
    }
}
#endif
'''.replace('ITEMS', items).replace('EXPECTED', ', '.join(map(str, expected)))
    (REPO / 'Scripts/WorkbenchGame/IA_BaseInteriorProbe.c').write_text(code)


if '--import-measurement' in sys.argv:
    logfile = Path(sys.argv[sys.argv.index('--import-measurement') + 1])
    output = REPO / 'docs/dynamic-base-interior-assets.json'
    records = json.loads(output.read_text()) if output.exists() else {}
    for ref, low, high in re.findall(r'\[IA\]\[InteriorMeasure\] (\{[A-F0-9]+\}[^\r\n]+?) mins=<([^>]+)> maxs=<([^>]+)>', logfile.read_text()):
        records[ref.split('}', 1)[1]] = dict(resource=ref, mins=list(map(float, low.split(','))), maxs=list(map(float, high.split(','))))
    output.write_text(json.dumps(records, indent=2) + '\n')
    raise SystemExit()

if '--probe' in sys.argv:
    write_probe()
    raise SystemExit()

# Asset, x/y/z, yaw. Ground-contact Y is measured, not guessed. Supported items
# sit on the measured top of their table/crate/wheel, never on a lifted tent root.
MEASUREMENTS = json.loads((REPO / 'docs/dynamic-base-interior-assets.json').read_text())


def item(asset, x, z, yaw=0, support=0):
    bounds = MEASUREMENTS[ASSETS[asset]]
    # Message-board posts intentionally embed 30 cm into terrain.
    return asset, x, (0 if asset in ('board', 'latrine') else support - bounds['mins'][1]), z, yaw


table_top = MEASUREMENTS[ASSETS['table']]['maxs'][1]
crate_top = MEASUREMENTS[ASSETS['crate']]['maxs'][1] - MEASUREMENTS[ASSETS['crate']]['mins'][1]
wheel_height = MEASUREMENTS[ASSETS['wheel']]['maxs'][1] - MEASUREMENTS[ASSETS['wheel']]['mins'][1]
CLUSTERS = {
    'Briefing': [item('board', 0, 1.8), item('table', -.25, 0, 3), item('chair', -1.3, -.55, 65), item('crate', 1.35, -.05, -8)],
    'Mess': [item('table', 0, 0), item('chair', -1.1, 0, 90), item('chair', 1, .2, 260), item('chair', .05, -.95, 5), item('rations', .32, -.03, 4, table_top), item('rations', -.34, .04, -5, table_top)],
    'Stores': [item('crate', -.75, .1), item('crate', .65, .35, 12), item('rations', -.75, .1, 5, crate_top), item('sacks', -.15, -1.6, 15), item('can', 1.35, -.7, 8)],
    'Water': [item('barrel', -.65, .25), item('barrel', .25, .4, 12), item('bucket', .75, -.5), item('can', -.55, -.65, 8), item('can', -.2, -.7, -4)],
    'Workshop': [item('cable', -.95, .15, 10), item('wheel', .75, .2), item('wheel', .75, .2, 8, wheel_height), item('tools', .75, .2, 12, wheel_height * 2), item('crate', .15, -1.25, -8), item('can', -1.05, -.95, 5)],
    'Utility': [item('cable', -.55, .2, 20), item('tools', .55, -.4, -12), item('can', .6, .4, 8), item('can', .95, .4, -5)],
}

# Upgrade existing functions instead of stacking a second layer of clutter.
CLUSTERS['WaterWash'] = [item('sink', -.65, 0), item('barrel', .65, .25), item('bucket', -.65, -.6), item('can', .8, -.5)]
CLUSTERS['BulkWater'] = [item('tank', 0, 0), item('sink', -1.5, 0), item('bucket', -1.5, -.65)]
CLUSTERS['Sanitation'] = [item('latrine', 0, .6), item('sink', 1.5, -.3), item('can', 1.5, .3)]
CLUSTERS['Kitchen'] = [item('kitchen', 0, .4), item('table', 2.4, -.6, 90), item('rations', 2.4, -.6, 90, table_top), item('extinguisher', -1.5, -1.5)]
CLUSTERS['StoresCovered'] = [item('covered', -.85, .5), item('pallet', .9, .4, 8), item('case', .9, .4, 8, .144), item('extinguisher', -.9, -.6)]
CLUSTERS['Power'] = [item('generator', -.6, 0, 90), item('crate', .8, .3), item('electric', .8, -.202, 180, .45), item('extinguisher', -1.1, -.85), item('cable', .9, 1.55), item('nosmoking', 1.304, .3, 90, .3)]
CLUSTERS['Workshop'] = [item('table', -.8, 0), item('tools', -.8, 0, 8, table_top), item('wheel', .85, .15), item('wheel', .85, .15, 12, wheel_height), item('extinguisher', -1.3, -.6)]
CLUSTERS['Medical'] = [item('chair', -.8, .2, 10), item('chair', .1, .2, -10), item('medical', 1.2, .3), item('table', .2, -1.2)]
CLUSTERS['Rest'] = [item('chair', -.6, 0, 25), item('chair', .6, .25, -20), item('case', -1.2, .8)]
CLUSTERS['Comms'] = [item('table', 0, 0), item('case', 1.2, .3), item('tools', -.25, 0, 0, table_top)]
CLUSTERS['Waste'] = [item('bin', -.45, 0, 8), item('bin', .45, .1, -8)]
CLUSTERS['EntranceLight'] = [item('floodlight', 0, 0)]
for name, x, z in [('Briefing', .2, 0), ('Mess', -.4, .05), ('Kitchen', 2.4, -.2), ('Workshop', -.35, .1), ('Medical', .55, -1.2), ('Comms', .3, 0)]:
    CLUSTERS[name + 'Lit'] = (CLUSTERS[name][:-1] if name == 'Mess' else CLUSTERS[name]) + [item('lamp', x, z, 0, table_top)]

# Additional reserved working space in front of fixtures is part of the pad.
ACCESS_PADS = {'Sanitation': (2.4, 2.8), 'Kitchen': (3.5, 3.2), 'KitchenLit': (3.5, 3.2),
               'Power': (2.0, 2.5), 'BulkWater': (2.2, 3.4), 'EntranceLight': (2.0, 1.5)}

# Deliberate functional destinations. If a reserved lane/post/pad conflicts,
# choose the nearest clear authored metre, never move a tent or a wall.
SCENES = {
    'Full': [('BriefingLit',15,45), ('KitchenLit',-47,53), ('Mess',-46,-2), ('BulkWater',-78,32),
             ('StoresCovered',45,-26), ('Stores',30,-48), ('WorkshopLit',-65,-22), ('CommsLit',28,57),
             ('MedicalLit',78,33), ('Power',72,-32), ('StoresCovered',-30,-58), ('Rest',-15,43),
             ('Sanitation',-78,-4), ('Sanitation',78,3), ('Waste',76,-62), ('EntranceLight',9,-60)],
    'Compact': [('BriefingLit',16,30), ('KitchenLit',-21,27), ('BulkWater',-49,10), ('StoresCovered',24,-39),
                ('WorkshopLit',-15,-27), ('Power',43,-21), ('Rest',-34,43), ('Medical',42,8),
                ('Comms',29,42), ('Sanitation',-49,-14), ('Waste',43,-40), ('EntranceLight',9,-42)],
    'Courtyard': [('BriefingLit',16,29), ('KitchenLit',-25,30), ('WaterWash',-6,-25), ('StoresCovered',26,-7),
                  ('Power',35,-25), ('Medical',-7,-19), ('Sanitation',-37,31), ('Waste',37,30), ('EntranceLight',9,-29)],
    'Roadside': [('BriefingLit',17,33), ('Mess',-17,23), ('WaterWash',-17,-9), ('StoresCovered',18,22),
                 ('Power',17,-40), ('WorkshopLit',-17,-32), ('Sanitation',-22,38), ('Waste',20,-20), ('EntranceLight',9,-40)],
    'CommandPost': [('BriefingLit',18,18), ('Mess',-19,8), ('WaterWash',-19,-24), ('Stores',19,7),
                    ('Power',20,-15), ('Comms',8,20), ('Sanitation',-23,20), ('Waste',23,-23), ('EntranceLight',8,-21)],
    'RallyPost': [('MessLit',-9,-9), ('Stores',9,-10), ('WaterWash',-10,-17), ('Power',9,-18)],
}


def component(text, name):
    match = re.search(r'^  ' + name + r' [^\n]*\{\n.*?^  }\n', text, re.M | re.S)
    assert match, ('Missing stock component', name)
    return match[0]


def write_meta(out, stem):
    meta = [f'MetaFileClass {{', f' Name "{{{stable_id(stem)}}}{stem}.et"', ' Configurations {']
    for platform in ['PC', 'XBOX_ONE', 'XBOX_SERIES', 'PS4', 'PS5', 'HEADLESS']:
        meta += [f'  EntityTemplateResourceClass {platform}' + ('' if platform == 'PC' else ' : PC') + ' {', '  }']
    Path(str(out) + '.meta').write_text('\n'.join(meta + [' }', '}']) + '\n')


@cache
def scenery_resource(key):
    if key not in ('generator', 'lamp', 'floodlight', 'medical', 'covered', 'latrine', 'electric', 'nosmoking'):
        return resource(ASSETS[key])
    # Explicit component allowlist: no inventory, actions, campaign or extra lamp controller.
    stock = (BASE / ASSETS[key]).read_text()
    stem = 'IA_InfrastructureAsset_' + key
    out = REPO / 'Prefabs/DynamicBase' / (stem + '.et')
    if key == 'covered':
        stock = (BASE / 'Prefabs/Props/Military/Camps/PalletAmmo_01_Soviet.et').read_text()
    comps = component(stock, 'MeshObject')
    if key == 'nosmoking':
        base_sign = (BASE / 'Prefabs/Props/Signs/Industrial/SignIndustrial_30x20_1_base.et').read_text()
        mesh_object = re.search(r'^   Object [^\n]+', base_sign, re.M)[0]
        comps = comps.replace('{\n', '{\n' + mesh_object + '\n', 1)
    comps += '  RigidBody {\n   ModelGeometry 1\n   Static 1\n  }\n'
    comps += f'  Hierarchy "{{{stable_id(stem + "/hierarchy")}}}" {{\n   Enabled 1\n  }}\n'
    comps += '  RplComponent : "{B193E926C1935A4C}Prefabs/Editor/Components/Default_RplComponent.ct" {\n  }\n'
    if key == 'generator':
        comps += component(stock, 'StaticSoundComponent')
    if key in ('lamp', 'floodlight'):
        comps += component(stock, 'ParametricMaterialInstanceComponent')
        light = component(stock, 'SCR_BaseInteractiveLightComponent')
        light = light.replace('Enabled 0', 'Enabled 1').replace('LIT_ON_SPAWN', 'LIT')
        light = re.sub(r'm_eLightFlags \w+', 'm_eLightFlags 0', light)
        light = light.replace('m_fRadius 20', 'm_fRadius 14')
        if key == 'floodlight':
            light = light.replace('m_fConeAngle 120', 'm_fConeAngle 95\n     m_vLightConeDirection 0 -0.6 1')
        comps += light
    children = ''
    if key == 'covered':
        net = (BASE / 'Prefabs/Props/Military/Camps/CamoNet_PalletAmmo_Soviet.et').read_text()
        mesh = component(net, 'MeshObject')
        children = ' {\n  GenericEntity {\n   ID "' + stable_id(stem+'/net') + '"\n   components {\n' + ''.join('  '+line+'\n' for line in mesh.splitlines()) + '    Hierarchy {\n     Enabled 1\n    }\n   }\n  }\n }\n'
    if key == 'latrine':
        # Closed scenery door retains the stock model socket, without door actions.
        children = ' {\n  GenericEntity {\n   ID "' + stable_id(stem+'/door') + '"\n   components {\n    MeshObject {\n     Object "{F9451F4315B6F8D5}Assets/Structures/BuildingsParts/Doors/Door_Latrine/Door_Latrine_01_A_LEFT_EXT_COV_B.xob"\n    }\n    RigidBody {\n     ModelGeometry 1\n     Static 1\n    }\n    Hierarchy {\n     Enabled 1\n     PivotID "socket_door_ext_left_01"\n    }\n   }\n  }\n }\n'
    out.write_text('GenericEntity {\n ID "' + stable_id(stem+'/root') + '"\n components {\n' + comps + ' }\n coords 0 0 0\n' + children + '}\n')
    write_meta(out, stem)
    return '{' + stable_id(stem) + '}' + out.relative_to(REPO).as_posix()


def transformed_bounds(children):
    points = []
    for key, x, y, z, yaw in children:
        b = MEASUREMENTS[ASSETS[key]]
        a = math.radians(yaw)
        for u in (b['mins'][0], b['maxs'][0]):
            for v in (b['mins'][2], b['maxs'][2]):
                points.append((x + math.cos(a)*u + math.sin(a)*v, z - math.sin(a)*u + math.cos(a)*v))
    return tuple(math.ceil((max(abs(p[axis]) for p in points) + .3)*10)/10 for axis in (0, 1))


def check_supported_props(children):
    bounds = []
    for key, x, y, z, yaw in children:
        b = MEASUREMENTS[ASSETS[key]]
        # Horizontal tires are circular; rotating their AABB invents overhang.
        a = math.radians(0 if key == 'wheel' else yaw)
        corners = [(x + math.cos(a)*u + math.sin(a)*v, z - math.sin(a)*u + math.cos(a)*v)
                   for u in (b['mins'][0], b['maxs'][0]) for v in (b['mins'][2], b['maxs'][2])]
        bounds.append((key, min(p[0] for p in corners), min(p[1] for p in corners),
                       max(p[0] for p in corners), max(p[1] for p in corners), y+b['mins'][1], y+b['maxs'][1]))
    for key, x1, z1, x2, z2, bottom, top in bounds:
        if key not in ('lamp', 'rations', 'tools', 'wheel') or bottom < .02:
            continue
        assert any(other in ('table', 'crate', 'wheel') and abs(high-bottom) < .005
                   and x1 >= left-.01 and x2 <= right+.01 and z1 >= back-.01 and z2 <= front+.01
                   for other,left,back,right,front,low,high in bounds), ('Unsupported raised prop', key, x1, z1)


def write_cluster(name, children):
    stem = 'IA_Dressing_' + name
    out = REPO / 'Prefabs/DynamicBase' / (stem + '.et')
    lines = ['GenericEntity {', f' ID "{stable_id(stem + "/root")}"', ' components {',
             '  RplComponent "{54AC8D6E673199EE}" : "{B193E926C1935A4C}Prefabs/Editor/Components/Default_RplComponent.ct" {', '  }',
             '  Hierarchy "{5DC704E4D968FC31}" {', '   Enabled 1', '  }', ' }', ' Flags 0x403 0', ' coords 0 0 0', ' {']
    for i, (key, x, y, z, yaw) in enumerate(children):
        ref = scenery_resource(key)
        assert ref.startswith('{'), ('Measure asset first', key)
        hierarchy = stable_id('IA_InfrastructureAsset_' + key + '/hierarchy') if 'IA_InfrastructureAsset_' in ref else stable_id(stem + '/hierarchy/' + str(i))
        lines += [f'  GenericEntity : "{ref}" {{', f'   ID "{stable_id(stem + "/" + str(i))}"',
                  '   components {', f'    Hierarchy "{{{hierarchy}}}" {{', '     Enabled 1', '    }', '   }',
                  f'   coords {x:g} {y:.6f} {z:g}', f'   angles 0 {yaw:g} 0', '  }']
    out.write_text('\n'.join(lines + [' }', '}']) + '\n')
    meta = [f'MetaFileClass {{', f' Name "{{{stable_id(stem)}}}{stem}.et"', ' Configurations {']
    for platform in ['PC', 'XBOX_ONE', 'XBOX_SERIES', 'PS4', 'PS5', 'HEADLESS']:
        meta += [f'  EntityTemplateResourceClass {platform}' + ('' if platform == 'PC' else ' : PC') + ' {', '  }']
    Path(str(out) + '.meta').write_text('\n'.join(meta + [' }', '}']) + '\n')
    return '{' + stable_id(stem) + '}' + out.relative_to(REPO).as_posix()


sys.path.insert(0, str(REPO / 'tools'))
import check_dynamic_base_layouts as checks
layout_file = REPO / 'Scripts/Game/IA_DynamicSiteLayout.c'
layout_text = layout_file.read_text()
walls_before = re.findall(r'^.*layout.AddCover.*$', layout_text, re.M)
layout_text = re.sub(r'\n\t\t// INTERIOR DRESSING BEGIN.*?\t\t// INTERIOR DRESSING END\n', '\n', layout_text, flags=re.S)
layout_text = re.sub(r'^\tstatic const ResourceName PREFAB_DRESSING_.*\n', '', layout_text, flags=re.M)
layout_text = re.sub(r'\n{3,}(?=\tint m_iLayoutId;)', '\n\n', layout_text)
checks.TEXT = layout_text
constants = []
pads = {}
for name, children in CLUSTERS.items():
    check_supported_props(children)
    constants.append(f'\tstatic const ResourceName PREFAB_DRESSING_{name.upper()} = "{write_cluster(name, children)}";')
    pads[name] = tuple(max(a,b) for a,b in zip(transformed_bounds(children), ACCESS_PADS.get(name, (0,0))))
layout_text = layout_text.replace('\tint m_iLayoutId;', '\n'.join(constants) + '\n\n\tint m_iLayoutId;', 1)
placements = {}


def choose_spot(target, pad, half_w, half_d, boxes, guards, cross, capture, kind):
    routes = [((0, -half_d), (0, cross)), ((-half_w, cross), (half_w, cross)), ((0, cross), capture)]
    def clear(x, z, w, d):
        box = (x-w, z-d, x+w, z+d)
        if abs(x)+w > half_w-3 or abs(z)+d > half_d-3:
            return False
        for _, b in boxes:
            if min(box[2]+1, b[2]) > max(box[0]-1, b[0]) and min(box[3]+1, b[3]) > max(box[1]-1, b[1]):
                return False
        if kind in ('Sanitation', 'Waste'):
            for ident, b in boxes:
                if any(tag in ident for tag in ('dressing_kitchen', 'dressing_mess', 'dressing_water', 'dressing_bulkwater')):
                    gap_x = max(box[0]-b[2], b[0]-box[2], 0)
                    gap_z = max(box[1]-b[3], b[1]-box[3], 0)
                    if math.hypot(gap_x, gap_z) < 6:
                        return False
        if any(checks.inside(post, box, 1.8) for post in guards):
            return False
        for start, end in routes:
            steps = max(1, math.ceil(math.dist(start, end)/.25))
            for i in range(steps+1):
                point = tuple(start[a] + (end[a]-start[a])*i/steps for a in (0, 1))
                if checks.inside(point, box, 2.5):
                    return False
        return True
    # This is offline authoring, not the runtime survey; search the whole layout
    # for the closest clear pocket if a facility's preferred edge is occupied.
    options = sorted((dx*dx+dz*dz, dx, dz) for dx in range(-80, 81) for dz in range(-80, 81))
    for _, dx, dz in options:
        for yaw in ((0,) if kind == 'EntranceLight' else (0, 90, 180, 270)):
            w, d = pad if yaw % 180 == 0 else pad[::-1]
            x, z = target[0]+dx, target[1]+dz
            if clear(x, z, w, d):
                return x, z, yaw, (x-w, z-d, x+w, z+d)
    raise ValueError(('No clear dressing location', target, pad))


for name, scenes in SCENES.items():
    half_w, half_d, _, capture, cross, _, boxes, guards = checks.read_layout(name)
    additions = ['\t\t// INTERIOR DRESSING BEGIN']
    placements[name] = []
    for i, (kind, x, z) in enumerate(scenes):
        x, z, yaw, box = choose_spot((x, z), pads[kind], half_w, half_d, boxes, guards, cross, capture, kind)
        ident = 'dressing_' + kind.lower() + '_' + str(i)
        w, d = pads[kind]
        expanded = len(CLUSTERS[kind]) + 1 + sum(3 if child[0] == 'board' else 1 if child[0] in ('covered', 'latrine') else 0 for child in CLUSTERS[kind])
        additions.append(f'\t\tlayout.AddDressing("{ident}", PREFAB_DRESSING_{kind.upper()}, {x:g}, {z:g}, {yaw}, {w:g}, {d:g}, {expanded});')
        boxes.append((ident, box))
        placements[name].append(dict(id=ident, kind=kind, x=x, z=z, yaw=yaw, half_width=w, half_depth=d))
    additions += ['\t\t// INTERIOR DRESSING END', '']
    start = layout_text.index('static IA_DynamicSiteLayout Create' + name + '()')
    end = layout_text.index('\t\treturn layout;', start)
    layout_text = layout_text[:end] + '\n'.join(additions) + layout_text[end:]
assert walls_before == re.findall(r'^.*layout.AddCover.*$', layout_text, re.M), 'Sandbag declarations changed'
layout_file.write_text(layout_text)
(REPO / 'docs/dynamic-base-interior-scenes.json').write_text(json.dumps(dict(pads=pads, scenes=placements), indent=2) + '\n')
write_probe()
print('Authored', len(CLUSTERS), 'vignettes;', {k: len(v) for k, v in placements.items()}, '; sandbags unchanged')
