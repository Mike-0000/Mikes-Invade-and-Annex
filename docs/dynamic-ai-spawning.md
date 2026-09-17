# Dynamic AI Spawning

Dynamic AI Spawning is an optional server-authoritative system that removes supported I&A infantry characters and recreates surviving soldiers when needed. Budget mode concentrates full squads near players instead of waking every objective inside the same city. Individual members of less important squads can remain cached. Native groups, their orders, and the mission's logical survivor counts remain present.

## Admin settings and compatibility

Open **Admin configuration → Dynamic AI**. The dedicated tab holds the master toggle, soldier budget, distances, and timing. The existing Scaling tab retains the saved AI multiplier, static override, and combat settings.

| Setting | Behavior |
| --- | --- |
| **Dynamic AI Spawning OFF** | Default. Uses the original spawning path and stored AI scaling settings. If soldiers are already cached, restores all surviving records through the worker before becoming inactive. |
| **Dynamic AI Spawning ON, Dynamic AI Budget 1–2000** | Uses the shared soldier budget described below and fixes effective AI scale at **1.0**. The budget defaults to **160**. |
| **Dynamic AI Spawning ON, Dynamic AI Budget 0** | Removes the budget policy while keeping effective AI scale at **1.0**. Existing budget reserves first restore, then the previous distance-only Dynamic AI path takes over. |

The budget is an **absolute target number of managed soldiers**, independent of the Game Master percentage and vanilla editor limits. The target itself controls how many assigned soldiers remain physical; it does not multiply an objective's initial roster. Settings are saved with the admin configuration; existing profiles without a budget retain the configured/default value.

While the **Dynamic AI Spawning master toggle is ON**, the shared AI scale function returns **1.0** before applying the player-count curve, AI scale multiplier, or static AI scale override. This applies with **any budget value, including 0**, and to every spawning/count calculation that uses that function. The stored multiplier and static override remain editable and are not overwritten; switching the master toggle OFF resumes those stored settings and the previous player-scaling behavior. Separate settings such as the military vehicle count multiplier still apply. Changing the scale affects subsequent calculations; it does not rebuild an already assigned roster.

The target is the **restore cap after the first cache**. Walking into a cached objective restores the nearest infantry first and stops when physical-and-reserved cost reaches the budget. Farther cached groups stay cached; already-live soldiers that are too far can be evicted to free slots. Live soldiers already inside the keep/release distances, fresh min-live soldiers, injuries, and already-admitted retries are not deleted just to make room. Those leftover physical soldiers can still sit above the target until they become evictable. Switching Dynamic AI **OFF** or setting budget **0** still restores every remaining survivor.

### Distance, timing, and stability controls

All fields use whole meters or seconds. Defaults below preserve the original budget tuning; the runtime uses the current saved settings rather than these fixed numbers.

| Control | Default | Allowed range | Applies to |
| --- | --- | --- | --- |
| Wake / eligibility distance | 1000 m | 100–5000 m | Whole-squad wake in distance-only mode; optional demand eligibility in budget mode. |
| Cache / outer retention distance | 1500 m | 150–7500 m | Departure boundary in distance-only mode; outer eligibility for existing budget allocations. |
| Close keep distance | 300 m | 50–2000 m | Budget mode: live soldiers in this band stay in close-keep hysteresis. Approach does not force-full a cached town. |
| Keep release distance | 400 m | 100–3000 m | Budget mode: close-keep ends beyond it; live soldiers inside it cannot be evicted. Farther teammates can still be cached. |
| Departure quiet time | 60 s | 0–600 s | Distance-only mode: continuous eligible departure before caching. Zero removes this delay only. |
| Combat quiet time | 60 s | 10–300 s | Both modes: recent target/danger protection. |
| Minimum live time | 30 s | 5–300 s | Budget mode: minimum dwell for newly observed or restored soldiers. |
| Allocation reduction delay | 10 s | 1–120 s | Budget mode: sustained lower allocation before removal work. |
| Existing allocation preference | 50 m | 0–500 m | Budget mode: distance-ranking advantage for existing allocations. Zero disables this advantage. |
| Capture seed window | 30 s | 5–120 s | Budget mode: how long a contested objective may restore one defender over the target. |
| Hard cap | OFF | on/off | Budget mode: when ON, recent combat does not protect a squad from eviction. Soldiers inside the keep-release distance still stay. |

Saving normalizes the distances by raising outer values: **release ≥ protection + 50 m**, **wake ≥ release**, and **cache ≥ wake + 50 m**. This prevents conflicting boundaries without silently reducing the protection distance. The next menu view shows the normalized server values.

