# Dynamic AI performance investigation — 2026-09-09

**Recommendation: retain the per-soldier snapshot layer and change its scheduling and building-arrival policy.** The inspected native group despawn path calls the same character deletion primitive already used here. Switching APIs alone does not remove character/component/replication teardown work, and native group restoration does not preserve our individual survivor identities and transforms. A larger persistence system or character pool is not the best first change.

This investigation covers the implementation at `01444cb`, the user's current Workbench log, local read-only GameSources, and Bohemia's public documentation. It proposes the next implementation; runtime behavior was not changed in this pass. Engine C++ dispatch internals and multiplayer frame-time measurements are unavailable in the supplied evidence.

**What the live log proves**

The running session recompiled at 23:57:15 with Game CRC `87851bc8`, matching the expanded infantry implementation. Evidence comes from [console.log](<C:/Users/'admin'/Documents/My Games/ArmaReforgerWorkbench/logs/logs_2026-09-08_22-44-10/console.log:6343>); the file continues across midnight.

| Time | Cached groups / soldiers | Waiting for building arrival | Other relevant state |
| --- | ---: | ---: | --- |
| 23:59:50 | 11 / 44 | 17 / 68 | 7 / 22 in the quiet period |
| 00:00:20 | 18 / 66 | 17 / 68 | No ready-to-despawn backlog reported |
| 00:01:50 | 19 / 70 | 14 / 56 | 2 / 8 in the quiet period |

Each snapshot also listed 8 vehicle groups / 45 soldiers and 4 special patrol groups / 32 soldiers. These are exclusions, not a slow deletion queue. Counts cover registered area groups, not every world entity or every editor budget cost. Snapshot evidence: [23:59:50](<C:/Users/'admin'/Documents/My Games/ArmaReforgerWorkbench/logs/logs_2026-09-08_22-44-10/console.log:7697>), [00:00:20](<C:/Users/'admin'/Documents/My Games/ArmaReforgerWorkbench/logs/logs_2026-09-08_22-44-10/console.log:7768>), [00:01:50](<C:/Users/'admin'/Documents/My Games/ArmaReforgerWorkbench/logs/logs_2026-09-08_22-44-10/console.log:7904>).

From 23:59:21.947 through 00:01:33.424, 19 cache transactions removed 70 soldiers. All 19 delayed removal audits reported **zero original entities remaining**. Their editor AI-budget decreases sum to 70. The 250 ms audit is a delayed verification, not a measurement showing deletion took 250 ms. Some active-AI counts stayed unchanged while the editor budget fell, so editor-budget savings alone do not establish server-FPS improvement.

The first 18 groups / 66 soldiers cached across approximately 58 seconds. The next group cached roughly 74 seconds later. This is consistent with groups becoming eligible at different times. The log does not justify attributing the entire elapsed interval to expensive deletion calls.

**Why it can take minutes**

1. **The queue intentionally allows only one successful group cache per second.** The manager's `cachedThisScan` flag prevents every later candidate in that scan from entering the cache. After a shared 60-second quiet period, G simultaneously eligible groups need approximately another G seconds. The existing 4 ms work allowance applies to restoration only; it does not bound the full scan or a whole-group deletion transaction. See [manager](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_DynamicAISpawning.c:117).
2. **Building arrival precedes the quiet period.** Every living member must satisfy the same-building bounds, distance, floor-height and roof checks before `m_bHoldEntered` becomes true. Until then, caching resets its quiet timer. The effective delay is building approach time, plus 60 seconds, plus queue time. One straggler can delay the whole group indefinitely. A missing building reference can never pass; that is a source-supported failure possibility, not a proven diagnosis for every team in this log. See [arrival checks](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_BuildingGarrison.c:73), [eligibility](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_AI_Group.c:6092), and [timer](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_DynamicAIActivityGate.c:11).
3. **A simulation pin keeps unfinished building teams running.** The hold-march code deliberately pins their AI and does not release the pin based on ordinary distance while entry is incomplete. Thus some of the troops we most want to remove can also remain forced to simulate. See [march](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_AI_Group.c:4179) and [pin lifecycle](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_AI_Group.c:4697).

Illustrative scheduling arithmetic, assuming all groups finish initialization together, have no blockers, and every operation is cheap enough to meet the proposed worker limits:

| Eligible groups | Current: 1 group/s | Proposed ceiling: 10 groups/s |
| ---: | ---: | ---: |
| 20 | ~20 s drain; ~80 s including quiet period | ~2 s drain; ~62 s including quiet period |
| 100 | ~100 s drain; ~160 s total | ~10 s drain; ~70 s total |
| 300 | ~300 s drain; ~360 s total | ~30 s drain; ~90 s total |

