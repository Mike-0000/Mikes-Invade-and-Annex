# Dynamic enemy base: seize, regroup, defend

Status: proposed implementation plan. No gameplay changes made. Numerical values below are starting points for playtesting, not verified engine limits.

**Implementation handoff:** Read [DYNAMIC_BASE_IMPLEMENTATION_RUNBOOK.md](DYNAMIC_BASE_IMPLEMENTATION_RUNBOOK.md) after this overview. The runbook resolves implementation choices, specifies interfaces and settings transport, and provides ordered work packages and acceptance cases. Where this overview offers alternatives, the runbook's v1 decisions take precedence. Existing source code takes precedence over line numbers and API examples in either document.

## Recommended experience

After the required objectives in an AO are complete, reveal a newly placed enemy operating base. Players converge, assault it, capture its command area, reorganize inside the base, then defend that same position against a counterattack. Only then does the AO complete.

Make this a separate mandatory objective once selected, not an optional defense event or another initial objective that encourages players to split up. Optional mortar pits retain their current optional status. Start with a 100% selection chance per completed AO, subject to finding a valid location. Allow admins to change the percentage or disable the feature.

The base should feel like a substantial field installation: approximately 180 × 140 m for the main layout, with organized command, accommodation, medical, supply, and vehicle areas. Use a separately authored 120 × 100 m layout when the larger one cannot fit. Do not scale down models or randomly squeeze buildings together.

The assault is a closing action, not a second full AO: target roughly 5–8 minutes of movement and fighting under ordinary conditions. Keep the existing configured defense duration initially, but measure the added total AO time; the enhanced defense config currently defaults to 18–22 minutes before event adjustments.

## What the current code establishes

| Existing system | Finding and implication |
| --- | --- |
| `IA_MissionInitializer.CheckCurrentZoneComplete`, around line 828 | Required captures lead directly to `CheckAndStartDefendMission` or AO completion. This is the integration point. Mortar pits do not gate progression. |
| `IA_MissionInitializer.CheckAndStartDefendMission`, around line 2819 | Requires an authored `DefendObjective` marker, rolls an 80% chance, and immediately starts defense. Dynamic base selection must have explicit precedence over this branch. |
| `IA_EnhancedDefendDirector.Start/Update`, around lines 64–124 | Begins preparation and an initial beat; contact or preparation timeout starts the clock. This does not wait for the player force to assemble. |
| `IA_DefendEvent.SpawnFob`, around line 791 | Already uses Soviet command/barracks tents, antenna and sandbags. Piece yaw rotates, but `OffsetFlat` adds world-axis offsets. A larger layout needs a single transform for offsets and orientations. |
| `IA_SpawnPlacement.FindFlatEventSite`, around line 1123 | Checks a 12 m radius footprint and eventually returns the flattest candidate even if strict checks failed. That is insufficient for a large base. |
| `IA_AreaMarker.ShouldCloseCaptureForDefendOrShutdown`, around line 1861 | Starting an active defense retires non-mortar capture. Do not register the base assault as an active defense. |
| `IA_DefendMission.CollectAffectedAreas`, around line 422 | Chooses the closest existing area as its wave host. Supply the new base host explicitly instead of relying on proximity. |
| `IA_MissionInitializer.ForceFinishCurrentAreaInstancesExceptDefend` | Existing cleanup deliberately preserves the defense host. Extend ownership protection across base spawning, capture, regroup and defense. |
| `IA_AdminOverrides`, `IA_AdminConfigUtil`, admin menu and player controller | Existing live settings, packed transport, authorization and persistent profile mechanisms should carry the new controls. |

The checked-in `IA_AI_Group.c` and `IA_AreaMarker.c` already have local modifications. Implementation must preserve and account for those changes.

## Mission flow and selection policy

Server-owned flow:

`Ordinary objectives → Select/validate base → Spawn → Seize → Regroup → Defend → AO complete`

