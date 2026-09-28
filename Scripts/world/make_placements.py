"""Every placed object of the open world, computed offline from the layout and terrain (sub-project #3, W3).

  python Scripts/world/make_placements.py [out.json]

Writes SourceArt/World/Terrain/placements.json (regenerated, not committed): buildings, the village wall ring and gate,
camp dressing and camp markers, the boss ruins and marker, the bridge, and the nature scatter chunked into 128 m cells
(so World Partition streams it). Positions are UE units (cm), z sampled from the same heightmap the landscape uses.
Mesh keys are "<Category>/<Name>" entries of Scripts/world/library_catalog.json. Deterministic (fixed seed).
Replaces the plan's PCG graphs: offline sampling is deterministic, reviewable and unit-testable (see test_layout.py).
"""
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as layout_mod  # noqa: E402
import make_heightmap as hm  # noqa: E402

SEED = 11
CHUNK_M = 128.0
WALL_RADIUS_M = 106.0
WALL_SEGMENT_M = 10.2
FOOTPRINT_M = hm.FOOTPRINT_M
# Close-up tour points (m): meadow grass, forest floor, mountain edge, village street.
TOUR_CLOSEUPS = {"Meadow": (40.0, -35.0), "Forest": (-125.0, -60.0), "Edge": (-400.0, 40.0), "Street": (20.0, -175.0)}
BUILDING_MESH = {"house_small": ["Arch/house_small"], "house_medium": ["Arch/house_medium", "Arch/house_round"],
                 "house_large": ["Arch/house_large"], "stall": ["Arch/stall"], "lantern": ["Arch/lantern"],
                 "gate": ["Arch/gate"], "shrine": ["Arch/shrine"], "torii": ["Arch/torii"], "well": ["Arch/well"],
                 "training_post": ["Arch/training_post"]}
TREES = {"common": [f"Tree/CommonTree_{i}" for i in range(1, 6)], "pine": [f"Tree/Pine_{i}" for i in range(1, 6)],
         "maple": [f"Tree/TwistedTree_{i}" for i in range(1, 6)], "dead": [f"Tree/DeadTree_{i}" for i in range(1, 6)]}
TREE_SCALE = {"common": (0.9, 1.3), "pine": (1.0, 1.5), "maple": (0.55, 0.8), "dead": (0.6, 0.85)}
GRASS = ["Grass/Grass_Common_Short", "Grass/Grass_Common_Tall", "Grass/Grass_Wispy_Short", "Grass/Grass_Wispy_Tall",
         "Grass/Clover_1", "Grass/Clover_2", "Grass/Fern_1"]
FLOWERS = ["Grass/Flower_3_Group", "Grass/Flower_4_Group", "Grass/Plant_1", "Grass/Plant_7", "Grass/Mushroom_Common"]
BUSHES = ["Bush/Bush_Common", "Bush/Bush_Common_Flowers"]
ROCKS = ["Rock/Rock_Medium_1", "Rock/Rock_Medium_2", "Rock/Rock_Medium_3"]
CAMP_PROPS = ["Prop/Barrel", "Prop/Crate_Wooden", "Prop/FarmCrate_Empty", "Prop/Bag", "Prop/WeaponStand", "Prop/Bench",
              "Prop/Stool", "Prop/Cauldron", "Prop/Rope_1"]


class Terrain:
    def __init__(self, lay):
        self.lay = lay
        self.h, self.weights, self.d_path, self.d_river = hm.build(lay)
        self.cell = lay.size_m / (hm.PIXELS - 1)
        self.half = lay.size_m / 2
        self.x, self.y = hm.grid(lay.size_m)

    def height(self, x, y):
        """Bilinear sample in metres (row 0 = minimum Y, as in make_heightmap)."""
        fx = np.clip((np.asarray(x) + self.half) / self.cell, 0, hm.PIXELS - 1.001)
        fy = np.clip((np.asarray(y) + self.half) / self.cell, 0, hm.PIXELS - 1.001)
        x0, y0 = np.floor(fx).astype(int), np.floor(fy).astype(int)
        tx, ty = fx - x0, fy - y0
        h = self.h
        return (h[y0, x0] * (1 - tx) * (1 - ty) + h[y0, x0 + 1] * tx * (1 - ty)
                + h[y0 + 1, x0] * (1 - tx) * ty + h[y0 + 1, x0 + 1] * tx * ty)

    def pixel(self, x, y):
        return (np.clip(np.round((np.asarray(y) + self.half) / self.cell), 0, hm.PIXELS - 1).astype(int),
                np.clip(np.round((np.asarray(x) + self.half) / self.cell), 0, hm.PIXELS - 1).astype(int))


