# Dynamic AI Spawning

Dynamic AI Spawning is an optional server-authoritative system that removes supported I&A infantry characters and recreates surviving soldiers when needed. Budget mode concentrates full squads near players instead of waking every objective inside the same city. Individual members of less important squads can remain cached. Native groups, their orders, and the mission's logical survivor counts remain present.

## Admin settings and compatibility

Open **Admin configuration → Scaling**.

| Setting | Behavior |
| --- | --- |
| **Dynamic AI Spawning OFF** | Default. Uses the original spawning path and stored AI scaling settings. If soldiers are already cached, restores all surviving records through the worker before becoming inactive. |
| **Dynamic AI Spawning ON, Dynamic AI Budget 1–2000** | Uses the shared soldier budget described below and fixes effective AI scale at **1.0**. The budget defaults to **160**. |
| **Dynamic AI Spawning ON, Dynamic AI Budget 0** | Removes the budget policy while keeping effective AI scale at **1.0**. Existing budget reserves first restore, then the previous distance-only Dynamic AI path takes over. |

The budget is an **absolute target number of managed soldiers**, independent of the Game Master percentage and vanilla editor limits. The target itself controls how many assigned soldiers remain physical; it does not multiply an objective's initial roster. Settings are saved with the admin configuration; existing profiles without a budget retain the configured/default value.

While the **Dynamic AI Spawning master toggle is ON**, the shared AI scale function returns **1.0** before applying the player-count curve, AI scale multiplier, or static AI scale override. This applies with **any budget value, including 0**, and to every spawning/count calculation that uses that function. The stored multiplier and static override remain editable and are not overwritten; switching the master toggle OFF resumes those stored settings and the previous player-scaling behavior. Separate settings such as the military vehicle count multiplier still apply. Changing the scale affects subsequent calculations; it does not rebuild an already assigned roster.

The target is **soft**. Nearby fighting, protected roles, injuries, fresh spawns, and already admitted restoration work can exceed it. Mandatory soldiers still restore when the target is full. A strict cap would require withholding close enemies or removing soldiers during a firefight; this system preserves those gameplay protections and reports the overage.

## Budget allocation

1. Objectives initially spawn through their existing paths. The system observes each registered group's actual members, including subsequent additions. This version does not create initial reserves without first spawning their characters.
2. A sliced census measures the minimum horizontal distance from any current live or saved soldier to **any player pawn**. Objective centers and the first connected player do not determine priority. Split player teams therefore create separate nearby fronts within the same shared budget.
3. Protected demand reserves slots first, even when it exceeds the target. Remaining capacity fills eligible squads in nearest-first order, using their **current surviving roster**. A boundary squad receives only the remaining individual slots; more distant squads may receive none. There is no compulsory one-soldier allocation for every squad.
4. Previous allocations receive a **50 m ranking preference**. Equal ranks use stable registration order. Small player movements therefore do not constantly swap equally distant squads. Ranking does not rearrange the group registry.
5. Ordinary optional demand begins inside **1000 m**. An existing allocation may remain eligible out to **1500 m**, reducing boundary churn. These are eligibility distances, not unconditional full-squad wake distances in budget mode.

For example, after reserving 20 protected soldiers, a target of 160 provides 140 optional slots. Closest eligible squads fill those slots; the last squad can be partial. If protected demand grows above 160, optional admission waits while protected soldiers remain available.

### Protection and eviction timing

- A group reaching **300 m** from a player requests its full surviving roster. This close state releases only beyond **400 m**. Individual live soldiers inside 400 m are never selected for eviction.
- Recent combat protects a group. Current targets seen within the last **60 seconds** and recent recorded danger prevent removal. A casualty in a partial squad also requests its surviving reserves, even if a long-range first shot kills its last physical member. Capture readiness and assignment to protected mission roles also request restoration. Temporary spawn/order initialization protects current members without independently waking all reserves.
- Newly observed or recreated soldiers remain live for at least **30 seconds**. A lower allocation must then remain pending for **10 seconds** before removal work can begin. Fresh groups therefore commonly need roughly 40 seconds plus census/queue time before their first budget reduction; this is not the distance-only 60-second departure timer.
- Immediately before each removal, the worker rechecks players, combat, role, membership, ownership, attachments, forced LOD, and health. An arrival-owned `PreventMaxLOD` pin is not treated as immovable. It prefers removing the farthest eligible non-leader when a squad still has a positive allocation. The leader can also be removed if another protected member already occupies the remaining allocation.

