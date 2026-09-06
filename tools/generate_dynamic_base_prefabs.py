"""Rebuild scenery-only base modules from an extracted Reforger data directory.

Usage: python tools/generate_dynamic_base_prefabs.py D:/ReforgerGameSources/data/data007
Keeps authored child transforms, removes campaign roots and AI/arsenal/service
children, and recursively replaces nested compositions with scenery roots.
"""
from pathlib import Path
import re
import sys
import uuid

REPO = Path(__file__).resolve().parents[1]
BASE = Path(sys.argv[1])
OUT = REPO / 'Prefabs/DynamicBase'
OUT.mkdir(exist_ok=True)
GENERATED = {}
SOURCES = {
    'HQ': 'Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et',
    'BARRACKS': 'Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et',
    'MEDICAL': 'Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Medical_USSR_01.et',
    'SUPPLY': 'Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Supply_Large_USSR_01.et',
    'WORKSHOP': 'Prefabs/Compositions/Slotted/SlotFlatMedium/VehicleMaintenance_M_USSR_01.et',
    'FUEL': 'Prefabs/Compositions/Slotted/SlotFlatSmall/FuelStorage_S_USSR_01.et',
}


def guid(name):
    return uuid.uuid5(uuid.NAMESPACE_URL, 'ia-dynamic-base/' + name).hex[:16].upper()


def write(name, children, physical_parent=None, hierarchy_id=None):
    resource = f'Prefabs/DynamicBase/{name}.et'
    header = '''GenericEntity {
 ID "5282DCB5DFC8538F"
 components {
  RplComponent "{54AC8D6E673199EE}" : "{B193E926C1935A4C}Prefabs/Editor/Components/Default_RplComponent.ct" {
  }
  Hierarchy "{5DC704E4D968FC31}" {
   Enabled 1
  }
 }
 Flags 0x403 0
 coords 0 0 0
 {
'''
    if physical_parent:
        # Furniture subcompositions often inherit the chair/table mesh itself.
        # Preserve that physical root, or its authored clutter would float in air.
        header = header.replace('GenericEntity {', f'GenericEntity : "{physical_parent}" {{', 1)
    if hierarchy_id:
        header = header.replace('5DC704E4D968FC31', hierarchy_id)
    (REPO / resource).write_text(header + children + ' }\n}\n', encoding='utf-8')
    meta = f'MetaFileClass {{\n Name "{{{guid(name)}}}{name}.et"\n Configurations {{\n'
    for platform in ['PC', 'XBOX_ONE', 'XBOX_SERIES', 'PS4', 'PS5', 'HEADLESS']:
        parent = '' if platform == 'PC' else ' : PC'
        meta += f'  EntityTemplateResourceClass {platform}{parent} {{\n  }}\n'
    (REPO / (resource + '.meta')).write_text(meta + ' }\n}\n', encoding='utf-8')
    return '{' + guid(name) + '}' + resource


def wrapper(path):
    if path in GENERATED:
        return GENERATED[path]
    text = (BASE / path).read_text()
    parts = text.split('\n {\n', 1)
    if len(parts) != 2:
        raise ValueError('Composition without explicit children: ' + path)
    children = parts[1].rsplit('\n }\n}', 1)[0] + '\n'
    blocks = re.findall(r'^  \S[^\n]*\{\n.*?^  }\n', children, re.M | re.S)
    assert ''.join(blocks) == children, path
    kept = []
    for block in blocks:
        match = re.search(r'"\{[0-9A-F]+\}([^"\n]+\.et)"', block.split('\n')[0])
        if match:
            child_path = match[1]
            # The HQ may be lifted on its stock earth foundation. Omit its
            # ground-level sign and decorative dirt mound so they cannot hover;
            # all replicated children keep their authored prefab transforms.
            if path.endswith('/Tent_CommandPost_USSR_01.et') and ('/Signs/' in child_path or '/DirtPile_01/' in child_path):
                continue
            if path.endswith('/Tent_CommandPost_USSR_01.et') and child_path.endswith('/TentUSSR_01_camonet_CompositionDestruction.et'):
                # Use the same tent/floor/earth foundation without the separate
                # wide camonet poles, which cannot follow a lifted foundation.
                # Discard the old camonet foundation's child-ID override too.
                kept.append('''  StaticModelEntity : "{9F7211592BE80DC2}Prefabs/Compositions/Misc/CustomEntities/DestructionEntities/TentUSSR_01_CompositionDestruction.et" {
   ID "5CB6B281C4F9CB49"
   coords 0 0 0
  }
''')
                continue
            if '/Systems/' in child_path or '/Arsenal/' in child_path or re.search(r'(Fuel|Repair|Medical|Vehicle)Service', child_path):
                continue
            # Repeated litter/decals consume the base-wide entity budget quickly.
            # Keep the furniture, beds, equipment and storage that define each room.
            if '/Garbage/' in child_path or '/CustomEntities/CompositionDecals/' in child_path or '/Vegetation/Cuttings/' in child_path:
                continue
            if child_path.startswith('Prefabs/Compositions/') and '/CustomEntities/' not in child_path:
                block = block.replace(match[0], '"' + wrapper(child_path) + '"', 1)
        kept.append(block)
    name = 'IA_' + Path(path).stem
    parent = re.search(r'^\w+ : "(\{[0-9A-F]+\}Prefabs/(?:Props|Structures)/[^\"]+)"', text)
    hierarchy = re.search(r'Hierarchy "\{([0-9A-F]+)\}"', parts[0])
    GENERATED[path] = write(name, ''.join(kept), parent[1] if parent else None, hierarchy[1] if hierarchy else None)
    return GENERATED[path]


layout_path = REPO / 'Scripts/Game/IA_DynamicSiteLayout.c'
layout = layout_path.read_text()
for key, source in SOURCES.items():
    resource = wrapper(source)
    layout = re.sub(rf'(PREFAB_{key} = ")[^"\n]+(";)', lambda m: m[1] + resource + m[2], layout)

layout_path.write_text(layout, encoding='utf-8')
for source, target in GENERATED.items():
    print(source, '->', target)