def uu(v):
    return round(float(v) * 100.0, 1)


def item(key, x, y, z, yaw=0.0, scale=1.0):
    return {"mesh": key, "x": uu(x), "y": uu(y), "z": uu(z), "yaw": round(float(yaw), 1), "scale": round(float(scale), 3)}


def build(lay=None):
    lay = lay or layout_mod.load()
    t = Terrain(lay)
    rng = np.random.default_rng(SEED)
    vx, vy, vr = lay.areas["village"]
    out = {"buildings": [], "walls": [], "camps": [], "boss": {}, "bridge": {}, "props": [], "scatter": {}, "villagers": []}
    blockers = []  # (x, y, radius) no scatter inside

    # Village buildings (the gate moves to where the wall ring crosses the north street).
    for index, b in enumerate(lay.buildings):
        x, y = (vx, vy + WALL_RADIUS_M) if b["type"] == "gate" else (b["x"], b["y"])
        choices = BUILDING_MESH[b["type"]]
        out["buildings"].append(item(choices[index % len(choices)], x, y, t.height(x, y) - 0.05, b["yaw"]))
        blockers.append((x, y, FOOTPRINT_M[b["type"]]))

    # Wall ring with a gap for the gate (north, +Y).
    count = round(2 * math.pi * WALL_RADIUS_M / WALL_SEGMENT_M)
    for i in range(count):
        a = 2 * math.pi * i / count
        if abs(math.degrees(a) - 90) < 360 / count * 0.9:
            continue
        x, y = vx + WALL_RADIUS_M * math.cos(a), vy + WALL_RADIUS_M * math.sin(a)
        ends = [(vx + WALL_RADIUS_M * math.cos(a + s), vy + WALL_RADIUS_M * math.sin(a + s)) for s in (-0.05, 0.05)]
        z = min(float(t.height(px, py)) for px, py in ends + [(x, y)]) - 0.4
        out["walls"].append(item("Arch/wall_segment", x, y, z, math.degrees(a) + 90))
    blockers.append((vx, vy, WALL_RADIUS_M + 6))

    # Camps: tents facing the fire, a palisade arc open toward the village, props, torches at the opening.
    for camp in lay.camps:
        cz = float(t.height(camp.x, camp.y))
        towards_village = math.atan2(vy - camp.y, vx - camp.x)
        dressing = [item("Camp/campfire", camp.x, camp.y, cz - 0.05)]
        tents = 2 + camp.difficulty // 2
        for i in range(tents):
            a = towards_village + math.pi * (0.45 + 1.1 * i / max(1, tents - 1))
            x, y = camp.x + camp.r * 0.55 * math.cos(a), camp.y + camp.r * 0.55 * math.sin(a)
            dressing.append(item("Camp/tent", x, y, t.height(x, y) - 0.05, math.degrees(a) + 180))
            for k in range(2):
                pa = a + (k - 0.5) * 0.35
                px, py = camp.x + camp.r * 0.72 * math.cos(pa), camp.y + camp.r * 0.72 * math.sin(pa)
                dressing.append(item(CAMP_PROPS[int(rng.integers(len(CAMP_PROPS)))], px, py, t.height(px, py), rng.uniform(0, 360)))
        circumference = 2 * math.pi * (camp.r + 1)
        segments = int(circumference * 0.72 / 3.6)
        for i in range(segments):
            a = towards_village + math.radians(50) + (2 * math.pi - math.radians(100)) * (i + 0.5) / segments
            x, y = camp.x + (camp.r + 1) * math.cos(a), camp.y + (camp.r + 1) * math.sin(a)
            dressing.append(item("Camp/palisade", x, y, t.height(x, y) - 0.3, math.degrees(a)))
        for s in (-1, 1):
            a = towards_village + s * math.radians(40)
            x, y = camp.x + (camp.r + 1) * math.cos(a), camp.y + (camp.r + 1) * math.sin(a)
            dressing.append(item("Prop/Torch_Metal", x, y, t.height(x, y), 0, 1.4))
        out["camps"].append({"id": camp.id, "x": uu(camp.x), "y": uu(camp.y), "z": uu(cz), "r": uu(camp.r),
                             "difficulty": camp.difficulty, "waves": camp.waves, "dressing": dressing})
        blockers.append((camp.x, camp.y, camp.r + 8))

    # Boss ruins: a broken ring of big rocks and leaning wall pieces around the arena, lanterns at the south entrance.
    bx, by, br = lay.boss["x"], lay.boss["y"], lay.boss["r"]
    bz = float(t.height(bx, by))
    ruins = []
    for i in range(14):
        a = 2 * math.pi * i / 14
        if abs(math.degrees(a) - 270) < 18:
            continue  # entrance toward the path (south)
        x, y = bx + br * math.cos(a), by + br * math.sin(a)
        if i % 3 == 0:
            ruins.append(item("Arch/ruin_wall", x, y, t.height(x, y) - 0.3, math.degrees(a) + 90 + rng.uniform(-12, 12), 1.0))
        elif i % 3 == 1:
            ruins.append(item("Arch/ruin_pillar", x, y, t.height(x, y) - 0.2, rng.uniform(0, 90), rng.uniform(0.9, 1.2)))
        else:
            ruins.append(item(ROCKS[i % 3], x, y, t.height(x, y) - 0.5, rng.uniform(0, 360), rng.uniform(2.2, 3.4)))
    for s in (-1, 1):
        x, y = bx + s * 5, by - br - 3
        ruins.append(item("Arch/stone_lantern", x, y, t.height(x, y), 90))
    ruins.append(item("Arch/torii", bx, by - br - 8, t.height(bx, by - br - 8), 90))
    out["boss"] = {"x": uu(bx), "y": uu(by), "z": uu(bz), "r": uu(br), "dressing": ruins}
    blockers.append((bx, by, br + 12))

    # Bridge over the river, on the floodplain level.
    bridge_x, bridge_y, bridge_yaw = lay.bridge
    plain = max(float(t.height(bridge_x - 20, bridge_y)), float(t.height(bridge_x + 20, bridge_y)))
    out["bridge"] = item("Arch/bridge", bridge_x, bridge_y, plain - 0.25, bridge_yaw)
    blockers.append((bridge_x, bridge_y, 20))

    # Villager routes (points on the ground; the villager walks between them).
    for route in lay.villagers:
        out["villagers"].append([{"x": uu(x), "y": uu(y), "z": uu(t.height(x, y))} for x, y in route])

    out["tour_points"] = {name: {"x": uu(x), "y": uu(y), "z": uu(t.height(x, y))} for name, (x, y) in TOUR_CLOSEUPS.items()}
    scatter(t, rng, blockers, out)
    return out


