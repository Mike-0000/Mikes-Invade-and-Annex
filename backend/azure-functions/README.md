# Game API

The HTTP API the mod calls (`IA_ApiHandler.c`). It replaces the original
`invadestats` function app, whose source and Azure account were lost. Keep this
folder as the only copy of the code: deploy from here, never edit in the portal.

Base URL: `https://invade-annex-api.azurewebsites.net/api`

| Route | Database call |
| --- | --- |
| `POST /registerServer` | `register_server` |
| `POST /submitStats` | `api_submit_stats` |
| `GET /getAllLeaderboards?serverGuid=` | `api_get_all_leaderboards` |
| `GET /leaderboard?serverGuid=&board=&sort=&dir=&offset=&limit=&playerId=` | `api_get_leaderboard` |
| `POST /submitTransport` | `submit_transport_batch` |
| `POST /getTransportRatings` | `get_transport_ratings` |
| `POST /sync` | `api_sync` |

The current build makes one `/sync` exchange a minute while it has something to
send or a player looking at a board, and none when idle. It asks `/leaderboard`
only for what a sync does not carry: another sort order, a page below the first
hundred rows, a player's rank in another order. A build that finds no `/sync`
uses `/submitStats`, `/submitTransport`, `/getTransportRatings` and
`/leaderboard` as before; every one of those routes answers as it always did.

`getAllLeaderboards` is the legacy route: three whole boards in one answer.
Older mod builds still call it, so it stays.

Request and response bodies: `API_DOCUMENTATION.md`. Every route is anonymous,
because the game sends no credentials. Writes are accepted only for a
registered, active server GUID; anything else gets
`403 Invalid or inactive server GUID.`

Responses must stay compact JSON (`JSON.stringify`). The game finds
`"serverGuid":"` and `"<name>Leaderboard":[` by string search, so a space after
the colon breaks released builds.

## Server name

`servers.name` is the last real name a server reported. The server GUID alone
selects the row a request writes; the name only labels that row and can never
redirect a write.

- `POST /registerServer` makes a new row with the name it is given, trimmed and
  cut to 255 characters. A blank name is refused with `400`. The game's
  placeholder is accepted here: a new row has no better name to lose.
- `POST /submitStats` carries `serverName` with every batch, and
  `api_submit_stats` decides what to keep
  (`backend/supabase/migrations/20261002000000_server_name_retention.sql`):

| `serverName` in the batch | Stored name |
| --- | --- |
| absent, not a string, empty or only whitespace | kept |
| a placeholder, while the stored name is a real one | kept |
| a placeholder, while the stored name is also a placeholder | replaced |
| anything else | replaced, trimmed and cut to 255 characters |

A placeholder is any name containing `PLEASE RENAME IN server_name.txt`, in any
letter case. Both defaults the game has written into `server_name.txt` carry
it, and so does a name whose owner replaced only the first words.

`last_seen` moves on every accepted batch, whether or not the name does.

The original `invadestats` app (see Older builds) updates `servers.name` itself
on every batch and does not go through this rule, so a server on an old build
still stores whatever its file says.

On the `servers` board a name is cut to 48 characters, and a server whose name
is exactly one of the defaults is left out. A half-replaced placeholder is
still shown: it is a real server with real stats, and its live name replaces
the placeholder once it runs a build that sends one.

## `GET /leaderboard`

One page of one board in one sort order, plus the asking player's own row. An
answer does not grow with the number of players. The first hundred rows by
score come with `/sync`; this route serves the rest.

| Query | Values | Default |
| --- | --- | --- |
| `serverGuid` | the asking server's GUID | required |
| `board` | `global`, `server`, `servers` | `global` |
| `sort` | `score`, `kills`, `deaths`, `kd`, `hvt`, `guard`, `obj`, `transport`, `insertions`, `players` | `score` |
| `dir` | `asc`, `desc` | `desc` |
| `offset` | rows to skip, 0 to 1000000 | `0` |
| `limit` | rows wanted, 0 to 100 | `25` |
| `playerId` | identity id whose own row is wanted, 1 to 64 characters | none |

- The session board is not served here. The game server builds it itself.
- `server` is the asking server's players. `servers` has one row per active
  server that has player stats; servers still on a default name are left out.
- `kd` sorts by kills / max(deaths, 1). A row has no such field.
- An `offset` or `limit` above its cap is cut to the cap. One that is not a
  whole number takes the default.
