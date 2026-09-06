# Dynamic base objective — implementation runbook

Planning artifact only. The gameplay feature has not been implemented or tested in-engine.

Read `DYNAMIC_BASE_OBJECTIVE_PLAN.md` first for the intended experience. This runbook is the implementation contract for v1. Proposed identifiers below are new APIs to implement, not claims that they already exist. Use the current repository and `D:/ReforgerGameSources/data/data007/` as the source of truth for engine APIs. Do not invent pathfinding, replication, resource GUID, faction, inventory or UI APIs to make pseudocode compile.

## 1. Fixed v1 decisions

1. One new terminal objective type: `BaseAssault`. Do not implement other dynamic missions in this change.
2. At most one base attempt and one defense per AO activation. Default chance is 100%; disabled/missed/failed pre-reveal selection returns to the current authored-defense decision, which retains its 80% roll.
3. Successful base selection works even when the AO has no `DefendObjective` marker. It replaces selection of the authored defense location. It does not pass through its 80% roll.
4. Required ordinary objectives finish first. Optional mortar pits remain optional. The new base is never included in the ordinary marker completion counter.
5. Use a transient `IA_Area`/`IA_AreaInstance` host and an objective-owned capture evaluator. Do not create an `IA_AreaMarker` for the base and do not reuse `IA_DefendEvent` as its lifecycle.
6. Keep the base host's internal enemy faction identity throughout capture and defense. Track player ownership as objective state. Do not call `OnFactionChange(US)` to capture the base; that mutates reinforcement and group behavior intended for ordinary zones.
7. `Seize → Regroup → Warning → Defend` occurs at one physical site. No defense mission object is active before Warning finishes.
8. The new director gets one server `CallLater` tick at 1000 ms. The placer may have its own 200 ms bounded work callback; it never advances mission state independently. Existing defense update scheduling remains unchanged in v1; do not add a third defense tick from the new director.
9. New base settings, enemy faction and defense options are snapshotted at selection. Changes to those options affect the next selection. Existing global combat/profile adjustments and normal defense population scaling retain their current live behavior. A queued force action carries the requesting AO activation token and is rejected if it becomes stale.
10. Initial art uses USSR field-base assets. Existing configured enemy faction selection still controls actual enemy soldiers/vehicles. Log an art mismatch for non-USSR enemy selections rather than overriding the administrator's enemy settings.
11. No new respawn, unlimited arsenal, construction service, controllable mortar, manned static weapon, or spawned playable vehicle is required in v1. Author parking/service props and cover. A bounded friendly medical/ammo cache is included only after ordinary capture/defense works; specify and verify real inventory resources before enabling it. It is not a release blocker for the core objective.
12. Two authored sizes are required for final delivery: Full 180 × 140 m and Compact 120 × 100 m. A development milestone may implement Full first. No model scaling or arbitrary module displacement.

## 2. Source map and hazards to preserve

Read these symbols, using symbol search rather than relying on current line numbers:

| File | Symbols / reason |
| --- | --- |
| `Scripts/Game/IA_MissionInitializer.c` | `ProceedToNextZone`, GM activation paths, `CheckCurrentZoneComplete`, `CheckAndStartDefendMission`, `SuccessZoneComplete`, `OnDefendMissionComplete`, `ForceFinishAllCurrentAreaInstances`, `ForceFinishCurrentAreaInstancesExceptDefend`, `RetireGroupObjectivesForDefend`, `RPC_ForceCompleteZone`, `RPC_ForceCompleteZoneAndDefend`. These own sequencing and cleanup. |
| `Scripts/Game/IA_Game.c` | `PeriodicalGameTask`, `AddTransientArea`, `RemoveTransientArea`, `ClearAllAreas`, `SetActiveDefendMission`, `AddEntityToGc`. Transient hosts already get `RunNextTask`; ordinary area lookup does not automatically cover every transient use. |
| `Scripts/Game/IA_AreaInstance.c` | `Create`, `RunNextTask`, `OnAttacked`, `UpdatePlayerScaling`, `ForceFinish`, `AddMilitaryGroup`, `RemoveMilitaryGroup`, `SetDefendMode`, `CancelPendingSpawns`, deferred cleanup. Ordinary `Create` automatically generates AI, civilians, vehicles and an elite patrol. |
| `Scripts/Game/IA_DefendMission.c` | `Create`, `CollectAffectedAreas`, `BeginEnhancedHold`, `StartDefendMission`, `EndDefendMission`, `OnHostAreaForceFinish`, `GetOrCreateQrfManager`, `SpawnEventGroup`. `CollectAffectedAreas` finds the closest ordinary area; a transient host needs explicit injection. |
| `Scripts/Game/IA_EnhancedDefendDirector.c` | `Start`, `StartClock`, `TickEvents`, `BuildEventPlan`, `FireFirstBeat`, `CleanupEvents`. Prepare/contact handling must be bypassed only for the new prepared handoff. |
| `Scripts/Game/IA_SideObjective.c` | Existing `CreateTransient`/`AddTransientArea` pattern and `Math3D.MatrixMultiply4` usage. Reuse actual patterns, not all assassination behavior. |
| `Scripts/Game/IA_AreaMarker.c` | `CaptureZoneQueryCallback`, pawn/world-position helpers, life-state filtering and contribution submission. Read these to implement the new sampler; ordinary scoring remains unchanged. |
| `Scripts/Game/IA_Config.c`, `IA_AdminOverrides.c`, `IA_PlayerController.c` | Settings, hand-written JSON persistence, authenticated RPC pattern. |
| `Scripts/Game/UI/Menus/IA_AdminConfigMenu.c` | `BuildDefensePage`, `PopulateFromConfig`, `SubmitAdminConfig`. The visible menu is constructed through MUI, so editing a layout file alone is insufficient. |
| `Scripts/Game/UI/IA_ObjectiveHudStrip.c`, `IA_CaptureHud.c`, `IA_DefendHud.c` | Reuse visual conventions. Existing capture tiles display locally by capture radius; the new regroup/status tile must also be visible to travelling players. |
| `D:/ReforgerGameSources/data/data007/Scripts/Game/AI/SCR_AIWorld.c` | `GetNavmeshRebuildAreas`, `RequestNavmeshRebuildAreas`, `RequestNavmeshRebuildEntity`. Collection includes children. |

Important existing behavior:

- `IA_Game.AddEntityToGc` only queues an entity; its GC deletes it later without checking nearby players. The site must decide when it is safe before queueing roots.
- `IA_AreaInstance.ForceFinish` calls `defend.OnHostAreaForceFinish()` before shutting down when the area hosts active defense. Currently that leads to completion. Add a cancellation path before using ForceFinish for aborted base missions.
- Do not restore the deleted `PLAYER_EXPERIENCE_IMPROVEMENTS.md` or overwrite existing changes in `IA_AI_Group.c`, `IA_AreaMarker.c` or `resourceDatabase.rdb` as incidental cleanup. Recheck the working tree before implementation.
- No automated Enforce build/test command was established during planning. Compile through the available Workbench toolchain and report exactly what ran; text searches or a JavaScript test are not an Enforce compile.

## 3. New files and responsibility boundaries

Create the following files. Avoid moving unrelated existing code while adding the feature.

