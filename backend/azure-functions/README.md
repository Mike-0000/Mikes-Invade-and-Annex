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

`getAllLeaderboards` is the legacy route: three whole boards in one answer. The
current build asks `/leaderboard` for a page at a time and no longer calls it.
Older mod builds still do, so it stays.

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

One page of one board in one sort order, plus the asking player's own row. The
game server asks for a page when a player looks at it, so an answer does not
grow with the number of players.

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

## Hosting

- Subscription `Azure subscription 1`, resource group `rg-invade-annex-api`, region `eastus2`.
- Function app `invade-annex-api`: Flex Consumption, Node 22, 512 MB instances.
- Storage account `stinvadeannexapi` and Application Insights `invade-annex-api`.

## Database access

The app connects straight to Postgres through the Supabase transaction pooler
as the `ia_game_api` role. That role has no table privileges and can only
execute the six functions above
(`backend/supabase/migrations/20260930010000_game_api.sql`,
`20261001030000_leaderboard_pages.sql` for `api_get_leaderboard`, and
`20261002000000_server_name_retention.sql` for the current `api_submit_stats`).
It uses no Supabase API key.

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

`test/handlers.test.js` runs the handlers against a stubbed database.
`test/server-name.test.js` applies every file in `backend/supabase/migrations`
to an in-process Postgres (PGlite, a dev dependency) and calls the real
functions through the handlers. It builds the two tables and `register_server`
itself, because those were made by hand in the Supabase project and have no
migration. Without dev dependencies that suite is skipped.

## Deploy

The database and the function app are deployed separately, and either order
works: each version of one runs against each version of the other.

Database: run the new file from `backend/supabase/migrations` in the Supabase
SQL editor of the project, as it is. Each file is one transaction and can be
run again. `20261002000000_server_name_retention.sql` replaces
`api_submit_stats` in place and keeps its grants. To roll it back, run the
`CREATE OR REPLACE FUNCTION public.api_submit_stats` statement from
`20260930010000_game_api.sql`.

Function app: leave the dev dependencies out, then zip `host.json`,
`package.json`, `src/` and `node_modules/` (forward-slash paths):

```
npm ci --omit=dev
az functionapp deployment source config-zip -g rg-invade-annex-api -n invade-annex-api --src api.zip
```

Run `npm install` afterwards to get the test dependencies back. To roll back,
check out the previous commit of this folder and deploy it the same way.

## Older builds

Builds released before this API call the original app at
`invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net`. It writes to the same
tables, so both can run side by side: each game server uses one or the other.
That app holds a Supabase service key; rotating the key in Supabase cuts it off.
