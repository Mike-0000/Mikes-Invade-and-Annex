# Dynamic-base garrison behavior

## Whole-base defense

Each occupying group keeps its own indefinite stock `DefendSmall` waypoint, but all waypoints now share the site's centre and a radius equal to the footprint's half-diagonal. The prefab's default 15 m is overridden. Full/Compact/Courtyard/Roadside/CommandPost/RallyPost radii are approximately 114/78/58/57/43/30 m.

Soldiers still spawn at the separate, validated authored guard posts. The shared area lets vanilla Defend choose positions and cover throughout the base rather than confining each group to its spawn point. It retains the Defending/CoverPost preset. This is not a continuous patrol route: vanilla defenders may settle in cover between moves and combat reactions.

Defend is circular, so covering every corner of a rectangular footprint also includes some ground outside the walls. This is intentionally a soft base assignment, not a hard wall boundary. Vanilla group autonomous-position checks use the current Defend waypoint's radius (`SCR_AIGroupUtilityComponent.IsPositionAllowed`). The pinned I&A lifecycle prevents unrelated AO/patrol/assault retasking; it does not disable vanilla combat reactions.

## Priority defect

Dynamic defenders now use waypoint priority level **0**, independently of ordinary I&A Defend orders. Level 20 was forwarded by the vanilla group activity to soldiers, making Defend evaluate to **81** (61 + 20). That beat normal attack initiation (**70**) and incoming-fire observation (**69**). Exceptional high-threat attacks could still win, explaining why this could appear as rare rather than completely absent return fire.

The previous per-post radius clipping also produced a 0.599803 m radius and several 2–4 m circles in `logs_2026-09-05_00-45-04/script.log` at 21:48–21:58. Perimeter clipping remains a spawn validity check; it no longer determines the area that the group defends.

## Verification

`IA_BaseGarrisonTest` runs through Workbench ResourceManager (`-wbModule=ResourceManager -plugin=IA_BaseGarrisonTest -run`). It exercises native `AIActionBase.Evaluate()` and checks whole-base coverage, shared centres, outside-spawn rejection, every authored/fallback guard post, and 24 headings for all six layouts.

2026-09-05: the native test logged `oldDefend=81 newDefend=61 attack=70 observe=69` and completed with zero failures. The existing selection regression and six-layout source checks also passed. Script validation succeeded for WORKBENCH, PC, XBOX, PS4 and PS5. These checks prove the priority arithmetic and area configuration, **not** live movement or firing.

Live acceptance remains necessary: create a fresh base after loading the updated scripts; allow defenders time to spread, fire unsuppressed from an enemy faction, then check turning, cover movement and return fire. Test multiple approach directions and entrances, and verify defenders resume base defense when contact ends. Existing spawned waypoints are not migrated by this change. If a defender still stalls, inspect its active behavior, target/perception state and local navmesh rather than raising waypoint priority.
