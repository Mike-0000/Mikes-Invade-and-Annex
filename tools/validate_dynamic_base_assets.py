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
