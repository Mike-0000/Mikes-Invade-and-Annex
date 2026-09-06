"""Author explicit, closely spaced sandbag runs around each base, retaining gates.

No random placement: styles repeat in a fixed solid / firing-opening / high
parapet rhythm, with curved high sections terminating runs beside gateways.
"""
from pathlib import Path
import math
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Scripts/Game/IA_DynamicSiteLayout.c'
text = SOURCE.read_text(encoding='utf-8')

for name, w, d, cross in [('Full',90,70,-8),('Compact',60,50,-5),
                         ('Courtyard',44,38,-2),('Roadside',30,48,-10),
                         ('CommandPost',32,28,2),('RallyPost',18,24,-10)]:
    start = text.index('static IA_DynamicSiteLayout Create' + name + '()')
    end = text.index('return layout;', start)
    section = text[start:end]
    old = re.findall(r'^\s*layout.AddCover\([^\n]+\);', section, re.M)
    assert old
    x, z = w - 0.9, d - 0.9
    gate = 6 if name in ('Full','Compact') else 4
    runs = [('rear', -x, x, z, 0, 0),
            ('frontW', -x, -gate, -z, 180, 2),
            ('frontE', gate, x, -z, 180, 2),
            ('westS', -z, cross-4, -x, 270, 3),
            ('westN', cross+4, z, -x, 270, 3),
            ('eastS', -z, cross-4, x, 90, 1),
            ('eastN', cross+4, z, x, 90, 1)]
    lines = []
    for run, lo, hi, fixed, yaw, side in runs:
        first, last = lo + 1.8, hi - 1.8
        count = max(1, math.ceil((last-first)/3) + 1)
        for i in range(count):
            v = (lo+hi)/2 if count == 1 else first + (last-first)*i/(count-1)
            px, pz = (v, fixed) if yaw % 180 == 0 else (fixed, v)
            style = [0, 1, 0, 2][i % 4]
            if run != 'rear' and (i == 0 or i == count-1):
                style = 3
            lines.append(f'\t\tlayout.AddCover("wall_{run}_{i:02d}", {px:.3f}, {pz:.3f}, {yaw}, {style}, {side});')
    first_cover = section.index(old[0])
    section = re.sub(r'^\s*layout.AddCover\([^\n]+\);', '', section, flags=re.M)
    section = section[:first_cover] + '\n\n' + '\n'.join(lines) + '\n' + section[first_cover:]
    text = text[:start] + section + text[end:]

# Rear guard positions must stay inside the new continuous back wall.
text = text.replace('Vector(0, 0, 36)', 'Vector(-15, 0, 33)')
text = text.replace('Vector(0, 0, 45)', 'Vector(0, 0, 44.7)')
text = text.replace('Vector(0, 0, 27)', 'Vector(-15, 0, 24)')
text = re.sub(r'\n{3,}', '\n\n', text)
SOURCE.write_text(text, encoding='utf-8', newline='\n')
