"""City Sample crowd Londoners for the PvP arenas: crowd markers (TargetPoints tagged ShinobiVillager, Barks:London,
Look:Crowd, Route:...), which AShinobiPvpGameMode spawns as ambient walkers (never targeted). UE Python commandlet, close the
GUI editor:
  $env:SHINOBI_ARENA = "Tower"   # WestEnd | Camden | Westminster | Tower | all (default)
  Scripts/run_python.ps1 Scripts/london/arena_crowd.py

London's city data (road lines, paving polygons) that make_city.py's pavement_route walks on is gone, so the pavements are
read back from the arena's ground chunks: every triangle in a G_ chunk's Paving slot is rasterised onto a CELL_M grid inside
the arena (its NavMeshBoundsVolume less MARGIN_M). Each walk starts on a paved cell at least SPACING_M from the others, runs
along the pavement's direction there (the main axis of the paved cells around it) in STEP_M steps while every cell on the
way is paved (so it never crosses a road), up to RUN_M, then back the same way, as London's pavement walks do.
An arena that already has crowd markers is left alone; delete them (and their __ExternalActors__ files) to rebuild.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import unreal
import build_london as bl

ARENAS = ("WestEnd", "Camden", "Westminster", "Tower")
COUNT = 16
CELL_M, STEP_M, RUN_M, SPACING_M, MARGIN_M = 2.0, 8.0, 100.0, 25.0, 6.0


def log(message):
    unreal.log(f"[arena_crowd] {message}")


def paved_cells(actors, box_min, box_max):
    """{(i, j): z cm} for the CELL_M cells inside the box whose centre lies on a Paving triangle of a ground chunk."""
    cell = CELL_M * 100
    cells = {}
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.static_mesh
            if not mesh or not mesh.get_name().startswith("G_"):
                continue
            slots = [str(s.material_slot_name) for s in mesh.static_materials]
            if "Paving" not in slots:
                continue
            xf = comp.get_world_transform()
            md = mesh.get_static_mesh_description(0)
            for poly in md.get_polygon_group_polygons(unreal.PolygonGroupID(slots.index("Paving"))):
                for tri in md.get_polygon_triangles(poly):
                    # get_triangle_vertices returns 3 junk IDs, then the real 3 (london-map memory)
                    a, b, c = (xf.transform_location(md.get_vertex_position(v)) for v in md.get_triangle_vertices(tri)[-3:])
                    i0, i1 = math.floor(max(min(a.x, b.x, c.x), box_min[0]) / cell), math.floor(min(max(a.x, b.x, c.x), box_max[0]) / cell)
                    j0, j1 = math.floor(max(min(a.y, b.y, c.y), box_min[1]) / cell), math.floor(min(max(a.y, b.y, c.y), box_max[1]) / cell)
                    det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y)
                    if abs(det) < 1e-6:
                        continue
                    for i in range(i0, i1 + 1):
                        for j in range(j0, j1 + 1):
                            x, y = (i + 0.5) * cell, (j + 0.5) * cell
                            l1 = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det
                            l2 = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det
                            if l1 >= 0 and l2 >= 0 and l1 + l2 <= 1:
                                cells[(i, j)] = l1 * a.z + l2 * b.z + (1 - l1 - l2) * c.z
    return cells


def walks(cells, seed):
    """Up to COUNT out-and-back pavement walks [(x, y, z) cm], starts SPACING_M apart."""
    cell = CELL_M * 100
    paved = lambda x, y: (math.floor(x / cell), math.floor(y / cell)) in cells
    rng = random.Random(seed)
    keys = sorted(cells)
    starts, routes = [], []
    for _ in range(4000):
        if len(routes) >= COUNT:
            break
        i, j = rng.choice(keys)
        x, y = (i + 0.5) * cell, (j + 0.5) * cell
        if any(math.hypot(x - sx, y - sy) < SPACING_M * 100 for sx, sy in starts):
            continue
        near = [((a - i) * cell, (b - j) * cell) for a in range(i - 3, i + 4) for b in range(j - 3, j + 4) if (a, b) in cells]
        if len(near) < 12:
            continue
        sxx, syy, sxy = (sum(p[0] * p[0] for p in near), sum(p[1] * p[1] for p in near), sum(p[0] * p[1] for p in near))
        angle = 0.5 * math.atan2(2 * sxy, sxx - syy)  # the paved patch's long axis
        line = []
        for sign in (-1, 1):
            run, s = [], 0.0
            while s + STEP_M <= RUN_M / 2:
                # every cell along the next step must be paved: a road between two pavements ends the run
                if not all(paved(x + sign * math.cos(angle) * (s + d) * 100, y + sign * math.sin(angle) * (s + d) * 100)
                           for d in [k * CELL_M for k in range(1, int(STEP_M / CELL_M) + 1)]):
                    break
                s += STEP_M
                px, py = x + sign * math.cos(angle) * s * 100, y + sign * math.sin(angle) * s * 100
                run.append((px, py, cells[(math.floor(px / cell), math.floor(py / cell))]))
            line = run[::-1] + [(x, y, cells[(i, j)])] if sign < 0 else line + run
        if len(line) < 4:
            continue
        starts.append((x, y))
        routes.append(line + line[-2:0:-1])
    return routes


def build(arena):
    world = bl.load_map(f"/Game/Shinobi/Maps/Lvl_Arena_{arena}")
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    if any("Look:Crowd" in [str(t) for t in a.tags] for a in actors):
        log(f"{arena}: already has crowd markers, left alone")
        return
    volume = next(a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume))
    o, e = volume.get_actor_bounds(False)
    m = MARGIN_M * 100
    cells = paved_cells(actors, (o.x - e.x + m, o.y - e.y + m), (o.x + e.x - m, o.y + e.y - m))
    routes = walks(cells, arena)
    for n, route in enumerate(routes):
        tag = "Route:" + "|".join(f"{x:.0f},{y:.0f},{z:.0f}" for x, y, z in route)
        if len(tag) > 1000:  # FName limit 1024
            raise RuntimeError(f"{arena}: route {n} tag too long ({len(tag)})")
        x, y, z = route[0]
        point = bl.EAS.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z + 50), unreal.Rotator(0, 0, 0))
        point.set_actor_label(f"Pedestrian_{n}")
        bl.tagged(point, f"Arena{arena}", "ShinobiVillager", "Barks:London", "Look:Crowd", tag, always_loaded=True)
    bl.save(world, f"/Game/Shinobi/Maps/Lvl_Arena_{arena}")
    lengths = [round(sum(math.dist(a[:2], b[:2]) for a, b in zip(r, r[1:])) / 200) for r in routes]
    log(f"{arena}: {len(cells)} paved {CELL_M:.0f} m cells, {len(routes)} walks (one-way m: {lengths})")


for name in ARENAS if os.environ.get("SHINOBI_ARENA", "all") == "all" else (os.environ["SHINOBI_ARENA"],):
    build(name)
