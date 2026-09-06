"""Bounded, measured composition recipes. Rebuild after composition measurement.

Writes the runtime catalog, 120 checked layout recipes and a representative sheet.
No runtime packing search or unbounded rerolling is required.
"""
from pathlib import Path
import argparse
import json
import math
import random

from author_base_compositions import parse, REF, Node

ROOT=Path(__file__).resolve().parents[1]
THEMES=['Strongpoint','Encampment','RoadControl','Logistics','Camouflaged']
SIZES=[('Full',90,70,36),('Compact',70,58,36),('Courtyard',60,52,24),('Roadside',50,60,20),('CommandPost',46,44,16),('RallyPost',38,38,12)]
CAPS=[4,3,2,2,1,1]


def vec(v):
    return '"'+' '.join(f'{x:.6g}' for x in v)+'"'


def box(item):
    w,d=item['half_width'],item['half_depth']
    if item['yaw']%180:
        w,d=d,w
    x,z=item['position'][0],item['position'][2]
    return x-w,z-d,x+w,z+d


def overlap(a,b,gap=0):
    return not(a[2]+gap<=b[0] or b[2]+gap<=a[0] or a[3]+gap<=b[1] or b[3]+gap<=a[1])


WALL_INSET=0.9
GUNNED_COVER_WEIGHT=1.5
SANDBAG_FACE=0.62
APRON_ALLOW=4.0
_FACE_CACHE={}


def cover_weight(key,catalog):
    if catalog.get(key,{}).get('sockets'):
        return GUNNED_COVER_WEIGHT
    return 1.0


def weighted_cover_order(rng,keys,catalog):
    """Gunned fighting positions are 1.5x as likely as bare cover."""
    remaining=list(keys)
    ordered=[]
    while remaining:
        weights=[cover_weight(key,catalog) for key in remaining]
        pick=rng.choices(remaining,weights=weights,k=1)[0]
        remaining.remove(pick)
        ordered.append(pick)
    return ordered


def mesh_box(item,measure):
    """World-axis AABB of the measured mesh, not the reserved clearance pad."""
    m=measure[item['key']]
    angle=math.radians(item['yaw'])
    cx,cz=item['position'][0],item['position'][2]
    xs=[]; zs=[]
    for lx,lz in ((m['mins'][0],m['mins'][2]),(m['mins'][0],m['maxs'][2]),
                  (m['maxs'][0],m['mins'][2]),(m['maxs'][0],m['maxs'][2])):
        xs.append(cx+lx*math.cos(angle)+lz*math.sin(angle))
        zs.append(cz-lx*math.sin(angle)+lz*math.cos(angle))
    return min(xs),min(zs),max(xs),max(zs)


def wall_line(W,D,side):
    if side in (0,2):
        return (D-WALL_INSET)*(1 if side==0 else -1)
    return (W-WALL_INSET)*(1 if side==1 else -1)


def resource_file(resource_name):
    if '}' in resource_name:
        return ROOT/resource_name.split('}',1)[1]
    return ROOT/resource_name


def _prop_floats(node,name,fallback):
    raw=node.prop(name,' '.join(str(v) for v in fallback)).split()
    return [float(raw[i]) if i<len(raw) else fallback[i] for i in range(3)]


def _wall_aligned(yaw):
    wrapped=((yaw+180.0)%360.0)-180.0
    return abs(wrapped)<25.0


def _collect_sandbag_faces(resource_name,offset_z=0.0,offset_yaw=0.0):
    path=resource_file(resource_name)
    if not path.is_file():
        return []
    root=parse(path.read_text(encoding='utf-8-sig'))
    block=root.block('')
    if not block:
        return []
    faces=[]
    for child in block.body:
        if not isinstance(child,Node):
            continue
        match=REF.search(child.head)
        if not match:
            continue
        target=match[1]
        coords=_prop_floats(child,'coords',(0,0,0))
        angles=_prop_floats(child,'angles',(0,0,0))
        child_z=offset_z+coords[2]
        child_yaw=offset_yaw+angles[1]
        if 'Sandbags/' in target:
            if _wall_aligned(child_yaw):
                faces.append(child_z+SANDBAG_FACE)
            continue
        if target.startswith('Prefabs/BaseCompositions/'):
            faces.extend(_collect_sandbag_faces(target,child_z,child_yaw))
    return faces


def fighting_face_z(key,catalog,measure):
    """Outward sandbag parapet, not dirt berms / wire / slot padding."""
    if key in _FACE_CACHE:
        return _FACE_CACHE[key]
    faces=_collect_sandbag_faces(catalog[key]['prefab'])
    if faces:
        out=max(faces)
    else:
        out=measure[key]['maxs'][2]
    _FACE_CACHE[key]=out
    return out


