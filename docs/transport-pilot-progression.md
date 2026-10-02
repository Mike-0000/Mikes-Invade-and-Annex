# Transport pilot progression

Helicopter pilots earn a **global transport rating** for flying players into the
AO. The rating unlocks helicopter liveries: plain military paint colours that
any helicopter with paint channels can wear, vanilla or modded.

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

| Livery | Key | Rating |
| --- | --- | --- |
| Forest Green | `heli_green` | 5000 |
| Field Drab | `heli_drab` | 12000 |
| Gunship Grey | `heli_grey` | 25000 |
| Desert Tan | `huey_tan` | 50000 |
| Night Black | `heli_black` | 100000 |

Desert Tan is meant to take dozens of hours: a full cabin of 12 dropped inside
the objective pays 360, so the fastest possible path is 139 such trips, and
ordinary part-full flights take several times longer. The cheaper liveries are
steps on the way; Night Black is for the pilot who kept going.

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
  next time it fetches ratings. The value in `IA_HeliSkinCatalog` is the
  default: it is used until the first fetch, and for good by a livery that has
  no row. Only `huey_tan` has a row today; the other four run on their script
  defaults until a row is added for them.
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

- A player's rating is fetched the first time they pilot a helicopter, carry
  a passenger or walk up to a parked helicopter that has a skin. Until the answer arrives the rating is **unknown**, and unknown
  never unlocks anything: with the backend down, nobody gains a skin.
- Earned points are queued and sent every 60 s. Until the backend acknowledges
  them they count on top of the fetched total, so a pilot who crosses the
  threshold mid-session gets the skin without waiting.
- Failed requests back off from 60 s up to 30 min.
- Points still unsent when the server process stops are lost (at most about one
  minute of flying). Nothing is written to disk.

## Liveries

A livery is put on a helicopter **where it stands**: crew aboard, engine
running, as often as wanted. The helicopter is never deleted, respawned or
given a new mesh. Everything goes through
`IA_HeliSkinManagerComponent.SetVehicleSkin(vehicle, skinId)`.

A livery is **one plain colour** (`IA_HeliSkinCatalog`). It is not a file: the
same five liveries go on every helicopter family, and a new one is one line.
Patterns are not offered. A camouflage texture is laid out for one helicopter's
UVs, and neither a projected pattern nor run-time decals gave a result that
looked right on more than one airframe (`IA_HeliCamoProbe`; see the vehicle
paint rule). A plain colour looks right on all of them.

### How it works

- Changing a vehicle hull's mesh from script (`SetObject`,
  `SetVObjectFromPrefab`) crashes the engine a frame later, so that is never
  done. Instead the colours of the **material itself** are changed with
  `Material.SetParam`, which every mesh using that material shows at once.
- A material is shared by every entity that names it, so recolouring a stock
  hull material would recolour every helicopter of that type. A **paint
  channel** is a prefab twin of a stock airframe whose hull and slotted parts
  name their own copies of the paint materials. The copies inherit the stock
  materials and set nothing, so a channel airframe looks stock.
- A **family** is one helicopter type: its stock airframes, the parts that show
  paint, its paint materials ("surfaces") and, per surface, a **recipe**: which
  parameters take the livery's colour (`primary`, with a gain, for a material
  built of colour layers; `tint` for one whose paint is in its texture), which
  take a fixed colour or number while a livery is on, and what each is in stock
  paint.
  Families live in `tools/heli_paint_families.json`. Two ship: `uh1h` (four
  Hueys; body and two interior materials) and `mi8mt` (four Mi-8MTs with seven
  slotted parts; one body material, whose camouflage layers all take the
  livery colour).
- `tools/author_heli_paint_channels.py` writes, from that registry, twelve
  channels per family (`.../Paint/` under `Assets/` and `Prefabs/`) and the
  script manifest `IA_HeliPaintManifest`, which registers the families with
  `IA_HeliPaintChannels`. Channel numbers are global: family index x 12 + 1..12,
  in the order the manifest registers them, the same on every machine.
