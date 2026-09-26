"""Permanent-headquarters scenery and recipes (second dynamic-base style).

Three stages, each reproducible and checkable with --check:

  assets DATA     author IA HQ prefabs from extracted vanilla data (data007)
  measure LOG OUT parse a Workbench [IA][CompositionMeasure] log into JSON
  designs         write the runtime catalog, 72 recipes, JSON and sheet

Stock buildings and concrete walls are never spawned directly. Buildings are
SCR_DestructibleBuildingEntity (persistence, subscene, doors) and walls are
SCR_DestructibleEntity without an RplComponent, so both are wrapped as
mesh-only IA prefabs: the vanilla MeshObject/RigidBody, nothing scripted.
"""
from pathlib import Path
import argparse
import copy
import json
import math
import random
import re
import sys

sys.dont_write_bytecode=True
from author_base_compositions import Author, Node, REF, guid, resource, metadata
from author_base_designs import SIZES, CAPS, RECIPE_EXPANDED_BUDGET, box, overlap, mesh_box, vec

ROOT=Path(__file__).resolve().parents[1]
OUT='Prefabs/BaseCompositions/Headquarters/'
PANEL=2.698
ROOT_LIMIT=240
HQ_VARIANTS=12
ARCHETYPES=['Command','Garrison','Depot','Border']
WALL_IN=5.0
LANE_HALF=4.0
MESH_KEYS=('MeshObject','RigidBody')

STOCK={
    'panelV1':'{27EB771E3DD9C354}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V1.et',
    'panelV2':'{4BBF43FE811D4C7A}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V2.et',
    'panelV3':'{AE230F078DF8DB11}Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_V3.et',
    'panelDamaged':'Prefabs/Structures/Walls/Concrete/ConcreteWall_USSR_01/ConcreteWall_USSR_01_damaged_V1.et',
    'parapet':'{879865F07EA907B0}Prefabs/Structures/Walls/Concrete/ConcreteWall_02/ConcreteWall_02_4m.et',
    'cheek':'Prefabs/Structures/Walls/Concrete/ConcreteWall_01/ConcreteWall_01_camo_3m.et',
    'slab':'Prefabs/Structures/Walls/Concrete/ConcreteWall_01/ConcreteWall_01_camo_6m_A.et',
    'tower':'{52D3F2118C1B68E0}Prefabs/Structures/Military/Houses/GuardTower_USSR_01/GuardTower_USSR_01_green.et',
    'command':'{3F91DEEC9C78E473}Prefabs/Structures/Military/Houses/GuardHouse_01/GuardHouse_01.et',
    'pillbox':'{6214C73708EA0E2D}Prefabs/Structures/Military/Fortifications/Bunker_SPS/Bunker_SPS.et',
    'shelter':'{4BE4C27399CF3B00}Prefabs/Structures/Military/Bunkers/ShelterMilitary_E_01/ShelterMilitary_E_01.et',
    'guardbox':'{F50905235FBAA094}Prefabs/Structures/Military/Houses/GuardBox_01/GuardBox_01_beige.et',
    'barracks':'{2CB4D91249389DFD}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_USSR_01_military_base.et',
    'teeth1':'{C16C62041E2AAAB2}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V1.et',
    'teeth2':'{8B7B5323534404A7}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V2.et',
    'hedgehog':'{A46D45922F8CAD6C}Prefabs/Props/Military/Fortification/CzechHedgehog_01_painted.et',
    'wire':'{93E06E731212BD96}Prefabs/Structures/Walls/BarbedWire/BarbedTape_01/BarbedTape_01_Triple.et',
    'pkm':'{723870DBB19D30B0}Prefabs/Weapons/Tripods/Tripod_6T5_PKM.et',
    'nsv':'{29F0CC704A582154}Prefabs/Weapons/Tripods/6T7/Tripod_6T7_NSV.et',
}
# Vanilla sub-composition tripod poses sit on a 0.656 m sandbag stack. The
# concrete parapet top is placed at exactly that height.
TRIPOD={'pkm':(0.004,0.656,-0.224),'nsv':(0.016,0.656,-0.147)}
PARAPET_HALF_HEIGHT=1.85
# Outward door faces from the stock mesh sockets (Workbench bone dump,
# logs_2026-09-25_23-20-44): (local outward normal x,z, offset along the face,
# required clear depth). The first entry is the main door and must face the yard.
DOORS={
    'HQCommand':[((0,1),-2.12,3.0),((0,-1),2.18,2.0),((-1,0),3.78,2.0),((1,0),3.78,2.0)],
    'HQBarracks':[((0,-1),0.0,3.0),((0,1),0.0,3.0)],
    'HQShelter':[((0,1),0.24,3.0)],
    'HQPillbox':[((0,1),-0.56,3.0)],
    'HQTower':[((0,1),0.0,3.0)],
}
APRON_HALF=1.5


# Emitted into IA_HeadquartersProbe: vanilla CoverPost/ObservationPost smart
# actions must survive the mesh-only wrapper (IA area garrisons query them).
PROBE_POSTS='''
	protected void CheckPosts(BaseWorld world, string key, ResourceName name, int expected, int addons, int ladders)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(name), world, params);
		Check(entity != null, "post resource " + key);
		if (!entity)
			return;
		array<Managed> found = {};
		entity.FindComponents(SCR_AISmartActionSentinelComponent, found);
		Check(found.Count() == expected, "sentinel post count " + key);
		foreach (Managed item : found)
		{
			SCR_AISmartActionSentinelComponent post = SCR_AISmartActionSentinelComponent.Cast(item);
			array<string> tags = {};
			post.GetTags(tags);
			Check(tags.Find("CoverPost") >= 0 || tags.Find("ObservationPost") >= 0, "sentinel post tag " + key);
			Print(string.Format("[IA][HeadquartersProbe] post key=%1 tags=%2 offset=%3 accessible=%4", key, tags, post.GetActionOffset(), post.IsActionAccessible()), LogLevel.NORMAL);
		}
		// Stairs, entry steps, door frames and the pillbox ladder spawn as children.
		int children = 0;
		int climbable = 0;
		IEntity child = entity.GetChildren();
		while (child)
		{
			children++;
			if (child.FindComponent(LadderComponent))
				climbable++;
			child = child.GetSibling();
		}
		Check(children == addons, "wrapper add-ons " + key);
		Check(climbable == ladders, "wrapper ladders " + key);
		Print(string.Format("[IA][HeadquartersProbe] addons key=%1 children=%2 ladders=%3", key, children, climbable), LogLevel.NORMAL);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
'''


