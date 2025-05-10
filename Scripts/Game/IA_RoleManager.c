// IA_RoleManager.c
// A singleton class that manages player roles and enforces role limits

class IA_RoleManager
{
    // Singleton instance
    private static ref IA_RoleManager s_Instance;
    
    // Role count tracking - how many players currently have each role
    private ref map<IA_PlayerRole, int> m_RoleCounts = new map<IA_PlayerRole, int>();
    
    // Role limits - maximum number of players allowed for each role
    private ref map<IA_PlayerRole, int> m_RoleLimits = new map<IA_PlayerRole, int>();
    
    // Player to role mapping for quick lookups
    private ref map<int, IA_PlayerRole> m_PlayerRoles = new map<int, IA_PlayerRole>();
    
    //------------------------------------------------------------------------------------------------
    // SINGLETON ACCESS
    //------------------------------------------------------------------------------------------------
    
    static IA_RoleManager GetInstance()
    {
        if (!s_Instance)
        {
            s_Instance = new IA_RoleManager();
            s_Instance.Init();
        }
        return s_Instance;
    }
    
    //------------------------------------------------------------------------------------------------
    // INITIALIZATION
    //------------------------------------------------------------------------------------------------
    
    private void Init()
    {
        // Initialize role counts to zero
        m_RoleCounts[IA_PlayerRole.NONE] = 0;
        m_RoleCounts[IA_PlayerRole.RIFLEMAN] = 0;
        m_RoleCounts[IA_PlayerRole.MACHINEGUNNER] = 0;
        m_RoleCounts[IA_PlayerRole.GRENADIER] = 0;
        m_RoleCounts[IA_PlayerRole.MARKSMAN] = 0;
        m_RoleCounts[IA_PlayerRole.MEDIC] = 0;
        m_RoleCounts[IA_PlayerRole.ANTITANK_LIGHT] = 0;
        m_RoleCounts[IA_PlayerRole.ANTITANK_HEAVY] = 0;
        m_RoleCounts[IA_PlayerRole.TEAMLEADER] = 0;
        m_RoleCounts[IA_PlayerRole.PILOT] = 0;
        m_RoleCounts[IA_PlayerRole.CREWMAN] = 0;
        
        // Set default role limits
        // These can be adjusted based on game balance needs
        m_RoleLimits[IA_PlayerRole.NONE] = 999; // No limit for default/none role
        m_RoleLimits[IA_PlayerRole.RIFLEMAN] = 999; // No practical limit for riflemen
        m_RoleLimits[IA_PlayerRole.MACHINEGUNNER] = 4; // Example: Limit to 4 machine gunners
        m_RoleLimits[IA_PlayerRole.GRENADIER] = 4;
        m_RoleLimits[IA_PlayerRole.MARKSMAN] = 4;
        m_RoleLimits[IA_PlayerRole.MEDIC] = 6;
        m_RoleLimits[IA_PlayerRole.ANTITANK_LIGHT] = 6;
        m_RoleLimits[IA_PlayerRole.ANTITANK_HEAVY] = 2; // More restricted for balance
        m_RoleLimits[IA_PlayerRole.TEAMLEADER] = 12;
        m_RoleLimits[IA_PlayerRole.PILOT] = 8;
        m_RoleLimits[IA_PlayerRole.CREWMAN] = 12;
        
        Print("IA_RoleManager initialized", LogLevel.NORMAL);
    }
    
    //------------------------------------------------------------------------------------------------
    // ROLE MANAGEMENT METHODS
    //------------------------------------------------------------------------------------------------
    
    // Check if a role has reached its limit
    bool IsRoleFull(IA_PlayerRole role)
    {
        // If role doesn't exist in limits map for some reason, default to allowing
        if (!m_RoleLimits.Contains(role))
            return false;
            
        // If role doesn't exist in counts map for some reason, assume zero
        int currentCount = 0;
        if (m_RoleCounts.Contains(role))
            currentCount = m_RoleCounts[role];
            
        return currentCount >= m_RoleLimits[role];
    }
    
    // Get the current count for a specific role
    int GetRoleCount(IA_PlayerRole role)
    {
        if (m_RoleCounts.Contains(role))
            return m_RoleCounts[role];
        return 0;
    }
    