**Save** applies settings to the current mission. **Save for restart** also stores them in the server profile, which loads after the mission configuration on subsequent starts. **Clear saved** removes profile overrides for the next restart; it does not undo the current mission's values. Older profiles and older admin payloads that omit the new controls preserve the configured/default values. Applying tuning refreshes planning and eligibility timing while preserving survivor records and already admitted restoration work. Native work-rate limits remain internal and are not admin controls.

## Budget allocation

1. Objectives still create a live leader. When the shared target is already full and the spawn point is beyond wake distance from every player, remaining infantry are recorded as reserves instead of characters. Building garrisons, HVTs, emplacements, vehicles, and airborne drops still spawn their full physical roster. The system then observes each registered group's actual members, including subsequent additions.
2. A sliced census measures the minimum horizontal distance from any current live or saved soldier to **any player pawn**. Objective centers and the first connected player do not determine priority. Split player teams therefore create separate nearby fronts within the same shared budget.
3. Already-live protected soldiers (roles, min-live, keep-distance, unsafe-to-remove) reserve slots first. Remaining capacity fills eligible squads in nearest-first order, using their **current surviving roster**. A boundary squad receives only the remaining individual slots; more distant squads may receive none. There is no compulsory one-soldier allocation for every squad. Close and combat no longer mark an entire cached roster as protected.
4. Previous allocations receive the configured ranking preference, **50 m by default**. Equal ranks use stable registration order. Small player movements therefore do not constantly swap equally distant squads. Ranking does not rearrange the group registry.
5. Ordinary optional demand begins inside the wake distance, **1000 m by default**. An existing allocation may remain eligible out to the cache distance, **1500 m by default**, reducing boundary churn. These are eligibility distances, not unconditional full-squad wake distances in budget mode.

For example, after reserving 20 protected soldiers, a target of 160 provides 140 optional slots. Closest eligible squads fill those slots; the last squad can be partial. If protected demand grows above 160, optional admission waits while protected soldiers remain available.

### Protection and eviction timing

- A group reaching the keep distance, **300 m by default**, stays in close-keep hysteresis until beyond the release distance, **400 m by default**. Individual live soldiers inside the release distance are never selected for eviction. Walking in after a cache does **not** restore every nearby squad over the budget; nearest optional restore fills up to the target, and farther groups stay cached or are evicted.
- Recent combat prevents eviction of a fighting squad for the combat quiet time, **60 seconds by default**, only while the shared target still has room. Once the target is full, or if the admin hard cap is on, distance decides who stays. Combat never force-restores remaining cached teammates over the shared target. Capture waits for at least one physical defender in budget mode rather than the entire virtual roster. Defend orders apply to hybrid live members. Temporary spawn/order initialization protects current members without independently waking all reserves.
- Newly observed or recreated soldiers remain live for the configured minimum live time, **30 seconds by default**. A lower allocation must then remain pending for the reduction delay, **10 seconds by default**, before removal work can begin. At default settings, fresh groups commonly need roughly 40 seconds plus census/queue time before their first budget reduction. These controls are separate from the distance-only departure quiet time.
- Immediately before each removal, the worker rechecks players, combat, role, membership, ownership, attachments, and health. Forced LOD no longer vetoes snapshot or eviction; inbound simulation pins are released on suspend. It prefers removing the farthest eligible non-leader when a squad still has a positive allocation. The leader can also be removed if another protected member already occupies the remaining allocation. Seated occupants are never deleted in place: occupancy ejects first, waits for behavior-tree quiescence, then deletes.

One current, healthy soldier's actual prefab and complete world transform are captured immediately before its entity is deleted. Virtualization is not a kill and does not reduce logical objective strength. A fully empty physical group pauses; a partial group's remaining soldiers keep acting.

### Admission, retries and work limits

One worker owns both directions in budget mode. It first services mandatory restoration, then releases capacity, then admits optional restorations. Capacity includes current managed native members and reserved slots for admitted attempts that have not attached yet. Optional work checks that shared cost again before creating another soldier; a desired allocation is not evidence that older physical soldiers have already disappeared.

An unadmitted reserve may wait as priorities change. **Once admitted, a restoration keeps its record and retries until resolved.** OFF and budget-0 drains restore every survivor even when the target is full. Approach, combat, and defend no longer bypass the shared cap. Line of sight never vetoes restoration. Missing resources, failed creation, and incomplete attachment retry with backoff; persistent restoration trouble warns after five failures. A killed replacement becomes a casualty, and a pawn taken over by a player is not duplicated or forcibly regrouped.

