# Composition-based dynamic bases

Implementation complete in the experimental addon. The user reports the composition bases are working well; the subsequent sandbag-infill change still needs its short visual/gameplay check. This replaces the active fixed scenery/low-tripod placement approach. Historical layouts and their regression fixtures remain available to Workbench tools, not the runtime recipe selector.

## Designs and limits

The runtime uses 120 deterministic recipes: twenty per size, across Strongpoint, Encampment, RoadControl, Logistics and Camouflaged themes. Recipes change accommodation/service clusters and the perimeter's structure mix and placement, not just prop rotations. The last six committed variants are avoided; selection also avoids the last two themes. Selection is fixed for an AO's survey, not rerolled for every heading. History is session-local, not persistent.

| Size | Footprint (m) | Base guard ceiling* | Maximum guns |
|---|---:|---:|---:|
| Full | 180 x 140 | 36 | 4 |
| Compact | 140 x 116 | 36 | 3 |
| Courtyard | 120 x 104 | 24 | 2 |
| Roadside | 100 x 120 | 20 | 2 |
| Command post | 92 x 88 | 16 | 1 |
| Rally post | 76 x 76 | 12 | 1 |

*Existing settings and the existing 1.75x garrison scaling still apply. Gun crews are deducted from that total, not added to it. One heavy position (NSV, scoped NSV or AA) is allowed on Full/Compact only; AA shares that allowance. Installed counts may be lower, including zero after optional access/firing validation.

The catalog contains 28 composition types. Large living quarters are the default accommodation on every size, including Command and Rally posts, with extra LivingSmall / LivingLarge clusters and more hospital, maintenance, ammo, fuel and supply pieces filling the courtyard. The previous 820-entity authoring cap and full-width empty crossing are what kept LivingLarge unused; recipes now budget 2000 expanded entities against a 2200 runtime ceiling. Small living areas, HQ, hospital/medical, maintenance, supplies/ammunition/fuel, towers, bunkers, MG nests, four infantry sandbag positions and three sizes of checkpoint/barricade appear across the library. RoadControl describes an internal approach design; it does not promise alignment with an existing map road.

Three reserved approaches join an outdoor capture point and the central crossing. Standard solid burlap sandbag panels now fill the perimeter between the authored fortifications. Perimeter compositions snap the bags that should continue the wall onto the wall line, so a nest or tower is not pulled inland by its front parapet. The generator subtracts the three ten-metre gate openings, those joining-bag spans and conservatively padded horizontal gun-firing corridors before tiling wall runs. Sub-panel-length slivers are left clear rather than scaling a stock wall into a reserved opening. The result is a much denser perimeter, not a guarantee of an impassable sealed ring. Outdoor guard posts avoid measured composition pads; native Defend remains a whole-base soft leash. Towers are scenery with their physical ladders, **not a promise that AI will climb or occupy the platform**.

## Adaptation and ownership

- `tools/author_base_compositions.py` resolves vanilla prefab placements into new I&A resource GUIDs. Nested compositions become scenery roots. Campaign construction/disassembly, service/arsenal/spawn logic, activity points and weapons are excluded. Some vegetation, decals and generators are intentionally excluded too.
- Physical props normally retain their vanilla inheritance. Service-bearing physical hierarchies are adapted using mesh/rigid-body data and physical children rather than inheriting their service scripts. Consequently these adapted objects do not promise vanilla service actions or destruction behavior. Living/service compositions are scenery, not functional Conflict facilities.
- Native Preview measurements cover mesh bounds and expanded hierarchy counts, not campaign interaction boxes. The generated runtime catalog adds count headroom; recipes reserve up to twelve hardware entities per socket and stay at or below 2000 expanded entities. Existing absolute ceilings remain 256 roots / 2200 expanded entities. Wall infill still reserves its budget, but accommodation is chosen first so LivingLarge is no longer starved by the sandbag ring. Optional service selection yields to walls when necessary.
- Composition roots follow the terrain plane, with a ten-degree inclination cap and a size-scaled support residual (0.8 m typical, up to 1.6 m on LivingLarge). The previous five-degree / 0.15 m pair rejected ordinary Everon fields once every recipe required HQ plus a 50 m living cluster. Complete authored support arrangements carry the guns; the obsolete three-centimetre tripod-foot test is not used for composition sockets.
- Guns are separate site-owned roots. Checkpoint/nest weapons are extracted into the **same** socket and budget system, preventing hidden extra guns. Failed gun checks retain the fortification and continue construction.
- Optional installation is sliced and deadline-bounded. Access tries at most five outdoor positions. Firing checks require the central ray and at least three of five useful lanes, excluding only the gun's own hierarchy—not its sandbags or the base. This is not certification of every angle or every barrel sweep.
- Scenery/nav/guns finish before normal garrison readiness and reveal. Elevated seats use a checked outdoor crew access point rather than requiring navmesh on the seat platform itself.

## Weapons and configuration

PKM carries exactly **400 rounds**; NSV, scoped NSV and AA carry **200 rounds** each, including chambered ammunition. One loaded magazine and three reserve belts; no automatic restock, repair, replacement or replacement operators after casualties. Native reload behavior is retained. Mounted weapon removal is locked while magazines remain available for native reload; live interaction acceptance is still required.

