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
    refs += ['{' + stable_id(p.stem) + '}' + p.relative_to(REPO).as_posix()
             for p in sorted((REPO / 'Prefabs/DynamicBase').glob('IA_Dressing_*.et'))]
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
        foreach (ResourceName name : resources)
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
'''.replace('ITEMS', items)
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
    return asset, x, (0 if asset == 'board' else support - bounds['mins'][1]), z, yaw


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

# Deliberate functional destinations. If a reserved lane/post/pad conflicts,
# choose the nearest clear authored metre, never move a tent or a wall.
SCENES = {
    'Full': [('Briefing', 15, 45), ('Mess', -47, 53), ('Mess', -46, -2), ('Water', -78, 32), ('Stores', 45, -26), ('Stores', 30, -48), ('Workshop', -65, -22), ('Utility', 28, 57), ('Water', 78, 33), ('Utility', 72, -32), ('Stores', -30, -58), ('Mess', -15, 43)],
    'Compact': [('Briefing', 16, 30), ('Mess', -21, 27), ('Water', -49, 10), ('Stores', 24, -39), ('Workshop', -15, -27), ('Utility', 43, -21), ('Mess', -34, 43), ('Water', 42, 8)],
    'Courtyard': [('Briefing', 16, 29), ('Mess', -25, 30), ('Water', -6, -25), ('Stores', 26, -7), ('Utility', 39, -25), ('Mess', 25, 30)],
    'Roadside': [('Briefing', 17, 33), ('Mess', -17, 23), ('Water', -17, -9), ('Stores', 18, 22), ('Utility', 17, -42)],
    'CommandPost': [('Briefing', 18, 18), ('Mess', -19, 8), ('Water', -19, -24), ('Stores', 19, 7)],
    'RallyPost': [('Mess', -9, -9), ('Stores', 9, -10), ('Water', -10, -17)],
}


def transformed_bounds(children):
    points = []
    for key, x, y, z, yaw in children:
        b = MEASUREMENTS[ASSETS[key]]
        a = math.radians(yaw)
        for u in (b['mins'][0], b['maxs'][0]):
            for v in (b['mins'][2], b['maxs'][2]):
                points.append((x + math.cos(a)*u + math.sin(a)*v, z - math.sin(a)*u + math.cos(a)*v))
    return tuple(math.ceil((max(abs(p[axis]) for p in points) + .3)*10)/10 for axis in (0, 1))


def write_cluster(name, children):
    stem = 'IA_Dressing_' + name
    out = REPO / 'Prefabs/DynamicBase' / (stem + '.et')
    lines = ['GenericEntity {', f' ID "{stable_id(stem + "/root")}"', ' components {',
             '  RplComponent "{54AC8D6E673199EE}" : "{B193E926C1935A4C}Prefabs/Editor/Components/Default_RplComponent.ct" {', '  }',
             '  Hierarchy "{5DC704E4D968FC31}" {', '   Enabled 1', '  }', ' }', ' Flags 0x403 0', ' coords 0 0 0', ' {']
    for i, (key, x, y, z, yaw) in enumerate(children):
        ref = resource(ASSETS[key])
        assert ref.startswith('{'), ('Measure asset first', key)
        lines += [f'  GenericEntity : "{ref}" {{', f'   ID "{stable_id(stem + "/" + str(i))}"',
                  '   components {', f'    Hierarchy "{{{stable_id(stem + "/hierarchy/" + str(i))}}}" {{', '     Enabled 1', '    }', '   }',
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
    constants.append(f'\tstatic const ResourceName PREFAB_DRESSING_{name.upper()} = "{write_cluster(name, children)}";')
    pads[name] = transformed_bounds(children)
layout_text = layout_text.replace('\tint m_iLayoutId;', '\n'.join(constants) + '\n\n\tint m_iLayoutId;', 1)
placements = {}


def choose_spot(target, pad, half_w, half_d, boxes, guards, cross, capture):
    routes = [((0, -half_d), (0, cross)), ((-half_w, cross), (half_w, cross)), ((0, cross), capture)]
    def clear(x, z, w, d):
        box = (x-w, z-d, x+w, z+d)
        if abs(x)+w > half_w-3 or abs(z)+d > half_d-3:
            return False
        for _, b in boxes:
            if min(box[2]+1, b[2]) > max(box[0]-1, b[0]) and min(box[3]+1, b[3]) > max(box[1]-1, b[1]):
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
    options = sorted((dx*dx+dz*dz, dx, dz) for dx in range(-12, 13) for dz in range(-12, 13))
    for _, dx, dz in options:
        for yaw in (0, 90, 180, 270):
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
        x, z, yaw, box = choose_spot((x, z), pads[kind], half_w, half_d, boxes, guards, cross, capture)
        ident = 'dressing_' + kind.lower() + '_' + str(i)
        w, d = pads[kind]
        expanded = 8 if kind == 'Briefing' else len(CLUSTERS[kind]) + 1
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
