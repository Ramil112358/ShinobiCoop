# Open World (Sub-project #3) Implementation Plan

> **For agentic workers:** lean process per the user's standing preference: inline (native) execution, one light check per milestone (build + tests + screenshots), the user's playtest at the end. Steps use checkbox (`- [ ]`) syntax.

**Goal:** a roughly 1 km² World Partition valley (village, forest, river, 5 camps, boss ridge) that 2–4 players explore and clear with the existing combat.

**Architecture:**
- Python scripts turn one layout JSON into a heightmap and weightmaps (numpy). An editor-only C++ helper creates the landscape.
- UE Python places the village and camps; PCG scatters nature.
- Gameplay reuses the arena's spawn logic through a new `FShinobiEncounter`, driven by `AShinobiCamp` actors under a new `AShinobiWorldGameMode`.

**Tech stack:** UE 5.8 (C++ game module, Landscape, World Partition, PCG plugin, UE Python), Blender 5.2 headless, Python 3 + numpy (system Python) for the offline generators.

**Spec:** `docs/superpowers/specs/2026-09-27-open-world-design.md`

## Global Constraints

- The world is about 1 km × 1 km; the map is `/Game/Shinobi/Maps/Lvl_HiddenLeafValley` with World Partition.
- `Scripts/world/world_layout.json` is the single source for areas, paths, river, building slots, camps, the boss and villager routes.
- No Naruto IP in new content: an original village name, symbols and banners.
- Art is consistent and stylised. Every imported mesh gets the shared toon-ready material setup, and credits go in `docs/world/asset-credits.md` (source, licence, URL).
- CC0 downloads are scripted; Fab packs are added only by the user. Missing packs fall back to placeholders with a logged warning, and the build never fails for art.
- Cleared camps persist until the host restarts; there's no save file.
- A dead player respawns in the village after 10 s. A wipe respawns everyone in the village and resets camps in progress. A camp with no players inside for 30 s resets.
- Camp HP scales like the arena: Base × (1 + 0.5 × (players − 1)).
- Performance: 60+ fps at the highest preset (host plus 1 client, RTX 5080) in the village and forest.
- Existing tests and both smoke tests stay green. The Arena, Training Grounds and Combat Level stay playable.
- Standing rules: close the editor before builds and scripts; never stage the user's `GameDefaultMap`/`EditorStartupMap` lines in `Config/DefaultEngine.ini` (stage other ini hunks by index); screenshots only through in-engine harnesses.

## Review Focus

1. **Two players in different areas:** the server keeps both players' surroundings loaded, and a camp 700 m from the host still activates for the client standing in it. Test: `CampActivatesForAnyPlayer` (Task 5).
2. **A player disconnects inside an active camp:** the camp resets after 30 s like an empty camp, with no stuck enemies. Covered by `CampResetsWhenEmpty` via "no living player inside".
3. **A downed player inside a camp:** they don't count as present, so a camp with only downed players inside resets after 30 s. This is the same test with a downed player.
4. **Revive and talk in the same spot:** E next to both a downed ally and a villager revives. Test: `InteractPrefersRevive` (Task 6).
5. **Layout errors** (a camp outside the map, a duplicate camp id): the layout parser rejects them with a clear message before any build step runs. Test: `test_layout_rejects_bad_camps` (Task 1).

---

### Task 1 (W1): Layout, heightmap, weightmaps

**Files:**
- Create: `Scripts/world/world_layout.json`
- Create: `Scripts/world/layout.py` (parser and validation)
- Create: `Scripts/world/make_heightmap.py`
- Create: `Scripts/world/test_layout.py`

**Interfaces:**
- Produces: `layout.load(path) -> Layout`, with fields `size_m`, `areas{name: (x, y, r)}`, `paths[list[list[(x, y)]]]`, `river[list[(x, y)]]`, `buildings[list[{type, x, y, yaw}]]`, `camps[list[{id, x, y, r, difficulty, waves[list[{ninjas, throwers}]]}]]`, `boss{x, y, r}`, `villagers[list[list[(x, y)]]]`. Coordinates are metres from the map centre, +X east, +Y north.
- Produces: `make_heightmap.py <out_dir>`, which writes `height.png` (16-bit, 1009 × 1009, about 1 m per pixel) and `weight_grass.png`, `weight_dirt.png`, `weight_rock.png`, `weight_bank.png` (8-bit, same size).

