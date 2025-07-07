// IA_HVTSpawnPositionMarker.c

class IA_HVTSpawnPositionMarkerClass: ScriptedGameTriggerEntityClass
{
};

class IA_HVTSpawnPositionMarker : ScriptedGameTriggerEntity
{
    private static ref array<IA_HVTSpawnPositionMarker> s_AllMarkers = new array<IA_HVTSpawnPositionMarker>();

    void IA_HVTSpawnPositionMarker(IEntitySource src, IEntity parent)
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

    static array<IA_HVTSpawnPositionMarker> GetAllMarkers()
    {
        return s_AllMarkers;
    }
} 