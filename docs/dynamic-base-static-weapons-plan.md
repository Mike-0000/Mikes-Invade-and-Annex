# Dynamic-base machine-gun emplacements — implementation plan

Status: historical standalone-tripod proposal, superseded by [the implemented composition-based design](dynamic-base-compositions.md). Its unchanged-layout/no-platform and PKM/NSV-only boundaries below are no longer the active specification. Retained for the earlier implementation rationale and legacy fixtures.

## 1. Objective and boundaries

Add usable, initially AI-manned machine-gun positions to dynamic bases. They should provide recognizable defensive positions during the assault and remain available for players during the subsequent defense. Use existing base-game tripod weapons, finite ammunition and existing garrison personnel. Preserve sandbag geometry, structural layouts, largest-first site selection and existing infantry behavior.

V1 includes Soviet PKM and NSV tripod weapons only. Exclude optics variants, US weapons, mortars, grenade launchers, missile systems, new fortifications, extra defenders, unlimited ammunition and automatic repair/replacement. No map or HUD indicators are required. Existing optional infrastructure remains in place wherever it fits.

Tripods stand on supported ground behind suitable existing openings or low sections. Do not float weapons on sandbags, raise guns artificially to clear walls, remove panels, widen entrances or add platforms. A base with zero suitable weapon positions remains a valid base.

## 2. Verified starting points

- Installed stock resources include `Prefabs/Weapons/Tripods/Tripod_6T5_PKM.et` and `Prefabs/Weapons/Tripods/6T7/Tripod_6T7_NSV.et`. Both expose turret/compartment machinery. PKMN, optic variants, M60 and M2HB also exist but are outside V1. Source installation: `D:/ReforgerGameSources/data/data007`.
- PKM storage declares three 100-round 7.62×54 mm boxes; the NSV base declares three 50-round 12.7×108 mm boxes. These declarations alone do not establish total ammunition: the mounted weapon's loaded magazine must be included and measured.
- `IA_DynamicSiteLayout.c` defines the panels, guard posts, perimeter stations and optional dressing. Perimeter stations are panel-derived locations, not verified firing positions.
- `IA_DynamicSitePlacer.c` constructs modules, checks navigation, creates the garrison and waits for readiness before reveal. Current infantry groups receive the whole-base area resolved by `IA_BaseGarrisonArea`; preserve that current behavior, including its documented soft boundary.
- `IA_AI_Group.c` already seats mortar gunners through a turret compartment. Its implementation also distributes mortar shells, handles mortar targets and overrides orders. Reuse the seating mechanics, not the mortar behavior or `m_isMortarCrew` flag.
- The site owns roots and garrison groups. Successful defense calls `RetireSite`; normal cleanup waits until players are more than 600 m from the footprint, checked every 8 seconds.
- The source-level infrastructure baseline is 230 roots / 769 expanded entities for Full, against 256 / 850 limits. Recalculate from the current working tree during implementation; do not rely on these numbers remaining unchanged.