    // Get the maximum allowed for a specific role
    int GetRoleLimit(IA_PlayerRole role)
    {
        if (m_RoleLimits.Contains(role))
            return m_RoleLimits[role];
        return 999; // Default to no practical limit
    }
    
    // Set a new limit for a specific role
    void SetRoleLimit(IA_PlayerRole role, int limit)
    {
        m_RoleLimits[role] = limit;
    }
    
    // Try to assign a role to a player
    // Returns true if successful, false if the role is full
    bool TryAssignRole(int playerId, IA_PlayerRole newRole)
    {
        if (!Replication.IsServer())
        {
            Print("TryAssignRole can only be called on the server", LogLevel.ERROR);
            return false;
        }
        
        // Check if role is full
        if (IsRoleFull(newRole))
        {
            Print(string.Format("Role %1 is at capacity (%2/%3)", 
                typename.EnumToString(IA_PlayerRole, newRole),
                GetRoleCount(newRole),
                GetRoleLimit(newRole)), LogLevel.NORMAL);
            return false;
        }
        
        // If player already has a role, unregister from old role first
        if (m_PlayerRoles.Contains(playerId))
        {
            IA_PlayerRole oldRole = m_PlayerRoles[playerId];
            if (m_RoleCounts.Contains(oldRole))
            {
                int oldCount = m_RoleCounts[oldRole];
                if (oldCount > 0)
                {
                    oldCount = oldCount - 1;
                    m_RoleCounts[oldRole] = oldCount;
                }
            }
        }
        
        // Assign new role and update counts
        m_PlayerRoles[playerId] = newRole;
        if (!m_RoleCounts.Contains(newRole))
            m_RoleCounts[newRole] = 0;
            
        int newCount = m_RoleCounts[newRole];
        newCount = newCount + 1;
        m_RoleCounts[newRole] = newCount;
        
        // Update the player's component
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (playerManager)
        {
            IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
            if (playerEntity)
            {
				SCR_ChimeraCharacter char = SCR_ChimeraCharacter.Cast(playerEntity);
                if (char)
                {
                    char.SetRole(newRole, true); // Force replication as this is a server-authoritative change
                    Print(string.Format("Assigned role %1 to player %2", 
                        typename.EnumToString(IA_PlayerRole, newRole), 
                        playerId), LogLevel.NORMAL);
                }
                else
                {
                    Print(string.Format("Could not cast player entity to SCR_ChimeraCharacter for player %1", playerId), LogLevel.WARNING);
                }
            }
        }
        
        return true;
    }
    
    // Unregister a player's role (e.g., when they disconnect)
    void UnregisterPlayer(int playerId)
    {
        if (!Replication.IsServer())
            return;
            
        if (m_PlayerRoles.Contains(playerId))
        {
            IA_PlayerRole oldRole = m_PlayerRoles[playerId];
            if (m_RoleCounts.Contains(oldRole))
            {
                int oldCount = m_RoleCounts[oldRole];
                if (oldCount > 0)
                {
                    oldCount = oldCount - 1;
                    m_RoleCounts[oldRole] = oldCount;
                }
            }
                
            m_PlayerRoles.Remove(playerId);
        }
    }
    
    // Get a string for displaying role availability to players
    string GetRoleAvailabilityString(IA_PlayerRole role)
    {
        int count = GetRoleCount(role);
        int limit = GetRoleLimit(role);
        
        return string.Format("%1/%2", count, limit);
    }
    
    // Get human-readable name for a role
    static string GetRoleName(IA_PlayerRole role)
    {
        switch (role)
        {
            case IA_PlayerRole.NONE: return "Unassigned";
            case IA_PlayerRole.RIFLEMAN: return "Rifleman";
            case IA_PlayerRole.MACHINEGUNNER: return "Machine Gunner";
            case IA_PlayerRole.GRENADIER: return "Grenadier";
            case IA_PlayerRole.MARKSMAN: return "Marksman";
            case IA_PlayerRole.MEDIC: return "Combat Medic";
            case IA_PlayerRole.ANTITANK_LIGHT: return "Light Anti-Tank";
            case IA_PlayerRole.ANTITANK_HEAVY: return "Heavy Anti-Tank";
            case IA_PlayerRole.TEAMLEADER: return "Team Leader";
            case IA_PlayerRole.PILOT: return "Pilot";
            case IA_PlayerRole.CREWMAN: return "Crewman";
        }
        
        return "Unknown Role";
    }
}; 