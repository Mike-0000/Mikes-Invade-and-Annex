"""Review sheet for authored base interiors, using the actual layout manifest."""
from pathlib import Path
from html import escape
import json
from check_dynamic_base_layouts import read_layout

ROOT = Path(__file__).resolve().parents[1]
scenes = json.loads((ROOT / 'docs/dynamic-base-interior-scenes.json').read_text())['scenes']
colors = dict(Briefing='#e6a7d9', Mess='#9ed5a0', Stores='#e9c579', Water='#83c9e5', Workshop='#f2a77d', Utility='#c7c0ee')
parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="990" viewBox="0 0 1440 990">',
         '<title>Dynamic base interiors: authored scene placement</title>',
         '<rect width="1440" height="990" fill="#142027"/>',
         '<style>text{font-family:Segoe UI,Arial,sans-serif;fill:#e8eded}.muted{fill:#afc0c6;font-size:13px}</style>',
         '<text x="28" y="36" font-size="24" font-weight="600">Field-base interiors</text>',
         '<text x="28" y="60" class="muted">Purposeful pockets of activity · unchanged sandbag walls · clear approach lanes · all plans at the same scale</text>']

for index, name in enumerate(scenes):
    half_w, half_d, radius, capture, cross, guards, boxes, posts = read_layout(name)
    ox, oy = 16 + (index % 3)*474, 80 + (index // 3)*397
    cx, cy, scale = ox + 234, oy + 215, 2.1
    def pt(x, z):
        return cx + x*scale, cy-z*scale
    title = {'CommandPost': 'Command post', 'RallyPost': 'Rally post'}.get(name, name)
    parts += [f'<rect x="{ox}" y="{oy}" width="462" height="385" rx="12" fill="#21313a"/>',
              f'<text x="{ox+18}" y="{oy+28}" font-size="19" font-weight="600">{title}</text>',
              f'<text x="{ox+18}" y="{oy+48}" class="muted">{half_w*2:g} × {half_d*2:g} m · {len(scenes[name])} optional scenes</text>']
    for start, end in [((0,-half_d),(0,cross)), ((-half_w,cross),(half_w,cross)), ((0,cross),capture)]:
        x1,y1 = pt(*start); x2,y2 = pt(*end)
        parts.append(f'<path d="M{x1},{y1} L{x2},{y2}" stroke="#3e6563" stroke-width="10"/>')
    for module, (left,bottom,right,top) in boxes:
        if module.startswith('dressing_'):
            continue
        x,y = pt(left,top)
        fill = '#82988b' if module.startswith('wall_') else '#465b68'
        parts.append(f'<rect x="{x}" y="{y}" width="{(right-left)*scale}" height="{(top-bottom)*scale}" fill="{fill}" rx="1"/>')
        if not module.startswith('wall_'):
            parts.append(f'<text x="{x+(right-left)*scale/2}" y="{y+(top-bottom)*scale/2+3}" font-size="9" text-anchor="middle">{escape(module.upper())}</text>')
    for scene in scenes[name]:
        x,y = pt(scene['x'],scene['z'])
        w,d = scene['half_width'],scene['half_depth']
        if scene['yaw'] % 180:
            w,d = d,w
        parts.append(f'<rect x="{x-w*scale}" y="{y-d*scale}" width="{2*w*scale}" height="{2*d*scale}" fill="{colors[scene["kind"]]}" stroke="#f5f2e8" stroke-width=".7"><title>{scene["kind"]}: {scene["x"]}, {scene["z"]}</title></rect>')
    for post in posts:
        x,y=pt(*post)
        parts.append(f'<circle cx="{x}" cy="{y}" r="2" fill="#fff"/>')
    parts.append(f'<text x="{cx}" y="{oy+369}" text-anchor="middle" class="muted">↑ Main entrance</text>')

for i,(name,color) in enumerate(colors.items()):
    x=32+i*230
    parts += [f'<rect x="{x}" y="894" width="15" height="15" rx="2" fill="{color}"/>',
              f'<text x="{x+24}" y="906" font-size="14">{name}</text>']
parts += ['<text x="28" y="942" class="muted">Colored rectangles are measured vignette pads; white dots are guard posts. Paths remain clear.</text>',
          '<text x="28" y="965" class="muted">Authored plan, not an in-game screenshot. Individual scenes are omitted where terrain or existing objects prevent a clean fit.</text>', '</svg>']
output=ROOT/'docs/dynamic-base-interiors.svg'
output.write_text('\n'.join(parts),encoding='utf-8')
print(output)
