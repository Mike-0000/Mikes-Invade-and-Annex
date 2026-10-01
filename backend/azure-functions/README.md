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
| `POST /submitTransport` | `submit_transport_batch` |
| `POST /getTransportRatings` | `get_transport_ratings` |

Request and response bodies: `API_DOCUMENTATION.md`. Every route is anonymous,
because the game sends no credentials. Writes are accepted only for a
registered, active server GUID; anything else gets
`403 Invalid or inactive server GUID.`

Responses must stay compact JSON (`JSON.stringify`). The game finds
`"serverGuid":"` and `"<name>Leaderboard":[` by string search, so a space after
the colon breaks released builds.

## Hosting

- Subscription `Azure subscription 1`, resource group `rg-invade-annex-api`, region `eastus2`.
- Function app `invade-annex-api`: Flex Consumption, Node 22, 512 MB instances.
- Storage account `stinvadeannexapi` and Application Insights `invade-annex-api`.

## Database access

The app connects straight to Postgres through the Supabase transaction pooler
as the `ia_game_api` role. That role has no table privileges and can only
execute the five functions above
(`backend/supabase/migrations/20260930010000_game_api.sql`). It uses no Supabase
API key.

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
