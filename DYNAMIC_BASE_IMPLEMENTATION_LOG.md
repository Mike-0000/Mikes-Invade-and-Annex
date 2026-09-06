# Dynamic base objective — implementation log

Planning artifacts: `DYNAMIC_BASE_OBJECTIVE_PLAN.md`, `DYNAMIC_BASE_IMPLEMENTATION_RUNBOOK.md`.

This is **not** a game-tested release. Workbench compile and dedicated playtest were not run in this session.

## Packages

| Package | Status | Notes |
| --- | --- | --- |
| 1 Configuration and contracts | Code complete | Nine `IA_Config` fields, pack/unpack at outer token 24, Rpl string, profile `dynamicBaseVersion`/`dynamicBaseExtras`, Defense-page widgets, first-member JSON comma fix. |
| 2 Manual full-layout prototype | Code manifests only | Full 180×140 and Compact 120×100 live in `IA_DynamicSiteLayout`. Uses existing vanilla USSR tent/cover ResourceNames. No Workbench visual/path proof. No new IA-owned module prefabs. |
| 3 Transient host and ownership | Code complete | `Create(..., dynamicObjectiveHost)`, skipped ordinary generators, `OnAttacked` no-op, garrison abort on `Despawn`, deferred site cleanup. |
| 4 Seize / regroup / status | Code complete | Sampler, 90s uncontested capture, roster/regroup/warning, replicated HUD tile, seize/regroup tasks. |
| 5 Prepared defense and cancellation | Code complete | `CreateForDynamicBase`, `AbortDefendMission`, enhanced Probe/clock/event shift, success routes through director then `OnDefendMissionComplete` once. |
| 6 Runtime placement | Code complete, unvalidated | Candidate shortlist, 45s deadline, Full-then-Compact, 0/90/180/270, player 250 m + LOS, lattice routes, staggered garrison. No in-engine geometry/nav acceptance. |
| 7 Natural / GM / admin sequencing | Code complete | AO serial, `TryBeginTerminalObjective`, fallback helper, ordinary-host retire on commit, Complete AO / Complete+Defend (full seize chain) / Complete objectives+seize base, QRF/side/arty pressure gates. |
| 8 Dedicated playtest | Not started | No Workbench compile, no dedicated-server run, no acceptance-matrix results. |

## Changed contracts

- `IA_AreaType.DynamicBase` appended (value 11). Ordinary quotas are 0.
- `IA_AreaInstance.Create` last optional `bool dynamicObjectiveHost = false`.
- Admin payload outer token 24 is Dynamic Base extras. Tokens 0–23 unchanged.
- `IA_DefendMission.CreateForDynamicBase` / `GetDefenseConfig` / `AbortDefendMission` / `IsPreparedDynamicBase`.
- `IA_MissionInitializer` owns one `IA_DynamicObjectiveDirector` and an AO activation serial.

## Engine-validation blockers

1. Compile every changed Enforce file in Workbench and fix any new script errors. Text search is not a compile.
2. Author or measure real module pads, door yaw corrections, and expanded entity counts in Workbench. Current sockets are authoring targets using vanilla USSR tents.
3. Prove two infantry routes from distinct entries to HQ on Full and Compact at 0/90/180/270.
4. Dedicated-server JIP: props, task, map marker, HUD phase/countdown during Seize/Regroup/Warning/Defend.
5. Run runbook §14 acceptance IDs before calling the feature game-tested.

## Playtest admin note

`Complete + Defend` now starts the same validated field-base attempt as `Complete objectives + seize base`. It no longer skips capture/regroup or jumps to an authored Defend marker. Clicking it again while Placing/Seize/Regroup/Warning/Defend is ignored so the live chain can be played.

## Tuned values

None. Defaults remain enabled / 100% / Auto / 90 / 90 / 240 / 0.60 / 1.0.

## Post-implementation review and smaller layouts

The review corrected placement/fallback ownership, proximity-safe retirement, navigation rebuild order, roster tracking throughout the AO, casualty/disconnection target recalculation, terrain-based capture anchors, duplicate task notifications, stale HUD updates, and defense reinforcement-manager ownership. Scenery compositions now avoid inherited campaign construction actions and excessive repeated clutter. The stock furniture roots and authored child transforms are retained.

Auto now includes Courtyard (88 × 76 m), Roadside (60 × 96 m), and Command post (64 × 56 m). The admin dropdown can force each design. Their occupying-garrison ceilings are 24, 20 and 16. Search considers 256 samples, retains up to 32 dispersed centers, checks eight headings in bounded callbacks, and tries smaller designs at a clearing when a larger construction attempt fails. The placement deadline is 60 seconds. Rejection counts are logged by stage.

Validation completed on 2026-09-04:

- Workbench ScriptEditor validation reported **Script validation successful** for the final scripts, after checking WORKBENCH, PC, XBOX, PS4 and PS5 configurations.
- Authored small-layout tests passed: pads stay inside footprints, pads do not overlap, guard scatter avoids modules, and all three approaches reach the capture area.
- Source asset/reference checks passed for all five layouts. Conservative scenery counts: Full 557, Compact 414, Courtyard 245, Roadside 217, Command post 148; ceiling 600.
- Changed-file whitespace checks passed.

The engine compile item above is now complete. Runtime acceptance, exact mesh/door alignment, terrain placement frequency, multiplayer replication and full capture/regroup/defense playtests remain unverified. See `docs/dynamic-base-layouts.md` and the accompanying SVG for the new floor plans and testing instructions.

## Terrain-search correction — 2026-09-05

The Kamensk diagnostic run rejected all 1,280 center/layout/heading combinations: 608 crossed water, 514 exceeded the whole-footprint height limit, 105 failed module-pad height checks, and 53 hit obstructions. The old shortlist's center-only 18 m terrain score did not establish suitability for any base, and rotating around that center moved the mandatory HQ away from the patch being ranked.

Replaced that shortlist generator with a staged survey of up to 2,048 HQ anchors. A retained location must fit an entire allowed layout, including module pads/obstructions and AO boundaries. Up to 64 dispersed fits are cached during the ordinary AO. Layout origins are derived from the HQ anchor for each orientation. Active placement revalidates terrain, player clearance/visibility and access routes, and resumes the survey when cached candidates are exhausted. Size-mode changes rebuild the survey; cancellation and AO-serial guards cover precomputation as well as construction. The overall active placement deadline remains 60 seconds. Survey callbacks limit full checks to two and yield between poses after four milliseconds, with an additional 64-pose cap.

Corrected module obstruction traces to use the engine's oriented box rather than an inflated world-axis bounding box. This removes false collisions outside diagonally rotated pads. Existing pad dimensions, height-span limits and player safeguards remain in force. Terrain scans stop once the sampled span exceeds the limit.

Validation: Workbench reported **Script validation successful** on 2026-09-05 for WORKBENCH, PC, XBOX, PS4 and PS5. Existing authored-layout and recursive asset/reference checks passed. The live Kamensk success rate, server survey cost and physical mesh placement still need an in-game run; these changes do not guarantee a legal site exists in every AO.


## Measured support profiles — 2026-09-05

The next hillside run rejected 80,818 of 81,920 poses at pad-height validation. The search improvements did not address the remaining mismatch: reserved clearance around buildings was still treated as flat bearing ground. The previous unchanged 0.35 m span across the HQ's 24 x 20 m reserved pad was inappropriate for the actual tent and stock earth foundation.

Measured the installed models using a Workbench plugin and an isolated preview world. The USSR tent body is approximately 8 x 8.6 m; its shared earth foundation extends to local Y=-2.746 m. The floor begins above local Y=0.027 m and is authored at +0.1 m in the composition. Supply shelter/contents aggregate bounds are approximately 8.6 x 10.9 m. These are engine measurements, not estimated composition radii.

Added separate bearing footprints for HQ/barracks/medical and supply. Foundation-backed tents sample a 2 m interior grid over the tent body, accept up to 0.8 m sampled span, and raise the upright root only enough to keep bearing terrain below its floor allowance, with a strict 0.35 m lift cap. Supply retains its 0.35 m span and zero lift, now checked over its measured footprint. Validation and spawning share the height solver. Reserved collision pads are unchanged. Obstruction checks cover entities from the low side of each pad, independently of its allowable terrain span; terrain remains separately validated.

The HQ scenery now uses the plain stock tent/floor/foundation, without the wider separate camonet, ground-level sign or decorative dirt mound. This avoids floating accessories when the foundation is raised and keeps all replicated child transforms authored. The generator preserves this choice. Updated conservative entity counts: Full 553, Compact 410, Courtyard 241, Roadside 213, Command post 144.

Validation completed: actual Enforce support regression tests ran in Workbench with zero failures (flat/gentle ground, excessive span, excessive lift, sample containment, interior coverage and supply limits); installed supply bounds fit the new profile; all five script configurations compiled; authored-layout and recursive asset/reference checks passed. Probe and tests are repeatable via Scripts/WorkbenchGame/IA_BaseFoundationProbe.c. Runtime AO placement rate, foundation-edge appearance and doorway traversal still need an in-game run. No terrain was modified and the user's running editor was not used for the measurements.


## On-map diagnosis and Rally post — 2026-09-05

Reproduced the production terrain survey against the actual IA_Kolguyev map in an isolated World Editor process. The five existing designs yielded only two shortlisted fits for group 0 using 2,048 anchors and seed 12345. Per-module diagnostics showed additional rejections at supply, barracks and perimeter pads after HQ suitability passed. This confirmed that remaining failures were not exclusively the mandatory HQ check.