1. At the start of an AO, precompute a bounded shortlist of terrain/layout candidates. Spawn no base objects yet.
2. Once required objectives complete, latch a single end-of-AO decision using the AO activation identity. Stop repeated completion checks from rolling or spawning again.
3. If the feature is enabled and its percentage roll succeeds, revalidate candidates against current players, vehicles and terrain occupancy. It works without an authored defense marker.
4. Build and validate the site before announcing the assault task. Its garrison must be ready before players are directed there.
5. Capture and regroup happen at this base. Starting defense uses the same location and base entities. There is no second movement order and no second defense chance roll.
6. If selection is disabled, misses its roll, or placement fails, use the existing authored-defense path, including its existing 80% chance, or complete the AO if no defense is selected.

Thus 100% means “attempt this end-of-AO objective every time,” not “100% of the existing 80% defense roll.” Successful base selection replaces the old defense-location selection for that AO. It does not add two defenses.

For GM-directed AOs, respect the existing distinction between staged and live content. Recommend automatic base objectives off in director mode by default, with a separate opt-in; a force-base action uses the same validation and cannot bypass placement requirements.

## Capture and regroup rules

### Seize

- One shared, clearly named task: **Seize the enemy operating base**. Reveal its marker and full site area together.
- Place an approximately 35 m capture radius around the command area. Players can approach through multiple entrances, clear the compound, and secure the headquarters.
- Require at least one living friendly player on the ground in the command zone and no effective hostile combatants in that zone. Do not require every enemy across the whole base to die; a stray AI in an outer tower must not block completion.
- Use about 90 seconds of uncontested capture. Hostile contest or absence pauses progress. This is a per-objective rule, not a global change to ordinary capture balance.
- Reuse capture HUD/task/contribution infrastructure with an explicit policy for this objective. Existing capture is based on numerical faction superiority; do not accidentally inherit that if the intended rule is an uncontested command zone.
- Spawn a finite, population-scaled garrison in authored positions: gate guards, perimeter pairs, central reserve, HQ protection. Begin with roughly 12–36 soldiers across supported population bands, governed by existing AI scaling and a total budget. Validate the actual bands in playtests.
- Do not run defense waves during capture. Include any ambient/QRF actors in the budget, and prevent old AO systems from continuing to produce unrelated pressure indefinitely.

### Regroup

Capturing alone does not solve the spread-out-player problem: one fast squad could capture while the rest are still travelling.

- On capture, show **Base secured — regroup and prepare**. Keep the site physically unchanged and make the assembly radius approximately 150 m around the base center.
- Wait at least 90 seconds. After that, transition when approximately 60% of the eligible deployed force is present. Cap the assembly wait at 240 seconds, followed by a clearly displayed 30-second counterattack warning.
- Define the eligible roster from friendly players participating in this AO at capture time. Exclude spectators and players who never deployed from HQ. Disconnects leave the roster; late joins do not increase the current target. Allow a bounded recovery grace for casualties/respawns so a wipe does not instantly lower the target to one survivor.
- Count only living players actually at ground level in the assembly zone as present. Passing over the base in a helicopter does not count.
- The time cap allows stragglers to opt out; it must not start an empty defense. If nobody is present, display **Awaiting friendly forces**, hold the transition, and allow normal admin skip/cancel. Do not silently grant capture or advance the AO.
- Offer a modest, bounded friendly-compatible medical/ammunition resupply once secured. Its functionality and stock must be explicit; enemy ammunition props alone are not a reliable reward for friendly loadouts. Defer new respawn, arsenal and vehicle-spawning services to a later decision.

### Defend

Start the existing selected legacy/enhanced defense at this same base after the warning. For enhanced defense, provide an entry option indicating that preparation already happened, so it does not roll a second preparation window or trigger a premature first beat. Legacy defense begins only at this handoff too.

Retain current defense timing and completion rules in the first iteration. A new loss/recapture mechanic is a separate balancing decision. Explicitly test what happens if the force leaves after defense begins, since current defense completion is timer-based.

Use authored approach directions for infantry and suitable roads for vehicles. Validate actual paths through the base; do not direct all attackers into a single narrow gate. Delay optional off-site defense events until the first local counterattack is established so regrouping has a useful payoff. Exclude the base footprint from off-site event spawning and prevent overlap with optional mortar sites.

## Base composition and visual design

Author the layout in Workbench using local coordinates and explicit sockets. Runtime logic selects a validated location, layout variant and overall heading. It does not scatter props.