One current, healthy soldier's actual prefab and complete world transform are captured immediately before its entity is deleted. Virtualization is not a kill and does not reduce logical objective strength. A fully empty physical group pauses; a partial group's remaining soldiers keep acting.

### Admission, retries and work limits

One worker owns both directions in budget mode. It first services mandatory restoration, then releases capacity, then admits optional restorations. Capacity includes current managed native members and reserved slots for admitted attempts that have not attached yet. Optional work checks that shared cost again before creating another soldier; a desired allocation is not evidence that older physical soldiers have already disappeared.

An unadmitted reserve may wait as priorities change. **Once admitted, a restoration keeps its record and retries until resolved.** Close/combat/capture/OFF requests can bypass capacity, and line of sight never vetoes restoration. Missing resources, failed creation, and incomplete attachment retry with backoff; persistent restoration trouble warns after five failures. A killed replacement becomes a casualty, and a pawn taken over by a player is not duplicated or forcibly regrouped.

The worker runs every **100 ms**, with at most **four character operations total per tick** across restoration, removal, and downed-death work. It uses a **4 ms soft allowance**, checked between operations. The theoretical combined ceiling is **40 operations/second**, not 40 removals plus 40 spawns, and not a measured safe throughput. An individual engine call can exceed the allowance. Player/protection sampling, census slices, and later engine or client replication work also have costs outside that worker allowance.

The allocation census targets approximately one second and uses a 2 ms allowance per slice. Stale plans stop optional admission and eviction; mandatory restoration remains available. Actual transition speed depends on protected demand, census and queue time, and native entity costs.

## Survivor state and partial squads

Each new removal saves the soldier's **latest exact world position and orientation**, including floor height. Dead soldiers never refill their old slots. Downed soldiers selected for caching are killed through the normal damage-manager path with the original instigator; a refused death is retried later without inventing a survivor snapshot. Corpses remain under normal corpse/objective cleanup.

Conscious wounded soldiers and soldiers with persistent damage/status effects remain physical until safely recreatable. Budget mode can still cache other eligible members of that squad. Distance-only mode keeps its existing whole-team safety checks. Neither path heals an injured soldier through deletion and recreation.

The selected preservation scope remains **positions and casualties**. Equipment, inventory contents, and ammunition reset from the saved character prefab. Randomized appearance/loadouts may be initialized again. Animation and behavior-tree execution are not serialized; the group's native orders and addon state remain, and the combat profile is reapplied to replacements.

A partly cached patrol can keep moving with its physical members. Its absent members stay at their last saved positions until restored; this version does not simulate virtual patrol movement or teleport reserves to the moving leader. Restored members rejoin their retained group and follow its current orders. This can temporarily spread out a patrol.

Building teams can cache before completing their walk indoors. The arrival-owned `PreventMaxLOD` pin does not veto snapshot or eviction; suspension releases that pin. Cover or observation posts with no enclosing building arrive when every living member is at the post, so they do not stay forced-simulated indefinitely. Live partial teams continue approaching their post; missing reserves cannot prove that the entire team has arrived. Fully dormant teams release their arrival-owned simulation pin. Restored agents regain movement support, while the original destination and strict interior-arrival checks remain. A rapid arrival can therefore reveal troops still outside and walking in.

## Scope and mission safety

The budget covers registered **area-owned military groups**, including groups created while Dynamic AI Spawning was off. Ordinary infantry, ordinary patrols, and building/base garrisons can be reduced. Arbitrary GM-placed troops and groups outside this ownership path are not included. This is not a cap over every AI entity in the world.

Registered vehicle crews/passengers, static-gun and mortar crews, airborne forces, elite/sweep forces, HVT/objective units, active defense waves, and mission-owned simulation pins remain protected and consume managed capacity. Only a building-arrival-owned pin allows the normal infantry cache path. Player-controlled pawns are never intentionally removed or duplicated.