def snap_outward_face(x,z,side,key,catalog,measure,W,D):
    """Put the fighting sandbag face on the same line as the perimeter walls."""
    out=fighting_face_z(key,catalog,measure)
    fixed=wall_line(W,D,side)
    if side==0:
        return x,fixed-out
    if side==1:
        return fixed-out,z
    if side==2:
        return x,fixed+out
    return fixed+out,z


def perimeter_walls(W,D,modules,catalog,measure=None):
    """Tile the unused perimeter, subtracting gates, assemblies and firing lanes.

    Fighting-position gaps use the measured mesh so walls meet the bags instead
    of the oversized reservation pad. Firing corridors stay padded.
    """
    walls=[]
    for side in range(4):
        horizontal=side in (0,2)
        extent=(W if horizontal else D)-WALL_INSET
        fixed=wall_line(W,D,side)
        blocked=[]
        if side==2:
            blocked.append((-5,5))
        elif not horizontal:
            blocked.append((-13,-3))
        for m in modules:
            if m['side']==side:
                a=mesh_box(m,measure) if measure else box(m)
                blocked.append((a[0],a[2]) if horizontal else (a[1],a[3]))
            for socket in catalog[m['key']]['sockets']:
                x,z=m['position'][0],m['position'][2]
                yaw=m['yaw']
                for pose in socket['chain']:
                    angle=math.radians(yaw)
                    px,_,pz=pose['position']
                    x+=px*math.cos(angle)+pz*math.sin(angle)
                    z+=-px*math.sin(angle)+pz*math.cos(angle)
                    yaw+=pose['angles'][1]
                hits=[]
                for offset in (-30,-15,0,15,30):
                    angle=math.radians(yaw+offset)
                    dx,dz=math.sin(angle),math.cos(angle)
                    component=dz if horizontal else dx
                    if abs(component)<1e-6:
                        continue
                    distance=(fixed-(z if horizontal else x))/component
                    if 0<distance<=55:
                        hits.append((x+dx*distance) if horizontal else (z+dz*distance))
                if hits:
                    blocked.append((min(hits)-2,max(hits)+2))
        spans=[(-extent,extent)]
        for lo,hi in sorted(blocked):
            remaining=[]
            for a,b in spans:
                if hi<=a or lo>=b:
                    remaining.append((a,b))
                else:
                    if a<lo:remaining.append((a,lo))
                    if hi<b:remaining.append((hi,b))
            spans=remaining
        for a,b in spans:
            # Stock mesh width is 2.966 m. Keep small overlaps, never scale it.
            if b-a<2.966:
                continue
            count=max(1,math.ceil((b-a-2.966)/2.9)+1)
            step=(b-a-2.966)/(count-1) if count>1 else 0
            for i in range(count):
                along=a+1.483+i*step
                position=[along,0,fixed] if horizontal else [fixed,0,along]
                # Solid / firing slit / high parapet, with an end-cap next to
                # gates and fighting positions. Same family as vanilla positions.
                style=[0,1,0,2][i%4]
                if i==0 or i==count-1:
                    style=3
                walls.append({'position':[round(v,4) for v in position],'yaw':[0,90,180,270][side],'side':side,'style':style})
    return walls


def catalog_script(catalog,measure):
    lines=['// Generated by tools/author_base_designs.py. Mesh bounds from native Preview.','class IA_BaseCompositionCatalog','{','\tstatic IA_BaseCompositionAsset Get(string key)','\t{','\t\tref IA_BaseCompositionAsset asset;','\t\tIA_BaseGunSocket socket;','\t\tswitch (key)','\t\t{']
    for key,entry in catalog.items():
        m=measure[key]
        cost=m['count']+max(2,math.ceil(m['count']*.05))
        lines += [f'\t\t\tcase "{key}":',f'\t\t\t\tasset = IA_BaseCompositionAsset.Create(key, "{entry["prefab"]}", {vec(m["mins"])}, {vec(m["maxs"])}, {cost});']
        for socket in entry['sockets']:
            lines += [f'\t\t\t\tsocket = asset.AddSocket({socket["kind"]});']
            for pose in socket['chain']:
                lines += [f'\t\t\t\tsocket.Append({vec(pose["position"])}, {vec(pose["angles"])});']
        lines += ['\t\t\t\tbreak;']
    return '\n'.join(lines+['\t\t}','\t\treturn asset;','\t}','}',''])