- Each twin carries `IA_HeliPaintRigComponent`, which tells the server its
  channel is held and gives it back when the helicopter is deleted. Whatever
  spawns a stock airframe of a family asks
  `IA_HeliPaintRigComponent.ResolveSpawnPrefab` first and spawns the twin on a
  free channel of that family instead: a helicopter pad (`IA_VehicleRespawner`)
  and the editor, for Game Master and build mode (`IA_HeliPaintEditorSpawn`).
  An empty channel is taken before one whose helicopter is a wreck. With all
  twelve of a family held the server logs one warning and further ones spawn
  stock and cannot be repainted.
- `IA_HeliSkinPaint.Apply(channel, skinId)` sets the recipe on the channel's
  materials. No material file is read at run time.
- `IA_HeliSkinManagerComponent` (on `GameMode_IA.et`) replicates which livery
  each channel shows. Every machine that renders paints its own materials when
  the value arrives, and on a timer for a player who joins later. A new
  airframe starts in stock paint.

Decals (stars, numbers), glass, rotors and weapons keep their stock look. A
helicopter of no family, and a stock one that was not spawned by a pad or the
editor (saved in the world, or spawned by other code), has no paint channel and
cannot be repainted: giving it one would mean replacing it.

### How a pilot gets it

From the pilot's seat, in the paint bay (below). Before that,
`IA_HeliSkinPadService` runs once a second on the server: for a helicopter
still on its pad, it takes the nearest living friendly player on foot within
10 m and paints the livery they last chose in the bay this session, or the
highest one they have unlocked when they have not chosen. An unknown rating is
requested and unlocks nothing.

- Nothing else about the helicopter changes: fuel, damage and inventory stay.
- The pad service never trades a livery down, and never paints over one a pilot
  chose from the seat. A replacement airframe spawns in stock paint.

### Adding a livery

One `AddDef` line in `IA_HeliSkinCatalog`: a new id constant, a key, a name, a
default threshold, the paint as a **linear** colour and a swatch in sRGB bytes
for the UI. Keep it dark and close to grey; `tools/test_transport_pilot.py`
rejects a bright or saturated paint. No file, prefab or material is needed, and
every family takes it. Add a `transport_skin_thresholds` row only to override
the default centrally. Look at it on every family with the livery sheet below.

### Adding a helicopter, vanilla or modded

The helicopter's addon must be loaded, so the mod has to be a dependency of
the addon the twins are written into. `addon.gproj` is the developer's to edit.
For a helicopter that should stay optional, put the family in a small
compatibility addon that depends on both (last step).

1. **Survey.** Run the Workbench plugin `IA_HeliPaintSurvey` from the command
   line with `-iaSurveyPrefab <stock prefab>[,<variant>...]` and
   `-iaSurveyOut <name>.json`. It reads the prefabs' ids, mesh slots, slotted
   parts, what each material file sets and which textures it names, and writes
   the JSON to the Workbench profile folder. It changes nothing.
2. **Adopt.** `python tools/author_heli_paint_channels.py --adopt <survey.json>
   --family <key> --name "DISPLAY NAME" --stock-paint "Factory Camo" --art
   <key>`. It adds the family to the registry with a draft recipe. Without
   `--surfaces` it guesses the hull paint from the material names (`Exterior`,
   `Body`, `Hull`...) and prints its guess: check it. `--surfaces Stem,Stem`
   names the paint materials by hand, a slotted part's too (pylons, doors), and
   takes every part that shows one; `--parts` limits the parts. A paint
   material is one of two kinds:
   - **Layered** (`MatPBRMulti` with `Color_n`): the draft has the first layer
     of the first surface taking the livery colour. Steps 3 and 4 finish it.
   - **Tinted** (`MatPBRBasic`): the paint is in the texture, and the material
     multiplies it by `Color`. The recipe is `{"Color": {"tint": [r, g, b]}}`,
     the texture's own paint colour (linear); a livery sets `Color` to its
     colour over that. With `--textures <the mod's unpacked folder>` the tool
     measures it from the texture (the commonest colour, leaving out the grey
     and black that fill unused space; needs Pillow and lz4). Without it the
     draft holds a placeholder to replace by hand. Skip to step 5.