- `limit=0` returns `total` and `me` with no rows.
- A `playerId` of any other length is treated as absent. On the `servers`
  board it is ignored and `me` is the asking server's row.

Response:

```
{"board","sort","dir","offset","total","rows":[row],"me":[row] or []}
```

`board`, `sort`, `dir` and `offset` repeat what was asked; the game drops an
answer that does not match the request it has open. `total` is the number of
rows on the board. `me` holds one row wherever it ranks, or nothing when no
`playerId` was sent or the player is not on the board.

A row has one-letter keys, because a page repeats them for every row:

| Key | Meaning |
| --- | --- |
| `r` | rank in the requested sort, from 1 |
| `n` | player name, or server name on `servers`; at most 48 characters |
| `k` | kills |
| `d` | deaths |
| `h` | HVT kills |
| `g` | HVT guard kills |
| `o` | objective score |
| `s` | score |
| `t` | transport rating: global on `global`, earned on the asking server on `server`, summed over a server's players on `servers` |
| `i` | insertions |
| `p` | players: servers the player has stats on (`global`), 1 (`server`), players on the server (`servers`) |

Numbers are capped at 2000000000, since the game reads 32-bit integers. Rows
carry no player ids. Ties are broken by score, then by id, so a page is the
same between calls.

Errors, as plain text:

- `403 Invalid or inactive server GUID.` when `serverGuid` is missing, not a
  UUID, unknown or inactive.
- `400 board must be one of: global, server, servers.`
- `400 sort must be one of: score, kills, deaths, kd, hvt, guard, obj, transport, insertions, players.`
- `400 dir must be asc or desc.`

## `POST /sync`

Everything a game server sends and reads, in one call. Every part but
`serverGuid` is optional, and each part reports its own result: one that fails
is undone alone and the others stand. The server GUID alone selects the rows
that are written; nothing else in the body can.

Request, a JSON object:

| Member | Value | Bound |
| --- | --- | --- |
| `serverGuid` | the sending server's GUID | required |
| `serverName` | its current name; the rule under Server name decides what is kept | first 1024 characters read |
| `matchData` | events, as `/submitStats` takes them | at most 5000 |
| `statsBatchId` | id of this stats batch: new for each batch, repeated when the same batch is sent again | 1 to 64 characters |
| `transport` | `{"batchId","entries":[{"playerId","playerName","points","insertions"}]}`, as `/submitTransport` takes it | id 1 to 64 characters, at most 512 entries |
| `ratingIds` | identity ids whose transport ratings are wanted | first 256, each 1 to 64 characters |
| `players` | identity ids whose own lines are wanted | first 128 different ones, each 1 to 64 characters |
| `boards` | `[{"board","etag"}]`: the snapshots wanted and the etag held of each, `""` when none is held | `server`, `global`, `servers`, once each; etag at most 64 characters |

A stats or transport part over its bound, or with a batch id that is not a
string of 1 to 64 characters, is `rejected` whole and the rest of the call still
runs. Ids outside their length and unknown boards are dropped. Members not
listed are ignored.

Response, compact JSON of numbers, strings and lists of flat objects:

| Member | Value |
| --- | --- |
| `status` | `ok` |
| `statsStatus` | `none` nothing sent, `accepted` stored, `duplicate` this batch id was already stored, `rejected` cannot be stored as sent, `failed` not stored |
| `statsPlayers` | players the batch touched; on `duplicate`, the count of the first time |
| `transportStatus` | the same five values |
| `transportPlayers` | players the batch touched; 0 on `duplicate` |
| `ratingsStatus` | `none` not asked, `ok`, `failed` |
| `ratings`, `skins` | as `/getTransportRatings` answers; ratings include a transport batch sent in the same call |
| `boardsStatus` | `none` not asked, `ok`, `failed` |
| `boards` | `[{"board","etag","total","unchanged"}]` for each board asked; `unchanged` is 1 when the etag sent is still current, else 0 |
| `serverRows`, `globalRows`, `serversRows` | the first `snapshotRows` rows by score, high to low, in the `/leaderboard` row shape; empty when the board is unchanged or was not asked |
| `own` | own lines: a `/leaderboard` row with `board` and `id` added |
| `snapshotRows` | 100 |
| `nextSyncSeconds`, `pageBudgetPerHour`, `pageBudgetBurst`, `serverPageSeconds`, `globalPageSeconds` | hints, see Settings |