def build(size_id,variant,catalog,measure):
    name,W,D,garrison=SIZES[size_id]
    theme=variant//4
    rng=random.Random(13007+size_id*1999+variant*337)
    modules=[]
    guns=0
    heavy=0
    budget=0
    # Reserve a complete plain-wall ring before selecting costly clusters.
    # Release unused reservation once the actual perimeter openings are known.
    wall_reserve=math.ceil(4*(W+D)/2.9)+4
    mirror=1 if variant%2 else -1
    capture=[0,0,D-32]
    lanes=[(-4,-D,4,capture[2]),(-W,-12,W,-4)]
    def insert(key,x,z,yaw,role,side=-1,required=False,ignore_lane=False):
        nonlocal budget,guns,heavy
        m=measure[key]
        cost=m['count']+max(2,math.ceil(m['count']*.05))
        socks=catalog[key]['sockets']
        h=sum(s['kind']>0 for s in socks)
        if guns+len(socks)>CAPS[size_id] or heavy+h>int(size_id<2):
            return False
        if budget+cost+(guns+len(socks))*12+wall_reserve>820:
            return False
        item={'key':key,'position':[round(x,3),0,round(z,3)],'yaw':yaw,'role':role,'side':side,'required':required,
              'half_width':max(abs(m['mins'][0]),abs(m['maxs'][0]))+1,
              'half_depth':max(abs(m['mins'][2]),abs(m['maxs'][2]))+1,'expanded':cost}
        a=box(item)
        if side>=0:
            mb=mesh_box(item,measure)
            if mb[0]<-W-APRON_ALLOW or mb[2]>W+APRON_ALLOW or mb[1]<-D-APRON_ALLOW or mb[3]>D+APRON_ALLOW:
                return False
        elif a[0]<-W+1 or a[2]>W-1 or a[1]<-D+1 or a[3]>D-1:
            return False
        if any(overlap(a,box(other),1) for other in modules):
            return False
        if not ignore_lane and any(overlap(a,lane,1) for lane in lanes):
            return False
        modules.append(item)
        budget+=cost
        guns+=len(socks)
        heavy+=h
        return True
    # Headquarters terminates the central approach, not the crossing lane.
    assert insert('Headquarters',0,D-20,180,'Hq',required=True,ignore_lane=True),(name,variant,'hq')
    hq_box=box(modules[0])
    capture=[0,0,hq_box[1]-3]
    lanes[0]=(-4,-D,4,capture[2])
    # Every recipe has accommodation. Large quarters are a deliberately costly
    # alternative to several separate service areas, not additional decoration.
    living='LivingLarge' if size_id==0 and theme==1 else 'LivingSmall'
    candidates=[(mirror*W*.48,D*.13),(mirror*W*.48,D*.34),(mirror*W*.48,D*.27),(mirror*W*.48,-D*.5),(mirror*W*.6,D*.27)]
    placed=False
    for x,z in candidates:
        if insert(living,x,z,0,'Barracks',required=True):
            placed=True
            break
    if not placed:
        for x,z in candidates:
            if insert('LivingSmall',x,z,0,'Barracks',required=True):
                placed=True
                break
    assert placed,(name,variant,'living')
    # Secure each side before allocating optional interior luxuries.
    palettes=[['Bunker','Tower','PKMNest','Position2','Position3','Position1','PKM'],
              ['PKM','Position1','Position3','Position4','Tower','Position2'],
              ['CheckpointM','CheckpointS','PKMNest','BarricadeM','BarricadeL','Position2','Position3','Tower','Position1'],
              ['Tower','PKM','Position2','BarricadeS','Position1','Position3'],
              ['Bunker','Position4','PKMNest','Position3','Tower','Position2','Position1']]
    gunned=[[key for key in pal if catalog[key]['sockets']] for pal in palettes]
    bare=[[key for key in pal if not catalog[key]['sockets']] for pal in palettes]
    slots=[(2,-.48),(2,.48),(1,.55),(3,.55),(0,-.52),(0,.52),(1,-.63),(3,-.63)]
    if size_id<2:
        slots.insert(4,(0,0))
    used={}
    for index,(side,fraction) in enumerate(slots):
        # Socketed guns first so a wall of sandbag positions is the fallback,
        # not the default. Weight still prefers gunned pieces 1.5x among peers.
        options=weighted_cover_order(rng,gunned[theme],catalog)+weighted_cover_order(rng,bare[theme],catalog)
        if index==0:
            options=['CheckpointM' if theme==2 and size_id<4 else 'PKMNest','PKM']+options
        if index==2 and size_id<2:
            options=[['NSV','AA','ScopedNest','CheckpointL','NSVNest'][(theme+variant%4)%5]]+options
        # Preserve at least one of each side: small bare positions are fallbacks.
        options += ['PKM','Position1','Position3','BarricadeS','CheckpointS']
        for key in dict.fromkeys(options):
            if used.get(key,0)>=2:
                continue
            m=measure[key]
            w=max(abs(m['mins'][0]),abs(m['maxs'][0]))+1
            d=max(abs(m['mins'][2]),abs(m['maxs'][2]))+1
            yaw=[0,90,180,270][side]
            if side in (0,2):
                x=W*fraction
                if side==2 and key.startswith('Checkpoint'):
                    x=(w+5)*(1 if fraction>0 else -1)
                z=0
            else:
                x=0
                z=D*fraction
            x,z=snap_outward_face(x,z,side,key,catalog,measure,W,D)
            if insert(key,x,z,yaw,'Cover',side):
                used[key]=used.get(key,0)+1
                break
    sides={x['side'] for x in modules}
    assert all(side in sides for side in range(4)),(name,variant,'side missing',sides)
    walls=perimeter_walls(W,D,modules,catalog,measure)
    wall_reserve=len(walls)
    service_lists=[['Hospital','Ammo','Medical','Fuel'],['Fuel','Medical','Supply'],
                   ['MaintenanceSmall','Supply','Medical'],['MaintenanceLarge','Supply','Fuel','Ammo'],
                   ['Medical','Ammo','Supply']]
    services=service_lists[theme][:]
    if size_id>1:
        services=[{'Hospital':'Medical','MaintenanceLarge':'MaintenanceSmall'}.get(k,k) for k in services]
    if variant%4>=2:
        services=services[1:]+services[:1]
    service_spots=[(-mirror*W*.45,D*.15),(-mirror*W*.43,-D*.5),(mirror*W*.44,-D*.5),(-mirror*W*.4,D*.48)]
    max_services=[3,3,2,2,1,1][size_id]
    placed_services=0
    for key in dict.fromkeys(services):
        for x,z in service_spots:
            yaw=0 if variant%4<2 else 180
            if insert(key,x,z,yaw,'Supply',required=True):
                placed_services+=1
                break
        if placed_services==max_services:
            break
    assert budget+guns*12+len(walls)<=820,(name,variant,'wall budget')
    assert len(modules)+len(walls)+guns<=256,(name,variant,'wall roots')
    # Outdoor posts are selected from clear grid points, never inside a tent,
    # bunker or ladder. Native infantry cover behavior remains independent.
    posts=[]
    points=[capture,[0,0,-D+12],[-W+12,0,-8],[W-12,0,-8]]
    points += [[x,0,z] for z in range(-int(D)+12,int(D)-10,5) for x in range(-int(W)+12,int(W)-10,5)]
    tail=points[4:]
    rng.shuffle(tail)
    points=points[:4]+tail
    for p in points:
        a=(p[0]-1.5,p[2]-1.5,p[0]+1.5,p[2]+1.5)
        if any(overlap(a,box(m),1) for m in modules):
            continue
        if any(math.dist((p[0],p[2]),(q[0],q[2]))<6 for q in posts):
            continue
        posts.append(p)
        if len(posts)>=garrison:
            break
    assert len(posts)>=20 if size_id<2 else len(posts)>=12,(name,variant,'posts',len(posts))
    assert not any(overlap((capture[0]-.5,capture[2]-.5,capture[0]+.5,capture[2]+.5),box(m)) for m in modules)
    return {'name':name+' / '+THEMES[theme]+f' {variant%4+1}','size':size_id,'variant':variant,'theme':THEMES[theme],
            'half_width':W,'half_depth':D,'garrison':garrison,'capture':capture,'modules':modules,'posts':posts,
            'expanded':budget+len(walls),'guns':guns,'heavy':heavy,'walls':walls}


