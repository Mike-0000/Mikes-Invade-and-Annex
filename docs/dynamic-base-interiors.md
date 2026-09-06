# Dynamic base interiors

The interior pass adds purposeful work and living areas around the existing buildings. Sandbag declarations and perimeter geometry are unchanged. See [the layout sheet](dynamic-base-interiors.svg) for all six bases at the same scale.

Six reusable scenes provide a briefing board and desk, mess table with ration cartons, staged crates and sacks, water barrels and cans, workshop spares and tools, and small utility stations. Individual props use slight offsets and rotations; supplies sit on measured table/crate surfaces. Scenes are placed near related facilities while preserving entrances, guard posts and circulation lanes.

| Layout | Added scenes | Conservative expanded entity total |
| --- | ---: | ---: |
| Full | 12 | 757 |
| Compact | 8 | 556 |
| Courtyard | 6 | 349 |
| Roadside | 5 | 317 |
| CommandPost | 4 | 218 |
| RallyPost | 3 | 108 |

All totals remain below the configured 850-entity ceiling. Dressing is optional: it adds no survey terrain checks and cannot reject a base position or force a smaller layout. Construction checks each scene's support and clearance and omits unsuitable scenes. Explicit hierarchy components keep child props owned by their scene root for positioning and cleanup.

## Reproduction and verification

Run `python tools/author_base_interiors.py D:/ReforgerGameSources/data/data007` to regenerate prefabs, layout placements, scene metadata and the Workbench measurement probe. The generator asserts sandbag declarations remain identical and is idempotent. Run `tools/check_dynamic_base_layouts.py` and `tools/validate_dynamic_base_assets.py` (the latter takes the same source directory) to check spacing, routes, guard clearance, measured scene bounds, references and budgets.

`IA_BaseInteriorProbe` measures stock assets and complete vignette hierarchies in Workbench. Its script log can be imported using the author tool's `--import-measurement` argument. Regenerate assets after updating stock measurements, then repeat the probe to measure the final clusters. `tools/draw_dynamic_base_interiors.py` regenerates the SVG plan.

Validation on 2026-09-05: all five Workbench script configurations passed; Enforce regression and complete vignette measurements reported zero failures. The Kolguyev group-0, seed-12345 terrain audit returned exactly the same 31 layout/position/heading results as before dressing, with 3,300 callbacks and 15.9 seconds of work in its final pass. All six authored layouts pass clearance and budget checks. Actual in-game visual composition, AI navigation and multiplayer frame time still need a playtest.
