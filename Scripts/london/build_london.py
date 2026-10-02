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
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "world"))
import build_world as bw  # noqa: E402
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


def save(world):
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
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


def load_map():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.load_level(MAP):
        raise RuntimeError(f"could not load {MAP} (run the terrain stage first)")
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
def stage_terrain():
    meta = load_json("terrain.json")
    markers = load_json("placements.json")["markers"]
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorAssetLibrary.delete_asset(MAP)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.new_level(MAP, True):
        raise RuntimeError(f"could not create World Partition map {MAP}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    weightmaps = {name: os.path.join(ART, f"weight_{name.lower()}.png") for name in LAYERS}
    material = bw.build_landscape_material("M_LandscapeLondon", LAYERS, GRASS)
    landscape = unreal.ShinobiWorldBuilderLibrary.create_landscape(
        world, os.path.join(ART, "height.png"), material, weightmaps, f"{ROOT}/LayerInfo", 4)
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

    hub = markers["hub"]
    start = bw.spawn_class(unreal.PlayerStart, (uu(hub["x"]), uu(hub["y"]), uu(hub["z"]) + 120), yaw=0, label="HubStart")
    tagged(start, f"Hub:{hub['name']}")
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/ShinobiCoop.ShinobiWorldGameMode"))
    settings.set_editor_property("kill_z", -3000.0)
    save(world)
    log(f"terrain: {MAP} ({meta['pixels']} px landscape), hub at {hub['name']}")


# --------------------------------------------------------------------------------------------------------------- city
def stage_city():
    world = load_map()
    clear(world, "LondonCity")
    count = 0
    for kind in ("G", "B", "K", "W", "X"):
        folder = f"{ROOT}/City/{kind}"
        for path in sorted(unreal.EditorAssetLibrary.list_assets(folder, recursive=True)):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                continue
            mesh = data.get_asset()
            actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
            actor.set_actor_label(mesh.get_name())
            if kind in ("K", "W"):
                actor.static_mesh_component.set_collision_profile_name("NoCollision")
            if kind in ("W", "X"):
                actor.static_mesh_component.set_cast_shadow(False)
            if kind == "X":
                actor.set_actor_hidden_in_game(True)
                actor.static_mesh_component.set_collision_profile_name("InvisibleWall")
            # Always loaded: London is seen across kilometres (skyline, river, bridges), and Nanite streams the
            # detail anyway; streamed cells left the distant city empty with landmarks floating over the void.
            tagged(actor, "LondonCity", f"City:{kind}", always_loaded=True)
            count += 1
    check_axes()
    save(world)
    log(f"city: {count} chunk actors")


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
def stage_dress():
    with open(os.path.join(HERE, "london_catalog.json"), encoding="utf-8") as f:
        catalog = json.load(f)
    placements = load_json("placements.json")
    world = load_map()
    clear(world, "LondonDress")
    meshes, missing = {}, set()

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
        return meshes[path]

    chunks = {}
    for i, it in enumerate(placements["items"]):
        variant = int((it["x"] * 7.31 + it["y"] * 3.17)) & 0xff
        m = mesh(it["key"], variant)
        if m is None:
            continue
        key = (math.floor(it["x"] / 128), math.floor(it["y"] / 128))
        scale = it["scale"]
        if it["key"] == "tree_plane" and catalog.get("_tree_variants"):  # street trees ~14 m whatever the source size
            entry = catalog["_tree_variants"][variant % len(catalog["_tree_variants"])]
            scale *= TREE_HEIGHT_M / max(entry["height_m"], 1.0)
        t = unreal.Transform(unreal.Vector(uu(it["x"]), uu(it["y"]), uu(it["z"])), unreal.Rotator(0, 0, it["yaw"]),
                             unreal.Vector(scale, scale, scale))
        chunks.setdefault(key, {}).setdefault((m.get_path_name(), it["key"]), []).append(t)
    instances = 0
    for (cx, cy), groups in chunks.items():
        actor = EAS.spawn_actor_from_class(unreal.ShinobiScatterActor, unreal.Vector(uu(cx * 128 + 64), uu(cy * 128 + 64), 0),
                                           unreal.Rotator(0, 0, 0))
        actor.set_actor_label(f"Street_{cx}_{cy}")
        tagged(actor, "LondonDress")
        for (path, key), transforms in groups.items():
            actor.add_instances(meshes[path], transforms, CULL.get(key, DEFAULT_CULL))
        instances += actor.get_instance_count()

    m = placements["markers"]
    landmarks = 0
    for lm in landmark_poses(m):
        lm_mesh = mesh(lm["id"])
        if lm_mesh is None:
            continue
        actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(uu(lm["x"]), uu(lm["y"]), uu(lm.get("z", 0.15))),
                                           unreal.Rotator(0, 0, lm.get("yaw", 0)))
        actor.static_mesh_component.set_static_mesh(lm_mesh)
        actor.set_actor_label(f"Landmark_{lm['id']}")
        tagged(actor, "LondonDress", "Landmark", always_loaded=True)
        landmarks += 1

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
        tagged(wall, "LondonDress", always_loaded=True)
    save(world)
    log(f"dress: {instances} instances in {len(chunks)} chunks, {landmarks} landmarks, {len(m['camps'])} camps, "
        f"{len(m['villagers'])} Londoners + {len(m.get('pedestrians', []))} crowd, {len(m['tour'])} tour points; no mesh for {sorted(missing)}")


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


STAGES = {"terrain": stage_terrain, "city": stage_city, "dress": stage_dress, "mood": stage_mood}

if __name__ == "__main__":
    STAGES[os.environ.get("SHINOBI_LONDON_STAGE", "terrain")]()
