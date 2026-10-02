# Game API Documentation

This document describes the Game API endpoints and internal script APIs used in the Invade & Annex mission, specifically focusing on those invoked within `IA_AreaInstance.c`.

## 1. Backend REST API
These endpoints connect to the remote statistics server. The API handler is implemented in `IA_ApiHandler.c`.

**Base URL:** `https://invade-annex-api.azurewebsites.net/api`

Source, hosting and deploy steps: `backend/azure-functions/README.md`. Builds released before October 2026 call the original `invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net` app, which writes to the same database and has no source.

### `POST /registerServer`
Registers the game server with the backend to receive a unique `serverGuid`. Sent once, when `api_config.json` holds no GUID. `serverName` must not be blank; see "Server name" below for what the game sends.
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
Submits match statistics and player data at the end of a session or periodically. A non-empty `serverName` replaces the name on record; an empty one keeps it. The game never sends a name of spaces only.
- **Request Body:**
  ```json
  {
    "serverGuid": "String",
    "serverName": "String",
    "matchData": { ...JSON Object... }
  }
  ```

### `POST /sync`
The one timed exchange between a game server and the stats service (`IA_ApiSync`). About once a minute it carries whatever waits: the statistics events, the transport points, the rating lookups players wait on and, while somebody has a leaderboard open, the boards and players whose rows are wanted. Only `serverGuid` is required; every other member is left out when there is nothing for it. With nothing waiting and nobody looking, the request is not sent at all.
- **Request Body:**
  ```json
  {
    "serverGuid": "String (UUID)",
    "serverName": "String",
    "statsBatchId": "String",
    "matchData": [ ...events, as for /submitStats... ],
    "transport": { "batchId": "String", "entries": [ ...as for /submitTransport... ] },
    "ratingIds": ["String"],
    "players": ["String"],
    "boards": [ { "board": "server", "etag": "String" } ]
  }
  ```
