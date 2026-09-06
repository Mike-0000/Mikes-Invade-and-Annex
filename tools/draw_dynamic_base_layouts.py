"""Draw the authored small-layout pads at a common scale, from the game manifest."""
from pathlib import Path
from html import escape
import sys
sys.dont_write_bytecode = True
from check_dynamic_base_layouts import read_layout

DEST = Path(__file__).resolve().parents[1] / 'docs/dynamic-base-layouts.svg'
DEST.parent.mkdir(exist_ok=True)
COLORS = {'hq': '#c5a3ec', 'barracks': '#97bcdb', 'medical': '#e6a59f', 'supply': '#d7c582', 'fuel': '#e9b073', 'cover': '#667a73'}
parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="1434" height="570" viewBox="0 0 1434 570" role="img" aria-labelledby="title desc">',
         '<title id="title">Smaller enemy base layouts</title><desc id="desc">Four authored floor plans drawn to the same scale. Rectangles show reserved module pads, green circles show capture zones, and dots show guard posts.</desc>',
         '<rect width="1434" height="570" fill="#101b21"/>',
         '<style>text{font-family:Segoe UI,Arial,sans-serif;fill:#e4ebeb}.label{font-size:9px;fill:#15242b;font-weight:600}.note{font-size:13px;fill:#aabcc4}</style>',
         '<text x="26" y="32" font-size="21" font-weight="600">Smaller bases · full-size buildings</text>',
         '<text x="26" y="53" class="note">Reserved module pads at the same scale. +Z is the rear; the main entrance is below each plan.</text>']

for index, (name, title) in enumerate([('Courtyard', 'Courtyard'), ('Roadside', 'Roadside'), ('CommandPost', 'Command post'), ('RallyPost', 'Rally post')]):
    half_w, half_d, radius, capture, cross_z, guards, boxes, posts = read_layout(name)
    card_x = 18 + index * 354
    cx, cy, scale = card_x + 168, 300, 2.8
    def point(x, z):
        return cx + x * scale, cy - z * scale
    parts += [f'<rect x="{card_x}" y="72" width="340" height="417" rx="12" fill="#1b2b33"/>',
              f'<text x="{card_x+16}" y="102" font-size="19" font-weight="600">{title}</text>',
              f'<text x="{card_x+16}" y="125" class="note">{half_w*2:g} × {half_d*2:g} m · up to {guards} occupying guards</text>']
    x, y = point(-half_w, half_d)
    parts.append(f'<rect x="{x}" y="{y}" width="{half_w*2*scale}" height="{half_d*2*scale}" fill="#263b40" stroke="#7b9597" stroke-dasharray="4 4"/>')
    for start, end in [((0, -half_d), (0, cross_z)), ((-half_w, cross_z), (half_w, cross_z)), ((0, cross_z), capture)]:
        x1, y1 = point(*start)
        x2, y2 = point(*end)
        parts.append(f'<path d="M{x1},{y1} L{x2},{y2}" stroke="#64867d" stroke-width="8" fill="none"/>')
    x, y = point(*capture)
    parts.append(f'<circle cx="{x}" cy="{y}" r="{radius*scale}" fill="#60c29b" fill-opacity=".08" stroke="#6fd2a8" stroke-dasharray="4 3"/>')
    for module, (left, bottom, right, top) in boxes:
        role = next((key for key in COLORS if module.startswith(key)), 'cover')
        x, y = point(left, top)
        width, height = (right-left)*scale, (top-bottom)*scale
        parts.append(f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="2" fill="{COLORS[role]}"/>')
        if role != 'cover':
            label = 'HQ' if role == 'hq' else role.upper()
            parts.append(f'<text x="{x+width/2}" y="{y+height/2+3}" text-anchor="middle" class="label">{escape(label)}</text>')
    for post in posts:
        x, y = point(*post)
        parts.append(f'<circle cx="{x}" cy="{y}" r="3" fill="#ffffff" stroke="#263b40"/>')
    parts.append(f'<text x="{cx}" y="{cy+half_d*scale+21}" text-anchor="middle" class="note">↑ Main entrance</text>')

parts += ['<circle cx="32" cy="515" r="5" fill="#fff"/><text x="46" y="520" class="note">Guard post</text>',
          '<circle cx="185" cy="515" r="8" fill="none" stroke="#6fd2a8" stroke-dasharray="3 2"/><text x="201" y="520" class="note">Capture zone</text>',
          '<rect x="349" y="510" width="22" height="9" fill="#667a73"/><text x="380" y="520" class="note">Tall sandbag wall</text>',
          '<text x="26" y="548" class="note">Auto tries Full → Compact → Courtyard → Roadside → Command post → Rally post at each candidate. Assembly radius remains 150 m.</text>', '</svg>']
DEST.write_text('\n'.join(parts), encoding='utf-8')
print(DEST)