def jittered(t, rng, spacing):
    """A jittered grid over the playable map plus the mountain foot."""
    coords = np.arange(-470, 470, spacing)
    gx, gy = np.meshgrid(coords, coords)
    gx = gx + rng.uniform(-spacing * 0.45, spacing * 0.45, gx.shape)
    gy = gy + rng.uniform(-spacing * 0.45, spacing * 0.45, gy.shape)
    return gx.ravel(), gy.ravel()


def free_of(blockers, x, y):
    ok = np.ones(x.shape, bool)
    for bx, by, r in blockers:
        ok &= np.hypot(x - bx, y - by) > r
    return ok


def scatter(t, rng, blockers, out):
    lay = t.lay
    vx, vy, vr = lay.areas["village"]
    cell = t.cell
    gy_, gx_ = np.gradient(t.h, cell)
    slope = np.degrees(np.arctan(np.hypot(gx_, gy_)))
    edge = np.maximum(np.abs(t.x), np.abs(t.y))
    d_village = np.hypot(t.x - vx, t.y - vy)
    d_boss = np.hypot(t.x - lay.boss["x"], t.y - lay.boss["y"])
    forest = hm.forest_density(lay, t.x, t.y)
    placed = {}

    def add(key, x, y, z, yaw, scale, cull=0.0):
        chunk = f"{int((x + t.half) // CHUNK_M)}_{int((y + t.half) // CHUNK_M)}"
        placed.setdefault(chunk, {}).setdefault(key, {"cull": cull, "items": []})["items"].append(
            [uu(x), uu(y), uu(z), round(float(yaw), 1), round(float(scale), 3)])

    def field(f, x, y):
        rows, cols = t.pixel(x, y)
        return f[rows, cols]

    # Trees
    x, y = jittered(t, rng, 6.5)
    keep = free_of(blockers, x, y) & (field(t.d_path, x, y) > 6) & (field(t.d_river, x, y) > 12) & (field(slope, x, y) < 32)
    keep &= rng.random(x.shape) < field(forest, x, y) * 0.8
    x, y = x[keep], y[keep]
    z = t.height(x, y)
    high = z > float(np.median(t.h)) + 14
    for xi, yi, zi, hi in zip(x, y, z, high):
        roll = rng.random()
        near_river = field(t.d_river, xi, yi) < 45
        near_boss = field(d_boss, xi, yi) < 130
        near_village = field(d_village, xi, yi) < vr + 60
        if near_boss and roll < 0.5:
            kind = "dead"
        elif hi or abs(xi) > 440 or abs(yi) > 440:
            kind = "pine" if roll < 0.75 else "common"
        elif (near_river or near_village) and roll < 0.45:
            kind = "maple"
        else:
            kind = "common" if roll < 0.62 else ("pine" if roll < 0.85 else "maple")
        key = TREES[kind][int(rng.integers(5))]
        add(key, xi, yi, zi - 0.2, rng.uniform(0, 360), rng.uniform(*TREE_SCALE[kind]))

    # Bushes: forest floor and around the village wall.
    x, y = jittered(t, rng, 5.0)
    keep = free_of(blockers, x, y) & (field(t.d_path, x, y) > 3.5) & (field(t.d_river, x, y) > 9) & (field(slope, x, y) < 35)
    density = np.maximum(field(forest, x, y) * 0.28, (np.abs(field(d_village, x, y) - (WALL_RADIUS_M + 9)) < 4) * 0.5)
    keep &= rng.random(x.shape) < density
    for xi, yi in zip(x[keep], y[keep]):
        add(BUSHES[int(rng.integers(2))], xi, yi, t.height(xi, yi) - 0.1, rng.uniform(0, 360), rng.uniform(0.8, 1.4), cull=20000.0)

    # Rocks: sparse everywhere, more on slopes and the mountain foot.
    x, y = jittered(t, rng, 12.0)
    keep = free_of(blockers, x, y) & (field(t.d_path, x, y) > 4) & (field(t.d_river, x, y) > 8)
    density = 0.08 + 0.35 * hm.smoothstep(18, 35, field(slope, x, y)) + 0.3 * (field(edge, x, y) > 440)
    keep &= (rng.random(x.shape) < density) & (np.abs(x) < 478) & (np.abs(y) < 478)
    for xi, yi in zip(x[keep], y[keep]):
        add(ROCKS[int(rng.integers(3))], xi, yi, t.height(xi, yi) - 0.4, rng.uniform(0, 360), rng.uniform(0.6, 1.8), cull=20000.0)

    # The grass carpet is landscape grass (build_world.py GRASS_TYPES), generated on the GPU near each viewer.

    out["scatter"] = placed


def write(path=None):
    path = path or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "World", "Terrain", "placements.json")
    data = build()
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"))
    counts = {}
    for chunk in data["scatter"].values():
        for key, entry in chunk.items():
            counts[key.split("/")[0]] = counts.get(key.split("/")[0], 0) + len(entry["items"])
    print(f"[placements] {len(data['buildings'])} buildings, {len(data['walls'])} wall segments, {len(data['camps'])} camps, "
          f"scatter {counts} in {len(data['scatter'])} chunks -> {os.path.abspath(path)}")
    return data


if __name__ == "__main__":
    write(sys.argv[1] if len(sys.argv) > 1 else None)
