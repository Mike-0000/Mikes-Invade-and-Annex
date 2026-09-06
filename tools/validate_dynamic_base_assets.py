"""Check base prefab references and conservative expanded-entity budgets.

Usage: python tools/validate_dynamic_base_assets.py D:/ReforgerGameSources/data/data007
This is a source-level asset check, not a replacement for Workbench/playtesting.
Inherited entity additions are counted conservatively (overrides can overcount).
"""
from functools import cache
from pathlib import Path
import re
import sys
import json
import math

REPO = Path(__file__).resolve().parents[1]
BASE = Path(sys.argv[1])
REF = re.compile(r'(?m)^( *)(?:\$grp )?\w+ : "\{[0-9A-F]+\}([^"\n]+\.et)" \{')


def source(path):
    local = REPO / path
    return (local if local.exists() else BASE / path).read_text(encoding='utf-8-sig')


@cache
def count(path):
    text = source(path)
    total = max(1, len(re.findall(r'(?m)^\s*ID "', text)))
    for match in REF.finditer(text):
        indent = match[1]
        multiplier = 1
        if '$grp ' in match[0]:
            end = text.index('\n' + indent + '}', match.end())
            block = text[match.end():end]
            multiplier = len(re.findall(r'(?m)^' + indent + r'  ID "', block))
            assert multiplier > 0, (path, match[0])
        total += multiplier * (count(match[2]) - 1)
    return total


layout = (REPO / 'Scripts/Game/IA_DynamicSiteLayout.c').read_text()
measurements = json.loads((REPO / 'docs/dynamic-base-interior-assets.json').read_text())
scenes = json.loads((REPO / 'docs/dynamic-base-interior-scenes.json').read_text())
for kind, (half_width, half_depth) in scenes['pads'].items():
    path = f'Prefabs/DynamicBase/IA_Dressing_{kind}.et'
    bounds = measurements[path]
    for axis, extent in [(0, half_width), (2, half_depth)]:
        assert -extent <= bounds['mins'][axis] <= bounds['maxs'][axis] <= extent, (kind, 'measured bounds exceed pad')

@cache
def ambience(path):
    text = source(path)
    lights = len(re.findall(r'^\s*SCR_BaseLightData ', text, re.M))
    sounds = text.count('StaticSoundComponent ')
    for ref in REF.finditer(text):
        child_lights, child_sounds = ambience(ref[2])
        lights += child_lights
        sounds += child_sounds
    return lights, sounds

limits = dict(Full=6, Compact=4, Courtyard=3, Roadside=3, CommandPost=2, RallyPost=1)
for name, placements in scenes['scenes'].items():
    totals = [ambience(f'Prefabs/DynamicBase/IA_Dressing_{scene["kind"]}.et') for scene in placements]
    assert sum(t[0] for t in totals) <= limits[name], (name, 'light budget')
    assert sum(t[1] for t in totals) == 1, (name, 'exactly one generator ambience source')
    for scene in placements:
        if scene['kind'] == 'EntranceLight':
            assert scene['yaw'] == 0 and scene['z'] < 0, (name, 'entrance light must face inward')
        if scene['kind'] not in ('Sanitation', 'Waste'):
            continue
        def box(s):
            w,d = s['half_width'],s['half_depth']
            if s['yaw'] % 180:
                w,d = d,w
            return s['x']-w,s['z']-d,s['x']+w,s['z']+d
        a = box(scene)
        for other in placements:
            if other['kind'] not in ('KitchenLit','Mess','MessLit','WaterWash','BulkWater'):
                continue
            b = box(other)
            assert math.hypot(max(a[0]-b[2],b[0]-a[2],0), max(a[1]-b[3],b[1]-a[3],0)) >= 6, (name, 'hygiene separation')
    print(f'{name}: {sum(t[0] for t in totals)}/{limits[name]} lights, one ambience source')
for prefab in (REPO / 'Prefabs/DynamicBase').glob('IA_InfrastructureAsset_*.et'):
    text = prefab.read_text()
    assert not re.search(r'ActionsManager|Inventory|Service|SCR_LampComponent|SoundComponent ', text.replace('StaticSoundComponent ', '')), prefab
    for flag in re.findall(r'm_eLightFlags (\S+)', text):
        assert flag == '0', (prefab, 'shadow casting enabled')
    for radius in re.findall(r'm_fRadius ([\d.]+)', text):
        assert float(radius) <= (20 if 'floodlight' in prefab.name else 6), (prefab, 'light radius')
    if 'SCR_BaseInteractiveLightComponent' in text:
        assert 'm_eInitialLightState LIT\n' in text and 'Enabled 0' not in text, (prefab, 'fixed initial illumination')
prefabs = dict(re.findall(r'(PREFAB_\w+) = "\{[0-9A-F]+\}([^"\n]+)"', layout))
for key, path in prefabs.items():
    if key != 'PREFAB_TOWER':  # not used by either layout
        print(f'{key}: <= {count(path)} entities')

for name in ['Full', 'Compact', 'Courtyard', 'Roadside', 'CommandPost', 'RallyPost']:
    section = layout.split('static IA_DynamicSiteLayout Create' + name + '()', 1)[1].split('return layout;', 1)[0]
    modules = re.findall(r'layout.Add(?:Module|Dressing)\("\w+", (PREFAB_\w+),', section)
    covers = section.count('layout.AddCover(')
    roots = len(modules) + covers
    expanded = sum(count(prefabs[key]) for key in modules) + covers * count(prefabs['PREFAB_COVER'])
    for key, estimate in re.findall(r'layout.AddDressing\("\w+", (PREFAB_\w+),[^;]*, (\d+)\);', section):
        assert int(estimate) >= count(prefabs[key]), (name, key, 'underestimated dressing entities')
    assert roots <= 256, (name, roots)
    placer = (REPO / 'Scripts/Game/IA_DynamicSitePlacer.c').read_text()
    ceiling = int(re.search(r'MAX_EXPANDED = (\d+)', placer)[1])
    assert expanded <= ceiling, (name, expanded)
    print(f'{name}: {roots} roots, <= {expanded}/{ceiling} expanded entities')

guids = set()
for prefab in (REPO / 'Prefabs/DynamicBase').glob('*.et'):
    text = prefab.read_text()
    assert text.startswith('GenericEntity {') or re.match(r'GenericEntity : "\{[0-9A-F]+\}Prefabs/(Props|Structures)/', text), prefab
    assert 'SCR_Campaign' not in text and 'ActionsManagerComponent' not in text, prefab
    assert '/Systems/' not in text and '/Arsenal/' not in text, prefab
    meta = Path(str(prefab) + '.meta').read_text()
    guid, name = re.search(r'Name "\{([0-9A-F]+)\}([^\"]+)"', meta).groups()
    assert name == prefab.name and guid not in guids, prefab
    guids.add(guid)
    for ref_guid, path in re.findall(r'"\{([0-9A-F]+)\}([^"\n]+\.et)"', text):
        source(path)  # Every entity reference must exist.
        if path.startswith('Prefabs/DynamicBase/'):
            assert '{' + ref_guid + '}' in Path(str(REPO / path) + '.meta').read_text(), path
print(f'PASS: {len(guids)} scenery prefabs, unique metadata and valid entity references')
