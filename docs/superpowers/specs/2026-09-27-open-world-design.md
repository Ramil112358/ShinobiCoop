# ShinobiCoop Sub-project #3: Open World (Design)

**Date:** 2026-09-27. **Status:** approved in brainstorming, awaiting spec review.

## 1. Goal

A roughly 1 km² open world that 2–4 players explore together from a village, clearing enemy camps and finally a boss, using the existing combat slice (jutsu, enemies, downed and revive) and the hero character.

It must look good and consistent: stylised, cel-shaded, Naruto-inspired without copying Naruto's IP.

**Decisions made in brainstorming:**

| Topic | Decision |
|---|---|
| Core loop | Explore from the village and clear 5 camps, then the boss ridge. No XP or quests; those are #4 and will hook onto camps. |
| Size | About 1 km × 1 km, dense. |
| NPCs | Ambient villagers who walk and idle, and say a one-line bark on E. No branching dialogue (#4). |
| Art | A graphically good, consistent look from publicly available assets: CC0 libraries, plus free Fab packs the user adds. Gaps are filled and polished by Claude in Blender. |
| Persistence | Cleared camps stay cleared until the host restarts. There's no save file (#4). |
| Approach | A script-generated Landscape (through a C++ editor helper), PCG scatter for nature, and village and camps placed from a layout file, all on World Partition. |

**Naming.** The map is `Lvl_HiddenLeafValley` (working name). In game, the village gets an original name, not Konoha, and signs and banners use original symbols.

## 2. World layout

| Area | Location | Contents |
|---|---|---|
| Village | Centre-south plateau, about 200 m across | Wall and main gate, wooden houses with tiled roofs, market stalls, lanterns, a training yard with posts, a shrine with a torii. The PlayerStart is in the square. |
| Forest ring | Around the village | Dense stylised trees with big canopies, bushes, grass and rocks. Dirt paths lead to the camps and the ridge. |
| River valley | East | A shallow river players can walk through, a stone bridge, reeds and bank rocks. |
| Camps (5) | Clearings along the paths | Bandit camps with tents, palisades, campfires and crates. Difficulty rises with distance from the village. |
| Boss ridge | North, up a rocky slope | A ruined stone fortress and the Rogue Jonin arena. |
| Edge | Border | Mountains and cliffs, plus an invisible wall. A kill-Z plane teleports fallen players to the village. |

**Layout file.** One data file, `Content/Shinobi/World/WorldLayout.json` (a plain JSON source kept under `Scripts/world/`), is the single source for the world build and for exclusion zones:
- area centres and radii;
- path splines (point lists);
- the river spline;
- village building slots (type, position, yaw);
- camps (id, centre, radius, difficulty, enemy list);
- the boss camp;
- villager route points.

## 3. Art pipeline

**Sources (licences recorded in `docs/world/asset-credits.md`):**
- **CC0, downloaded by scripts:**
  - Quaternius (stylised nature, village and medieval kits);
  - Kenney (props);
  - Poly Haven (terrain textures).
- **Free Fab packs, added by the user:** Claude proposes 1–2 stylised Japanese / nature kits only where the CC0 sets fall short. The user adds them with Fab's "Add to Project"; Claude has no Epic login.

**Consistency pass (`Scripts/world/import_library.py`).** Imported meshes are:
- assigned instances of one shared toon-ready material (`M_ShinobiSolid` extended with a texture/atlas variant and palette tint), so mixed sources match;
- scaled to real-world size;
- given collision where missing, and LODs where missing (via Blender).

**Blender gap pieces (`Scripts/world/blender/`).** A torii, lanterns and palisade segments, plus any missing modular pieces, made to match the kit style.

**Landscape material.** A new toon-friendly `M_Landscape` with layers for grass, dirt path, rock and riverbank. It's painted from the layout (paths, river, slope-based rock) by weightmap import. `M_ToonPost` keeps supplying the bands and outlines.

**Review.** Every area is screenshotted and iterated on by Claude before the user's playtest.

## 4. Terrain and dressing

- **Heightmap generator** (`Scripts/world/make_heightmap.py`, plain Python + numpy). Noise hills, a flattened village plateau, a carved river bed and path corridors, a raised boss ridge and edge mountains. It outputs a 16-bit PNG plus the weightmaps.
- **Landscape creation** (`UShinobiWorldBuilderLibrary`, an editor-only C++ BlueprintFunctionLibrary in the game module under `#if WITH_EDITOR`). It creates the World-Partition landscape from the heightmap (`ALandscape::Import`), assigns `M_Landscape` and imports the weightmaps. It's called from `Scripts/world/build_world.py`.
- **Water.** A river plane or mesh along the river spline, with a stylised water material. The Water plugin is not required.
- **PCG** (graphs built by script, or authored once and committed if scripting them proves impractical).
  - Forest, bush, grass and rock scatter by biome masks: slope, distance to paths, buildings and river.
  - Exclusion zones come from the layout file.
- **Village and camps** are placed by `build_world.py` from the layout file.
- **HLOD** is built for distant forest and mountains.

## 5. Gameplay systems

| Unit | Responsibility |
|---|---|
| `FShinobiEncounter` | Extracted from `AShinobiArenaGameMode`: spawns an enemy list at points, scales HP by player count, tracks remaining enemies and a died callback. The arena uses it with unchanged behaviour. |
| `AShinobiWorldGameMode` | Spawns players in the village square. Downed, revive and bleed-out work as today. A dead player respawns in the village after 10 s. A wipe (no living player) respawns everyone in the village, and camps in progress reset. Uses `AShinobiGameState` (a new phase value `OpenWorld`, plus `CampsCleared` and `NumCamps`). |
| `AShinobiCamp` | Placed per layout camp. When a player enters its trigger radius (server), it spawns up to 2 small waves through `FShinobiEncounter`. Cleared when all its enemies die: banner, the camp torch turns blue, and it stays cleared until the host restarts. If no player is inside the radius or everyone is down, it resets after 30 s (enemies despawn). Spawn points are projected onto the navmesh, falling back to the camp centre. |
| Boss camp | An `AShinobiCamp` with the Rogue Jonin (its summon behaviour is unchanged). Clearing it shows the "Valley cleared" banner. |
| `AShinobiVillager` | A light replicated character (Quinn or Manny with a village tint) that walks and idles between 2–4 route points by navmesh. Not a combat target. |
| Interaction (E) | Revive when a downed ally is in range, else talk to the villager in front: a local floating bark picked from its lines. |
| Navigation | Runtime navmesh with navigation invokers on enemies and villagers. |
| HUD | A compass bar at the top with camps (red, grey when cleared), the village and the boss. "Camps cleared n/5" replaces the wave line in world mode. |
| Map picker | "Hidden Leaf Valley" is the first Host option; the arenas stay. |

**Networking.** Server-authoritative as today. World Partition streams around every player (the server keeps the areas near all players loaded; clients load around their own pawn). No new replication patterns.

## 6. Error handling

| Situation | Handling |
|---|---|
| An asset pack or Fab item is missing | Build scripts fall back to Blender placeholders or primitives with the same material and log what's missing; the world always builds. |
| PCG blocks paths or buildings | Exclusion zones from the layout; the build fails if a path spline is blocked. |
| An enemy spawns off the navmesh | Navmesh projection, falling back to the camp centre. |
| A client streams in late | Camps are server-driven and spawn only near players; gameplay never depends on client streaming. |
| A player falls out of the world | Kill-Z plane: teleport to the village. |

## 7. Performance

Targets on the user's RTX 5080 (host plus 1 client):
- 60+ fps at the highest preset in the village and forest, and 90+ at medium;
- instanced foliage (PCG), LODs on all scattered meshes, HLOD for distance.

## 8. Testing

- **Automation:**
  - camp activation, clearing, reset after 30 s, and HP scaling;
  - world-mode respawn after 10 s and the wipe reset;
  - villager bark selection;
  - E priority (revive before talk);
  - the layout parser;
  - `FShinobiEncounter` (arena tests stay green).
- **Smoke:** the M4 LAN smoke test hosts Hidden Leaf Valley; the client joins and sees the camps replicated.
- **Visual:** a screenshot harness flies a camera route (village, forest, river, each camp, boss ridge) for Claude's review loop.
- **Manual:** a 2-player playtest checklist in the plan.

## 9. Milestones (lean process)

| # | Milestone | Done when |
|---|---|---|
| W1 | Terrain | Landscape helper, heightmap, painted layers, World Partition map, river water |
| W2 | Art library | CC0 packs imported with the consistency pass, Blender gap pieces, the user's Fab pick, credits file |
| W3 | Dressing | Village and camps placed, PCG forest/rocks/grass, HLOD; screenshot review |
| W4 | Gameplay | Encounter refactor, camps, world game mode, villagers and barks, compass HUD, map picker |
| W5 | Polish | Performance pass, lighting mood and fog, ambient audio; tests, smoke and the user's playtest |
