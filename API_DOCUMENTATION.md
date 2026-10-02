# Game API Documentation

This document describes the Game API endpoints and internal script APIs used in the Invade & Annex mission, specifically focusing on those invoked within `IA_AreaInstance.c`.

## 1. Backend REST API
These endpoints connect to the remote statistics server. The API handler is implemented in `IA_ApiHandler.c`.

**Base URL:** `https://invade-annex-api.azurewebsites.net/api`

Source, hosting and deploy steps: `backend/azure-functions/README.md`. Builds released before October 2026 call the original `invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net` app, which writes to the same database and has no source.

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
Fetches global and server-specific leaderboard data. Legacy: the current build does not call it and uses `GET /leaderboard` instead. Older builds still call it, so the route stays.
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

### `GET /leaderboard`
Fetches one page of one board in one sort order, plus the asking player's own row. The session board is not served here; the game server builds it itself.
- **Parameters:**
  - `serverGuid`: The unique identifier for this server. Required.
  - `board`: `global` (every player, all servers added up), `server` (this server's players) or `servers` (one row per server). Default `global`.
  - `sort`: `score`, `kills`, `deaths`, `kd`, `hvt`, `guard`, `obj`, `transport`, `insertions` or `players`. Default `score`.
  - `dir`: `asc` or `desc`. Default `desc`.
  - `offset`: Rows to skip. Default 0, at most 1000000.
  - `limit`: Rows wanted. Default 25, at most 100. 0 returns `total` and `me` only.
  - `playerId`: Identity id of the player whose own row is wanted, 1 to 64 characters. Optional. Ignored on `servers`, where `me` is the asking server's row.
- **Response Body:**
  ```json
  {
    "board": "global",
    "sort": "score",
    "dir": "desc",
    "offset": 0,
    "total": 1234,
    "rows": [ { "r": 1, "n": "String", "k": 0, "d": 0, "h": 0, "g": 0, "o": 0, "s": 0, "t": 0, "i": 0, "p": 0 } ],
    "me": [ { "r": 57, "n": "String", "k": 0, "d": 0, "h": 0, "g": 0, "o": 0, "s": 0, "t": 0, "i": 0, "p": 0 } ]
  }
  ```
  Row keys: `r` rank in the requested sort, `n` name, `k` kills, `d` deaths, `h` HVT kills, `g` HVT guard kills, `o` objective score, `s` score, `t` transport rating, `i` insertions, `p` players. `me` is empty when no `playerId` was sent or the player is not on the board. Rows carry no player ids. Numbers are capped at 2000000000.
- **Errors:** `403 Invalid or inactive server GUID.`; `400` when `board`, `sort` or `dir` is not one of the values above.

Query defaults, caps and the meaning of each column per board: `backend/azure-functions/README.md`.

---

### `POST /submitTransport`
Adds transport-pilot rating to players' **global** totals and to their totals on this server, which the `server` and `servers` leaderboards show. Sent about once a minute while there is something to send. `batchId` makes a resend harmless.
- **Request Body:**
  ```json
  {
    "serverGuid": "String (UUID)",
    "batchId": "String",
    "entries": [ { "playerId": "String", "playerName": "String", "points": 40, "insertions": 2 } ]
  }
  ```

### `POST /getTransportRatings`
Fetches global transport ratings and the central skin thresholds.
- **Request Body:**
  ```json
  { "serverGuid": "String (UUID)", "playerIds": ["String"] }
  ```
- **Response Body:**
  ```json
  {
    "ratings": [ { "playerId": "String", "rating": 50310, "insertions": 2140 } ],
    "skins": [ { "key": "huey_tan", "required": 50000 } ]
  }
  ```

Rules, schema and the function contract: `docs/transport-pilot-progression.md`.

### Leaderboard storage
Migration: `backend/supabase/migrations/20261001030000_leaderboard_pages.sql`. It only adds; the functions older builds call are unchanged.
- `player_transport_server`: one row per `(player_bohemia_id, server_id)` with `rating` and `insertions`, what a player has flown on one server. `submit_transport_batch` now adds each batch here as well as to the global `player_transport_ratings`. Batches accepted earlier were backfilled from `transport_batches`.
- `api_get_leaderboard`: the function behind `GET /leaderboard`. Transport rating is its own column and is not part of score. A player with transport rating and no other stats still has a row.

### How the game shows a leaderboard
Nothing is replicated. A board is asked for a page of 25 rows at a time (`IA_BoardProtocol.PAGE_ROWS`).
1. The menu (`IA_StatisticsMenu`) asks for the page under the rows in view through `SCR_PlayerController.IA_AskLeaderboardPage` (a view tag that comes back with the answer, board, sort key, direction, row offset). The server refuses a player who asks again within 100 ms.
2. `IA_LeaderboardManagerComponent` checks the request. The session board is read from RAM (`IA_SessionRankManagerComponent.BuildBoardPage`). For the others it keeps answered pages for 45 seconds, so players reading the same board share a request, and drops them when a stats batch is accepted.
3. A page it does not hold is fetched with `IA_ApiHandler.FetchLeaderboardPage`, one request at a time, at most 32 waiting, each given 20 seconds. When only the player's own row is missing it asks with `limit=0`.
4. The answer goes to the asking player only, as `SCR_PlayerController` RPCs: a head (status, total, offset, the player's own row), then the rows as packed `IA_BoardRow` lines in chunks of about 700 characters (`IA_BoardProtocol.CHUNK_CHARS`). A status other than OK (failed, offline, busy) is a head with no rows.
5. The menu files the rows in `IA_BoardModel`, and `IA_LeaderboardBoard` draws the table from it.

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
Server side. Queues `IA_StatEvent` objects and sends them every 60 seconds as the `matchData` array of `POST /submitStats`. Each one is also credited to the player's session rank (`IA_SessionRankManagerComponent`). Events:
- `PlayerKill`: a player killed a member of an AI group (HVTs and their guards are counted below instead)
- `PlayerDeath`: a player died
- `HVTKill`: a player killed a side objective's HVT
- `HVTGuardKill`: a player killed one of the HVT's guards
- `CaptureContribution`: a player's capture score when a zone or a base is captured

Transport insertions are not in this batch. They go through `IA_TransportPilotStore` and `POST /submitTransport`; `QueueTransportInsertion` only adds session XP.