| New file | Owns | Must not own |
| --- | --- | --- |
| `Scripts/Game/IA_DynamicObjectiveTypes.c` | Global enums and small request/result data classes. | Engine entities or tick scheduling. |
| `Scripts/Game/IA_DynamicObjectiveDirector.c` | AO activation identity, one selection decision, objective reference, terminal result dispatch. | Terrain algorithms or defense waves. |
| `Scripts/Game/IA_BaseAssaultObjective.c` | Seize/regroup/warning state, roster, contributions, defense handoff, published status. | Ordinary AO marker completion. |
| `Scripts/Game/IA_DynamicSiteLayout.c` | Full/Compact manifests and typed module/anchor metadata. | Runtime random relocation of individual modules. |
| `Scripts/Game/IA_DynamicSitePlacer.c` | Candidate shortlist, bounded validation/spawn work, partial rollback, diagnostic result. | Task announcements or AO advancement. |
| `Scripts/Game/IA_DynamicSiteInstance.c` | Spawned root entities, host reference, selected transform, exclusion geometry, delayed cleanup ownership. | Player capture score evaluation. |
| `Scripts/Game/IA_BasePlayerSampler.c` | Eligible player roster and spatial/life-state sampling, hostile query. | Mission transitions or notifications. |
| `Scripts/Game/UI/IA_BaseObjectiveHud.c` | One MUI tile following current HUD conventions. | Server decisions or local countdown authority. |
| `Prefabs/IA_DynamicSites/Modules/` | IA-owned, behavior-audited HQ, barracks, medical, storage, service and perimeter modules. | Conflict base/spawn/construction logic. |

Store manifests as typed Enforce data constructed by `IA_DynamicSiteLayout` for v1, rather than inventing a generic JSON loader. Author visual module prefabs in Workbench; use code only for their placement metadata. Register every new resource with the project's normal metadata workflow. Do not invent or copy GUIDs from another resource.

Define the small records in `IA_DynamicObjectiveTypes.c`: `IA_BaseObjectiveSettings` copies the nine config fields plus the selected enemy Faction and the current defense settings needed at handoff; `IA_DynamicSiteCandidate` stores center/yaw/layout ID/seed/score/validated capabilities; `IA_DynamicSiteResult` stores serial, success flag, reason code and optional site; `IA_BaseRosterEntry` stores session/persistent identity, deployed-last-seen time, grace-start time and snapshot eligibility. A result with success=false cannot transfer a half-built site to gameplay; the placer retains it for cleanup.

Use global enums, `ref` ownership conventions and ordinary methods consistent with this repository. Do not introduce C# interfaces/properties, async/await, lambdas or JavaScript collection helpers into Enforce source. Signature blocks below express the desired contract; verify syntax with the actual compiler.

## 4. Identity, lifecycle and ownership contract

Add to `IA_MissionInitializer`:

```text
protected int m_iAoActivationSerial;
protected ref IA_DynamicObjectiveDirector m_DynamicObjectives;
GetAoActivationSerial() -> int
GetDynamicObjectiveDirector() -> IA_DynamicObjectiveDirector
BeginDynamicObjectiveAo(int groupId) -> void
HandleDynamicObjectiveResult(int activationSerial, int result, string reason) -> void
```

Increment `m_iAoActivationSerial` exactly once on every actual AO activation, including GM reactivation of the same group number. Create one director if absent and call `BeginAo(serial, groupId)` after live markers are assigned and before starting completion polling. The serial is session-local; restarting the server restarts the mission and does not resume this objective.

Core proposed contracts:

```text
IA_DynamicObjectiveDirector
  BeginAo(int serial, int groupId)
  TryBeginTerminalObjective(bool forceBase = false) -> bool
  Tick()
  Cancel(int cancelReason)
  IsBlockingAoCompletion() -> bool
  OwnsActivation(int serial) -> bool
  OnObjectiveResult(int serial, int result, string reason)

IA_BaseAssaultObjective
  Begin(int serial, int groupId, IA_BaseObjectiveSettings settings)
  Tick(int nowMs)
  Cancel(int reason)
  OnSiteReady(int serial, IA_DynamicSiteInstance site)
  OnSiteFailed(int serial, string reason)
  OnDefenseEnded(int serial, bool completed)
  GetPhase() -> IA_BaseObjectivePhase

IA_DynamicSitePlacer
  BeginPrecompute(int serial, int groupId)
  BeginPlacement(int serial, IA_BaseObjectiveSettings settings)
  ProcessWork()
  Cancel(int serial)
  IsComplete() -> bool
  TakeResult() -> IA_DynamicSiteResult

IA_DynamicSiteInstance
  GetHost() -> IA_AreaInstance
  GetCapturePoint() -> vector
  GetAssemblyPoint() -> vector
  ContainsExpandedFootprint(vector position, float marginM) -> bool
  BeginDeferredCleanup()
  TickCleanup()
  CancelOwnedSpawns()
```

`TryBeginTerminalObjective` returns true only when it has accepted responsibility for blocking AO completion, including while placement is pending. Return false for a disabled/missed decision so the existing fallback branch runs once. An accepted asynchronous placement failure returns through `HandleDynamicObjectiveResult(Fallback)`; never return false while a spawn job continues.

`BeginAo` resets the decision latch, selected settings and current result. The group ID alone is insufficient as a job token. Every queued request and callback carries the serial; stale callbacks discard their work and release any resources they created, without changing the new AO.

Selection order is explicit: current-serial/live-AO check → already-decided check → retained-site budget check → enabled/GM gate unless forced → 0/100 special cases or one random comparison → snapshot settings/faction → Placing. At 0 return fallback without sampling; at 100 accept without a boundary-sensitive random comparison. Forced requests bypass enabled/chance/GM-auto only, not retained-site, asset, geometry or AI limits.

Initializer strongly owns director; director strongly owns active objective; objective strongly owns site and settings; site strongly owns the transient host and root list. Back-references are non-owning where supported by the existing Enforce patterns. During deferred cleanup, move a strong site reference into a director-owned retired-site list. Do not clear that list on BeginAo. Otherwise the next AO can orphan the previous base's props.

## 5. State table and exact transition rules

Append global enums; preserve numeric values of existing enums. Suggested new phase values:

```text
None=0, Placing=1, Seize=2, Regroup=3, Warning=4,
Defend=5, Completed=6, Cancelled=7, Failed=8
```

Director results: `Completed`, `Fallback`, `Aborted`, `Failed`. `Failed` after reveal is an admin-attention state, not success. Cancellation reasons: `AdminComplete`, `AdminDirectDefense`, `AoReplaced`, `MissionShutdown`, `RuntimeFailure`.