def stock_path(name):
    ref=STOCK[name]
    return ref.split('}',1)[1] if '}' in ref else ref


def fmt(values):
    return ' '.join(f'{v:.6g}' for v in values)


class HeadquartersAuthor(Author):
    """Adds mesh-only wrappers and hand-built concrete compositions."""

    def __init__(self,base):
        super().__init__(base)
        self.catalog={}

    def stock_resource(self,name):
        path=stock_path(name)
        # Some variants are not referenced by an already-known GUID; their
        # .meta carries it in the extracted data.
        meta=self.base/(path+'.meta')
        if meta.exists():
            match=re.search(r'Name "\{([0-9A-F]{16})\}',meta.read_text(encoding='utf-8-sig'))
            if match:
                return '{'+match[1]+'}'+path
        return STOCK[name]

    def has_rpl(self,path):
        for node in self.chain(path):
            components=node.block('components')
            if components and any(isinstance(c,Node) and c.head.startswith('RplComponent ') for c in components.body):
                return True
        return False

    def has_component(self,path,kind):
        for node in self.chain(path):
            components=node.block('components')
            if components and any(isinstance(c,Node) and c.head.split(' ',1)[0]==kind for c in components.body):
                return True
        return False

    def mesh_components(self,path):
        merged={}
        for ancestor in reversed(self.chain(path)):
            components=ancestor.block('components')
            if not components:
                continue
            for c in components.body:
                if not isinstance(c,Node):
                    continue
                kind=c.head.split(' ',1)[0]
                if kind not in MESH_KEYS:
                    continue
                merged[kind]=self.merge(merged[kind],c) if kind in merged else copy.deepcopy(c)
        assert set(merged)==set(MESH_KEYS),(path,sorted(merged))
        return [merged[k] for k in MESH_KEYS]

    def smart_actions(self,path):
        """Vanilla cover/observation posts, merged by component id down the chain.

        Stock GenericEntity prefabs (camo-net observation posts) carry the same
        component, so it does not need the destructible building class.
        """
        merged={}
        for ancestor in reversed(self.chain(path)):
            components=ancestor.block('components')
            if not components:
                continue
            for c in components.body:
                if not isinstance(c,Node) or not c.head.startswith('SCR_AISmartActionSentinelComponent '):
                    continue
                merged[c.head]=self.merge(merged[c.head],c) if c.head in merged else copy.deepcopy(c)
        return list(merged.values())

    def write(self,out,root):
        self.outputs[out]=root.render()
        self.outputs[out+'.meta']=metadata(out)
        return resource(out)

    def static_mesh(self,name):
        """Non-replicated StaticModelEntity, the same shape as vanilla DirtCover."""
        return self.static_mesh_path(stock_path(name))

    def static_mesh_path(self,path):
        out=OUT+'Mesh/IA_HQ_Mesh_'+Path(path).stem+'.et'
        if out in self.outputs:
            return resource(out)
        root=Node('StaticModelEntity',['ID "'+guid(out)+'"',Node('components',self.mesh_components(path)),'coords 0 0 0'])
        return self.write(out,root)

    def root(self,out,extra=None):
        components=[Node('RplComponent "{'+guid(out+'/rpl')+'}"'),Node('Hierarchy "{'+guid(out+'/hierarchy')+'}"',['Enabled 1'])]
        return Node('GenericEntity',['ID "'+guid(out)+'"',Node('components',components+(extra or [])),'coords 0 0 0'])

    def child(self,out,index,ref,coords,angles=(0,0,0),cls=None):
        path=ref.split('}',1)[1]
        if cls is None:
            head=self.load(path).head if not path.startswith(OUT) else 'StaticModelEntity'
            cls=head.split(' : ',1)[0]
        hierarchy='{'+guid(path+'/Hierarchy')+'}' if path.startswith(OUT) else self.component_id(path,'Hierarchy')
        body=['ID "'+guid(out+'/child/'+str(index))+'"','coords '+fmt(coords)]
        if any(angles):
            body.append('angles '+fmt(angles))
        body.append(Node('components',[Node('Hierarchy "'+hierarchy+'"',['Enabled 1'])]))
        return Node(cls+' : "'+ref+'"',body)

    def composition(self,key,name,children,sockets=()):
        out=OUT+'IA_HQ_'+name+'.et'
        root=self.root(out)
        root.body.append(Node('',children(out)))
        self.catalog[key]={'prefab':self.write(out,root),'sockets':list(sockets),'source':'authored'}

    def wrapper(self,key,name,stock):
        """Mesh-only building: no doors, windows, persistence or destruction."""
        path=stock_path(stock)
        out=OUT+'IA_HQ_'+name+'.et'
        posts=self.smart_actions(path)
        root=self.root(out,self.mesh_components(path)+posts)
        kept=[]
        ladders=0
        frames=0
        for child in self.children(path):
            match=REF.search(child.head)
            if not match or self.excluded(match[1]):
                continue
            if self.has_component(match[1],'DoorSlotComponent'):
                # Door frame trim only; the replicated door leaf is dropped so
                # the opening stays walkable and never needs door AI.
                kept.append(self.pivot_mesh(out,len(kept),self.static_mesh_path(match[1]),child))
                frames+=1
                continue
            ladder=self.has_component(match[1],'LadderComponent')
            if not ladder and (self.has_rpl(match[1]) or child.head.split(' : ',1)[0]!='StaticModelEntity'):
                continue
            node=Node(child.head,[x for x in child.body if isinstance(x,str) and x.split(' ',1)[0] in ('ID','coords','angles','scale')])
            components=child.block('components')
            hierarchy=next((c for c in components.body if isinstance(c,Node) and c.head.startswith('Hierarchy ')),None) if components else None
            assert hierarchy and hierarchy.prop('PivotID'),(path,child.head)
            kept_components=[copy.deepcopy(hierarchy)]
            if ladder:
                # Plain replicated ladder (no destruction): the only way up to
                # the observation slits. Parented to the wrapper's RPL node like
                # the casemate tripods.
                ladders+=1
                kept_components.insert(0,Node('RplComponent "'+self.component_id(match[1],'RplComponent')+'"',['"Parent Node From Parent Entity" 1']))
            node.body.append(Node('components',kept_components))
            kept.append(node)
        if kept:
            root.body.append(Node('',kept))
        self.catalog[key]={'prefab':self.write(out,root),'sockets':[],'source':STOCK[stock],'addons':len(kept),'posts':len(posts),'ladders':ladders,'frames':frames}

    def pivot_mesh(self,out,index,mesh,child):
        """IA mesh prefab on the stock child's building socket."""
        components=child.block('components')
        hierarchy=next((c for c in components.body if isinstance(c,Node) and c.head.startswith('Hierarchy ')),None) if components else None
        assert hierarchy and hierarchy.prop('PivotID'),(out,child.head)
        body=['ID "'+guid(out+'/frame/'+str(index))+'"']
        body+=[x for x in child.body if isinstance(x,str) and x.split(' ',1)[0] in ('coords','angles','scale')]
        path=mesh.split('}',1)[1]
        body.append(Node('components',[Node('Hierarchy "{'+guid(path+'/Hierarchy')+'}"',['Enabled 1','PivotID '+hierarchy.prop('PivotID'),'AutoTransform 1'])]))
        return Node('StaticModelEntity : "'+mesh+'"',body)

    def wall_run(self,key,name,panels):
        """Panels start at their own pillar and span +x; the run is centred."""
        meshes=[self.static_mesh(p) for p in panels]
        start=-PANEL*len(panels)/2
        def children(out):
            return [self.child(out,i,m,(start+i*PANEL,0,0)) for i,m in enumerate(meshes)]
        self.composition(key,name,children)

    def casemate(self,key,name,gun):
        """Concrete gun position: parapet at tripod height, camo cheeks and roof.

        Local +Z faces the enemy. The crew enters from the open -Z side.
        """
        parapet=self.static_mesh('parapet')
        cheek=self.static_mesh('cheek')
        slab=self.static_mesh('slab')
        pose=TRIPOD[gun]
        tripod=self.stock_resource(gun)
        tripod_path=stock_path(gun)
        def children(out):
            nodes=[self.child(out,0,parapet,(0,pose[1]-PARAPET_HALF_HEIGHT,0)),
                   # 3 m cheeks, yawed 90 so local +x runs to -Z (behind the parapet).
                   self.child(out,1,cheek,(-2.45,0,0.4),(0,90,0)),
                   self.child(out,2,cheek,(2.45,0,0.4),(0,90,0)),
                   # 6 m panel laid flat on the cheeks. Measured: pitch 90 maps local
                   # +y to -Z, so the slab (y -0.8..2.96) spans z 0.55..-3.2 and its
                   # 0.126 m half-thickness rests on the 2.964 m cheek tops.
                   self.child(out,3,slab,(-3,3.09,-0.25),(90,0,0))]
            gun_node=Node(self.load(tripod_path).head.split(' : ',1)[0]+' : "'+tripod+'"',[
                'ID "'+guid(out+'/tripod')+'"','coords '+fmt(pose),
                Node('components',[Node('IA_StaticGunComponent "{'+guid(out+'/gun')+'}"'),
                                   Node('RplComponent "'+self.component_id(tripod_path,'RplComponent')+'"',['"Parent Node From Parent Entity" 1']),
                                   Node('Hierarchy "'+self.component_id(tripod_path,'Hierarchy')+'"',['Enabled 1'])])])
            return nodes+[gun_node]
        kind=0 if gun=='pkm' else 1
        self.composition(key,name,children,[{'kind':kind,'chain':[{'position':list(pose),'angles':[0,0,0]}]}])

    def obstacles(self,key,name,items):
        def children(out):
            return [self.child(out,i,self.stock_resource(s),c,a) for i,(s,c,a) in enumerate(items)]
        self.composition(key,name,children)

    def generate(self):
        self.wall_run('HQWallA','WallRun_A',['panelV1','panelV2'])
        self.wall_run('HQWallB','WallRun_B',['panelV3','panelV1'])
        self.wall_run('HQWallC','WallRun_C',['panelV2','panelDamaged'])
        self.wall_run('HQWallPanel','WallPanel',['panelV3'])
        self.casemate('HQCasematePKM','Casemate_PKM','pkm')
        self.casemate('HQCasemateNSV','Casemate_NSV','nsv')
        self.wrapper('HQCommand','Command_GuardHouse','command')
        self.wrapper('HQBarracks','Barracks','barracks')
        self.wrapper('HQShelter','Shelter','shelter')
        self.wrapper('HQPillbox','Pillbox','pillbox')
        self.wrapper('HQTower','Tower','tower')
        self.wrapper('HQGuardBox','GuardBox','guardbox')
        self.obstacles('HQTeeth','Belt_Teeth',[('teeth1',(-1.7,0,0),(0,0,0)),('teeth2',(0,0,0.3),(0,90,0)),('teeth1',(1.7,0,-0.1),(0,180,0))])
        self.obstacles('HQHedgehogs','Belt_Hedgehogs',[('hedgehog',(-1.4,0,0.2),(0,15,0)),('hedgehog',(1.4,0,-0.2),(0,-30,0))])
        self.obstacles('HQWire','Belt_Wire',[('wire',(0,0,0),(0,90,0))])
        self.outputs['docs/headquarters-catalog.json']=json.dumps(self.catalog,indent=2)+'\n'
        calls='\n'.join(f'\t\tMeasure(world, "{k}", "{v["prefab"]}");' for k,v in self.catalog.items())
        calls+='\n'+'\n'.join(f'\t\tCheckPosts(world, "{k}", "{v["prefab"]}", {v["posts"]}, {v["addons"]}, {v["ladders"]});' for k,v in self.catalog.items() if 'posts' in v)
        self.outputs['Scripts/WorkbenchGame/IA_HeadquartersProbe.c']=(
            '#ifdef WORKBENCH\n'
            '// Generated by tools/author_headquarters.py. Preview geometry only: proves\n'
            '// the authored HQ prefabs load and gives mesh bounds, not live RPL or AI.\n'
            '[WorkbenchPluginAttribute(name: "IA headquarters measurement", wbModules: {"ResourceManager"})]\n'
            'class IA_HeadquartersProbe : IA_BaseCompositionProbe\n{\n'
            '\toverride void RunCommandline()\n\t{\n'
            '\t\tref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAHeadquartersProbe");\n'
            '\t\tBaseWorld world = preview.GetRef();\n\t\tm_World = world;\n'
            +calls+'\n'
            '\t\tPrint(string.Format("[IA][HeadquartersProbe] failures=%1", m_iFailures), LogLevel.NORMAL);\n'
            '\t\tWorkbench.Exit(m_iFailures);\n\t}\n'
            +PROBE_POSTS+
            '}\n#endif\n')
        return self.outputs


