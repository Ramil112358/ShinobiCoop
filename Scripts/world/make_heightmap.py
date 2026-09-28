"""Heightmap and weightmaps for the open world, from Scripts/world/world_layout.json (sub-project #3).

  python Scripts/world/make_heightmap.py <out_dir>

Writes (1009 x 1009 px, ~1 m per pixel, +X = image right, +Y = image down: row 0 is the landscape's minimum Y):
  height.png          16-bit, UE landscape encoding: value = 32768 + metres * 128 (landscape Z scale 100)
  weight_<layer>.png  8-bit layer weights for LAYERS, summing to 255 per pixel
  terrain.json        {"size_m", "pixels", "height_scale_z": 100.0, "min_m", "max_m", "layers", ...}
The hills and mountains are weathered by droplet erosion (masked off the village, camps, boss arena, paths and the
river floodplain, which keep their exact shape); steep slopes get a little rocky detail.
Deterministic (fixed seed). Needs numpy + Pillow (system Python, not UE's).
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as layout_mod  # noqa: E402

PIXELS = 1009
SEED = 7
BANK_SLOPE = 0.35  # rise per metre from the river edge (~19 degrees)
RIVER_HALF_WIDTH_M = 7.0
PATH_HALF_WIDTH_M = 3.0
BOSS_RISE_M = 30.0
MOUNTAIN_START_M = 450.0
LAYERS = ["grass", "forest", "dirt", "rock", "bank"]
# Building footprint radii (m): no scatter inside them (make_placements) and bare dirt under them (no grass).
FOOTPRINT_M = {"house_small": 5.0, "house_medium": 6.0, "house_large": 7.5, "stall": 2.5, "lantern": 0.6, "gate": 7.0,
               "shrine": 5.5, "torii": 4.0, "well": 2.0, "training_post": 0.6}


def grid(size_m):
    half = size_m / 2
    coords = np.linspace(-half, half, PIXELS)
    x, y = np.meshgrid(coords, coords)  # row 0 = minimum Y (south): UE maps image row 0 to the landscape's minimum Y
    return x, y


def value_noise(x, y, cell_m, rng):
    """Smooth value noise: random lattice values, smoothstep-interpolated."""
    gx, gy = x / cell_m, y / cell_m
    x0, y0 = np.floor(gx).astype(int), np.floor(gy).astype(int)
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    lattice = rng.random((257, 257))
    v = lambda ix, iy: lattice[iy % 257, ix % 257]  # noqa: E731  (wraps: any coordinate range works)
    top = v(x0, y0) * (1 - fx) + v(x0 + 1, y0) * fx
    bottom = v(x0, y0 + 1) * (1 - fx) + v(x0 + 1, y0 + 1) * fx
    return top * (1 - fy) + bottom * fy


def fractal(x, y, rng):
    total, amplitude, cell = np.zeros_like(x), 1.0, 220.0
    for _ in range(5):
        total += amplitude * value_noise(x, y, cell, rng)
        amplitude *= 0.5
        cell *= 0.5
    return total / 1.9375  # 0..1


def distance_to_polyline(x, y, points):
    best = np.full(x.shape, np.inf)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        dx, dy = bx - ax, by - ay
        t = np.clip(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0, 1)
        best = np.minimum(best, np.hypot(x - (ax + t * dx), y - (ay + t * dy)))
    return best


def smoothstep(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def forest_density(lay, x, y):
    """0-1 tree density: the forest ring around the village, thinning into the mountain foot. Trees are placed from it
    (make_placements) and the forest-floor layer is painted from it."""
    vx, vy, vr = lay.areas["village"]
    edge = np.maximum(np.abs(x), np.abs(y))
    f = smoothstep(vr + 15, vr + 60, np.hypot(x - vx, y - vy)) * (edge < 478)
    return f * (1 - smoothstep(452, 478, edge) * 0.6)


def bilinear(h, px, py):
    x0, y0 = px.astype(int), py.astype(int)
    fx, fy = px - x0, py - y0
    return (h[y0, x0] * (1 - fx) * (1 - fy) + h[y0, x0 + 1] * fx * (1 - fy)
            + h[y0 + 1, x0] * (1 - fx) * fy + h[y0 + 1, x0 + 1] * fx * fy)


def erode(h, mask, iterations=200_000, seed=SEED, batch=20_000, steps=48):
    """Hydraulic droplet erosion (after Lague), all droplets of a batch stepped together. Heights in metres on a 1 m
    grid. Returns h + (eroded - h) * mask, the change softened by a small blur so single droplets leave no pits."""
    rng = np.random.default_rng(seed + 1)
    n = h.shape[0]
    work = h.astype(np.float64).copy()
    inertia, capacity_k, deposit_k, erode_k, evaporate, gravity, min_slope = 0.1, 6.0, 0.3, 0.4, 0.03, 9.8, 0.02
    for _ in range(iterations // batch):
        px, py = rng.uniform(1, n - 2.001, batch), rng.uniform(1, n - 2.001, batch)
        dx, dy = np.zeros(batch), np.zeros(batch)
        speed, water, sediment = np.ones(batch), np.ones(batch), np.zeros(batch)
        alive = np.ones(batch, bool)
        for _ in range(steps):
            x0, y0 = px.astype(int), py.astype(int)
            fx, fy = px - x0, py - y0
            h00, h10, h01, h11 = work[y0, x0], work[y0, x0 + 1], work[y0 + 1, x0], work[y0 + 1, x0 + 1]
            gx = (h10 - h00) * (1 - fy) + (h11 - h01) * fy
            gy = (h01 - h00) * (1 - fx) + (h11 - h10) * fx
            here = h00 * (1 - fx) * (1 - fy) + h10 * fx * (1 - fy) + h01 * (1 - fx) * fy + h11 * fx * fy
            dx, dy = dx * inertia - gx * (1 - inertia), dy * inertia - gy * (1 - inertia)
            length = np.hypot(dx, dy)
            alive &= length > 1e-9
            length[length <= 1e-9] = 1.0
            dx, dy = dx / length, dy / length
            nx, ny = px + dx, py + dy
            alive &= (nx >= 1) & (nx < n - 2.001) & (ny >= 1) & (ny < n - 2.001)
            nx, ny = np.clip(nx, 1, n - 2.001), np.clip(ny, 1, n - 2.001)
            dh = bilinear(work, nx, ny) - here
            capacity = np.maximum(-dh, min_slope) * speed * water * capacity_k
            depositing = (sediment > capacity) | (dh > 0)
            deposit = np.where(dh > 0, np.minimum(dh, sediment), (sediment - capacity) * deposit_k)
            erosion = np.minimum((capacity - sediment) * erode_k, np.maximum(-dh, 0))
            change = np.where(depositing, deposit, -erosion) * alive
            sediment -= change
            for oy, ox, weight in ((0, 0, (1 - fx) * (1 - fy)), (0, 1, fx * (1 - fy)), (1, 0, (1 - fx) * fy), (1, 1, fx * fy)):
                np.add.at(work, (y0 + oy, x0 + ox), change * weight)
            speed = np.sqrt(np.maximum(speed ** 2 - dh * gravity, 0.0))
            water *= 1 - evaporate
            px, py = nx, ny
    delta = work - h
    for _ in range(3):  # soften: 3x3 box blur
        padded = np.pad(delta, 1, mode="edge")
        delta = sum(padded[1 + oy:n + 1 + oy, 1 + ox:n + 1 + ox] for oy in (-1, 0, 1) for ox in (-1, 0, 1)) / 9.0
    return h + delta * mask


def build(lay, erosion=True):
    rng = np.random.default_rng(SEED)
    x, y = grid(lay.size_m)
    # Rolling hills with real relief: contrast-stretched fractal noise, ~5-40 m.
    f = fractal(x, y, rng)
    h = 5 + 35 * np.clip((f - 0.5) * 2.4 + 0.5, 0, 1) ** 1.3

    # The village plateau sits at the average height of its surroundings, so it's neither a pit nor a mesa.
    vx, vy, vr = lay.areas["village"]
    d_village = np.hypot(x - vx, y - vy)
    village_height = float(np.mean(h[(d_village > vr) & (d_village < vr + 80)]))

    # Boss ridge: a broad rise to the north, flat on top where the fortress stands.
    bx, by, br = lay.boss["x"], lay.boss["y"], lay.boss["r"]
    d_boss = np.hypot(x - bx, y - by)
    ridge_top = village_height + BOSS_RISE_M
    h = h + (ridge_top - h) * (1 - smoothstep(br, br + 140, d_boss))

    # Edge mountains: a rounded square (superellipse), its foot line wobbled by noise so it doesn't read as a wall.
    edge = (np.abs(x) ** 6 + np.abs(y) ** 6) ** (1 / 6)
    edge = edge + 30 * (fractal(x * 0.8 + 97, y * 0.8 - 51, rng) - 0.5)
    peaks = 60 + 70 * fractal(x * 2.3 + 311, y * 2.3, rng)
    h = h + smoothstep(MOUNTAIN_START_M - 30, lay.size_m / 2 + 20, edge) * peaks

    # Paths: pull toward a gently smoothed local height so they're walkable, not cut into steps.
    d_path = np.min([distance_to_polyline(x, y, p) for p in lay.paths], axis=0)
    path_ease = 1 - smoothstep(PATH_HALF_WIDTH_M, PATH_HALF_WIDTH_M + 10, d_path)
    smooth = np.asarray(Image.fromarray(h.astype(np.float32)).resize((64, 64), Image.BILINEAR).resize((PIXELS, PIXELS), Image.BILINEAR))
    h = h + (smooth - h) * path_ease * 0.8

    # Flat village plateau and camp clearings.
    h = h + (village_height - h) * (1 - smoothstep(vr, vr + 70, d_village))
    for camp in lay.camps:
        d = np.hypot(x - camp.x, y - camp.y)
        local = float(h[np.unravel_index(np.argmin(d), d.shape)])
        h = h + (local - h) * (1 - smoothstep(camp.r, camp.r + 45, d))

    # River: one constant water level (a single flat water plane in UE) below every bit of playable ground, so the
    # plane can't flood hollows elsewhere. The land eases down into a meadow floodplain around the river (so the
    # banks stay low and the bridge short), the bed sits 1.5 m under the water, and the banks rise at a steady slope
    # until they meet the terrain (cutting a gorge through the edge mountains).
    d_river = distance_to_polyline(x, y, lay.river)
    playable = np.maximum(np.abs(x), np.abs(y)) < MOUNTAIN_START_M - 30
    water_level = float(h[playable].min()) - 1.0
    plain = water_level + 2.5
    floodplain = 1 - smoothstep(20, 110, d_river)
    h = h + (plain - h) * floodplain * 0.9
    bank_target = np.where(d_river < RIVER_HALF_WIDTH_M, water_level - 1.5,
                           water_level - 1.5 + (d_river - RIVER_HALF_WIDTH_M) * BANK_SLOPE)
    h = np.minimum(h, bank_target)
    dry = d_river >= RIVER_HALF_WIDTH_M + 3
    h = np.where(dry, np.maximum(h, water_level + 0.8), h)
    build.water_level = water_level

    # Weathering, masked to zero on playable ground (with a soft ramp beyond it).
    protect = 1 - smoothstep(vr + 15, vr + 40, d_village)
    protect = np.maximum(protect, 1 - smoothstep(br + 10, br + 30, d_boss))
    protect = np.maximum(protect, 1 - smoothstep(PATH_HALF_WIDTH_M + 5, PATH_HALF_WIDTH_M + 20, d_path))
    protect = np.maximum(protect, 1 - smoothstep(110, 140, d_river))
    for camp in lay.camps:
        protect = np.maximum(protect, 1 - smoothstep(camp.r + 10, camp.r + 30, np.hypot(x - camp.x, y - camp.y)))
    mask = 1 - protect
    if erosion:
        h = erode(h, mask)
        cell = lay.size_m / (PIXELS - 1)
        gy_, gx_ = np.gradient(h, cell)
        steep = smoothstep(22, 38, np.degrees(np.arctan(np.hypot(gx_, gy_))))
        h = h + (value_noise(x, y, 4.0, rng) - 0.5) * 0.8 * steep * mask
        h = np.where(dry, np.maximum(h, water_level + 0.8), h)

    # Weights.
    cell = lay.size_m / (PIXELS - 1)
    gy_, gx_ = np.gradient(h, cell)
    slope_deg = np.degrees(np.arctan(np.hypot(gx_, gy_)))
    dirt = 1 - smoothstep(PATH_HALF_WIDTH_M, PATH_HALF_WIDTH_M + 2, d_path)
    dirt = np.maximum(dirt, 1 - smoothstep(30, 40, d_village))  # village square
    for camp in lay.camps + [type("B", (), {"x": bx, "y": by, "r": br})]:
        dirt = np.maximum(dirt, 1 - smoothstep(camp.r * 0.7, camp.r, np.hypot(x - camp.x, y - camp.y)))
    for b in lay.buildings:
        if b["type"] != "gate":
            radius = max(FOOTPRINT_M[b["type"]], 1.0)
            dirt = np.maximum(dirt, 1 - smoothstep(radius, radius + 1.5, np.hypot(x - b["x"], y - b["y"])))
    bank_w = 1 - smoothstep(RIVER_HALF_WIDTH_M + 2, RIVER_HALF_WIDTH_M + 8, d_river)
    rock = np.maximum(smoothstep(28, 36, slope_deg), smoothstep(MOUNTAIN_START_M, MOUNTAIN_START_M + 30, edge))
    rock = rock * (1 - dirt)
    bank_w = bank_w * (1 - rock)
    dirt = dirt * (1 - bank_w)
    grass = np.clip(1 - dirt - rock - bank_w, 0, 1)
    total = grass + dirt + rock + bank_w
    weights = {name: w / total for name, w in (("grass", grass), ("dirt", dirt), ("rock", rock), ("bank", bank_w))}
    # Forest floor takes over the grass under the trees.
    forest = np.minimum(weights["grass"], smoothstep(0.15, 0.5, forest_density(lay, x, y)))
    weights["grass"] = weights["grass"] - forest
    weights["forest"] = forest
    build.village_height = village_height
    return h, weights, d_path, d_river


def write(out_dir, lay=None):
    lay = lay or layout_mod.load()
    h, weights, _, _ = build(lay)
    os.makedirs(out_dir, exist_ok=True)
    encoded = np.clip(np.round(32768 + h * 128), 0, 65535).astype(np.uint16)
    Image.fromarray(encoded).save(os.path.join(out_dir, "height.png"))
    # Floor three layers and give grass the remainder: the sum is exactly 255 and grass never goes negative.
    quantised = {name: np.floor(weights[name] * 255) for name in LAYERS if name != "grass"}
    quantised["grass"] = 255 - sum(quantised.values())
    for name, w in quantised.items():
        Image.fromarray(np.clip(w, 0, 255).astype(np.uint8)).save(os.path.join(out_dir, f"weight_{name}.png"))
    meta = {"size_m": lay.size_m, "pixels": PIXELS, "height_scale_z": 100.0, "min_m": float(h.min()), "max_m": float(h.max()),
            "village_height_m": build.village_height, "water_level_m": build.water_level, "layers": LAYERS}
    with open(os.path.join(out_dir, "terrain.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    return meta


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "World", "Terrain")
    print(write(os.path.abspath(out)))