- A batch is stored once per `(serverGuid, batch id)`. After `accepted` or
  `duplicate` the game forgets the batch. After `failed`, or a `500`, it sends
  the same batch again under the same id. After `rejected`, sending it
  unchanged will not help. Stats batch ids are kept for seven days. Transport
  batch ids are the ones `/submitTransport` keeps, so a batch sent on either
  route counts once. A stats batch with no id is counted every time it is sent.
- `own` holds, for each id in `players`, the player's line on this server's
  board (`"board":"server"`) and on the global board (`"board":"global"`), where
  the player is on them, with `id` the player's id. When the `servers` board is
  asked it also holds this server's line (`"board":"servers"`, `"id":""`). Own
  lines are ranked by score and come whether or not a board changed. The rank
  in another order is `/leaderboard` with `limit=0`.
- The `server` board is read live and already holds a stats batch sent in the
  same call. `global` and `servers` are at most one refresh interval behind.
- An etag covers the snapshot's rows, not `total`, which is always current.
- With `failed` boards, `boards`, the row lists and `own` are all empty; the
  game keeps what it holds.

Request and response, shortened:

```
{"serverGuid":"<guid>","serverName":"My Server","statsBatchId":"s-17",
 "matchData":[{"eventType":"PlayerKill","killerPlayerId":"p1","killerPlayerName":"One"}],
 "transport":{"batchId":"t-9","entries":[{"playerId":"p1","playerName":"One","points":20,"insertions":1}]},
 "ratingIds":["p1"],"players":["p1"],
 "boards":[{"board":"server","etag":""},{"board":"global","etag":"9f0c..."},{"board":"servers","etag":"41ab..."}]}

{"status":"ok","statsStatus":"accepted","statsPlayers":1,"transportStatus":"accepted","transportPlayers":1,
 "ratingsStatus":"ok","ratings":[{"rating":20,"playerId":"p1","insertions":1}],"skins":[{"key":"...","required":100}],
 "boardsStatus":"ok","boards":[{"board":"server","etag":"c21d...","total":1,"unchanged":0},
 {"board":"global","etag":"9f0c...","total":17487,"unchanged":1},{"board":"servers","etag":"41ab...","total":90,"unchanged":1}],
 "serverRows":[{"d":0,"g":0,"h":0,"i":1,"k":1,"n":"One","o":0,"p":1,"r":1,"s":1,"t":20}],"globalRows":[],"serversRows":[],
 "own":[{"d":0,"g":0,"h":0,"i":1,"k":1,"n":"One","o":0,"p":1,"r":1,"s":1,"t":20,"id":"p1","board":"server"},
 {"d":0,"g":0,"h":0,"i":1,"k":1,"n":"One","o":0,"p":1,"r":9120,"s":1,"t":20,"id":"p1","board":"global"},
 {"d":4,"g":0,"h":0,"i":1,"k":9,"n":"My Server","o":0,"p":3,"r":61,"s":9,"t":20,"id":"","board":"servers"}],
 "snapshotRows":100,"nextSyncSeconds":60,"pageBudgetPerHour":12,"pageBudgetBurst":20,"serverPageSeconds":120,"globalPageSeconds":300}
```

Errors, as plain text:

- `400 The body must be a JSON object.`
- `403 Invalid or inactive server GUID.` Nothing is written.
- `501 Sync is not available.` when the database has no `api_sync`. An app
  deployed before this route answers `404` with an empty body. On either, the
  game uses the older routes.
- `500 Internal error.` The game sends the same batches again under the same
  ids, which is harmless whether or not they were stored.

## Caches

Database (`backend/supabase/migrations/20261003000000_leaderboard_cache_and_sync.sql`):
the `global` and `servers` boards are kept in `leaderboard_cache`, with the
total, the first hundred rows and their etag in `leaderboard_cache_state`. A
call that reads a board rebuilds it first when it is older than
`ia_board_refresh_seconds()` (60; change it in that one function). The rebuild
reads the source tables again and writes only the rows that differ, so nothing
depends on totals being kept in step, and the old `invadestats` app writing to
the same tables is picked up too. One caller at a time rebuilds, under an
advisory lock; the others answer from the board as it stands. There is no
scheduled job. The `server` board is always read live.

Function app (`src/cache.js`, `src/handlers.js`), per instance and lost on a
restart:

