// IA_ChangeRoleUserAction.c
// User action to request a change to a specific player role

class IA_ChangeRoleUserAction : ScriptedUserAction
{
    // The role this action will request
    [Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Role to assign when action is used", enums: ParamEnumArray.FromEnum(IA_PlayerRole))]
    private IA_PlayerRole m_TargetRole;
    
    // Optional attribute to define special requirements (e.g., rank, whitelist)
    [Attribute(defvalue: "0", desc: "Minimum rank/level required (0 = no requirement)")]
    private int m_RequiredRank;
    
    //------------------------------------------------------------------------------------------------
    override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent) 
    {
        // Initialization if needed
    }
    
    //------------------------------------------------------------------------------------------------
    override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
    {   
        // Ensure we're on the server
        if (!Replication.IsServer())
        {
            // On client, just send an RPC to the server to handle this
            // Note: In a real implementation, you would need to set up proper RPC handling
            Print("Role change requests must be processed on the server. Please use the server version.", LogLevel.WARNING);
            return;
        }
        
        // Ensure the user entity is valid
        if (!pUserEntity)
        {
            Print("Error: User entity is null.", LogLevel.ERROR);
            return;
        }
        
        // Get player ID
        PlayerManager playerManager = GetGame().GetPlayerManager();
        int playerId = playerManager.GetPlayerIdFromControlledEntity(pUserEntity);
        if(!playerId){
			Print("PlayerID is NULL!",LogLevel.ERROR);
		}
        // Check if player is allowed to take this role
        if (!IsPlayerAllowed(pUserEntity, playerId))
        {
            // Notify player they can't take this role (ideally via UI)
            Print("You don't meet the requirements for this role.", LogLevel.WARNING);
            return;
        }
        
        // Get the role manager and try to assign the role
        IA_RoleManager roleManager = IA_RoleManager.GetInstance();
        bool success = roleManager.TryAssignRole(playerId, m_TargetRole);
        
        if (success)
        {
            string roleName = IA_RoleManager.GetRoleName(m_TargetRole);
            Print(string.Format("You are now a %1", roleName), LogLevel.NORMAL);
        }
        else
        {
            // Role is full or other error
            int current = roleManager.GetRoleCount(m_TargetRole);
            int max = roleManager.GetRoleLimit(m_TargetRole);
            Print(string.Format("Cannot assign role %1 (%2/%3 slots filled)",
                IA_RoleManager.GetRoleName(m_TargetRole),
                current,
                max), LogLevel.WARNING);
        }
    }
    
    //------------------------------------------------------------------------------------------------
    override bool CanBeShownScript(IEntity user)
    {
        // Don't show action if user already has this role
		SCR_ChimeraCharacter char = SCR_ChimeraCharacter.Cast(user);
        if (char && char.GetRole() == m_TargetRole)
            return false;
            
        // If there are specific conditions for showing this action, check them here
        // For example, only show pilot role actions near an aircraft
        
        return true;
    }
    
    //------------------------------------------------------------------------------------------------
    override bool CanBePerformedScript(IEntity user)
    {
        // Quick check to see if the action can be performed
        // Detailed verification happens in PerformAction
        
        if (!user)
            return false;
            
        // Don't allow if user already has this role
		SCR_ChimeraCharacter char = SCR_ChimeraCharacter.Cast(user);
        if (char && char.GetRole() == m_TargetRole)
            return false;
            
        // Check if role is at capacity (only if we're on the server)
        if (Replication.IsServer())
        {
            IA_RoleManager roleManager = IA_RoleManager.GetInstance();
            if (roleManager.IsRoleFull(m_TargetRole))
                return false;
        }
        
        return true;
    }
    
    //------------------------------------------------------------------------------------------------
    override bool GetActionNameScript(out string outName)
    {
        string roleName = IA_RoleManager.GetRoleName(m_TargetRole);
        
        // Add availability info on server side
        if (Replication.IsServer())
        {
            IA_RoleManager roleManager = IA_RoleManager.GetInstance();
            string availability = roleManager.GetRoleAvailabilityString(m_TargetRole);
            outName = string.Format("Become %1 (%2)", roleName, availability);
        }
        else
        {
            outName = string.Format("Become %1", roleName);
        }
        
        return true;
    }
    
    //------------------------------------------------------------------------------------------------
    override bool HasLocalEffectOnlyScript()
    {
        return false; // This action affects the game state
    }
    
    //------------------------------------------------------------------------------------------------
    override bool CanBroadcastScript() 
    { 
        return true; // This should be broadcast to other players
    }

    //------------------------------------------------------------------------------------------------
    /**
     * Helper function to determine if the player is allowed to take this role
     */
    private bool IsPlayerAllowed(IEntity pUserEntity, int playerId)
    {
        // If no special requirements, always allow
        if (m_RequiredRank <= 0)
            return true;
            
        // Here you can implement various checks:
        // 1. Check player rank/level if your game has such a system
        // 2. Check whitelist (e.g., for special roles like pilot)
        // 3. Check player stats or qualifications
        
        // Example: check for a hypothetical rank component
        // RankComponent rankComp = RankComponent.Cast(pUserEntity.FindComponent(RankComponent));
        // if (rankComp && rankComp.GetRank() >= m_RequiredRank)
        //    return true;
            
        // For specialized roles like pilot, you might want more specific checks
        if (m_TargetRole == IA_PlayerRole.PILOT)
        {
            // Example: Check if player has pilot qualification or is on a whitelist
            // return IsPilotWhitelisted(playerId);
        }
        
        // For demonstration, always allow
        return true;
    }
}; 