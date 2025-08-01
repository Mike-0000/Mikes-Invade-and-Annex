// IA_AISpawnPoint.c

class IA_AISpawnPointClass: GenericEntityClass
{
};

class IA_AISpawnPoint : GenericEntity
{
    private static ref array<IA_AISpawnPoint> s_AllSpawnPoints = new array<IA_AISpawnPoint>();

    void IA_AISpawnPoint(IEntitySource src, IEntity parent)
    {
        SetEventMask(EntityEvent.INIT);
    }

    override void EOnInit(IEntity owner)
    {
        if (Replication.IsServer())
        {
            if (!s_AllSpawnPoints.Contains(this))
            {
                s_AllSpawnPoints.Insert(this);
                Print(string.Format("[IA_AISpawnPoint] Registered spawn point %1 at %2. Total spawn points: %3", this, GetOrigin(), s_AllSpawnPoints.Count()), LogLevel.NORMAL);
            }
        }
    }

    static array<IA_AISpawnPoint> GetAllSpawnPoints()
    {
        int initialCount = s_AllSpawnPoints.Count();
        int removedCount = 0;

        // Clean up null entries from the static array, which can happen if entities are deleted.
        for (int i = s_AllSpawnPoints.Count() - 1; i >= 0; i--)
        {
            if (!s_AllSpawnPoints[i])
            {
                s_AllSpawnPoints.Remove(i);
                removedCount++;
            }
        }

        if (removedCount > 0)
        {
            Print(string.Format("[IA_AISpawnPoint] Cleaned up %1 null spawn points. Original count: %2, New count: %3", removedCount, initialCount, s_AllSpawnPoints.Count()), LogLevel.NORMAL);
        }
        
        return s_AllSpawnPoints;
    }
}
