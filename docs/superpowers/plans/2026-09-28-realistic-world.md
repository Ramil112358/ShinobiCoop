# Realistic World Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hidden Leaf Valley looks realistic and polished (Poly Haven + Megaplants assets, photoscanned layered ground, landscape grass, Water plugin river, realistic lighting, character-only outline) with gameplay unchanged.

**Architecture:** Swap in place: the offline pipeline (`world_layout.json` → `make_heightmap.py` / `make_placements.py` → `build_world.py` stages) stays; the catalog points at new assets, the landscape gets a new material and a `forest` layer, the dense grass moves to `ULandscapeGrassType`, the river becomes a `WaterBodyRiver`, and `M_ToonPost` is replaced by a stencil outline.

**Tech Stack:** UE 5.8.2 (Nanite, Nanite landscape displacement, Lumen, VSM, Water plugin, RVT, Interchange), UE Python commandlet, system Python 3.12 + numpy, Blender 5.2 headless, Poly Haven API (`https://api.polyhaven.com`), Fab (user-added Megaplants).

**Spec:** `docs/superpowers/specs/2026-09-28-realistic-world-design.md`

## Global Constraints

- Branch `feature/realistic-world` (stacked on unmerged `feature/open-world`).
- Never stage the user's `Config/DefaultEngine.ini` map-line edit (`GameDefaultMap`/`EditorStartupMap` → Lvl_Combat). Other ini hunks are staged with `git hash-object -w --stdin` + `git update-index --cacheinfo`.
- Stage by explicit path, never `git add -A`. Leave the user's untracked `SourceArt/Mixamo/*.fbx` alone.
- Close/check the GUI editor (`Get-Process UnrealEditor`) before builds and commandlets; never kill it.
- Git Bash: `MSYS_NO_PATHCONV=1` for `/Game/...` arguments.
- Text files UTF-8 without BOM (Write/Edit/python, not PowerShell `Set-Content`).
- Never screen-capture the desktop; only in-engine harnesses (`Scripts/shot_chakra.ps1`).
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Raw downloads gitignored (`SourceArt/World/PolyHaven/`); imported assets committed.
- Perf floor: 30+ fps at Epic, host + client (fidelity first).
- Unity build: file-local globals get unique names (a clash already bit us once).

## Review Focus

1. The hidden Manny driver mesh must not write custom depth, or an invisible mannequin gets an outline next to the hero. Pinned in Task 5 test.
2. Photoscanned trees must not get per-poly collision (players snag on leaves, physics cost explodes): trunk-only capsule/UCX. Pinned in Task 1 catalog test.
3. Landscape grass must not grow on paths, camp clearings, village square or through building floors (dirt layer and building footprints). Pinned in Task 2 weightmap test (dirt under footprints) + tour shot review.
4. Wading: the camera at bank depth must not flip into the underwater post-process while the camera is above the water surface. Pinned by the Task 3 riverbank tour shot.
5. A re-run of the fetch after an interrupted download must resume, not re-download 10 GB or keep a truncated file. Pinned in Task 1 fetch test.

---

### Task 1 (R1): Asset pipeline

**Files:**
- Create: `Scripts/world/fetch_polyhaven.py`, `Scripts/world/polyhaven_assets.json`, `Scripts/world/testdata/polyhaven_rock_07_files.json` (recorded from `https://api.polyhaven.com/files/rock_07`)
- Modify: `Scripts/world/import_library.py`, `Scripts/world/library_catalog.json` (generated), `Scripts/world/test_layout.py`, `.gitignore`, `docs/world/asset-credits.md`