These are theoretical rate ceilings and approximate minimum drain times, **not measured performance forecasts**. Group sizes, initial spawning, combat, player movement, worker budgets and server load change the result. Increasing queue throughput does not resolve excluded or uninitialized groups. Keeping the current quiet period means caching still intentionally cannot start immediately.

**Native alternatives**

| Path | What it provides | Fit for this mod |
| --- | --- | --- |
| `SCR_AIGroup.DespawnMembers()` / `SpawnMembers()` | Dormant group bookkeeping, native lifecycle integration, per-member deletion and prefab-slot reconstruction | Despawn uses our existing primitive. Restore retains aggregate size rather than each saved survivor's identity/transform; the addon manually populates empty prefab slots. Not a replacement without substantial adaptation. |
| Native AI spawn dispatcher | Enqueued member creation, observer and population/importance policy | Useful scheduling precedent, but its prefab-slot model and additional spawning gates do not satisfy unconditional exact restoration. C++ throughput is unverified. |
| Scenario Framework dynamic spawn/despawn | Area/slot ownership, distance activation and staggered child spawning | A broader scenario architecture, not a faster drop-in delete call. Its AI slot stores a group position and reforms agents. Migrating ownership/task callbacks would add work without proving a speed gain. |
| AI LOD, deactivation or pooling | Reduced simulation while retaining existing entities | May help simulation cost, but retained pawns do not provide the actual despawn/editor-budget reduction requested. Pooling also requires resetting ownership, inventory, wounds and callbacks correctly. |
| Native persistence | Serialized tracked entities and asynchronous restoration | Potential future route for richer state fidelity. It adds tracking/configuration and serialization; callers still delete entities. Not a teardown optimization. |
| Garbage system / common deletion helpers | Lifetime-based cleanup or wrappers around replicated deletion | No selective, faster snapshot/restore primitive found. Global garbage flushes are inappropriate for this feature. World Editor bulk deletion targets editor sources, not live server characters. |
| Small custom queue over native entity APIs | Exact survivor snapshots plus explicit work pacing | Best fit. Reuse native creation/deletion, retain current accounting and retries, and optimize when work is admitted and how much runs together. |

The decisive local source evidence is [native group despawn](D:/ReforgerGameSources/data/data007/scripts/Game/Entities/SCR_AIGroup.c:2864): it loops over controlled entities and calls `RplComponent.DeleteRplEntity(entity, false)`. [DeleteEntityAndChildren](D:/ReforgerGameSources/data/data007/scripts/Game/Helpers/SCR_EntityHelper.c:174) is explicitly a wrapper around that same call. No inspected path avoids character/component/replication teardown.

[Native queued spawning](D:/ReforgerGameSources/data/data007/scripts/Game/Entities/SCR_AIGroup.c:2678) ultimately selects prefab slots through `ExpandOneMember`; [SpawnGroupMember](D:/ReforgerGameSources/data/data007/scripts/Game/Entities/SCR_AIGroup.c:1658) computes formation placement. The [dispatcher binding](D:/ReforgerGameSources/data/data007/scripts/Game/generated/AI/ChimeraAIWorld.c:19) is `proto`; source comments describing pacing are not an independently verified C++ implementation or throughput benchmark. Treat the local revision as the inspected version, not proof that every installed or upstream version has identical behavior.

Bohemia's [Scenario Framework documentation](https://community.bistudio.com/wiki/Arma_Reforger%3AScenario_Framework#Dynamic_Spawn/Despawn) describes periodic distance checks and deliberately staggered child spawning to reduce stutters. That supports bounded scheduling as a design pattern, not an exact per-soldier persistence or performance guarantee. The local [SlotAI implementation](D:/ReforgerGameSources/data/data007/scripts/Game/ScenarioFramework/Components/SCR_ScenarioFrameworkSlotAI.c:91) determines the actual saved-position granularity.

[PersistenceSystem](D:/ReforgerGameSources/data/data007/scripts/Game/generated/Plugins/Persistence/System/PersistenceSystem.c:15) documents saving before entity deletion and asynchronous spawning; the [persistence deletion hook](D:/ReforgerGameSources/data/data007/scripts/Game/Plugins/Persistence/System/SCR_PersistenceSystem.c:57) still delegates to entity deletion. [GarbageSystem](D:/ReforgerGameSources/data/data007/scripts/Game/generated/System/GarbageSystem.c:14) describes disposal lifetime/proximity and global flush operations. Neither establishes faster selective character removal.

**Recommended first implementation**

Keep the current per-unit ledger, casualty rules, replicated deletion, empty-group protection, repeated cycles and OFF path. Change the following as a bounded update:

