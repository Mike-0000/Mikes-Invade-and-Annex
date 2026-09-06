"""Author I&A scenery shells and gun sockets from vanilla compositions.

No engine scripts are copied. Only authored entity placement data is retained;
stock physical prefabs supply their models/components. Campaign/service/AI roots
are deliberately not inherited. Run with data007 and optionally --check.
"""
from pathlib import Path
from functools import lru_cache
import argparse
import copy
import json
import re
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUT = 'Prefabs/BaseCompositions/'
SLOTTED = 'Prefabs/Compositions/Slotted/'
SUB = 'Prefabs/Compositions/Misc/SubCompositions/'
SOURCES = {
    'Headquarters': SLOTTED+'SlotFlatSmall/Headquarters_S_USSR_01.et',
    'LivingSmall': SLOTTED+'SlotFlatSmall/LivingArea_S_USSR_01.et',
    'LivingLarge': SLOTTED+'SlotFlatLarge/LivingArea_L_USSR_01.et',
    'Hospital': SLOTTED+'SlotFlatMedium/FieldHospital_M_USSR_01.et',
    'Medical': SUB+'Tents/Tent_Medical_USSR_01.et',
    'MaintenanceSmall': SLOTTED+'SlotFlatSmall/VehicleMaintenance_S_USSR_01.et',
    'MaintenanceLarge': SLOTTED+'SlotFlatMedium/VehicleMaintenance_M_USSR_01.et',
    'Supply': SLOTTED+'SlotFlatSmall/SupplyStorage_S_USSR_01.et',
    'Ammo': SLOTTED+'SlotFlatSmall/AmmoStorage_S_USSR_01.et',
    'Fuel': SLOTTED+'SlotFlatSmall/FuelStorage_S_USSR_01.et',
    'Tower': SLOTTED+'SlotFlatSmall/GuardTower_S_USSR_01.et',
    'Bunker': SLOTTED+'SlotFlatSmall/Bunker_S_USSR_01.et',
    'PKMNest': SLOTTED+'SlotFlatSmall/MachineGunNest_S_USSR_01.et',
    'NSVNest': SLOTTED+'SlotFlatSmall/MachineGunNest_S_USSR_02.et',
    'ScopedNest': SLOTTED+'SlotFlatSmall/MachineGunNest_Scoped_S_USSR_01.et',
    'PKM': SUB+'Fortifications/Sandbag_MG_USSR_01_PKM.et',
    'NSV': SUB+'Fortifications/Sandbag_MG_USSR_01_NSV.et',
    'AA': 'Prefabs/Compositions/Misc/FreeRoamBuilding/Emplacement_AA_MG_USSR_NSV_SPP.et',
    **{f'Position{i}': SLOTTED+f'SlotFlatSmall/SandbagPosition_S_USSR_0{i}.et' for i in range(1,5)},
    **{f'Checkpoint{s}': SLOTTED+f'SlotRoad{size}/Checkpoint_{s}_USSR_01.et' for s,size in [('S','Small'),('M','Medium'),('L','Large')]},
    **{f'Barricade{s}': SLOTTED+f'SlotRoad{size}/Barricade_{s}_USSR_01.et' for s,size in [('S','Small'),('M','Medium'),('L','Large')]},
}
REF = re.compile(r'"\{[0-9A-F]+\}([^"\n]+\.et)"')


class Node:
    def __init__(self, head='', body=None):
        self.head, self.body = head, body or []

    def block(self, name):
        return next((x for x in self.body if isinstance(x,Node) and x.head == name), None)

    def prop(self, name, default=''):
        return next((x[len(name)+1:] for x in self.body if isinstance(x,str) and x.startswith(name+' ')), default)

    def render(self, indent=0):
        pad = ' '*indent
        return pad+self.head+' {\n'+''.join(x.render(indent+1) if isinstance(x,Node) else pad+' '+x+'\n' for x in self.body)+pad+'}\n'


def parse(text):
    stack = [Node()]
    for line in text.splitlines():
        line=line.strip()
        if not line or line.startswith('//'):
            continue
        if line.endswith('{'):
            node=Node(line[:-1].strip())
            stack[-1].body.append(node)
            stack.append(node)
        elif line == '}':
            stack.pop()
        else:
            stack[-1].body.append(line)
    assert len(stack)==1
    return stack[0].body[0]


def guid(path):
    return uuid.uuid5(uuid.NAMESPACE_URL,'ia-composition-v2/'+path).hex[:16].upper()


def resource(path):
    return '{'+guid(path)+'}'+path


def metadata(path):
    name=Path(path).name
    lines=['MetaFileClass {',f' Name "{{{guid(path)}}}{name}"',' Configurations {']
    for platform in ['PC','XBOX_ONE','XBOX_SERIES','PS4','PS5','HEADLESS']:
        suffix='' if platform=='PC' else ' : PC'
        lines += [f'  EntityTemplateResourceClass {platform}{suffix} {{','  }']
    return '\n'.join(lines+[' }','}',''])