**Interfaces:**
- Produces: `polyhaven_assets.json` = `{"models": {"<id>": {"category": "Tree|Shrub|Grass|Rock|Cliff|Prop", "res": "2k"}}, "textures": {"<id>": {"res": "4k"}}}` (ids from spec §2.1–2.3, §3.2); `fetch_polyhaven.py` downloads each model's glTF (`files[id]["gltf"][res]`, including its `include` textures) and each texture's albedo/normal (GL)/roughness/displacement PNG into `SourceArt/World/PolyHaven/<id>/`; functions `plan_downloads(files_json: dict, res: str, kind: str) -> list[tuple[url, rel_path, size]]`, `needs_download(path: str, size: int) -> bool`.
- Produces: catalog categories `PH_Tree`, `PH_Shrub`, `PH_Grass`, `PH_Rock`, `PH_Cliff`, `PH_Prop`, `Mega_Tree`, each entry `{"mesh": path, "collision": "trunk|kdop|none|complex|box"}`; PBR material instances `/Game/Shinobi/World/Library/Materials/MI_PBR_<texture_id>` from parent `M_PBRLayer` (params `Albedo`, `Normal`, `Roughness`, `Tiling`).

- [ ] **Step 1: Tests** in `test_layout.py`:
  - `test_fetch_plan`: feed a recorded sample of the API `files` JSON for `rock_07` (saved at `Scripts/world/testdata/polyhaven_rock_07_files.json`) to `plan_downloads(..., "2k", "model")`; assert the `.gltf` plus every `include` texture is listed with its size; `needs_download(existing_file, same_size)` is False, with a truncated file (size-1) True.
  - `test_catalog_coverage`: every key `make_placements.build()` emits exists in `library_catalog.json`; every `PH_Tree`/`Mega_Tree` entry has `collision == "trunk"`.
- [ ] **Step 2:** Run `python Scripts/world/test_layout.py` → the two new tests fail.
- [ ] **Step 3:** Write `fetch_polyhaven.py` (stdlib `urllib`, User-Agent `ShinobiCoop-dev`, skip files whose size matches, write to `.part` then rename) and `polyhaven_assets.json`; add `SourceArt/World/PolyHaven/` to `.gitignore`. Run it; record total size.
- [ ] **Step 4:** Extend `import_library.py`: sources for the `PH_*` categories from `SourceArt/World/PolyHaven/<id>/*.gltf`; Nanite on (foliage: Nanite with masked materials preserved); collision: trees = one capsule from the trunk (lowest 20% of bounds, radius from that slice), rocks/cliffs = kdop (cliffs complex-as-simple), grass/shrubs = none, props = box; keep the glTF's PBR materials but remap to `MI_PBR_*` when a texture id matches. Build `M_PBRLayer` + `MI_PBR_<id>` for every texture in the list. Write the catalog.
- [ ] **Step 5 (user action):** Ask the user to add Megaplants Japanese Cypress, Norway Maple, Japanese Medlar to the ShinobiCoop project via Fab (launcher → Fab → Add to project). Then inspect what landed under `Content/Fab/`: if static meshes (or Nanite assemblies whose `UStaticMesh` HISM accepts) → catalog `Mega_Tree`; otherwise note the fallback (instanced foliage / per-tree actors) for Task 4. Generate trunk collision the same way.
- [ ] **Step 6:** `python Scripts/world/test_layout.py` → all pass (catalog coverage may still use old keys; it passes because placements are unchanged until Task 4).
- [ ] **Step 7:** Credits (Poly Haven CC0 list, Fab listings); commit "Realistic R1: Poly Haven fetch, PBR import, Megaplants".

### Task 2 (R2): Ground

**Files:**
- Modify: `Scripts/world/make_heightmap.py`, `Scripts/world/test_layout.py`, `Scripts/world/build_world.py` (terrain stage material + grass types), `Source/ShinobiCoop/Shinobi/World/ShinobiWorldBuilderLibrary.cpp` only if the new layer or Nanite landscape flag needs C++.
- Create: `/Game/Shinobi/World/M_LandscapeReal` (+ `MI_`), `/Game/Shinobi/World/Grass/LGT_Meadow`, `LGT_Forest`, `/Game/Shinobi/World/RVT_Ground`.

**Interfaces:**
- Consumes: `MI_PBR_*` textures (Task 1).
- Produces: `terrain.json` gains `"layers": ["grass","forest","dirt","rock","bank"]`; `weight_forest.png`; `make_heightmap.erode(h: np.ndarray, mask: np.ndarray, iterations: int, seed: int) -> np.ndarray` (mask 0 = protected).

