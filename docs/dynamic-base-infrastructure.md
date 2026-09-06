# FOB infrastructure and daily life

This pass develops the existing optional interior scenes into recognizable service areas. All authored sandbag declarations, structural modules, selection code and defender code are unchanged. The [review sheet](dynamic-base-interiors.svg) shows all six layouts at one scale; it is an authored plan, not an in-game screenshot.

## Facilities

Full includes a Soviet field kitchen, bulk water and washing, two latrine/handwashing points, covered ammunition receiving areas, workshop tools and fire equipment, medical seating and cases, barracks rest space, communications equipment, contained waste, power distribution and six task/entrance lights. Compact combines receiving/maintenance support and uses one sanitation point. Courtyard includes a kitchen and medical receiving pocket. Roadside emphasizes stores and servicing. CommandPost has compact power/comms, washing, sanitation and ration/rest scenes. RallyPost retains only portable power, a lit ration table, stores and handwashing.

Overlapping water, utility, stores and workshop scenes were replaced or upgraded instead of duplicating functions. Sanitation and waste pads remain at least 6 m from food/water pads. Full/Compact have stock water tanks; smaller layouts use barrels, containers and sinks. A stock no-smoking sign and electrical box are attached to the power cluster's crate. Latrine doors are closed scenery without player actions. Scene access pads include servicing/door space; the existing route and guard margins remain enforced. The authoring search may find a nearby alternative pocket within the layout but never moves structural modules.

## Budgets and behavior

| Layout | Optional clusters | Roots | Expanded entities (conservative) | New lights |
| --- | ---: | ---: | ---: | ---: |
| Full | 16 | 230 | 769 | 6 |
| Compact | 12 | 157 | 566 | 4 |
| Courtyard | 9 | 115 | 358 | 3 |
| Roadside | 9 | 110 | 336 | 3 |
| CommandPost | 9 | 83 | 239 | 2 |
| RallyPost | 4 | 53 | 114 | 1 |

The existing limits remain 256 roots and 850 expanded entities. Lights use the stock interactive-light component with no interaction actions: a single 4 m point emitter per table lamp or a 14 m, downward-aimed, inward-facing entrance spotlight. All new lights are shadowless and initially lit. There is no day/night controller. Each power cluster has one stock localized portable-generator sound component; floodlight units have no extra sound source. No new supply services, AI routines, power simulation or custom per-frame loops were added.

Scenery variants for generators, lamps, covered ammunition, medical boxes, latrines, electrical boxes and signs use explicit component allowlists. Stock meshes/materials are retained without inventory, action, composition or service controllers. Parent/child hierarchy ownership is explicit, including the latrine door and covered-stock net. These variants are static scenery; they do not inherit the stock destruction gameplay. Existing props elsewhere retain their previous behavior.

Infrastructure uses the existing optional Dressing role. It adds no survey traces and does not affect site ranking. Construction retains support/volume checks and can omit individual clusters. All dependent accessories belong to the same root. No runtime API or configuration migration is required.

## Verification

- All six layout checks pass: reserved routes, guard clearance, pad separation and unchanged perimeters.
- Asset checks pass: references, metadata, measured bounds, conservative counts, hygiene spacing, inward lighting, actual emitter counts and one ambience source per base. The author tool also checks that raised lamps, cartons and tools remain on support surfaces.
- Workbench measurement passes for every new complete cluster and standalone scenery variant. Direct-child checks catch missing hierarchy attachments; final runs have no scene/entity script leaks.
- Enforce selection/defend regressions pass with zero failures.
- `IA_BaseInfrastructureTest` passes 177 real prefab construction attempts across all six layouts with flat, gentle and excessive synthetic support spans. Injected optional obstruction rejection allows subsequent construction to continue. Cleanup leaves zero scene entities. Its 49 ms fixture work measurement is not a live construction benchmark.
- The Kolguyev group-0, seed-12345 audit returns the exact same 31 unique layout/center/heading results as the previous interior pass. Its final pass used 3,302 callbacks and 16.158 seconds of survey work, comparable to the prior 3,300 / 15.934 seconds. This is a survey audit, not a client/server frame-time comparison.
- All five Workbench script validation configurations pass.

Rebuild with `python tools/author_base_interiors.py D:/ReforgerGameSources/data/data007`. Re-measure using `IA_BaseInteriorProbe`, import its script log with the author tool's `--import-measurement` option, then run `tools/check_dynamic_base_layouts.py` and `tools/validate_dynamic_base_assets.py` (the latter takes the same source directory). `tools/draw_dynamic_base_interiors.py` regenerates the review sheet. Run `IA_BaseInfrastructureTest` and `IA_BaseSelectionTest` through Workbench ResourceManager and `IA_BaseTerrainAudit` through WorldEditor.

## Required live verification

The command-line preview world does not register immediate live physics contacts or run the normal lighting/AI/network session. Thus its synthetic obstruction test verifies continuation behavior, not a physical-blocker playtest. Day/night player-eye screenshots, light glare and sound volume review, walking routes, defender combat navigation, join-in-progress lighting and multiplayer cleanup have not been verified here. No in-game screenshots are supplied or implied by the layout sheet.

For release acceptance, test each size at noon and midnight, including an uneven site and a deliberately obstructed facility; capture player-eye and overview screenshots. Exercise capture/defense and cleanup with two clients, including a late joiner. Compare construction latency and client/server frame times against the previous interior commit under matching conditions. If a repeatable regression exceeds 5%, reduce optional detail or lighting cost without changing the placement sample set or structural layout. These live acceptance checks remain outstanding.
