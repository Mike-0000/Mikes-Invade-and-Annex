# Dynamic AI Spawning

Dynamic AI Spawning is an optional server-authoritative cache for supported stationary infantry garrisons. Teams can repeatedly leave the live simulation and return. Each cycle saves the **current survivors and their latest exact world transforms**, rather than refilling the original squad. Equipment and ammunition reset from the saved character prefab on restoration. Live multiplayer behavior and performance savings remain unverified.

## Enable or disable

Open the admin configuration menu, select **Scaling**, and use **Dynamic AI Spawning**. The setting defaults to **off**. With it off and no previously cached soldiers, spawning follows the existing path.

Turning it off requests restoration of every cached team through the shared work queue. Pending survivors continue to count toward logical strength, and capture readiness stays protected until restoration finishes. Settings changes reset the quiet period. Turning it back on permits future cache cycles after the normal eligibility checks and a fresh quiet period.

## Cache and restore cycle

1. Initial areas, groups and soldiers spawn through the existing phased paths. Their normal placement must finish; building garrisons must pass their existing arrival checks.
2. Every member must remain **more than 1500 m horizontally from every sampled player for 60 continuous seconds** while the team otherwise qualifies. Approaching players or another eligibility blocker reset that timer. Recorded danger must also be at least **60 seconds old**; a currently selected combat target seen within the last 60 seconds blocks caching. Earlier combat and casualties do not permanently disqualify a team.
3. The cache rebuilds its records from the team's current living members. Each surviving soldier's actual prefab and world transform, including height and orientation, replace the previous cycle's snapshot. Dead soldiers are omitted. The original native group and addon ownership remain while the saved character entities are removed. At most one team enters the cache per one-second scan.
4. Any saved soldier within **1000 m horizontally of any sampled player** requests the whole team's restoration. Capture readiness can request defenders whose saved positions lie inside its capture sphere. Turning the setting off requests every cached team.
5. A wake request stays latched. Restoration runs every 100 ms, with a shared limit of four attempts and approximately 4 ms of work per tick, checked between attempts. Failed resource, creation or attachment work keeps its soldier record and retries after one second. There is no population-cap or line-of-sight veto that cancels a requested wake.
6. Successfully restored soldiers can respond while their teammates remain queued. Finishing restoration resets the quiet period; the surviving team can cache again after meeting the same conditions. A death during restoration remains a casualty, never a fresh spawn attempt. A recreated pawn possessed by a player is released from pending restoration without regrouping or duplicating it.

These distances and timers are prototype constants in `Scripts/Game/IA_DynamicAISpawning.c`, not admin-adjustable controls.

Town capture retains physical faction counts. Live friendlies that could capture request missing relevant defenders, and progress/scoring pause until a subsequent census confirms readiness. Base seize likewise waits before awarding time. Scanning an empty distant zone does not itself wake its garrison. Area shutdown retires cached records before delayed work can recreate soldiers in a completed objective.

## Casualties, injuries and equipment

**Downed AI are killed when the team commits to caching.** The code uses the normal damage-manager death path with the existing instigator and verifies that death actually occurred. If that death request is refused, the remaining team stays live with its accounting retained. These soldiers do not enter the survivor snapshot. Existing corpses and newly killed downed soldiers remain subject to ordinary corpse/objective cleanup; they are never revived by restoration.

A conscious wounded soldier, or one carrying a persistent damage/status effect, keeps the **whole team live until it recovers**. Health and status are not serialized, so the system does not heal such a soldier by deleting and recreating its prefab. If another blocker prevents the cache transaction, downed members are not killed merely because the team was considered for caching.

The chosen persistence scope is positions and casualties. **Changed equipment, inventory and ammunition are not preserved**: restored soldiers receive their saved prefab's normal equipment/ammo, and randomized appearance or loadout choices may be initialized again. Animation state and behavior-tree execution are also not serialized. Existing combat-profile setup is reapplied. Full inventory and health persistence is separate future work.

## Supported scope and limits

Candidates are groups assigned to supported stationary hold posts, including qualifying building and base garrisons. Partial surviving squads can qualify. Player-controlled members, forced permanent AI LOD, missing components, pending spawns or unresolved placement keep a team live.