- [ ] **Step 1: Tests** (`test_terrain` extended, plus new):
  - `test_erosion_protects_playable`: after `build`, max |Δh| vs a no-erosion build inside village radius, camp radii, boss radius and within 3 m of paths is < 0.05 m; elsewhere mean |Δh| > 0.2 m (erosion did something).
  - `test_weights_five_layers`: the five weightmaps sum to 255 at every pixel; `forest` > 128 at ≥ 60% of tree placements' pixels; `dirt` == 255 under every building footprint and camp clearing centre (grass can't spawn there).
- [ ] **Step 2:** Run → fail.
- [ ] **Step 3:** Implement: erosion (particle-based hydraulic, ~50k droplets, plus 20 thermal passes, talus 0.8), masked by a smooth protection field; rocky detail = high-frequency noise × slope; `forest` weight from a blurred tree-density field (tree positions are generated from the same seed, so `make_placements` exposes `tree_points(t, rng) -> list[(x, y)]` for reuse); buildings/camps force `dirt`.
- [ ] **Step 4:** Tests pass. Terrain stage: `M_LandscapeReal` (5 layers, height-blend by displacement, macro variation from a large noise texture, distance UV blend 1×/0.25×, RVT output), landscape Nanite on with displacement enabled, RVT volume covering the map; `LGT_Meadow` (`grass_medium_01/02`, `grass_bermuda_01`, dandelion; density 400/m²·scaled, end cull 8000 uu, scalability-aware) on `grass`, `LGT_Forest` (fern, moss, sparse grass) on `forest`. Re-run terrain (`-Render`), dress and mood stages.
- [ ] **Step 5:** Tour screenshots: ground reads photoreal, no tiling at medium distance, no grass on paths/in camps/through floors. Build, 64 tests, `smoke_m4.ps1`.
- [ ] **Step 6:** Commit "Realistic R2: eroded terrain, photoscanned landscape, landscape grass".

### Task 3 (R3): River

**Files:**
- Modify: `ShinobiCoop.uproject` (enable `Water` plugin), `Scripts/world/build_world.py` (new stage `water`), `Scripts/world/make_placements.py` (bank reeds and pebbles), `Scripts/world/test_layout.py`, `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp` (tour: riverbank point via a `Tour:Riverbank` marker).

**Interfaces:**
- Produces: stage `water` (idempotent, tag `ShinobiWater`): deletes the `RiverWater` plane, spawns `WaterZone` + `WaterBodyRiver` with spline points from `lay.river` at `water_level_m`, width = 2 × `RIVER_HALF_WIDTH_M` + 2 m, `affects_landscape` False, swimming disabled (no physics volume / `bCanBeUsedForSwimming`-equivalent off); placements `"bank"` items (reeds `PH_Grass` tall variants, `river_small_rocks`-style pebbles) within 0–3 m of the water edge.

- [ ] **Step 1: Test** `test_bank_dressing`: every bank item lies 0–3 m from the water edge, above `water_level_m - 0.3`, and none on the bridge footprint.
- [ ] **Step 2:** Fail → implement bank placements → pass.
- [ ] **Step 3:** Enable Water plugin, build, write the `water` stage; tune the river material instance (depth colour ramp clear→teal, foam, flow speed ~0.6 m/s along the spline). Fallback if spawning/rendering fails in the commandlet or at runtime: `M_WaterReal` on the old plane (panning flow normals, depth fade, foam by depth, same ramp) and record the deviation.
- [ ] **Step 4:** Tour incl. `Riverbank` and `Bridge`: waves/flow visible, player wades (walks on the bed), camera above water shows no underwater tint. 64 tests, `smoke_m4.ps1`.
- [ ] **Step 5:** Commit "Realistic R3: Water plugin river and banks".

### Task 4 (R4): Dressing

**Files:**
- Modify: `Scripts/world/make_placements.py` (catalog keys, densities, cliff ring), `Scripts/world/blender/make_gap_pieces.py` (detail + PBR slot names), `Scripts/world/import_library.py` (palette key → `MI_PBR_*` map), `Scripts/world/build_world.py` (Megaplant fallback path if Task 1 found one), `Scripts/world/test_layout.py`.