The worker runs every **100 ms**, with at most **four character operations total per tick** across restoration, removal, and downed-death work. A nearby squad in recent combat may use **two extra optional restorations** in that same tick so reinforcements arrive in seconds. It uses a **4 ms soft allowance**, checked between operations. Optional restore prefers a saved position at least **60 m** from every player, and among similar distances prefers a WORLD|ENTS occlusion; it never refuses a restore because a soldier would be visible. An individual engine call can exceed the allowance. Player/protection sampling, census slices, and later engine or client replication work also have costs outside that worker allowance.

The allocation census targets approximately one second and uses a 2 ms allowance per slice. Stale plans stop optional admission and eviction; mandatory restoration remains available. Actual transition speed depends on protected demand, census and queue time, and native entity costs.

## Survivor state and partial squads

Each new removal saves the soldier's **latest exact world position and orientation**, including floor height. Dead soldiers never refill their old slots. Downed soldiers selected for caching are killed through the normal damage-manager path with the original instigator; a refused death is retried later without inventing a survivor snapshot. Corpses remain under normal corpse/objective cleanup.

Conscious wounded soldiers and soldiers with persistent damage/status effects remain physical until safely recreatable. Budget mode can still cache other eligible members of that squad. Distance-only mode keeps its existing whole-team safety checks. Neither path heals an injured soldier through deletion and recreation.

The selected preservation scope remains **positions and casualties**. Equipment, inventory contents, and ammunition reset from the saved character prefab. Randomized appearance/loadouts may be initialized again. Animation and behavior-tree execution are not serialized; the group's native orders and addon state remain, and the combat profile is reapplied to replacements.

A partly cached patrol can keep moving with its physical members. Its absent members stay at their last saved positions until restored; this version does not simulate virtual patrol movement or teleport reserves to the moving leader. Restored members rejoin their retained group and follow its current orders. This can temporarily spread out a patrol.

Building teams can cache before completing their walk indoors. The arrival-owned `PreventMaxLOD` pin does not veto snapshot or eviction; suspension releases that pin. Cover or observation posts with no enclosing building arrive when every living member is at the post, so they do not stay forced-simulated indefinitely. Live members of a hybrid team can finish arrival and release that pin; restored reserves inherit the group's current Hold order. Fully dormant teams still cannot arrive. A rapid arrival can therefore reveal troops still outside and walking in.

## Scope and mission safety

The budget covers registered **area-owned I&A groups**: military infantry, elites, HVT/objective units, finished defend waves, inbound-pinned QRF, foot civilians, static-gun crews, and mortar crews. Vehicles, vehicle crews, and vehicle passengers stay spawned and **do not consume the military soldier budget**. Civilians use a separate distance-only pool. Arbitrary GM-placed troops and player-controlled pawns are not included. This is not a cap over every AI entity in the world. Far ordinary infantry that would exceed the target are recorded as reserves at spawn; other roles still spawn through their normal paths, and the budget then caches far infantry down to the target. Distance is measured to player pawns, not the Game Master camera.

Emplacement occupants (static guns and mortars) cache only while their host is stationary. Occupancy is all-or-nothing for one emplacement. Vehicles and anyone seated in a world vehicle are excluded from caching and keep their hulls fully crewed.

In-flight airborne forces (`m_bAirborneDrop`) stay physical until they land. Pending seat teleport and staggered-spawn initialization still block caching. Player-controlled pawns are never intentionally removed or duplicated.

Logical strength includes virtual survivors. Town capture and base seize request missing relevant defenders when a living friendly could capture, and pause progress/scoring until physical readiness is confirmed. An empty distant capture zone does not wake its garrison. Defense-mode changes restore missing members before applying deferred group orders. Caching an HVT is not killing it: assassination objectives wait for a restored entity or a real death.

Area shutdown retires saved records before delayed work can recreate soldiers. Spawn, attachment, suspension, and deletion paths check ownership around callback-producing operations. Group-empty handling distinguishes deliberate removal from elimination. Mandatory OFF restoration uses the same bounded worker and can take time; changing the toggle does not synchronously spawn every reserve.

Registered vehicle reservations stay on live crews. Vehicles are not virtualized, so inactive-group cleanup still sees a living reserving group.

### Not covered

These stay out of Dynamic AI caching:

- GM/editor-placed troops that never received `SetDynamicAIOwner`
- Player-controlled pawns
- In-flight airborne groups (`m_bAirborneDrop`) until they land
- Vehicles, vehicle crews, vehicle passengers, and anyone seated in a world vehicle
- Loadout, magazine, and injury state (recreation resets from the saved prefab)
- A line-of-sight or camera gate; distance and combat quiet still decide eligibility

