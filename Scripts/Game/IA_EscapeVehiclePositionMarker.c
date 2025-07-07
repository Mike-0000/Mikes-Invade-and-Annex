// IA_EscapeVehiclePositionMarker.c

class IA_EscapeVehiclePositionMarkerClass: ScriptedGameTriggerEntityClass
{
};

class IA_EscapeVehiclePositionMarker : ScriptedGameTriggerEntity
{
    private static ref array<IA_EscapeVehiclePositionMarker> s_AllMarkers = new array<IA_EscapeVehiclePositionMarker>();

    void IA_EscapeVehiclePositionMarker(IEntitySource src, IEntity parent)
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

    static array<IA_EscapeVehiclePositionMarker> GetAllMarkers()
    {
        return s_AllMarkers;
    }
} 