Added a final authored Rally post fallback (36 x 48 m, one stock HQ/foundation, five sandbag parapets, three entrances, 9 m capture radius, 12 occupying guards, six scenery roots / 61 entities). Auto appends it after the existing designs; admin value 6 forces it without changing existing saved values. All terrain/lift/obstruction/visibility/boundary rules remain unchanged. The same on-map audit with Rally post enabled returned 27 shortlisted fits. The capture/regroup/defend implementation is shared with the larger bases.

Added a repeatable read-only World Editor audit plugin, Workbench-only world injection in the placer, and module IDs in rejection diagnostics. Normal game builds continue to query the current game world. Updated admin bounds/help, layout/asset checks and floor-plan documentation/SVG. Geometry and asset checks pass. A separate exploratory game-mode comparison encountered an existing Math.RandomInt(0,0) startup error and did not establish a live mission result; the 2-versus-27 comparison is specifically the static editor-map terrain/obstruction/AO-boundary audit.

Final validation: Workbench reported Script validation successful for WORKBENCH, PC, XBOX, PS4 and PS5; the support regression plugin included all six layouts and completed with zero failures. Changed-file whitespace and generated SVG XML checks passed.


### Tall, continuous sandbag perimeters (2026-09-05)

Replaced half-height cover clusters in all six layouts with 48-204 individually grounded stock sandbag panels: solid walls, firing-opening walls, high straight sections and high curved sections. Fixed authored runs preserve the three entrances and existing footprints. Placement and construction require 75% overall coverage and at least 60% on each side; existing obstructions can omit individual sections within those limits. Adjacent panel traces exclude owned site entities. Raised scenery ceilings to 256 roots / 750 entities (Full estimates 214 / 681). The prefab generator no longer reintroduces the low-wall wrapper.

Verified all six layouts for wall footprint/building/guard clearance, three entrances, fixed style variety and wall spacing. Workbench script validation passed. The support/coverage probe completed with zero failures. Read-only Kolguyev group-0 terrain audit (seed 12345, 2048 anchors, all six layouts) found 61 fits with the new perimeters. This does not verify live spawning, navigation or multiplayer replication; those still need a game playtest.

### Auto prefers the largest legal sampled layout (2026-09-05)

Fixed location-first selection: the mixed-size shortlist was ranked only by player distance, and candidate validation downgraded at that same anchor before visiting other fields. Survey now exhausts each size's anchors and candidates across the AO before advancing to the next size. Each size replays the same seed, and each shortlist batch contains only one size. Construction failures resume remaining headings and other sites of that size. Immediate placement gets a bounded 180-second search budget followed by a separate shared 60-second construction/retry budget; background survey remains sliced during ordinary objectives.

Added a WORKBENCH-only synthetic terrain fixture and IA_BaseSelectionTest plugin. Actual Enforce tests completed with zero failures for a small first clearing versus a larger second clearing, retry size preservation, candidate exhaustion before downgrade, smaller fallback and forced-size failure. Authored layout checks pass. Workbench script validation passes for WORKBENCH, PC, XBOX, PS4 and PS5. The screenshot's exact field capacity and live construction/navigation still require an in-game check; terrain and visibility restrictions remain enforced.

### Progressive survey and bounded refinement (2026-09-05)

Replaced random HQ samples with seed-rotated Halton disk coverage, retaining 2,048 broad anchors per size across the objective circles. After broad coverage, refine up to 32 separated near fits ranked by required modules passed: original anchor plus eight 7.5 m grid offsets, each with 24 headings at 15 degrees. Refinement cannot enqueue more refinement. Existing work-slice and deadline limits apply. Auto completes each size's refinement before downgrade, and placement starts at the exact surveyed heading with a stable retry reference. Reduced shortlisted-anchor duplicate suppression from 25 m to 1 m so local shifts are not discarded. Preserved the user's 600 m AO margin, 350 m player clearance and 850 expanded-entity limit. Removed stray `Gre` at a placer field declaration that prevented compilation.

Extended actual Enforce regression cases for rotation-only and shifted full-base fits, exact-pose revalidation, broad coverage, deterministic sampling, bounded near-fit retention/ranking and per-layout reset. Selection tests completed with zero failures; all five Workbench script configurations and existing authored-layout checks passed. Updated the read-only terrain audit termination condition to include pending refinement.

Kolguyev group-0 audit with seed 12345 completed twice (Rally enabled/disabled): Full and Compact exhausted their broad/refinement budgets; Courtyard yielded 31 candidate poses, three from broad coverage and 28 from refinement, including 15/30-degree headings. These are terrain/obstruction/AO-boundary fits, not independent fields or verified live spawns. Player visibility, construction/navigation and scheduled server frame-time behavior remain live-playtest items.