Recommended conceptual arrangement, relative to a front gate at the bottom:

| Part | Placement and purpose |
| --- | --- |
| Command area | Rear-center HQ tent and briefing yard, accessible from the central lane; capture anchor here. Antenna nearby with clear space. |
| Barracks | Two orderly rows along one side, entrances facing a shared pedestrian lane. Several tents create the scale of an operating base. |
| Medical area | Opposite the accommodation area, sheltered from direct gate fire and reachable from the main lane. |
| Motor pool/workshop | Front-side yard near the vehicle gate, with aligned parking bays and room for trucks to turn. |
| Supply stores | Near the motor pool for unloading, clear of the capture anchor and separated from fuel. |
| Fuel/service corner | Separate edge compartment with vehicle access; avoid an explosive cluster beside the assembly point. |
| Perimeter | Purposeful sandbag positions, limited towers and barriers with at least three usable approach/entry lanes. Cover supports both assault and defense. |
| Central circulation | Approximately 8–10 m main vehicle lane, a turning/unloading area, and 2–3 m pedestrian clearances as initial authoring targets. |

The 180 × 140 m envelope includes separation, lanes and yards. It is a size target, not a measured fit of the inspected prefabs. Measure real bounds, entrances, foundations and collision in Workbench before committing socket coordinates. Scale comes from multiple functional compounds and clear circulation, not a prop count alone.

Create one polished full layout and one polished compact layout first. Later variants can exchange a workshop wing, medical wing or storage module at compatible sockets. Avoid arbitrary positional jitter; small clutter variation belongs inside authored modules. Fuel, entrances, turret arcs and walking lanes are never random clutter locations.

For terrain placement, rotate all local offsets and local orientations with the same root transform. Keep buildings/tents upright; accept them only where their support footprint is sufficiently level. Terrain-following wall sections can use explicitly permitted pitch/roll. Do not apply whole-base slope tilt or independent child snapping that displaces furniture from its tent.

### Verified base-game asset starting points

All paths below were found under `D:/ReforgerGameSources/data/data007/`. They are source candidates, not a guarantee that a composition is safe to spawn unchanged.

| Use | Base-game resource path |
| --- | --- |
| HQ | `Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et` |
| Barracks | `Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et` through available authored variants |
| Medical | `Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Medical_USSR_01.et` |
| Supply tent | `Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Supply_Large_USSR_01.et` |
| Antenna | `Prefabs/Props/Military/Compositions/USSR/Antenna_02_USSR.et` |
| Cover reference | `Prefabs/Compositions/Slotted/SlotFlatSmall/SandbagPosition_S_USSR_01.et` |
| Tower reference | `Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_USSR_01.et` |
| Workshop reference | `Prefabs/Compositions/Slotted/SlotFlatMedium/VehicleMaintenance_M_USSR_01.et` |
| Fuel reference | `Prefabs/Compositions/Slotted/SlotFlatSmall/FuelStorage_S_USSR_01.et` |

Inspect nested prefab inheritance before selecting pieces. For example, `Headquarters_S_USSR_01.et` contains a supply spawn point, campaign HQ radio and military-base logic; `FieldHospital_M_USSR_01.et` inherits a buildable composition and includes disassembly actions. Use their art/layout as references and construct IA-owned modules with only intentional behavior. Audit tent subcompositions too, including embedded furniture, weapons, supplies, destruction and replication settings.

Use USSR art for the initial base-game enemy presentation. Resolve infantry and vehicle factions through existing configured enemy selection, with one stable site faction context. If an admin selects a faction without matching base art, explicitly report the USSR art fallback in admin diagnostics; a US/FIA/modded art pack can follow later. Do not silently override enemy AI settings just to match scenery.

## Location selection and validation

This is the highest-risk implementation area. Existing small-site placement is a source of reusable primitives, not a complete large-base placer.

