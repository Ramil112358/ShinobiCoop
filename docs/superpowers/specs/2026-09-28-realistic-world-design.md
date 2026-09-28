# ShinobiCoop: Realistic World Pass (Design)

**Date:** 2026-09-28. **Status:** approved in brainstorming, awaiting spec review. **Branch:** `feature/realistic-world`, stacked on `feature/open-world` (unmerged).

## 1. Goal

Make Hidden Leaf Valley look realistic and polished: its terrain, trees, grass, rocks, water, bridge, buildings and props, using Poly Haven CC0 assets, free Fab (Quixel Megaplants) assets the user adds, and Claude's Blender and UE pipeline.

The layout, camps, villagers and gameplay are unchanged. This is an art pass on the existing map, not a new map. If the user dislikes the result, both branches stay intact, so we can switch to another approach (in-engine PCG foliage, or a ready-made Fab environment).

**Decisions made in brainstorming:**

| Topic | Decision |
|---|---|
| Art direction | The whole game goes realistic (world, arenas, characters' lighting). The screen-wide cartoon post-process `M_ToonPost` is removed. Only a thin outline on characters remains, so the anime hero still reads. |
| Performance | Fidelity first. The floor is 30+ fps at Epic, host plus client, on the user's RTX 5080. Scalability presets carry weaker settings. This supersedes the open-world spec's 60/90 fps targets. |
| Sources | Poly Haven first (CC0, via its public API). Free Fab / Megascans for gaps; the user adds them through their Epic account. Paid packs are not assumed. |
| Approach | A (swap in place): keep the offline, deterministic, unit-tested placement pipeline and replace what it places. The dense grass carpet is the one exception: it moves to landscape grass. |

## 2. Assets

### 2.1 Trees

| Asset | Source | Use |
|---|---|---|
| Megaplants: Japanese Cypress | Fab (free, Fab Standard License) | village surroundings, mid-forest |
| Megaplants: Norway Maple | Fab | forest ring; leaf tint shifted to autumn red (the Leaf-village look) |
| Megaplants: Japanese Medlar | Fab | village gardens, small accents |
| `pine_tree_01`, `fir_tree_01`, `fir_sapling`, `fir_sapling_medium`, `pine_sapling_small` | Poly Haven | northern slopes, ridges, boss approach |
| `dead_tree_trunk`, `dead_tree_trunk_02`, `tree_stump_01/02`, `root_cluster_01/02` | Poly Haven | forest floor dressing, camps |

**Risk, checked first in R1:** Megaplants may ship as Nanite assemblies or skeletal foliage rather than plain static meshes. If `AShinobiScatterActor` (HISM) can't place them, trees use UE's instanced foliage (or one actor per tree for the few hundred trees), placed from the same offline transforms.

### 2.2 Undergrowth, rocks and cliffs (Poly Haven)

- **Shrubs and plants (offline scatter):** `shrub_01–04`, `fern_02`, `moss_01`, `nettle_plant`, `periwinkle_plant`, `dandelion_01`, `weed_plant_02`, and sparse flowers (`flower_heliophila`, `flower_gazania`).
- **Grass carpet (landscape grass, §3.3):** `grass_medium_01`, `grass_medium_02`, `grass_bermuda_01`.
- **Rocks:** `rock_moss_set_01/02`, `boulder_01`, `namaqualand_boulder_02–06`, `stone_01`, `rock_07`, `rock_09`.
- **Cliffs on the mountain edge:** `coastal_cliff_02`, `coastal_cliff_04`, `rock_face_01/02`. They also hide the invisible edge walls.

### 2.3 Buildings, gate, bridge and props

- **Default:** Claude's Blender pieces (`make_gap_pieces.py`) gain detail (bevels, rafters, tile rows, door and window frames) and switch from flat palette colours to Poly Haven PBR materials:

  | Material | Used for |
  |---|---|
  | `japanese_stone_wall` | plinths, wall ring, ruins |
  | `clay_roof_tiles`, `grey_roof_tiles` | roofs |
  | `white_plaster_rough_01` | walls |
  | `wood_planks_grey`, `oak_wood_planks` | timber, bridge deck (the bridge gets stone piers) |
  | `reed_roof_03`, `thatch_roof_angled` | camp tents and stalls |

  The palette keys stay as slot names, and the importer maps them to the PBR material instances.
- **Camps:** Poly Haven `stone_fire_pit`. The stylised Quaternius props are replaced or dropped. Anything left stylised must not stand next to realistic assets.
- **Paid packs:** a Fab Japanese village kit, if the user buys one, slots into the catalog the same way. It is not assumed.

### 2.4 Storage and credits

- Raw downloads live in `SourceArt/World/PolyHaven/<asset>/` (gitignored), fetched by `Scripts/world/fetch_polyhaven.py`. Fab content lives under `Content/Fab/...`, as added by the launcher.
- Imported assets are committed, as today. The repo grows by a few GB; there is no remote.
- Poly Haven assets (CC0) and the Fab listings are credited in `docs/world/asset-credits.md`.

## 3. Ground

### 3.1 Terrain shape

`make_heightmap.py` gains:
- a numpy erosion pass (hydraulic and thermal, deterministic), for gullies and ridges;
- small rocky detail on steep slopes.

It is masked so the village plateau, camp clearings, paths, river bed and boss arena keep their shape. The existing terrain tests continue to guard them.

### 3.2 Landscape material

Five layers, from Poly Haven 4k texture sets (albedo, normal, roughness, displacement):

| Layer | Texture | Weightmap |
|---|---|---|
| Grass | `leafy_grass` (`grass_ground` variation) | existing `grass` |
| Forest floor | `forest_leaves_02` (`forest_ground_04` variation) | **new** `forest`, from tree density near the offline tree placements |
| Dirt path | `stony_dirt_path` (`rocky_trail` variation) | existing `dirt` |
| Rock | `rocky_terrain` (`rock_face` variation) | existing `rock` |
| Riverbank | `river_small_rocks` (`dry_riverbed_rock` variation) | existing `bank` |

Features:
- the layers blend by height map;
- large-scale colour variation and distance-based UV scaling hide tiling;
- Nanite landscape with displacement from the height maps;
- a runtime virtual texture, so rock and tree bases blend into the ground colour.

Weightmaps still sum to 255 per texel.

### 3.3 Landscape grass

- `ULandscapeGrassType` assets on the grass and forest-floor layers.
- They spawn `grass_medium_*`, clover and small flowers on the GPU near each viewer, densely, fading out at about 80 m. Density and distance scale with the foliage scalability setting.
- Trees, rocks, shrubs and everything with collision stay in `make_placements.py`.

## 4. River

- **UE Water plugin.** One `WaterBodyRiver` spline through the layout's river points, at the constant water level and the carved width, plus a `WaterZone`.
- **Landscape editing is off,** since the offline heightmap already carves the bed.
- **Look:** flowing surface normals, shoreline foam, depth-based colour (clear at the banks, deep teal in the middle), Lumen reflections, and the underwater post-process.
- **Players wade.** Swimming stays off, so combat in water is out of scope.
- **Banks:** rocks, pebbles and reeds (tall grass variants) at the water's edge, from the offline placements.
- **Fallback** if the Water plugin misbehaves in UE 5.8 (experimental): the existing plane gets a custom water material with flow-map normals, depth fade, foam and the same colour ramp.

## 5. Look

### 5.1 Outline

- `M_ToonPost` comes out of every map (the open world, `Lvl_TrainingGrounds`, `Lvl_Combat`, `Lvl_ThirdPerson`) and is replaced by `M_OutlinePost`.
- Characters (`AShinobiCharacterBase` meshes, including `HeroMesh`, plus `AShinobiVillager`) render custom depth with stencil IDs: 1 for players and villagers, 2 for enemies.
- `M_OutlinePost` draws a thin edge (about 1.5 px, thinning with distance) only at stencil edges: dark for ID 1, a faint red rim for ID 2.
- Project setting: `r.CustomDepth=3`. The hunk is staged via `git hash-object` + `update-index`, never together with the user's map-line edit.
- The arenas keep their geometry and will look plainer than the valley; that is accepted for now.

### 5.2 Lighting and post

- Keep the warm late-afternoon sun, sky atmosphere and volumetric clouds.
- Lumen GI and reflections, virtual shadow maps, contact shadows.
- Volumetric fog with light shafts, and valley height fog.
- Auto-exposure clamped (about EV 8–13).
- A gentle filmic grade (slightly warm), mild bloom, no film grain or chromatic aberration.

### 5.3 Wind

- Megaplants use their built-in wind.
- Poly Haven grass and leaves use a shared material function (`MF_Wind`) for sway plus gusts.

### 5.4 Jutsu VFX and characters

- **VFX:** re-shoot with `shot_chakra.ps1 -Jutsu`. Retune the emissive strength and bloom of the mesh VFX that wash out or go dull under realistic exposure.
- **Characters:** keep their textures on lit materials, with sensible roughness for cloth and skin. No re-modelling; a realistic character pass would be a later sub-project.

## 6. Performance

- Measured each milestone with `shot_chakra.ps1 -Perf` (1080p, Epic, 10 s per point, window in the foreground), and reported.
- Floor: 30+ fps at Epic with host plus client.
- Scalability, Medium and Low:
  - landscape grass density and distance;
  - cull distances;
  - shadow quality;
  - Lumen quality.

## 7. Testing

| Check | What |
|---|---|
| Existing | the 64 automation tests, `smoke_m1.ps1`, `smoke_m4.ps1`, `test_layout.py` |
| New Python | erosion keeps the playable areas flat, dry and clear; the `forest` weightmap sums correctly with the others; every placed catalog key has an imported asset; the Poly Haven fetch is complete (file list and sizes from the API) |
| New C++ | `ShinobiCoop.Look.CharactersWriteOutlineStencil`: player, enemy and villager meshes render custom depth with stencil IDs 1, 2 and 1 |
| Visual | the tour gains close-up points (forest interior, riverbank, mountain edge, village street); Claude reviews every screenshot each milestone |
| User | the playtest (§8): the 8 open-world rows plus "the valley looks realistic and polished" |

## 8. Milestones

| # | Content |
|---|---|
| R1 | Asset pipeline: `fetch_polyhaven.py`; importer changes (Nanite, PBR materials from texture sets, collision by category); Megaplants check (the user adds them via Fab); catalog update. |
| R2 | Ground: erosion, the `forest` weightmap, the 5-layer landscape material, Nanite displacement, RVT, landscape grass. |
| R3 | River: Water plugin river, banks, reeds, pebbles (or the fallback material). |
| R4 | Dressing: trees, rocks and cliffs swapped in; Blender buildings, gate and bridge re-detailed and re-textured; camp props. |
| R5 | Look: outline pass and stencils, lighting and post, wind, VFX retune, arenas switched to the outline pass. |
| R6 | Handoff: perf numbers and scalability, final tour review, user playtest, merge `feature/open-world` then `feature/realistic-world` to main, tag `realistic-world`. |

## 9. Out of scope

- Realistic character models.
- Interiors.
- Swimming.
- Weather and day–night cycle.
- New gameplay.
- Replacing the IP-sensitive headband and cloak (still required before any public release).