def recipe_script(recipes):
    lines=['// Generated by tools/author_base_designs.py. No runtime packing search.','class IA_BaseDesignRecipes','{',
           '\tstatic IA_DynamicSiteLayout Create(int size, int variant)','\t{','\t\tref IA_ComposedSiteLayout layout = new IA_ComposedSiteLayout();',
           '\t\tint recipe = size * 20 + variant;','\t\tswitch (recipe)','\t\t{']
    for i,r in enumerate(recipes):
        lines += [f'\t\t\tcase {i}:',f'\t\t\t\tBuild{i}(layout);','\t\t\t\tbreak;']
    lines += ['\t\t\tdefault:','\t\t\t\treturn null;','\t\t}','\t\treturn layout;','\t}']
    for i,r in enumerate(recipes):
        lines += [f'\tprotected static void Build{i}(IA_ComposedSiteLayout layout)','\t{',
                  f'\t\tlayout.Initialize({r["size"]}, {r["variant"]}, "{r["name"]}", {r["half_width"]}, {r["half_depth"]}, {r["garrison"]});',
                  f'\t\tlayout.m_vCaptureLocal = {vec(r["capture"])};']
        for m in r['modules']:
            required='true' if m['required'] else 'false'
            lines += [f'\t\tlayout.AddComposition("{m["key"]}", {vec(m["position"])}, {m["yaw"]}, IA_DynamicSiteModuleRole.{m["role"]}, {m["side"]}, {required});']
        lines += [f'\t\tBuildWalls{i}(layout);']
        for p in r['posts']:
            lines += [f'\t\tlayout.m_aGuardPosts.Insert({vec(p)});']
        lines += ['\t}']
        lines += [f'\tprotected static void BuildWalls{i}(IA_ComposedSiteLayout layout)','\t{']
        for wall in r['walls']:
            lines += [f'\t\tlayout.AddPerimeterWall({vec(wall["position"])}, {wall["yaw"]}, {wall["side"]}, {wall.get("style",0)});']
        lines += ['\t}']
    return '\n'.join(lines+['}',''])