**Interfaces:**
- Consumes: catalog categories (Task 1), `forest` layer (Task 2).
- Produces: `TREES = {"cypress": [...Mega_Tree], "maple": [...Mega_Tree], "medlar": [...], "pine": ["PH_Tree/pine_tree_01", ...], "fir": [...], "dead": ["PH_Tree/dead_tree_trunk", ...]}`; `SHRUBS`, `ROCKS`, `CLIFFS` lists of `PH_*` keys; placements `"cliffs"` ring at 455–470 m from centre; palette map in `import_library.py`: `Plaster→white_plaster_rough_01, Stone→japanese_stone_wall, RoofTile→grey_roof_tiles, RoofTileRed→clay_roof_tiles, WoodDark→wood_planks_grey, WoodLight→oak_wood_planks, Cloth/Rope→reed_roof_03` (Red, Paper, Black, ClothRed stay solid).

- [ ] **Step 1: Tests:** `test_placements` updated for new keys (catalog coverage from Task 1 now bites); `test_cliff_ring`: cliffs cover ≥ 95% of the edge perimeter (angular gaps < 5°) and none within 20 m of a path end.
- [ ] **Step 2:** Fail → update `make_placements.py` (tree mix by zone: cypress/maple/medlar in the village ring and mid-forest, pine/fir north and on ridges; fewer, larger trees than now; shrubs/rocks/dead trunks by `forest` density; `stone_fire_pit` replaces `Camp/campfire` dressing; stylised `Prop/*` removed or swapped for `PH_Prop`) → pass.
- [ ] **Step 3:** Blender: add bevels, rafters, tile rows, frames to `house_*`, `shrine`, `gate`, `wall_segment`, `ruin_*`, `bridge` (stone piers), `stall`, `tent`; UVs in metres (box-projected, 1 UV = 1 m) so PBR textures tile correctly. Re-export, re-import, re-dress, mood.
- [ ] **Step 4:** Tour (all points incl. new `Tour:Forest`, `Tour:Edge`, `Tour:Street` markers): nothing stylised left next to realistic assets, no floating/buried trees on the eroded terrain (placements already sample the new heights), buildings read as timber/plaster/tile. 64 tests, both smoke tests.
- [ ] **Step 5:** Commit "Realistic R4: Poly Haven and Megaplants dressing, PBR buildings, cliff ring".

### Task 5 (R5): Look

**Files:**
- Create: `/Game/Shinobi/Materials/M_OutlinePost`, `/Game/Shinobi/Materials/MF_Wind`, `Source/ShinobiCoop/Tests/ShinobiLookTests.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.cpp` (+ `ShinobiPlayerCharacter.cpp` for `HeroMesh`, `ShinobiEnemyCharacter.cpp`, `World/ShinobiVillager.cpp`), `Scripts/setup_m5.py` or a new `Scripts/setup_look.py` (swap post material in `Lvl_TrainingGrounds`, `Lvl_Combat`, `Lvl_ThirdPerson`), `Scripts/world/build_world.py` (`apply_mood` → post settings, volumetric fog, exposure), `Config/DefaultEngine.ini` (`r.CustomDepth=3`, staged alone), jutsu VFX material instances as needed.

**Interfaces:**
- Produces: `namespace ShinobiLook { constexpr uint8 StencilFriendly = 1; constexpr uint8 StencilEnemy = 2; void EnableOutline(UPrimitiveComponent* Mesh, uint8 Stencil); }` in `Shinobi/ShinobiLook.h`.