1. **Decouple distant caching from building arrival.** Once the group's soldiers finish spawning and otherwise qualify, save their actual current positions even if they are still approaching a building. Preserve the destination, current orders and arrival state. On return they continue the approach. Keep strict arrival checks for switching from Defend to Wait; do not mark absent soldiers as having arrived or teleport them to the intended interior.
2. **Handle movement-pin ownership explicitly.** Distinguish a building-arrival pin from QRF/defense/vehicle/airborne pins. Suspend and reacquire only the arrival pin. Existing suspension leaves the pin flag set but deletes the agents; blindly relaxing eligibility would restore new agents without the intended protection because the flag already says they are pinned. Restart navmesh requests and march monitoring only after the entire saved roster has materialized. Preserve other mission pins as exclusions.
3. **Separate the 1 Hz eligibility scan from execution.** Queue eligible group references, not saved snapshots. Drain the queue on the existing 100 ms worker cadence, initially at most one whole-group transaction per worker call, subject to a small shared elapsed-time allowance and a soldier-count limit. Revalidate current player positions, role, combat, health, roster and ownership immediately before taking the actual snapshot and deleting originals. Deduplicate and rotate the queue; a group larger than the normal count allowance must eventually receive an exclusive turn.
4. **Give requested restoration priority.** Keep every wake request latched, with capture/nearby restoration ahead of background work and aging for distant requests. Use an actual waking queue rather than scanning every registration each worker call. Permit further turns for a single waking group within the existing item/time allowance; today that group can restore only one soldier per 100 ms even when budget remains. Never introduce a visibility or population veto that consumes/cancels a missing soldier's record.
5. **Remove repeated setup work.** Reject role-excluded groups before enumerating their agents. Apply delayed combat-profile setup to the restored soldier, or coalesce equivalent group passes. Currently every restored soldier queues another whole-group profile pass two seconds later, which can approach N-squared member visits during a quick group restoration. Preserve the post-initialization timing. See [setup](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_AI_Group.c:6128) and [profile loop](F:/Mikes-Invade-and-Annex-Exp/Scripts/Game/IA_AI_Group.c:4437).
6. **Measure before raising limits further.** Report scan duration, ready count/oldest age, wake backlog/oldest age, successful soldiers per second, capture/delete/restore durations, maximum transaction duration and budget exhaustion. Separate policy waits from ready-queue waits. Keep these counters/logs debug-gated and summarized instead of adding a line per scan or member.

A starting worker allowance around the existing 4 ms, shared across queued caching and restoration, is a test setting rather than a promise. Measure the separate eligibility scan; if that scan itself causes spikes, distribute group checks across worker calls while preserving the intended detection interval. A script time check runs **between** operations; it cannot interrupt a single expensive native spawn/delete or account perfectly for later replication/client work. Keep the count limit as well. The stock [building-interior cleanup](D:/ReforgerGameSources/data/data007/scripts/Game/Destruction/SCR_RegionalDestructionManager.c:211) also distributes direct deletes, but its 30-prop cap is not evidence that 30 equipped characters per frame is safe.

The main gameplay tradeoff is explicit: a team cached before entry can return outside its intended building and still need to walk in. A 1000 m wake lead provides some approach time; it cannot guarantee settlement before aircraft or teleports. Requiring final indoor placement while also guaranteeing rapid removal is not always compatible when pathfinding stalls.

**What should wait for measurements**

Keep capture and deletion as a synchronous group transaction initially. Deleting one soldier per frame would require new states for partial removal, newly wounded members, player possession, arrival during deletion, logical counts, cancellation and objective shutdown. Add that complexity only if measured whole-group transactions exceed the acceptable frame-time cost. Even then, one equipped character remains the minimum native teardown unit.

Initial spawning is also outside the dynamic restoration allowance: many independent group callbacks can overlap. If telemetry identifies initial-spawn overlap as a major source of spikes, introduce a shared initial-spawn budget only in the enabled path. Avoiding the initial spawn entirely would require authored or separately generated placement records and would change the original seed-and-record design.

**Smallest decisive runtime comparison**

Use the same objective, player count and hardware for the current implementation and the bounded update. Keep all players beyond 1500 m, record eligibility versus queue time, and compare actual characters removed, editor budget, server frame-time median/p95/p99, and the worst transition frame. Exercise both many small groups and one large group. Repeat return/withdrawal, then test two player clusters, a rapid arrival, a casualty/downed member, OFF during restoration and objective shutdown during queued work. Verify unfinished building teams resume their approach without duplicating soldiers or losing their movement pin.

The existing log proves physical removal and identifies policy delays. It does not contain the timings needed to claim that native teardown is the lag source, that a particular new AI-per-second rate is safe, or that multiplayer frame times improved. The proposed instrumentation and this comparison are the next evidence needed.