- [ ] **Tests (`test_layout.py`, plain asserts, run with `python Scripts/world/test_layout.py`):**
  - the shipped layout loads and has 5 camps plus a boss;
  - every camp lies inside ±450 m;
  - camp ids are unique;
  - `test_layout_rejects_bad_camps`: a camp at x=900 and a duplicated id both raise `ValueError` naming the camp;
  - the heightmap village plateau varies by less than 0.5 m;
  - the river-bed pixels sit lower than the ground beside them;
  - the path weightmap is 255 along a path centre.
- [ ] **Layout values:**
  - village at (0, −150), r 100;
  - river along x ≈ 250;
  - boss at (0, 380), r 40;
  - camps at difficulty 1–5, spread over the forest ring;
  - about 40 building slots;
  - 6 villager routes.
- [ ] **Heightmap:** fractal noise hills of 0–25 m, the flattened plateau, a 4 m carved river bed, path corridors eased by 1–2 m, the boss ridge raised about 30 m, and edge mountains of 80 m+ outside 450 m. Weightmaps come from paths, river banks and slope (rock above 30°).
- [ ] Commit.

### Task 2 (W1): Landscape helper and world map

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/World/ShinobiWorldBuilderLibrary.h/.cpp` (`#if WITH_EDITOR`)
- Create: `Scripts/world/build_world.py` (stage 1: map and landscape)
- Modify: `ShinobiCoop.Build.cs` (editor-only `Landscape`, `UnrealEd` dependencies)
- Modify: `ShinobiCoop.uproject` (PCG plugin)

**Interfaces:**
- Produces: `UFUNCTION(BlueprintCallable, Category="Shinobi|World") static ALandscape* UShinobiWorldBuilderLibrary::CreateLandscape(UWorld* World, const FString& HeightmapPng, FVector2D SizeMeters, float HeightRangeMeters, UMaterialInterface* Material, const TMap<FName, FString>& WeightmapPngs)`. It builds components sized for 1009 × 1009, imports the heights, assigns the material with layer infos (Grass, Dirt, Rock, Bank) and imports the weightmaps. It returns null and logs on any failure.
- Produces: `M_Landscape` (`Scripts/world/build_world.py`): 4 layers blended by the landscape layer weights, flat toon colours plus a subtle texture (Poly Haven, Task 3; flat colours until then).
- Produces: the map `Lvl_HiddenLeafValley` (World Partition), with sun, sky, fog, clouds, the toon post volume (reusing `setup_m5.add_toon_volume`), a PlayerStart in the village square, a river water plane mesh along the spline (stylised water material), a kill-Z volume and edge walls.

- [ ] Build; run `build_world.py --stage terrain`; screenshot with `shot_chakra.ps1 -Look -Map /Game/Shinobi/Maps/Lvl_HiddenLeafValley` (overview). The terrain must read correctly: plateau, river, ridge, mountains.
- [ ] Commit (W1 check: build, tests, screenshot).

### Task 3 (W2): Art library

**Files:**
- Create: `Scripts/world/fetch_assets.py` (downloads the CC0 packs into `SourceArt/World/<pack>/`; packs are gitignored, the credits file is committed)
- Create: `Scripts/world/import_library.py`
- Create: `Scripts/world/blender/make_gap_pieces.py`
- Create: `docs/world/asset-credits.md`

**Interfaces:**
- Produces: `/Game/Shinobi/World/Library/<Category>/SM_*`. Categories: Tree, Bush, Grass, Rock, House, Wall, Gate, Prop, Camp, Ruin. It also produces `Library/catalog.json` mapping each layout building type and scatter kind to mesh paths. The world build and the PCG graphs read only the catalog.
- Consistency pass: every mesh gets a `MI_Library_*` instance of `M_ShinobiSolid`'s textured variant (base-colour texture × palette tint), a real-world scale check (door ≈ 2.1 m, tree 8–15 m), simple collision if missing, and LODs if missing (Blender decimate at 50% / 25%).