| State | Entry work | Advance condition | Exit / failure behavior |
| --- | --- | --- | --- |
| None | Await required-objective completion; precompute may run. | One successful selection/force request. | Disabled/miss uses existing fallback once. |
| Placing | Snapshot settings and enemy faction. Stop completion polling; retain old AO instances until commit. Start bounded placer. No assault task yet. | Complete site, ready garrison and valid host. | Pre-reveal failure rolls back and invokes fallback once. |
| Seize | Commit site; retire required ordinary tasks; shut down old non-mortar hosts and their pending content; publish seize task/status. | 90 accumulated seconds with a living friendly player in capture zone and zero effective hostiles there. | Empty/contested pauses capture, without decay. Runtime loss of required site/host enters Failed. |
| Regroup | Complete seize task once, freeze contributor totals, capture roster, publish assembly status. No new waves. | At least 90 seconds elapsed AND presence >= target, OR 240 seconds elapsed AND presence >= 1. | If zero present, remain Regroup and show awaiting forces, even after max time. |
| Warning | Publish a 30-second warning with server remaining seconds. No waves yet. | Warning elapsed and at least one living grounded friendly remains in assembly zone. | If empty, return to Regroup, retain capture/roster, and require a fresh full warning after return. |
| Defend | Start prepared defense once with explicit site host. New director does not tick defense. | Defense callback completed=true. | completed=false enters Failed unless already cancelling. |
| Completed | Publish completion once; clear director-owned map marker; transfer site to cleanup list. | Terminal. | Initializer advances the AO exactly once. |
| Cancelled | Invalidate active jobs; abort defense without success; dismiss active objective tasks; defer site cleanup. | Terminal. | Caller determines whether to complete AO, start direct defense or replace AO. No automatic fallback. |
| Failed | Stop owned generation/defense without success, preserve useful marker and status, log reason. | Admin Complete AO or AO replacement. | Do not silently report base/defense success. |

Use `System.GetTickCount()` deltas for server gameplay accumulation and clamp each capture tick's contribution to 0–2 seconds so a long server stall cannot finish capture instantly. Store elapsed milliseconds rather than assuming each callback is exactly one second. For replication send authoritative remaining seconds, not raw server tick-count timestamps that clients may interpret against a different clock.

Regroup elapsed is measured from capture and does not reset on Warning → Regroup. The 30-second warning resets every time. If hostiles enter the command zone during Regroup or Warning, pause the transition and show “Clear the command area”; cancel an active warning. Do not reset the completed capture or award it again. Normal combat with surviving garrison is allowed; no new enemies are created in these phases.

## 6. Exact sequencing edits

### IA_MissionInitializer

At the top of `CheckCurrentZoneComplete`, after normal server/current-group checks, return if the director owns this serial and blocks progression. Immediately before the existing `CheckAndStartDefendMission(currentGroup)` call in the all-required-complete branch:

```text
if director.TryBeginTerminalObjective(false):
    remove CheckCurrentZoneComplete callback
    return
run existing CheckAndStartDefendMission / SuccessZoneComplete fallback
```

Factor that existing fallback into a private helper `ContinueWithoutDynamicBase(serial, groupId)` so synchronous misses and asynchronous placement failures use the same once-only path. It must check the current serial and its own fallback-dispatched flag, preserve the existing defend-host cleanup branch, and never call dynamic selection recursively.

Do not call `ForceFinishCurrentAreaInstancesExceptDefend` while Placing. There is no active defense host yet and the original instances are needed if placement fails. On successful commit, add `RetireOrdinaryObjectivesForBase(serial, host)` to:

1. Verify current serial/site host.
2. Cancel delayed ordinary area/vehicle spawns.
3. Retire ordinary required objectives, using the existing retirement behavior while preserving optional mortar capture.
4. Stop the old area-group manager and create a new manager list containing the new host plus any retained optional mortar hosts. Call it only for allowed pressure during each new phase.
5. ForceFinish old non-mortar instances, retaining the explicit base host and optional mortar instances. The base host is transient, so explicitly preserve it even though it is not in `m_currentAreaInstances`.

Suppress automatic new QRF/side-mission selection during Placing, Seize, Regroup and Warning. Keep existing active unrelated side missions alive. A small query `BlocksAutomaticPressure()` on the director should gate automatic producers; do not implement this by changing persisted admin settings or globally cancelling another side objective. Already spawned nearby enemies remain and count toward site contention and budget if they enter it. Optional mortar capture and its existing site-specific threat remain; do not create a new ambient artillery loop during the regroup wait.

`HandleDynamicObjectiveResult(Completed)` verifies and latches completion, then uses `OnDefendMissionComplete()` once for the existing post-defense flow. `Fallback` calls the factored fallback. `Aborted` does nothing further. `Failed` leaves the AO waiting for admin recovery.

Hook cancellation before cleanup in `RPC_ForceCompleteZone`, `RPC_ForceCompleteZoneAndDefend`, AO replacement, `InitializeNow`, mission shutdown and GM activation/reset paths. Invalidate the job serial before deleting hosts. The caller remains responsible for its existing intended progression. Do not call `OnDefendMissionComplete()` as part of cancellation.

### IA_Area / IA_AreaInstance

Append `DynamicBase` to `IA_AreaType`. Its ordinary military-group, civilian and automatic reinforcement defaults must all return zero. These defaults supplement, not replace, the creation policy below.

Extend `IA_AreaInstance.Create` with a final optional `bool dynamicObjectiveHost = false`. Set `m_bDynamicObjectiveHost` before `UpdatePlayerScaling` or any spawn call. Keep common field/reaction-manager initialization. When true, skip all automatic initial infantry, civilians, initial military/civilian vehicles, mortar crew and elite-patrol setup. Existing callers remain unchanged.

Create the base host as:

```text
areaName = "IA_Base_" + serial + "_" + groupId
area = IA_Area.CreateTransient(areaName, DynamicBase, assemblyPoint, 150)
host = IA_AreaInstance.Create(area, IA_Faction.USSR, selectedEnemyFaction, 0, groupId, true)
IA_Game.Instantiate().AddTransientArea(host)
```

The `IA_Faction.USSR` enum represents the existing enemy-side abstraction used by this mod; the actual `Faction` argument is the selected enemy catalog. Follow existing faction resolution, not a new hardcoded soldier list.

For dynamic hosts, prevent automatic reinforcement/vehicle replenishment/elite generation from `OnAttacked`, ordinary reinforcement tasks, vehicle management and the population-scaling adjustment block. Keep storing scale/count and keep defense-specific wave APIs available. Do not set `m_canSpawn=false` permanently because that would also prevent defense spawning.

In `RunNextTask`, preserve strength, military order, reaction and task maintenance. Skip ordinary civilian, attacker-generator, vehicle-replacement, radio and side-defense jobs for this host. Keep defense wave group and hunter behavior working when the host later enters defend mode. Review all indirect generators reached by these maintenance methods; testing only initial Create is insufficient.

### IA_DefendMission / enhanced director

Add a new factory, leaving existing `Create` callers intact:

```text
CreateForDynamicBase(vector defendPoint, int groupId, string displayName,
                    IA_AreaInstance host, Faction enemyFaction,
                    int activationSerial, IA_Config defenseConfigSnapshot) -> IA_DefendMission
```

Store explicit host/faction, owner activation serial and `m_bPreparationAlreadyComplete=true`. `CollectAffectedAreas` must prefer this host, verify it is live and matches group ID, insert only once and return; no nearest-area fallback for an invalid explicitly provided host. Pass layout approach/exclusion data via the site/context reference.

Create the defense snapshot at base selection by packing/unpacking the current defense extras into an objective-owned `IA_Config`. The new factory chooses legacy/enhanced from this snapshot, not the global config at Warning completion. Expose `GetDefenseConfig()` on the mission: return its explicit snapshot when present, otherwise the existing global config. Change enhanced `Start` to obtain config through that method. This closes the existing global-config reads in both factories/startup without freezing unrelated global AI controls.