MEASURE=re.compile(r'\[IA\]\[CompositionMeasure\] key=(\w+) count=(\d+) min=<([^>]*)> max=<([^>]*)>')


def parse_measurements(text,evidence):
    assets={}
    for key,count,low,high in MEASURE.findall(text):
        assets[key]={'count':int(count),'mins':[float(v) for v in low.split(',')],'maxs':[float(v) for v in high.split(',')]}
    return {'evidence':evidence,'scope':'Workbench Preview mesh bounds and hierarchy; not live physics/AI/RPL certification','assets':assets}


# ---------------------------------------------------------------- designs

def cost(m):
    return m['count']+max(2,math.ceil(m['count']*.05))


def half_extent(m):
    return max(abs(m['mins'][0]),abs(m['maxs'][0])),max(abs(m['mins'][2]),abs(m['maxs'][2]))


def wall_extent(W,D):
    """Wall half-extents snapped to whole panels so corners close exactly."""
    return round((W-WALL_IN)*2/PANEL)*PANEL/2,round((D-WALL_IN)*2/PANEL)*PANEL/2


def side_axis(side,Wl,Dl):
    """(horizontal, fixed coordinate, half length) for a wall face."""
    if side in (0,2):
        return True,Dl*(1 if side==0 else -1),Wl
    return False,Wl*(1 if side==1 else -1),Dl


