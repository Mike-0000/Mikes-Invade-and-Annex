# Smaller dynamic bases

> Historical fixed-layout reference. Runtime placement now uses [composition-based recipes](dynamic-base-compositions.md), including revised footprints. The layouts and validators described below remain legacy Workbench fixtures.

Four additional authored designs are available through the existing **Base size** admin dropdown. **Auto** includes them automatically. Existing saved values remain compatible: Auto = 0, Full = 1, Compact = 2; Courtyard = 3, Roadside = 4, Command post = 5, Rally post = 6. A named selection requires that design.

| Layout | Footprint | Facilities | Occupying guard ceiling | Conservative scenery count |
| --- | --- | --- | --- | --- |
| Full | 180 × 140 m | Existing operating base | 36 | 681 |
| Compact | 120 × 100 m | Existing compact base | 36 | 504 |
| Courtyard | 88 × 76 m | HQ, two barracks, medical, supply | 24 | 310 |
| Roadside | 60 × 96 m | HQ, two barracks, supply, fuel | 20 | 285 |
| Command post | 64 × 56 m | HQ, barracks, supply | 16 | 191 |
| Rally post | 36 × 48 m | HQ and tall perimeter walls | 12 | 89 |

The four new layouts use full-size prefab buildings at explicit positions. Facilities are omitted to reduce the footprint; buildings are not scaled down. Each layout has three entrances, a central approach, a cross lane and dedicated guard posts. Roadside describes a narrow floor plan and does not require or permit building on an occupied road.

The command post uses about 70% less land than the previous smallest layout. The capture radii are 22, 20, 18 and 9 metres respectively. Capturing the command zone now starts the normal defense immediately at the base; there is no separate assembly gate or counterattack countdown. Guard ceilings affect the initial occupying garrison, not counterattack difficulty.

## Capture-to-defense flow and HUD

Capture now hands directly to `IA_DefendMission` at the same base. There is no
regroup roster gate or separate 30-second counterattack warning. Legacy defense
uses the normal legacy hold; Enhanced uses the normal doctrine selection,
PREPARE/PROBE/ASSAULT/CRISIS/SECURE phases, mini-objective scheduling, durations,
notifications and `IA_DefendHud`. The normal defense timers remain; only the
extra base-stage countdowns were removed. Defense settings are snapshotted when
the base objective is selected, as before.

`IA_BaseObjectiveHud` now inherits the existing capture tile's beveled frosted
body, tracked status tab, ring, edge glow and slide/fade timings. Capture adds a
smoothed progress rail; placement uses an indeterminate sweep. Contested capture
pauses rather than predicting loss or resetting progress. Narrow docks omit the
ring to reserve room for copy and percentage. Defense takes over via the existing
shared HUD, not a second base-specific defense display.

The `defense_create` failure after the constructor's "Created defend mission"
message had an ownership gap: the base objective held only a weak mission
reference before registering it with `IA_Game`. The objective and factory locals
now retain `ref IA_DefendMission` through that handoff. Old regroup config fields
and enum/status slots remain reserved for compatibility but no longer gate play.

Validation: Workbench script validation passed WORKBENCH, PC, XBOX, PS4 and PS5.
`python -m unittest discover -s tools -p "test_dynamic_base_flow.py"` checks the
handoff, ownership declarations, defense parity and HUD/protocol contracts. These
are compile/source checks, not a live defense simulation. Playtest capture into
both legacy and Enhanced defense, each enabled doctrine (where terrain permits),
JIP during seize/defense, contested capture, narrow multi-tile docks, cancellation
and completion. Confirm there is no base warning/regroup pause or false failure.

## Placement changes

