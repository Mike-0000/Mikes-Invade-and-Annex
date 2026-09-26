# Permanent enemy headquarters (second dynamic-base style)

Status: implemented behind the existing dynamic-base pipeline. The scrappy
composition bases are unchanged. See "Verification record" at the end for what
was and was not proven.

## 1. Goal

About half of dynamic bases should be a **permanent headquarters (HQ)**
instead of the existing scrappy sandbag base. An HQ is a built-up concrete
compound. It keeps the scrappy concepts:

- a defended perimeter
- exterior guns
- interior emplacements
- varied interior structures

It adds its own identity:

- concrete walls and concrete gun casemates instead of sandbags
- a layered obstacle belt, then the wall, then towers
- organized rows of permanent buildings along an internal road

## 2. Architecture and extension point

The existing pipeline was inspected end to end: `IA_DynamicSitePlacer` →
`IA_BaseDesignLibrary` → `IA_BaseDesignRecipes` → `IA_ComposedSiteLayout` →
`IA_CompositionGunBuilder` → `IA_DynamicSiteInstance`.

The pipeline is data-driven. The placer only sees an `IA_DynamicSiteLayout`: a
list of modules, each a prefab plus a pose, footprint, grounding policy and
perimeter side, together with gun sockets and guard posts. The HQ therefore
plugs in as **another set of generated recipes**. It needs no new runtime
subsystem.

| Piece | Change |
|---|---|
| `IA_BaseDesignLibrary` | `Select` is unchanged. New `SelectDesign(seed, hqChancePct)` rolls the style, and HQ variants are encoded as `HQ_BASE (100) + v`. New `CreateLayout(size, variant)` dispatches to either recipe class. `Committed` routes HQ variants into a separate history, so the scrappy rotation is untouched. |
| `IA_HeadquartersRecipes` (generated) | 6 sizes × 12 variants = 72 recipes. |
| `IA_HeadquartersCatalog` (generated) | Measured HQ scenery assets, with the same asset/socket type as the scrappy catalog. |
| `IA_HeadquartersSiteLayout : IA_ComposedSiteLayout` | Resolves keys from the HQ catalog before the scrappy one. Adds `AddWallRun` (upright stepped concrete, collective coverage) and `AddObstacle` (outer belt dressing). |
| `IA_ComposedSiteLayout` | A single virtual `ResolveAsset(key)` hook is extracted from `AddComposition`. Scrappy behaviour is byte-for-byte the same code path. |
| `IA_DynamicSitePlacer` | Uses `SelectDesign` / `CreateLayout`. Falls back once from HQ to scrappy when HQ resources are missing or no legal HQ site exists. |
| `IA_Config` | Adds `m_iDynamicBaseHeadquartersChancePct` (default 50, clamped 0–100). |

## 3. Asset findings

Assets were catalogued from extracted base-game data (`data007`), including
prefabs that are not exposed in the editor. Every asset below was then spawned
and measured in a Workbench Preview world with
`IA_HeadquartersAssetProbe` (35 prefabs, `failures=0`).

**Walls**

- `ConcreteWall_USSR_01_V1/V2/V3` + `_pillar`:
  - `StaticModelEntity` with **no `RplComponent`**, so they must be children of
    an IA root that replicates.
  - Panel origin is at its pillar, with a pitch of 2.698 m.
  - 3.33 m above the origin, plus a buried skirt of about 1.0 m.
  - The vanilla wall generator `WG_ConcreteWall_03_base` tiles V1/V2/V3, which
    confirms they join.
- `ConcreteWall_02_4m`:
  - ±2 m long, ±1.85 m high and centred, 0.52 m thick.
  - Used sunk into the ground as a gun parapet.
- `ConcreteWall_01_3m`: 3 × 2.96 × 0.25 m, used for casemate cheeks.
- `ConcreteWall_01_6m_A`: 6 × 2.96 × 0.25 m, used as the casemate roof slab.

**Buildings**

These are `SCR_DestructibleBuildingEntity`, which carries RPL, persistence,
world subscene, occluder, doors and windows. They are never used as
composition children in vanilla, and runtime-spawning them is unproven.
Decision: author **mesh-only IA wrappers**, reusing the production precedent
already used for `IA_Antenna_02_USSR` (the `mesh_only` path in
`author_base_compositions.py`).

