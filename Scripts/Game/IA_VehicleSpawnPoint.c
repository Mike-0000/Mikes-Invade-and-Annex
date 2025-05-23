class IA_VehicleSpawnPointClass: GenericEntityClass {};

class IA_VehicleSpawnPoint: GenericEntity
{
    [Attribute(defvalue: "0", desc: "Area group this spawn point belongs to")]
    protected int m_areaGroup;
    
    [Attribute(defvalue: "1", desc: "Allow civilian vehicles")]
    bool m_allowCivilian;
    
    [Attribute(defvalue: "1", desc: "Allow military vehicles")]
    bool m_allowMilitary;
    
    private Vehicle m_spawnedVehicle;
    private bool m_isOccupied;
    
    static ref array<IA_VehicleSpawnPoint> s_allSpawnPoints = {};
    
    void IA_VehicleSpawnPoint(IEntitySource src, IEntity parent)
    {
        if (!Replication.IsServer())
            return;
            
        if (!s_allSpawnPoints.Contains(this))
            s_allSpawnPoints.Insert(this);
    }
    
    static array<IA_VehicleSpawnPoint> GetSpawnPointsByGroup(int groupNumber)
    {
        array<IA_VehicleSpawnPoint> points = {};
        
        foreach (IA_VehicleSpawnPoint point : s_allSpawnPoints)
        {
            if (!point)
                continue;
                
            if (point.GetAreaGroup() == groupNumber)
                points.Insert(point);
        }
        
        return points;
    }
    
    int GetAreaGroup()
    {
        return m_areaGroup;
    }
    
    bool CanSpawnVehicle()
    {
        return !m_spawnedVehicle && !m_isOccupied;
    }
    
    Vehicle SpawnVehicle(IA_Faction faction, Faction AreaFaction)
    {
        if (!CanSpawnVehicle())
            return null;
            
        m_spawnedVehicle = IA_VehicleManager.SpawnVehicle(faction, GetOrigin(), AreaFaction);
        return m_spawnedVehicle;
    }
    
    // Spawn a random vehicle according to the spawn point's settings
    Vehicle SpawnRandomVehicle(IA_Faction faction, Faction AreaFaction)
    {
        if (!CanSpawnVehicle())
            return null;

        m_spawnedVehicle = IA_VehicleManager.SpawnRandomVehicle(faction, m_allowCivilian, m_allowMilitary, GetOrigin(), AreaFaction);
        return m_spawnedVehicle;
    }
    
    void OnVehicleRemoved()
    {
        m_spawnedVehicle = null;
        m_isOccupied = false;
    }
    
    override void EOnFrame(IEntity owner, float timeSlice)
    {
        super.EOnFrame(owner);
        
        if (!Replication.IsServer())
            return;
            
        if (m_spawnedVehicle)
        {
            // Check if vehicle still exists and is nearby
            if (vector.Distance(GetOrigin(), m_spawnedVehicle.GetOrigin()) > 100)
            {
                OnVehicleRemoved();
            }
            else
            {
                m_isOccupied = IA_VehicleManager.IsVehicleOccupied(m_spawnedVehicle);
            }
        }
    }
}; 