def gate_blocked(side,a,b):
    # Same openings as the scrappy recipes and m_aEntries: front |x|<5, sides z in [-13,-3].
    if side==2:
        return b>-5 and a<5
    if side in (1,3):
        return b>-13 and a<-3
    return False


class Plan:
    def __init__(self,size_id,variant,catalog,measure):
        self.size=size_id; self.variant=variant
        self.name,self.W,self.D,self.garrison=SIZES[size_id]
        self.archetype=variant//3
        self.rng=random.Random(40503+size_id*1999+variant*337)
        self.catalog=catalog; self.measure=measure
        self.Wl,self.Dl=wall_extent(self.W,self.D)
        self.modules=[]; self.walls=[]; self.belt=[]
        self.guns=0; self.heavy=0
        self.lanes=[]
        self.capture=[0,0,0]
        n=[int(round(2*self.Wl/PANEL)),int(round(2*self.Dl/PANEL))]
        # Slot state per face: None wall, 'gate', or a casemate key.
        self.slots={s:[None]*(n[0] if s in (0,2) else n[1]) for s in range(4)}
        for s in range(4):
            horizontal,_,half=side_axis(s,self.Wl,self.Dl)
            for i in range(len(self.slots[s])):
                a=-half+i*PANEL
                if gate_blocked(s,a,a+PANEL):
                    self.slots[s][i]='gate'

    def item(self,key,x,z,yaw,role,side,required):
        m=self.measure[key]
        hw,hd=half_extent(m)
        pad=0 if side>=0 else 1
        return {'key':key,'position':[round(x,3),0,round(z,3)],'yaw':yaw,'role':role,'side':side,'required':required,
                'half_width':round(hw+pad,4),'half_depth':round(hd+pad,4),'expanded':cost(m)}

    def limits(self):
        # Wall panels are 0.49 m thick; keep the padded interior box 1.2 m
        # clear of the inner face so clearance traces never hit our own wall.
        return self.Wl-0.5-1.2,self.Dl-0.5-1.2

    def inside(self,a):
        lim_x,lim_z=self.limits()
        return a[0]>=-lim_x-1e-6 and a[2]<=lim_x+1e-6 and a[1]>=-lim_z-1e-6 and a[3]<=lim_z+1e-6

    def snug(self,key,yaw,cx,cz):
        """Position that puts the padded box against the interior limits."""
        hw,hd=half_extent(self.measure[key])
        if yaw%180:
            hw,hd=hd,hw
        lim_x,lim_z=self.limits()
        # Round inward so the 3-decimal recipe never pokes past the limit.
        return cx*math.floor((lim_x-hw-1)*100)/100,cz*math.floor((lim_z-hd-1)*100)/100

    def aprons(self,item):
        """Clear ground in front of each door, from the unpadded mesh edge out.
        Returns (box, depth) per door, main door first."""
        pad=0 if item['side']>=0 else 1
        x,z=item['position'][0],item['position'][2]
        hw,hd=item['half_width']-pad,item['half_depth']-pad
        c,s=round(math.cos(math.radians(item['yaw']))),round(math.sin(math.radians(item['yaw'])))
        W,D=(hd,hw) if item['yaw']%180 else (hw,hd)
        out=[]
        for (nx,nz),along,depth in DOORS.get(item['key'],[]):
            lx,lz=(along,nz*hd) if nz else (nx*hw,along)
            # Yaw 90 turns local +Z to world +X.
            px,pz=x+lx*c+lz*s,z-lx*s+lz*c
            wx,wz=nx*c+nz*s,-nx*s+nz*c
            if wx:
                edge=x+wx*W
                a=(min(edge,edge+wx*depth),pz-APRON_HALF,max(edge,edge+wx*depth),pz+APRON_HALF)
            else:
                edge=z+wz*D
                a=(px-APRON_HALF,min(edge,edge+wz*depth),px+APRON_HALF,max(edge,edge+wz*depth))
            out.append((tuple(round(v,3) for v in a),(wx,wz)))
        return out

    def doors_clear(self,item):
        """Doors open onto walkable yard: inside the inner wall face and off
        every other module; the new item also stays off existing aprons."""
        inner_x,inner_z=self.Wl-0.5,self.Dl-0.5
        own=box(dict(item,half_width=item['half_width']-(0 if item['side']>=0 else 1),
                     half_depth=item['half_depth']-(0 if item['side']>=0 else 1)))
        for a,_ in self.aprons(item):
            if a[0]<-inner_x or a[2]>inner_x or a[1]<-inner_z or a[3]>inner_z:
                return False
            for o in self.modules:
                if overlap(a,self.bare(o)):
                    return False
        for o in self.modules:
            for a,_ in self.aprons(o):
                if overlap(own,a):
                    return False
        return True

    def bare(self,o):
        pad=0 if o['side']>=0 else 1
        return box(dict(o,half_width=o['half_width']-pad,half_depth=o['half_depth']-pad))

    def insert(self,key,x,z,yaw,role,side=-1,required=False,ignore_lane=False):
        socks=self.catalog[key]['sockets']
        heavy=sum(s['kind']>0 for s in socks)
        if self.guns+len(socks)>CAPS[self.size] or self.heavy+heavy>int(self.size<2):
            return False
        item=self.item(key,x,z,yaw,role,side,required)
        a=box(item)
        if not self.doors_clear(item):
            return False
        if side<0:
            if not self.inside(a):
                return False
            if any(overlap(a,box(o),0 if o['side']>=0 else 1) for o in self.modules):
                return False
            if not ignore_lane and any(overlap(a,lane,1) for lane in self.lanes):
                return False
            if role!='Hq' and overlap(a,(self.capture[0]-.5,self.capture[2]-.5,self.capture[0]+.5,self.capture[2]+.5),0):
                return False
        self.modules.append(item)
        self.guns+=len(socks); self.heavy+=heavy
        return True

    # -- perimeter -------------------------------------------------------
    def casemate(self,side,fraction,key):
        slots=self.slots[side]
        n=len(slots)
        start=int(round((fraction+1)/2*n))-1
        for delta in (0,1,-1,2,-2,3,-3,4,-4):
            i=min(max(start+delta,1),n-3)
            if slots[i] is None and slots[i+1] is None and slots[i-1]!='gate' and slots[i+2]!='gate':
                horizontal,fixed,half=side_axis(side,self.Wl,self.Dl)
                along=-half+(i+1)*PANEL
                x,z=(along,fixed) if horizontal else (fixed,along)
                if not self.insert(key,x,z,[0,90,180,270][side],'Cover',side,True):
                    return False
                slots[i]=slots[i+1]=key
                return True
        return False

    def build_walls(self):
        mix=['HQWallA','HQWallB','HQWallA','HQWallC'] if self.variant%3!=1 else ['HQWallB','HQWallA','HQWallC','HQWallA']
        for side in range(4):
            horizontal,fixed,half=side_axis(side,self.Wl,self.Dl)
            slots=self.slots[side]
            i=0; count=0
            while i<len(slots):
                if slots[i] is not None:
                    i+=1; continue
                pair=i+1<len(slots) and slots[i+1] is None
                width=2 if pair else 1
                along=-half+(i+width/2)*PANEL
                x,z=(along,fixed) if horizontal else (fixed,along)
                key=mix[(count+self.variant)%len(mix)] if pair else 'HQWallPanel'
                self.walls.append({'key':key,'position':[round(x,4),0,round(z,4)],'yaw':[0,90,180,270][side],'side':side,
                                   'half_width':round(PANEL*width/2,4),'half_depth':0.6,'expanded':cost(self.measure[key])})
                count+=1; i+=width

    def build_belt(self):
        """Obstacle belt between the wall's outer face and the site footprint."""
        styles=[['HQTeeth','HQWire'],['HQWire','HQWire','HQHedgehogs'],['HQHedgehogs','HQWire'],['HQTeeth','HQTeeth','HQWire']][self.archetype]
        spacing=[9,10,11,12,12,12][self.size]
        for side in range(4):
            horizontal=side in (0,2)
            wall=self.Dl if horizontal else self.Wl
            edge=self.D if horizontal else self.W
            gap=edge-wall-0.5
            rows=[wall+0.5+gap/2]
            if self.archetype==3 and gap>=4.6:
                rows=[wall+1.7,edge-1.1]
            for r,offset in enumerate(rows):
                fixed=offset*(1 if side in (0,1) else -1)
                half=(self.Wl if horizontal else self.Dl)+0.5
                t=-half+2+r*spacing/2
                k=0
                while t<=half-2:
                    gate=(side==2 and abs(t)<8) or (side in (1,3) and -17<t<1)
                    if not gate:
                        key=styles[(k+side+self.variant)%len(styles)]
                        if len(rows)>1 and r==0 and key=='HQHedgehogs':
                            key='HQTeeth'
                        x,z=(t,fixed) if horizontal else (fixed,t)
                        m=self.measure[key]
                        hw,hd=half_extent(m)
                        item={'key':key,'position':[round(x,3),0,round(z,3)],'yaw':[0,90,180,270][side],'side':-1,
                              'half_width':round(hw,4),'half_depth':round(hd,4),'expanded':cost(m)}
                        a=box(item)
                        assert a[0]>=-self.W and a[2]<=self.W and a[1]>=-self.D and a[3]<=self.D,(self.name,self.variant,'belt',item)
                        assert abs(offset)-hd>=wall+0.49,(self.name,self.variant,'belt wall',item)
                        self.belt.append(item)
                        k+=1
                    t+=spacing

    # -- interior ----------------------------------------------------------
    def place_row(self,key,role,required,mirror):
        """Organised lots: inner, middle and outer columns per flank, long axis
        parallel to the road. Each item takes the lot farthest from what is
        already built so the yard reads as planned blocks, not a heap by the lane."""
        m=self.measure[key]
        hw,hd=half_extent(m)
        yaw=0 if hd>=hw else 90
        w,d=(hw,hd) if yaw==0 else (hd,hw)
        lim_x,lim_z=self.limits()
        inner=LANE_HALF+2+w+1
        outer=lim_x-w-1
        if outer<inner:
            return False
        columns=sorted({round(inner,2),round((inner+outer)/2,2),round(outer,2)})
        rows=[]
        z=-lim_z+d+1
        while z<=lim_z-d-1:
            rows.append(round(z,2)); z+=2
        others=[((box(o)[0]+box(o)[2])/2,(box(o)[1]+box(o)[3])/2) for o in self.modules]
        best=None
        for s in (mirror,-mirror):
            for x in columns:
                for z in rows:
                    for flip in self.door_flips(key,s*x,z,yaw,role,required):
                        item=self.item(key,s*x,z,(yaw+flip)%360,role,-1,required)
                        if not self.fits(item):
                            continue
                        score=min(math.dist((s*x,z),c) for c in others)
                        # Deterministic tie-break keeps the preferred flank first.
                        if best is None or score>best[0]+1e-6:
                            best=(score,s*x,z,(yaw+flip)%360)
                        break
        if not best:
            return False
        return self.insert(key,best[1],best[2],best[3],role,required=required)

    def door_flips(self,key,x,z,yaw,role,required):
        """Main door toward the lane (x=0) first, then toward the yard middle."""
        def facing(flip):
            aprons=self.aprons(self.item(key,x,z,(yaw+flip)%360,role,-1,required))
            if not aprons:
                return 0
            wx,wz=aprons[0][1]
            return -wx*math.copysign(1,x)*2-wz*math.copysign(1,z)
        return sorted((0,180),key=lambda f:-facing(f))

    def fits(self,item):
        a=box(item)
        if not self.inside(a):
            return False
        if not self.doors_clear(item):
            return False
        if any(overlap(a,box(o),0 if o['side']>=0 else 1) for o in self.modules):
            return False
        if any(overlap(a,lane,1) for lane in self.lanes):
            return False
        return not overlap(a,(self.capture[0]-.5,self.capture[2]-.5,self.capture[0]+.5,self.capture[2]+.5),0)

    def build(self):
        size=self.size; arch=self.archetype; v=self.variant%3
        mirror=1 if (self.variant+size)%2 else -1
        # HQ building against the rear wall, facing the gate down the lane.
        _,hq_z=self.snug('HQCommand',180,0,1)
        hq_x=[0,-mirror*4,mirror*4][v] if size<4 else 0
        assert self.insert('HQCommand',hq_x,hq_z,180,'Hq',required=True,ignore_lane=True),(self.name,self.variant,'hq')
        hq_box=box(self.modules[0])
        self.capture=[0,0,round(hq_box[1]-0.8,3)]
        self.lanes=[(-LANE_HALF,-self.D,LANE_HALF,self.capture[2])]
        # One concrete gun per face before extras; heavy NSV on the east face.
        heavy='HQCasemateNSV' if size<2 else 'HQCasematePKM'
        required=[(2,-.5,'HQCasematePKM'),(1,.3,heavy),(3,.3,'HQCasematePKM'),(0,-.55+.1*v,'HQCasematePKM')]
        for side,fraction,key in required:
            assert self.casemate(side,fraction,key),(self.name,self.variant,'casemate',side)
        # Interior overwatch across the gate throat where the gun cap allows.
        if size<2:
            # Faces across the gate throat: yaw 90 turns local +Z to world +X.
            gx=-mirror*(10+2*v)
            yaw=90 if gx<0 else 270
            _,gz=self.snug('HQCasematePKM',yaw,0,-1)
            assert self.insert('HQCasematePKM',gx,gz,yaw,'Cover'),(self.name,self.variant,'overwatch')
        extras=[(2,.55),(0,.5),(1,-.6),(3,-.6)]
        if arch==3:
            extras=[(2,.55),(1,-.6),(3,-.6),(0,.5)]
        for side,fraction in extras:
            if self.guns>=CAPS[size]:
                break
            self.casemate(side,fraction,'HQCasematePKM')
        # Towers inside the corners, stairs toward the yard.
        corners=[(-1,1),(1,1),(-1,-1),(1,-1)]
        count=[4,2,2,4][arch] if size<4 else 2
        if arch==1:
            corners=[corners[0],corners[3],corners[1],corners[2]] if v%2 else [corners[1],corners[2],corners[0],corners[3]]
        elif arch==2:
            corners=[corners[2],corners[3],corners[0],corners[1]]
        placed=0
        for cx,cz in corners:
            if placed>=count:
                break
            yaw=180 if cz>0 else 0
            x,z=self.snug('HQTower',yaw,cx,cz)
            if self.insert('HQTower',x,z,yaw,'Tower',required=False,ignore_lane=True):
                placed+=1
        # Gate guard box beside the front opening.
        if arch in (0,3):
            gate_edge=max(-self.Wl+(i+1)*PANEL for i,s in enumerate(self.slots[2]) if s=='gate')
            hw,_=half_extent(self.measure['HQGuardBox'])
            _,z=self.snug('HQGuardBox',0,0,-1)
            self.insert('HQGuardBox',mirror*(gate_edge+hw+1.5),z,0,'Cover',required=False,ignore_lane=True)
        barracks={0:'HQShelter',1:'HQBarracks' if size<2 else 'HQShelter',2:'LivingSmall',3:'HQShelter'}[arch]
        options=[barracks,'HQShelter','LivingSmall']
        assert any(self.place_row(k,'Barracks',True,mirror) for k in dict.fromkeys(options)),(self.name,self.variant,'barracks')
        services=[['HQPillbox','Supply','Medical','HQPillbox','Ammo','HQShelter','Fuel'],
                  ['HQShelter','Supply','Hospital','HQPillbox','Fuel','MaintenanceSmall'],
                  ['HQShelter','Supply','Ammo','Fuel','MaintenanceLarge','MaintenanceSmall','HQShelter'],
                  ['HQPillbox','HQPillbox','Ammo','HQShelter','Supply','Medical']][arch][:]
        services=services[v:]+services[:v]
        if size>=4:
            services=[{'Hospital':'Medical','MaintenanceLarge':'MaintenanceSmall'}.get(k,k) for k in services]
        limit=[7,6,5,4,3,2][size]
        placed=0
        for key in services+['HQPillbox','Fuel','Supply']:
            if placed>=limit:
                break
            if self.place_row(key,'Supply',True,-mirror if placed%2 else mirror):
                placed+=1
        self.build_walls()
        self.build_belt()
        self.check()
        return self.result()

    def check(self):
        guns_by_side={m['side'] for m in self.modules if self.catalog[m['key']]['sockets'] and m['side']>=0}
        assert guns_by_side=={0,1,2,3},(self.name,self.variant,guns_by_side)
        assert self.guns>=4 and self.guns<=CAPS[self.size]
        roots=len(self.modules)+len(self.walls)+len(self.belt)
        assert roots<=ROOT_LIMIT,(self.name,self.variant,'roots',roots)
        assert self.expanded()+self.guns*12<=RECIPE_EXPANDED_BUDGET,(self.name,self.variant,'expanded')
        posts=self.posts()
        assert len(posts)>=(16 if self.size<2 else 6),(self.name,self.variant,'posts',len(posts))
        self._posts=posts

    def expanded(self):
        return sum(m['expanded'] for m in self.modules+self.walls+self.belt)

    def posts(self):
        posts=[]
        lim_x=int(self.Wl)-4; lim_z=int(self.Dl)-4
        points=[self.capture,[0,0,-self.Dl+5],[-self.Wl+5,0,-8],[self.Wl-5,0,-8]]
        tail=[[x,0,z] for z in range(-lim_z,lim_z+1,4) for x in range(-lim_x,lim_x+1,4)]
        self.rng.shuffle(tail)
        for p in points+tail:
            a=(p[0]-1.5,p[2]-1.5,p[0]+1.5,p[2]+1.5)
            if any(overlap(a,box(m),1) for m in self.modules):
                continue
            if any(math.dist((p[0],p[2]),(q[0],q[2]))<5 for q in posts):
                continue
            posts.append([round(c,3) for c in p])
            if len(posts)>=self.garrison:
                break
        return posts

    def result(self):
        return {'name':f'{self.name} / HQ {ARCHETYPES[self.archetype]} {self.variant%3+1}','size':self.size,'variant':self.variant,
                'archetype':ARCHETYPES[self.archetype],'half_width':self.W,'half_depth':self.D,'wall_half_width':self.Wl,
                'wall_half_depth':self.Dl,'garrison':self.garrison,'capture':self.capture,'modules':self.modules,
                'walls':self.walls,'belt':self.belt,'posts':self._posts,'guns':self.guns,'heavy':self.heavy,
                'expanded':self.expanded(),'roots':len(self.modules)+len(self.walls)+len(self.belt)}


