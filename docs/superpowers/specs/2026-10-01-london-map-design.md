# London map (`Lvl_London`): design + plan

## Context

You want a London map in ShinobiCoop: a collage of real districts built from publicly available assets, with Claude generating the missing parts in Blender and UE.

**You said (2026-10-01):**
- Districts: West End, Camden Town, Tower Bridge + City, Westminster + South Bank.
- Era: modern day.
- Gameplay: the Hidden Leaf Valley loop (camps, then a boss, plus NPCs).
- Use a free ready-made city kit if one exists; otherwise the open-data build.

**Assumed (correct me at review):**
- Realistic look, like the realistic-world pass.
- 1:1 scale. The playable area is about 1.6 × 1.6 km (about 2.5× the valley).
- Vehicles are static. There is no traffic or crowd simulation; Londoners are the existing villager NPCs.
- No interiors, no swimming, and the Thames can't be entered.

**Research outcome, which drives the approach:**
- **The linked Cesium post is about Camden, New Jersey.** It used Google Photorealistic 3D Tiles. That route is rejected for gameplay:
  - Google's terms ban caching and extracting tiles, so it would stream online only, with keys for every player.
  - It looks melted at street level.
  - Cesium collision on a listen host only loads in the host's view, so clients elsewhere fall through.
  - It can't make a collage.
- **There is no free, licence-clean London kit:**
  - British City Pack (Fab): paid.
  - Victorian Street (Fab): wrong era.
  - AccuCities London tiles (CGTrader): non-commercial licence, untextured massing.
  - Epic City Sample: an American city.
  - Free Fab city packs: time-limited and generic.
  - Pirate mirrors (unityunreal, assetfreaks): never used.
- **Watch Dogs: Legion rips are excluded** (Ubisoft copyright and EULA).
- **So the build is offline from open data**, in the same offline, deterministic pipeline as Hidden Leaf Valley. It ships offline, gives collision everywhere, and works in co-op.

## Design

### Collage (1:1 scale, gaps between districts compressed)

```
 N   [CAM Camden]
      | Regent's Park-edge strip (stucco terraces, park)
     [WE West End]
      | Embankment (the Thames bends from flowing north to flowing east)
     [WST Westminster/South Bank] ~~Thames~~> [TWR Tower Bridge/City]
```

Four real cut-outs, each north-up. OSM content inside each rectangle is kept; a building crossing the edge is dropped whole.

| Cut | Real centre | Size | Must contain |
|---|---|---|---|
| WST | Westminster Bridge 51.5007, -0.1220 | 700×650 m | Parliament Sq, Palace + Elizabeth Tower, bridge, County Hall, London Eye, Jubilee Gardens |
| TWR | 51.5055, -0.0770 | 750×550 m | Tower of London, Tower Bridge, More London/Potters Fields, St Katharine Docks edge |
| WE | 51.5108, -0.1290 | 850×450 m | Piccadilly Circus, Leicester Sq, south Soho, Covent Garden piazza |
| CAM | 51.5405, -0.1445 | 450×450 m | High St, Camden Lock + market, Regent's Canal |

- **Connectors** are generated boulevards along the cut edges, so OSM roads clipped at an edge meet them through the road union.
- **Beyond the edge:**
  - a backdrop ring of OSM massing about 250 m outside the outer edges, with cheap facades;
  - skyline set pieces: Shard, St Paul's, Gherkin, Walkie-Talkie, BT Tower;
  - fog.
- **The edge itself:** "ROAD CLOSED" barriers and hoardings, with invisible walls behind.
- **Coordinates** live in `Scripts/london/london_layout.json`. A top-down `plan.png` is approved before any 3D work.

### Ground and water

- **Street level is flat (z 0).** The Landscape is used only for parks, planting beds and the river bed. Its layers are Grass, Soil, Gravel and Riverbed, built with the existing `CreateLandscape` and landscape grass.
- **Roads, pavements (12 cm kerbs), plazas and markings are Blender meshes** made from shapely unions, chunked at 128 m.
  - Markings: centre and lane lines, double yellows, zebra crossings with zigzags and Belisha beacons, and "LOOK RIGHT" / "LOOK LEFT".
- **The Thames:**
  - banks come from OSM water polygons in WST and TWR, plus the layout's bend;
  - water at −5 m, with embankment walls and parapets;
  - the surface is a water mesh with the existing `M_WaterReal`, not a full-map plane, because the canal sits at another level.
- **Regent's Canal and its lock** in CAM are built the same way.
- **Water edges and parapets get invisible blocking walls.**

### Buildings (the generator, the core art piece)

**Style per building** (`make_city.py`):
- Comes from the OSM tags `building`, `building:material` and `building:colour`, plus height and the district default.
- The styles:
  - `georgian_terrace`
  - `victorian_terrace`
  - `stucco`
  - `portland_stone`
  - `gothic` (Palace of Westminster only)
  - `warehouse`
  - `modern_glass`
  - `concrete_modern`
- **Height:** the `height` tag, else `building:levels` × 3.3 m, else the style default.