- `/leaderboard` pages of `global` and `servers`, keyed by board, sort,
  direction, offset and limit, without the own row. The asking player's row is
  read fresh on every call and is never held. `server` pages are never held.
- The last `global` and `servers` snapshots `/sync` read. The database is
  shown their etags, so the rows leave it once per change, not once per server.
- Server GUIDs the database has called active. Only a positive answer is held,
  and only to let a held page be served without a database call. An unknown or
  inactive GUID is asked of the database every time, and a newly registered
  server works at once.

Nothing held is keyed by a server or a player, and bodies are never logged.

## Settings

App settings, all optional. A value that is not a whole number takes the
default; one outside its range is brought to the nearest end.

| Setting | Default | Range | Meaning |
| --- | --- | --- | --- |
| `PAGE_CACHE_SECONDS` | 60 | 0 to 600 | life of a held `/leaderboard` page; 0 turns the page cache off |
| `PAGE_CACHE_ENTRIES` | 500 | 0 to 5000 | pages held per instance |
| `ACTIVE_SERVER_CACHE_SECONDS` | 60 | 0 to 300 | how long an active GUID is trusted for a held page |
| `SYNC_INTERVAL_SECONDS` | 60 | 30 to 3600 | hint `nextSyncSeconds`: seconds between syncs |
| `PAGE_BUDGET_PER_HOUR` | 12 | 0 to 3600 | hint `pageBudgetPerHour`: `/leaderboard` calls a server may make an hour |
| `PAGE_BUDGET_BURST` | 20 | 0 to 500 | hint `pageBudgetBurst`: calls it may save up |
| `SERVER_PAGE_CACHE_SECONDS` | 120 | 30 to 3600 | hint `serverPageSeconds`: how long the game holds a page of its own board |
| `GLOBAL_PAGE_CACHE_SECONDS` | 300 | 60 to 3600 | hint `globalPageSeconds`: how long it holds a `global` or `servers` page |

The hints go out with every `/sync` answer, so a change reaches every server
within a minute of the app setting changing, with no mod release. The game
obeys them within its own limits.

Budget, from 4,123 active server-minutes a day: about 125,000 syncs a month,
plus at most 12 pages an hour over about 2,090 active server-hours, 25,000.
That is 150,000 executions against the 250,000 of the free plan. Every 12 added
to `PAGE_BUDGET_PER_HOUR` allows 25,000 more.

## Hosting

- Subscription `Azure subscription 1`, resource group `rg-invade-annex-api`, region `eastus2`.
- Function app `invade-annex-api`: Flex Consumption, Node 22, 512 MB instances.
- Storage account `stinvadeannexapi` and Application Insights `invade-annex-api`.

## Database access

The app connects straight to Postgres through the Supabase transaction pooler
as the `ia_game_api` role. That role has no table privileges and can only
execute the seven functions above
(`backend/supabase/migrations/20260930010000_game_api.sql`,
`20261002000000_server_name_retention.sql` for the current `api_submit_stats`,
and `20261003000000_leaderboard_cache_and_sync.sql` for the current
`api_get_leaderboard` and for `api_sync`). The cache tables are written only
inside those functions. It uses no Supabase API key.

App settings: `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`.

To rotate the password, set a new one in the Supabase SQL editor and in the app
setting, in that order:

```
ALTER ROLE ia_game_api PASSWORD '<new password>';
az functionapp config appsettings set -g rg-invade-annex-api -n invade-annex-api --settings "PGPASSWORD=<new password>"
```

## Test

```
npm install
npm test
```

`test/handlers.test.js` and `test/cache.test.js` run the handlers, the memory
cache and the settings against a stubbed database. The others apply every file
in `backend/supabase/migrations` to an in-process Postgres (PGlite, a dev
dependency) and call the real functions:

- `test/server-name.test.js`: the name rule.
- `test/leaderboard-cache.test.js`: every board, sort, direction and page
  equals the answer of the function it replaces; refresh interval and lock;
  etags; grants; the migration run twice; the rollback.
- `test/sync.test.js`: each part of `/sync` alone and together, batches sent
  twice, one part failing, bounds, the GUID as the only write selector, the
  older routes.
- `test/dev-server.test.js`: the dev server, and that nothing under `src/`
  reaches it.

Each builds the two tables and `register_server` itself, because those were
made by hand in the Supabase project and have no migration. Without dev
dependencies the database suites are skipped.

`npm run measure` prints the database time and answer size of the leaderboard
and sync calls before and after the cache migration, on the seeded dev
database. See `dev/measure.js` for timing a Postgres on this machine instead.