3. **Find the layers** (layered only). `IA_HeliSkinLiveProbe -iaSkinMode 5
   -iaSkinPrefab <stock prefab> -iaSkinMaterial <emat[,emat]>` paints each
   colour layer of a material in a loud colour on its own airframe and takes a
   screenshot, so it is plain which `Color_n` is the hull, which is trim and
   which is not paint. Add `-iaSkinTilt 65` to tip the roofs toward the camera:
   a layer can cover only the top of the hull (the Huey's roof walkway is
   `Color_4`), and the side views do not show it.
4. **Write the recipe** in the registry, per surface under `paint`:
   `{"primary": 1}` for a layer that takes the livery colour (the number is a
   gain: each layer has its own texture, and a dark one needs a high gain to
   look like the hull, 9 on the Huey's walkway, which also takes
   `Roughness_4` 3 to lose its sheen),
   `{"color": [r, g, b]}` for a fixed colour while a livery is on, and
   `{"value": n}` for a number. Give a wear layer the livery colour times its
   stock ratio to the hull (1.55 on the Huey's `Color_2`) and leave a bare
   metal layer out (the Huey's `Color_3`: rotor head and skids): fixed dark
   colours there dull the whole helicopter. Set `Specular` / `SpecularIBL` to a neutral
   value if the stock material tints them (the Mi-8's khaki sheen shows through
   dark paint otherwise). Set `stock_swatch` to the stock colour in sRGB bytes.
   A refresh (`--adopt` on a family the registry has) keeps the recipe.
5. **Generate.** `python tools/author_heli_paint_channels.py`, then open
   Workbench once so it imports the files, and run `IA_HeliSkinAssetCheck`
   until it prints `PASS`.
6. **Look.** `IA_HeliSkinLiveProbe -iaSkinMode 6 -iaSkinFamily <key>` stands
   one airframe per livery in a row and takes a screenshot (`106` with the
   engine running; `-iaSkinAirframe <index>` picks a variant). Run it once more
   with `-iaSkinTilt 65` and look at the roofs.
   `IA_HeliPaintMenuProbe -iaMenuFamily <key>` shows its paint bay.
7. **Silhouette (optional).** `art` picks the paint bay's side view: `huey`,
   `hip`, anything else the generic helicopter. A new one is one `IA_HeliArt`
   subclass that lists its polygons, and one line in `IA_HeliArt.Create`.

A compatibility addon keeps its own registry and passes `--registry <file>
--root <its folder>`. Its registry's `manifest` is
`{"path": "Scripts/Game/XX_HeliPaintManifest.c", "modded": true}`, which writes
a `modded class IA_HeliPaintManifest` that adds its families after the built-in
ones. Server and clients must load the same addons in the same order, as for
any mod, so the channel numbers agree. The addon holds only the registry, the
generated twins (which inherit the mod's prefabs and materials and copy
nothing) and the manifest script.

The first one is `F:\Mikes-Invade-and-Annex-H60-Paint-Exp`, for the Sikorsky
H-60 mod (`uh60`: four airframes on two meshes, four tinted hull materials and
a pylon part): asset check `PASS`, all five liveries in play mode with the
engines running, and the paint bay. This addon holds nothing about a modded
helicopter and does not depend on a compatibility addon; its registry lists
the vanilla Huey and Mi-8 only.

What a tint cannot do, since it multiplies the texture:

- Markings, wear and panel shading painted into the texture stay, tinted with
  it. Lettering in the stock paint's darker shade stays readable; a white
  marking takes the tint's hue.
- A texture with a camouflage pattern keeps the pattern. Only a layered
  material can be painted flat.
- One multiplier serves a whole material, so parts of one texture painted in
  different colours stay different.
- A wrong paint colour shows as a hull material off-hue from its neighbours
  (the H-60's engine cowl came out lavender while its measured colour was
  off). Look at the livery sheet and correct the `tint` in the registry.

What can go wrong with a modded helicopter, and what the tools say:

- Its paint material is neither layered nor tinted (another class): `--adopt`
  refuses it by name. It cannot be recoloured without the mod's author.
- The guess names the wrong materials (the H-60's interior is layered, its hull
  is not): name them with `--surfaces`.
- Airframes of one family differ (the H-60 ambulance has its own mesh and hull
  materials; only the gunship has pylons): name every variant's materials. The
  asset check asks that each surface shows on some airframe, not on all.
- A prefab already assigns a material to a slot: `--adopt` prints a warning and
  the twin's entry replaces it.
- A part's ids are missing from the survey: `--adopt` prints a warning and that
  part keeps stock paint.

### Looking at a livery alone

In Workbench only: `IA_HeliSkinLiveProbe` paints a helicopter in play mode and
`IA_HeliPaintMenuProbe` shows the bay. In a mission nothing puts a livery on a
helicopter except the rating: there is no admin repaint.

## Paint bay

The pilot of a helicopter (the pilot's seat only, not the co-pilot's) opens the
paint bay with **I**, or by holding **D-pad left** on a gamepad. The key can be
rebound under Controls, Helicopter, as *Paint bay (pilot seat)*. A hint names
the key the first time a pilot sits in a helicopter that can be repainted.

The bay sits along the bottom of the screen so the helicopter stays in view. It
shows the airframe as a blueprint in its family's silhouette, under the family's
name, one tile per livery (stock paint first), and for the livery
pointed at: its name, the pilot's rating against its threshold, and whether it
is on the airframe, ready, locked, or waiting for the rating. Selecting a ready
livery repaints the helicopter in place, on the ground or in flight. The bay
closes with Back, with the key again, or when the pilot leaves the seat.

| Shown | Meaning |
| --- | --- |
| ON AIRFRAME | The livery the helicopter wears now |
| READY TO PAINT | Earned; select it to repaint |
| LOCKED | Rating below the threshold; the bar and the blueprint show how far |
| SYNCING | The rating has not arrived from the backend yet; nothing unlocks |
| UNAVAILABLE | This helicopter has no paint channel (no paint family, not spawned by a pad or the editor, or all twelve of its family were held when it spawned) |

An admin is treated as any other pilot: a livery below its threshold is locked
for them too, in the bay and on the server.

How it works:

- `IA_HeliPaintHotkey` (ticked by `IA_NotificationDisplay`) keeps the input
  context `IA_HeliPilotContext` alive only while the local player is a
  helicopter pilot and opens `IA_HeliPaintMenu`. The action and context are in
  `Configs/System/chimeraInputCommon.conf`.
- The menu asks; the server decides. The client sends only a skin id through
  the player controller (`IA_AskSetHeliSkin`). The server
  (`IA_HeliPaintService.TrySetSkin`) checks the seat, the paint channel and the
  unlock against the rating it holds, then sets the skin through
  `IA_HeliSkinManagerComponent.SetVehicleSkin`. The reply carries the result,
  the rating and the thresholds in force.
- The bay shows the worn livery from the replicated skin manager, so it follows
  the airframe and celebrates only when the repaint has arrived.
- A skin the pilot chose stays until they change it or the helicopter respawns;
  the pad service no longer paints over it.

### Looking at the bay alone

Host a game alone (Workbench play mode or a listen server), take the pilot's
seat of a Huey or an Mi-8 from a pad, or place one from the editor, and press
**I**. Only the liveries the host's rating has earned are selectable; being the
host or an admin unlocks nothing.

The Workbench plugin **IA paint bay menu probe** (`IA_HeliPaintMenuProbe`)
opens the bay over a helicopter in a small world and steps it through every
state (`-iaMenuFamily <key>`; `-iaMenuArt <key>` draws it with another
silhouette). It feeds the menu its state directly, so it shows the drawing and
that the input context exists, not the key in a seat or the server's side.

## Pilot HUD

The pilot sees one card (`IA_PilotHud`), docked top-right under the rank chip.
It is not a toast and never waits behind, or delays, objective toasts.

- **Landing.** Passengers of one landing reach the ground over several seconds.
  The server batches their credits per pilot and reports at most once a second;
  the card merges every update that arrives while it is open. Twelve passengers
  are twelve pips on one card, not twelve messages. The card shows the points
  earned, the troop count, the insertion weight (`x3.0`), `HOT LZ` or the
  distance out, and below that the progress block.
- **Progress block.** A helicopter that is painted nose to tail in the colour
  of the next livery as the rating nears its threshold, with the total, the
  threshold and the percent to one decimal. The card is not told the airframe,
  so the drawing is always the Huey.
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

### Testing the card alone

Admin menu, **HQ** tab, **Pilot card preview**. Each button closes the menus
and replays, on your own HUD only, the updates a server would send
(`IA_PilotHudPreview`). No passengers, no helicopter and no backend are needed,
and no rating is earned. It works in Workbench play mode and on a server where
you are admin.

| Button | Shows |
| --- | --- |
| Pilot seat | Rating card on taking the pilot seat |
| Full landing | 12 passengers in the objective, arriving over four updates on one card |
| Far landing | 5 passengers set down short: lower weight, distance label |
| Total syncing | Landing with the total unknown; the total arrives 3.5 s later |
| Skin unlock | The landing that crosses the threshold, with passengers still stepping out |
| Late unlock | The unlock arriving on a rating card |
| Unlocked seat | Rating card of a pilot who owns the skin |
| Play all | Every scene in turn, each after the previous card has left |

The cards are filled by `IA_PilotDropoffPayload.SetProgress` and
`IA_TransportScoring`, the same code the server uses, and enter through
`IA_NotificationDisplay.ShowPilotProgress` like a real update. The preview
skips the server side: crediting, batching per pilot, and the RPC.

## Not yet proven in game

Seen in Workbench play mode: every livery on the Huey and the Mi-8, a repaint
with a pilot seated and the engine running, only the painted airframe changing,
channels handed out and given back per family, and the paint bay's drawing for
both families and the generic silhouette. Not yet seen:

- **A modded helicopter in a mission.** The whole path ran on the Sikorsky H-60
  mod in Workbench, from its compatibility addon. Not yet seen: a
  compatibility addon published and loaded by a server, a pad or
  the editor spawning its twin in a mission, and its seat opening the bay. A
  second mod may still use a material class the tools do not know.
- **Multiplayer.** The repaint on other clients, on a player who joins
  afterwards, on a dedicated server, and after the helicopter streams out and
  back in. Check by repainting from the paint bay with a pilot who has earned a
  livery, with a second helicopter of the same type in view keeping its paint.
- **The editor in a mission.** The editor's variant pick returned the twin on
  a free channel (`IA_HeliSkinLiveProbe -iaSkinMode 4`). Not yet seen: a
  helicopter placed through the Game Master or build-mode interface, and the
  editor treating the twin as the helicopter it was asked for (budget, refund,
  its entry in the entity list).
- **The paint bay in a seat.** Not yet seen: the key opening it from the
  pilot's seat, flight controls while it is open, the gamepad hold against the
  helicopter's own D-pad bindings, the entry in the keybinding menu, and a
  repaint asked by a client of a dedicated server.
- **A published build.** The generated prefabs and materials are registered
  and load in Workbench; only a packed build proves they ship.
- **The pilot card.** Layout, timing and the unlock sound can be checked alone
  with the preview above; the hull in the next livery's colour compiles but has
  not been looked at. The multi-tick merge with real passengers needs a flight.