Add `AbortDefendMission()` alongside successful `EndDefendMission()`. Factor shared stop work if helpful: stop enhanced callbacks/events, shut down owned QRF manager, stop future waves, clear game active-defense pointer if it still references this mission, and set inactive. Success completes task/HUD and sends one callback; abort dismisses/cancels tasks/HUD and never calls initializer success. For dynamic hosts, `OnHostAreaForceFinish` reports an abort rather than success. Legacy authored behavior remains unchanged unless an explicit admin cancellation invokes Abort.

At the end of a successful dynamic defense, notify `director/activeObjective.OnDefenseEnded(serial, true)` instead of directly invoking initializer progression. Mark the mission inactive and detach the active-defense reference before calling out to prevent reentrant cleanup from completing twice.

In `IA_EnhancedDefendDirector.Start`, after doctrine/duration/event-plan setup and `BeginEnhancedHold`:

- Existing authored path retains Prepare/contact/timeout behavior.
- Prepared dynamic-base path starts the clock immediately, enters Probe, emits counterattack wording rather than “Contact”, fires the initial beat once, and starts the existing update mechanism.
- Parameterize `StartClock` messaging or use a small helper; do not fabricate player contact to force the transition.
- For dynamic bases, shift all planned off-site event offsets by the same amount if the first would occur before 120 seconds of defense clock time. Preserve order and spacing; retain existing suppression when an event is too late for the current defense phase.

`GetOrCreateQrfManager` must use a manager with the live base host; do not reuse a manager whose area list consists of shut-down ordinary areas. Guard vehicle/doctrine selection with validated approach capabilities. Empty capability pools must fall back to infantry behavior rather than returning a null mission.

## 7. Player, capture and garrison algorithms

### Shared spatial definitions

- Horizontal containment uses X/Z squared distance. Height is checked separately.
- Grounded-present: living conscious friendly controlled character, no airborne aircraft occupancy, inside radius, and feet no more than 3 m above the sampled terrain/support surface. Treat grounded ground-vehicle occupants consistently using their vehicle world position; aircraft occupants do not count even when passing low. Validate passenger pawn position behavior using existing pawn-position helpers.
- Capture: radius 35 m at capture anchor, grounded-present and inside the HQ/command region. No AI allies count as a human capturer.
- Hostile contest: live, conscious enemy characters in the capture cylinder; include occupants of nearby enemy vehicles at their correct world position. Ignore corpses and unconscious/incapacitated AI. Use faction hostility to the friendly player faction; avoid assuming every hostile is literally keyed `USSR`.
- Terrain/support height tolerance must not count a player on an unrelated roof or a hill above the HQ. Flat-site validation and a small vertical band are both required.

Implement the sampler using the existing PlayerManager controlled-entity and character damage/controller patterns. Existing `CaptureZoneQueryCallback` illustrates those checks but uses existing ordinary faction counting; do not change it globally just to implement the base rule.

### Eligible roster

During the AO, every 5 seconds record player IDs that have been physically deployed in the approved AO region, along with last-seen time. Do this before the terminal objective starts so capture-time roster creation can distinguish deployed casualties from people who stayed at HQ.

At capture, freeze to connected friendly players recorded in that region within the last 10 minutes, and include any current friendly player inside the base assembly radius. Exclude spectators and non-friendly players. Players who only remained at HQ are not in the denominator.

For each frozen member: a connected/living member remains eligible even if they return to HQ after the snapshot; this avoids needing a new HQ-boundary detector and the wait cap handles their absence. A dead/unconscious/no-pawn member keeps a 120-second recovery grace before leaving the denominator if still unavailable; disconnect removes immediately. A recovered frozen member can rejoin while still in Regroup, but never increase the target above its initial capture-time value. New player IDs after the snapshot do not raise the target and do not substitute for eligible members in the percentage check; they can satisfy the at-least-one-presence safety check after the wait cap. Once Warning starts, roster changes cannot raise its target or restart it except when there are no present players or the command zone is contested. If player IDs can be reused after disconnect, store the existing persistent player identity alongside the session ID and treat a different identity as a late join.

`target = max(1, ceil(eligibleCount * regroupFraction))`, capped at initial target. For the percentage route compare eligible members present to target; for the timeout route compare all grounded friendly players present to at least one. Never auto-start from `0 >= 0`.

Example at defaults: 10 eligible, target 6. Five arrive at 90 seconds: keep waiting. Six arrive at 110 seconds: warn until 140 seconds, then defend. Only two ever arrive: warn at 240 seconds, defend at 270 seconds. Nobody arrives: keep waiting. If everyone leaves during the warning, return to Regroup and issue a fresh 30-second warning after someone returns.

### Capture and contribution

Accumulate capture time only while the capture condition holds. Pause with retained progress when empty/contested. Do not apply ordinary `CAPTURE_RATE=1.3`; 90 seconds here means 90 seconds of valid occupation. On the first transition to Regroup, mark/complete the seize task and submit each participant's accrued contribution through the existing `IA_StatsManager.QueueCaptureContribution` pattern exactly once. Freeze/clear that ledger on success or cancellation. Award only actual contribution seconds, not a second ordinary-marker reward.

### Garrison

Default budget: `clamp(round(24 * IA_Game.GetAIScaleFactor() * baseGarrisonMultiplier), 4, 36)`. GetAIScaleFactor already contains existing player/admin scaling; do not multiply again by connected player count or the global AI multiplier. Snapshot this budget once. This resolves the overview's approximate 12–36 range and permits fewer soldiers for solo/low scaling.

Allocate a two-person HQ group and a two-person entrance group first (the minimum budget is four). Split remaining troops into groups of at most four, assigning west side, east side, rear and reserve in that order, then remaining perimeter stations in manifest order. The last group may contain fewer than four. No player-count-based changes spawn replacement garrison after the assault begins. Limit simultaneous new group spawns to one per placer work callback, and respect the engine's current AI activation budget; if all required guards cannot become ready before the placement timeout, fail before reveal.

Use the real `IA_AiGroup.CreateMilitaryGroupFromUnits`/`Spawn`/`SetAssignedArea`/`host.AddMilitaryGroup` patterns in `IA_DefendMission.SpawnEventGroup`. The inspected group API provides `SetHoldPost(vector pos, float radius=0)`, `IsHoldingPost()` and `IA_AiOrder.Hold`; the Hold waypoint sets an indefinite holding time and uses the configured radius. For guards, set their assigned area and `SetHoldPost(worldPost, 5)` before adding them to the host, then give the existing Hold order. Do not use `SetDefendMode(true, guardPost)` as a fixed sentry command; defend mode also drives attack behavior elsewhere. V1 does not require static-weapon crews.

Group construction is staggered: a non-null group is not proof that all requested soldiers exist. `CreateMilitaryGroupAtPosition` initializes `m_pendingUnitsToSpawn` and calls `SpawnNextUnit`; expose a small read-only `HasPendingUnitSpawns()`/`GetPendingUnitCount()` if no equivalent exists in the current source. Wait until the requested live garrison count is reached and pending count is zero before reveal. Verify `Despawn()` cancels staggered spawning; add an abort guard to `SpawnNextUnit` if it does not. Do not introduce another independent soldier-spawn loop in the objective. Also verify unit offsets from `SpawnNextUnit` stay within validated walkable guard pads; validating only the group origin is insufficient.

