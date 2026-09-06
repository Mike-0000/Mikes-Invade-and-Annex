# Runtime logging

I&A runtime diagnostics are **off by default**, including in Workbench.
No prefab/config migration is needed. Gameplay, notifications, and statistics
submission still run normally; only diagnostic output is gated.

## Operator controls

- Normal production: launch without `-iaDebug`, or with `-iaDebug 0`.
- Troubleshooting: append **`-iaDebug 1`** to the affected process's launch arguments.
- The switch is local to each server/client/Workbench process, not replicated and
  not changeable through a client RPC. It is read once; restart after changing it.
- Opt-in emits `[IA][Log] Runtime diagnostics enabled (-iaDebug 1).` on the first
  diagnostic check. Traces use `LogLevel.NORMAL` once enabled, so an additional
  engine DEBUG-level switch is not needed.
- Remove the switch when troubleshooting is complete; verbose output can be large.

Production retains actionable warnings/errors, security rejections, configuration
load/save and admin-operation records, and infrequent objective/placement milestones.
Repeated civilian checks, QRF/artillery eligibility checks, spawn/waypoint traces,
AI scaling, inbound pins, role lifecycle chatter, leaderboard updates, and successful
API polling/batches are debug-only. API bodies and raw statistics payloads are not
logged even in debug mode.

Missing side-objective markers warn once per absence episode. Polling continues,
so dynamically added GM markers still work. Finding markers resets the warning latch.

This only controls this addon's runtime logging. Engine/vanilla/other-addon warnings
are not globally filtered. Explicit Workbench tests and their Game-module test
helpers retain their pass/fail output.

## Implementation policy

`Scripts/Game/IA_Log.c` caches the opt-in. Guard at the **call site**, before string
formatting or debug-only scans, rather than passing an eagerly formatted string to
a disabled logger:

```c
if (IA_Log.IsDebugEnabled())
{
    Print(string.Format("[IA][Subsystem] State=%1", state), LogLevel.NORMAL);
}
```

Use `IA_Log.Info("[IA][Subsystem] ...")` for deliberate, infrequent production
milestones/audit. Warnings/errors/fatal diagnostics stay as direct `Print` calls
outside the gate. Expected eligibility skips and successful recovery traces are
not warnings. Existing legacy trace prefixes remain searchable.

Checks:

```text
python tools/check_runtime_logging.py
python -m unittest discover -s tools -p "test_runtime_logging.py"
```

The checker scans active calls (ignoring comments/strings), enforces debug-block
scope, and rejects warnings/errors hidden behind the debug gate. These are static
checks, not an Enforce compiler or runtime simulation.

## September 2026 cleanup evidence

Reviewed recent Workbench `console.log` sessions, particularly
`logs_2026-09-05_00-45-04`, `logs_2026-09-04_23-10-53`, and
`logs_2026-09-02_17-32-21`.

- September 5 gameplay session: 21,069 SCRIPT lines; over 5,600 civilian-status
  checks, 625 staggered-spawn completions, and 525 each of civilian-revolt totals,
  civilian-kill percentages, and periodic area-group checks.
- September 4 session: 899 missing-side-objective-marker warnings, 462 inbound
  pin traces, and 385 staggered-spawn completions.
- The September 5 17:56 startup-only session contained no SCRIPT messages.

The cleanup gates 292 routine trace sites (including one debug-only group census),
reclassifies ten expected-state warnings as traces, and keeps 44 existing normal
records as explicit production audit/milestones. These are source counts, not a
measurement of a new gameplay run.

## Workbench/server smoke test (required before promotion)

1. Compile Game and WorkbenchGame scripts with the addon and dependencies loaded.
2. Run normally without the switch. Let an AO spawn and tick through civilian,
   QRF/artillery, reinforcement, and leaderboard updates. Verify routine traces
   are absent while gameplay and HUD notifications still work.
3. In a world without side-objective markers, enable automatic side objectives
   and wait through startup. Verify one missing-marker warning, not one per tick.
   Add a marker and verify automatic objectives resume; a later absence may warn
   once again.
4. Restart with `-iaDebug 1`. Verify the banner and routine traces appear. Compare
   behavior with step 2; logging must not control spawning, callbacks, or state.
5. Exercise an admin config save and a controlled invalid-resource/config path.
   Verify audit and warning/error records remain visible in normal mode.
6. Restart without the switch before publishing. No promotion/push is performed by
   this cleanup.
