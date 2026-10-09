# Full-level minimap with baked map tiles

## Context

The gameplay HUD has a bottom-right minimap panel with world bounds and live player, teammate, village, and camp markers. Its background is only a grid, so it does not help players recognize roads or nearby surroundings. The player marker already updates with the HUD, but the map artwork does not contain the world geometry needed to orient it.

The requested result is a full-level, north-up map in the bottom-right HUD. The player marker moves across the level map in real time. The intended visual reference is the readable, layered map presentation of large RPGs such as *The Witcher 3*.

The project has map-source data for the two open-world levels:

- `Scripts/world/world_layout.json` contains Hidden Leaf Valley's path lines, river, areas, and camps.
- `Scripts/london/london_layout.json` and `Scripts/london/make_city.py` describe London's map and generated road/water/green-space geometry.
- The arena and training maps do not have equivalent road-layout files, but their placed world geometry can be captured in the editor.

CD PROJEKT RED's REDkit documentation describes minimap artwork generated from bird's-eye world images, separated into data layers, processed, and divided into coordinate-addressed tiles. That supports a baked, tiled pipeline for this project. GTA V modding documentation also describes a tile-based map texture system. Public *Throne and Liberty* patch notes describe minimap marker behavior but do not disclose its rendering pipeline.

## Approved design

### Map artwork and tile generation

Generate each playable level's static map artwork in the editor and store it as tiled, cooked textures. Do not render the map with a live scene capture during gameplay.

The generation workflow should produce a readable, muted cartographic look from the actual level:

- terrain and ground color;
- roads and paths, with stronger contrast for main routes;
- rivers and other water;
- built-up areas/building footprints where the level has them;
- parks or broad vegetation regions where available.

Use explicit source data when it is more accurate than extracting the scene. Valley paths and river should come from `world_layout.json`. London's generated road, river, and surface geometry should be represented by the London map source/build pipeline. For maps without that data, capture the relevant scene layers from the loaded level in the editor.

Tiles use a common north-up world coordinate transform and include lower-resolution mip/LOD levels for the small HUD display. Their aspect ratio follows the playable map extent. Generation must include all relevant streamed world cells and place the capture within explicit level bounds; bounds must not be inferred at runtime from arbitrary mesh actors.

Each map has a minimap data asset (or equivalent level-owned data record) that contains:

- explicit world XY bounds and north orientation;
- tile coordinates and texture references for the finished map and its LODs;
- optional named points of interest for future labels, if those are not already represented by gameplay actors.

The map generation tool must be repeatable, and map changes must have a clear rebake step. The map asset is a build-time product; no per-frame or per-client scene capture is part of runtime behavior.

### Runtime HUD

Keep the map fixed to the full level, north-up, in the bottom-right corner. Enlarge the current 220-pixel panel modestly (target approximately 250 pixels, subject to viewport layout) and preserve the HUD's existing margins and legibility at smaller resolutions.

At runtime, the HUD selects the current level's minimap data and draws its tiles into one map surface. Overlay markers use the data asset's XY bounds to convert actor world positions to map positions:

- local player: high-contrast directional arrow, updated every HUD tick;
- co-op teammates: distinct player markers, updated as their replicated pawn positions change;
- village/hub: persistent landmark;
- camps and boss: distinct icons, with a subdued state after a camp is cleared.

The map itself does not pan or rotate; the player icon visibly moves across the entire level. Markers remain clipped to the map boundary when an actor is outside the mapped extent. North-up behavior and the XY-to-texture transform must be consistent across levels, including London's coordinate convention.

The menu has no minimap. Every playable level that uses the Shinobi gameplay HUD must have a map asset. During development, missing map data should be visible as a clear fallback and reported to the log rather than silently presenting incorrect world bounds.

### Scope and non-goals

In scope are Hidden Leaf Valley, London, the combat arena, the training grounds, and any other playable level that uses the Shinobi HUD. Each needs artwork and explicit bounds. Newly added playable levels need a minimap bake/data asset before release.

This work does not add route planning, turn-by-turn directions, a player-centered zoom, map rotation, floor switching, or dynamic world changes to the baked background. Those would need separate design work.

## Alternatives considered

1. **Editor-baked tiles (chosen):** real level detail, low runtime cost, supports all levels. Map geometry changes require rebaking and reviewing its output.
2. **Live top-down scene capture:** automatically reflects geometry changes, but rendering a whole level continuously is expensive and scene roofs/foliage can obscure roads at minimap scale.
3. **Hand-authored vector maps for every level:** cleanest road rendering but duplicates geometry and requires new route/surface data for the arena and training maps.

