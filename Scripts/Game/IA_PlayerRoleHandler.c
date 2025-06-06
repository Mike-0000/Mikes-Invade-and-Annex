// IA_PlayerRoleHandler.c
// Connects the role system to player lifecycle events

class IA_PlayerRoleHandler
{
    // Track players that we've already processed
    private static ref map<string, bool> s_ProcessedPlayers = new map<string, bool>();
    
    // Track players who disconnected with grace period (GUID -> disconnect timestamp)
    private static ref map<string, int> s_DisconnectedPlayers = new map<string, int>();
    
    // Grace period for reconnection (3 minutes in seconds)
    private static const int RECONNECT_GRACE_PERIOD_SECONDS = 180;
    
    private static bool s_IsInitialized = false;
    
    // Initialize the handler
    static void Initialize()
    {
        if (!Replication.IsServer())
            return;
        
        if (s_IsInitialized)
            return;
            
        s_IsInitialized = true;
        s_ProcessedPlayers = new map<string, bool>();
        s_DisconnectedPlayers = new map<string, int>();
        
        // Start periodic player checks
        GetGame().GetCallqueue().CallLater(PeriodicPlayerCheck, 2500, true);
        
        Print("IA_PlayerRoleHandler initialized with periodic player checks and 3-minute grace period.", LogLevel.NORMAL);
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
        
        map<string, bool> currentPlayerGuids = new map<string, bool>();
        
        // Check for new players that we haven't processed yet, and reapply roles for existing players
        foreach (int playerId : currentPlayers)
        {
            string playerGuid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
            if (playerGuid.IsEmpty())
                continue;
            
            currentPlayerGuids.Insert(playerGuid, true);

            // Check if this player was in grace period (reconnecting)
            if (s_DisconnectedPlayers.Contains(playerGuid))
            {
                // Player reconnected within grace period - restore them
                HandlePlayerReconnect(playerId, playerGuid);
                s_DisconnectedPlayers.Remove(playerGuid);
                
                // Mark them as processed so they don't get treated as a new player
                s_ProcessedPlayers[playerGuid] = true;
            }

            if (!s_ProcessedPlayers.Contains(playerGuid))
            {
                // New player found
                s_ProcessedPlayers.Insert(playerGuid, true);
                HandleNewPlayer(playerId);
            }
            else
            {
                // Existing player - check if they need role reapplied (respawn case)
                CheckAndReapplyPlayerRole(playerId, playerGuid);
            }
        }
        
        // Check for players who have disconnected
        array<string> guidsToRemove = {};
        
        foreach (string trackedPlayerGuid, bool processed : s_ProcessedPlayers)
        {
            if (!currentPlayerGuids.Contains(trackedPlayerGuid))
            {
                // Player has disconnected
                guidsToRemove.Insert(trackedPlayerGuid);
                HandleDisconnectedPlayer(trackedPlayerGuid);
            }
        }
        
        // Clean up our tracking list for disconnected players
        foreach (string idToRemove : guidsToRemove)
        {
            s_ProcessedPlayers.Remove(idToRemove);
        }
        
        // Check grace period expiration for disconnected players
        CheckGracePeriodExpiration();
    }
    
    // Check for grace period expiration and clean up roles for players who didn't reconnect
    private static void CheckGracePeriodExpiration()
    {
        int currentTime = System.GetUnixTime();
        array<string> expiredGuids = {};
        
        foreach (string playerGuid, int disconnectTime : s_DisconnectedPlayers)
        {
            if (currentTime - disconnectTime > RECONNECT_GRACE_PERIOD_SECONDS)
            {
                // Grace period expired
                expiredGuids.Insert(playerGuid);
            }
        }
        
        // Clean up expired players
        foreach (string expiredGuid : expiredGuids)
        {
            s_DisconnectedPlayers.Remove(expiredGuid);
            
            // Now actually remove their role
            IA_RoleManager roleManager = IA_RoleManager.GetInstance();
            roleManager.UnregisterPlayer(expiredGuid);
            
            Print(string.Format("Grace period expired for player GUID %1. Role reservation released.", expiredGuid), LogLevel.NORMAL);
        }
    }
    
    // Handle player reconnecting within grace period
    private static void HandlePlayerReconnect(int playerId, string playerGuid)
    {
        Print(string.Format("Player %1 (GUID: %2) reconnected within grace period. Restoring role...", playerId, playerGuid), LogLevel.NORMAL);
        
        // The role is still stored in the role manager, so we just need to reapply it to the character
        CheckAndReapplyPlayerRole(playerId, playerGuid);
    }
    
    // Check if a player's role needs to be reapplied to their current character
    private static void CheckAndReapplyPlayerRole(int playerId, string playerGuid)
    {
        IA_RoleManager roleManager = IA_RoleManager.GetInstance();
        IA_PlayerRole storedRole = roleManager.GetPlayerRole(playerId);
        
        // If player has no stored role, nothing to do
        if (storedRole == IA_PlayerRole.NONE)
            return;
            
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!playerManager)
            return;
            
        IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
        if (!playerEntity)
            return;
            
        SCR_ChimeraCharacter playerChar = SCR_ChimeraCharacter.Cast(playerEntity);
        if (!playerChar)
            return;
            
        // Check if character's current role matches what we have stored
        IA_PlayerRole currentRole = playerChar.GetRole();
        if (currentRole != storedRole)
        {
            // Role mismatch - reapply the stored role
            playerChar.SetRole(storedRole, true);
            Print(string.Format("Reapplied role %1 to player %2 after respawn", 
                typename.EnumToString(IA_PlayerRole, storedRole), playerId), LogLevel.NORMAL);
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
    private static void HandleDisconnectedPlayer(string playerGuid)
    {
        Print(string.Format("Player with GUID %1 disconnected. Starting 3-minute grace period...", playerGuid), LogLevel.NORMAL);
        
        // Don't immediately clean up role - add to grace period tracking instead
        int currentTime = System.GetUnixTime();
        s_DisconnectedPlayers[playerGuid] = currentTime;
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