- Survey up to 2,048 HQ anchors across the approved AO circles during ordinary objectives, retaining up to 64 dispersed locations that pass **whole-layout** terrain, obstruction and AO-boundary checks. The previous 32-center shortlist ranked only an 18-metre patch at the base center, which could be flat while the actual buildings were on slopes or over water.
- Find a usable HQ pad first and derive the base origin from its authored HQ offset. Each heading/layout keeps the HQ on that surveyed patch instead of rotating the HQ away from it. All other buildings retain their authored relative positions.
- Refresh travel-distance ranking when the terminal objective actually starts. Recheck all terrain, obstruction, player-distance, visibility and access constraints before building; precomputed fits are hints, not permission to skip validation.
- Auto tries Full, Compact, Courtyard, Roadside, Command post and Rally post at each anchor, testing eight headings in 45-degree increments. A changed admin size selection restarts the survey with the selected design.
- Spread survey work across callbacks: at most 64 initial HQ checks and two whole-layout checks per callback, yielding between poses after a 4 ms time budget. Final selection checks at most two headings per callback. An individual engine query/whole-layout check can exceed that time budget.
- If the shortlist is exhausted, resume surveying fresh anchors. An immediate admin start can use the partial survey and continue it. The 60-second placement deadline still includes remaining survey work, construction, navigation checks and guard spawning; precomputation during ordinary objectives runs outside that deadline.
- If construction or navigation fails after choosing a design, Auto tries the next smaller design at the same clearing before moving on. All abandoned scenery retains its cleanup owner.
- Check module clearance using the engine's rotated box (`TraceOBB`). Previously a diagonal 24 × 20 m HQ pad became a roughly 31 × 31 m world-axis box, falsely including obstacles outside the reserved rectangle. Reserved pad sizes are unchanged; physical support profiles are described below.
- Terrain rejections are broken down by layout into `footprint_water`, `footprint_height_span`, `pad_water`, `pad_height_span` and `module_obstruction`, plus `anchor_water` for immediately discarded underwater HQ samples. One `Rejection example:` per reason/layout/module records the location, orientation, affected module and measured height difference/limit or hit prefab. Counts now include survey and final revalidation, rather than equaling a fixed number of centers times headings. `Terrain-qualified site:` records a complete terrain fit; player visibility and routes can still reject it. `Terrain survey exhausted` means the anchor budget was consumed. Steep terrain exits as soon as sampled height span exceeds four metres, so its logged span can be a lower bound on the full footprint's range.
- Validate local navigation within one metre of a route sample instead of using the ordinary infantry-spawn helper's 16 metre search and rejecting its distant result.

## Validation and playtesting

### Physical support correction (2026-09-05)

A subsequent hillside test exhausted 2,048 anchors / 81,920 poses. Of those, 80,818 failed pad height checks. The mandatory HQ was still being tested as if its entire 24 × 20 m reserved area needed to be a flat foundation. Increasing search size could not resolve that mismatch.

`Scripts/WorkbenchGame/IA_BaseFoundationProbe.c` measures the installed game models in an isolated preview world. Measured local bounds:

| Asset | Minimum XYZ (m) | Maximum XYZ (m) |
| --- | --- | --- |
| USSR tent body | -3.951, -0.363, -3.917 | 3.972, 4.491, 4.699 |
| USSR tent earth foundation | -7.081, -2.746, -6.419 | 7.072, 0.024, 7.321 |
| USSR tent floor | -2.920, 0.027, -3.014 | 2.942, 0.102, 3.813 |
| Supply shelter and contents, aggregate | -4.293, -0.929, -5.461 | 4.262, 3.852, 5.464 |

HQ, barracks and medical all use the same stock earth foundation, with a +0.1 m authored tent/foundation offset. Their bearing samples now cover the tent body (X ±4 m, Z -4..4.75 m), including interior points on a maximum 2 m grid. They permit at most 0.8 m terrain span and 0.35 m upward composition adjustment. The root height is the greater of center-ground height and highest bearing terrain minus 0.1 m. Tents remain upright and furniture retains its position relative to the floor. These are conservative chosen limits informed by the measured geometry, not measured guarantees of traversability on every terrain profile.

Supply uses its measured 9 × 11 m physical footprint while retaining the stricter 0.35 m span limit and no foundation lift. All other module support rules remain as authored. Reserved clearance pads, overall base footprints and the four-metre whole-site terrain limit remain intact. The HQ uses the plain version of the same stock tent/floor/foundation, omitting the wider camonet and its independent poles. Its separate ground-level sign and decorative dirt mound are also omitted so lifting the foundation cannot leave these accessories suspended. No child transforms are adjusted after spawning.

