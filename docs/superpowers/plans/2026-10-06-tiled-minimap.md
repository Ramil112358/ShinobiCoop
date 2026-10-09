# Full-level tiled minimap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans. Implement the tasks in order and verify only the changed minimap/map bake at the stated milestones.

**Goal:** Give every playable Shinobi HUD map readable full-level cartographic artwork and a live north-up player position marker.

**Architecture:** Generate static map art in the editor/build pipeline, split it into coordinate-addressed tiles and store explicit XY bounds and tile references in a level-owned data asset. Render those tiles in the existing HUD and update the player, teammate, village, and camp overlays from world positions.

**Tech Stack:** Unreal Engine 5.8, C++, UMG, Unreal Editor Python, existing Python/Pillow map-generation scripts, Unreal Automation Framework.

**Spec:** `docs/superpowers/specs/2026-10-06-full-level-minimap-design.md`

## Global Constraints

- The map is north-up and covers the full playable extent without squashing its aspect ratio or relying on runtime mesh-bound guesses.
- The map itself does not pan or rotate; the player icon visibly moves across the entire level.
- The menu has no minimap. Every playable level that uses the Shinobi gameplay HUD must have a map asset.
- Keep map capture and texture generation out of runtime; rebake static map art when a source level changes.
- Verify the updated HUD and relevant map bake only at milestones. Do not run unrelated whole-game tests for routine changes.

## Review Focus

- **World-axis conventions:** London has the opposite Y sign from Hidden Leaf Valley; top-left/top-right and north/south markers must land correctly. Pin this with the coordinate automation test in Task 1.
- **Bounds and aspect ratio:** map edges and non-square levels must map to tile edges without distortion. Check explicit-bound corner UVs in Task 1 and the generated edge tiles in Task 2.
- **Missing or incomplete tile assets:** an absent texture or grid hole must fail the bake validation and show the documented HUD fallback. Check the map-data validator in Task 2 and fallback in Task 3.
- **World Partition coverage:** unloaded cells must not become blank map areas. Check the captured tile coverage against each map's explicit XY bounds in Task 2.
- **Live marker correctness:** local/remote positions and cleared-camp state must not drift or remain stale. Verify pawn movement and camp state at the Task 4 HUD milestone.

---

### Task 1: Define map data and world-to-map coordinates

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiMiniMapData.h`
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiMiniMapData.cpp`
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiMiniMapMapInfo.h`
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiMiniMapMapInfo.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiMiniMapTests.cpp`

**Interfaces:**
- Produces `UShinobiMiniMapData`, with `WorldMin`, `WorldMax`, `bPositiveYIsNorth`, `TileCount`, and row-major `Tiles`.
- Produces `FShinobiMiniMapTile`, with integer `X`/`Y` and optional `Base`, `RoadMask`, `WaterMask`, and `BuiltAreaMask` textures.
- Produces `AShinobiMiniMapMapInfo : AInfo`, with a hard `UShinobiMiniMapData` reference for the current level; the HUD locates this actor in Task 3.
- Produces `UShinobiMiniMapData::WorldToUV(const FVector& WorldLocation) const`, returning clamped 0..1 top-left-origin UV coordinates using explicit bounds and north orientation.

- [ ] **Step 1: Add the data types and `WorldToUV`** in the four UI files. Reject zero-width or zero-height bounds in the data validator. Keep tile coordinates explicit so a missing row/column can be detected.
- [ ] **Step 2: Add `ShinobiCoop.UI.MiniMap.WorldToUV`** in `ShinobiMiniMapTests.cpp`. Assert all four corners, midpoint, clamping, and north-up output for both positive-Y-north and negative-Y-north data.
- [ ] **Step 3: Run only the coordinate test** with `& .\Scripts\run_tests.ps1 -Filter "ShinobiCoop.UI.MiniMap.WorldToUV"`. Expected: one test passes, with no unrelated test groups run.
- [ ] **Step 4: Build the editor target** with `Scripts/build.ps1`. Expected: `BUILD OK`.

### Task 2: Bake and validate per-level tiled artwork

**Files:**
- Create: `Scripts/minimap/minimap_maps.json` (per-level source, bounds, north orientation, and resolution settings)
- Create: `Scripts/minimap/make_minimap_art.py` (deterministic source-layer rendering and tile/LOD output)
- Create: `Scripts/minimap/bake_minimaps.py` (editor load/capture/import, data asset creation, and validation)
- Modify: `Source/ShinobiCoop/Shinobi/World/ShinobiWorldBuilderLibrary.h/.cpp` only for editor capture/export functions proven necessary by the Valley prototype.
- Modify: `Scripts/world/build_world.py` and `Scripts/london/build_london.py` only to invoke or register their map-data/bake steps where needed.
- Generated: `SourceArt/World/Minimaps/` source PNG tiles and `Content/Shinobi/UI/Minimaps/` cooked texture/data assets.
- Modify: the `Lvl_TrainingGrounds` and `/Game/Variant_Combat/Lvl_Combat` level data through the editor bake script so each owns an always-loaded `AShinobiMiniMapMapInfo` reference.