- [ ] Fetch the CC0 sets:
  - Quaternius: Stylized Nature MegaKit, Medieval Village MegaKit, plus any Japanese/fantasy props available;
  - Kenney: props;
  - Poly Haven: 2–3 ground textures.
  Record the credits.
- [ ] Review coverage by rendering a contact sheet of the library through a Blender screenshot script. List the gaps, then:
  - make the gap pieces in Blender: torii, stone and paper lanterns, palisade segments, tents if missing;
  - and/or ask the user for specific free Fab packs, with exact names and links, only if the CC0 sets fall clearly short on the village houses.
- [ ] Import, run the consistency pass, and write the catalog. Commit.

### Task 4 (W3): Village, camps and nature dressing

**Files:**
- Modify: `Scripts/world/build_world.py` (stages `village`, `camps`, `nature`, `hlod`)
- Create: `Scripts/world/pcg_graphs.py` (builds `PCG_Forest`, `PCG_Rocks`, `PCG_Grass` if PCG graph scripting holds up; otherwise author them once in the editor and commit, recorded as a deviation)

**Interfaces:**
- Consumes: `Library/catalog.json`, the layout.
- Produces: placed village buildings from the layout slots. Camps get tents, palisade, campfire, crates, a torch, and a tagged `ShinobiCampSite` actor with `CampId` and `Difficulty` that Task 5's `AShinobiCamp` replaces or uses. The boss ruin gets a `ShinobiCampSite` with `bBoss`.
- Produces: PCG volumes per biome with exclusion from the layout (village radius, paths ±4 m, river ±6 m, camp radii, building footprints), and a path-clear check: sample every path spline every 2 m and fail if an actor or instance overlaps within 1.5 m.

- [ ] Run the stages, then screenshots along a camera route: a new `-ShinobiWorldTour` harness visits the village square, gate, forest path, river bridge, each camp and the boss ridge, one screenshot each.
- [ ] Iterate on density, palette and lighting until the tour reads well. HLOD build. Commit (W3 check).

