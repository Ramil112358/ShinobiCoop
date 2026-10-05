"""Builds the PvP arena levels (docs/superpowers/specs/2026-10-04-pvp-arena-design.md section 1) from Lvl_London's pieces
and arena_layout.json. UE Python commandlet, -Render for the landscape layer merge:
  $env:SHINOBI_ARENA = "Westminster"   # WestEnd | Camden | Westminster | Tower | all
  Scripts/run_python.ps1 -Render -TimeoutSec 3600 Scripts/london/build_arenas.py

Per arena a fresh World Partition map /Game/Shinobi/Maps/Lvl_Arena_<Id> (build_london.make_world: London's landscape,
sun, sky, fog, post; game mode AShinobiPvpGameMode, kill_z -3000) with:
  the chunk actors, Street_ scatter actors and landmarks of every 128 m cell that touches the rect (cells crossing the
  edge stay as backdrop behind the wall); a ring of invisible walls on the rect edge, 60 m tall (the dragon mount
  flies); PlayerStarts tagged Team:A / Team:B at the layout yaw; TargetPoints tagged Pickup:<Type>; a NavMeshBoundsVolume
  over the rect and the RecastNavMesh; Tour points for Scripts/shot_chakra.ps1 -Tour (top-down, team views, edges).
arena_layout.json is in raw map metres (origin_offset_m not applied); the chunk grid, placements.json and the meshes are
in offset-applied map metres, so every rect, start and pickup gets london_layout.json's origin_offset_m added here.
Every actor carries the stage tag Arena<Id>. chunk_touches, map_rect and cells are pure (test_arenas.py).
"""
import json
import math
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
try:
    import unreal
    import build_london as bl
except ImportError:  # the pure checks run without the editor
    unreal = bl = None

CHUNK_M = 128.0
GAME_MODE = "/Script/ShinobiCoop.ShinobiPvpGameMode"
WALL_TOP_M, WALL_BOTTOM_M, WALL_THICK_M = 60.0, -10.0, 1.0  # bottom below the riverbed (-7.5 m)
PICKUP_TYPES = ("Health", "Attack", "Speed", "Shield")
# Cells within this of the rect are kept too, so the view over a wall never ends at the wall: a rect edge that falls on a
# cell edge (Westminster's east wall is exactly x = -5 * 128) would otherwise drop the Thames and the bridge behind it.
BACKDROP_M = 30.0


def uu(m):
    return m * 100.0


def log(message):
    unreal.log(f"[build_arenas] {message}")


def load_arenas():
    with open(os.path.join(HERE, "arena_layout.json"), encoding="utf-8") as f:
        return json.load(f)["arenas"]


def origin_offset():
    with open(os.path.join(HERE, "london_layout.json"), encoding="utf-8") as f:
        return json.load(f)["origin_offset_m"]


def map_rect(rect, offset=None):
    """A raw arena_layout rect [x0, y0, x1, y1] in the offset-applied map metres of the chunk grid."""
    ox, oy = offset or origin_offset()
    return [rect[0] + ox, rect[1] + oy, rect[2] + ox, rect[3] + oy]


def chunk_touches(rect, cx, cy):
    """True when 128 m cell (cx, cy) overlaps rect (offset-applied metres); a cell that only shares an edge does not."""
    return rect[0] < (cx + 1) * CHUNK_M and rect[2] > cx * CHUNK_M and rect[1] < (cy + 1) * CHUNK_M and rect[3] > cy * CHUNK_M


def grow(rect, margin_m):
    return [rect[0] - margin_m, rect[1] - margin_m, rect[2] + margin_m, rect[3] + margin_m]


def cells(rect):
    """Every (cx, cy) that touches rect, x-major."""
    return [(cx, cy) for cx in range(math.floor(rect[0] / CHUNK_M), math.floor(rect[2] / CHUNK_M) + 1)
            for cy in range(math.floor(rect[1] / CHUNK_M), math.floor(rect[3] / CHUNK_M) + 1) if chunk_touches(rect, cx, cy)]