**Interfaces:**
- Consumes `UShinobiMiniMapData`/`FShinobiMiniMapTile` from Task 1.
- Consumes Valley data from `Scripts/world/world_layout.json` and generated terrain weights; consume London's map data from `Scripts/london/london_layout.json` and `SourceArt/World/London/city.json`.
- Produces per-level textures and data assets for `Lvl_HiddenLeafValley`, `Lvl_London`, `Lvl_TrainingGrounds`, and `Lvl_Combat`; map-info actors in World Partition levels are not spatially loaded.
- Produces an editor validator that checks non-empty bounds, valid tile count, every expected tile/layer, texture dimensions, and map actor reference before saving assets.

- [ ] **Step 1: Prototype the Valley output** from its path, river, terrain, and area data. Generate one base tile and the roads/water layer files, import them through the editor, and check that the village-to-river-to-boss route is legible and north-up.
- [ ] **Step 2: Implement deterministic tile and LOD generation** for the explicit source data. Use source data directly for Valley paths/river and London's roads/water/surfaces. Preserve each map's explicit bounds and aspect ratio in `minimap_maps.json`.
- [ ] **Step 3: Implement editor capture for maps without layout layers.** Load the combat and training maps, capture their relevant scene background from above into the base layer, and include all needed World Partition cells. Missing coverage within configured bounds is a bake error.
- [ ] **Step 4: Import tiles and create one `UShinobiMiniMapData` plus `AShinobiMiniMapMapInfo` binding per playable map.** Bake LODs, preserve Y orientation, and run the validator; expected: all four maps have complete tile grids and no missing map bindings.
- [ ] **Step 5: Run the bake for one map at a time and inspect each generated atlas/tile seam.** First Valley, then London, then combat and training. Expected: roads, water/green areas where present, and built-up surroundings remain aligned at tile edges; no broad game test run.

### Task 3: Render map tiles in the shared HUD

**Files:**
- Create: `Content/Shinobi/UI/Materials/M_UI_MiniMapTile` (composites base, roads, water, and built-area masks with the approved muted palette)
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.cpp`

**Interfaces:**
- Consumes `AShinobiMiniMapMapInfo` and `UShinobiMiniMapData` from Tasks 1–2.
- Replaces runtime world-bound discovery with the loaded map data's explicit bounds and north orientation.
- Adds a tile surface to the map canvas; each tile occupies its correct normalized grid rectangle and uses a dynamic instance of `M_UI_MiniMapTile` for optional layer textures.
- Keeps `UpdateMiniMap` as the single update path for the local player, teammates, village, camps, and boss. Convert world positions through `WorldToUV` every HUD tick.

- [ ] **Step 1: Add tile image pooling and map-data binding** to the existing minimap canvas. When the level's map-info actor is missing or invalid, display a clear fallback label and emit one warning for that level.
- [ ] **Step 2: Replace the grid-only background** with the correct tiles and map layer material. Preserve full-level north-up display, aspect ratio, map clipping, and the bottom-right anchor; resize to approximately 250 pixels while preserving margins.
- [ ] **Step 3: Bind all markers to `WorldToUV`** and set the player arrow rotation from pawn yaw. Retain camp cleared tint, village, boss, and teammate markers; collapse unused pooled markers when actors disappear.
- [ ] **Step 4: Build the editor target** with `Scripts/build.ps1`, then run only `& .\Scripts\run_tests.ps1 -Filter "ShinobiCoop.UI.MiniMap.WorldToUV"`. Expected: build succeeds and the targeted coordinate test passes.

### Task 4: Verify representative movement and per-map HUD display

**Files:**
- Modify only map source/config/art if visual inspection finds a concrete bounds, orientation, tile, or contrast defect.

**Interfaces:**
- Consumes the complete map data and HUD implementation from Tasks 1–3.

- [ ] **Step 1: Play Hidden Leaf Valley and move from the village along a path toward the river.** Confirm the marker traverses the same visible route and moves continuously; check north/east corners against known landmarks.
- [ ] **Step 2: Play London and check a north/south landmark pair.** Confirm its negative-Y north convention is rendered north-up and the marker follows movement without mirroring.
- [ ] **Step 3: Play combat and training individually.** Confirm each shows its baked surroundings, explicit full-map bounds, and a live player arrow without missing-cell gaps.
- [ ] **Step 4: In a two-player session, move the second pawn and clear one camp.** Confirm the teammate marker moves and the camp marker changes state once; do not rerun unrelated combat or ability suites.
- [ ] **Step 5: Run `git diff --check` for changed files and report the four map-specific visual results.**