## Distance-only path: budget 0

Once any budget-mode drain finishes, the previous Dynamic AI path remains available intact:

- A supported team caches only after every member stays beyond the configured cache distance (**1500 m by default**) horizontally from every player for the departure quiet time (**60 continuous seconds by default**), with combat protection expired and all whole-team safety checks satisfied.
- Any saved member inside the configured wake distance (**1000 m by default**) requests the whole team's restoration unconditionally. Capture, defense assignment, and OFF can also request it.
- The existing queue handles one whole-group cache transaction per 100 ms tick, subject to its limits, alongside prioritized restoration. Large groups receive an exclusive turn. A complete restoration starts a fresh quiet period for the next cycle.

This mode still saves current positions and casualties and resets equipment/ammo. It intentionally has no population target, so clustered objectives can all become live together as before. The master toggle remains ON, so effective AI scale stays fixed at **1.0**; budget 0 does not reactivate stored scale overrides.

## Why this design

Shrinking wake radii cannot reliably control overlapping city objectives and increases rapid-arrival pop-in. Whole-group budgeting preserves cohesion but wastes the slots remaining at a boundary. Uniform percentage thinning weakens every nearby squad. **Nearest-group allocation with individual boundary thinning** concentrates full squads where players are closest and uses the available capacity precisely.

The system retains custom per-soldier records and native character creation/deletion. Native group dormancy does not provide these individual snapshots, while pooling would retain the entities the system aims to remove. The earlier [performance investigation](dynamic-ai-performance-investigation.md) documents those native API limits; its whole-group recommendation predates this request for budget-driven partial squads.

Distance does not prove invisibility. Long-range optics, aircraft, HALO, teleports, and remote cameras can expose absent or newly restored troops. There is no LOS gate, hidden-placement fallback, or aircraft prediction. Exact saved transforms can also be affected by changed geometry and physics. Dormant soldiers cannot take ordinary bullet or explosion damage, and there is no pre-impact artillery wake integration. These limitations still require observation in real missions.

## Validation status

The admin tuning implementation passed all five native Workbench plugins on 2026-09-09, with exit code **0** and their own zero-failure result markers:

| Plugin | Coverage | Evidence |
| --- | --- | --- |
| IA_DynamicAIBudgetTest | 48 allocation checks, including custom/zero retention preference, clustered squads, protected reservations, partial boundaries and budget conservation | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Tuning_20260909_234702/IA_DynamicAIBudgetTest/logs/logs_2026-09-09_23-47-09/console.log:187>) |
| IA_DynamicAIBudgetControllerTest | 21 checks: shared operation/time limits, phase ordering, capacity, actual admission/retry logic, reservations, fairness and budget-zero drain | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Tuning_20260909_234702/IA_DynamicAIBudgetControllerTest/logs/logs_2026-09-09_23-47-29/console.log:187>) |
| IA_DynamicAIBudgetLifecycleTest | 29 checks: live/partial/dormant transitions, settings invalidation preserving pending work and actual soldier ages, OFF drain, casualties and retirement | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Tuning_20260909_234702/IA_DynamicAIBudgetLifecycleTest/logs/logs_2026-09-09_23-47-23/console.log:187>) |
| IA_DynamicAIBudgetConfigTest | 45 checks: defaults, all tuning bounds, distance normalization, atomic malformed-input rejection, admin tokens, persistence and old-profile compatibility | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Tuning_20260909_234702/IA_DynamicAIBudgetConfigTest/logs/logs_2026-09-09_23-47-03/console.log:187>) |
| IA_DynamicAISpawningConfigTest | Existing distance-only/scheduler/wrapper regressions plus 17 custom-timing checks for quiet/combat boundaries, zero departure delay and configuration continuity | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Tuning_20260909_234702/IA_DynamicAISpawningConfigTest/logs/logs_2026-09-09_23-47-16/console.log:187>) |

All runs loaded Game CRC `374a706f` and WorkbenchGame CRC `b67cf8f1`. Scripts also validated for **WORKBENCH, PC, XBOX, PS4 and PS5**, ending with `Script validation successful` and no compiler errors ([validation log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_TuningValidate_20260909_234834/logs/logs_2026-09-09_23-48-34/console.log:561>)). Configuration-default warnings remain. The runtime logging scan passed across **138** scripts and its **eight** Python tests passed.

These tests exercise production policy and state transitions with deterministic fixtures replacing engine/world effects. They do not establish live entity lifecycle, multiplayer continuity, pathfinding, or frame-time improvement. No live budget performance benchmark is claimed; the clustered-city comparison below remains required.

