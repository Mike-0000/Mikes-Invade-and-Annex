// IA_PlayerRoleHandler.c
// Connects the role system to player lifecycle events

class IA_PlayerRoleHandler
{
    // Track players that we've already processed
    private static ref map<int, bool> s_ProcessedPlayers = new map<int, bool>();
    private static bool s_IsInitialized = false;
    
    // Initialize the handler
    static void Initialize()
    {
        if (!Replication.IsServer())
            return;
        
        if (s_IsInitialized)
            return;
            
        s_IsInitialized = true;
        s_ProcessedPlayers = new map<int, bool>();
        
        // Start periodic player checks
        GetGame().GetCallqueue().CallLater(PeriodicPlayerCheck, 2500, true);
        
        Print("IA_PlayerRoleHandler initialized with periodic player checks.", LogLevel.NORMAL);
    }
    
    // Periodically check for new/disconnected players
    private static void PeriodicPlayerCheck()
    {
        if (!Replication.IsServer())
            return;
            
        // Get the player manager
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return;
            
        // Get all current players
        array<int> currentPlayers = {};
        playerManager.GetPlayers(currentPlayers);
        
        // Check for new players that we haven't processed yet
        foreach (int playerId : currentPlayers)
        {
            if (!s_ProcessedPlayers.Contains(playerId))
            {
                // New player found
                s_ProcessedPlayers.Insert(playerId, true);
                HandleNewPlayer(playerId);
            }
        }
        
        // Check for players who have disconnected
        array<int> playersToRemove = {};
        
        foreach (int trackedPlayerId, bool processed : s_ProcessedPlayers)
        {
            bool stillConnected = false;
            
            // Check if player is still in the current players list
            foreach (int currentId : currentPlayers)
            {
                if (currentId == trackedPlayerId)
                {
                    stillConnected = true;
                    break;
                }
            }
            
            if (!stillConnected)
            {
                // Player has disconnected
                playersToRemove.Insert(trackedPlayerId);
                HandleDisconnectedPlayer(trackedPlayerId);
            }
        }
        
        // Clean up our tracking list
        foreach (int idToRemove : playersToRemove)
        {
            s_ProcessedPlayers.Remove(idToRemove);
        }
    }
    
    // Handle new player joining
    private static void HandleNewPlayer(int playerId)
    {
        Print(string.Format("Player %1 connected, initializing role...", playerId), LogLevel.NORMAL);
        
        // The player entity might not be ready yet, set up a delayed check
        GetGame().GetCallqueue().CallLater(InitializePlayerRole, 1500, false, playerId);
    }
    
    // Initialize a player's role (called with delay after connection)
    private static void InitializePlayerRole(int playerId)
    {
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return;
            
        IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
        if (!playerEntity)
        {
            // Player entity still not available, try again later
            Print(string.Format("InitializePlayerRole: Player entity for player %1 not yet available. Retrying.", playerId), LogLevel.DEBUG);
            GetGame().GetCallqueue().CallLater(InitializePlayerRole, 1000, false, playerId);
            return;
        }
        
        // Check if the player entity is an SCR_ChimeraCharacter
		SCR_ChimeraCharacter playerChar = SCR_ChimeraCharacter.Cast(playerEntity);
        if (!playerChar) 
        {
            // This means the controlled entity is not an SCR_ChimeraCharacter.
            // Roles are applied to characters, so we cannot proceed with role assignment for this entity.
            Print(string.Format("InitializePlayerRole: Controlled entity for player %1 is not an SCR_ChimeraCharacter. Cannot assign role.", playerId), LogLevel.WARNING);
            return; 
        }
        
        // Initialize with default role (Rifleman)
        IA_RoleManager roleManager = IA_RoleManager.GetInstance();
        roleManager.TryAssignRole(playerId, IA_PlayerRole.RIFLEMAN);
    }
    
    // Handle player disconnection event
    private static void HandleDisconnectedPlayer(int playerId)
    {
        Print(string.Format("Player %1 disconnected, cleaning up role...", playerId), LogLevel.NORMAL);
        
        IA_RoleManager roleManager = IA_RoleManager.GetInstance();
        roleManager.UnregisterPlayer(playerId);
    }
}

// Add this integration to IA_Game to ensure it's initialized
modded class IA_Game
{
    // Called during IA_Game initialization
    override void Init()
    {
        super.Init();
        
        // Initialize the role system
        if (Replication.IsServer())
        {
            Print("Initializing IA_PlayerRoleHandler from IA_Game", LogLevel.NORMAL);
            IA_PlayerRoleHandler.Initialize();
        }
    }
}; 