- A wrapper is a `GenericEntity` with RPL, `Hierarchy`, and the merged
  `MeshObject` and `RigidBody`.
- Static `PivotID` add-ons without RPL are kept: tower stairs, and the entry
  steps on buildings.
- Doors, windows and furniture are dropped. The results are permanent and
  indestructible.

| Building | Measured size | Use |
|---|---|---|
| `GuardHouse_01` | 18.4 × 14 m | HQ command building |
| `GuardTower_USSR_01` | 2.3 × 5 m, 6.8 m high | Tower |
| `Bunker_SPS` | 3 × 5.6 m | Pillbox |
| `ShelterMilitary_E_01` | 10.5 × 7.3 m | Shelter |
| `GuardBox_01` | 2.3 m | Gate guard box |
| `Barracks_USSR_01_military` | 16 × 41 m | Barracks on Full/Compact only |

**Rejected**

- `Bunker_01_*`: sunken more than 5 m, which needs a dug pit.
- `AmmoDump_E_02`: 37 × 44 m, too large.

**Obstacles**

All of these carry their own RPL. RPL props as children are already
production-proven in `IA_Checkpoint` and `IA_Barricade`.

- `Dragontooth_01_V1/V2`: 1.3 m
- `CzechHedgehog_01_painted`: 1.5 m
- `BarbedTape_01_Triple`: 3.45 m run

**Not used, because replication was not verified**

- flag poles
- generator floodlights
- `VehicleObstacle_*`
- storage shelters

## 4. HQ layout and generation rules

The recipes are generated offline by `tools/author_headquarters.py`, so there
is no runtime packing search. The footprints and garrison sizes are the same
six sizes as the scrappy bases (Full 90 × 70 half-extents down to Rally
38 × 38). That keeps `GunCap`, `GetAllowedLayoutIds`, the admin size mode,
capture radius, entries and budgets valid.

Layers, from the outside in:

1. **Obstacle belt**, 2 m inside the footprint edge. Dragon's-teeth rows,
   hedgehog clusters and triple barbed wire alternate according to the
   archetype. Gates leave a 12 m opening. Belt items are non-blocking
   dressing: they never veto a site and are skipped on bad ground.
2. **Concrete wall**, 5 m inside the footprint edge.
   - Built from two-panel runs, 5.396 m each, with three panel mixes chosen
     deterministically.
   - A pillar caps every segment end.
   - Gates keep the scrappy geometry, which keeps `m_aEntries` valid: front gate
     |x| < 5 m, side gates z ∈ [−13, −3].
3. **Wall-line concrete casemates** (exterior guns). Each is one run wide:
   - a sunk `ConcreteWall_02` parapet at 0.656 m, the vanilla tripod height
   - concrete cheeks and a roof slab
   - the vanilla PKM/NSV tripod with `IA_StaticGunComponent`, exactly as in
     the scrappy sockets

   At least one gun per face. Caps are the scrappy `CAPS` (6/5/4/4/4/4). At
   most one heavy NSV, on Full/Compact only.
4. **Towers**: guard towers inside the wall corners, stairs facing inward.
5. **Interior**: the HQ building sits against the rear wall, facing the gate
   along a 8 m internal road (the lane). The other buildings are packed in
   **rows either side of the road**, all aligned to the grid. That packing is
   the main visual difference from the scattered scrappy yard. Contents:
   - barracks (a building or a small living area)
   - shelters
   - pillboxes covering the gate
   - the existing IA functional compositions (supply, ammo, fuel,
     maintenance, medical), reused from the scrappy catalog
6. **Interior emplacement**: a gate-overwatch casemate inside the front gate
   on sizes that fit it.

**Archetypes** (4 × 3 variants per size)

| Archetype | Character |
|---|---|
| Command | Four towers, a gate guard box, two pillboxes, a shelter and a supply row |
| Garrison | Barracks building on Full/Compact, living area otherwise; two towers; shelters |
| Depot | Shelters and supply/ammo/fuel/maintenance rows, hedgehog belt |
| Border strongpoint | Double dragon's-teeth belt, extra casemates, pillboxes on the lane, four towers |

## 5. Variety and determinism

