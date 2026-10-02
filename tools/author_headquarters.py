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
from author_base_designs import SHELTERS, SHELTER_EXPANDED, square, shelter_box, shelter_entrance, gun_reserves, plan_shelters

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
    # Game Master "Sandbag_Camo_*_USSR" sub-compositions are a stock sandbag
    # with a Soviet camo net child; the runs rebuild exactly that pairing.
    'bagHigh':'{9C9C4BED9E19C374}Prefabs/Props/Military/Sandbags/Sandbag_01_wall_solid_burlap.et',
    'bagWindow':'{CD67070EFAFC28C7}Prefabs/Props/Military/Sandbags/Sandbag_01_wall_burlap.et',
    'bagLong':'{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et',
    'bagEnd':'{B547CF929FCC6BB7}Prefabs/Props/Military/Sandbags/Sandbag_01_end_high_burlap.et',
    'bagRound':'{7AF4B627D5C90235}Prefabs/Props/Military/Sandbags/Sandbag_01_round_high_burlap.et',
    'bagBunker':'{5821AF9613353C80}Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_burlap_camonet.et',
    'netHigh':'{0667675C48220BD6}Prefabs/Structures/Military/CamoNets/Soviet/CamoNet_Wall_high_Soviet.et',
    'netMedium':'{08313D18309DAC02}Prefabs/Structures/Military/CamoNets/Soviet/CamoNet_Wall_medium_Soviet.et',
    'netTop':'{8088790C2D455045}Prefabs/Structures/Military/CamoNets/Soviet/CamoNet_Small_Top_Soviet.et',
    'gateHolder':'Prefabs/Structures/Infrastructure/Barriers/BarGate_01/BarGate_01_base.et',
    'gateBar':'Prefabs/Structures/Infrastructure/Barriers/BarGate_01/BarGate_01_bar_base.et',
    'plateA':'{C1B7AB00FF7C1424}Prefabs/Props/Industrial/ConcretePanel_01_A.et',
    'plateB':'{ADE39FE043B89B0A}Prefabs/Props/Industrial/ConcretePanel_01_B.et',
    'plateC':'{487FD3194F5D0C61}Prefabs/Props/Industrial/ConcretePanel_01_C.et',
    'plateD':'{754BF6213A318556}Prefabs/Props/Industrial/ConcretePanel_01_D.et',
    'plateStack':'{795552B887CD50E7}Prefabs/Props/Industrial/ConcretePanel_01_stack.et',
    'duckboard':'{CC66836BF4E211F9}Prefabs/Props/Military/Camps/Duckboard_01.et',
    'roadblock':'{64C9C1D1C2C1B38D}Prefabs/Props/Infrastructure/Roadblocks/RoadBlock_01.et',
    'signCommand':'{7DA20C66CE0299D6}Prefabs/Structures/Signs/Military/Sign_CommandPost_USSR_01.et',
    'signWarning':'{547A7CACBC8B0865}Prefabs/Structures/Signs/Military/Sign_Warning_USSR_01.et',
    'stop':'{C1977A326F2AA011}Prefabs/Structures/Signs/Military/SignCheckpoint_01_stop.et',
    'knife':'{DDF59362051B28BC}Prefabs/Props/Military/Fortification/BarbedTape_KnifeRest.et',
    'chair':'{172DD50ACF177B9E}Prefabs/Props/Military/Furniture/ChairMilitary_USSR_01.et',
    'flag':'{651E545B53BBD034}Prefabs/Structures/Military/Flags/FlagPole_02/FlagPole_02_V1_USSR.et',
    'decalPatch1':'{BAA55688399B3545}Prefabs/Decals/Dirt/Decal_Dirt_Patch_01.et',
    'decalPatch2':'{AF4AA779A4546D51}Prefabs/Decals/Dirt/Decal_Dirt_Patch_02.et',
    'decalPatch3':'{4AD6EB80A8B1FA3A}Prefabs/Decals/Dirt/Decal_Dirt_Patch_03.et',
    'decalPatch4':'{ACAEDB73441F87DB}Prefabs/Decals/Dirt/Decal_Dirt_Patch_04.et',
    'decalPatch6':'{2566A36AF43E9F9E}Prefabs/Decals/Dirt/Decal_Dirt_Patch_06.et',
    'decalCross':'{D0ADBF5A3B6783C0}Prefabs/Decals/Dirt/Decal_Dirt_Crossroad_01.et',
    'decalSteps':'{FC56FA6166DEE7B7}Prefabs/Decals/Dirt/Decal_Dirt_Footprints_01.et',
}
# Net offset from the vanilla Sandbag_Camo_wall_high_USSR sub-composition.
CAMO_NET=(-0.02,-0.022)
# Sandbags have no buried foundation (measured y -0.07); sink runs so a
# terrain-following run never shows daylight under a bag.
BAG_SINK=0.1
# Workbench Preview (logs_2026-09-26_03-08-37): BarGate_01 socket_bar sits at
# (-4.284, 1.34, 0.09) and maps bar local -Z (arm, 8.53 m) to holder +X; the
# DoorComponent swings about bar local Y (holder -Z) by its -70 degree range.
BAR_OPEN=-70
BAR_SOCKET_X=-4.284
# ConcretePanel_01 road plate: 3.86 x 1.93 m, 0.22 m thick, already flat.
PLATE_W=3.86
PLATE_D=1.93
PLATE_SINK=0.09
# Existing lived-in vignettes (Prefabs/DynamicBase) with the scrappy
# layout's authored footprints: (prefab, half width, half depth, expanded).
VIGNETTES={
    'VigBriefing':('{986F4B80B0BE52E0}Prefabs/DynamicBase/IA_Dressing_BriefingLit.et',2.3,2.6,9),
    'VigComms':('{D7D10DFBACA05AD0}Prefabs/DynamicBase/IA_Dressing_CommsLit.et',2,1.3,5),
    'VigPower':('{3F22F8B9E0EB5442}Prefabs/DynamicBase/IA_Dressing_Power.et',2,2.5,7),
    'VigKitchen':('{7381F407AD8A54B8}Prefabs/DynamicBase/IA_Dressing_KitchenLit.et',3.5,3.2,6),
    'VigMess':('{06111FFC070A5834}Prefabs/DynamicBase/IA_Dressing_MessLit.et',1.7,1.5,7),
    'VigRest':('{754823CC537E5D85}Prefabs/DynamicBase/IA_Dressing_Rest.et',2,1.8,4),
    'VigWash':('{AB8A3CB589625084}Prefabs/DynamicBase/IA_Dressing_WaterWash.et',1.4,1.2,5),
    'VigWater':('{CE0933F9935A5FD4}Prefabs/DynamicBase/IA_Dressing_BulkWater.et',2.2,3.4,4),
    'VigSanitation':('{6F3672FEE4B857A1}Prefabs/DynamicBase/IA_Dressing_Sanitation.et',2.4,2.8,5),
    'VigWaste':('{A70917953C1D58AB}Prefabs/DynamicBase/IA_Dressing_Waste.et',1,0.7,3),
    'VigStores':('{EBACA024C71C5CAF}Prefabs/DynamicBase/IA_Dressing_Stores.et',1.8,2.9,6),
    'VigStoresCovered':('{674112EA1BA85649}Prefabs/DynamicBase/IA_Dressing_StoresCovered.et',1.9,1.6,6),
    'VigWorkshop':('{965FEDAFCD3F54D5}Prefabs/DynamicBase/IA_Dressing_WorkshopLit.et',2,1.3,7),
    'VigMedical':('{A9D76F51AE7B5D37}Prefabs/DynamicBase/IA_Dressing_MedicalLit.et',2,2,6),
    'VigLight':('{8726E742FEE65661}Prefabs/DynamicBase/IA_Dressing_EntranceLight.et',2,1.5,2),
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
# Walls within this many slots of a face corner or the front gate stay
# concrete; the rest of the ring is Game Master camo sandbag runs.
CONCRETE_SLOTS=2
# Dressing kinds: solid props reserve ground, decals only paint it.
SOLID=('mesh','road','gate')
# Residual (after following the terrain plane) each dressing kind tolerates.
RESIDUAL={'gate':0.6,'road':0.3,'mesh':0.35,'decal':0.8}
# Lived-in vignettes beside each building type, most characteristic first.
VIGNETTES_BY={'HQCommand':['VigBriefing','VigComms','VigPower'],
              'HQBarracks':['VigKitchen','VigMess','VigWash','VigWater','VigSanitation','VigRest','VigWaste'],
              'HQShelter':['VigRest','VigStoresCovered','VigWaste'],
              'LivingSmall':['VigKitchen','VigMess','VigWash','VigSanitation','VigWaste'],
              'Supply':['VigStores','VigStoresCovered'],'Ammo':['VigStoresCovered'],
              'MaintenanceSmall':['VigWorkshop','HQPlateStack'],'MaintenanceLarge':['VigWorkshop','HQPlateStack'],
              'Medical':['VigMedical','VigWater'],'Hospital':['VigMedical','VigWater'],
              'Fuel':['VigPower'],'HQGuardBox':['VigLight']}
# Destructible stock scenery with its own RplComponent gets a mesh-only
# wrapper; sandbags, nets, signs and knife rests stay stock children like the
# vanilla checkpoints and Game Master sub-compositions.
MESH_ONLY={'panelV1','panelV2','panelV3','panelDamaged','gateHolder','gateBar','plateA','plateB','plateC','plateD',
           'plateStack','duckboard','roadblock','signCommand','signWarning'}


# Emitted into IA_HeadquartersProbe: vanilla CoverPost/ObservationPost smart
# actions must survive the mesh-only wrapper (IA area garrisons query them).
PROBE_POSTS='''
	// Decal scenes and reused vignettes have authored footprints, not mesh bounds.
	protected void Load(BaseWorld world, string key, ResourceName name)
	{
		ref EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(name), world, params);
		Check(entity != null, "load resource " + key);
		if (!entity)
			return;
		int children = 0;
		IEntity child = entity.GetChildren();
		while (child)
		{
			children++;
			child = child.GetSibling();
		}
		Check(children > 0, "loaded scene has children " + key);
		Print(string.Format("[IA][HeadquartersProbe] load key=%1 class=%2 children=%3", key, entity.ClassName(), children), LogLevel.NORMAL);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

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

    def piece(self,out,index,name,coords,angles=(0,0,0)):
        """Stock child, or the mesh-only wrapper for destructible scenery."""
        if name in MESH_ONLY:
            return self.child(out,index,self.static_mesh(name),coords,angles)
        return self.child(out,index,self.stock_resource(name),coords,angles)

    def scene(self,key,name,items,footprint=None):
        """Hand-placed scene: (stock name, coords, angles) per child."""
        def children(out):
            return [self.piece(out,i,s,c,a) for i,(s,c,a) in enumerate(items)]
        self.composition(key,name,children)
        if footprint:
            self.catalog[key]['measure']=footprint

    def camo_run(self,key,name,units):
        """Game Master camo sandbag wall, one bag per 2.698 m wall slot.

        The 2.97 m bags overlap their neighbour by 0.27 m, so a run that
        follows the grade still reads as one continuous wall."""
        start=-PANEL*(len(units)-1)/2
        def children(out):
            nodes=[]
            for i,(bag,net) in enumerate(units):
                x=start+i*PANEL
                nodes.append(self.piece(out,len(nodes),bag,(x,-BAG_SINK,0)))
                if net:
                    nodes.append(self.piece(out,len(nodes),net,(x+CAMO_NET[0],-BAG_SINK,CAMO_NET[1])))
            return nodes
        self.composition(key,name,children)

    def plates(self,rows,cols=2,z0=None):
        """Flat road plates, row-major, variants alternating like a laid road."""
        kinds=['plateA','plateC','plateB','plateD']
        z0=-PLATE_D*(rows-1)/2 if z0 is None else z0
        return [(kinds[(r*cols+c+r)%4],(PLATE_W*(c-(cols-1)/2),-PLATE_SINK,z0+r*PLATE_D),(0,180*((r+c)%2),0))
                for r in range(rows) for c in range(cols)]

    def gate(self,key,name,edge):
        """Formal front gate. Local +Z faces outward like the casemates.

        Concrete wing walls flare out from both wall ends, the stock bar gate
        stands open over the lane (mesh only, so it never closes on a convoy)
        and a road-plate apron runs through the opening."""
        # Clear of the narrow gate's wing panel pillar at x=edge.
        holder_x=0.1
        items=[('panelV1',(edge,0,0),(0,270,0)),('panelV3',(-edge,0,0),(0,270,0)),
               ('signCommand',(edge+1.3,0,1.6),(0,180,0)),('signWarning',(-edge-1.3,0,1.6),(0,180,0)),
               ('stop',(-3.4,0,4.0),(0,180,0)),
               ('roadblock',(edge-0.9,0,3.2),(0,90,0)),('roadblock',(-edge+0.9,0,3.2),(0,90,0))]
        items+=self.plates(4)
        bar=self.static_mesh('gateBar')
        bar_path=bar.split('}',1)[1]
        def children(out):
            nodes=[self.piece(out,i,s,c,a) for i,(s,c,a) in enumerate(items)]
            # Holder yawed 180: its socket lands at local x=holder_x-BAR_SOCKET_X
            # and the raised arm leans back over the lane.
            holder=self.child(out,len(nodes),self.static_mesh('gateHolder'),(holder_x,0,0),(0,180,0))
            holder.body.append(Node('',[Node('StaticModelEntity : "'+bar+'"',[
                'ID "'+guid(out+'/bar')+'"','angles '+fmt((0,BAR_OPEN,0)),
                Node('components',[Node('Hierarchy "{'+guid(bar_path+'/Hierarchy')+'}"',['Enabled 1','PivotID "socket_bar"','AutoTransform 1'])])])]))
            return nodes+[holder]
        self.composition(key,name,children)

    def side_gate(self,key,name,edge):
        """Side entry closed down to a 4.4-4.8 m walk-through by camo sandbag
        wings, with a knife-rest chicane outside. Local +Z faces outward."""
        items=[]
        for s in (1,-1):
            items+=[('bagHigh',(s*(edge-1.5),-BAG_SINK,0),(0,0,0)),('netHigh',(s*(edge-1.5)+CAMO_NET[0],-BAG_SINK,CAMO_NET[1]),(0,0,0))]
            if edge-3>3:
                items.append(('bagEnd',(s*(edge-3.5),-BAG_SINK,0),(0,0,0)))
        items+=[('knife',(1.2,0,2.6),(0,90,0)),('stop',(-2.6,0,3.6),(0,180,0)),('signWarning',(edge-1.0,0,2.2),(0,180,0))]
        self.scene(key,name,items)

    def decals(self,key,name,items,radius):
        """Ground decals under a replicated root, so clients draw them too.

        Projected down from 0.6 m to 1.4 m below the snapped root, which
        covers the residual of a terrain-oriented pad without reaching
        the tops of nearby props."""
        def children(out):
            nodes=[]
            for i,(decal,(x,z),scale) in enumerate(items):
                ref=STOCK[decal]
                nodes.append(Node('DecalEntity : "'+ref+'"',['ID "'+guid(out+'/decal/'+str(i))+'"',
                    Node('components',[Node('Hierarchy "'+self.component_id(stock_path(decal),'Hierarchy')+'"')]),
                    'coords '+fmt((x,0.6,z)),'angles -90 0 0','scale '+fmt((scale,)),'FarPlane 2']))
            return nodes
        self.composition(key,name,children)
        self.catalog[key]['measure']={'count':len(items),'mins':[-radius,0,-radius],'maxs':[radius,0.2,radius]}

    def vignette(self,key,prefab,hw,hd,expanded):
        self.catalog[key]={'prefab':prefab,'sockets':[],'source':'dressing',
                           'measure':{'count':expanded,'mins':[-hw,0,-hd],'maxs':[hw,2.5,hd]}}

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
        high=('bagHigh','netHigh'); window=('bagWindow',None)
        self.camo_run('HQCamoWallA','CamoWall_A',[high,high])
        self.camo_run('HQCamoWallB','CamoWall_B',[high,window])
        self.camo_run('HQCamoWallC','CamoWall_C',[window,high])
        self.camo_run('HQCamoWallPanel','CamoWall_Panel',[high])
        # Three-slot runs keep the two largest rings under the root cap.
        self.camo_run('HQCamoWallLongA','CamoWall_LongA',[high,window,high])
        self.camo_run('HQCamoWallLongB','CamoWall_LongB',[high,high,high])
        # Front openings are 4 or 5 wall slots wide depending on panel parity.
        self.gate('HQGateNarrow','Gate_Narrow',2*PANEL)
        self.gate('HQGateWide','Gate_Wide',2.5*PANEL)
        self.side_gate('HQSideGateNarrow','SideGate_Narrow',2*PANEL)
        self.side_gate('HQSideGateWide','SideGate_Wide',2.5*PANEL)
        # Inner checkpoint behind the front gate, in three roots so each clears
        # the casemate reservation boxes on its own. Local +Z faces the HQ.
        # The knife rests leave a 3.7 m slalom for vehicles.
        self.scene('HQChicane','Chicane',[('knife',(-2.1,0,-2.5),(0,90,0)),('knife',(2.1,0,2.5),(0,90,0)),
                                          ('stop',(-3.3,0,-4.3),(0,0,0))])
        self.scene('HQCheckpointPost','CheckpointPost',[('bagBunker',(0,0,0.3),(0,270,0)),('chair',(-1.6,0,-2.6),(0,200,0)),
                                                        ('signWarning',(-1.2,0,2.9),(0,0,0))])
        self.scene('HQCheckpointNest','CheckpointNest',[('bagRound',(0,-BAG_SINK,0),(0,90,0)),('chair',(1.0,0,-1.6),(0,160,0)),
                                                        ('bagEnd',(0.2,-BAG_SINK,2.4),(0,0,0))])
        # Parade square in front of the command building.
        self.scene('HQSquare','Square',[('flag',(0,0,0),(0,0,0)),('signCommand',(1.4,0,-0.8),(0,0,0)),
                                        ('bagRound',(-2.2,-BAG_SINK,1.2),(0,30,0))])
        self.scene('HQRoadPlates','Road_Plates',self.plates(4))
        self.scene('HQRoadPlatesShort','Road_PlatesShort',self.plates(2))
        self.scene('HQDuckboards','Path_Duckboards',[('duckboard',(0,0,-1.9),(0,0,0)),('duckboard',(0,0,1.9),(0,180,0))])
        self.scene('HQDuckboard','Path_Duckboard',[('duckboard',(0,0,0),(0,0,0))])
        self.scene('HQPlateStack','PlateStack',[('plateStack',(0,0,0),(0,0,0)),('plateA',(0,-PLATE_SINK,2.3),(0,8,0))])
        self.decals('HQDirtPatch','Dirt_Patch',[('decalPatch4',(0,0),5)],2.5)
        self.decals('HQDirtWorn','Dirt_Worn',[('decalPatch2',(0,-2.6),4),('decalPatch6',(0.7,1.0),4.5),('decalSteps',(-0.5,3.4),2.5)],4)
        self.decals('HQDirtTrack','Dirt_Track',[('decalPatch1',(0,-3.2),4.5),('decalPatch3',(0.3,0),4.5),('decalPatch4',(-0.2,3.2),4.5)],4)
        self.decals('HQDirtCross','Dirt_Cross',[('decalCross',(0,0),6)],3)
        for key,(prefab,hw,hd,expanded) in VIGNETTES.items():
            self.vignette(key,prefab,hw,hd,expanded)
        self.outputs['docs/headquarters-catalog.json']=json.dumps(self.catalog,indent=2)+'\n'
        calls='\n'.join(f'\t\tMeasure(world, "{k}", "{v["prefab"]}");' for k,v in self.catalog.items() if 'measure' not in v)
        calls+='\n'+'\n'.join(f'\t\tLoad(world, "{k}", "{v["prefab"]}");' for k,v in self.catalog.items() if 'measure' in v)
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
        self.modules=[]; self.walls=[]; self.belt=[]; self.dressing=[]; self.shelters=[]
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
            if any(overlap(a,self.dbox(d)) for d in self.solid()):
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
            if any(overlap(a,self.dbox(d)) for d in self.solid()):
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

    def concrete(self,side,i):
        slots=self.slots[side]
        if i<CONCRETE_SLOTS or i>=len(slots)-CONCRETE_SLOTS:
            return True
        # Concrete shoulders either side of each casemate read as a bastion.
        if any(isinstance(slots[j],str) and slots[j].startswith('HQCasemate') for j in (i-1,i+1) if 0<=j<len(slots)):
            return True
        return side==2 and any(slots[j]=='gate' for j in range(max(0,i-CONCRETE_SLOTS),min(len(slots),i+CONCRETE_SLOTS+1)))

    def build_walls(self):
        """Concrete corners and gate shoulders; Game Master camo sandbag
        runs everywhere else, so the ring reads as a dug-in permanent camp."""
        mix=['HQWallA','HQWallB','HQWallA','HQWallC'] if self.variant%3!=1 else ['HQWallB','HQWallA','HQWallC','HQWallA']
        camo=['HQCamoWallA','HQCamoWallB','HQCamoWallA','HQCamoWallC'] if self.variant%2 else ['HQCamoWallA','HQCamoWallC','HQCamoWallA','HQCamoWallB']
        for side in range(4):
            horizontal,fixed,half=side_axis(side,self.Wl,self.Dl)
            slots=self.slots[side]
            i=0; count=0
            while i<len(slots):
                if slots[i] is not None:
                    i+=1; continue
                hard=self.concrete(side,i)
                pair=i+1<len(slots) and slots[i+1] is None and self.concrete(side,i+1)==hard
                width=2 if pair else 1
                if (not hard and pair and self.size<2 and i+2<len(slots) and slots[i+2] is None
                        and not self.concrete(side,i+2)):
                    width=3
                along=-half+(i+width/2)*PANEL
                x,z=(along,fixed) if horizontal else (fixed,along)
                if hard:
                    key=mix[(count+self.variant)%len(mix)] if pair else 'HQWallPanel'
                elif width==3:
                    key=['HQCamoWallLongA','HQCamoWallLongB'][(count+self.variant)%2]
                else:
                    key=camo[(count+self.variant)%len(camo)] if pair else 'HQCamoWallPanel'
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
                    gate=(side==2 and abs(t)<10) or (side in (1,3) and -17<t<1)
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
        if any(overlap(a,self.dbox(d)) for d in self.solid()):
            return False
        if any(overlap(a,lane,1) for lane in self.lanes):
            return False
        return not overlap(a,(self.capture[0]-.5,self.capture[2]-.5,self.capture[0]+.5,self.capture[2]+.5),0)

    # -- dressing ----------------------------------------------------------
    def solid(self):
        return [d for d in self.dressing if d['kind'] in SOLID]

    def dbox(self,d):
        a=mesh_box(d,self.measure)
        if d['kind']!='gate':
            return a
        # Gate wings, signs and chicanes stand outside the wall; inside,
        # only the road apron across the opening reserves ground.
        lim_x,lim_z=self.limits()
        x,z=d['position'][0],d['position'][2]
        a=(max(a[0],-lim_x,x-4),max(a[1],-lim_z,z-4),min(a[2],lim_x,x+4),min(a[3],lim_z,z+4))
        if a[0]>=a[2] or a[1]>=a[3]:
            return (1e6,1e6,1e6,1e6)
        return a

    def room(self,key,reserve=0):
        return (self.roots()+1+reserve<=ROOT_LIMIT and
                self.expanded()+cost(self.measure[key])+self.guns*12<=RECIPE_EXPANDED_BUDGET)

    def gun_conflict(self,item):
        """Solid dressing stays off each gun's crew box, the same rectangle as
        IA_StaticGunRecord.ContainsReservedPoint (x +-1.5, z -4..1.25), grown
        0.5 m for the socket offset. Runtime trusts this (m_bPlannedClearance)
        instead of its coarse bounding-circle test."""
        if item['kind']=='decal':
            return False
        # Gate wings sit a full slot clear of any face casemate (casemate()).
        a=self.dbox(item)
        for g in self.modules:
            if not self.catalog[g['key']]['sockets']:
                continue
            reserve=mesh_box({'key':None,'position':g['position'],'yaw':g['yaw']},{None:{'mins':[-2,0,-4.5],'maxs':[2,0,1.75]}})
            if overlap(a,reserve):
                return True
        return False

    def dress_item(self,key,x,z,yaw,kind):
        hw,hd=half_extent(self.measure[key])
        return {'key':key,'position':[round(x,3),0,round(z,3)],'yaw':yaw%360,'kind':kind,'residual':RESIDUAL[kind],
                'half_width':round(hw,4),'half_depth':round(hd,4),'expanded':cost(self.measure[key])}

    def dress_fits(self,item,lane_ok=False,apron_ok=False,extra=()):
        a=self.dbox(item)
        kind=item['kind']
        if self.gun_conflict(item):
            return False
        if kind=='gate':
            return True
        if not self.inside(a):
            return False
        if kind=='decal':
            return True
        if any(overlap(a,self.bare(o),0.6) for o in self.modules):
            return False
        if any(overlap(a,shelter_box(s),0.6) for s in self.shelters):
            return False
        # Road segments butt together; everything else keeps a walking gap.
        if any(overlap(a,self.dbox(d),0.3) for d in self.solid()+list(extra) if kind!='road' or d['kind']!='road'):
            return False
        if kind=='road':
            return True
        if not lane_ok and any(overlap(a,lane,0.3) for lane in self.lanes):
            return False
        if not apron_ok and any(overlap(a,ap) for o in self.modules for ap,_ in self.aprons(o)):
            return False
        if not apron_ok and any(overlap(a,shelter_entrance(s)) for s in self.shelters):
            return False
        return not overlap(a,(self.capture[0]-1.5,self.capture[2]-1.5,self.capture[0]+1.5,self.capture[2]+1.5))

    def dress(self,key,x,z,yaw,kind,lane_ok=False,apron_ok=False,owner=None):
        if not self.room(key):
            return False
        item=self.dress_item(key,x,z,yaw,kind)
        if not self.dress_fits(item,lane_ok,apron_ok):
            return False
        if owner is not None:
            item['owner']=owner
        self.dressing.append(item)
        return True

    def opening(self,side):
        horizontal,fixed,half=side_axis(side,self.Wl,self.Dl)
        gates=[i for i,s in enumerate(self.slots[side]) if s=='gate']
        a,b=-half+gates[0]*PANEL,-half+(gates[-1]+1)*PANEL
        return fixed,(a+b)/2,(b-a)/2

    def build_gates(self):
        """Formal concrete bar gate on the front, camo sandbag side entries."""
        _,_,edge=self.opening(2)
        key='HQGateWide' if edge>2.25*PANEL else 'HQGateNarrow'
        assert self.dress(key,0,-self.Dl,180,'gate'),(self.name,self.variant,'gate')
        for side,yaw in ((1,90),(3,270)):
            fixed,centre,edge=self.opening(side)
            key='HQSideGateWide' if edge>2.25*PANEL else 'HQSideGateNarrow'
            self.dress(key,fixed,centre,yaw,'gate')

    def build_checkpoint(self,mirror):
        """Inner vehicle checkpoint across the lane, far enough in that
        the gate guard box and throat overwatch keep their ground."""
        post_w,_=half_extent(self.measure['HQCheckpointPost'])
        nest_w,_=half_extent(self.measure['HQCheckpointNest'])
        yaw=0 if mirror>0 else 180
        for step in range(16):
            z=-self.Dl+11+step
            if z>self.capture[2]-14 or not self.room('HQChicane',2):
                return
            parts=[self.dress_item('HQChicane',0,z,0,'mesh'),
                   self.dress_item('HQCheckpointPost',mirror*(LANE_HALF+post_w+0.4),z,yaw,'mesh'),
                   self.dress_item('HQCheckpointNest',-mirror*(LANE_HALF+nest_w+0.4),z,yaw,'mesh')]
            if all(self.dress_fits(p,lane_ok=(i==0),extra=parts[:i]) for i,p in enumerate(parts)):
                self.dressing+=parts
                return

    def build_square(self,mirror):
        """Flag and command sign beside the capture point, off the lane."""
        hw,hd=half_extent(self.measure['HQSquare'])
        for s in (mirror,-mirror):
            for dz in (0,-2,-4,-6,2):
                if self.dress('HQSquare',s*(LANE_HALF+hw+0.8),self.capture[2]-hd-1+dz,0,'mesh'):
                    return

    def build_roads(self):
        """Concrete road plates from the gate apron to the command building;
        worn dirt where the chicane stands so the knife rests keep clear."""
        z=-self.Dl+PLATE_W
        end=self.capture[2]-1
        chicanes=[d for d in self.dressing if d['key']=='HQChicane']
        while end-z>=PLATE_D:
            hit=next((c for c in chicanes if self.dbox(c)[1]-0.3<z+2*PLATE_W and self.dbox(c)[3]+0.3>z),None)
            if hit:
                a=self.dbox(hit)
                if z+PLATE_W<=a[1]-0.3 and self.dress('HQRoadPlatesShort',0,z+PLATE_D,0,'road'):
                    z+=PLATE_W
                    continue
                self.dress('HQDirtTrack',0,hit['position'][2],0,'decal')
                chicanes.remove(hit)
                z=a[3]+0.3
                continue
            key='HQRoadPlates' if end-z>=2*PLATE_W else 'HQRoadPlatesShort'
            length=2*PLATE_W if key=='HQRoadPlates' else PLATE_W
            if not self.dress(key,0,z+length/2,0,'road'):
                break
            z+=length
        self.dress('HQDirtCross',0,min(z,end),0,'decal')

    def build_paths(self):
        """Worn ground and duckboards out of each main door."""
        for o in list(self.modules):
            aprons=self.aprons(o)
            if not aprons:
                continue
            if not self.room('HQDirtWorn',4):
                return
            a,(wx,wz)=aprons[0]
            cx,cz=(a[0]+a[2])/2,(a[1]+a[3])/2
            yaw=0 if wz else 90
            self.dress('HQDirtPatch' if o['key'] in ('HQTower','HQPillbox') else 'HQDirtWorn',cx+wx,cz+wz,yaw,'decal')
            # Boards start at the door face and run outward along its normal.
            ex=a[0] if wx>0 else a[2] if wx<0 else cx
            ez=a[1] if wz>0 else a[3] if wz<0 else cz
            for key,length in (('HQDuckboards',7.6),('HQDuckboard',3.8)):
                if self.dress(key,ex+wx*(0.7+length/2),ez+wz*(0.7+length/2),yaw,'mesh',apron_ok=True):
                    break

    def build_vignettes(self,per_building):
        """Lived-in scenes tucked against each building's flanks."""
        for index,o in enumerate(list(self.modules)):
            have=[d['key'] for d in self.dressing if d.get('owner')==index]
            for key in VIGNETTES_BY.get(o['key'],[]):
                if len(have)>=per_building or not self.room(key,4):
                    break
                if key not in have and self.beside(index,o,key):
                    have.append(key)

    def beside(self,index,o,key):
        b=self.bare(o)
        hw,hd=half_extent(self.measure[key])
        mx,mz=(b[0]+b[2])/2,(b[1]+b[3])/2
        best=None
        for yaw in (0,90):
            w,d=(hw,hd) if yaw==0 else (hd,hw)
            cands=[]
            for t in [i*1.5 for i in range(-12,13)]:
                if b[1]+d-3<=mz+t<=b[3]-d+3:
                    cands+=[(b[0]-1.0-w,mz+t),(b[2]+1.0+w,mz+t)]
                if b[0]+w-3<=mx+t<=b[2]-w+3:
                    cands+=[(mx+t,b[1]-1.0-d),(mx+t,b[3]+1.0+d)]
            for x,z in cands:
                item=self.dress_item(key,x,z,yaw,'mesh')
                if not self.dress_fits(item):
                    continue
                score=math.dist((x,z),(mx,mz))
                if best is None or score<best[0]-1e-6:
                    best=(score,item)
        if not best:
            return False
        best[1]['owner']=index
        self.dressing.append(best[1])
        return True

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
        self.build_gates()
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
        gate_edge=max(-self.Wl+(i+1)*PANEL for i,s in enumerate(self.slots[2]) if s=='gate')
        hw,_=half_extent(self.measure['HQGuardBox'])
        _,z=self.snug('HQGuardBox',0,0,-1)
        for gx in (mirror*(gate_edge+hw+1.5),mirror*(gate_edge+hw+4.5)):
            if self.insert('HQGuardBox',gx,z,0,'Cover',required=False,ignore_lane=True):
                break
        self.build_checkpoint(mirror)
        self.build_square(mirror)
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
        self.build_shelters()
        self.build_walls()
        self.build_belt()
        self.build_roads()
        self.build_vignettes(1)
        self.build_paths()
        self.build_vignettes(99)
        self.check()
        return self.result()

    def check(self):
        guns_by_side={m['side'] for m in self.modules if self.catalog[m['key']]['sockets'] and m['side']>=0}
        assert guns_by_side=={0,1,2,3},(self.name,self.variant,guns_by_side)
        assert self.guns>=4 and self.guns<=CAPS[self.size]
        roots=self.roots()
        assert roots<=ROOT_LIMIT,(self.name,self.variant,'roots',roots)
        assert self.expanded()+self.guns*12<=RECIPE_EXPANDED_BUDGET,(self.name,self.variant,'expanded')
        assert len(self.shelters)==SHELTERS[self.size],(self.name,self.variant,'shelters',len(self.shelters))
        posts=self.posts()
        assert len(posts)>=(16 if self.size<2 else 6),(self.name,self.variant,'posts',len(posts))
        self._posts=posts

    def expanded(self):
        return sum(m['expanded'] for m in self.modules+self.walls+self.belt+self.dressing)+len(self.shelters)*SHELTER_EXPANDED

    def roots(self):
        return len(self.modules)+len(self.walls)+len(self.belt)+len(self.dressing)+len(self.shelters)

    def entries(self):
        """Fixed guard posts at the three gates."""
        return [[0,0,-self.Dl+5],[-self.Wl+5,0,-8],[self.Wl-5,0,-8]]

    def build_shelters(self):
        """Air-raid bunkers inside the capture circle. Planned after the
        buildings and before the dressing so vignettes and paths work around
        them; they stay off doors, the lane, the gate throats and gun crews."""
        lim_x,lim_z=self.limits()
        keep_out=[(box(m),1) for m in self.modules]
        keep_out+=[(a,1) for m in self.modules for a,_ in self.aprons(m)]
        keep_out+=[(self.dbox(d),1) for d in self.solid()]
        keep_out+=[(lane,1) for lane in self.lanes]
        keep_out.append((square(self.capture,2.5),0))
        keep_out+=[(r,0) for r in gun_reserves(self.modules,self.catalog)]
        keep_out+=[((-self.Wl,-13,-self.Wl+12,-3),0),((self.Wl-12,-13,self.Wl,-3),0)]
        keep_out+=[(square(p,1.5),1) for p in self.entries()]
        entrance_keep_out=[(self.bare(m),0) for m in self.modules]+[(self.dbox(d),0) for d in self.solid()]
        count=SHELTERS[self.size]
        while count and (self.roots()+count>ROOT_LIMIT or
                         self.expanded()+count*SHELTER_EXPANDED+self.guns*12>RECIPE_EXPANDED_BUDGET):
            count-=1
        self.shelters=plan_shelters(count,self.capture,min(self.W,self.D),lim_x-1,lim_z-1,keep_out,entrance_keep_out)

    def posts(self):
        posts=[]
        lim_x=int(self.Wl)-4; lim_z=int(self.Dl)-4
        points=[self.capture]+self.entries()
        tail=[[x,0,z] for z in range(-lim_z,lim_z+1,4) for x in range(-lim_x,lim_x+1,4)]
        self.rng.shuffle(tail)
        for p in points+tail:
            a=(p[0]-1.5,p[2]-1.5,p[0]+1.5,p[2]+1.5)
            if any(overlap(a,box(m),1) for m in self.modules):
                continue
            if any(overlap(a,self.dbox(d),0.5) for d in self.solid()):
                continue
            if any(overlap(a,shelter_box(s),1) or overlap(a,shelter_entrance(s),1) for s in self.shelters):
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
                'walls':self.walls,'belt':self.belt,'dressing':self.dressing,'shelters':self.shelters,'posts':self._posts,'guns':self.guns,'heavy':self.heavy,
                'expanded':self.expanded(),'roots':self.roots()}


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
        # Bunkers before the belt: the first dressing starts the gun phase.
        for s in r['shelters']:
            lines += [f'\t\tlayout.AddAirRaidBunker({vec(s["position"])}, {s["yaw"]});']
        for b in r['belt']:
            lines += [f'\t\tlayout.AddObstacle("{b["key"]}", {vec(b["position"])}, {b["yaw"]});']
        for d in r['dressing']:
            lines += [f'\t\tlayout.AddDressingItem("{d["key"]}", {vec(d["position"])}, {d["yaw"]}, {d["residual"]});']
        for p in r['posts']:
            lines += [f'\t\tlayout.m_aGuardPosts.Insert({vec(p)});']
        lines += ['\t}']
    return '\n'.join(lines+['}',''])


MEASURE_CACHE={}


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
            fill='#7d8a4a' if 'Camo' in w['key'] else '#c8c8c0'
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="{fill}"/>']
        tones={'road':'#9a9a92','decal':'#7a5a34','gate':'#d8d8cf','mesh':'#2f8f86'}
        for x in sorted(r['dressing'],key=lambda x:x['kind']!='decal'):
            a,bb,c,d=mesh_box(x,MEASURE_CACHE)
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="{tones[x["kind"]]}" fill-opacity="{0.45 if x["kind"]=="decal" else 0.9}"/>']
        for s in r['shelters']:
            a,bb,c,d=shelter_box(s)
            rows += [f'<rect x="{cx+a*scale:.2f}" y="{cy-d*scale:.2f}" width="{(c-a)*scale:.2f}" height="{(d-bb)*scale:.2f}" fill="#55642f" stroke="#ffcf73"/>']
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
    # Decal scenes and reused vignettes carry authored footprints.
    for key,entry in hq_catalog.items():
        if 'measure' in entry:
            hq_measure[key]=entry['measure']
    catalog={**base_catalog,**hq_catalog}
    measure={**base_measure,**hq_measure}
    return hq_catalog,hq_measure,catalog,measure


def generate_designs():
    hq_catalog,hq_measure,catalog,measure=load_inputs()
    MEASURE_CACHE.update(measure)
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
