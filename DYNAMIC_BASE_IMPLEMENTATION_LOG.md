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
| 7 Natural / GM / admin sequencing | Code complete | AO serial, `TryBeginTerminalObjective`, fallback helper, ordinary-host retire on commit, Complete AO / Complete+Defend / Complete objectives+seize base, QRF/side/arty pressure gates. |
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

## Tuned values

None. Defaults remain enabled / 100% / Auto / 90 / 90 / 240 / 0.60 / 1.0.