- **Style roll**:
  - `SelectDesign(seed, chance)` uses a seed-derived roll
    (`hash(seed) % 100 < chance`).
  - Anti-streak rule: when 34 ≤ chance ≤ 66, two committed bases of the same
    style force the other style next. Committed history holds 4 styles.
  - Chance 0 or 100 is absolute.
- **HQ variant**:
  - Rotation over 12 variants with stride 5 (coprime to 12).
  - Skips the last 4 committed HQ variants and the last archetype.
- **Offline generation** is seeded per (size, variant) with `random.Random`, so
  output is reproducible. `--check` enforces this in tests.
- Rejected sites do not consume history. Only `Committed` does, matching the
  scrappy behaviour.

## 6. Perimeter and defense placement

- **Wall runs**:
  - Grounding is upright (`UprightPad`), with origin = the highest ground
    sample − 0.05 m.
  - Allowed rise over a 5.4 m run is 0.9 m, and lift is at most 0.9 m.
  - The buried 1.0 m skirt hides the low side, so walls step on slopes instead
    of leaning.
  - Runs are non-required individually. `HasPerimeterCoverage` (60 % per face,
    75 % total) governs, exactly as for sandbag walls.
- **Casemates** are wall-line compositions: required, perimeter side set, and
  registered as panels. The gun builder therefore finds their
  assembly-owned `IA_StaticGunComponent` through the unchanged
  `FindInTree` path.

## 7. Interior composition

- Rows either side of the lane:
  - Items are sorted by depth.
  - Yaw is 90/270, facing the road.
  - Gap between items is ≥ 3 m.
  - Items stay ≥ 3 m from the inner wall face.
- Every interior item keeps the scrappy clearance rules:
  - boxes overlap-checked with a 1 m gap
  - capture point kept clear
  - the lane kept clear, except HQ and barracks
- Required modules:
  - the HQ building, module 0, as required by `StepSurvey`
  - one Barracks-role item
- Guard posts use the same grid-and-clearance algorithm as the scrappy bases:
  - ≥ 16 posts on Full/Compact, ≥ 6 on the smaller sizes
  - never inside a building footprint

## 8. Spacing, collision and navigation

- Axis-aligned boxes use measured mesh bounds + 1 m pads, checked offline and
  again in tests.
- Survey collision uses the unchanged per-module `TraceOBB` clearance, with
  `ENTS` flags.
- Construction still calls `RequestNavRebuild`, so the HQ buildings' own
  interiors get navmesh. Mesh-only wrappers have no doors, so the openings stay
  walkable.
- Wall gates line up with the scrappy entry vectors, so garrison, QRF and
  route validation are unchanged.

## 9. Performance

- **Roots**: every recipe asserts roots (modules + wall runs + pillars +
  obstacles + guns) ≤ 240, below `MAX_ROOTS` 256.
- **Expanded entities**: every recipe asserts the expanded estimate ≤ 2000,
  below `MAX_EXPANDED` 2200.
- **Cheaper than scrappy**:
  - A concrete run replaces about two sandbag panels, so a Full HQ has about
    110 wall roots against about 210 for scrappy.
  - Mesh-only buildings are one entity each, plus static add-ons.
- **Budgets unchanged**: survey and construction budgets are untouched, and
  there is no new per-frame work.

## 10. Configuration and compatibility

- `m_iDynamicBaseHeadquartersChancePct`:
  - A new attribute on `IA_Config`. Old `.conf` files that lack it get the
    default of 50.
  - It is clamped in `ClampDynamicBaseSettings`.
  - It is **not** added to the packed admin override string. That keeps the
    `PackDynamicBaseExtras` v2 format and the Admin Config menu unchanged, so
    there is no save or JIP format change. Workshop and server operators can
    set it in the `.conf`.
- Admin `Size mode` still restricts the size for both styles.
- Nothing about the chosen style is persisted. Variants live only in memory,
  as before.

## 11. Fallback

1. If HQ preflight loads fail for every size, `InitializeSurvey` immediately
   rebuilds the survey with a scrappy variant.
2. If an HQ survey ends with `no_legal_site` (after the relaxed terrain pass),
   the placer switches to a scrappy variant once and re-surveys. The
   pre-existing timeout still bounds total time.
3. A missing HQ catalog key yields no module (the `ResolveAsset` null path).
   The generator tests prove that every recipe key exists.