def catalog_script(catalog,measure):
    lines=['// Generated by tools/author_headquarters.py. Mesh bounds from native Preview.',
           'class IA_HeadquartersCatalog','{','\tstatic IA_BaseCompositionAsset Get(string key)','\t{',
           '\t\tref IA_BaseCompositionAsset asset;','\t\tIA_BaseGunSocket socket;','\t\tswitch (key)','\t\t{']
    for key,entry in catalog.items():
        m=measure[key]
        lines += [f'\t\t\tcase "{key}":',f'\t\t\t\tasset = IA_BaseCompositionAsset.Create(key, "{entry["prefab"]}", {vec(m["mins"])}, {vec(m["maxs"])}, {cost(m)});']
        for socket in entry['sockets']:
            lines += [f'\t\t\t\tsocket = asset.AddSocket({socket["kind"]});']
            for pose in socket['chain']:
                lines += [f'\t\t\t\tsocket.Append({vec(pose["position"])}, {vec(pose["angles"])});']
        lines += ['\t\t\t\tbreak;']
    return '\n'.join(lines+['\t\t}','\t\treturn asset;','\t}','}',''])


def recipe_script(recipes):
    lines=['// Generated by tools/author_headquarters.py. No runtime packing search.','class IA_HeadquartersRecipes','{',
           f'\tstatic const int VARIANTS = {HQ_VARIANTS};','',
           '\tstatic IA_DynamicSiteLayout Create(int size, int variant)','\t{',
           '\t\tif (size < 0 || size > 5 || variant < 0 || variant >= VARIANTS)','\t\t\treturn null;',
           '\t\tref IA_HeadquartersSiteLayout layout = new IA_HeadquartersSiteLayout();',
           '\t\tint recipe = size * VARIANTS + variant;','\t\tswitch (recipe)','\t\t{']
    for i in range(len(recipes)):
        lines += [f'\t\t\tcase {i}:',f'\t\t\t\tBuild{i}(layout);','\t\t\t\tbreak;']
    lines += ['\t\t\tdefault:','\t\t\t\treturn null;','\t\t}','\t\treturn layout;','\t}']
    for i,r in enumerate(recipes):
        lines += [f'\tprotected static void Build{i}(IA_HeadquartersSiteLayout layout)','\t{',
                  f'\t\tlayout.Initialize({r["size"]}, IA_BaseDesignLibrary.HQ_BASE + {r["variant"]}, "{r["name"]}", {r["half_width"]}, {r["half_depth"]}, {r["garrison"]});',
                  f'\t\tlayout.m_vCaptureLocal = {vec(r["capture"])};']
        for m in r['modules']:
            required='true' if m['required'] else 'false'
            lines += [f'\t\tlayout.AddComposition("{m["key"]}", {vec(m["position"])}, {m["yaw"]}, IA_DynamicSiteModuleRole.{m["role"]}, {m["side"]}, {required});']
        for w in r['walls']:
            lines += [f'\t\tlayout.AddWallRun("{w["key"]}", {vec(w["position"])}, {w["yaw"]}, {w["side"]});']
        for b in r['belt']:
            lines += [f'\t\tlayout.AddObstacle("{b["key"]}", {vec(b["position"])}, {b["yaw"]});']
        for p in r['posts']:
            lines += [f'\t\tlayout.m_aGuardPosts.Insert({vec(p)});']
        lines += ['\t}']
    return '\n'.join(lines+['}',''])


