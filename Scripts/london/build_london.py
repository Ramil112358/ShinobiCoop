"""Builds Lvl_London (docs/superpowers/specs/2026-10-01-london-map-design.md) from SourceArt/World/London (make_city.py,
make_city_meshes.py) and the assets import_london.py made. UE Python commandlet via Scripts/run_python.ps1 (-Render for
the terrain stage: the landscape layer merge renders). Stage from SHINOBI_LONDON_STAGE (default "terrain"):

  terrain  new World Partition map; the landscape (flat street level, parks, river and canal beds) with M_LandscapeLondon
           and lawn grass; sun, sky, fog, clouds, post; the PlayerStart at the hub (tag Hub:<name>); world settings
  city     one StaticMeshActor per imported chunk mesh (City/B,K,G,W; vertices are in map space, actors at the origin)
  dress    street furniture, vehicles and trees as one AShinobiScatterActor per 128 m chunk; landmarks; camp, boss,
           villager and tour markers (AShinobiWorldGameMode's tag format); invisible walls round the playable edge
  mood     ambient sound: the city bed, the river along the Thames
Every actor a stage spawns carries its stage tag (LondonCity, LondonDress) and re-running the stage replaces them.
build_arenas.py reuses make_world, place_city, place_streets and place_landmarks with a chunk filter and its own map
(save and load_map take the map path; MAP is the default).
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "world"))
import build_world as bw  # noqa: E402
import import_library as lib  # noqa: E402
import look  # noqa: E402

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT, "SourceArt", "World", "London")
MAP = "/Game/Shinobi/Maps/Lvl_London"
ROOT = "/Game/Shinobi/London"
EAS = bw.EAS
LAYERS = {"Grass": ("leafy_grass", "grass_ground", 3.0), "Soil": ("forest_ground_04", "forest_leaves_02", 3.0),
          "Riverbed": ("river_small_rocks", "dry_riverbed_rock", 2.5)}
GRASS = {"LondonLawn": ("Grass", [("grass_bermuda_01_medium", 2600, 2), ("grass_medium_02", 1400, 3), ("dandelion_01", 20, 1)])}
# furniture key -> cull distance (cm); 0 = never culled
CULL = {"tree_plane": 0, "bus": 30000, "bus_stop": 15000, "lamp_modern": 15000, "lamp_heritage": 15000,
        "phone_box": 12000, "traffic_light": 12000, "road_barrier": 10000}
DEFAULT_CULL = 9000
TREE_HEIGHT_M = 14.0


def log(message):
    unreal.log(f"[build_london] {message}")


def uu(m):
    return m * 100.0


def load_json(name):
    with open(os.path.join(ART, name), encoding="utf-8") as f:
        return json.load(f)


_doomed = []  # external actor packages of actors this run destroyed


def save(world, map_path=None):
    map_path = map_path or MAP
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError(f"failed to save {map_path}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    # The commandlet's save leaves a destroyed actor's external package (OFPA) on disk, so World Partition would bring
    # it back on the next load: delete the files ourselves.
    removed = 0
    for package in _doomed:
        path = os.path.join(PROJECT, "Content", package[len("/Game/"):] + ".uasset")
        if os.path.exists(path):
            os.remove(path)
            removed += 1
    _doomed.clear()
    if removed:
        log(f"removed {removed} stale actor packages")


def load_map(map_path=None):
    map_path = map_path or MAP
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.load_level(map_path):
        raise RuntimeError(f"could not load {map_path} (run the terrain stage first)")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
    return world


def clear(world, tag):
    victims = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.actor_has_tag(tag)]
    for actor in victims:
        package = actor.get_outermost().get_name()
        if "/__ExternalActors__/" in package:
            _doomed.append(package)
    EAS.destroy_actors(victims)


def tagged(actor, *tags, always_loaded=False):
    actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
    if always_loaded:
        actor.set_editor_property("is_spatially_loaded", False)
    return actor


# ------------------------------------------------------------------------------------------------------------ terrain
def make_world(map_path, game_mode, fresh_material=True, layer_info=f"{ROOT}/LayerInfo", terrain_dir=ART):
    """A new World Partition map with London's ground and sky: the landscape from terrain_dir's height.png and
    weight_<layer>.png, centred on the origin (M_LandscapeLondon, rebuilt unless fresh_material is False and it exists;
    layer infos created in layer_info), sun, sky, fog, clouds, post; world settings game_mode (a class path) and
    kill_z. Returns the world, unsaved."""
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorAssetLibrary.delete_asset(map_path)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.new_level(map_path, True):
        raise RuntimeError(f"could not create World Partition map {map_path}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    weightmaps = {name: os.path.join(terrain_dir, f"weight_{name.lower()}.png") for name in LAYERS}
    material_path = f"{bw.WORLD_DIR}/M_LandscapeLondon"
    if fresh_material or not unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = bw.build_landscape_material("M_LandscapeLondon", LAYERS, GRASS)
    else:
        material = unreal.load_asset(material_path)
    landscape = unreal.ShinobiWorldBuilderLibrary.create_landscape(
        world, os.path.join(terrain_dir, "height.png"), material, weightmaps, layer_info, 4)
    if landscape is None:
        raise RuntimeError("landscape creation failed (see LogShinobiCoop)")

    sun = bw.spawn_class(unreal.DirectionalLight, (0, 0, uu(300)), label="Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    bw.spawn_class(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
    sky = bw.spawn_class(unreal.SkyLight, (0, 0, uu(80)), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    fog = bw.spawn_class(unreal.ExponentialHeightFog, (0, 0, 0), label="Fog")
    bw.apply_mood(sun, fog)
    # London: a softer, hazier afternoon than the valley's; the fog thins out over the rooftops.
    fog.component.set_editor_property("fog_density", 0.008)
    fog.component.set_editor_property("fog_height_falloff", 0.08)
    fog.component.set_editor_property("start_distance", 8000.0)
    sun.set_actor_rotation(unreal.Rotator(0, -30, 140), False)
    bw.spawn_class(unreal.VolumetricCloud, (0, 0, 0), label="Clouds")
    if not unreal.EditorAssetLibrary.does_asset_exist(look.MATERIAL):
        look.outline_material()
    post = bw.spawn_class(unreal.PostProcessVolume, (0, 0, 0), label=look.VOLUME_LABEL)
    look.apply_post(post)
    settings = world.get_world_settings()
    mode = unreal.load_class(None, game_mode)
    if mode is None:
        raise RuntimeError(f"no game mode class {game_mode}")
    settings.set_editor_property("default_game_mode", mode)
    settings.set_editor_property("kill_z", -3000.0)
    return world


def stage_terrain():
    meta = load_json("terrain.json")
    markers = load_json("placements.json")["markers"]
    world = make_world(MAP, "/Script/ShinobiCoop.ShinobiWorldGameMode")
    hub = markers["hub"]
    start = bw.spawn_class(unreal.PlayerStart, (uu(hub["x"]), uu(hub["y"]), uu(hub["z"]) + 120), yaw=0, label="HubStart")
    tagged(start, f"Hub:{hub['name']}")
    save(world)
    log(f"terrain: {MAP} ({meta['pixels']} px landscape), hub at {hub['name']}")


# --------------------------------------------------------------------------------------------------------------- city
def stage_city():
    world = load_map()
    clear(world, "LondonCity")
    count = place_city()
    check_axes()
    save(world)
    log(f"city: {count} chunk actors")


def place_city(keep=None, tag="LondonCity"):
    """One StaticMeshActor per imported chunk mesh City/<Kind>/<Kind>_<cx>_<cy> whose 128 m cell passes keep(cx, cy)
    (every chunk when keep is None). Returns the actor count."""
    count = 0
    for kind in ("G", "B", "K", "W"):  # X (hidden river-edge walls) is not placed: shinobi walk onto the water
        folder = f"{ROOT}/City/{kind}"
        for path in sorted(unreal.EditorAssetLibrary.list_assets(folder, recursive=True)):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                continue
            if keep is not None:
                _, cx, cy = str(data.asset_name).split("_")
                if not keep(int(cx), int(cy)):
                    continue
            mesh = data.get_asset()
            actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
            actor.set_actor_label(mesh.get_name())
            if kind == "K":
                actor.static_mesh_component.set_collision_profile_name("NoCollision")
            if kind == "W":
                actor.static_mesh_component.set_cast_shadow(False)
                bw.walkable_water(actor)
            # Always loaded: London is seen across kilometres (skyline, river, bridges), and Nanite streams the
            # detail anyway; streamed cells left the distant city empty with landmarks floating over the void.
            extra = [bw.WATER_SURFACE_TAG] if kind == "W" else []
            tagged(actor, tag, f"City:{kind}", *extra, always_loaded=True)
            count += 1
    return count


def check_axes():
    """The chunk meshes must land where make_city put them (+Y south): a wrong glTF axis conversion would mirror London."""
    with open(os.path.join(ART, "city.json"), encoding="utf-8") as f:
        city = json.load(f)
    b = next(b for b in city["buildings"] if b["zone"] == "play" and b["top"] > 15)
    path = f"{ROOT}/City/B"
    mesh_path = next((p for p in unreal.EditorAssetLibrary.list_assets(path, recursive=True) if p.split(".")[-1] == f"B_{b['chunk']}"), None)
    if not mesh_path:
        unreal.log_warning("[build_london] axis check skipped (chunk mesh not found)")
        return
    box = unreal.load_asset(mesh_path.split(".")[0]).get_bounding_box()
    cx, cy = (int(v) for v in b["chunk"].split("_"))
    ok = box.min.y > uu(cy * 128 - 400) and box.max.y < uu((cy + 1) * 128 + 400)
    log(f"axis check B_{b['chunk']}: y {box.min.y / 100:.0f}..{box.max.y / 100:.0f} m, expected near {cy * 128}..{(cy + 1) * 128} -> {'OK' if ok else 'MIRRORED'}")
    if not ok:
        raise RuntimeError("chunk meshes are mirrored in Y: fix the export/import axis conversion before dressing")


# -------------------------------------------------------------------------------------------------------------- dress
def mesh_loader():
    """mesh(key, variant=0) -> the london_catalog.json StaticMesh for a furniture or landmark key, loaded once, or None
    (the key is added to mesh.missing; a catalog path that does not load also goes to mesh.failed). mesh.catalog is the
    catalog."""
    with open(os.path.join(HERE, "london_catalog.json"), encoding="utf-8") as f:
        catalog = json.load(f)
    meshes, missing, failed = {}, set(), set()

    def mesh(key, variant=0):
        if key == "tree_plane" and catalog.get("_tree_variants"):
            options = catalog["_tree_variants"]
            path = options[variant % len(options)]["mesh"]
        else:
            path = catalog.get(key)
        if not path:
            missing.add(key)
            return None
        if path not in meshes:
            meshes[path] = unreal.load_asset(path.split(".")[0])
        if meshes[path] is None:
            missing.add(key)
            failed.add(path)
        return meshes[path]

    mesh.catalog, mesh.missing, mesh.failed = catalog, missing, failed
    return mesh


def stage_dress():
    placements = load_json("placements.json")
    world = load_map()
    clear(world, "LondonDress")
    mesh = mesh_loader()
    instances, chunk_count = place_streets(world, None, "LondonDress", placements, mesh)
    m = placements["markers"]
    landmarks = place_landmarks(world, None, "LondonDress", m, mesh)

    def point(x, y, z, label, *tags, always_loaded=True):
        p = EAS.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z + 50), unreal.Rotator(0, 0, 0))
        p.set_actor_label(label)
        return tagged(p, "LondonDress", *tags, always_loaded=always_loaded)

    for c in m["camps"]:
        point(uu(c["x"]), uu(c["y"]), uu(c["z"]), c["id"], "ShinobiCampSite", f"Camp:{c['id']}", f"Difficulty:{c['difficulty']}",
              f"Radius:{uu(c['r']) + 1500:.0f}", "Waves:" + ";".join(f"{w['ninjas']},{w['throwers']}" for w in c["waves"]))
    boss = m["boss"]
    point(uu(boss["x"]), uu(boss["y"]), uu(boss["z"]), "boss", "ShinobiCampSite", "Boss", f"Radius:{uu(boss['r']) + 1500:.0f}",
          f"Victory:LONDON CLEARED")
    for i, route in enumerate(m["villagers"]):
        x, y, z = route[0]
        point(uu(x), uu(y), uu(z), f"Londoner_{i}", "ShinobiVillager", "Barks:London",
              "Route:" + "|".join(f"{uu(px):.0f},{uu(py):.0f},{uu(pz):.0f}" for px, py, pz in route))
    for i, route in enumerate(m.get("pedestrians", [])):  # City Sample crowd Londoners
        x, y, z = route[0]
        point(uu(x), uu(y), uu(z), f"Pedestrian_{i}", "ShinobiVillager", "Barks:London", "Look:Crowd",
              "Route:" + "|".join(f"{uu(px):.0f},{uu(py):.0f},{uu(pz):.0f}" for px, py, pz in route))
    peds = m.get("pedestrians", [])
    if peds:  # a look at the crowd near the hub
        hx, hy = m["hub"]["x"], m["hub"]["y"]
        x, y, z = min((r[0] for r in peds), key=lambda p: (p[0] - hx) ** 2 + (p[1] - hy) ** 2)
        point(uu(x), uu(y), uu(z + 1.0), "Tour_Check_crowd", "Tour", "Tour:Check_crowd", "TourOffset:-250,150,700")
    for name, (x, y) in m["tour"].items():
        if not name.startswith("Landmark_"):  # landmarks get framed wide views below
            point(uu(x), uu(y), uu(0.15), f"Tour_{name}", "Tour", f"Tour:{name}")
    poses = {lm["id"]: lm for lm in landmark_poses(m)}
    for lm_id, ((ox, oy, oz), target_z) in WIDE_VIEWS.items():
        lm = poses.get(lm_id)
        if not lm:
            continue
        if lm_id == "piccadilly_lights" and "eros" in poses:  # from Eros, looking at the screen
            ox, oy = uu(poses["eros"]["x"] - lm["x"]), uu(poses["eros"]["y"] - lm["y"])
        point(uu(lm["x"]), uu(lm["y"]), target_z, f"Tour_Wide_{lm_id}", "Tour", f"Tour:Wide_{lm_id}",
              f"TourOffset:{ox:.0f},{oy:.0f},{oz:.0f}")
    street_views(placements, point)
    for lm_id in ("westminster_bridge", "tower_bridge", "london_eye"):  # straight-down checks of placement
        lm = poses.get(lm_id)
        if lm:
            point(uu(lm["x"]), uu(lm["y"]), 0, f"Tour_Top_{lm_id}", "Tour", f"Tour:Top_{lm_id}", "TourOffset:-100,0,18000")
    for key in ("phone_box", "bin", "post_box", "black_cab", "bus_stop"):  # close-ups of street furniture
        items = [it for it in placements["items"] if it["key"] == key]
        if items:
            it = min(items, key=lambda i: (i["x"] - m["hub"]["x"]) ** 2 + (i["y"] - m["hub"]["y"]) ** 2)
            a = math.radians(it["yaw"] + 60)
            point(uu(it["x"]), uu(it["y"]), uu(it["z"] + 1.0), f"Tour_Check_{key}", "Tour", f"Tour:Check_{key}",
                  f"TourOffset:{900 * math.cos(a):.0f},{900 * math.sin(a):.0f},300")
    hub = m["hub"]
    point(uu(hub["x"]), uu(hub["y"]), 0, "Tour_Top_overview", "Tour", "Tour:Top_overview", "TourOffset:-60000,-40000,60000")

    # Navigation: one bounds volume over the whole playable area (runtime navmesh is generated only around invokers,
    # the villagers and enemies), else Londoners have no path and stand still.
    ring = placements["edge_wall"]
    xs, ys = [p[0] for p in ring], [p[1] for p in ring]
    nav = EAS.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(uu((min(xs) + max(xs)) / 2), uu((min(ys) + max(ys)) / 2), uu(5)),
                                     unreal.Rotator(0, 0, 0))
    nav.set_actor_scale3d(unreal.Vector(uu(max(xs) - min(xs) + 20) / 200, uu(max(ys) - min(ys) + 20) / 200, uu(30) / 200))
    nav.set_actor_label("LondonNavBounds")
    tagged(nav, "LondonDress", always_loaded=True)
    # The commandlet doesn't auto-create the navmesh actor the editor would (game log: "Unable to find
    # RecastNavMesh instance"): EnsureNavMesh does (not tagged, so it survives re-runs). Generation is dynamic, around invokers (Config/DefaultEngine.ini).
    if unreal.ShinobiWorldBuilderLibrary.ensure_nav_mesh(world) is None:
        raise RuntimeError("no navigation data (see LogShinobiCoop)")
    # Invisible walls just inside the playable edge, 40 m tall.
    ring = placements["edge_wall"]
    wall_mat = bw.solid_material("MI_LondonEdgeWall", (0.5, 0.5, 0.5))
    for i, (a, b) in enumerate(zip(ring, ring[1:])):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        if length < 0.5:
            continue
        wall = bw.spawn_mesh("/Engine/BasicShapes/Cube.Cube", wall_mat, (uu((a[0] + b[0]) / 2), uu((a[1] + b[1]) / 2), uu(20)),
                             (length, 0.5, 40), yaw=math.degrees(math.atan2(dy, dx)), label=f"EdgeWall_{i}")
        wall.set_actor_hidden_in_game(True)
        wall.static_mesh_component.set_collision_profile_name("InvisibleWall")
        tagged(wall, "LondonDress", bw.NO_TRAVERSAL_TAG, always_loaded=True)
    save(world)
    log(f"dress: {instances} instances in {chunk_count} chunks, {landmarks} landmarks, {len(m['camps'])} camps, "
        f"{len(m['villagers'])} Londoners + {len(m.get('pedestrians', []))} crowd, {len(m['tour'])} tour points; no mesh for {sorted(mesh.missing)}")


def place_streets(world, keep=None, tag="LondonDress", placements=None, mesh=None):
    """Street furniture, vehicles and trees as one AShinobiScatterActor (Street_<cx>_<cy>) per 128 m chunk that passes
    keep(cx, cy) (every chunk when keep is None). Returns (instance count, chunk count)."""
    placements = placements or load_json("placements.json")
    mesh = mesh or mesh_loader()
    catalog = mesh.catalog
    chunks, meshes = {}, {}
    for it in placements["items"]:
        key = (math.floor(it["x"] / 128), math.floor(it["y"] / 128))
        if keep is not None and not keep(*key):
            continue
        variant = int((it["x"] * 7.31 + it["y"] * 3.17)) & 0xff
        m = mesh(it["key"], variant)
        if m is None:
            continue
        scale = it["scale"]
        if it["key"] == "tree_plane" and catalog.get("_tree_variants"):  # street trees ~14 m whatever the source size
            entry = catalog["_tree_variants"][variant % len(catalog["_tree_variants"])]
            scale *= TREE_HEIGHT_M / max(entry["height_m"], 1.0)
        t = unreal.Transform(unreal.Vector(uu(it["x"]), uu(it["y"]), uu(it["z"])), unreal.Rotator(0, 0, it["yaw"]),
                             unreal.Vector(scale, scale, scale))
        meshes[m.get_path_name()] = m
        chunks.setdefault(key, {}).setdefault((m.get_path_name(), it["key"]), []).append(t)
    instances = 0
    for (cx, cy), groups in chunks.items():
        actor = EAS.spawn_actor_from_class(unreal.ShinobiScatterActor, unreal.Vector(uu(cx * 128 + 64), uu(cy * 128 + 64), 0),
                                           unreal.Rotator(0, 0, 0))
        actor.set_actor_label(f"Street_{cx}_{cy}")
        tagged(actor, tag)
        for (path, key), transforms in groups.items():
            actor.add_instances(meshes[path], transforms, CULL.get(key, DEFAULT_CULL))
        instances += actor.get_instance_count()
    return instances, len(chunks)


def place_landmarks(world, keep=None, tag="LondonDress", markers=None, mesh=None):
    """The landmark and skyline meshes at their final pose (landmark_poses), those whose 128 m cell passes keep(cx, cy)
    (all when keep is None). Returns the count."""
    markers = markers or load_json("placements.json")["markers"]
    mesh = mesh or mesh_loader()
    landmarks = 0
    for lm in landmark_poses(markers):
        if keep is not None and not keep(math.floor(lm["x"] / 128), math.floor(lm["y"] / 128)):
            continue
        lm_mesh = mesh(lm["id"])
        if lm_mesh is None:
            continue
        x, y, z, yaw = uu(lm["x"]), uu(lm["y"]), uu(lm.get("z", 0.15)), lm.get("yaw", 0)
        extra = []
        if lm["id"] == "piccadilly_lights":  # the marker is out in the Circus: wrap the screen round a building there
            extra = [f"ScreenFrom:{x:.0f},{y:.0f}"]  # for fix_piccadilly_screen.py
            wrap = screen_wrap(world, x, y, lm_mesh)
            if wrap:
                lm_mesh, (x, y, z), yaw = wrap[0], wrap[1], 0
            else:
                unreal.log_warning("[build_london] piccadilly_lights: no building face near its marker, left free-standing")
        actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
        actor.static_mesh_component.set_static_mesh(lm_mesh)
        actor.set_actor_label(f"Landmark_{lm['id']}")
        tagged(actor, tag, "Landmark", *extra, always_loaded=True)
        landmarks += 1
    return landmarks


SCREEN_WRAP = f"{ROOT}/Pieces/piccadilly_lights/SM_PiccadillyLights_Wrap"
SCREEN_LENGTH_M, SCREEN_BOTTOM_M, SCREEN_TOP_M = 38.0, 6.0, 17.5  # along the facade; above the shopfronts
SCREEN_RIGHT_M = 16.0  # carried on past the most-turning SCREEN_LENGTH_M, round the corner to the right
SCREEN_OUT_M, BEZEL_M = 0.6, 0.35  # clear of sills and string courses; dark border top and bottom


def ray_caster(world, x, y, r):
    """hit(a, b) -> (point, unit normal facing a) where the segment a -> b (cm tuples) first meets a City:B chunk
    triangle within r cm of (x, y), else None. Rays against the meshes' triangles (Moller-Trumbore), not traces: a map
    loaded in the commandlet has no mesh collision."""
    tris = []  # (bbox min, bbox max, p0, e1, e2); chunk vertices are in map space, the actors at the origin
    for chunk in unreal.GameplayStatics.get_all_actors_with_tag(world, "City:B"):
        origin, extent = chunk.get_actor_bounds(False)
        if abs(origin.x - x) > extent.x + r or abs(origin.y - y) > extent.y + r:
            continue
        # The source mesh: the render data of these Nanite meshes is a coarse fallback with whole walls missing.
        desc = chunk.static_mesh_component.get_editor_property("static_mesh").get_static_mesh_description(0)
        p = []
        for v in range(desc.get_vertex_count()):
            q = desc.get_vertex_position(unreal.VertexID(v))
            p.append((q.x, q.y, q.z))
        for t in range(desc.get_triangle_count()):
            # ponytail: UE 5.8's binding returns three junk IDs before the triangle's three vertices
            a, b, c = (p[i.id_value] for i in desc.get_triangle_vertices(unreal.TriangleID(t))[-3:])
            lo, hi = tuple(map(min, a, b, c)), tuple(map(max, a, b, c))
            if lo[0] > x + r or hi[0] < x - r or lo[1] > y + r or hi[1] < y - r:
                continue
            tris.append((lo, hi, a, (b[0] - a[0], b[1] - a[1], b[2] - a[2]), (c[0] - a[0], c[1] - a[1], c[2] - a[2])))

    def cross(u, v):
        return u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]

    def dot(u, v):
        return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]

    def hit(a, b):
        d =(b[0] - a[0], b[1] - a[1], b[2] - a[2])
        lo, hi = tuple(map(min, a, b)), tuple(map(max, a, b))
        best = None
        for tlo, thi, p0, e1, e2 in tris:
            if tlo[0] > hi[0] or thi[0] < lo[0] or tlo[1] > hi[1] or thi[1] < lo[1] or tlo[2] > hi[2] or thi[2] < lo[2]:
                continue
            h = cross(d, e2)
            det = dot(e1, h)
            if abs(det) < 1e-9:
                continue
            s = (a[0] - p0[0], a[1] - p0[1], a[2] - p0[2])
            u = dot(s, h) / det
            if not 0 <= u <= 1:
                continue
            q = cross(s, e1)
            v = dot(d, q) / det
            if v < 0 or u + v > 1:
                continue
            t = dot(e2, q) / det
            if 0 < t <= 1 and (best is None or t < best[0]):
                best = (t, e1, e2)
        if best is None:
            return None
        t, e1, e2 = best
        n = cross(e1, e2)
        length = math.sqrt(dot(n, n)) * (1 if dot(n, d) < 0 else -1)
        return (a[0] + d[0] * t, a[1] + d[1] * t, a[2] + d[2] * t), (n[0] / length, n[1] / length, n[2] / length)

    return hit


def facade_walk(hit, p, n, step, steps):
    """Points (x, y, nx, ny) along a building face from p (on it, outward normal n), steps of step cm (+ = to the right
    of someone facing the wall); stops where the face breaks (a corner, a recess, something jutting out). Each point is
    the median depth of rays at five heights, so window recesses and sills do not throw it."""
    out = []
    for _ in range(steps):
        t = (n[1], -n[0])
        qx, qy = p[0] + t[0] * step + n[0] * 300, p[1] + t[1] * step + n[1] * 300
        depths = []
        for z in (900, 1100, 1300, 1500, 1700):
            h = hit((qx, qy, z), (qx - n[0] * 700, qy - n[1] * 700, z))
            if h:
                depths.append(math.hypot(h[0][0] - qx, h[0][1] - qy))
        if len(depths) < 4:
            break
        d = sorted(depths)[len(depths) // 2]
        if not 150 < d < 450:  # the face jumps forward or falls back by 1.5 m
            break
        q = (qx - n[0] * d, qy - n[1] * d)
        tx, ty = q[0] - p[0], q[1] - p[1]
        length = math.hypot(tx, ty) * (1 if step > 0 else -1)
        nx, ny = n[0] - ty / length, n[1] + tx / length  # half the old normal, half the new run's: damps the noise
        length = math.hypot(nx, ny)
        p, n = q, (nx / length, ny / length)
        out.append((p[0], p[1], n[0], n[1]))
    return out


def screen_wrap(world, x, y, piece, reach_m=60.0):
    """Piccadilly Lights as in London: a screen that follows the face of a building near (x, y) cm round its corner,
    SCREEN_OUT_M proud of it (more where something sticks out), with a dark bezel and caps back into the wall so its
    back never shows. Of the faces within reach_m, the SCREEN_LENGTH_M stretch that turns the most wins (the curved
    corner), carried on SCREEN_RIGHT_M further to the right. Builds SCREEN_WRAP (with the piece's
    material slots), pivot on the ground under the middle, and returns (mesh, (x, y, z) cm for an unrotated actor), or
    None."""
    hit = ray_caster(world, x, y, uu(reach_m + SCREEN_LENGTH_M + 10))
    starts = []
    for i in range(72):
        a = math.radians(i * 5)
        h = hit((x, y, 1250), (x + uu(reach_m) * math.cos(a), y + uu(reach_m) * math.sin(a), 1250))
        flat = h and math.hypot(h[1][0], h[1][1])
        if flat and flat > 0.9 and all(math.dist(h[0][:2], s[0][:2]) > 500 for s in starts):
            starts.append((h[0], (h[1][0] / flat, h[1][1] / flat)))
    best = None
    for p, n in sorted(starts, key=lambda s: math.dist(s[0][:2], (x, y))):
        path = facade_walk(hit, p, n, -100, 60)[::-1] + [(p[0], p[1], n[0], n[1])] + facade_walk(hit, p, n, 100, 60)
        cum = [0.0]
        for a, b in zip(path, path[1:]):
            cum.append(cum[-1] + math.dist(a[:2], b[:2]))
        heading = [math.atan2(q[3], q[2]) for q in path]
        for i in range(1, len(heading)):  # unwrapped
            heading[i] += round((heading[i - 1] - heading[i]) / (2 * math.pi)) * 2 * math.pi
        j = 0
        for i in range(len(path)):
            while j < len(path) - 1 and cum[j] - cum[i] < uu(SCREEN_LENGTH_M):
                j += 1
            if cum[j] - cum[i] < uu(SCREEN_LENGTH_M) - 50:
                break
            turn = abs(heading[j] - heading[i])
            if best is None or turn > best[0] + 0.05:  # ties go to the face nearer the marker
                k = j  # then on round the corner to the right, as far as the face goes
                while k < len(path) - 1 and cum[k + 1] - cum[j] <= uu(SCREEN_RIGHT_M):
                    k += 1
                best = (turn, path[i:k + 1])
    if best is None:
        log(f"screen_wrap: no {SCREEN_LENGTH_M:.0f} m face among {len(starts)} near ({x / 100:.0f}, {y / 100:.0f}) m")
        return None
    turn, path = best
    # 5-point average, then normals from the smoothed run.
    pts = []
    for i in range(len(path)):
        near = path[max(0, i - 2):i + 3]
        pts.append((sum(q[0] for q in near) / len(near), sum(q[1] for q in near) / len(near)))
    norms = []
    for i in range(len(pts)):
        a, b = pts[max(0, i - 1)], pts[min(len(pts) - 1, i + 1)]
        length = math.dist(a, b)
        norms.append(((a[1] - b[1]) / length, (b[0] - a[0]) / length))
    mid = pts[len(pts) // 2]
    top = uu(SCREEN_TOP_M)
    for i in range(0, len(pts), 4):  # under the lowest roof along it
        rx, ry = pts[i][0] - norms[i][0] * 100, pts[i][1] - norms[i][1] * 100
        roof = hit((rx, ry, uu(80)), (rx, ry, 0))
        if roof:
            top = min(top, roof[0][2] - 100)
    bottom = uu(SCREEN_BOTTOM_M)
    # How far out the face must stand at each point: SCREEN_OUT_M, or 20 cm clear of anything (a corner the smoothing
    # cut, a pilaster) found by rays in from 3 m out at quarter steps and every 1.5 m up; then spread over +-2 points.
    clear = [uu(SCREEN_OUT_M)] * len(pts)
    for i in range(len(pts) - 1):
        for f in (0.0, 0.25, 0.5, 0.75):
            px, py = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * f, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * f
            nx, ny = norms[i][0] + (norms[i + 1][0] - norms[i][0]) * f, norms[i][1] + (norms[i + 1][1] - norms[i][1]) * f
            z = bottom + 50
            while z < top:
                h = hit((px + nx * 300, py + ny * 300, z), (px, py, z))
                if h:
                    need = 300 - math.dist(h[0][:2], (px + nx * 300, py + ny * 300)) + 20
                    clear[i] = max(clear[i], need)
                    clear[i + 1] = max(clear[i + 1], need)
                z += 150
    outs = [max(clear[max(0, i - 2):i + 3]) for i in range(len(pts))]
    outs = [sum(outs[max(0, i - 2):i + 3]) / len(outs[max(0, i - 2):i + 3]) for i in range(len(pts))]
    log(f"screen_wrap: {len(pts)} points, {math.degrees(turn):.0f} deg of turn, {bottom / 100:.1f}-{top / 100:.1f} m up, "
        f"{min(outs) / 100:.2f}-{max(outs) / 100:.2f} m out, middle ({mid[0] / 100:.1f}, {mid[1] / 100:.1f}) m")

    def at(i, out, z):  # out cm along the normal at point i (None: the screen face), z cm up; relative to the pivot
        out = outs[i] if out is None else out
        return pts[i][0] + norms[i][0] * out - mid[0], pts[i][1] + norms[i][1] * out - mid[1], z
    cum = [0.0]
    for a, b in zip(pts, pts[1:]):
        cum.append(cum[-1] + math.dist(a, b))
    front, back = None, -30.0
    s0, s1 = bottom + uu(BEZEL_M), top - uu(BEZEL_M)
    quads = []  # (corners, material slot, uvs); u runs left to right for someone facing the screen, v top to bottom
    for i in range(len(pts) - 1):
        u0, u1 = cum[i] / cum[-1], cum[i + 1] / cum[-1]
        quads.append(([at(i, front, s0), at(i + 1, front, s0), at(i + 1, front, s1), at(i, front, s1)], "Screen",
                      [(u0, 1), (u1, 1), (u1, 0), (u0, 0)]))
        for z0, z1 in ((bottom, s0), (s1, top)):
            quads.append(([at(i, front, z0), at(i + 1, front, z0), at(i + 1, front, z1), at(i, front, z1)], "Frame_Dark", None))
        for z in (bottom, top):
            quads.append(([at(i, back, z), at(i + 1, back, z), at(i + 1, front, z), at(i, front, z)], "Frame_Dark", None))
    for i in (0, len(pts) - 1):
        quads.append(([at(i, back, bottom), at(i, front, bottom), at(i, front, top), at(i, back, top)], "Frame_Dark", None))

    if unreal.EditorAssetLibrary.does_asset_exist(SCREEN_WRAP):
        mesh = unreal.load_asset(SCREEN_WRAP)
    else:  # the piece's material slots and settings
        mesh = unreal.EditorAssetLibrary.duplicate_asset(piece.get_path_name().split(".")[0], SCREEN_WRAP)
    desc = mesh.create_static_mesh_description(mesh)
    groups = {}
    for slot in ("Screen", "Frame_Dark"):
        groups[slot] = desc.create_polygon_group()
        desc.set_polygon_group_material_slot_name(groups[slot], slot)
    for corners, slot, uvs in quads:
        uvs = uvs or [(0, 0), (1, 0), (1, 1), (0, 1)]
        for order in ((0, 1, 2, 3), (3, 2, 1, 0)):  # ponytail: both windings, as the materials are one-sided
            ids = []
            for k in order:
                v = desc.create_vertex()
                desc.set_vertex_position(v, unreal.Vector(*corners[k]))
                vi = desc.create_vertex_instance(v)
                desc.set_vertex_instance_uv(vi, unreal.Vector2D(*uvs[k]), 0)
                ids.append(vi)
            desc.create_triangle(groups[slot], [ids[0], ids[1], ids[2]])
            desc.create_triangle(groups[slot], [ids[0], ids[2], ids[3]])
    mesh.build_from_static_mesh_descriptions([desc])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh, (mid[0], mid[1], 0.0)


BRIDGES = {"tower_bridge": "Tower Bridge", "westminster_bridge": "Westminster Bridge"}
# landmark -> (camera offset from it in cm, height in cm the camera looks at)
WIDE_VIEWS = {"london_eye": ((-21000, -9000, 2500), 7000), "elizabeth_tower": ((14000, -9000, 2500), 5500),
              "tower_bridge": ((-26000, -3000, 1800), 2500), "westminster_bridge": ((-6000, -14000, 2500), 0),
              "white_tower": ((-8000, 7000, 2500), 1500), "nelsons_column": ((-3500, 3500, 600), 2500),
              "eros": ((-1500, 1100, 250), 350), "piccadilly_lights": ((0, 0, 200), 1200)}
STREET_VIEWS = ["Hub", "Soho", "CamdenHighSt", "ParliamentSq", "ShadThames", "Camp_covent_garden", "Road_Whitehall",
                "Road_Strand", "Road_Victoria_Embankment"]


def street_views(placements, point):
    """Eye-level views along real streets: from the kerbside lamp nearest each named place, looking 25 m up the road."""
    lamps = [it for it in placements["items"] if it["key"] in ("lamp_modern", "lamp_heritage")]
    tour = placements["markers"]["tour"]
    for name in STREET_VIEWS:
        if name not in tour or not lamps:
            continue
        tx, ty = tour[name]
        lamp = min(lamps, key=lambda it: (it["x"] - tx) ** 2 + (it["y"] - ty) ** 2)
        side = math.radians(lamp["yaw"])
        road = math.radians(lamp["yaw"] - 90)
        x, y = lamp["x"] - 3.5 * math.cos(side), lamp["y"] - 3.5 * math.sin(side)  # step off the kerb into the road
        ahead = (x + 25 * math.cos(road), y + 25 * math.sin(road))
        point(uu(ahead[0]), uu(ahead[1]), uu(1.6), f"Tour_Street_{name}", "Tour", f"Tour:Street_{name}",
              f"TourOffset:{uu(x - ahead[0]):.0f},{uu(y - ahead[1]):.0f},{uu(0.6):.0f}")


def landmark_poses(markers):
    """Landmark markers with their final pose: bridges sit on their OSM deck line (centre, heading), the Eye's A-frame
    points east to the land, the Piccadilly screen faces Eros."""
    with open(os.path.join(ART, "city.json"), encoding="utf-8") as f:
        bridges = json.load(f)["bridges"]
    by_id = {lm["id"]: lm for lm in markers["landmarks"]}
    out = []
    for lm in markers["landmarks"] + markers["skyline"]:
        lm = dict(lm)
        if lm["id"] in BRIDGES:
            lines = [b["line"] for b in bridges if b["name"] == BRIDGES[lm["id"]]]
            if lines:
                pts = [p for line in lines for p in line]  # OSM splits a bridge into several ways
                (x0, y0), (x1, y1) = max(((a, b) for a in pts for b in pts), key=lambda ab: math.dist(*ab))
                lm.update(x=(x0 + x1) / 2, y=(y0 + y1) / 2, yaw=math.degrees(math.atan2(y1 - y0, x1 - x0)), z=0.0)
        elif lm["id"] == "london_eye":
            lm["yaw"] = -90.0  # model legs point local +Y (south in UE): turn them east
        elif lm["id"] == "piccadilly_lights" and "eros" in by_id:
            e = by_id["eros"]
            lm["yaw"] = math.degrees(math.atan2(e["y"] - lm["y"], e["x"] - lm["x"]))
        out.append(lm)
    return out


def stage_mood():
    """City bed everywhere (make_city_ambience.py) and the valley's river loop along the Thames, quieter."""
    import subprocess
    subprocess.run(["python", os.path.join(HERE, "make_city_ambience.py")], check=True)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(PROJECT, "SourceArt", "Audio", "World", "Amb_City.wav")
    task.destination_path = f"{ROOT}/Audio"
    task.automated = task.save = task.replace_existing = True
    bw.TOOLS.import_asset_tasks([task])
    city_wave = unreal.load_asset(f"{ROOT}/Audio/Amb_City")
    city_wave.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(city_wave)
    river_wave = unreal.load_asset(f"{bw.WORLD_DIR}/Audio/Amb_River")
    world = load_map()
    clear(world, "LondonMood")
    for fog in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog):
        fog.component.set_editor_property("fog_density", 0.008)
        fog.component.set_editor_property("fog_height_falloff", 0.08)
        fog.component.set_editor_property("start_distance", 8000.0)
    bw.ambient(world, city_wave, (0, 0, 0), 0.45, label="Amb_City").set_editor_property("tags", [unreal.Name("LondonMood")])
    with open(os.path.join(HERE, "london_layout.json"), encoding="utf-8") as f:
        lay = json.load(f)
    off = lay["origin_offset_m"]
    points = [(x + off[0], y + off[1]) for x, y in lay["river"]["line"]]
    points += [(lm["x"], lm["y"]) for lm in load_json("placements.json")["markers"]["landmarks"] if "bridge" in lm["id"]]
    for i, (x, y) in enumerate(points):
        if river_wave:
            a = bw.ambient(world, river_wave, (uu(x), uu(y), uu(-4)), 0.4, radius=uu(60), label=f"Amb_Thames_{i}")
            a.set_editor_property("tags", [unreal.Name("LondonMood")])
    save(world)
    log(f"mood: city bed + {len(points)} river sounds")


def stage_traversal():
    """Walls and water on the existing map: water chunks get exact collision that only Pawn hits, the hidden river-edge
    walls (City:X) go, and the playable-edge walls are marked unclimbable. Re-running is harmless."""
    for path in unreal.EditorAssetLibrary.list_assets(f"{ROOT}/City/W", recursive=True):
        mesh = unreal.EditorAssetLibrary.find_asset_data(path).get_asset()
        if isinstance(mesh, unreal.StaticMesh):
            lib.set_collision(mesh, "complex")
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    world = load_map()
    clear(world, "City:X")
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        add = None
        if actor.actor_has_tag("City:W"):
            bw.walkable_water(actor)
            add = bw.WATER_SURFACE_TAG
        elif actor.get_actor_label().startswith("EdgeWall_"):
            add = bw.NO_TRAVERSAL_TAG
        if add and not actor.actor_has_tag(add):
            actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(add)])
    save(world)
    log("traversal: water walkable, river-edge walls removed, edge walls unclimbable")


STAGES = {"terrain": stage_terrain, "city": stage_city, "dress": stage_dress, "mood": stage_mood, "traversal": stage_traversal}

if __name__ == "__main__":
    STAGES[os.environ.get("SHINOBI_LONDON_STAGE", "terrain")]()