### Placement throughput without fewer samples (2026-09-05)

Removed the survey's two-full-check limit, retaining the existing 4 ms / 64-pose budgets. Candidate validation likewise uses its 4 ms budget across all 24 headings. Construction keeps the two-module ceiling, adds a 4 ms loop budget, and requests its next callback after 16 ms instead of 200 ms. All anchors, refinements, headings, support/obstruction/visibility checks and size priority remain in place.

Added an exact single-circle convex containment shortcut with the original grid retained for multi-circle cases. Eliminated duplicate HQ pad screening within the same survey pose; live validation still checks every module. Reused authored layout objects for retries and removed temporary site allocations from player footprint-distance calculations.

Validation: actual Enforce selection and optimization regressions completed with zero failures; Workbench validation passed all five configurations. Repeated read-only Kolguyev group-0 seed-12345 audits returned the same 31 poses, compared exactly against the prior log (no layout/center/yaw differences). Callback counts fell from 9,086-9,104 to 3,424-3,462; audit work was 16.9-17.2 seconds. At 16 ms per requested callback delay this reduces estimated scheduler waiting from about 146 to 55 seconds for the full workload. These are audit measurements and scheduling estimates, not live multiplayer/frame-time measurements. Construction throughput still needs a live playtest.

### Dynamic-base garrison defend waypoints (2026-09-05)

Replaced the dynamic garrison's Wait posts with indefinite typed DefendSmall waypoints using the stock Defending/CoverPost preset. Added SetDefendPost on IA_AiGroup, retaining the pinned hold lifecycle and preventing attack/patrol retasking, defend-wave tracking, AO-marker recentering and later radius inflation. Hold recovery requests resolve to DefendSmall before typed-tree handling; existing ordinary hold posts keep their Wait behavior.

Each group's preferred 15 m defend radius is clipped against all four authored sandbag inside faces with a 1 m margin, using layout-local coordinates. No minimum-radius inflation is applied to clipped defend posts. A nonpositive result fails placement. This covers all authored guard positions and existing inward-offset perimeter fallback positions without changing spawn budgets.

Validation: all five Workbench configurations compiled. Actual Enforce regression checks passed for every authored/fallback position across all six layouts and 24 rotations, plus outside-post rejection; zero failures. Live AI combat/cover behavior remains unverified. Waypoint bounds and macro-order pinning do not constitute a physical movement barrier for individual soldiers.

### Interior scenery pass (2026-09-05)

Added six authored scenery prefabs: briefing, mess, stores, water, workshop and utility scenes. The six layouts receive 12/8/6/5/4/3 optional clusters respectively, preserving all sandbag declarations and circulation/guard clearance. Measured stock geometry supplies tabletop heights and reserved pads; explicit child hierarchy components retain scene ownership. Full remains below budget at a conservative 757/850 expanded entities.

Dressing bypasses survey terrain checks and cannot invalidate a site; construction retains support/obstruction checks and may omit individual scenes. All five Workbench configurations and Enforce regressions passed, complete vignette measurement reported zero failures, and layout/asset checks passed. The terrain audit returned exactly the same 31 unique poses as before this pass (final pass: 3,300 callbacks, 15.9 seconds of work). Generator idempotency and unchanged sandbag declarations verified. See docs/dynamic-base-interiors.md and its SVG layout sheet. Live visual, navigation and multiplayer performance checks remain playtest work.

### FOB infrastructure and daily-life pass (2026-09-05)

Upgraded optional interior scenery with Soviet kitchens, bulk water/handwashing, sanitation, covered stores, medical receiving, rest/comms, power distribution, fire equipment, waste and restrained lighting. Full/Compact/Courtyard/Roadside/CommandPost/RallyPost now have 16/12/9/9/9/4 optional clusters and 6/4/3/3/2/1 shadowless lights. Each has one generator ambience source. Full remains at 769/850 conservative expanded entities and 230/256 roots. No sandbag, structural module, placement algorithm or defender changes.

Measured source geometry and final hierarchies, removed interactive/service components from scenery variants, preserved supported props and attached accessories, and enforced hygiene separation and emitter budgets. Actual Enforce construction fixtures passed 177 flat/gentle/steep support attempts, optional-rejection continuation and cleanup with zero remaining entities. Selection/defend regressions passed. The terrain audit matched the previous 31 poses exactly (final pass 3,302 callbacks, 16.158 seconds of work). See docs/dynamic-base-infrastructure.md for evidence and the updated layout sheet.

Live physical-obstruction, day/night visual/audio, combat navigation, late-join replication and multiplayer frame-time checks remain outstanding; the preview fixture cannot verify these. No in-game screenshots or live performance acceptance are claimed.