class Author:
    def __init__(self, base):
        self.base=Path(base)
        self.outputs={}
        self.cache={}

    @lru_cache(None)
    def load(self,path):
        return parse((self.base/path).read_text(encoding='utf-8-sig'))

    def chain(self,path):
        node=self.load(path)
        match=REF.search(node.head)
        return ([node]+self.chain(match[1])) if match else [node]

    def component_id(self,path,kind):
        for node in self.chain(path):
            components=node.block('components')
            if components:
                for c in components.body:
                    if isinstance(c,Node) and c.head.startswith(kind+' '):
                        return re.search(r'"(\{[A-F0-9]+\})"',c.head)[1]
        return '{'+guid(path+'/'+kind)+'}'

    @staticmethod
    def excluded(path):
        return any(s in path for s in ['/Systems/','/Arsenal/','/MP/','/Garbage/','/CompositionDecals/','/Vegetation/','/Generators/']) or 'Service' in Path(path).stem

    @staticmethod
    def composition(path):
        return path.startswith('Prefabs/Compositions/') and '/CustomEntities/' not in path

    @staticmethod
    def placements(children):
        result=[]
        for child in children:
            if not isinstance(child,Node):
                continue
            if child.head.startswith('$grp '):
                for entry in child.body:
                    if isinstance(entry,Node):
                        result.append(Node(child.head[5:], copy.deepcopy(entry.body)))
            else:
                result.append(copy.deepcopy(child))
        return result

    def children(self,path):
        chain=list(reversed(self.chain(path)))
        children=[]
        for node in chain:
            b=node.block('')
            if not b:
                continue
            for entry in self.placements(b.body):
                ident=entry.prop('ID')
                previous=next((x for x in children if ident and x.prop('ID')==ident),None)
                if previous:
                    if REF.search(entry.head):
                        previous.head=entry.head
                    for field in entry.body:
                        if isinstance(field,str):
                            key=field.split(' ',1)[0]
                            previous.body=[x for x in previous.body if not(isinstance(x,str) and x.startswith(key+' '))]
                            previous.body.append(field)
                        elif field.head=='':
                            previous.body.append(field)
                else:
                    children.append(entry)
        return children

    @lru_cache(None)
    def needs_clean(self,path):
        for node in self.chain(path):
            components=node.block('components')
            if components and re.search(r'SupportStation|ServicePoint|ResourceComponent|ScriptedRadio|Campaign|EntitySpawner|MilitaryBase',components.render()):
                return True
        return any(self.needs_clean(m[1]) for child in self.children(path) if (m:=REF.search(child.head)))

    @staticmethod
    def merge(base,extra):
        result=copy.deepcopy(base)
        for field in extra.body:
            if isinstance(field,str):
                key=field.split(' ',1)[0]
                result.body=[x for x in result.body if not(isinstance(x,str) and x.split(' ',1)[0]==key)]
                result.body.append(field)
            else:
                old=next((x for x in result.body if isinstance(x,Node) and x.head==field.head),None)
                if old:
                    result.body[result.body.index(old)]=Author.merge(old,field)
                else:
                    result.body.append(copy.deepcopy(field))
        return result

    def author(self,path):
        if path in self.cache:
            return self.cache[path]
        out=OUT+'IA_'+Path(path).stem+'_'+guid(path)[:6]+'.et'
        sockets=[]
        body=[]
        physical=None
        mesh_only=not self.composition(path) and self.needs_clean(path)
        for ancestor in self.chain(path):
            match=REF.search(ancestor.head)
            if not mesh_only and match and not self.composition(match[1]):
                physical=match[0]
                break
        for child in self.children(path):
            match=REF.search(child.head)
            if not match:
                continue
            target=match[1]
            if self.excluded(target):
                continue
            pose={'position':[float(n) for n in child.prop('coords','0 0 0').split()],
                  'angles':[float(n) for n in child.prop('angles','0 0 0').split()]}
            if '/Weapons/' in target:
                if '/Tripods/' in target:
                    kind=0 if 'PKM' in target else 1
                    if 'SPP' in target:
                        kind=2
                    sockets.append({'kind':kind,'chain':[pose]})
                continue
            adapted=self.composition(target) or self.needs_clean(target)
            if adapted:
                sub,sock=self.author(target)
                child.head=child.head.replace(match[0],'"'+resource(sub)+'"')
                for s in sock:
                    sockets.append({'kind':s['kind'],'chain':[pose]+s['chain']})
            # Preserve placements, not campaign/editor/replication overrides.
            child.body=[x for x in child.body if isinstance(x,str) and x.split(' ',1)[0] in ('ID','coords','angles','scale')]
            hierarchy=self.component_id(target,'Hierarchy')
            if adapted:
                generated=parse(self.outputs[sub])
                hierarchy=next(c.head.split(' ',1)[1].strip('"') for c in generated.block('components').body if c.head.startswith('Hierarchy '))
            child.body.append(Node('components',[Node('Hierarchy "'+hierarchy+'"',['Enabled 1'])]))
            body.append(child)
        if path==SOURCES['AA']:
            for socket in sockets:
                socket['kind']=3
        head='GenericEntity'
        if physical:
            head=self.load(path).head.split(' : ',1)[0]+' : '+physical
        root=Node(head)
        rpl_id='{'+guid(out+'/rpl')+'}'
        hierarchy_id='{'+guid(out+'/hierarchy')+'}'
        if physical:
            parent=REF.search(physical)[1]
            rpl_id=self.component_id(parent,'RplComponent')
            hierarchy_id=self.component_id(parent,'Hierarchy')
        root.body=['ID "'+guid(out) +'"',Node('components',[
            Node('RplComponent "'+rpl_id+'"'),Node('Hierarchy "'+hierarchy_id+'"',['Enabled 1'])]),'coords 0 0 0']
        if mesh_only:
            physical_components={}
            for ancestor in reversed(self.chain(path)):
                components=ancestor.block('components')
                if not components:
                    continue
                for c in components.body:
                    if isinstance(c,Node) and c.head.split(' ',1)[0] in ('MeshObject','RigidBody'):
                        kind=c.head.split(' ',1)[0]
                        if kind in physical_components:
                            physical_components[kind]=self.merge(physical_components[kind],c)
                        else:
                            physical_components[kind]=copy.deepcopy(c)
            root.block('components').body.extend(physical_components.values())
        if body:
            root.body.append(Node('',body))
        self.outputs[out]=root.render()
        self.outputs[out+'.meta']=metadata(out)
        self.cache[path]=(out,sockets)
        return out,sockets

    def generate(self):
        catalog={}
        for key,path in SOURCES.items():
            out,sockets=self.author(path)
            catalog[key]={'source':path,'prefab':resource(out),'sockets':sockets}
        self.outputs['docs/base-composition-catalog.json']=json.dumps(catalog,indent=2)+'\n'
        calls='\n'.join(f'        Measure(world, "{key}", "{entry["prefab"]}");' for key,entry in catalog.items())
        self.outputs['Scripts/WorkbenchGame/IA_BaseCompositionProbe.c']='''#ifdef WORKBENCH
// Generated by tools/author_base_compositions.py. Preview geometry only.
[WorkbenchPluginAttribute(name: "IA composition measurement", wbModules: {"ResourceManager"})]
class IA_BaseCompositionProbe : IA_BaseFoundationProbe
{
    protected BaseWorld m_World;
    override void RunCommandline()
    {
        ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IACompositionProbe");
        BaseWorld world = preview.GetRef();
        m_World = world;
'''+calls+'''
        Print(string.Format("[IA][CompositionMeasure] failures=%1", m_iFailures), LogLevel.NORMAL);
        Workbench.Exit(m_iFailures);
    }

    protected void MeasureMeshes(IEntity entity, inout vector low, inout vector high)
    {
        if (!entity)
            return;
        if (entity.GetVObject())
        {
            // Entity bounds may include nonphysical volumes. Isolate meshes;
            // generated service-area meshes must also be removed at authoring.
            IEntity mesh = GetGame().SpawnEntity(GenericEntity, m_World);
            mesh.SetObject(entity.GetVObject(), "");
            vector mins, maxs, mat[4];
            mesh.GetBounds(mins, maxs);
            delete mesh;
            entity.GetWorldTransform(mat);
            for (int i = 0; i < 8; i++)
            {
                vector corner = mins;
                for (int axis = 0; axis < 3; axis++)
                {
                    if (i & (1 << axis))
                        corner[axis] = maxs[axis];
                }
                vector p = mat[3] + mat[0]*corner[0] + mat[1]*corner[1] + mat[2]*corner[2];
                for (int axis = 0; axis < 3; axis++)
                {
                    low[axis] = Math.Min(low[axis], p[axis]);
                    high[axis] = Math.Max(high[axis], p[axis]);
                }
            }
        }
        IEntity child = entity.GetChildren();
        while (child)
        {
            MeasureMeshes(child, low, high);
            child = child.GetSibling();
        }
    }

    protected void Measure(BaseWorld world, string key, ResourceName name)
    {
        ref EntitySpawnParams params = new EntitySpawnParams();
        params.TransformMode = ETransformMode.WORLD;
        Math3D.MatrixIdentity4(params.Transform);
        IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(name), world, params);
        Check(entity != null, "composition resource " + key);
        if (!entity)
            return;
        vector low = "99999 99999 99999";
        vector high = "-99999 -99999 -99999";
        MeasureMeshes(entity, low, high);
        Check(low[0] < high[0] && low[2] < high[2], "physical composition " + key);
        Print(string.Format("[IA][CompositionMeasure] key=%1 count=%2 min=%3 max=%4", key, IA_EmplacementBuilder.CountHardware(entity), low, high), LogLevel.NORMAL);
        SCR_EntityHelper.DeleteEntityAndChildren(entity);
    }
}
#endif
'''
        return self.outputs


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('data')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    outputs=Author(args.data).generate()
    for path,content in outputs.items():
        target=ROOT/path
        if args.check:
            assert target.read_text(encoding='utf-8')==content,path
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(content,encoding='utf-8')
    print(f'{len(SOURCES)} catalog entries; {len(outputs)} generated files')


if __name__=='__main__':
    main()