- **Response Body:** one flat object: `statsStatus` and `transportStatus` (`none`, `accepted`, `duplicate`, `rejected`, `failed`), `ratingsStatus` and `boardsStatus` (`none`, `ok`, `failed`), `ratings` and `skins` as for `/getTransportRatings`, `boards` (`board`, `etag`, `total`, `unchanged`), `serverRows`, `globalRows` and `serversRows` (the first 100 rows by score, high to low, left out for a board whose `etag` still matches), `own` (the asked players' rows, each with `board` and `id`), and what the stats service suggests for the game's timers: `nextSyncSeconds`, `pageBudgetPerHour`, `pageBudgetBurst`, `serverPageSeconds`, `globalPageSeconds`.
- **Errors:** `403` for an unknown GUID. `404` with an empty body or `501 Sync is not available.` mean the stats service has no such route; the game then uses `/submitStats`, `/submitTransport` and `/getTransportRatings` as before and tries `/sync` again once an hour.

A batch keeps its id and goes again unchanged until the answer says `accepted` or `duplicate`, so a lost answer is never counted twice. The full contract, limits and examples: `backend/azure-functions/README.md`.

### Server name
A server owner does not have to set a name. `IA_ServerNameResolver` picks the `serverName` for the routes above, in this order:
1. **Fixed name.** The first line of `$profile:MikesInvadeAndAnnex/server_name.txt`, when the owner has written a name of their own there. A missing or blank file, or one that still holds the text older builds wrote (any line containing `PLEASE RENAME IN server_name.txt`, in any case), is not a fixed name. The game no longer creates this file and never changes it. It is read at server start and again every five minutes.
2. **Live name.** The name the running server holds, asked of the engine with each batch: `GetGame().GetServerInfo().GetName()`, then `ServerLobbyApi.GetServerConfig().GetName()`, then the `game.name` of `BackendApi.GetRunningDSConfig`.
3. **Last known name.** The last live name seen, kept in `$profile:MikesInvadeAndAnnex/last_server_name.txt`. The game writes this file only when the live name changes; it is not for owners to edit.
4. **Nothing known.** `/sync` gets no `serverName` and `/submitStats` gets `""`, so the name on record stays. `/registerServer` gets `Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder`, which the servers board leaves out until a real name arrives with a stats batch.

The name is trimmed, kept to one line and cut to 255 characters. It has no part in which server the statistics belong to: that is the `serverGuid` in `api_config.json`, which the resolver neither reads nor writes. The server log records the name and its source when either changes: `[IA][ServerName] Reporting '<name>' to the statistics service (<source>).` The name reaches the backend with every `/sync` exchange. On a stats service without `/sync` it travels only with a stats batch, so a renamed server shows its new name after its next batch that has events.

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
Adds transport-pilot rating to players' **global** totals and to their totals on this server, which the `server` and `servers` leaderboards show. The current build sends the batch as the `transport` member of `POST /sync`, and uses this route only on a stats service without `/sync`: about once a minute while there is something to send. `batchId` makes a resend harmless.
- **Request Body:**
  ```json
  {
    "serverGuid": "String (UUID)",
    "batchId": "String",
    "entries": [ { "playerId": "String", "playerName": "String", "points": 40, "insertions": 2 } ]
  }
  ```

### `POST /getTransportRatings`
Fetches global transport ratings and the central skin thresholds. The current build asks through the `ratingIds` member of `POST /sync`, and uses this route only on a stats service without `/sync`.
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
2. `IA_LeaderboardManagerComponent` checks the request. The session board is read from RAM (`IA_SessionRankManagerComponent.BuildBoardPage`). The others go to `IA_BoardService`, which holds answered pages (shared by every player, with no player's own row in them) and each player's own row apart: 120 seconds for this server's board, 300 for the global and servers boards. A stats batch no longer clears them; they age out.
3. A page or own row it does not hold is fetched with `IA_ApiHandler.FetchLeaderboardPage`, one request at a time, at most 32 waiting, each given 20 seconds. When only the player's own row is missing it asks with `limit=0`. Each such request is paid for from an allowance: 20 at once and 12 an hour after that for the server, and half of each for one player. With the allowance spent, a page still held is served however old it is; one not held is refused with status LIMITED and the seconds to wait, and the menu shows `BOARD BUSY`.
4. While somebody looks at a board in the order the menu opens on (score, high to low), `POST /sync` brings that board's first 100 rows and the lookers' own rows every minute, and they are held like any other answer. That view then costs no request. Other sort orders, rows past the hundredth and a rank in another order are still asked for as in step 3.
5. The answer goes to the asking player only, as `SCR_PlayerController` RPCs: a head (status, total, offset, the player's own row), then the rows as packed `IA_BoardRow` lines in chunks of about 700 characters (`IA_BoardProtocol.CHUNK_CHARS`). A status other than OK (failed, offline, busy, limited) is a head with no rows.
6. The menu files the rows in `IA_BoardModel`, and `IA_LeaderboardBoard` draws the table from it. An open board asks again when its lifetime is over, not sooner.

### How often a game server calls the stats service
All the numbers are in `IA_ApiTunables`, each with a default and a range; the stats service may suggest others with a `/sync` answer, and a suggestion outside the range is brought into it.
- Nothing to send and nobody on a board: no request.
- Play going on: one `POST /sync` a minute (30 to 600 seconds), whatever it carries.
- Boards: nothing on top of that for the opening view once the exchange has brought it. The first look at a board, and any other order or depth, costs one `GET /leaderboard`, never more than the allowance: 20 at once, 12 an hour after.
- A stats service without `/sync`: one `POST /submitStats` a minute while there are events, `POST /submitTransport` and `POST /getTransportRatings` when there is something for them, `GET /leaderboard` within the same allowance, and one try of `/sync` an hour.

`IA_ApiHandler.CallSummary()` counts the requests sent per route; with debug logging on it is logged every ten minutes.

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

### `IA_ServerNameResolver` (Server name)
- `ForStats()`: The name for `POST /sync` and `POST /submitStats`; empty when none is known.
- `ForRegistration()`: The name for `POST /registerServer`; the old default text when none is known.
- `Pick(fileLine, liveName, lastName, out source)`: The order of precedence on values already read. `IA_ServerNameProbe` (Workbench plugin) checks it and the files.

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
Server side. Queues `IA_StatEvent` objects. `IA_ApiSync` takes them every 60 seconds, at most 1000 to a batch, as the `matchData` array of `POST /sync` under a `statsBatchId`, or of `POST /submitStats` on a stats service without `/sync`. Each one is also credited to the player's session rank (`IA_SessionRankManagerComponent`). Events:
- `PlayerKill`: a player killed a member of an AI group (HVTs and their guards are counted below instead)
- `PlayerDeath`: a player died
- `HVTKill`: a player killed a side objective's HVT
- `HVTGuardKill`: a player killed one of the HVT's guards
- `CaptureContribution`: a player's capture score when a zone or a base is captured

Transport insertions are not in this batch. They go through `IA_TransportPilotStore` as the `transport` member of the same exchange (or `POST /submitTransport`); `QueueTransportInsertion` only adds session XP.