At capture, surviving garrison remains hostile; do not respawn, teleport or change allegiance. Keep surviving guards registered and counted. At defense handoff permit the existing defense behavior to manage suitable surviving groups, preserving holding-post exceptions. Wave budget counts existing living host troops and pending wave reservations before spawning more.

## 8. Authoring manifest and layout coordinates

All coordinates below are local X/Z metres. Y is resolved by the module's support-pad policy. Root origin is the assembly center; local +Z points toward the rear of the base; front gate is on -Z. Module doors face the named lane. Actual prefab forward-axis offsets must be measured once and recorded as `prefabYawCorrectionDeg`, not guessed for each placement.

Module metadata fields:

```text
id, prefab ResourceName, localPosition, localYawDeg, prefabYawCorrectionDeg,
halfWidthM, halfDepthM, supportPoints[], maxSupportDeltaM,
groundingPolicy (UprightPad/TerrainSegment), required,
keepClearVolumes[], estimatedExpandedEntities
```

The footprint and support metadata describe the entire nested module, including furniture/collision beyond the tent fabric. Estimated expanded count is an authoring value; actual count is checked after spawn. `required=false` is only for decorative clutter, never HQ, entrances, lanes, perimeter safety or the capture anchor.

### Full layout target

Envelope X [-90,90], Z [-70,70]; assembly (0,0); capture (0,24); three open entries at (0,-70), (-90,-8), (90,-8).

| Socket | X | Z | Maximum intended pad W × D | Orientation / content |
| --- | ---: | ---: | --- | --- |
| HQ | 0 | 35 | 24 × 20 m | Door toward -Z; furnished command module. |
| Radio | 23 | 46 | 10 × 10 m | Antenna/service props outside tent pad. |
| Barracks A | -62 | 36 | 18 × 18 m | Door toward +X. |
| Barracks B | -62 | 12 | 18 × 18 m | Door toward +X. |
| Barracks C | -35 | 36 | 18 × 18 m | Door toward -X; shared lane with A. |
| Barracks D | -35 | 12 | 18 × 18 m | Door toward -X; shared lane with B. |
| Medical | 59 | 31 | 28 × 28 m | Door toward -X, accessible from cross lane. |
| Motor pool | -49 | -41 | 42 × 30 m | Bays face central/main lane; no playable parked vehicle required. |
| Supply | 30 | -31 | 22 × 22 m | Unloading access from main lane. |
| Fuel/service | 65 | -49 | 22 × 22 m | Separated from assembly and supplies. |
| Gate cover | ±17 | -61 | 10 × 8 m each | Facing outward; leave main gate fully open. |
| Perimeter posts | Near corners/side sectors | — | Measured per module | Outward firing arcs; no seal across side entries. |

Reserve main vehicle lane X [-5,5], Z [-70,14], cross lane Z [-13,-3] across the compound, and a turning pad around (0,-36) with at least 18 × 18 m usable clearance. Leave door and pedestrian paths explicit in module metadata.

### Compact layout target

Envelope X [-60,60], Z [-50,50]; assembly (0,0); capture (0,17); entries (0,-50), (-60,-5), (60,-5).

| Socket | X | Z | Maximum intended pad W × D | Content |
| --- | ---: | ---: | --- | --- |
| HQ | 0 | 27 | 20 × 18 m | Compact command module, door toward -Z. |
| Barracks A/B | -37 | 28 / 9 | 18 × 16 m each | Two tents, doors toward +X. |
| Medical | 37 | 24 | 22 × 22 m | Compact medical module, door toward -X. |
| Motor pool | -33 | -30 | 28 × 22 m | Small aligned yard. |
| Supply | 25 | -24 | 18 × 18 m | One supply module. |
| Fuel/service | 47 | -37 | 12 × 14 m | Small separated service corner. |

Compact capture radius is **30 m** rather than 35 m so its footprint stays inside the compact envelope; assembly remains 150 m. Reserve main lane X [-4,4], cross lane Z [-10,0], and a 16 × 16 m turning area. Use compact perimeter pieces with the same three-entry requirement. Do not copy full-size module bounds when substituting compact art.

### Perimeter and guard sockets

Use deliberately separated cover stations, not an unbroken sealed wall. The following additional local sockets remove discretion about where the first perimeter goes. Each cover module must fit a maximum 8 × 6 m pad; a tower module, if used, replaces a corner cover station inside that pad rather than being added on top of it.

| Layout | Cover stations (X,Z) | Outward direction |
| --- | --- | --- |
| Full rear | (-76,62), (-46,62), (0,62), (46,62), (76,62) | +Z |
| Full sides | (-82,38), (-82,10), (-82,-30), (-82,-56); mirrored at X=82 | -X on west, +X on east |
| Full front | (-65,-62), (-38,-62), (-17,-61), (17,-61), (38,-62), (65,-67) | -Z; eastern station offset outward to clear fuel pad |
| Compact rear | (-49,43), (-24,43), (24,43), (49,43) | +Z |
| Compact sides | (-53,24), (-53,-25); mirrored at X=53 | -X on west, +X on east |
| Compact front | (-40,-43), (-17,-43), (17,-43) | -Z; omit southeast station to leave fuel/service pad clear |

Full gate-cover sockets already listed in the module table are the same stations at (±17,-61), not extra overlapping modules. Keep side-entry cross lanes open; cover ends may not enter their keep-clear volumes. These stations intentionally leave maneuver gaps. Additional visual perimeter wire/fencing is optional only where it does not close the three validated routes or exceed budgets.

Guard group priorities, before distributing extra groups: (1) HQ exterior, (2) front gate, (3) west side, (4) east side, (5) rear perimeter, (6) central reserve. Full local posts are (0,22), (-14,-54), (-73,-24), (73,-24), (0,54), (19,1). Compact posts are (0,16), (-12,-36), (-46,-18), (46,-18), (0,39), (14,5). These are outdoor stand targets with 5 m hold radius; adjust inside their validated pad only if collision requires it and record the final authored socket. Higher garrison budgets add groups to remaining perimeter stations in manifest order, never a random point in a tent.

These are explicit authoring targets, not verified asset measurements. If a real required module exceeds its assigned pad, build an appropriate smaller subcomposition or deliberately revise the manifest and diagram before proceeding. Do not silently enlarge the module into a lane or auto-jitter its socket. Test both layouts at 0°, 90°, 180° and 270°; relative distances/door routes must remain identical.

Transform policy: build root transform from chosen origin/yaw, build local module transform including its yaw correction, then use `Math3D.MatrixMultiply4(root, local, world)` as the existing side-objective code does. Solve each upright module's support height, preserving all nested child offsets. For perimeter segments, use only the pitch/roll freedoms declared by that module. Keep the root yaw-only.

## 9. Placement algorithm and bounded work

Defaults in this section are implementation constants for v1, with diagnostic overrides available in development only. They are not engine limits.