## Local dev server

```
npm install
npm run dev
```

The real handlers on `http://127.0.0.1:7071/api/<route>`, over an in-process
Postgres with every migration applied. Nothing leaves the machine and no
credentials are needed. It is up in about three seconds.

- Seed: made up and the same on every start. 20,000 players over 100 servers
  (31,552 stat rows, the largest server 2,492 players), 3,057 transport
  ratings, names longer than 48 characters, empty player names, servers on
  both default names and on a half-replaced one, one inactive server.
- Any well-formed GUID it has not seen becomes an active server on first use,
  with a copy of one seeded server's players, so a game or Workbench instance
  can point at it with the GUID it already has. A GUID that is not a UUID is
  refused as in production.
- It logs one line a request: method, path, status, milliseconds, bytes in and
  out. Never a body or a query string.
- `GET http://127.0.0.1:7071/dev/calls` counts the calls per route since the
  start or the last reset: `{"since","total","adoptedServers","routes":[{"route",
  "calls","ok","failed","requestBytes","responseBytes","lastStatus"}]}`.
  `POST /dev/calls/reset` starts the count again.

| Variable | Default | |
| --- | --- | --- |
| `IA_DEV_PORT` | 7071 | port, on 127.0.0.1 only |
| `IA_DEV_PLAYERS` | 20000 | players seeded |
| `IA_DEV_SERVERS` | 100 | servers seeded |
| `IA_DEV_REFRESH_SECONDS` | 60 | board refresh interval of this database |

The settings under Settings apply too. `dev/` and `test/` are not part of a
deployment: the zip below is made from `src/`, and `.funcignore` leaves them
out of one made with the Functions tools.

## Deploy

The database and the function app are deployed separately, and either order
works: each version of one runs against each version of the other. Database
first is the order to use, so that `/sync` works from the moment it exists.

1. Database: run the new file from `backend/supabase/migrations` in the
   Supabase SQL editor of the project, as it is. Each file is one transaction
   and can be run again.
2. Check, in the SQL editor:
   `SELECT board, total, refreshed_at, refresh_ms FROM public.leaderboard_cache_state;`
   shows two rows, and a `/leaderboard` call from a game server still answers.
3. Function app: deploy as below.
4. Check: `POST /sync` with `{"serverGuid":"<an active GUID>"}` answers `200`
   with every status `none`.

What a game server sees in between:

| Database | Function app | `/leaderboard` and the older routes | `/sync` |
| --- | --- | --- | --- |
| old | old | as before | `404`, empty body |
| new | old | as before, from the cache tables | `404`, empty body |
| old | new | as before | `501 Sync is not available.` |
| new | new | as before | `200` |

`20261003000000_leaderboard_cache_and_sync.sql` adds three tables
(`leaderboard_cache`, `leaderboard_cache_state`, `stats_batches`), replaces
`api_get_leaderboard` in place with the same signature and answer, adds
`api_sync`, and builds both boards once. It changes no existing table or row.
To roll it back, run
`backend/supabase/rollbacks/20261003000000_leaderboard_cache_and_sync.sql`: it
puts back the `api_get_leaderboard` of `20261001030000_leaderboard_pages.sql`
and drops what the migration added. It can be run with either version of the
function app deployed, and twice. Stats are not lost: the dropped tables hold
only the boards, which are rebuilt from the source tables, and the stats batch
ids of the last seven days. A stats batch sent again across a rollback is the
one case that could then be counted twice.

`20261002000000_server_name_retention.sql` replaces `api_submit_stats` in place
and keeps its grants. To roll it back, run the
`CREATE OR REPLACE FUNCTION public.api_submit_stats` statement from
`20260930010000_game_api.sql`.

Function app: leave the dev dependencies out, then zip `host.json`,
`package.json`, `src/` and `node_modules/` (forward-slash paths):

```
npm ci --omit=dev
az functionapp deployment source config-zip -g rg-invade-annex-api -n invade-annex-api --src api.zip
```

Run `npm install` afterwards to get the test dependencies back. To roll back,
check out the previous commit of this folder and deploy it the same way; no
database change is needed first.

## Older builds

Builds released before this API call the original app at
`invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net`. It writes to the same
tables, so both can run side by side: each game server uses one or the other.
That app holds a Supabase service key; rotating the key in Supabase cuts it off.