Clearance boxes test entities from the lowest sampled reserved-pad ground height to above its highest point. The support-height allowance no longer raises the bottom of the collision check. Terrain itself is handled by the separate terrain/support checks so a natural slope is not counted as an obstructing entity.

`pad_height_span` examples now report physical bearing bounds. `foundation_lift` means terrain span passed but would require raising the tent by more than 0.35 m. Spawn rechecks and uses the same support-height solver as candidate validation.

Run the measurement/regression plugin through a separate Workbench process using `-wbModule=ResourceManager -plugin=IA_BaseFoundationProbe -run` plus the normal project/addon arguments. It checks flat ground, gentle uneven ground, excessive span, excessive lift, bearing containment, interior sampling and the supply profile using the actual Enforce support solver. It reports `[IA][SupportTest] completed failures=0` on success. It does not load or modify the active scenario.

`tools/check_dynamic_base_layouts.py` reads the new layouts from the game manifest and checks pad containment, pad overlap, capture bounds, three clear approaches and guard scatter clearance at maximum garrison.

`tools/validate_dynamic_base_assets.py <extracted-data-directory>` checks all six layouts against the 750 entity ceiling, verifies prefab references and checks generated metadata.

These checks cover authored geometry and source assets. They do not establish a placement success rate on live terrain or replace a multiplayer playtest. After recompiling/restarting the scenario, test each named design on a suitable clearing, then test Auto on an AO that previously failed. Confirm that guards spawn in clear areas, capture completes, defense begins immediately at the same site, and cancellation cleans up after players leave. If placement still falls back, the `[IA][Base] Rejections:` lines identify the limiting checks.

The floor-plan illustration is in `dynamic-base-layouts.svg`. It shows reserved pads rather than exact building silhouettes.


## Rally post and on-map audit (2026-09-05)

The smaller fallback is a 36 × 48 m authored fortification: one full-size HQ tent on its stock foundation, 48 tall sandbag sections, three entrances, a nine-metre capture circle and up to 12 initial occupying guards. It omits the separate barracks and supply shelter. Existing settings retain their numbers; Rally post is appended as admin value 6 and Auto's last layout at each surveyed anchor. Capture hands directly to the shared defense implementation.

A repeatable World Editor audit loaded the actual IA_Kolguyev world and ran the production terrain, obstruction and AO-boundary checks against group 0 with seed 12345 and 2,048 sampled anchors. The original five layouts produced **2** shortlisted fits. Including Rally post produced **27**. Terrain tolerances, foundation lift limits, reserved HQ clearance and AO bounds were identical between these runs. This is a static map audit, not a measurement of live mission success rate: player visibility/distance, access routes, constructed geometry and navigation still have to pass during gameplay.

`Scripts/WorkbenchGame/IA_BaseTerrainAudit.c` repeats the comparison using a separate Workbench process with `-wbModule=WorldEditor -plugin=IA_BaseTerrainAudit -run`. The default world is Worlds/IA_Kolguyev.ent; `-iaAuditWorld` can override its path. It reads the world without saving or starting the mission. The editor-world injection is compiled only for Workbench; normal placement uses the current game world. Rejection keys now include module IDs, distinguishing HQ screening failures from later supply, barracks or parapet failures.

The layout checker includes Rally post's pad containment/overlap, three routes and guard scatter at its maximum garrison. The asset checker includes its 49 roots and 89-entity scenery estimate. The updated SVG shows all four smaller layouts.

## Taller, denser perimeter walls

All six layouts now use four stock burlap sandbag variants: solid walls and walls with firing openings (about 2.25 m tall), long high sections (1.35 m), and curved high sections (1.24 m) near run ends. Explicit, evenly spaced wall runs replace the old half-height three-prop clusters. The authored footprints and three entrances remain unchanged. Each short section follows the terrain independently.

| Layout | Sandbag sections | Total scenery roots |
|---|---:|---:|
| Full | 204 | 214 |
| Compact | 138 | 145 |
| Courtyard | 101 | 106 |
| Roadside | 96 | 101 |
| Command post | 71 | 74 |
| Rally post | 48 | 49 |