Vehicles and their crews/passengers, static-gun and mortar crews, airborne forces, moving patrols, elite profiles, HVT/objective units, active defense waves, and groups moving under an explicit simulation pin remain excluded. This version does not simulate dormant patrol movement or combat between absent soldiers.

Distance does not prove invisibility. There is no visibility-aware exposed-post filter, aircraft prediction, remote-camera protection or concealed-placement fallback. Long-range optics, helicopters, HALO and teleports can expose a saved position before restoration finishes. Requested teams still restore at their saved positions even when visible. Changed geometry or objects can affect the resulting placement; restoring an exact transform does not guarantee physics will leave the soldier there.

Dormant characters cannot be hit by ordinary bullets or explosions. This version has no pre-impact artillery wake integration. Keeping mortar crews live does not make absent targets vulnerable. Reconnaissance pop-in and indirect-fire immunity remain explicit prototype limitations.

Caching saves distant simulation work but adds repeated deletion and creation costs. Native LOD already reduces some distant work. Measure a full objective before increasing nearby AI density, including split squads and repeated withdrawals/returns.

## Validation status

The expanded native `IA_DynamicAISpawningConfigTest` passed with exit code 0 and `failures=0`, including departure, combat-quiet and repeat-cycle reset timelines. The final scripts, including the downed-death result guard, validated successfully for WORKBENCH, PC, XBOX, PS4 and PS5 without compiler errors. The runtime logging scan passed across 126 scripts, and the logging, building-garrison and dynamic-base-flow suites passed 20 Python checks. These results do not establish live state fidelity, multiplayer continuity or performance gains.

## Smallest useful live test

Use a dedicated server with a second observing client. Launch the server with `-iaDebug 1` to see `[IA][DynamicAI] Cached` and `[IA][DynamicAI] Restored` transitions. Persistent restoration trouble warns after five failures while retaining the roster slot. See [runtime logging](runtime-logging.md).

With diagnostics enabled, each cache also prints a `Removal audit` after 250 ms. `remaining=0` means none of the original soldier entities remain; this is separate from the saved roster and retained group. The line includes active-AI and editor AI-budget values before/after (`-1` means unavailable). Global counts can include simultaneous spawns elsewhere, so compare those with the original-entity result. An unchanged Game Master budget should not be assumed to prove either successful removal or failure; its editable-entity accounting is separate from mission strength.

1. **Off baseline:** run the same objective with the toggle off; record live soldier counts, server frame times, capture behavior and initial spawn timing.
2. **Six-soldier survivor test:** enable the feature on a supported six-person stationary garrison. Kill two, leave one unconscious, and move the three healthy survivors to distinct valid positions, including different floors if available. Release any GM possession. Keep every player beyond 1500 m, wait for 60 seconds of eligible distance and expired combat activity, and confirm caching commits. The downed soldier should die normally. Return within 1000 m: exactly **three** soldiers should restore at their latest saved positions, with no revived casualties. Equipment/ammo reset is expected.
3. **Second cycle and interrupted departure:** move those three survivors again, withdraw, briefly approach inside 1500 m, then leave. Confirm the approach restarts the full quiet period. After another cache/return, still expect **three**, now at the second saved positions. Verify recent fighting and a conscious wounded/status-affected member keep the team live.
4. **Disable during restoration:** with several teams cached, turn the toggle off while work remains queued. Verify all surviving slots restore, including distant teams, without duplication or a logical strength loss. Capture must not progress through missing queued defenders.
5. **Cleanup during restoration:** finish/cancel an area while soldiers remain queued. Verify no later resurrection and ordinary cleanup protection for nearby/player-controlled actors.
6. **Multiplayer limits:** approach from two sides, watch through distant optics, then try a fast arrival and indirect fire at a dormant area. Record visible pop-in and absent-target damage explicitly. Compare full-objective frame-time tails and transition spikes with the baseline.

These are acceptance checks to run, not reported live test results. The broader visibility and full-state persistence investigation remains in [AI virtualization investigation](ai-virtualization-design.md).