## 12. Testing

- **Python** (`tools/test_headquarters.py`):
  - reproducible outputs
  - clean inheritance: no RPL-less child outside an RPL root, no campaign
    components
  - unique GUIDs and `.meta`
  - 72 recipes with 4 archetypes per size
  - at least one gun per face, caps, heavy ≤ 1
  - no overlaps, lane clear, posts clear
  - root and expanded budgets
  - wall coverage and gates
  - casemate sockets
- **Python**: existing scrappy tests unchanged. `author_base_designs.py
  --check` proves the scrappy outputs are byte-identical.
- **Workbench**:
  - `IA_HeadquartersProbe` measures the authored HQ prefabs, which proves they
    load and gives their mesh bounds.
  - `IA_HeadquartersDesignTest` builds all 72 layouts through the real Enforce
    classes and checks module counts, sockets, style selection split and
    history.
  - The existing `IA_BaseDesignTest` is re-run.
- **Repo checks**: `check_runtime_logging.py` and the full Python test
  discovery.
- **In-game** (hand-off; cannot be run here): spawn several HQs on Everon in
  multiplayer with JIP. Check:
  - that walls and buildings replicate
  - AI pathing through gates
  - that casemate guns are crewed and fire

## 13. Incremental rollout

1. Assets and probe, measured.
2. Generator, recipes and Python tests.
3. Enforce catalog, layout and dispatch. The chance defaults to 50, so for
   production the chance can be set to 0 in `.conf` to disable HQs instantly.
4. Workbench tests.
5. In-game MP validation on experimental before any promotion. This step is
   not done by this change.

## 14. Plan scoring

**Iteration 1: 6/10.** The draft spawned the vanilla buildings directly and
put obstacles outside the footprint. Critique:

- Destructible buildings carry persistence and world-subscene components that
  have never been runtime-spawned in this mod. That is a save and replication
  risk.
- Outside obstacles fail the apron check.
- Four-panel wall runs exceed terrain tolerance.
- There was no fallback.

**Iteration 2: 8/10.** Changes: mesh-only wrappers, obstacles inside the
footprint behind a 5 m wall inset, two-panel stepped runs, the single fallback,
and the config default. Critique:

- The casemates rested guns on sandbags, which defeats the identity.
- HQ history could starve scrappy rotation.
- The admin-pack change risked the save format.

**Iteration 3: 9/10.** Changes:

- concrete parapets at exact tripod height
- separate HQ history with the anti-streak rule
- the config field kept out of the admin pack
- the `ResolveAsset` hook, so scrappy code stays identical
- explicit root and expanded asserts
- a Workbench test of all 72 layouts

Remaining honest gaps, which keep it from 10:

- Mesh-only buildings lose vanilla AI smart actions (tower sentinel posts).
- Pillbox facing is chosen from the mesh bounds, not from a verified
  firing-slit socket.
- Live multiplayer, AI and navigation behaviour can only be proven in game.

## 15. As built (supersedes the plan where they differ)

- **Wall panels are IA mesh prefabs.** Vanilla `ConcreteWall_*` prefabs are
  `SCR_DestructibleEntity` without RPL. Spawning them as children would bring
  destruction phases that are never replicated. The generator instead writes
  `Prefabs/BaseCompositions/Headquarters/Mesh/IA_HQ_Mesh_*.et`
  (`StaticModelEntity`, merged `MeshObject` + `RigidBody` only). Runs
  (`IA_HQ_WallRun_A/B/C`, two panels) and `IA_HQ_WallPanel` (one panel for
  leftovers) are RPL roots that hold those meshes.
- **No pillar caps.** The panel mesh carries its own pillar at the left end
  (x −0.474..2.698), so panels tile exactly at 2.698 m. Wall extents are
  snapped to that grid, so the wall line sits 5–7.7 m inside the footprint
  edge rather than a fixed 5 m.
- **Casemate:**
  - The parapet is `ConcreteWall_02_4m`, sunk so its top is at the tripod
    height (0.656 m).
  - Cheeks are camo `ConcreteWall_01_3m`, and the roof is camo
    `ConcreteWall_01_6m_A`, laid flat. Pitch 90 maps local +y to world −Z.
  - Measured result: 5.9 m wide, 3.2 m deep, 3.22 m tall.
  - The PKM or NSV tripod has `IA_StaticGunComponent`, identical to the scrappy
    sockets.
