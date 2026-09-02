# Player Experience Improvements

Reviewed against experimental I&A (`F:\Mikes-Invade-and-Annex-Exp`). The first draft mixed live player-facing bugs with engine-handbook notes that are already shipped, and it described HUDs that do not exist.

Do the two remaining items. Do not reopen the dropped sections.

---

## Do this

### 1. HVT escape has almost no clock for players

**Where:** `IA_AssassinationObjective` in `IA_SideObjective.c`. Only side-objective type today.

**What actually happens**

1. First contact (`AreaInstance.IsUnderAttack()`) starts a silent server timer **and** defense waves. The only toast is `Reinforcements Inbound!` (plus a generator hint if one is still up).
2. That timer is `Math.RandomInt(5, 16)` minutes. Max is exclusive, so the range is **5–15**, not 5–16. Nothing on the HUD shows it.
3. The starting task is **Assassinate HVT** at the **side-objective marker** (`m_Position`), not the HVT pawn. Dedicated `IA_HVTSpawnPositionMarker` sites can put the target somewhere else. The task does not follow the HVT.
4. The escape point is rolled only at **T−60s** (`_GenerateEscapePoint`: road 500–750 m out, else ground). Then a second task and `The HVT is preparing to make their escape!` appear.
5. If the HVT reaches that point they need **60 s** on site to extract (`m_iHVTEscapeTimeEnd`). The log line still says `30s`. Toast: `HVT has reached the extraction point!` — still no timer.

So the long wait is invisible, the map pin is late, and the 60 s extract is also invisible. That is the real time-pressure problem.

**Do not**

- Treat this as “task points at the HVT.” It points at the marker.
- Put the clock only on `IA_NotificationToast`. Toasts expire; defend already has a persistent bar on `IA_ObjectiveHudStrip`.
- Reveal the extract pin at first contact. That gives 5–15 minutes to camp a road 500 m away.

**Action**

1. Persistent HUD timer on the objective strip (same family as `IA_DefendHud`), armed when the escape countdown starts. Show remaining time. After they reach the pin, switch the label to the 60 s extract.
2. At first contact, tell players the HVT will run, and for how long — toast plus the new timer. Keep the extract pin at T−60s.
3. Replace the 5–15 roll with a fixed window (8–10 minutes is fine). `RandomInt` max stays exclusive.
4. Fix the `30s` log to match the 60 s extract, or change the extract to 30 s and say so on the HUD.

---

### 2. A completed AO can skip its defend site

**Where:** `IA_MissionInitializer.CheckAndStartDefendMission`.

**What actually happens**

When a zone group finishes, a `DefendObjective` marker in that group is required. If one exists, a **20%** roll still skips the hold (`Math.RandomFloat01() > 0.8`) unless GM/admin `force` is set. Players then go straight to the next capture with no explanation.

That skip is the inconsistency, not Enhanced Defense pacing.

Enhanced is the default (`IA_Config.UseLegacyDefense()` false). On start it already toasts the doctrine and `PREPARE: …`, then the defend HUD shows **PREPARE / PROBE / ASSAULT / CRISIS / SECURE**, remaining **M:SS**, and a pressure **rail**. A 30 s “warning before defense begins” is the wrong layer — PREPARE *is* that warning.

Random lulls/surges live only on the **legacy** 12–16 min path (`IA_DefendMission.RollPressureProfile`). Do not flatten Enhanced events to “fix randomness.” The admin toggle (`Use legacy defense`) is for hosts, not a missing player legend.

**Action**

1. If a defend marker exists, always start the hold (or expose the chance as an admin percent defaulting to 100).
2. If a skip remains, toast that the AO is complete and the next sector is opening so it does not feel like a missed defend.
3. Leave Enhanced doctrine, PREPARE, and the existing HUD alone.

---

## Already shipped — do not reopen

### Elite patrols and typed waypoints

`IA_ObjectiveElitePatrol` already calls `StartSweepPatrol` (six walkable legs). `EvaluateTacticalState` re-issues `IssueNextSweepPoint` when `HasActiveWaypoint()` is false. After one lap they assault the AO (`SetDefendMode`), they do not idle on a completed Patrol WP.

Occupying infantry use `DefendPatrol` plus vanilla `E_AIWaypoint_Patrol` (a one-shot 5 m Move). `IA_AreaInstance` hands them a new random point when the WP is gone. That is a wander, not a looping route, and they are not stuck after the first point.

Typed-tree mismatches (`ActivityDefend.bt` needs `SCR_DefendWaypoint`, `GetInNearestVehicle.bt` needs `SCR_BoardingTimedWaypoint`) are already deferred in `IA_AiGroup.ShouldDeferOrderForTypedTree`. Do not “chain Patrol waypoints” inside `IssueNextSweepPoint` — that path is already sequential Move orders.

The 2026-08-31 elite-idle note was the old single Patrol WP. Sweep patrol replaced it.

### Rank / toast localization and list measure

`IA_RankHudPanel` already goes through `MUI_TextUtil.SetLiteral`, which stamps `WidgetFlags.NO_LOCALIZATION` then `SetText`. `IA_NotificationToast` is MUI paint (`DrawText`), not `TextWidget.SetText`.

This snippet is wrong and must not be copied:

```
Widget.SetText(string.Format("%1", place));
Widget.SetFlags(WidgetFlags.NO_LOCALIZATION);
```

Flag after `SetText` is too late. `NO_LOCALIZATION` plus `SetTextFormat("%1", …)` paints the literal `%1`. Use `MUI_TextUtil.SetLiteral`.

`GetTextSize` / `GetNumLines` not shrinking was the GM Director live/staging lists, not the rank HUD or toasts.

### Defend HUD copy

`IA_DefendHud` never shows `0.73 pressure` or raw phase ints. Pressure is a rail. Phase is the tab string. Remaining time is `M:SS`. `GetHudPressure01` / `IA_DefendHudPhase` are replication internals.

---

## Draft errors (for the next pass)

| Draft claim | Reality |
|---|---|
| Elite patrols idle after one vanilla Patrol WP | They sweep, then assault |
| Chain WPs in `IssueNextSweepPoint` | Already re-issues Move legs; vanilla Patrol is not a loop |
| Fix Defend / GetInNearest class mismatches | Already gated in `IA_AiGroup` |
| Task points at the HVT | Task is the side-objective marker; HVT can be elsewhere |
| Escape is 5–16 minutes | `RandomInt(5, 16)` → 5–15 |
| HUD shows `pressure01` and phase 0–4 | Defend HUD already labels phases and draws a rail |
| Stamp `NO_LOCALIZATION` after `SetTextFormat` | Wrong API; rank HUD already uses `SetLiteral` |
| 30 s pre-defend warning; flatten pressure | Enhanced already PREPARE + HUD; lulls are legacy-only |
| `IA_NotificationToast` localization spam | Custom compositor, not `TextWidget` |