### Task 5 (W4): Encounters, camps, world game mode

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/World/ShinobiEncounter.h/.cpp`
- Create: `Source/ShinobiCoop/Shinobi/World/ShinobiCamp.h/.cpp`
- Create: `Source/ShinobiCoop/Shinobi/World/ShinobiWorldGameMode.h/.cpp`
- Modify: `ShinobiArenaGameMode.*` (use the encounter)
- Modify: `Enemies/ShinobiEnemyCharacter.cpp` (a `UNavigationInvokerComponent`, generation radius 3000 uu / removal 3500 uu, so World Partition's runtime navmesh builds only near active enemies)
- Modify: `ShinobiGameState.h/.cpp` (`EShinobiArenaPhase::OpenWorld`, `CampsCleared`, `NumCamps`)
- Modify: `UI/ShinobiHUDWidget.*` (camps line)
- Test: `Source/ShinobiCoop/Tests/ShinobiWorldTests.cpp`

**Interfaces:**
- `struct FShinobiEncounter`:
  - `void Spawn(UWorld*, TSubclassOf<AShinobiEnemyCharacter>, int32 Count, TFunctionRef<FVector(int32 Slot)> Where, int32 NumPlayers)`
  - `int32 Remaining() const`
  - `void DespawnAll()`
  - `TFunction<void(AShinobiEnemyCharacter*)> OnEnemyDied`
  - `TFunction<void()> OnAllDead`
  Health scaling uses `AShinobiArenaGameMode::GetHealthScale`, which stays static.
- `AShinobiCamp`:
  - properties `CampId`, `Radius` (default 1500 uu), `Difficulty`, `Waves` (array of `{Ninjas, Throwers}`), `bBoss`;
  - `ECampState { Idle, Active, Cleared }`, replicated;
  - `FSimpleMulticastDelegate OnCleared`;
  - `static constexpr float ResetSeconds = 30.f`.
- `AShinobiWorldGameMode`:
  - `static constexpr float RespawnSeconds = 10.f`;
  - finds all camps at BeginPlay, sets `NumCamps` and counts `CampsCleared`;
  - the boss cleared → the "VALLEY CLEARED" banner (Victory phase);
  - a wipe → respawn all players in the village, and every Active camp resets.

- [ ] **Tests (a test world with hovering characters):**
  - `CampActivatesForAnyPlayer`: a camp at 700 m from player A activates when player B stands inside.
  - `CampClearsWhenAllWavesDie`: 2 waves spawn in sequence; state becomes Cleared and `OnCleared` fires once.
  - `CampResetsWhenEmpty`: no living player inside for 30 s (walked out, or only downed players inside) → enemies despawn and state is Idle.
  - `CampHealthScales`: a 2-player camp gives enemies 1.5× HP.
  - `WorldRespawnAfterTenSeconds`: a dead player respawns in the village after 10 s.
  - `WorldWipeResetsCamps`.
  - The arena tests stay green (encounter refactor).
- [ ] Commit.

### Task 6 (W4): Villagers, interaction, compass, map picker

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/World/ShinobiVillager.h/.cpp`
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiCompassWidget.h/.cpp` (or a section of the HUD widget)
- Modify: `Abilities/GA_Revive.*` (becomes "interact": revive first, else talk)
- Modify: `Online/ShinobiSessionSubsystem.cpp` (maps list: "Hidden Leaf Valley" first)
- Modify: `build_world.py` (stage `villagers`)
- Test: `ShinobiWorldTests.cpp`

**Interfaces:**
- `AShinobiVillager`:
  - `TArray<FVector> Route`;
  - `TArray<FText> Barks`;
  - `FText PickBark(int32 Seed) const`;
  - a local `ShowBark()` (overhead text, 3 s).
  It walks between route points by navmesh with 2–6 s idles, and it isn't an `AShinobiCharacterBase`, so it's never a combat target. It has a `UNavigationInvokerComponent` (2000 / 2500 uu).
- Interaction:
  - a downed ally within `UGA_Revive::Reach` wins;
  - else the nearest villager within 250 uu in front (60° cone) → a client-local `ShowBark()`.
- Compass: a 400 px bar at the top centre showing ±90° around the camera yaw. Icons: camps (red when Idle or Active, grey when Cleared), the village, the boss.

- [ ] **Tests:**
  - `InteractPrefersRevive`;
  - `VillagerBarkDeterministic` (the same seed gives the same line, and every line is reachable);
  - `VillagerNotTargetable` (`FindHostilesInRadius` ignores villagers);
  - the updated input test (E label unchanged).
- [ ] Commit (W4 check: tests, both smoke tests with Valley hosted, the world tour screenshots).

### Task 7 (W5): Polish and handoff

- [ ] Performance: `stat unit` / `stat gpu` capture in the village and forest via a `-ShinobiPerf` harness that logs the average frame time over 10 s at each tour point. Tune (foliage cull distance, shadow distance, HLOD, LOD bias) until the targets hold.
- [ ] Mood: time of day (warm late afternoon), height fog in the valley, the toon post-process retuned if needed; ambient audio loops (forest birds and wind, river, village chatter), synthesised by `make_sfx.py` additions or CC0 sounds (credited).
- [ ] Final: all tests, both smoke tests, the tour screenshots reviewed.
- [ ] User playtest checklist (2 players), appended here:
  1. host "Hidden Leaf Valley";
  2. explore all areas without getting stuck;
  3. clear each camp together (banner, blue torch, compass grey);
  4. boss and "VALLEY CLEARED";
  5. die → respawn in the village after 10 s; wipe inside a camp → village respawn and camp reset;
  6. talk to villagers with E;
  7. the frame rate feels smooth;
  8. the arenas still work from the map picker.
- [ ] Merge to main, tag `open-world`.