def sheet(recipes):
    picks=[recipes[n] for n in (0,3,6,9,12+1,60+10)]
    rows=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="1150" viewBox="0 0 1200 1150">',
          '<rect width="1200" height="1150" fill="#14181c"/>','<g font-family="sans-serif" fill="#e3e8ea">',
          '<text x="20" y="28" font-size="19">Permanent HQ recipes - generated plans (not live screenshots)</text>']
    colors={'Hq':'#a33b32','Cover':'#8a8f93','Tower':'#c9a227','Barracks':'#3d7a99','Supply':'#4f7d4a'}
    for i,r in enumerate(picks):
        cx=300+(i%2)*600; cy=215+(i//2)*355; scale=min(2.3,150/r['half_depth'])
        rows += [f'<text x="{cx-265}" y="{cy-165}" font-size="16">{r["name"]}</text>',
                 f'<rect x="{cx-r["half_width"]*scale}" y="{cy-r["half_depth"]*scale}" width="{r["half_width"]*2*scale}" height="{r["half_depth"]*2*scale}" fill="none" stroke="#4b555c" stroke-dasharray="4 3"/>']
        for b in r['belt']:
            a,bb,c,d=box(b)
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="#6b5a3a"/>']
        for w in r['walls']:
            a,bb,c,d=box(w)
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="#c8c8c0"/>']
        for m in r['modules']:
            a,bb,c,d=box(m)
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="{colors.get(m["role"],"#777")}" fill-opacity="0.85"/>',
                     f'<text x="{cx+(a+c)*scale/2:.2f}" y="{cy-(bb+d)*scale/2:.2f}" text-anchor="middle" font-size="8">{m["key"].replace("HQ","")}</text>']
        for p in r['posts']:
            rows += [f'<circle cx="{cx+p[0]*scale:.2f}" cy="{cy-p[2]*scale:.2f}" r="2" fill="#ffcf73"/>']
    return '\n'.join(rows+['</g></svg>',''])


