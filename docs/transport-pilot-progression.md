# Transport pilot progression

Helicopter pilots earn a **global transport rating** for flying players into the
AO. The rating unlocks helicopter skins. The first skin is a desert-tan Huey.

The rating is one number per player across every server. It is stored in
Supabase, not in a server profile and not in the session.

## What counts as an insertion

One passenger, one credit, when all of these hold (`IA_TransportPilotTracker`,
rules in `IA_TransportScoring`):

- The passenger is a living, conscious friendly player who rode in a helicopter
  with a player in the pilot seat. Credit goes to the last player who piloted
  while they were aboard. A pilot never earns from their own seat.
- The helicopter was intact when the passenger left it. Survivors of a
  shoot-down are not insertions.
- The passenger reached the ground alive within 120 s and did not board another
  vehicle first.
- The dropoff is at least 300 m (horizontal) from where they boarded.
- The dropoff is within 1000 m of the edge of an objective circle in the
  active AO group.
- That passenger has not been credited to anyone in the last 120 s. The
  cooldown is keyed by player identity, so a reconnect does not reset it.

Points per insertion are `round(10 x weight)`:

| Dropoff distance from the nearest objective circle | Weight | Points |
| --- | --- | --- |
| Inside, or up to 150 m outside | 3.0 | 30 |
| 150 m to 1000 m | linear 3.0 down to 1.0 | 30 to 10 |
| Beyond 1000 m | 0 | none |

Each point is also one session XP for the pilot.

The tan Huey needs 50000 rating. That is meant to take dozens of hours: a full
cabin of 12 dropped inside the objective pays 360, so the fastest possible path
is 139 such trips, and ordinary part-full flights take several times longer.

## Global storage

```
game server --POST /submitTransport------> invade-annex-api (Azure) --> submit_transport_batch
game server --POST /getTransportRatings--> invade-annex-api (Azure) --> get_transport_ratings
```

Schema and RPCs: `backend/supabase/migrations/20260930000000_transport_rating.sql`.
API source: `backend/azure-functions`.

- `player_transport_ratings`: one row per `player_bohemia_id` with `rating` and
  `insertions`. No server column; every registered server adds to the same row.
- `transport_batches`: one row per accepted batch, keyed `(server_guid, batch_id)`.
  A resent batch is a no-op, so a lost HTTP response cannot double-count. The
  stored entries are the audit trail if a server has to be reversed.
- `transport_skin_thresholds`: the central unlock setting, `skin_key` to
  `required_rating`. Changing a row changes the requirement on every server the
  next time it fetches ratings. The value in `IA_HeliSkinCatalog` is only the
  default used until the first fetch.
- `submit_transport_batch` refuses unknown or inactive servers and caps each
  entry at 30 points per insertion, the most the scoring rules can pay.

### Azure function contract

Both routes are thin pass-throughs to the RPCs, called as the `ia_game_api`
database role. An unknown or inactive server gets 403.

`POST /submitTransport`

```json
{
  "serverGuid": "uuid",
  "batchId": "string, at most 64 characters, unique per server",
  "entries": [
    { "playerId": "bohemia id", "playerName": "string", "points": 40, "insertions": 2 }
  ]
}
```

Call `submit_transport_batch(p_server_guid, p_batch_id, p_entries)`. Return 2xx
when the RPC returns, including `applied: false` for a duplicate. Return non-2xx
when it raises; the game then resends the same batch later.

`POST /getTransportRatings`

```json
{ "serverGuid": "uuid", "playerIds": ["bohemia id", "..."] }
```

Call `get_transport_ratings(p_player_ids)` and return its JSON unchanged:

```json
{
  "ratings": [ { "playerId": "bohemia id", "rating": 50310, "insertions": 2140 } ],
  "skins":   [ { "key": "huey_tan", "required": 50000 } ]
}
```

Players with no row are left out of `ratings`; the game treats them as 0.

## Game side

`IA_TransportPilotStore` is a RAM cache of the global totals.

- A player's rating is fetched the first time they pilot a helicopter or carry
  a passenger. Until the answer arrives the rating is **unknown**, and unknown
  never unlocks anything: with the backend down, nobody gains a skin.
- Earned points are queued and sent every 60 s. Until the backend acknowledges
  them they count on top of the fetched total, so a pilot who crosses the
  threshold mid-session gets the skin without waiting.
- Failed requests back off from 60 s up to 30 min.
- Points still unsent when the server process stops are lost (at most about one
  minute of flying). Nothing is written to disk.

## Skins

`IA_HeliSkinCatalog` lists skins; `IA_HeliSkinManagerComponent` (on
`GameMode_IA.et`) replicates which vehicle wears which skin and each client
repaints the matching material slots with `$remap`. The prefab is untouched, so
seats, catalog labels and pilot-role checks are unaffected.

When a pilot whose rating meets the threshold takes the pilot seat, the
airframe is painted. It stays painted for the life of that vehicle, including
after a different pilot takes over.

To add a skin: add an `.emat`, one `AddDef` line in `IA_HeliSkinCatalog` with a
new id and key, and one `transport_skin_thresholds` row.

## Pilot HUD

The pilot sees one card (`IA_PilotHud`), docked top-right under the rank chip.
It is not a toast and never waits behind, or delays, objective toasts.

- **Landing.** Passengers of one landing reach the ground over several seconds.
  The server batches their credits per pilot and reports at most once a second;
  the card merges every update that arrives while it is open. Twelve passengers
  are twelve pips on one card, not twelve messages. The card shows the points
  earned, the troop count, the insertion weight (`x3.0`), `HOT LZ` or the
  distance out, and below that the progress block.
- **Progress block.** A Huey that is painted nose to tail in the skin's colour
  as the rating nears the threshold, with the total, the threshold and the
  percent to one decimal.
- **Taking the pilot seat** shows the progress block alone, at most once every
  120 s per pilot and never within 120 s of a landing card.
- **Unlock.** When the points that cross the threshold are reported, the card
  counts up to it, turns gold, names the skin and plays one sound. It is
  announced once.
- **Total unknown.** If the backend total has not arrived, the card shows the
  points as banked. The total follows when it arrives: merged into the card if
  it is still open, otherwise as a progress card.
- A dead pilot has no HUD. The batch waits up to 60 s for a respawn, then the
  total is shown on the next progress card.

Wire format: the `PilotProgress` message text is a packed
`IA_PilotDropoffPayload`,
`kind|troops|points|rating|required|edge|unlockedRequired|unlockedName|skinName`
(`kind` 0 = landing, 1 = rating only; `rating` -1 = unknown; `-` = no name).
Servers and clients must run the same build. On the legacy text HUD (Mike's UI
failed to mount) updates inside 5 s are merged into one line.

## Not yet proven in game

- The pilot card: layout, fonts, timing, the unlock sound, and the multi-tick
  merge with real passengers. Only its data and rules run in Workbench.
- `SetObject` with `$remap` on a live helicopter and its slotted doors.
- Referencing `IA_UH_1H_Body01_Tan.emat` by path; Workbench has to register the
  file first.
- The tan colour values.
