# Dynamic AI Spawning

The first implementation is an optional, server-authoritative cache for untouched stationary infantry garrisons. It saves each soldier's prefab and exact world transform, removes distant character entities, and restores the team when needed. It does not change the intended enemy roster or increase reinforcement quotas. Live multiplayer behavior and performance savings remain unverified.

## Enable or disable

Open the admin configuration menu, select **Scaling**, and use **Dynamic AI Spawning**. The setting defaults to **off**. With it off and no previously cached soldiers, spawning follows the existing path.

Turning the setting off requests restoration of every cached team. Restoration finishes through the shared work queue; it is not instantaneous. Pending soldiers continue to count toward their area's logical strength during this drain, and capture readiness remains protected. Enabling it again does not make a restored team eligible for another cache cycle.

## What happens

1. Areas, groups and individual soldiers spawn through the existing phased creation paths. Initial spawning, replication and movement still cost server time.
2. A supported stationary team becomes a candidate once it has an area owner. Caching cannot begin before 30 seconds have passed since its cache registration. Its normal placement must also finish; building garrisons must complete their existing arrival checks. If readiness is still missing after 180 seconds, the team stays live.
3. Every soldier must be more than **1500 m horizontally from every sampled player** when considered. A player entering that region, recorded danger/contact, an injured or missing member, or another unsupported state keeps that team live. One team at most enters the cache per one-second scan.
4. Each soldier's actual prefab and full world transform are saved, including height and orientation. The original native group and addon ownership remain; its character entities are removed. Cached survivors still contribute to logical alive counts.
5. Any saved soldier within **1000 m horizontally of any sampled player** requests the whole team's restoration. Capture readiness can also request relevant cached defenders, and turning the setting off requests all cached teams.
6. A wake request stays latched. Restoration is attempted every 100 ms through a shared budget of at most four attempts and approximately 4 ms of work per tick, checked between attempts. Failed resource/spawn/attachment work retains the soldier's slot and retries after one second. There is no population-cap or line-of-sight veto that cancels a requested wake.
7. Successfully restored soldiers can respond while the rest of their team is queued. After restoration, the team stays live until normal death or objective cleanup. A casualty during restoration is recorded as dead and is not spawned again. A recreated pawn taken over by a player is released from pending restoration without regrouping or duplicating it. Area shutdown retires cached records so delayed work cannot revive a completed objective.

The 30-second threshold is measured from registration, not a fresh 30-second timer whenever the toggle changes. Distances are prototype constants in `Scripts/Game/IA_DynamicAISpawning.c`; they are not admin-adjustable in this version.

Town capture retains physical faction counts. When live friendlies could capture, cached defenders in the capture sphere request restoration and progress/scoring pause until a subsequent census is ready. Base seize follows the same principle before awarding time. Scanning an empty distant zone does not itself wake its garrison.

## Supported scope and limits

Candidates are groups assigned to supported stationary hold posts, including qualifying building and base garrisons. Their full original membership must be present, alive, conscious and undamaged. Groups with recorded contact, player-controlled soldiers or forced permanent AI LOD remain live. Eligibility is conservative; an unsupported team stays on its established behavior.

Vehicles and their crews/passengers, static-gun and mortar crews, airborne forces, moving patrols, elite profiles, HVT/objective units, active defense waves, and groups still moving under an explicit simulation pin are excluded. This version does not simulate dormant patrol movement or combat between absent soldiers.

The snapshot is **not a full inventory or character serializer**. It restores the saved character prefab and pose, with the existing combat-profile setup. Randomized appearance or equipment inside that prefab can be initialized again; magazine contents, changed inventory, wounds, animation state and behavior-tree execution are not serialized. The restrictions above are necessary, and custom loadout fidelity needs a separate test.

Distance provides preparation time but does not prove invisibility. This version has no visibility-aware exposed-post filter, aircraft prediction, remote-camera protection or concealed-placement fallback. Long-range optics, helicopters, HALO and teleports can expose a saved position before restoration finishes. An already-requested team still restores at its saved positions, even if those positions are now visible. Changed geometry or objects at those positions can also affect the resulting placement. Keeping the original world transform is not a guarantee that physics will leave the recreated soldier there.

Dormant character entities cannot be hit by ordinary bullets or explosions. There is no pre-impact artillery wake integration in this version. Keeping mortar crews live does not make dormant targets vulnerable. Long-range reconnaissance and indirect fire are therefore explicit limits of this prototype, not solved cases.

Caching pays the cost of creating and deleting soldiers before creating them again later. Native AI LOD already reduces some distant work. Measure a complete objective cycle before increasing nearby AI density: scouting can progressively restore most teams, and split squads can require many teams to remain live together.

## Validation so far

Workbench's native `IA_DynamicAISpawningConfigTest` completed with `failures=0`: six configuration compatibility checks and five pending/casualty/position-ledger assertions. Script validation succeeded for WORKBENCH, PC, XBOX, PS4 and PS5 without compiler errors; existing/default-value warnings remain. The Python runtime logging scan passed across 125 scripts, and the logging, building-garrison and dynamic-base-flow suites passed 20 tests in total. These checks do not establish live restoration fidelity, multiplayer continuity or performance gains.

## Smallest useful live test

Use a dedicated server with a second observing client and a known supported, undamaged stationary garrison. For diagnostics, launch the server with `-iaDebug 1`; successful transitions log `[IA][DynamicAI] Cached` and `[IA][DynamicAI] Restored`. A persistent restoration problem warns after five failures while retaining the roster slot. See [runtime logging](runtime-logging.md).

1. **Off baseline:** run the same objective with the toggle off. Record live soldier counts, server frame times, normal capture behavior and initial spawn timing.
2. **Distant cache:** start a fresh objective with the toggle on and all players beyond 1500 m from the team. Allow placement and the 30-second minimum. Verify physical soldiers disappear only after their records are captured, logical strength does not drop, and distant empty zones do not immediately wake themselves.
3. **Approach and fight:** cross 1000 m while the second client watches the post. Verify the original number and prefabs return at the expected floors and positions, groups respond, and capture cannot progress through missing queued defenders. Kill one soldier, withdraw and return; verify no second cache cycle or resurrection.
4. **Disable during restoration:** with several fresh teams cached, turn the toggle off while restoration is queued. Verify all surviving slots eventually restore, including teams far from every player, without duplicates or lost strength.
5. **Cleanup during restoration:** repeat with fresh teams and finish/cancel the area while work remains queued. Verify no later resurrection and that ordinary cleanup still protects nearby/player-controlled actors.
6. **Multiplayer limits:** approach from two sides, observe through distant optics, then try a fast arrival and an indirect-fire strike at a dormant area. Record visible pop-in and missing target damage explicitly; compare full-objective frame-time tails and spawn spikes against the baseline.

These are acceptance checks to run, not reported test results. The broader visibility, persistence and repeated-hibernation proposal remains in [AI virtualization investigation](ai-virtualization-design.md).
