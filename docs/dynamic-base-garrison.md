# Dynamic-base garrison behavior

## Shared base defense

Each occupying group keeps its own indefinite stock `DefendSmall` waypoint, but all waypoints share the site's centre. The radius is **85% of the footprint's half-diagonal**, controlled by `IA_BaseGarrisonArea.DEFEND_RADIUS_SCALE`. This trims the previous radius by 15% (about 28% less circle area) to reduce defenders wandering outside the perimeter. The prefab's default 15 m is overridden. Full/Compact/Courtyard/Roadside/CommandPost/RallyPost radii are approximately 97/66/49/48/36/25 m, down from 114/78/58/57/43/30 m.

Soldiers still spawn at the separate, validated authored guard posts. The shared area lets vanilla Defend choose positions and cover throughout the base rather than confining each group to its spawn point. It retains the Defending/CoverPost preset. This is not a continuous patrol route: vanilla defenders may settle in cover between moves and combat reactions.

Defend is circular, so the smaller area deliberately sacrifices corner coverage and can still include some ground outside the walls. Authored edge posts remain valid spawn positions even when outside the smaller circle; their groups receive the same central defense assignment. This is intentionally a soft base assignment, not a hard wall boundary. Vanilla group autonomous-position checks use the current Defend waypoint's radius (`SCR_AIGroupUtilityComponent.IsPositionAllowed`). The pinned I&A lifecycle prevents unrelated AO/patrol/assault retasking; it does not disable vanilla combat reactions.

## Priority defect

Dynamic defenders now use waypoint priority level **0**, independently of ordinary I&A Defend orders. Level 20 was forwarded by the vanilla group activity to soldiers, making Defend evaluate to **81** (61 + 20). That beat normal attack initiation (**70**) and incoming-fire observation (**69**). Exceptional high-threat attacks could still win, explaining why this could appear as rare rather than completely absent return fire.

The previous per-post radius clipping also produced a 0.599803 m radius and several 2–4 m circles in `logs_2026-09-05_00-45-04/script.log` at 21:48–21:58. Perimeter clipping remains a spawn validity check; it no longer determines the area that the group defends.

## Verification

`IA_BaseGarrisonTest` runs through Workbench ResourceManager (`-wbModule=ResourceManager -plugin=IA_BaseGarrisonTest -run`). It exercises native `AIActionBase.Evaluate()` and checks the 15% radius reduction, shared centres, outside-spawn rejection, every authored/fallback guard post, and 24 headings for all six layouts.

Before the radius reduction, on 2026-09-05, the native test logged `oldDefend=81 newDefend=61 attack=70 observe=69` and completed with zero failures. The existing selection regression and six-layout source checks also passed. Script validation succeeded for WORKBENCH, PC, XBOX, PS4 and PS5. These checks prove the priority arithmetic and area configuration, **not** live movement or firing.

The 15% reduction still needs Workbench/native and live validation. Create a fresh base after loading the updated scripts; allow defenders time to spread and compare outside-perimeter movement, especially along the shorter sides. Fire unsuppressed from an enemy faction, then check turning, cover movement and return fire. Test multiple approach directions and entrances, and verify defenders resume base defense when contact ends. Existing spawned waypoints are not migrated by this change. If a defender still stalls, inspect its active behavior, target/perception state and local navmesh rather than raising waypoint priority.

## Air-raid cover (2026-09-27)

Modded attack helicopters could wipe out a Seize garrison before the assaulting infantry arrived. This change makes the existing defenders survive the raid. It adds no reinforcements, extra units or AA.