1. Collect active group's primary marker circles, excluding `DefendObjective`, `MortarPit`, `RadioTower` and assassination side content. If none remain, use ordinary required markers excluding defense/mortar. If still empty, fail placement.
2. Eligible region is the **union** of these circles expanded by 350 m. An optional authored AO polygon is a later enhancement; do not add a new polygon-editor feature in v1. Test all footprint corners and a 10 m grid against this union so a rotated rectangle cannot bridge an unapproved gap simply because its center is inside.
3. Precompute at most 96 candidate centers from fixed-seed samples inside this union. Seed one `RandomGenerator` per AO activation and retain that seed. Keep at most 12 centers after cheap center/water/coarse-slope tests. At final selection, refresh player scoring and dynamic occupancy; precomputed legality is never final legality.
4. For each center test road-aligned heading when a usable road is available, then 0/90/180/270 degrees, deduplicating equivalent headings. In Auto size mode exhaust Full candidates before trying Compact. Do not force a road alignment if it blocks the only infantry entry.
5. Mandatory footprint/player clearance: distance from every current player position to the oriented base rectangle >=250 m. This uses ALL player entities/vehicle positions, not only eligible roster members. Exclude spectators without physical characters. A player outside the AO can still see a spawn.
6. Visibility: line-of-sight checks from current player eye/vehicle observation positions to footprint corners, center and representative tall-module tops. Use actual world/entity traces, ignoring the observing entity; any clearly unobstructed observer ray is a rejection. Distance is not proof of invisibility. This is a conservative finite sample, so diagnose it as such; actual visual playtests remain required.
7. Terrain: 10 m footprint grid plus every module support point; reject water, occupied cells and steep/discontinuous support. Start with per-pad delta <=0.35 m for tents, <=0.50 m for service yards, and overall footprint height spread <=4 m. Perimeter segment policies may allow local terrain following. Reject trunks/rocks/buildings using physical volume checks above ground; surface height alone is insufficient. Do not reject all terrain merely because a floor trace hits the ground it is intended to sit on.
8. Collision: validate module oriented bounds and keep-clear volumes, including doors, lanes and approach lanes. Registered dynamic/GM sites and retained mortar footprints are exclusion volumes. Existing roads may connect to an entry but may not be covered by a tent or sealed off by perimeter props.
9. Runtime navigation sanity: request/load relevant Soldiers navmesh tiles, inspect their validity, sample walkable positions along the reserved route lattice at 3 m spacing, and collision-test the linking segments with soldier clearance. Flood-fill that lattice from capture anchor to at least two distinct external entries. Reject disconnected candidates. This proves the tested geometric/nav-sampled corridor, not arbitrary engine AI route execution.
10. A full endpoint path-query/rebuild-completion API was not established from the inspected generated interfaces. Do not pretend `GetReachablePoint(origin,distance,...)` is a path between two endpoints, or `IsTileLoaded` means a rebuild finished. If a supported full query is found, wrap it in one adapter; otherwise use the conservative lattice check above and require actual AI traversal acceptance in Workbench before release.
11. Score mandatory-valid candidates with `median player distance + 0.35 * p90 player distance + 100 * overallHeightSpreadM`, using recently deployed participants rather than HQ occupants. Lower wins; use stable seed order for ties. This simple score balances movement without requiring an unverified route-distance API. Distances 400–800 m are design targets, not hard constraints. Record physical distance separately from tested route availability.
12. Preflight every required ResourceName. Spawn at most two module roots per 200 ms work callback. Apply a soft 4 ms work budget for checks, stopping after the current indivisible operation; one engine spawn may exceed it, so log measured callback duration. Count descendants; default ceiling 600 expanded site entities and 96 roots. If a finished authored layout needs a higher ceiling, raise only after profiling and record the measured baseline.
13. Collect rebuild areas for all structural roots through `SCR_AIWorld.GetNavmeshRebuildAreas`, merge overlapping bounds when straightforward while preserving `redoRoads` correspondence, request rebuilds, then re-run lattice/clearance checks. Allow up to 15 seconds of bounded rechecks and fail if invalid. Do not claim that merely waiting 15 seconds certifies completion.
14. Create the empty transient host, then spawn/register/confirm the finite garrison. Final player-distance/visibility validation happens before each module batch and once before reveal. Validation queries after spawn must ignore the newly owned site roots/children where they test external obstruction or visibility; otherwise the base can incorrectly screen itself from observers. Collision/nav tests of internal walkability still include the new structural obstacles.
15. Hard selection/placement deadline: 45 seconds from beginning Placing, including retries, validation and garrison readiness. Roll back on deadline or lost legality. Cleanup of rolled-back content remains proximity-aware if a player has approached it. Do not try another candidate until its predecessor's owned reservation has been released.

Before task reveal, assert: required modules present, host live, the entire snapshotted garrison spawned with zero pending units, HQ/entrance guard posts populated, no outstanding required module work, clearance valid and callback serial current. If any check fails, the objective has not committed and must use Fallback. When checking garrison count before reveal, do not count a dead guard toward the target or silently refill deaths forever; a killed guard during Placing indicates compromised placement and aborts the attempt.

## 10. Config fields, transport and persistence

Add explicit defaults both to attributes and member initializers where the existing configuration style requires them. Add matching baseline values to `Configs/IA/IA_Config_Master.conf`.

| Field on IA_Config | Type | Default | Clamp |
| --- | --- | --- | --- |
| `m_bDynamicBaseEnabled` | bool | true | — |
| `m_iDynamicBaseChancePct` | int | 100 | 0–100 |
| `m_bDynamicBaseInGm` | bool | false | — |
| `m_iDynamicBaseSizeMode` | int | 0 | 0 Auto, 1 Full, 2 Compact; invalid →0 |
| `m_iDynamicBaseCaptureSec` | int | 90 | 30–300 |
| `m_iDynamicBaseRegroupMinSec` | int | 90 | 0–300 |
| `m_iDynamicBaseRegroupMaxSec` | int | 240 | min–600 |
| `m_fDynamicBaseRegroupFraction` | float | 0.60 | 0.10–1.00 |
| `m_fDynamicBaseGarrisonMultiplier` | float | 1.0 | 0.25–2.0 |

Keep warning 30 seconds, AO margin 350 m and placement clearances as constants in v1. This avoids exposing safety-sensitive geometry before profiling. Add `ClampDynamicBaseSettings`, `PackDynamicBaseExtras`, `UnpackDynamicBaseExtras` to `IA_Config`.

Existing outer admin payload uses `|`; index 22 is defense extras and 23 AI-combat extras. **Append index 24**; do not shift previous fields. Inner format:

```text
version,enabled,chancePct,inGm,sizeMode,captureSec,regroupMinSec,regroupMaxSec,regroupFraction,garrisonMultiplier
1,1,100,0,0,90,90,240,0.6,1
```

Parser rules: require exactly the known 10 fields for v1, version=1, parse into a temporary settings record, validate numeric tokens, clamp, then apply. Missing, empty, malformed or unsupported payload leaves existing server settings unchanged. A valid zero is not an absent field. Reject non-finite floats. Do not use bare `ToInt()` failure-to-zero as proof of successful parsing.

Update all four directions:

1. `IA_AdminConfigMenu.SubmitAdminConfig`: build from the current config, apply the four visible widget values, preserve advanced fields, append `|` + extras. Do not construct a defaults-only object that silently resets advanced values.
2. `IA_MissionInitializer.ApplyPackedAdminConfig`: if `tokens.Count()>24`, call the parser. Existing authenticated `IA_PlayerController` RPC continues carrying the same string.
3. Replication: add `[RplProp()] string m_sDynamicBaseConfigPacked_Rpl`; assign in `PushConfigToReplication`, bump replication as existing code does.
4. `GetGlobalConfig` client branch unpacks this string. `PopulateFromConfig` uses those returned settings. A menu opened after a live save or by another admin must show the saved values.

For profile JSON add `dynamicBaseVersion:1` and one `dynamicBaseExtras` string with the comma payload. This fits the existing simple quoted-string extraction because it contains no quotes, backslashes or `|`. Store `m_bHasDynamicBaseOverride` as an internal parsed-presence flag; do not depend on a bool default to detect a missing section.

Wire `FillFrom`, `ApplyTo`, `ToJson`, `ParseJson` in `IA_AdminOverrides`. Old JSON without `dynamicBaseVersion` does not override new baseline fields. Unsupported/malformed new section is ignored with one warning. Roundtrip all nine settings including explicit disabled/0 chance. Preserve unrelated existing keys/behavior. The inspected `ToJson` starts with `{` followed by `,"v":2`; no first-member comma removal was found. Verify this against the current source and make the minimal first-member formatting correction if it is still present. Validate emitted output using a standard JSON parser as well as the project's permissive `ExtractValue` reader; the latter can conceal malformed JSON.

Visible widgets on Defense page: toggle Enabled, integer percentage 0–100 step1, toggle GM auto, dropdown Auto/Full/Compact. Add brief hints: “Applies after this AO's required objectives; successful placement uses the captured base for defense” and “0 disables automatic selection.” Advanced settings are supported in config transport/persistence, not mandatory widgets in v1.

## 11. HUD, tasks, replication and notifications

Use one objective task at a time on the transient host:

- Seize: “Seize the enemy operating base” / “Clear and secure the command area. Counterattack preparations begin after capture.”
- Regroup/Warning: “Regroup at the captured base” / “Assemble and prepare for the counterattack.” Update wording/status without recreating the task every tick.
- Defend: existing `CreateDefendTask` after dismissing/completing the regroup task intentionally; same assembly point.

Create one static map marker following `IA_DefendEvent.SpawnMapMarker` and retain/remove it through its actual manager API. Do not depend on client-only runtime `IA_AreaMarker` discovery.

Add one replicated packed status property on initializer, distinct from the configuration string. Suggested `|` format with stable field order:

```text
version|serial|groupId|phase|siteId|x|y|z|captureX|captureY|captureZ|captureRadius|capturePermille|eligiblePresent|target|allPresent|remainingSec|reasonCode
```

Keep `siteId` delimiter-safe (generated numeric ID string); use reason enums rather than arbitrary free-text strings in transport. Validate field count/version before replacing client state. Publish only changes; maximum once per second except immediate phase transitions. Provide initializer getter/parser wrappers so HUD code does not duplicate field indexes. Remaining seconds is server-authoritative; local interpolation is optional and never starts a transition.

Add one tile to `IA_ObjectiveHudStrip` using existing MUI patterns. Show the terminal objective tile globally during Seize/Regroup/Warning, including to travelling players, with name/distance and the current action. Capture progress is only shown as active capture when applicable. Regroup shows eligible present/target and remaining wait or “Awaiting friendly forces”; Warning shows the countdown; contested command zone shows “Clear the command area.” When Defend starts, hide this tile and let the existing defend HUD own the presentation. Failed status stays visible with “Objective unavailable — admin action needed.”

Avoid duplicate existing capture tiles: the new evaluator does not publish a second ordinary capture slot. Reuse drawing/contribution helpers, not two independent HUD publishing mechanisms. Joining clients derive everything from the replicated status and task/map state; do not replay old completion toasts.

For site props, verify actual RplComponent/inheritance behavior. Server spawning alone does not establish that a plain GenericEntity hierarchy appears correctly for late joiners. If runtime module roots/children need an IA replication wrapper, author it and verify transform/destruction on two clients. Do not attempt to replicate server object references in status strings.

## 12. Admin commands and cleanup

Keep existing commands recognizable:

| Command | Required behavior with a base phase active |
| --- | --- |
| Complete AO | Cancel new director/job, abort active base defense without success callback, defer site cleanup, then execute existing forced AO completion exactly once. |
| Complete + Defend | During Placing retain original hosts until cancelled. During Seize/Regroup/Warning prefer transitioning the existing live base directly into defense with explicit admin bypass of capture/regroup; label/log the bypass. During Defend do nothing beyond reporting already active. If no base is active, retain current authored-defense behavior. |
| Complete objectives + seize base (new) | Explicitly retires remaining required objectives, bypasses enabled/chance/GM-auto checks for the current AO, and starts one validated base attempt. Preserve optional mortar semantics. Geometry, faction, resource and AI budget checks still apply. If pre-reveal placement fails, attempt forced authored defense if a live suitable authored host/marker remains; otherwise complete the AO once and report placement failure. |
| New AO / reset / shutdown | Cancel current objective and pending jobs; no victory callback for the cancelled AO. |

The Complete + Defend behavior above resolves the overview's open choice for an already-active base and avoids targeting ordinary hosts already retired at commit. All new requests use the same server-side admin validation pattern as the existing force commands. Do not authorize based on a hidden button or a client-supplied admin boolean.

Cleanup sequence:

1. Set terminal/cancel flag and invalidate pending job token before external callbacks.
2. Stop placer batches, garrison jobs, warning events and owned QRF delayed work.
3. Detach/abort active defense when cancelling; never force-finish its host first.
4. Remove task/marker/HUD references intentionally, with Failed retaining its diagnostic status.
5. ForceFinish the transient host after defense is detached, preserving existing proximity-aware AI/vehicle cleanup. Remove it from transient updates once shut down; retain a strong owner while deferred cleanup is pending.
6. Keep scenery roots until no player is within 600 m of the base footprint and no player is occupying a site-owned vehicle. Use distance to footprint, not center. Check every 8 seconds through the retired-site manager, including after another AO starts.
7. Snapshot navmesh rebuild bounds before removing roots. Queue/delete roots using the established replicated deletion pattern verified for these prefabs; rebuild saved bounds after their actual removal. Remove each root once; do not also independently queue every child.
8. Clear strong references only after owned entities/reservations/callbacks are gone. On world shutdown, release callbacks/references using safe game/world null checks; no attempt to advance AO.

If players indefinitely occupy an old base, keep it deferred. Bound accumulation by suppressing new base placement when two retired scenery sites are still retained; use ordinary fallback and one admin diagnostic. Do not force-delete an occupied site to satisfy the limit.

## 13. Ordered work packages

Implement serially; keep every package reviewable. Use the existing repository's compile workflow. Do not finish after adding empty classes or a settings toggle.