Placement requires at least 75% of the authored sections overall and 60% on each side, both during the terrain survey and after construction. Obstructed sections may be omitted within those limits. Existing world obstructions are still checked; adjacent sections ignore the same base's own entities during construction because their ends intentionally meet. Scenery ceilings are 256 roots and 750 expanded entities; the largest layout estimates 681 entities before its garrison.

`tools/author_base_perimeters.py` reproduces the explicit wall coordinates and fixed style rhythm. `tools/check_dynamic_base_layouts.py` checks wall containment, building clearance, guard clearance and entrance routes in all six layouts; the four small layouts also retain their interior-route checks. Workbench script validation and the foundation/coverage regression probe pass. In-game movement, combat and multiplayer replication still need a playtest.

The updated read-only Kolguyev group-0 audit (seed 12345, 2048 anchors, all six layouts) found 61 terrain/obstruction fits with these walls. This survey excludes live player clearance and post-spawn navigation.
# Auto placement size priority (2026-09-05)

Auto now searches one layout across the AO before trying the next: Full, Compact,
Courtyard, Roadside, Command post, then Rally post. Previously it tried every size
at each HQ anchor and sorted the mixed shortlist by player distance. That could
select a small nearby post while a larger qualified field remained untried.

Each size receives up to 2,048 seeded HQ anchors and eight coarse headings per anchor.
Shortlists hold up to 64 sites of the same size. Exhausting a batch resumes that
size's remaining anchors; only exhausting its candidates, coarse anchors and local refinement
allows a downgrade. Failed construction tries remaining headings and other sites
of the same size. Forced-size settings still require the selected design.

Precomputation still runs during ordinary objectives. A placement requested before
that work finishes has up to 180 seconds to search; the first construction attempt
starts a separate 60-second budget shared by construction, retries, navigation and
garrison spawning. Search callbacks retain their existing bounded work slices.
Terrain, foundation, obstruction, AO-boundary and player-visibility checks still
apply. This prioritizes the largest legal sampled design; it is not an exhaustive
geometric proof over every possible position in the fields.

`IA_BaseSelectionTest` runs synthetic terrain cases against the production survey
and candidate validator in a separate Workbench process. Use the usual project
and addon arguments with `-wbModule=ResourceManager -plugin=IA_BaseSelectionTest -run`.
It checks a small first clearing versus a larger second clearing, construction
retry size, candidate exhaustion before downgrade, smaller fallback and forced
Full failure. The test fixture is compiled only under `WORKBENCH`.

## Progressive coverage and local refinement (2026-09-05)

The broad pass uses a disk-mapped Halton sequence with a seed-dependent rotation,
interleaved across objective circles. It distributes samples progressively instead
of drawing independent random points. All sizes replay the same spatial sequence.
The current AO margin is 600 m and player clearance is 350 m from the footprint;
this change preserves those user settings and the 850 expanded-entity ceiling.

After broad coverage, each size refines up to 32 separated near-fit anchors. An
anchor qualifies only after the HQ and AO boundary checks pass; the number of
required modules that subsequently pass terrain/obstruction validation ranks it.
Stronger near fits replace weaker ones in the bounded list. At each retained
anchor, refinement checks the original position and eight neighboring positions
on a 7.5 m grid (diagonal displacement approximately 10.6 m), at 15-degree headings.
This adds at most 288 positions / 6,912 poses per size. Refinement does not queue
further refinement. The same callback pose/full-check limits and deadline apply.

Candidate revalidation begins at the exact surveyed heading and retries 15-degree
headings relative to that fixed reference. Nearby anchors are no longer discarded
by the previous 25 m duplicate radius; only anchors within 1 m of a shortlisted
anchor are skipped. Terrain grid spacing remains 10 m, with existing module support
and obstruction checks intact.

The selection regression plugin additionally tests rotation-only and shifted fits,
exact-pose revalidation, deterministic disk coverage, the refinement-list ceiling,
near-fit ranking and reset between layouts. These are synthetic engine tests;
live navigation, visibility and server frame times still need a game playtest.