Full caching adds Workbench fixtures for opened on-foot roles, HVT-not-complete, civilian budget isolation, and occupancy qualify/abort/commit bookkeeping. Native GetIn/GetOut cannot be proven there. Live gates 8–10 below are the remaining acceptance for those paths.

Reserve-first spawn, pop-in preference, hybrid building arrival, combat restore throughput, budget-flow telemetry, capture-seed window, and the combat hard-cap toggle updated the same plugins and added fixture coverage. Those plugins have **not** been rerun in this session; run them before the live gate. Do not treat compilation as live proof that far over-budget squads now seed reserves.

## Focused live acceptance check

Restart the mission with the updated scripts. Use a dedicated server and a second observing client, with **-iaDebug 1** on the server for diagnostics. Budget mode reports **[IA][DynamicAI] Budget** every 30 seconds: target, physical-and-reserved cost, protected demand, planned demand, virtual members, partial groups, overage, and plan age. **Budget work** reports operations and maximum script work times. **Budget flow** reports nearest restore distance, farthest evict distance, capture seeds, denied-over-budget skips, optional restores, and reserves seeded at spawn. These are custom managed counts; GM budget percentage is not the acceptance metric. The original coverage/queue/removal-audit diagnostics apply to distance-only mode.

For the new controls, open **Dynamic AI**, change distances and delays, Save, and reopen to confirm the normalized values. Check those boundaries in both budget and distance-only mode. Use Save for restart and restart once to verify the configured values return. Live menu layout and multiplayer boundary behavior still need this in-game check.

1. **Clustered city:** use several overlapping objectives, set a target well below their combined rosters, and initially stay outside the release distance but inside the wake distance (450–900 m works with defaults). After live dwell and eviction delays, verify the closest squads fill first, a boundary squad can be partial, and farther squads reduce. Move laterally across the city and check priorities migrate without constant back-and-forth spawning.
2. **Approach after cache:** leave until infantry have cached at least once, then walk into the AO. Physical infantry should climb toward the budget and stop there. Nearest groups restore first; farther groups stay cached or get evicted. A firefight should stay populated around that budget, not dump the whole town. Split players share the same cap across fronts. After withdrawal and expired combat/min-live, capacity should become available again.
3. **Casualties and movement:** kill members, down another, wound a conscious survivor, and move healthy survivors to distinct valid positions. After eligible removal and restoration, killed/downed casualties must stay dead, the wounded survivor must not be healed by caching, and only actual surviving records may return. Move a partly cached patrol and verify live members continue acting while reserves retain their last positions. Repeat the cycle.
4. **Modes during queued work:** lower and raise the target; set budget 0 during partial restoration; then switch Dynamic AI Spawning off while reserves remain. Verify admitted retries persist, budget 0 completes its drain and uses the old distance thresholds, and OFF restores every remaining survivor without duplication. Effective AI scale should remain 1.0 in both ON modes, then resume the stored scale multiplier/static override when OFF. Re-enable and repeat a cycle.
5. **Mission lifecycle:** begin capture with virtual defenders, assign/cancel defense mode during restoration, and complete/cancel an area while work remains pending. Capture must wait for relevant defenders; deferred orders must use the latest request; retired areas must not respawn soldiers afterward.
6. **Performance and visibility:** compare the same mission/player count with OFF, distance-only, and budget mode. Record server frame times and transition spikes alongside managed live counts. Observe distant optics, fast arrivals, and indirect fire separately; report remaining pop-in and absent-target limitations rather than treating compiler checks as live validation.
7. **Live tuning and persistence:** change distances and timings while reserves exist, including conflicting distance values. Save and reopen the tab to confirm normalization and current values. Verify existing reserve/casualty state survives and the new boundaries govern subsequent work. Use Save for restart and restart to confirm the current packed extras return; test an older nine-field profile retains capture-seed and hard-cap defaults.
8. **On-foot roles:** a far town with an elite patrol, an HVT objective, and a finished defend wave should cache those groups. Approach restores them. Killing the restored HVT still completes the objective. Capture still waits on `EnsureReadyInRadius`.
9. **Civilians:** a far village empties its wanderers without changing military budget numbers. Returning restores surviving civilians. Killed civilians stay dead. Civilian vehicle crews stay spawned.
10. **Emplacements:** far static-gun and mortar crews can cache and remount. Parked trucks and their AI stay fully spawned.

These are acceptance checks to run, not reported results. Logging controls are documented in [runtime logging](runtime-logging.md).
