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
(`backend/supabase/migrations/20260930010000_game_api.sql`, and
`20261001030000_leaderboard_pages.sql` for `api_get_leaderboard`). It uses no
Supabase API key.

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

## Deploy

Zip `host.json`, `package.json`, `src/` and `node_modules/` (production
dependencies only, forward-slash paths), then:

```
az functionapp deployment source config-zip -g rg-invade-annex-api -n invade-annex-api --src api.zip
```

To roll back, check out the previous commit of this folder and deploy it the
same way.

## Older builds

Builds released before this API call the original app at
`invadestats-awatbsduh4hngrb6.eastus-01.azurewebsites.net`. It writes to the same
tables, so both can run side by side: each game server uses one or the other.
That app holds a Supabase service key; rotating the key in Supabase cuts it off.