Logical strength includes virtual survivors. Town capture and base seize request missing relevant defenders when a living friendly could capture, and pause progress/scoring until physical readiness is confirmed. An empty distant capture zone does not wake its garrison. Defense-mode changes restore missing members before applying deferred group orders.

Area shutdown retires saved records before delayed work can recreate soldiers. Spawn, attachment, suspension, and deletion paths check ownership around callback-producing operations. Group-empty handling distinguishes deliberate removal from elimination. Mandatory OFF restoration uses the same bounded worker and can take time; changing the toggle does not synchronously spawn every reserve.

## Distance-only path: budget 0

Once any budget-mode drain finishes, the previous Dynamic AI path remains available intact:

- A supported team caches only after every member stays more than **1500 m horizontally** from every player for **60 continuous quiet seconds**, with recent combat expired and all whole-team safety checks satisfied.
- Any saved member inside **1000 m** requests the whole team's restoration unconditionally. Capture, defense assignment, and OFF can also request it.
- The existing queue handles one whole-group cache transaction per 100 ms tick, subject to its limits, alongside prioritized restoration. Large groups receive an exclusive turn. A complete restoration starts a fresh quiet period for the next cycle.

This mode still saves current positions and casualties and resets equipment/ammo. It intentionally has no population target, so clustered objectives can all become live together as before. The master toggle remains ON, so effective AI scale stays fixed at **1.0**; budget 0 does not reactivate stored scale overrides.

## Why this design

Shrinking wake radii cannot reliably control overlapping city objectives and increases rapid-arrival pop-in. Whole-group budgeting preserves cohesion but wastes the slots remaining at a boundary. Uniform percentage thinning weakens every nearby squad. **Nearest-group allocation with individual boundary thinning** concentrates full squads where players are closest and uses the available capacity precisely.

The system retains custom per-soldier records and native character creation/deletion. Native group dormancy does not provide these individual snapshots, while pooling would retain the entities the system aims to remove. The earlier [performance investigation](dynamic-ai-performance-investigation.md) documents those native API limits; its whole-group recommendation predates this request for budget-driven partial squads.

Distance does not prove invisibility. Long-range optics, aircraft, HALO, teleports, and remote cameras can expose absent or newly restored troops. There is no LOS gate, hidden-placement fallback, or aircraft prediction. Exact saved transforms can also be affected by changed geometry and physics. Dormant soldiers cannot take ordinary bullet or explosion damage, and there is no pre-impact artillery wake integration. These limitations still require observation in real missions.

## Validation status

The budget implementation regression run on 2026-09-09 passed all five native Workbench plugins with exit code **0** and their own zero-failure result markers:

| Plugin | Coverage | Evidence |
| --- | --- | --- |
| IA_DynamicAIBudgetTest | 45 allocation checks: clustered squads, protected reservations/overage, partial boundaries, ties/retention, split-player inputs, casualties and budget conservation | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Final_20260909_230618/IA_DynamicAIBudgetTest/logs/logs_2026-09-09_23-06-19/console.log:187>) |
| IA_DynamicAIBudgetControllerTest | 21 checks: shared operation/time limits, phase ordering, capacity, actual admission/retry logic, reservations, fairness and budget-zero drain | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Final_20260909_230618/IA_DynamicAIBudgetControllerTest/logs/logs_2026-09-09_23-06-25/console.log:187>) |
| IA_DynamicAIBudgetLifecycleTest | 22 checks: live/partial/dormant transitions, first return, repeat cycles, OFF drain, casualty records and retirement | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Final_20260909_230618/IA_DynamicAIBudgetLifecycleTest/logs/logs_2026-09-09_23-06-30/console.log:187>) |
| IA_DynamicAIBudgetConfigTest | 20 checks: defaults, integer bounds, malformed input, admin tokens, profile round trips and old-profile compatibility | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Final_20260909_230618/IA_DynamicAIBudgetConfigTest/logs/logs_2026-09-09_23-06-36/console.log:187>) |
| IA_DynamicAISpawningConfigTest | Existing distance-only configuration, scheduler and wrapper regressions, including partial-group order/arrival guards | [Log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Final_20260909_230618/IA_DynamicAISpawningConfigTest/logs/logs_2026-09-09_23-06-42/console.log:187>) |