def load_inputs():
    base_catalog=json.loads((ROOT/'docs/base-composition-catalog.json').read_text())
    base_measure=json.loads((ROOT/'docs/base-composition-measurements.json').read_text())['assets']
    hq_catalog=json.loads((ROOT/'docs/headquarters-catalog.json').read_text())
    hq_measure=json.loads((ROOT/'docs/headquarters-measurements.json').read_text())['assets']
    catalog={**base_catalog,**hq_catalog}
    measure={**base_measure,**hq_measure}
    return hq_catalog,hq_measure,catalog,measure


def generate_designs():
    hq_catalog,hq_measure,catalog,measure=load_inputs()
    recipes=[Plan(size,v,catalog,measure).build() for size in range(6) for v in range(HQ_VARIANTS)]
    return {'Scripts/Game/IA_HeadquartersCatalog.c':catalog_script(hq_catalog,hq_measure),
            'Scripts/Game/IA_HeadquartersRecipes.c':recipe_script(recipes),
            'docs/headquarters-designs.json':json.dumps(recipes,indent=2)+'\n',
            'docs/headquarters-designs.svg':sheet(recipes)}


def emit(outputs,check):
    for path,content in outputs.items():
        target=ROOT/path
        if check:
            assert target.read_text(encoding='utf-8')==content,path
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(content,encoding='utf-8')
    print(f'{len(outputs)} headquarters outputs'+(' checked' if check else ''))


def main():
    parser=argparse.ArgumentParser()
    sub=parser.add_subparsers(dest='stage',required=True)
    a=sub.add_parser('assets'); a.add_argument('data'); a.add_argument('--check',action='store_true')
    m=sub.add_parser('measure'); m.add_argument('log'); m.add_argument('out'); m.add_argument('--evidence',default='')
    d=sub.add_parser('designs'); d.add_argument('--check',action='store_true')
    args=parser.parse_args()
    if args.stage=='assets':
        emit(HeadquartersAuthor(args.data).generate(),args.check)
    elif args.stage=='measure':
        data=parse_measurements(Path(args.log).read_text(encoding='utf-8',errors='replace'),args.evidence or str(Path(args.log).parent))
        assert data['assets'],'no [IA][CompositionMeasure] lines'
        emit({args.out:json.dumps(data,indent=2)+'\n'},False)
    else:
        emit(generate_designs(),args.check)


if __name__=='__main__':
    main()