1. **Define AO bounds.** Current code groups marker circles rather than providing a single authoritative AO polygon. Prefer an optional authored operating envelope; otherwise derive the eligible region from primary objective circles plus an explicitly configured near-objective margin (initially 350 m). Avoid a giant bounding circle that includes unrelated terrain between widely separated objectives. The complete base footprint must fit the approved region. Radio towers and optional mortar pits should not pull the site away from the principal AO.
2. **Balance access.** Score candidate travel distance for the deployed force, favouring a central reachable location rather than the last captured marker or a distant arithmetic centroid. Target roughly 400–800 m movement for the main participating cluster where geography permits; penalize very long journeys for the other active squads. These are soft preferences, not a requirement to remain that far from every player.
3. **Prevent visible materialization.** Require at least 250 m from every player to the nearest edge of the proposed footprint, including vehicle occupants. Check visibility to representative base points, not only its center. Prefer screened sites; if candidates remain clearly visible, fail the placement attempt rather than building in front of observers. Recheck during staged spawning and roll back if necessary.
4. **Validate the full footprint.** Sample a terrain grid and every module support pad; test water, local slope, elevation spread, buildings, trees, rocks, existing compositions, road obstruction and vehicle/player occupancy. Use horizontal AO distances and proper height tests. A flat center cannot validate a 180 m compound.
5. **Validate access.** Require at least two independent usable infantry approaches and connected interior lanes. Prefer a nearby road with a tested gate connection for motorized counterattacks; disable incompatible vehicle beats when access fails. A point on the navigation mesh is not proof of a complete route.
6. **Try bounded alternatives.** Test headings and authored full variants, then compact variants, then another approved candidate. Never relax essential collision/water/clearance requirements just to reach 100%.
7. **Commit in stages.** Preflight resources and all placements, spawn structural modules in bounded batches, update navigation, validate critical routes, then spawn garrison and announce the task. Roll back all owned content on critical failure. Decorative omissions are allowed only when marked optional.

Precomputation is an optimization: final validation must occur after ordinary objectives finish. Record layout seed, candidate scores, rejection reasons, elapsed placement time, expanded entity count and selected transform for repeatable debugging.

Base-game `SCR_AIWorld` exposes entity/area navigation rebuild requests. Inspect its collection behavior for nested IA modules and request bounded rebuilds; do not assume one request per prop or an arbitrary sleep proves navigation is ready. Use available engine completion/status information where supported, plus bounded route validation and a failure path. Cleanup also needs navigation rebuild coverage after obstacles disappear.

## Proposed code structure

Keep this extensible enough for later dynamic objectives without building a general mission scripting framework first.

- `IA_DynamicObjectiveDirector.c`: owns one terminal objective per AO activation, selection, state transitions, cancellation and one-time progression handoff.
- `IA_BaseAssaultObjective.c`: implements spawning/seize/regroup and hands its live site to `IA_DefendMission`.
- `IA_DynamicSitePlacer.c`: candidate evaluation and whole-footprint validation, using existing placement helpers when their guarantees fit.
- `IA_DynamicSiteLayout.c` plus IA-owned prefabs/configs: authored modules, footprint/support metadata, local transforms, guard posts, capture/assembly anchors, gates and approach sockets.
- `IA_DynamicSiteInstance.c`: explicit ownership of site roots, spawned actors, reservations, task references, faction and cleanup/navigation bounds.

Define a small objective lifecycle contract: begin, update, cancel, terminal result and owned site. Later depot, relay or vehicle-recovery objectives can implement it. Do not represent this phase as `IA_DefendEvent`, whose lifetime and reward logic belong inside an already active defense.

Integration changes:

- `IA_MissionInitializer.c`: route required-objective completion through the new director; keep AO completion pending through all base phases; explicitly account for skip, restart and GM paths.
- `IA_Game.c`: retain/update the active director or expose it through the initializer, with a single clear update owner. Do not run both a queued tick and game tick for the same objective.
- `IA_DefendMission.c`: accept an explicit live host area and a preparation-already-complete handoff; let the director own the final AO completion callback for this path. Existing authored defenses retain their current callback.
- `IA_EnhancedDefendDirector.c`: support the prepared handoff, base approach/event exclusion constraints, and delay the first off-site event.
- `IA_AreaInstance.c`: create a dedicated runtime host with standard initial AO population disabled; transfer/track surviving garrison appropriately and include it in the total AI budget. Freeze its original enemy garrison behavior after capture.
- `IA_AreaMarker.c` and HUD publishing: reusable capture policy and objective identity, without adding the terminal base to the ordinary required-marker counter. A transient base marker must not be hot-added as another ordinary AO objective.
- Config/menu/persistence files and `IA_PlayerController.c`: new settings through existing authorized server paths, with version-tolerant transport/defaults.