**Blender geometry:**
- Footprints are extruded (holes are supported through `tessellate_polygon`).
- Facades are split into floors and bays:
  - inset sash windows with frames and sills;
  - cornices and parapets;
  - chimney stacks on terraces;
  - slate or mansard roofs where tagged.
- **Shopfronts** on retail streets: fascia, glazing and awnings, with original shop names.
- **Modern buildings** get curtain walls.
- A random value per window (vertex colour) varies the blinds and interiors.
- Only street-facing walls get full detail. The backdrop ring uses texture-only windows.

**Assembly:**
- Merged per 128 m chunk; Nanite.
- Complex-as-simple collision.
- World-scale UVs.
- **Materials:** palette keys map to Poly Haven PBR instances through the existing `PBR_PALETTE` / `texture_set` path. Slots:
  - stock and red brick
  - stucco
  - sandstone (standing in for Portland stone)
  - slate
  - asphalt
  - paving
  - cobbles
  - concrete
  - granite kerbs

### Landmarks and props

| Item | Source |
|---|---|
| Elizabeth Tower | A Sketchfab CC-BY candidate, vetted by screenshot. Fallback: procedural |
| Palace of Westminster | The generator's gothic style on the OSM footprint, plus a procedural Victoria Tower |
| Westminster Bridge, London Eye, Shard, Tower Bridge, White Tower + walls, skyline pieces, Camden Lock bridge and gates, narrowboat, Piccadilly LED screens (original ads) | Blender procedural |
| Eros (Shaftesbury Memorial) | Sketchfab CC-BY, or Rodin |
| Street furniture: K6 phone box, pillar box, modern and heritage lamps, bollards, guardrails, traffic lights, bus stop + shelter, benches, bins, bike racks, signs, cabinets, barriers, hoardings, market stalls | Blender procedural (`make_london_pieces.py`), plus Poly Haven CC0 (hidden_alley, lighting) |
| Static vehicles: double-decker bus, black cab, cars, vans | Sketchfab CC-BY or CGTrader free. Fallback: Rodin |
| Trees (stand-ins for London planes) | The baked Megaplants Norway Maple (green tint) and Medlar, at OSM `natural=tree` positions and in street and square rows |

- **Downloads you assist with:**
  - **Sketchfab:** create an API token and put it in the env var `SKETCHFAB_TOKEN`; `fetch_models.py` then downloads the list. The token is never written to disk or logs.
  - **CGTrader:** manual download into `SourceArt/World/London/Downloads/`.
- **No TfL roundel or Underground logo, and no real brands.** Real place and street names are fine.

### Gameplay (logic in `AShinobiWorldGameMode` unchanged)

- **Markers use the existing tag formats:**
  - hub/spawn: Leicester Square;
  - camps, harder further from the hub: Covent Garden piazza (1), Camden Lock market (2), Jubilee Gardens (3), Potters Fields (4);
  - boss: the Tower of London inner ward;
  - about 12 Londoner routes on pavements.
- **Small C++ changes:**
  - a `Hub:<name>` tag on the PlayerStart and a `Victory:<text>` tag on the boss marker become two replicated FStrings on `AShinobiGameState` (defaults "Village" / "VALLEY CLEARED", so the valley is unchanged);
  - `Barks:London` on villager markers picks a London line bank in `AShinobiVillager`;
  - the map list gets "London" after Hidden Leaf Valley, so `smoke_m4.ps1` still hosts the first entry.

### Look and performance

- **Look:** reuse `apply_mood` and `look.apply_post` with a cooler London haze. City ambience (traffic bed, pigeons, river) is synthesized like `make_ambience.py`.
- **Floor: 30+ fps at Epic, host plus client.** How:
  - Nanite;
  - chunk merges;
  - HISM furniture through `AShinobiScatterActor`, with cull distances;
  - a cheap backdrop;
  - a World Partition HLOD build;
  - landmarks always loaded.

## Files

**New in `Scripts/london/`:**
- `london_layout.json`
- `fetch_osm.py` (stdlib; Overpass; caches to `SourceArt/World/London/osm/`, which is already gitignored)
- `make_city.py` (numpy + shapely). Parses and validates the layout, projects OSM to map metres, assigns styles, builds road unions and exclusions. Writes `city.json`, the heightmap and weightmaps, `placements.json` (markers, routes, tour points, furniture) and `plan.png`. UE reads only its JSON.
- `test_london.py` (plain asserts, like `test_layout.py`)
- `blender/make_city_meshes.py`: buildings, streets, embankments and water meshes, as chunk GLBs.
- `blender/make_london_pieces.py`: props and procedural landmarks, one GLB each, like `make_gap_pieces.py`.
- `fetch_models.py` (Sketchfab API)
- `build_london.py`, with stages `terrain | city | dress | water | mood` selected by `SHINOBI_LONDON_STAGE`.

