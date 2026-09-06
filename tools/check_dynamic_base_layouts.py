"""Validate the authored small-base floor plans directly from the game manifest.

Checks footprints, non-overlapping module pads, gate/capture routes, and enough
clear guard posts for each layout's maximum garrison. No game runtime required.
"""
from pathlib import Path
import math
import re

SOURCE = Path(__file__).resolve().parents[1] / 'Scripts/Game/IA_DynamicSiteLayout.c'
TEXT = SOURCE.read_text(encoding='utf-8')
NUMBER = r'-?\d+(?:\.\d+)?'


def inside(point, box, clearance=0):
    x, z = point
    left, bottom, right, top = box
    return left - clearance < x < right + clearance and bottom - clearance < z < top + clearance


def read_layout(name):
    section = TEXT.split(f'static IA_DynamicSiteLayout Create{name}()', 1)[1].split('return layout;', 1)[0]
    if name in ('Full', 'Compact'):
        def scalar(field):
            return float(re.search(field + r' = (' + NUMBER + r');', section)[1])
        half_w, half_d = scalar('m_fHalfWidthM'), scalar('m_fHalfDepthM')
        radius = scalar('m_fCaptureRadiusM')
        cap_x, cap_y, cap_z = map(float, re.search(r'm_vCaptureLocal = Vector\(([^)]+)\)', section)[1].split(','))
        cross_z = -8 if name == 'Full' else -5
        max_guards = 36
    else:
        setup = re.search(r'CreateSmallLayout\(\w+, "[^"]+", (.*)\);', section)[1]
        values = [float(value) for value in re.findall(NUMBER, setup)]
        half_w, half_d, radius, cap_x, cap_y, cap_z, cross_z, max_guards = values
    boxes = []
    for line in section.splitlines():
        if 'layout.AddModule(' in line or 'layout.AddDressing(' in line:
            match = re.search(r'Add(?:Module|Dressing)\("([^"]+)", (PREFAB_\w+), (.*)\);', line)
            coords = match[3].split('IA_DynamicSiteModuleRole.')[0]
            x, z, yaw, w, d = [float(value) for value in re.findall(NUMBER, coords)][:5]
            angle = math.radians(yaw)
            w, d = abs(math.cos(angle)) * w + abs(math.sin(angle)) * d, abs(math.sin(angle)) * w + abs(math.cos(angle)) * d
            boxes.append((match[1], (x-w, z-d, x+w, z+d)))
        elif 'layout.AddCover(' in line:
            match = re.search(r'AddCover\("([^"]+)", (.*)\);', line)
            x, z, yaw, style, side = [float(value) for value in re.findall(NUMBER, match[2])]
            w, d = (1.8, 0.8) if yaw % 180 == 0 else (0.8, 1.8)
            boxes.append((match[1], (x-w, z-d, x+w, z+d)))
    posts = []
    for coords in re.findall(r'm_aGuardPosts.Insert\(Vector\(([^)]+)\)\)', section):
        x, y, z = map(float, coords.split(','))
        posts.append((x, z))
    return half_w, half_d, radius, (cap_x, cap_z), cross_z, int(max_guards), boxes, posts


def validate(name):
    half_w, half_d, radius, capture, cross_z, max_guards, boxes, posts = read_layout(name)
    section = TEXT.split(f'static IA_DynamicSiteLayout Create{name}()', 1)[1].split('return layout;', 1)[0]
    panels = re.findall(r'AddCover\("([^"]+)", (.*)\);', section)
    runs = {}
    styles = set()
    for key, args in panels:
        x, z, yaw, style, side = map(float, re.findall(NUMBER, args))
        styles.add(style)
        runs.setdefault(key.rsplit('_', 1)[0], []).append(x if yaw % 180 == 0 else z)
    assert styles == {0, 1, 2, 3}, (name, 'missing sandbag variety')
    assert len(runs) == 7, (name, 'missing perimeter run')
    covered = 0
    for run, points in runs.items():
        points.sort()
        assert all(b-a <= 3.001 for a,b in zip(points, points[1:])), (name, run, 'large wall gap')
        covered += points[-1] - points[0] + 2.96
    assert covered / (4 * (half_w + half_d)) >= 0.75, (name, 'less than 75 percent authored perimeter')
    for module, (left, bottom, right, top) in boxes:
        assert left >= -half_w - 1e-6 and right <= half_w + 1e-6, (name, module, 'outside width')
        assert bottom >= -half_d - 1e-6 and top <= half_d + 1e-6, (name, module, 'outside depth')
    for index, (first, a) in enumerate(boxes):
        for second, b in boxes[index+1:]:
            if first.startswith('wall_') and second.startswith('wall_'):
                continue
            overlap_x = min(a[2], b[2]) - max(a[0], b[0])
            overlap_z = min(a[3], b[3]) - max(a[1], b[1])
            assert overlap_x <= 1e-6 or overlap_z <= 1e-6, (name, first, second, 'overlapping pads')
    assert abs(capture[0]) + radius <= half_w and abs(capture[1]) + radius <= half_d, (name, 'capture radius')
    # Spawn groups are 2, 2, then groups of at most 4. Each uses its own post.
    required_posts = 2 + math.ceil(max(0, max_guards - 4) / 4)
    assert name in ('Full', 'Compact') or len(posts) >= required_posts, (name, 'too few guard posts')
    for post in posts:
        for module, box in boxes:
            assert not inside(post, box, 1.3), (name, post, module, 'guard scatter clips module')
    junction = (0, cross_z)
    routes = [((0, -half_d), junction), ((-half_w, cross_z), junction), ((half_w, cross_z), junction), (junction, capture)]
    for start, end in routes:
        distance = math.dist(start, end)
        steps = max(1, math.ceil(distance / 0.25))
        for step in range(steps + 1):
            point = tuple(start[i] + (end[i] - start[i]) * step / steps for i in range(2))
            for module, box in boxes:
                # Large layouts retain their existing interior route rules.
                if name in ('Full', 'Compact') and not module.startswith(('wall_', 'dressing_')):
                    continue
                assert not inside(point, box, 1.2), (name, module, 'blocked reserved route')
    print(f'PASS {name}: {half_w*2:g} x {half_d*2:g} m, {len(boxes)} modules, {max_guards} guards, three clear approaches')


if __name__ == '__main__':
    for layout in ['Full', 'Compact', 'Courtyard', 'Roadside', 'CommandPost', 'RallyPost']:
        validate(layout)