1. **Configuration and contracts.** Add enums/settings/snapshot, pack/unpack and persistence including replication. Add config roundtrip/debug checks. Compile. Done when old payloads preserve new defaults and a second client's menu shows a saved 37% value.
2. **Manual full-layout prototype.** Build audited IA module prefabs and manifest; instantiate at a known valid test origin through a developer-only entry point. Record bounds/counts and fix all door/lane collisions. Compile/load. Done when the full base looks organized and two actual AI paths from different entries reach HQ.
3. **Transient host and ownership.** Add dynamic-host creation policy and cleanup token/site storage. Spawn only the finite garrison. Done when a host can exist for five minutes under attack without ordinary replenishment/civilians/vehicles appearing and can clean up without calling AO success.
4. **Seize/regroup/status.** Add sampler/roster, state machine, task/map marker and replicated tile using a manually valid site. Done when all time/empty/contested/late-join cases below behave as specified.
5. **Prepared defense and cancellation.** Add explicit-host factory, successful callback routing and Abort. Integrate initial beat/event delay. Done when legacy and enhanced defense use the same live site, count surviving garrison, and advance the AO exactly once.
6. **Runtime placement.** Add candidate generation, full footprint/visibility/geometry/nav sanity and rollback. Add compact manifest/modules. Done when candidate failures have bounded work and clean fallback, with no objects left unowned.
7. **Natural/GM/admin sequencing.** Wire AO activation and completion entry points, automatic pressure gates and force commands. Done when existing authored-only missions still function with the feature disabled and GM reactivation cannot accept stale jobs.
8. **Dedicated-server playtest and tuning.** Test multiple clients and representative AOs/terrain, capture logs and measured timing/entity cost. Fix failures; do not replace failed tests with optimistic comments. Update overview with any deliberate tuned values and record release validation.

During development default off in any temporary test mission if needed; the final baseline feature setting is enabled/100%. Do not change unrelated installed-server profile settings from tooling while implementing repository files.

## 14. Acceptance matrix

For each case record setup, observed outcome and whether it was tested in Workbench, a dedicated server, or only a deterministic helper check. Untested is not pass.

| ID | Setup | Expected outcome |
| --- | --- | --- |
| SEL-01 | Disabled or 0%; required objectives complete. | No base job or new HUD; authored 80% defense/normal completion unchanged. |
| SEL-02 | 100%, legal site, no authored defend marker. | Exactly one Seize→Regroup→Defend chain. No authored-marker requirement. |
| SEL-03 | 37%; deterministic random values just below/at/above threshold. | One defined `< chance/100` decision; 0 never and 100 always select. |
| SEL-04 | Completion callback invoked repeatedly while Placing. | One roll, one job, no AO advancement. |
| SEL-05 | No legal full or compact location / 45 sec timeout. | Pre-reveal rollback, one fallback decision, no stuck Placing or orphan props. |
| SEL-06 | Same GM group number activated again; old callback returns. | Old serial ignored and its content cleaned, new AO unaffected. |
| GEO-01 | Four cardinal headings on flat site. | Same relative module spacing and inward-facing doors; no world-axis offset bug. |
| GEO-02 | Flat center, steep corner / water under one module / tree in lane. | Candidate rejected, not silently relaxed. |
| GEO-03 | Center fits eligible union but outer corner crosses gap. | Candidate rejected. |
| GEO-04 | Player >=250 m from center but <250 m from footprint edge. | Candidate rejected. |
| GEO-05 | Distant player/aircraft observes tall part of base. | Visibility sample rejects; no task revealed with unsafe spawn. |
| GEO-06 | Players approach during staged spawn. | Abort/rollback without deleting content underneath them; fallback once. |
| GEO-07 | Missing required prefab / expanded entity ceiling exceeded. | Complete owned rollback; no partial announced base. |
| AI-01 | Group object created but staggered soldier spawns are pending. | No task reveal until the full target is ready; cancellation stops all remaining spawns. |
| NAV-01 | Two open exterior entrances to HQ, actual enemy squads ordered in. | Both squads reach command region without teleporting or getting permanently stuck. |
| NAV-02 | Runtime obstacle creates disconnected lane. | Validation rejects or post-spawn validation rolls back before reveal. |
| CAP-01 | One living player, empty command area, 90 valid seconds. | One capture award; Regroup begins. Ordinary 1.3 rate not applied. |
| CAP-02 | Hostile enters at 45 sec, leaves later. | Pause at 45; resume; no full-base kill requirement. |
| CAP-03 | Corpse/unconscious enemy in zone; live enemy in remote tower. | Neither blocks command capture; conscious enemy in command zone does. |
| CAP-04 | Helicopter flyover/no grounded friendly. | Capture and arrival do not progress. |
| REG-01 | 10 eligible, 6 present by 90 sec. | Warning at 90, defense at 120 at earliest. |
| REG-02 | 10 eligible, 2 present throughout. | Warning at 240, defense at 270; no indefinite unanimity wait. |
| REG-03 | Zero present after capture or everyone leaves warning. | No empty defense; fresh full warning on qualified return. |
| REG-04 | Disconnect/casualty/recovery/late join during regroup. | Roster rules honored; no denominator explosion or immediate casualty shortcut. |
| REG-05 | Command zone contested during Warning. | Warning cancelled, clear-area status, completed capture not awarded again. |
| DEF-01 | Prepared enhanced handoff. | Clock/first beat start once after warning; no second Prepare; off-site event >=120 sec. |
| DEF-02 | Prepared legacy handoff. | Legacy starts only after warning and uses explicit base host. |
| DEF-03 | ForceFinish host / admin cancel during defense. | No accidental successful callback or duplicate AO advance. |
| DEF-04 | Surviving garrison + pending wave units near cap. | New waves account for both; no duplicated initial garrison. |
| ADM-01 | Old JSON / shorter packed payload. | New baseline remains intact; no default-to-false regression. |
| ADM-02 | Save 37%, disabled flag, Compact; restart/open another client. | Exact values preserved; current selected objective unaffected; emitted profile passes a standard JSON parser. |
| ADM-03 | Non-admin sends new force/settings request. | Server rejects using existing authorization rules. |
| ADM-04 | Complete AO / Complete+Defend in every active phase. | Defined action executes once, pending work cancelled, no dead host selected. |
| RPL-01 | Join during Seize, Regroup, Warning and Defend. | Correct props, task, map marker, phase/countdown and no replayed old awards. |
| GC-01 | Players remain after success; next AO starts. | Site remains while occupied; removes safely later, rebuilds navigation. |
| GC-02 | Two retired occupied sites retained. | Next automatic base selection falls back; occupied scenery stays. |
| PERF-01 | Full layout plus scaled garrison on dedicated server. | Record entity expansion, placement callbacks/frame cost, active AI and total added AO time; tune against measured server capacity. |

## 15. Handoff checklist and reporting

- Maintain a short implementation log beside this runbook recording completed packages, changed contracts and current engine-validation blockers.
- Replace ambiguous TODOs with either working behavior or a named deferred scope item. Core seize/regroup/defense/rollback/JIP/cleanup are required; resupply service and additional objective types are optional follow-up scope.
- Before declaring complete: compile all changed Enforce files, resolve every new resource reference, verify module inheritance, execute relevant acceptance cases, inspect working-tree diff and preserve existing user changes.
- Final implementation report must state what works, what ran in-engine, what remains untested and any tuned values. If Workbench is unavailable, finish all supported code/resource work, identify the exact remaining validation steps, and do not label the feature game-tested.
- The minimum player-visible completion condition is an organized enemy base that is safely placed after required objectives, can be seized, provides a bounded regroup period, and transitions to a single defense at that same base without breaking AO progression.
