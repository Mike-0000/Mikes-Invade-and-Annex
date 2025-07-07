// IA_SideObjectiveMarker.c

class IA_SideObjectiveMarkerClass: ScriptedGameTriggerEntityClass
{
};

class IA_SideObjectiveMarker : ScriptedGameTriggerEntity
{
    [Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Type of side objective for this location", enums: ParamEnumArray.FromEnum(IA_SideObjectiveType))]
    private IA_SideObjectiveType m_ObjectiveType;

    [Attribute(desc: "Generator spawn position offset (for Assassination objectives). If set, a generator will be spawned that must be destroyed to prevent reinforcements.")]
    private ref PointInfo m_GeneratorPositionOffset;

    private static ref array<IA_SideObjectiveMarker> s_AllMarkers = new array<IA_SideObjectiveMarker>();

    void IA_SideObjectiveMarker(IEntitySource src, IEntity parent)
    {
        SetEventMask(EntityEvent.INIT);
    }

    override void EOnInit(IEntity owner)
    {
        if (Replication.IsServer())
        {
            if (!s_AllMarkers.Contains(this))
            {
                s_AllMarkers.Insert(this);
            }
        }
    }

    static array<IA_SideObjectiveMarker> GetAllMarkers()
    {
        return s_AllMarkers;
    }
    
    IA_SideObjectiveType GetObjectiveType()
    {
        return m_ObjectiveType;
    }
    
    PointInfo GetGeneratorPositionOffset()
    {
        return m_GeneratorPositionOffset;
    }
} 