# ---------------------------------------------------------------------------------------------------------- the build
def trace_z(world, x, y, top_m, bottom_m):
    """(z in cm, actor label) of the first thing a downward line from top_m to bottom_m hits at (x, y) cm, or None."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, uu(top_m)), unreal.Vector(x, y, uu(bottom_m)),
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    loc, label = hit_info(hit)
    return loc.z, label


def hit_info(hit):
    """(location, actor label) of a HitResult."""
    fields = hit.to_tuple()
    # ponytail: FHitResult has no Python accessors; its first vector field is Location (FaceIndex, Time, Distance come first).
    loc = next(v for v in fields if isinstance(v, unreal.Vector))
    actor = next((v for v in fields if isinstance(v, unreal.Actor)), None)
    if actor is None:
        component = next((v for v in fields if isinstance(v, unreal.ActorComponent)), None)
        actor = component.get_owner() if component else None
    return loc, actor.get_actor_label() if actor else "?"


def ground_at(world, x, y, what):
    """Street-level z (cm) under a start or pickup; warns when something hangs over it or the ground is not street."""
    over = trace_z(world, x, y, 80, 4)
    if over:
        unreal.log_warning(f"[build_arenas] {what}: {over[1]} overhead at {over[0] / 100:.1f} m")
    under = trace_z(world, x, y, 3.5, -20)
    if under is None:
        unreal.log_warning(f"[build_arenas] {what}: no ground")
        return uu(0.15)
    if not -50 < under[0] < 150:
        unreal.log_warning(f"[build_arenas] {what}: ground at {under[0] / 100:.2f} m on {under[1]}")
    return under[0]


def check_ground(world, rect, step_m=10.0):
    """A downward trace every step_m inside the rect: misses are holes in the ground, low hits the river."""
    holes, low, n = [], 0, 0
    x = rect[0] + step_m / 2
    while x < rect[2]:
        y = rect[1] + step_m / 2
        while y < rect[3]:
            n += 1
            hit = trace_z(world, uu(x), uu(y), 3.5, -30)
            if hit is None:
                holes.append((round(x), round(y)))
            elif hit[0] < -100:
                low += 1
            y += step_m
        x += step_m
    log(f"ground check: {n} traces every {step_m:.0f} m, {len(holes)} holes {holes[:8]}, {low} below -1 m (river bed)")
    return holes


def check_cells(rect):
    """The B mesh of each kept cell must lie in that cell: proves the chunk naming against the geometry."""
    ok, bad = 0, []
    for cx, cy in cells(rect):
        path = f"{bl.ROOT}/City/B/B_{cx}_{cy}/StaticMeshes/B_{cx}_{cy}"
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        box = unreal.load_asset(path).get_bounding_box()
        mx, my = (box.min.x + box.max.x) / 200, (box.min.y + box.max.y) / 200
        if cx * CHUNK_M - 64 <= mx <= (cx + 1) * CHUNK_M + 64 and cy * CHUNK_M - 64 <= my <= (cy + 1) * CHUNK_M + 64:
            ok += 1
        else:
            bad.append((cx, cy, round(mx), round(my)))
    log(f"cell check: {ok} B meshes inside their cell, off: {bad}")
    if bad:
        raise RuntimeError("chunk meshes are not where their names say: the arena would cut the wrong cells")


def walls(rect, tag):
    """Four InvisibleWall boxes centred on the rect edge, overlapping at the corners."""
    material =unreal.load_asset(f"{bl.bw.WORLD_DIR}/MI_LondonEdgeWall")  # London's grey; reused, never rebuilt here
    x0, y0, x1, y1 = rect
    height, zc = WALL_TOP_M - WALL_BOTTOM_M, (WALL_TOP_M + WALL_BOTTOM_M) / 2
    sides = {"N": ((x0 + x1) / 2, y0, x1 - x0 + WALL_THICK_M, 0), "S": ((x0 + x1) / 2, y1, x1 - x0 + WALL_THICK_M, 0),
             "W": (x0, (y0 + y1) / 2, y1 - y0 + WALL_THICK_M, 90), "E": (x1, (y0 + y1) / 2, y1 - y0 + WALL_THICK_M, 90)}
    out = []
    for name, (x, y, length, yaw) in sides.items():
        wall = bl.bw.spawn_mesh("/Engine/BasicShapes/Cube.Cube", material, (uu(x), uu(y), uu(zc)), (length, WALL_THICK_M, height),
                                yaw=yaw, label=f"ArenaWall_{name}")
        wall.set_actor_hidden_in_game(True)
        wall.static_mesh_component.set_collision_profile_name("InvisibleWall")
        wall.static_mesh_component.set_cast_shadow(False)
        out.append(bl.tagged(wall, tag, always_loaded=True))
    return out


def check_walls(rect):
    """A pawn capsule (the player's and the dragon's collision: Pawn) swept from 10 m inside each wall to 10 m outside,
    at five spots per side, on the street and at 55 m: none may get outside; each side and height must have a probe
    stopped by the wall itself (others may stop on a building first)."""
    x0, y0, x1, y1 = rect
    passed, by_wall, other = [], {}, 0
    for z_m in (1.5, 55.0):
        for f in (0.1, 0.3, 0.5, 0.7, 0.9):
            x, y = x0 + f * (x1 - x0), y0 + f * (y1 - y0)
            for side, (a, b) in {"N": ((x, y0 + 10), (x, y0 - 10)), "S": ((x, y1 - 10), (x, y1 + 10)),
                                 "W": ((x0 + 10, y), (x0 - 10, y)), "E": ((x1 - 10, y), (x1 + 10, y))}.items():
                body = bl.EAS.spawn_actor_from_class(unreal.Character, unreal.Vector(uu(a[0]), uu(a[1]), uu(z_m)), unreal.Rotator(0, 0, 0))
                result = body.set_actor_location(unreal.Vector(uu(b[0]), uu(b[1]), uu(z_m)), True, False)
                end = body.get_actor_location()
                bl.EAS.destroy_actor(body)
                if not (x0 <= end.x / 100 <= x1 and y0 <= end.y / 100 <= y1):
                    passed.append(f"{side}@{z_m:.0f}m f{f}")
                    continue
                hits = [r for r in (result if isinstance(result, tuple) else (result,)) if isinstance(r, unreal.HitResult)]
                label = hit_info(hits[0])[1] if hits else "?"
                if label == f"ArenaWall_{side}":
                    by_wall[(side, z_m)] = by_wall.get((side, z_m), 0) + 1
                else:
                    other += 1
    log(f"wall check: {len(passed)} probes got outside {passed}; stopped by the wall {sorted(by_wall.items())}; {other} stopped by something else first")
    missing = [(s, z) for s in "NSWE" for z in (1.5, 55.0) if (s, z) not in by_wall]
    if missing:
        unreal.log_warning(f"[build_arenas] wall check: no probe stopped by the wall itself for {missing}")
    if passed:
        raise RuntimeError(f"arena wall check failed: probes got through at {passed}")


def tour_points(rect, starts, tag):
    """Camera points for shot_chakra.ps1 -Tour: top-down, the view from each team's side, and out over each wall."""
    cx, cy = (rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2

    def point(x, y, z, name, offset):
        p = bl.EAS.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(uu(x), uu(y), uu(z)), unreal.Rotator(0, 0, 0))
        p.set_actor_label(f"Tour_{name}")
        bl.tagged(p, tag, "Tour", f"Tour:{name}", "TourOffset:{:.0f},{:.0f},{:.0f}".format(*offset), always_loaded=True)

    point(cx, cy, 0, "Top", (-100, 0, 30000))
    point(cx, cy, 0, "Oblique", (uu(rect[0] - cx) * 0.9, uu(rect[3] - cy) * 1.4, 14000))
    for team, team_starts in starts.items():
        ax, ay = sum(s[0] for s in team_starts) / 4, sum(s[1] for s in team_starts) / 4
        point(cx, cy, 2, f"Side{team}", (uu(ax - cx), uu(ay - cy), 400))
    for side, (x, y, dx, dy) in {"N": (cx, rect[1], 0, -1), "S": (cx, rect[3], 0, 1), "W": (rect[0], cy, -1, 0),
                                 "E": (rect[2], cy, 1, 0)}.items():  # from 35 m inside, looking 60 m past the wall
        point(x + 60 * dx, y + 60 * dy, 3, f"Edge{side}", (uu(-95 * dx), uu(-95 * dy), 600))