Ground variants retain the limited I&A ground traverse/elevation envelope. AA preserves the vanilla NSV-SPP aiming limits and authored socket tilt; **it is not a newly implemented 360-degree or unrestricted-elevation AA weapon**. Aircraft acquisition is vanilla AI behavior and still requires an in-game check.

The admin Defense page exposes **Optional finite-ammunition base emplacements**. It defaults on, persists in dynamic-base extras v2, reads old v1 snapshots as enabled, and is frozen for the current construction. Turning it off affects subsequent bases and leaves the new scenery unarmed.

Assignments release only their own reservation and balance their LOD pin. Existing release/abort/capture/defense transitions retain the guns for player reuse according to the site's retirement policy. Cleanup remains deferred for player occupancy and entry/exit transitions; hardware accounting excludes character descendants. No new map/HUD indicators or global infantry/turret behavior changes are introduced.

## Sandbag infill acceptance (2026-09-06)

- Python composition tests pass, including wall reproducibility, gate clearance, composition separation, LivingLarge on all 120 recipes and full wall-inclusive accounting.
- Native `IA_BaseDesignTest` passes all 120 recipes / 24 headings, `failures=0`: `F:/IA_EmplacementProbe/logs/logs_2026-09-06_01-53-48/script.log`.
- WORKBENCH, PC, XBOX, PS4 and PS5 compile validation passes: `.../logs_2026-09-06_01-53-55/script.log`.
- Logging validator passes 115 scripts. Infill panels use the original stock solid burlap wall and its independently terrain-aligned placement path. They are required modules, not silently omitted decorative pieces.
- Perimeter compositions now snap their joining-bag origins to the wall line (nests are no longer pulled inland by the front MG ring). Infill subtracts that joining span so walls meet the bags. Existing active bases are not modified in place.
- **Next live check:** spawn a new base, walk its perimeter and all three entrances, confirm nests/towers sit in the wall line, and confirm its previously working guns still fire outward.

## Original composition validation evidence

Final source/automation pass:

- 29 Python tests pass, including all recipes, clear pads/approaches/posts, shared checkpoint/heavy caps, unique designs, generated-file reproducibility and clean prefab inheritance.
- Runtime logging validator passes 115 scripts.
- Native `IA_BaseDesignTest`: 120 recipes, 24 headings, `failures=0`; log `F:/IA_EmplacementProbe/logs/logs_2026-09-06_01-18-32/script.log`.
- Native `IA_BaseEmplacementTest`: allocation, legacy fixtures, codec migration, frozen settings and persistence, `failures=0`; log `.../logs_2026-09-06_01-18-38/script.log`.
- Native `IA_BaseEmplacementProbe`: four own variants, hardware counts 6/7/8/8, ammunition 400/200/200/200, mechanical limits, initialization/item lock, untouched magazine locks and true zero ammunition, `failures=0`; log `.../logs_2026-09-06_01-18-44/script.log`.
- ScriptEditor reports validation successful for WORKBENCH, PC, XBOX, PS4 and PS5; log `.../logs_2026-09-06_01-18-50/script.log`. The automation closed the editor after validation; it does not exit automatically like the test plugins.
- Composition measurements: `docs/base-composition-measurements.json`, with the native evidence directory embedded. Preview damage reports uninitialized 0/0 even for stock guns; live usability/destruction is deliberately not asserted in Preview.

Regenerate/check:

```text
python -B tools/author_base_compositions.py D:/ReforgerGameSources/data/data007 --check
python -B tools/author_base_designs.py --check
python -B -m unittest discover -s tools -p "test_*.py"
python -B tools/check_runtime_logging.py
```

Generated catalog/design JSON and `base-composition-designs.svg` provide the reviewable recipe inventory. Re-run the composition measurement plugin and update its measured JSON before regenerating designs when physical assets change. The older layout/emplacement generators and tests describe retained legacy fixtures, not the active composition designs.

## Smallest live acceptance test

1. Enable `-iaDebug 1`, select Compact with emplacements on, and trigger a new seize-base on a broad, gently sloping/flat site while players are outside placement clearance. Approach only after reveal.
2. Confirm a complete composition base appears and locate its guns. Save the `[IA][Emplacements]` line: separate PKM/NSV/scopedNSV/AA counts, crew/fallback, attempts and omission reasons identify whether installation or boarding failed.
3. From outside cover, provoke one gunner. Confirm mounting, turning and sustained fire, including a belt reload. Kill that operator; verify no replacement is created, then mount/fire/reload/exit the gun as a player without losing normal infantry controls.
4. Capture and enter defense. Verify the gun remains usable. Clear/cancel the site while seated or transitioning: neither the player nor occupied hardware should disappear. Leave and move beyond cleanup distance; verify eventual cleanup.

Then test a recipe containing AA against a helicopter in its vanilla firing envelope, another base for design variety, and an emplacements-off base. Dedicated-server/client/JIP, gun destruction, full traverse collision, tower access and player item-removal restrictions remain explicit live acceptance cases. Do not treat compiler/Preview success as evidence for these behaviors.