def sheet(recipes):
    rows=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="1150" viewBox="0 0 1200 1150">',
          '<rect width="1200" height="1150" fill="#101923"/>','<g font-family="sans-serif" fill="#dce8ef">',
          '<text x="20" y="28" font-size="19">Composition base recipes - representative Full layouts (not live screenshots)</text>']
    for i,r in enumerate([recipes[n] for n in (0,4,8,12,16,5)]):
        cx=300+(i%2)*600; cy=210+(i//2)*355; scale=2.3
        rows += [f'<text x="{cx-265}" y="{cy-160}" font-size="16">{r["name"]}</text>',
                 f'<rect x="{cx-r["half_width"]*scale}" y="{cy-r["half_depth"]*scale}" width="{r["half_width"]*2*scale}" height="{r["half_depth"]*2*scale}" fill="none" stroke="#516976"/>']
        for m in r['modules']:
            a,b,c,d=box(m)
            color='#718349' if m['side']>=0 else '#326f88'
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-b)*scale:.2f}" fill="{color}"/>',
                     f'<text x="{cx+(a+c)*scale/2:.2f}" y="{cy-(b+d)*scale/2:.2f}" text-anchor="middle" font-size="9">{m["key"]}</text>']
        for wall in r['walls']:
            x,z=wall['position'][0],wall['position'][2]
            dx,dz=(1.483,0) if wall['side'] in (0,2) else (0,1.483)
            rows += [f'<path d="M {cx+(x-dx)*scale:.2f} {cy-(z-dz)*scale:.2f} L {cx+(x+dx)*scale:.2f} {cy-(z+dz)*scale:.2f}" stroke="#c5b488" stroke-width="3"/>']
        for p in r['posts']:
            rows += [f'<circle cx="{cx+p[0]*scale:.2f}" cy="{cy-p[2]*scale:.2f}" r="2" fill="#ffcf73"/>']
    return '\n'.join(rows+['</g></svg>',''])


def generate(catalog_only=False):
    _FACE_CACHE.clear()
    catalog=json.loads((ROOT/'docs/base-composition-catalog.json').read_text())
    measure=json.loads((ROOT/'docs/base-composition-measurements.json').read_text())['assets']
    outputs={'Scripts/Game/IA_BaseCompositionCatalog.c':catalog_script(catalog,measure)}
    if not catalog_only:
        recipes=[build(size,v,catalog,measure) for size in range(6) for v in range(20)]
        outputs.update({'Scripts/Game/IA_BaseDesignRecipes.c':recipe_script(recipes),
                        'docs/base-composition-designs.json':json.dumps(recipes,indent=2)+'\n',
                        'docs/base-composition-designs.svg':sheet(recipes)})
    return outputs


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--catalog-only',action='store_true')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    outputs=generate(args.catalog_only)
    for path,content in outputs.items():
        p=ROOT/path
        if args.check:
            assert p.read_text(encoding='utf-8')==content,path
        else:
            p.write_text(content,encoding='utf-8')
    print(f'{len(outputs)} design outputs')


if __name__=='__main__':
    main()