Reference: [Bohemia's asset catalog](https://community.bistudio.com/wiki/Arma_Reforger%3AAssets). Installed resources and actual engine probes are authoritative for this project.

## 3. Loadouts and gameplay rules

These are maximum successful installations, not minimum requirements:

| Layout | PKM | NSV | Maximum dedicated gunners |
| --- | ---: | ---: | ---: |
| Full | 3 | 1 | 4 |
| Compact | 2 | 1 | 3 |
| Courtyard | 2 | 0 | 2 |
| Roadside | 2 | 0 | 2 |
| CommandPost | 1 | 0 | 1 |
| RallyPost | 1 | 0 | 1 |

- Prefer different perimeter sides. Full covers up to four sides; Compact up to three; two-gun layouts use different sides. Prioritize front and side approaches. Put an NSV on a side approach rather than directly facing the main entrance; preserve an approach without a heavy-machine-gun position.
- Minimum station separation: 12 m, measured between gun roots. Never place a gun or its mounting/access envelope in a reserved entrance or circulation lane.
- Each PKM starts with exactly 400 rounds: one loaded 100-round belt and three reserve belts. Each NSV starts with exactly 200 rounds: one loaded 50-round belt and three reserve belts. Normalize inherited inventory so this is the total, not an addition to stock ammunition. Use the stock ball/tracer and AP/APIT boxes identified above; no automatic replenishment. Players may manually reload with compatible ammunition they bring.
- Initial AI gunner count is `min(installed weapons, floor(garrisonBudget / 4), max(0, garrisonBudget - 4))`. Each gunner consumes one existing defender slot. Allocate those one-person groups first, then apply the existing infantry grouping rules to the remaining budget. Preserve the exact original total budget and normal faction/loadout selection.
- An installed gun that cannot receive a gunner remains usable and unmanned. Gunner death does not generate another soldier or automatically recruit a roving infantryman. A surviving crew member whose gun becomes unusable returns to ordinary whole-base defend behavior.
- Players can enter surviving guns and use remaining ammunition without an ownership transfer UI. Do not eject players, reserve occupied seats for AI, refill on capture or change unrelated faction behavior.

## 4. Assets and authored station profiles

Create dedicated `IA_Emplacement_PKM` and `IA_Emplacement_NSV` prefab variants inheriting the armed stock tripod prefabs. Preserve weapon, turret, sight, compartment, ammunition, AI usage and replication components. Do not send these through the scenery component-stripping tool.

Remove pack/disassemble and mounted-weapon removal actions from these site-owned variants; retain enter, exit, aiming, firing, reload and ammunition access. Disable any inherited automatic resupply or supply-service behavior. Keep damage behavior, but suppress persistent detached tripod/gun/wreck spawns that would escape site ownership; allow only stock transient effects with verified finite lifetimes. A destroyed gun must become unusable and stay associated with its site until cleanup.

Measure each complete variant in Workbench: all inventory/weapon children, three tripod contact points, gunner seat, entry/exit position, lowest muzzle positions and barrel sweep. Record resource GUIDs and these measurements in a checked-in emplacement manifest. Count runtime-created entities as well as prefab references. Do not approximate the weapon as a single prop.

Add a separate per-layout `m_aEmplacements` array and a small `IA_DynamicBaseEmplacementSpec` record containing stable ID, weapon type, local transform, supporting panel ID, perimeter side, contact points, gunner/access envelope, approved yaw limits and conservative entity estimate. Keep these specifications out of `m_aModules` so ordinary terrain-survey validation cannot accidentally start testing them.

Author no more than eight candidate stations per layout. Derive candidates from existing window/low-panel positions, using fixed inward offsets of 1.5, 2.0 and 2.5 m, and measured gunner geometry. Select the first geometrically valid offset in that order; discard the panel when none fits. Persist the resulting local transforms rather than repeating this authoring search at runtime. Stable panel IDs and fixed ordering make results reproducible.

Each station reserves the measured swept weapon and seated-gunner envelope with 0.25 m padding, plus a 1.2 m wide by 2 m deep inward access strip. Include these envelopes in offline route, guard-post and infrastructure checks. Reposition only optional dressing that conflicts, using its existing authoring rules. Never move the walls, structural modules or existing guard spawn positions to fit a gun.

Use an outward yaw sector of ±30 degrees from the panel's outward normal and an elevation sector of -5 to +20 degrees, intersected with the stock weapon's mechanical limits. Constrain the actual turret to that approved sector, including when players operate it. If the resulting sweep cannot clear the actual wall across this sector, reject that station. The authoring probe must confirm which turret limits are configurable and write the measured values into the station manifest; inability to enforce the limits blocks that weapon variant rather than allowing unrestricted traverse.

## 5. Runtime placement and performance

Add a bounded emplacement phase after structural modules and sandbags, before optional dressing and garrison creation. Preserve the original order among structural modules and panels. Later dressing continues to use existing volume checks and may be omitted if a successful gun occupies its reserved space.

Process authored candidates in stable side-balanced order. On Full/Compact, try the designated NSV side first, then PKM candidates on the remaining sides. If the NSV cannot fit, allow a PKM on that side within the same total-gun cap. Never expand the spatial search, add random headings or retry an identical failed candidate.

For each candidate:

1. Resolve its transform from the selected base's exact origin and heading. Require the referenced supporting panel to have actually spawned; missing optional panels invalidate only their dependent weapon stations.
2. Check all three feet and the gunner standing point. Maximum contact-height spread is 0.15 m and maximum tilt is 5 degrees. Keep the stock upright tripod pose; no custom foundations or forced vertical lift.
3. Trace the swept weapon, gunner and access envelopes against terrain and existing entities. Do not exclude the entire base or its sandbags. Use the live navigation/standing checks to verify access from inside the perimeter.
4. Validate muzzle clearance at sector center and ±15/±30 degrees, using low, level and high usable elevations. No barrel/wall collision is acceptable. Trace the initial 3 m of every tested firing direction against the actual panel and nearby assets.
5. Require an unobstructed level center ray to 50 m and at least three of five level sector rays clear to 25 m. Terrain and props count as blockers; transient characters do not determine the permanent firing-arc profile. Reject blocked stations rather than altering walls or widening the allowed yaw.
6. Check root and expanded-entity headroom, then spawn the weapon and verify its actual hierarchy, ammunition and compartment. Register ownership immediately. If verification fails, remove only that attempted emplacement and continue. No optional weapon may fail the overall base or cause a smaller layout to be chosen.

Use the existing 4 ms construction work budget with at most one emplacement completed per callback and 16 ms continuation scheduling. Split tracing into resumable steps so a candidate cannot run an unbounded batch. Persist progress per candidate. No new survey work or per-gun frame loop is permitted.

Count successful emplacements in the existing 256/850 site limits and retain headroom for later modules. If measured costs exceed available headroom, omit the lowest-priority gun; do not raise limits or silently discard structural facilities. Keep initial player-distance/visibility and final reveal checks intact.

## 6. AI ownership and lifecycle

Add an explicit static-machine-gun assignment to `IA_AiGroup`, separate from mortar crew state, with operations equivalent to `AssignStaticGun`, `TickStaticGunAssignment` and `ReleaseStaticGunAssignment`. Store site serial, gun entity, assigned agent, seat reservation, assignment state and retry deadline. The site owns an ordered list of successful weapon records and their optional crew groups.

Before initial mounting, clear the crew group's conflicting Defend/Patrol orders while the soldier is still on foot. The existing mortar code documents that clearing Defend after seating can force a dismount. Confirm a living eligible gunner and an empty, unreserved valid seat; never steal another actor's reservation. Use the existing server-side compartment-access seating mechanism only during hidden initial base construction. Do not issue an additional GetIn message after successfully seating the agent.

Give initial assignment at most three attempts, 1 second apart, bounded by 5 seconds after the soldier finishes spawning. Never wait indefinitely for a gun. When mounting fails, release the reservation and return that same soldier to normal infantry defense. Infantry readiness counts all existing crew groups normally; a mounting failure does not fail base readiness.

Once visible, use stock turret aiming and combat perception. Prevent patrol, search-and-destroy, defend-wave tracking and ordinary waypoint recovery from retasking an actively assigned crew. Do not run mortar targeting, shell allocation or artillery routines. Preserve current whole-base defend behavior for all ordinary infantry.

Tick assignment health on the site's existing server lifecycle at a 1-second maximum frequency. On death, destruction, empty usable ammunition, unexpected dismount or lost ownership, release assignment once and stop automatic remounting. A surviving soldier receives ordinary defense orders at a valid interior location. On ammunition exhaustion, allow up to 10 seconds for a stock reload before declaring the gun exhausted; count loaded and reserve ammunition before making that decision. Pair every LOD override and reservation with a corresponding release.

Cancel assignment activity on capture, retirement, host shutdown, construction failure and serial replacement. On capture, surviving original hostile crew must not remount or contest a player seat through stale callbacks. This is not an allegiance conversion or a mechanism to delete surviving enemies: release their weapon assignments and let existing objective/host behavior govern them.

## 7. Player use, replication and cleanup

Keep gameplay mutations server-authoritative. Set initial gun affiliation to the site's resolved enemy faction while retaining the Soviet artwork; verify this does not prevent later player entry. Replicate guns and their stock weapon/compartment state through site-owned roots; do not broadcast a duplicate visual weapon or custom ammunition simulation. Client and late-joining players should observe the same crew, traverse limits, damage, ammunition and occupation state.

Track actual panel entities and weapon roots explicitly rather than inferring them from root-array offsets. Preserve this association even if a weapon is damaged or a panel disappears. Do not delete or reposition a visible gun merely because its supporting panel was later destroyed.

At retirement, stop assignment retries, cancel pending crew spawns and release reservations. Keep the existing player-proximity protection. Add an explicit player-occupant check across registered weapon compartments so an occupied gun cannot be deleted because of a stale or missing sampled position. AI occupancy must not keep the site alive forever; use the normal host retirement/disembark lifecycle before final deletion.

Delete the weapon hierarchy and attached inventory with other site roots, then rebuild navigation as today. Do not recursively destroy a player entity, player inventory or legitimately transferred ammunition. Transferred ammunition is ordinary player loot and no longer owned by the site. No mount/gun assembly can be carried away because its disassembly/removal actions are disabled. Persistent detached weapon/wreck entities are prohibited by the asset policy above.

Verify both completed-defense and abort paths. Current `OnDefenseEnded(false)` leaves the site in a failed state rather than retiring it immediately; preserve that existing mission policy. Ensure gun assignment stops while failed, and subsequent objective cancellation/admin retirement performs the same deferred cleanup. Do not change global failure handling as a side effect of this feature.

## 8. Configuration, implementation order and diagnostics

Add one persisted mission setting, `DynamicBaseEmplacementsEnabled`, default true for new configuration snapshots and when missing from an existing configuration. Resolve it once when starting site construction. Disabled means no weapons, crews or assignment ticks for that new site; do not remove guns from a site already active. Use the project's current configuration persistence mechanism. No new admin menu is required for V1.

Implementation sequence:

1. Create and measure the two armed prefab variants and their inventory/seat/destruction behavior.
2. Author the station manifest for all six layouts; extend geometry and asset-budget checks.
3. Add optional station construction, panel association, ownership and config snapshot handling.
4. Add bounded crew assignment and ordinary-infantry fallback; preserve existing mortar and infantry tests.
5. Integrate capture/failed/retired transitions, player occupancy protection and cleanup.
6. Complete runtime validation before enabling the feature in a release build.

Route diagnostics through the current `IA_Log` mechanism. Emit one debug summary per site: attempted/installed weapon types, omission reasons, crew assignments/fallbacks and construction work time. Log unexpected cleanup/ownership failures at warning level. Do not display technical placement or crew diagnostics in player HUD text and do not log every assignment tick.

## 9. Acceptance tests and release gate

### Automated and Workbench

- Compile all supported script configurations; verify all prefab resources, GUIDs, initial ammunition totals, actual expanded counts and complete child ownership.
- Validate all six layout manifests at 24 base headings. Confirm unchanged sandbag/structural declarations, route and guard clearance, station separation, side priority and configured gun caps.
- Test flat/gentle/steep support, missing panels, blocked gunner access, blocked muzzle rays, insufficient field of fire, resource failure and budget exhaustion. All such failures omit the station without changing the base candidate or blocking readiness.
- Repeat a fixed seeded terrain audit against a baseline captured from the current working tree: identical selected sizes, positions and headings, including when emplacements are disabled.
- Test crew totals at low and normal garrison budgets; failed spawning/seating; exhausted retry deadline; reservation conflicts; gunner death; destruction; ammunition depletion; capture; failure; cancellation and stale serial callbacks. No duplicate crew, extra budget consumption, indefinite readiness wait or repeated remount occurs.
- Test actual scene cleanup, not only clearing tracking arrays. Include attached magazines and occupied/dead crew. Preview worlds that lack active physics or networking cannot satisfy live collision or replication assertions.

### Required live session

- For every size, inspect daylight and darkness from gunner and player eye level. Fire across the entire allowed traverse/elevation sector: rounds and barrel clear the bags, the seated soldier fits, and players can enter/exit from inside the base.
- Observe original defenders attacking visible targets from the guns, reloading from finite reserve belts and returning to infantry defense when appropriate. Ordinary infantry keeps its current whole-base defend behavior; mortars remain unaffected.
- Capture a base and use its surviving guns during defense. Confirm no hostile automatic remounting, ammunition reset, player ejection or new interaction lock.
- With a dedicated server and at least two clients, test late join during occupation, firing, reload and damage. Test defense completion/abort, player lingering within 600 m, a player seated in a gun, final departure and deletion. Repeat several mission cycles and verify no accumulating entities, reservations, crew callbacks or sounds.
- Capture overview, gunner-eye and rear-access screenshots for each successful weapon/layout combination. Record intentional omissions where the unchanged walls cannot support a safe station.
- Compare feature off/on using the same seed, scene, player count and rendering settings: five repetitions after warm-up. Record survey work, construction latency, server script/AI time, client median/p95 frame time and entity totals. A repeatable >5% regression in survey work or steady-state frame time blocks release; reduce optional gun count or assignment work before changing placement quality. Report construction latency separately because new physical modules necessarily add work.

Release only after live firing, crew behavior, player takeover, replication and deferred cleanup pass. A useful partial rollout is achieved by lowering successful gun counts or disabling the setting, never by weakening muzzle/seat clearance checks. Ship the measured manifest, updated layout sheet, screenshots and test results alongside the implementation.