def clean_old(map_path):
    """Delete a previous build of the map and its external actor/object packages (World Partition would load stale ones)."""
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorAssetLibrary.delete_asset(map_path)
    rel = map_path[len("/Game/"):]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    umap = os.path.join(bl.PROJECT, "Content", rel + ".umap")
    if os.path.exists(umap):  # delete_asset can leave the file behind; new_level then refuses the path
        os.remove(umap)
        registry.scan_files_synchronous([umap], True)
    for folder in ("__ExternalActors__", "__ExternalObjects__"):
        path = os.path.join(bl.PROJECT, "Content", folder, rel)
        if os.path.isdir(path):
            # ponytail: plain file removal plus a rescan; EditorAssetLibrary.delete_directory loads every actor first
            # and fails (an Error, so a non-zero commandlet exit) once their map is gone.
            shutil.rmtree(path, ignore_errors=True)
            registry.scan_paths_synchronous([f"/Game/{folder}/{rel}"], True)
            log(f"removed old {folder}/{rel}")


def build_arena(arena, offset):
    aid, map_path = arena["id"], arena["map"]
    tag = f"Arena{aid}"
    rect = map_rect(arena["rect"], offset)
    backdrop = grow(rect, BACKDROP_M)
    keep = lambda cx, cy: chunk_touches(backdrop, cx, cy)  # noqa: E731
    log(f"{aid}: rect {rect} m (offset {offset}), cells {cells(backdrop)} ({len(cells(rect))} touch the rect itself)")
    clean_old(map_path)
    # Own layer infos next to the map: re-creating London's would rewrite /Game/Shinobi/London/LayerInfo.
    world = bl.make_world(map_path, GAME_MODE, fresh_material=False, layer_info=f"{map_path}_LayerInfo")
    check_cells(backdrop)
    chunk_actors = bl.place_city(keep, tag)
    mesh = bl.mesh_loader()
    placements = bl.load_json("placements.json")
    instances, streets = bl.place_streets(world, keep, tag, placements, mesh)
    landmarks = bl.place_landmarks(world, keep, tag, placements["markers"], mesh)
    walls(rect, tag)

    ox, oy = offset
    starts = {team: [(x + ox, y + oy, yaw) for x, y, yaw in arena["starts"][team]] for team in "AB"}
    for team, team_starts in starts.items():
        for i, (x, y, yaw) in enumerate(team_starts):
            z = ground_at(world, uu(x), uu(y), f"start {team}{i}")
            start = bl.bw.spawn_class(unreal.PlayerStart, (uu(x), uu(y), z + 120), yaw=yaw, label=f"Start_{team}{i}")
            bl.tagged(start, tag, f"Team:{team}", always_loaded=True)
    for i, p in enumerate(arena["pickups"]):
        if p["type"] not in PICKUP_TYPES:
            raise ValueError(f"{aid}: unknown pickup type {p['type']}")
        x, y = p["x"] + ox, p["y"] + oy
        z = ground_at(world, uu(x), uu(y), f"{p['type']} pickup")
        point = bl.EAS.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(uu(x), uu(y), z), unreal.Rotator(0, 0, 0))
        point.set_actor_label(f"Pickup_{i}_{p['type']}")
        bl.tagged(point, tag, f"Pickup:{p['type']}", always_loaded=True)
    tour_points(rect, starts, tag)

    # Navigation for the bots: bounds over the rect (the cube brush is 200 uu, so scale = half-extent in m).
    nav = bl.EAS.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(uu((rect[0] + rect[2]) / 2), uu((rect[1] + rect[3]) / 2), uu(5)),
                                        unreal.Rotator(0, 0, 0))
    nav.set_actor_scale3d(unreal.Vector((rect[2] - rect[0]) / 2 + 5, (rect[3] - rect[1]) / 2 + 5, 15))
    nav.set_actor_label("ArenaNavBounds")
    bl.tagged(nav, tag, always_loaded=True)
    if unreal.ShinobiWorldBuilderLibrary.ensure_nav_mesh(world) is None:
        raise RuntimeError("no navigation data (see LogShinobiCoop)")

    holes = check_ground(world, rect)
    check_walls(rect)
    bl.save(world, map_path)
    log(f"{aid}: {map_path}: {chunk_actors} chunk actors, {instances} street instances in {streets} chunks, {landmarks} landmarks, "
        f"8 starts, {len(arena['pickups'])} pickups, {len(holes)} ground holes; no mesh for {sorted(mesh.missing)}")


def main():
    which = os.environ.get("SHINOBI_ARENA")
    arenas = load_arenas()
    ids = [a["id"] for a in arenas]
    if which != "all" and which not in ids:
        raise RuntimeError(f"set SHINOBI_ARENA to one of {ids} or all (got {which!r})")
    offset = origin_offset()
    for arena in arenas:
        if which in ("all", arena["id"]):
            build_arena(arena, offset)


if __name__ == "__main__":
    main()