**Modified:**
- `Scripts/world/build_world.py`: add a `__main__` guard so `build_london.py` can import its helpers; `build_landscape_material` takes layers and grass types as parameters.
- `Scripts/world/import_library.py`: a `london()` entry (`SHINOBI_IMPORT=london`, following the existing switch).
- `Scripts/world/polyhaven_assets.json`: London texture sets and urban props.
- `Scripts/shot_chakra.ps1`: `-Tour` honours `-Map`.
- C++:
  - `ShinobiSessionSubsystem.cpp`
  - `ShinobiGameState.h/.cpp`
  - `ShinobiWorldGameMode.cpp`
  - `ShinobiHUDWidget.cpp`
  - `ShinobiVillager.h/.cpp`
  - the tests in `Source/ShinobiCoop/Tests/`
- `docs/world/asset-credits.md`: OSM (ODbL, "© OpenStreetMap contributors"), the new Poly Haven ids and the CC-BY authors.
- The spec and plan get saved to `docs/superpowers/specs|plans/2026-10-01-london-map*.md`.

## Reuse

- `UShinobiWorldBuilderLibrary::CreateLandscape` and `BakeSkeletalToStatic` (`Source/ShinobiCoop/Shinobi/World/ShinobiWorldBuilderLibrary.h`).
- `build_world.py`:
  - `fresh_asset`, `spawn_class`, `spawn_mesh`
  - `apply_mood`, `water_material`
  - `load_map_fully`, `tag`, `marker`, `place`
  - `catalog_meshes`, `ambient`, `grass_types`
- `import_library.py`:
  - `import_file`, `set_collision`
  - `texture_set`, `pbr_parent`, `realistic_model`
  - `autumn_material` (leaf tint)
  - `Mega_Tree` catalog entries
- `fetch_polyhaven.py`, `contact_sheet.py`, `run_python.ps1`, `run_tests.ps1`, `smoke_m4.ps1`.
- `AShinobiScatterActor`.
- `AShinobiWorldGameMode::ReadMarker` / `ReadVillagerMarker`.
- `AShinobiPlayerController::RunWorldTourTest` (works on any map from `Tour` tags).

## Milestones

| # | Work | Done when |
|---|---|---|
| L0 | Branch `feature/london`; save the spec and plan to docs; `pip install shapely` | — |
| L1 Data | Layout, `fetch_osm`, `make_city`, tests, `plan.png` | Tests pass; **you approve `plan.png`** |
| L2 Generator | London Poly Haven materials; buildings, streets and water edges in Blender | A contact sheet per style and one full district render; **you approve the look** |
| L3 Landmarks/props | Procedural pieces, Sketchfab/CGTrader downloads, Rodin for gaps, credits | A contact sheet of landmarks and props, reviewed |
| L4 UE build | Import, `build_london` stages, World Partition, collision, nav | The map loads, it's walkable everywhere, and every tour screenshot is reviewed |
| L5 Gameplay | Markers, C++ tags, barks, map list, tests | Automation green; a LAN host and client see the camps, boss, Londoners and the "LONDON CLEARED" banner |
| L6 Polish/perf | Lighting, ambience, HLOD, scalability | 30+ fps at Epic at every tour point, host plus client; **your playtest** |

**Defer unless time allows:**
- giant Camden shop-sign sculptures (Rodin);
- red bus lanes;
- a rotating London Eye;
- Big Ben chimes.

## Verification

1. `python Scripts/london/test_london.py` checks:
   - the projection (the Elizabeth Tower node lands within 1 m of its expected map position);
   - height and style rules;
   - the road union produces kerbs;
   - no building sits in water or a landmark exclusion zone;
   - markers are inside the playable area;
   - every placed key exists in the catalog.

   Also run `python Scripts/world/test_layout.py`: the valley must be unaffected.
2. `plan.png` and the Blender contact sheets, reviewed by Claude and then by you.
3. `Scripts/run_python.ps1 Scripts/london/build_london.py` for each stage, with clean logs (editor closed).
4. `Scripts/run_tests.ps1`: every existing test plus the new tag and bark tests pass.
5. `Scripts/shot_chakra.ps1 -Tour -Map /Game/Shinobi/Maps/Lvl_London`: review every district, landmark and camp, and iterate.
6. `-Perf`: 30+ fps at Epic, host plus client.
7. `Scripts/smoke_m4.ps1` still passes. Then a manual LAN London session: the client joins and sees the camps replicated.

## Risks and fallbacks

| Risk | Fallback |
|---|---|
| Overpass times out | Cache each district's query; use a mirror |
| OSM heights are missing | Fall back to levels, then to style defaults |
| The generator looks wrong at street level | The review loop at L2 |
| Perf | Strip window geometry from non-street walls; shrink the backdrop |
| UE Python's `__name__` guard fails | Move the helpers to `Scripts/world/ue_common.py` |
| Commandlet timeouts | `run_python.ps1 -TimeoutSec` |
| Repo size | Imported assets are committed through LFS, about 1–3 GB, as before |

**Upgrade path:** if you later buy British City Pack, the generator can place its modules in place of procedural facades.

## Out of scope

- interiors;
- traffic and crowd simulation;
- Underground stations;
- day/night;
- swimming;
- real brands and TfL marks;
- assets licensed for non-commercial use only.

## Git hygiene

Work on `feature/london`. Never stage your own edits (`DefaultEngine.ini`, `.uproject`, `Megaplant_Library`, the Mixamo FBXs). Commit only when you say so.