Do not have both the base director and `OnDefendMissionComplete` advance the AO. Mark completion once, preserve the host until defense has ended, cancel pending spawns, then schedule proximity-aware cleanup. Player-occupied vehicles or an occupied compound must not disappear at the success toast. Track outstanding delayed work by activation identity so it cannot spawn into the next AO.

Replicate the objective phase, stable site identity, task location, capture progress, assembly counts and server deadlines. Joining clients must reconstruct current state without relying on a notification they missed. Verify actual replication of nested props, destruction and any service interactions on a dedicated server.

## Admin controls

Keep the main interface short:

| Control | Proposed default | Meaning |
| --- | --- | --- |
| Dynamic base objectives | Enabled | Global feature switch. |
| Chance after AO objectives | 100% | One roll per AO activation; 0% has the same selection effect as disabling. |
| Automatic bases in GM mode | Disabled | Keeps manually directed sequencing explicit. |
| Base size | Auto | Full first, authored compact fallback. |

Expose capture duration, regroup minimum/maximum, assembly percentage, garrison multiplier and AO margin as advanced settings once the prototype establishes useful ranges. Placement safety requirements are validated limits, not sliders that can force invalid geometry.

Persist using the existing profile override mechanism. Add an explicit presence/version marker for new fields so old JSON or shorter packed payloads preserve intended defaults instead of disabling the feature accidentally. Snapshot settings when an objective is selected; subsequent edits apply to the next selection.

Preserve **Complete AO** and **Complete + Defend** meanings. Complete AO cancels any active base phase safely and completes once. Complete + Defend can retain its direct authored-defense behavior; provide a separately labelled **Start base objective** for this new route. If the new site is already active, admin actions must transition/cancel it rather than spawn a duplicate. Document behavior when no suitable authored defense marker exists.

## Delivery sequence and acceptance checks

1. **Author and prove the full layout.** Place it manually on a test site. Verify module inheritance, dimensions, doors, furniture, cover, vehicle turns, soldier traversal and appearance from ground/air. Exit when it looks like an intentionally organized operating base and works for attackers and defenders.
2. **Build the runtime placer and ownership.** Place that exact layout at multiple headings on flat, rolling, wooded, coastal and built-up AO samples. Exit when it either produces an intact reachable site or cleanly rejects/rolls back, including missing resources and navigation failure.
3. **Implement seize and regroup.** Use a manually selected site initially. Exercise solo, small squad, split squads 1–2 km apart, casualty/respawn, disconnect, late join, helicopter overflight, empty base and hostile contest. Exit when a rushing player cannot start an immediate counterattack and the HUD explains every wait.
4. **Connect defense and progression.** Verify identical site/host across phases, one AO completion, no duplicate defense, no leftover spawns, optional mortar handling and both defense modes. Test force-complete and GM transitions during every phase.
5. **Add selection, admin persistence and rollout telemetry.** Test 0/50/100%, absent authored markers, failed placement, old config files, live saves and restart saves. Repeated completion ticks must never reroll. Verify join-in-progress and cleanup on a dedicated server with multiple clients.

Use meaningful focused checks for selection/state transitions, transform math, config migration and cleanup idempotence where the project can run them. Terrain fit, AI navigation, nested prefab replication and feel require Workbench/dedicated-server validation; source inspection cannot establish those outcomes.

Measure placement success by AO and size, time to first arrival/capture/defense, proportion assembled when defense begins, total added AO time, garrison/wave totals, entity expansion and server frame cost. If full-size placement is unreliable, add authored candidate anchors/layout variants before weakening placement constraints. If the additional phase becomes repetitive, reduce the admin chance or introduce another objective type using the same lifecycle.

The first playable milestone is one polished Soviet base that spawns correctly, can be seized, gathers the force and transitions into the existing defense at the same place. Additional objective types follow after this proves the experience.
