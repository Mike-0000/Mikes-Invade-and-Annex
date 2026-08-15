// IA_AISpawnPoint.c
// Placeable occupying-force spawn marker. If none exist inside an area,
// GenerateRandomAiGroups keeps the default random-in-area behavior.

class IA_AISpawnPointClass: ScriptedGameTriggerEntityClass
{
};

class IA_AISpawnPoint : ScriptedGameTriggerEntity
{
    [Attribute(defvalue: "25", UIWidgets.EditBox, "Radius (m) around this marker to scatter occupying AI. 0 = exact position.", category: "Spawn", params: "0 2000")]
    protected float m_fSpawnRadius;

    private static ref array<IA_AISpawnPoint> s_AllSpawnPoints = new array<IA_AISpawnPoint>();

    void IA_AISpawnPoint(IEntitySource src, IEntity parent)
    {
        SetEventMask(EntityEvent.INIT);
    }

    override void EOnInit(IEntity owner)
    {
        EnablePeriodicQueries(false);

        float radius = m_fSpawnRadius;
        if (radius < 0)
            radius = 0;

        SetSphereRadius(radius);

        if (Replication.IsServer())
        {
            if (!s_AllSpawnPoints.Contains(this))
            {
                s_AllSpawnPoints.Insert(this);
                Print(string.Format("[IA_AISpawnPoint] Registered spawn point %1 at %2 (radius %3m). Total spawn points: %4", this, GetOrigin(), radius, s_AllSpawnPoints.Count()), LogLevel.NORMAL);
            }
        }
    }

    float GetSpawnRadius()
    {
        return m_fSpawnRadius;
    }

    //! Random position inside this marker's radius. Radius 0 (or below 1m) returns the marker origin.
    vector GetRandomSpawnPosition()
    {
        vector origin = GetOrigin();
        float radius = m_fSpawnRadius;
        if (radius < 1)
            return origin;

        vector pos = IA_Game.rng.GenerateRandomPointInRadius(1, radius, origin);
        BaseWorld world = GetGame().GetWorld();
        if (!world)
            return pos;

        float y = world.GetSurfaceY(pos[0], pos[2]);
        pos[1] = y;
        return pos;
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

    //! Spawn points whose origin sits inside the given area. Empty if area is null.
    static ref array<IA_AISpawnPoint> GetSpawnPointsInArea(IA_Area area)
    {
        ref array<IA_AISpawnPoint> inArea = new array<IA_AISpawnPoint>();
        if (!area)
            return inArea;

        array<IA_AISpawnPoint> allSpawnPoints = GetAllSpawnPoints();
        foreach (IA_AISpawnPoint spawnPoint : allSpawnPoints)
        {
            if (!spawnPoint)
                continue;

            if (area.IsPositionInside(spawnPoint.GetOrigin()))
                inArea.Insert(spawnPoint);
        }

        return inArea;
    }
}