Validation: all five Workbench script configurations passed and the selection
plugin reported zero failures. The read-only Kolguyev group-0 audit, seed 12345,
found 31 Courtyard fits: three during broad coverage and 28 during refinement,
including 15- and 30-degree headings. Full and Compact exhausted their coarse and
refinement budgets without a fit. Both audit runs (Rally enabled/disabled) produced
the same 31 fits because Courtyard candidates prevent downgrade. These are nearby
candidate poses, not 31 independent clearings. The audit excludes live players,
construction and post-spawn navigation; its tight loop does not measure scheduled
server callback latency.

## Placement speed without reducing search coverage (2026-09-05)

Survey callbacks now use their 4 ms / 64-pose budgets instead of additionally
stopping after two HQ-qualified poses. Candidate revalidation similarly uses
the 4 ms budget while preserving every 15-degree retry. Construction retains the
two-module ceiling and adds a 4 ms loop budget, but schedules the next batch after
16 ms instead of 200 ms. Capture, guard spawning and navigation timing are unchanged.
Individual engine operations are not preemptible, so these are yield budgets,
not a hard guarantee that every callback completes in under 4 ms.

An exact convex-containment shortcut avoids scanning interior boundary points
when all four footprint corners fit inside one objective circle. Cases spanning
multiple circles still use the original grid. Survey no longer repeats its HQ pad
check within the same pose; subsequent live validation still checks every module.
Candidate attempts reuse the authored layout objects, and player-distance checks
compute the same rectangle distance without allocating temporary site instances.

The engine regression suite checks the containment shortcut, including corners in
different circles with an invalid interior gap; distance equivalence at all 24
headings; complete live module checks; and unchanged near-fit ranking when survey
reuses its HQ check. The terrain audit reports both work milliseconds and callback
counts so scheduler overhead can be distinguished from map-loading and validation.

The same Kolguyev seed-12345 audit retained all 31 candidate poses exactly
(layout, center and yaw): no differences against the previous log. Callbacks fell
from 9,086–9,104 to 3,424–3,462, approximately 62% fewer. Measured audit work took
16.9–17.2 seconds, versus approximately 18.1–18.4 seconds previously. The requested
16 ms delays therefore contribute approximately 55 seconds rather than 146 seconds
for the complete audit workload. This is a scheduling estimate, not measured live
mission latency; background precomputation, early candidate selection and frame
rate affect actual waiting. All five script configurations and regression tests
passed. Live construction throughput and server frame times remain playtest items.

## Interior scenery (2026-09-05)

The six layouts now include optional functional scenery clusters, from four in RallyPost to sixteen in Full after the infrastructure pass. See [infrastructure details](dynamic-base-infrastructure.md) and the [layout sheet](dynamic-base-interiors.svg). Existing sandbag geometry is preserved. Optional scenery is checked during construction and does not influence site search or size selection.

## Garrison defend posts (2026-09-05)

Dynamic-base occupying groups now receive persistent SCR_DefendWaypoint orders
instead of Wait waypoints. Each group defends its authored guard post (or the
existing inward-offset perimeter fallback), using a preferred 15 m radius clipped
to the nearest authored wall's inside face with a further 1 m margin. The circle
therefore remains inside the sandbag perimeter on every layout and orientation.
Posts with no positive interior clearance fail placement instead of creating an
out-of-bounds waypoint. Small clipped radii are preserved without the ordinary
Wait waypoint's minimum-radius expansion.

The existing pinned-garrison lifecycle remains active: tactical changes and defend
wave tracking cannot reassign these groups to patrol, attack or Search and Destroy.
Lost-waypoint recovery maps Hold back to DefendSmall at the pinned location before
typed-waypoint tree handling. The ordinary AO marker clamp cannot move the post
away from the dynamic base. The stock Defending/CoverPost preset and indefinite
holding time are applied; later radius overrides cannot enlarge the defended area.

Workbench script validation passed all five configurations. The engine regression
suite checked all authored guard posts and every perimeter fallback in all six
layouts at all 24 headings, including clipped-radius containment and rejection of
outside posts. These tests verify waypoint geometry; individual combat movement
and cover selection still require a live playtest. A Defend waypoint is an AI order,
not a physical movement barrier.
