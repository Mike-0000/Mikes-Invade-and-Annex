# Dynamic base interiors

The interior pass adds purposeful work and living areas around the existing buildings. Sandbag declarations and perimeter geometry are unchanged. See [the layout sheet](dynamic-base-interiors.svg) for all six bases at the same scale.

The infrastructure pass extends the original scenes with kitchens, washing, sanitation, bulk water, covered supplies, medical receiving areas, power distribution, rest spaces and task lighting. Individual props use slight offsets and rotations; supplies and lamps sit on measured support surfaces. See [infrastructure details and verification limits](dynamic-base-infrastructure.md).

| Layout | Added scenes | Conservative expanded entity total |
| --- | ---: | ---: |
| Full | 16 | 769 |
| Compact | 12 | 566 |
| Courtyard | 9 | 358 |
| Roadside | 9 | 336 |
| CommandPost | 9 | 239 |
| RallyPost | 4 | 114 |

All totals remain below the configured 850-entity ceiling. Dressing is optional: it adds no survey terrain checks and cannot reject a base position or force a smaller layout. Construction checks each scene's support and clearance and omits unsuitable scenes. Explicit hierarchy components keep child props owned by their scene root for positioning and cleanup.

## Reproduction and verification

Run `python tools/author_base_interiors.py D:/ReforgerGameSources/data/data007` to regenerate prefabs, layout placements, scene metadata and the Workbench measurement probe. The generator asserts sandbag declarations remain identical and is idempotent. Run `tools/check_dynamic_base_layouts.py` and `tools/validate_dynamic_base_assets.py` (the latter takes the same source directory) to check spacing, routes, guard clearance, measured scene bounds, references and budgets.

`IA_BaseInteriorProbe` measures stock assets and complete vignette hierarchies in Workbench. Its script log can be imported using the author tool's `--import-measurement` argument. Regenerate assets after updating stock measurements, then repeat the probe to measure the final clusters. `tools/draw_dynamic_base_interiors.py` regenerates the SVG plan.

The original interior pass and this infrastructure pass both preserve the same 31 Kolguyev group-0, seed-12345 terrain results. Current validation evidence and remaining live playtests are recorded in the infrastructure notes.