- **No `VehicleObstacle_*`.** The belt is dragon's teeth, hedgehogs and
  triple wire. It sits between the snapped wall line and the footprint, and
  Border gets two rows when the gap is at least 4.6 m.
- **Interior:**
  - Uses column lots (inner, mid and outer per flank, one row every 2 m),
    filled by farthest-point selection. This replaced the single row either
    side of the road, which bunched against the lane.
  - Buildings with a buried plinth of 0.6 m or more stand upright. They do
    not tilt with the grade, and their lift is capped at plinth − 0.3 m.
- **Interior overwatch casemate** is on Full and Compact only. It is set back
  from the gate at x = ±(10 + 2v), faces sideways over the gate, and is snug
  to the front wall.
- **Selection hash.** `Mix(seed)` is a murmur3-style finaliser. The first
  multiplicative hash was correlated for sequential seeds, and the Workbench
  split test caught it. Enforce int32 wrap and masked shifts were verified to
  match a Python int32 emulation.
- **History order.** Style and HQ histories use `RemoveOrdered(0)`. The
  scrappy `s_aRecent.Remove(0)` swap-removes, so its history is unordered. That
  bug predates this change and is left untouched to keep scrappy selection
  identical.

**Final plan score: 9/10.** Every gap from iteration 3 is either closed or
explicitly bounded:

- Assets are measured, not assumed.
- Scrappy code paths and outputs are byte-identical, except the dispatcher
  indirection.
- The config is backward compatible, and the save and admin formats are
  unchanged.
- Two fallback paths exist.
- All 72 recipes are validated by the real Enforce classes.

It is still not 10, because live multiplayer, JIP and AI behaviour are
unproven. Mesh-only buildings also have no doors or smart actions.

## Verification record

All results are from 2026-09-25, on this branch's working tree.

**Workbench CLI** (Steam must be running, `-wbModule=ResourceManager -plugin=... -run`):

| Plugin | Log | Result |
|---|---|---|
| `IA_HeadquartersDesignTest` | `logs_2026-09-25_23-06-13` | failures=0; selection hq=199/400 |
| `IA_HeadquartersProbe` | `logs_2026-09-25_23-06-20` | failures=0 (15 HQ prefabs spawned and measured) |
| `IA_BaseFoundationProbe` | `logs_2026-09-25_23-06-27` | failures=0 |
| `IA_BaseGarrisonTest` | `logs_2026-09-25_23-06-34` | failures=0 |
| `IA_BaseSelectionTest` | `logs_2026-09-25_23-06-42` | failures=0 |
| `IA_HeadquartersAssetProbe` | `logs_2026-09-25_22-31-03` | failures=0 (35 stock prefabs) |

- `IA_BaseDesignTest` reports 920 failures ("composition or wall grounding
  enabled").
- `IA_BaseEmplacementTest` reports 1 failure ("about seven in ten bases leave
  one wall empty").
- Both reproduce identically on a clean `HEAD` worktree, so they predate this
  change.

**Python:**

- `author_base_compositions.py <data007> --check`: 28 entries / 268 files,
  unchanged.
- `author_base_designs.py --check`: unchanged.
- `author_headquarters.py assets <data007> --check`: 46 outputs.
- `author_headquarters.py designs --check`: 4 outputs.
- `unittest discover -s tools`: 52 tests. Three failures are identical on
  clean `HEAD`, so they predate this change:
  - `test_capture_and_failed_paths_stop_assignment` (missing
    `IA_StaticGunCombat.c`)
  - `test_actual_member_arrival_is_the_only_entry_trigger`
  - `test_no_per_gun_frame_or_call_queue`
- `test_base_compositions` now asserts the dispatcher path instead of the
  direct `IA_BaseDesignRecipes.Create` call.
- `check_runtime_logging.py`: flags only `IA_AreaGroupManager.c:138`, which
  predates this change (commit 66459f8).

**Not verified (in-game hand-off):**

- replication and JIP of wall runs, casemates and wrappers on a dedicated
  server
- AI pathing through the gates and navmesh around buildings
- casemate crewing and firing arcs from under the roof
- how long an HQ survey plus scrappy fallback takes on rough maps