- [ ] **Step 1: Test** `ShinobiCoop.Look.CharactersWriteOutlineStencil`: spawn player, rogue ninja, villager in the test world; the visible mesh of each (`HeroMesh` for the player) has `bRenderCustomDepth` and stencil 1 / 2 / 1; the player's hidden Manny driver mesh has `bRenderCustomDepth == false`.
- [ ] **Step 2:** Build + run → fails.
- [ ] **Step 3:** Implement `ShinobiLook::EnableOutline`, call it from each class's BeginPlay/ApplyHero; `M_OutlinePost` (Sobel on CustomStencil, 1.5 px at 1080p scaled by resolution, fade 30–60 m, colour by stencil: `#141414` for 1, `#8a1a14` at 60% for 2); stage the ini line alone.
- [ ] **Step 4:** Test passes. Swap `M_ToonPost` → `M_OutlinePost` in all four maps; `apply_mood`: volumetric fog on (scattering 0.4, sun volumetric scattering 1.5), exposure min/max EV 8/13, bloom 0.5, film grain 0, chromatic aberration 0, filmic white balance 6000 K; `MF_Wind` in the PH grass/leaf materials (sway 3–8 cm, gust period ~6 s). Characters: roughness 0.7 cloth / 0.5 skin on the hero and enemy MIs.
- [ ] **Step 5:** `shot_chakra.ps1 -Jutsu` and `-Look` (Training Grounds) and `-Tour`: every jutsu reads (retune emissive/bloom where washed out or dull), outlines thin and only on characters, arenas not broken. 65 tests, both smoke tests.
- [ ] **Step 6:** Commit "Realistic R5: outline pass, realistic post and lighting, wind, VFX retune".

### Task 6 (R6): Handoff

**Files:**
- Modify: `Config/DefaultScalability.ini` (create if missing; foliage/shadow/GI groups for Medium/Low), this plan (progress notes + checklist).

- [ ] **Step 1:** `shot_chakra.ps1 -Perf` (window foreground) at Epic; if any point < 30 fps, trim (grass density/end cull, cliff/tree cull, shadow distance) until ≥ 30. Record numbers here.
- [ ] **Step 2:** Scalability: Medium = grass density ×0.5 / cull ×0.6 / Lumen medium; Low = grass ×0.25, VSM off → shadow maps, Lumen off. Spot-check Medium with `-Perf` + `-ExecCmds="scalability 1"`.
- [ ] **Step 3:** Final: 65 tests, both smoke tests, `test_layout.py`, full tour review.
- [ ] **Step 4:** User playtest checklist (2 players): the 8 open-world rows (open-world plan Task 7) plus: 9. the valley looks realistic and polished (ground, trees, grass, river, village, camps, boss ruins); 10. outlines only on characters and readable in the forest; 11. jutsu effects still read well.
- [ ] **Step 5:** After the user's OK: merge `feature/open-world` then `feature/realistic-world` into `main`, tag `open-world` and `realistic-world`.

## Progress notes
- **R1 (f9b4576):** Poly Haven fetch (about 3 GB, resumable) and PBR import: 173 Nanite model variants and 18 texture sets. Trees get trunk-capsule collision.
- **R2 (0968f5d):** eroded terrain, five height-blended photoscanned layers, landscape grass (dense meadow and forest floor).
  - Poly Haven foliage needs its separate alpha maps and its own material (`M_PBRFoliage`, default lit), and its pivots recentred.
  - The Nanite landscape was dropped: it rendered flat under the water.
- **R3 (be078e2):** river surface via the spec fallback, because the Water plugin rendered no surface in UE 5.8.
  - The surface is single-layer water with scrolling ripple normals.
  - Reeds and pebbles line the banks.
- **R4 (0355e6b):** Poly Haven trees, logs, rocks, bushes, camp props and the stone fire pit; a cliff ring on the mountain edge; Blender pieces with bevels, metric UVs and PBR materials.
  - Megaplants are pending: the launcher's project picker didn't list the moved project. Placements switch to `Mega_Tree` automatically once they're imported.
- **R5 (385bb7d):** character-only outline (stencil custom depth; `M_OutlinePost` in the valley and all arenas), realistic post, volumetric fog, foliage wind, Nanite PreserveArea.
- **R6:** performance.
  - Hardware ray-traced Lumen plus dense Nanite foliage ran at about 4–8 fps.
  - Software Lumen (`r.Lumen.HardwareRayTracing=False`) plus `r.Nanite.MaxPixelsPerEdge=2` brings Epic at 1080p to 94–145 fps across all 15 tour points, with no visible quality loss.
  - Medium/Low scalability tuning is skipped as unnecessary.
- **Tests:** 68 automation tests pass and both smoke tests pass. The 3 charge tests fail only because of parallel jutsu edits outside this branch, which the user said to leave alone.
