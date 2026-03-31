# Game API Documentation

This document describes the Game API endpoints and internal script APIs used in the Invade & Annex mission, specifically focusing on those invoked within `IA_AreaInstance.c`.

## 1. Backend REST API
These endpoints connect to the remote statistics server. The API handler is implemented in `IA_ApiHandler.c`.

**Base URL:** `https://invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net/api`

### `POST /registerServer`
Registers the game server with the backend to receive a unique `serverGuid`.
- **Request Body:**
  ```json
  {
    "serverName": "String",
    "ownerEmail": "String"
  }
  ```
- **Response Body:**
  ```json
  {
    "serverGuid": "String (UUID)"
  }
  ```

### `POST /submitStats`
Submits match statistics and player data at the end of a session or periodically.
- **Request Body:**
  ```json
  {
    "serverGuid": "String",
    "serverName": "String",
    "matchData": { ...JSON Object... }
  }
  ```

### `GET /getAllLeaderboards?serverGuid={guid}`
Fetches global and server-specific leaderboard data.
- **Parameters:**
  - `serverGuid`: The unique identifier for this server.
- **Response Body:**
  ```json
  {
    "globalPlayerLeaderboard": [...],
    "serverPlayerLeaderboard": [...],
    "globalServerLeaderboard": [...]
  }
  ```

---

## 2. Internal Game API Classes
Internal script classes that facilitate game logic, AI control, and inter-system communication.

### `IA_Game` (Manager)
- `GetInstance()`: Access the singleton game manager.
- `GetAIScaleFactor()`: Returns the current AI scaling factor based on player count.
- `GetMaxVehiclesForPlayerCount(int players)`: Returns the maximum number of vehicles allowed for the current player load.
- `rng`: A shared `RandomGenerator` instance for consistent randomized outcomes.

### `IA_AiGroup` (AI Control)
- `CreateMilitaryGroupFromUnits(vector pos, IA_Faction faction, int count, Faction areaFaction)`: Spawns a military squad at the specified position.
- `SetTacticalState(IA_GroupTacticalState state, vector pos, IEntity target, bool authority)`: Sets the high-level behavior of a group (e.g., `Attacking`, `Defending`, `Flanking`, `Retreating`).
- `AddOrder(vector pos, IA_AiOrder order, bool instant)`: Directly issues AI orders such as `SearchAndDestroy`, `Move`, or `Patrol`.

### `IA_VehicleManager` (Vehicle Logistics)
- `SpawnRandomVehicle(IA_Faction faction, bool isCivilian, bool isMilitary, vector pos, Faction areaFaction)`: Spawns a vehicle from the catalog.
- `PlaceUnitsInVehicle(Vehicle vehicle, IA_Faction faction, vector areaOrigin, IA_AreaInstance instance, Faction areaFaction)`: Efficiently populates a vehicle with AI.
- `DespawnVehicle(Vehicle vehicle)`: Safely removes a vehicle from the simulation.

### `IA_ApiConfigManager` (Persistence)
- `GetConfig()`: Retrieves the current API configuration.
- `SaveConfig()`: Persists API settings (like `serverGuid`) to `$profile:MikesInvadeAndAnnex/api_config.json`.
- `GetServerNameFromFile()`: Loads the server name from `server_name.txt`.

---

## 3. Global Notification API
Used to broadcast mission-critical information to all players.

### `TriggerGlobalNotification(string messageType, string taskTitle)`
- **Parameters:**
  - `messageType`: Key identifying the message string/layout to display (e.g., `"RadioTowerDefenseStarted"`).
  - `taskTitle`: Human-readable title or area name associated with the notification.
- **Usage Example:**
  `TriggerGlobalNotification("RadioTowerDefenseStarted", m_area.GetName());`

---

## 4. Statistics Tracking (`IA_StatsManager`)
Implicitly uses `IA_StatEvent` and `IA_PlayerStatEntry` to track:
- Kills (AI and Player)
- Deaths
- Objective completions
- Vehicle destructions