**Bunkers.** Every generated base places optional camouflaged sandbag bunkers inside its capture circle: 6 on Full, 4 on Compact, 3 on Courtyard and 2 on each smaller size. All 120 composition recipes and all 72 headquarters recipes carry their full count. Each is the stock roofed, net-covered bunker with three `CoverPost` smart actions. Placement and budgets are in [Air-raid bunkers](dynamic-base-compositions.md#air-raid-bunkers-2026-09-27). The legacy Workbench layouts carry bunkers too ([Camouflaged bunkers](dynamic-base-layouts.md#camouflaged-bunkers-2026-09-27)).

**Drill.** `IA_BaseAirRaidCover` runs only during Seize. Every 2 s it looks for an aircraft within 1.5 km of the base that is more than 8 m above ground and has a living, conscious pilot hostile to the garrison. Aircraft are recognised by their AI vehicle usage type, or by a `HelicopterControllerComponent` for modded aircraft that lack one. On alert, each intact bunker takes the nearest free living group within 70 m, closest pair first, one group per bunker. A group is free only while it holds a Defend post; gun and mortar crews and cached or paused groups are never sent.

The group's existing Defend waypoint moves onto the bunker with a 6 m radius (`IA_AiGroup.RepinDefendPost`). No waypoint is removed or added, and the priority stays at level 0. The repin then fires the waypoint's properties-changed event. From the vanilla source, that restarts the defend loop's cover search at the new post instead of waiting out its 150–180 s timeout; a live run has not confirmed it yet. Vanilla never releases a group's smart-action allocation, so the repin releases the group's own posts, and idle posts on that bunker held by other garrison groups are handed back first.

**Losses and the all-clear.** A bunker whose group dies is not refilled during that raid, so a second group is not sent into the same gun run. If a bunker is destroyed, its group returns to its home post. After 45 s with no qualifying aircraft, every sheltering group returns to its original post and radius. Leaving Seize for any reason (capture, a lost site or cancellation) does the same. Defend waves are unchanged.

**Combat.** Sheltering groups keep priority level 0, so attack and incoming-fire reactions still interrupt Defend when infantry arrives. Soldiers still target aircraft, but group perception ignores them, which is why the response is a scripted drill. Capture rules are unchanged. Every bunker lies inside the capture circle, so a sheltered defender still contests capture.

**Alternatives rejected.**

- A roof over the whole base looks wrong, needs new navmesh and construction work for every layout, and makes the helicopter pointless.
- More AA destroys the helicopter instead of letting defenders survive it, and does nothing once the AA is killed.
- Reinforcement waves were ruled out by the brief.
- A higher Defend priority starves infantry combat (see [Priority defect](#priority-defect)).

**Limits.**

- Each bunker has three cover posts, so at most 18 defenders are in bunker posts on Full, 12 on Compact, 9 on Courtyard and 6 on each smaller size. The rest of a sheltering group takes radial cover within 6 m, and other groups keep their normal Defend behavior.
- Soldiers may still leave cover to fight.
- The net's effect on visibility and the bunker's protection against rockets and cannon are unmeasured. Bunkers are destructible.
- Any hostile crewed aircraft triggers the drill, including transports.
- Bunkers are optional, so construction skips one that fails its ground support or clearance check.
- Bases built before this change have no bunkers.

**Verification.**

- `tools/test_base_compositions.py` and `tools/test_headquarters.py` check every recipe's bunker count, capture-circle reach (centre plus the 1.7 m cover-post reach, 1 m inside), clearance from compositions, doors, the approach lane, gun crews, gate throats, entries and guard posts, and outward facing. `author_base_designs.py --check` and `author_headquarters.py designs --check` reproduce the committed outputs.
- `tools/check_dynamic_base_layouts.py` passed the six legacy layouts, and `tools/validate_dynamic_base_assets.py` passed. `tools/check_runtime_logging.py` flags only the existing `IA_AreaGroupManager.c:138`.
- `IA_BaseGarrisonTest` gained closest-pair assignment checks and a bunker footprint, overhead-cover and cover-post check. It was **not run**, and script compilation is unverified: the Workbench CLI waits at `SteamAPI_Init` when Steam is not running.

Live acceptance: start a fresh Seize base and fly a hostile attack helicopter within 1.5 km of it, above 8 m. Confirm the `[IA][Base] Air raid` log line, groups moving into the bunkers, and the all-clear 45 s after the aircraft leaves. Then confirm the ground assault still meets defenders who fight back.