All runs loaded Game CRC `31c637d3` and WorkbenchGame CRC `bec56788`. Scripts also validated for **WORKBENCH, PC, XBOX, PS4 and PS5**, ending with `Script validation successful` and no compiler errors; existing configuration-default warnings remain ([validation log](<C:/Users/'admin'/AppData/Local/Temp/IA_Budget_Validate_20260909_230757/logs/logs_2026-09-09_23-07-57/console.log:516>)). The runtime logging scan passed across **138** scripts, and logging, building-garrison and dynamic-base-flow suites passed **20** Python checks.

These tests exercise production policy and state transitions with deterministic fixtures replacing engine/world effects. They do not establish live entity lifecycle, multiplayer continuity, pathfinding, or frame-time improvement. No live budget performance benchmark is claimed; the clustered-city comparison below remains required.

The subsequent fixed-1.0-scale change compiled successfully and passed the existing Dynamic AI configuration/work-queue regression with exit code 0 ([follow-up log](<C:/Users/'admin'/AppData/Local/Temp/IA_DynamicAI_Scale_20260909_232145/logs/logs_2026-09-09_23-21-45/console.log:187>)). The logging scan and its eight tests passed. Source inspection confirmed that the master-toggle check precedes both the static override and player-based scaling, and that gameplay callers use this shared scale function.

## Focused live acceptance check

Restart the mission with the updated scripts. Use a dedicated server and a second observing client, with **-iaDebug 1** on the server for diagnostics. Budget mode reports **[IA][DynamicAI] Budget** every 30 seconds: target, physical-and-reserved cost, protected demand, planned demand, virtual members, partial groups, overage, and plan age. **Budget work** reports operations and maximum script work times. These are custom managed counts; GM budget percentage is not the acceptance metric. The original coverage/queue/removal-audit diagnostics apply to distance-only mode.

1. **Clustered city:** use several overlapping objectives, set a target well below their combined rosters, and initially stay 450–900 m from the troops. After live dwell and eviction delays, verify the closest squads fill first, a boundary squad can be partial, and farther squads reduce. Move laterally across the city and check priorities migrate without constant back-and-forth spawning.
2. **Protection and split players:** approach within 300 m or engage a partial squad. Its missing survivors should return even if this temporarily exceeds the target. Move a second player to another front. Neither player should lose close enemies merely to satisfy the target. After withdrawal and expired combat protection, capacity should become available again.
3. **Casualties and movement:** kill members, down another, wound a conscious survivor, and move healthy survivors to distinct valid positions. After eligible removal and restoration, killed/downed casualties must stay dead, the wounded survivor must not be healed by caching, and only actual surviving records may return. Move a partly cached patrol and verify live members continue acting while reserves retain their last positions. Repeat the cycle.
4. **Modes during queued work:** lower and raise the target; set budget 0 during partial restoration; then switch Dynamic AI Spawning off while reserves remain. Verify admitted retries persist, budget 0 completes its drain and uses the old distance thresholds, and OFF restores every remaining survivor without duplication. Effective AI scale should remain 1.0 in both ON modes, then resume the stored scale multiplier/static override when OFF. Re-enable and repeat a cycle.
5. **Mission lifecycle:** begin capture with virtual defenders, assign/cancel defense mode during restoration, and complete/cancel an area while work remains pending. Capture must wait for relevant defenders; deferred orders must use the latest request; retired areas must not respawn soldiers afterward.
6. **Performance and visibility:** compare the same mission/player count with OFF, distance-only, and budget mode. Record server frame times and transition spikes alongside managed live counts. Observe distant optics, fast arrivals, and indirect fire separately; report remaining pop-in and absent-target limitations rather than treating compiler checks as live validation.

These are acceptance checks to run, not reported results. Logging controls are documented in [runtime logging](runtime-logging.md).