## Acceptance criteria

- Every playable Shinobi HUD level displays its actual surroundings, roads, and water/land regions in the bottom-right map; it does not show the placeholder grid when configured.
- The map is north-up and covers the full playable extent without squashing its aspect ratio or relying on runtime mesh-bound guesses.
- The player arrow moves smoothly to the correct map location as the pawn moves and points in the pawn's facing direction.
- Co-op, village, and camp markers use the correct XY transform; cleared camps have a visually distinct state.
- Map tiles remain aligned at tile boundaries and at the HUD's display resolution.
- The editor bake can regenerate the affected map art after its source level changes.
- Verification is scoped to the updated HUD and a representative level/map bake, then a visual pass across each playable level's minimap. Avoid running unrelated whole-game test suites for routine changes.

## Implementation planning notes

The implementation plan should first validate the editor-side capture/export/import path with one representative map. Prefer the Valley because its source road/river paths are explicit; then exercise the same data/runtime interface on London and the arena/training maps. Choose tile dimensions, layer encoding, import automation, and asset ownership after that probe. Do not begin broader implementation until the reviewed spec is approved and the plan's execution method is selected.

## References

- [The Witcher 3 REDkit: Generate minimaps](https://cdprojektred.atlassian.net/wiki/spaces/W3REDkit/pages/51904544/HOW-TO%2BGenerate%2Bminimaps)
- [GTA V minimap tile and radar texture overview](https://fivem-minimaps.com/guides/fivem-map-tiles-and-radar-textures) (community modding documentation)
- [Throne and Liberty update notes: minimap icons and interactions](https://www.playthroneandliberty.com/en-gb/news/articles/update-2-12-0)

## Implementation notes (2026-10-09)

The shipped version differs from the plan above in these points:

- **One orientation for every map.** UE's axes are left-handed, so the unmirrored top-down view with +X to the right has +Y pointing down. Every map, the compass included, uses +X east and +Y south (north = yaw -90). The old Valley art drew +Y upward, which mirrored it: the arrow turned the wrong way, and the compass had E and W swapped. The `bPositiveYIsNorth` flag is gone. Valley's boss ridge is now in the south.
- **Bounds are in world cm.** Valley and London previously stored layout metres, so their markers were pinned to the map centre. `source_units_cm` in `minimap_maps.json` now converts the source data.
- **No level binding actor.** The HUD finds `Lvl_<Name>`'s data at `/Game/Shinobi/UI/Minimaps/<Name>/DA_<Name>MiniMap` (`UShinobiMiniMapData::ForLevel`). Level packages are not edited. A `Lvl_` level with no data logs a warning and hides the panel.
- **PvP arenas are covered.** Each arena is a crop of the London artwork (`source: london_crop`), because the arenas keep London's world coordinates. In PvP the map shows the player and teammates, never opponents.
- **Single base layer only.** There are no road, water or built-area masks and no tile material. The tiles get mipmaps so the 2048 px art downsamples cleanly.
- **Re-bake:** `python Scripts/minimap/make_minimap_art.py --map <key> [--retile]`, then `Scripts/run_python.ps1 Scripts/minimap/bake_minimaps.py`. To bake only some maps, set `$env:SHINOBI_MINIMAP`. Training and Combat have no scene-capture step. Their `artwork.png` files (recovered from the cooked tiles) are re-tiled as they are. A layout change to those levels needs new artwork.
- **Zoom and big map (added 2026-10-09).** This replaces the "no player-centred zoom" non-goal above.
  - **Controls:** M (gamepad View) opens a big map in the middle of the screen, in place of the minimap and the compass. While it is open, the mouse wheel zooms the big map and the camera does not zoom. `=` / `-` (numpad `+` / `-`, D-pad up) step the zoom of whichever map is showing. Stepping in past the closest level wraps back to the whole map, because D-pad down is Dismount.
  - **Zoom levels:** the steps are 1x, 2x, 4x and 8x. A step is offered only while the view still spans at least 60 m and one art texel covers at most 2.5 screen units. The minimap starts at 2x.
  - **Following the player:** zoomed views centre on the player and stop at the map edges.
  - **Detail limit:** the art is 2048 px, so close zoom on London cannot show more detail than that. Sharper close-ups need the art redrawn at a higher resolution from the London and world build data.
