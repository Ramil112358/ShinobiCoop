"""Heightmap and weightmaps for the open world, from Scripts/world/world_layout.json (sub-project #3).

  python Scripts/world/make_heightmap.py <out_dir>

Writes (1009 x 1009 px, ~1 m per pixel, +X east = image right, +Y north = image up):
  height.png          16-bit, UE landscape encoding: value = 32768 + metres * 128 (landscape Z scale 100)
  weight_grass/dirt/rock/bank.png   8-bit layer weights, summing to 255 per pixel
  terrain.json        {"size_m", "pixels", "height_scale_z": 100.0, "min_m", "max_m"}
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


def grid(size_m):
    half = size_m / 2
    coords = np.linspace(-half, half, PIXELS)
    x, y = np.meshgrid(coords, coords[::-1])  # row 0 = north edge
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


def build(lay):
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

    # Weights.
    cell = lay.size_m / (PIXELS - 1)
    gy_, gx_ = np.gradient(h, cell)
    slope_deg = np.degrees(np.arctan(np.hypot(gx_, gy_)))
    dirt = 1 - smoothstep(PATH_HALF_WIDTH_M, PATH_HALF_WIDTH_M + 2, d_path)
    dirt = np.maximum(dirt, 1 - smoothstep(30, 40, d_village))  # village square
    for camp in lay.camps + [type("B", (), {"x": bx, "y": by, "r": br})]:
        dirt = np.maximum(dirt, 1 - smoothstep(camp.r * 0.7, camp.r, np.hypot(x - camp.x, y - camp.y)))
    bank_w = 1 - smoothstep(RIVER_HALF_WIDTH_M + 2, RIVER_HALF_WIDTH_M + 8, d_river)
    rock = np.maximum(smoothstep(28, 36, slope_deg), smoothstep(MOUNTAIN_START_M, MOUNTAIN_START_M + 30, edge))
    rock = rock * (1 - dirt)
    bank_w = bank_w * (1 - rock)
    dirt = dirt * (1 - bank_w)
    grass = np.clip(1 - dirt - rock - bank_w, 0, 1)
    total = grass + dirt + rock + bank_w
    weights = {name: w / total for name, w in (("grass", grass), ("dirt", dirt), ("rock", rock), ("bank", bank_w))}
    build.village_height = village_height
    return h, weights, d_path, d_river


def write(out_dir, lay=None):
    lay = lay or layout_mod.load()
    h, weights, _, _ = build(lay)
    os.makedirs(out_dir, exist_ok=True)
    encoded = np.clip(np.round(32768 + h * 128), 0, 65535).astype(np.uint16)
    Image.fromarray(encoded).save(os.path.join(out_dir, "height.png"))
    # Floor three layers and give grass the remainder: the sum is exactly 255 and grass never goes negative.
    quantised = {name: np.floor(weights[name] * 255) for name in ("dirt", "rock", "bank")}
    quantised["grass"] = 255 - quantised["dirt"] - quantised["rock"] - quantised["bank"]
    for name, w in quantised.items():
        Image.fromarray(np.clip(w, 0, 255).astype(np.uint8)).save(os.path.join(out_dir, f"weight_{name}.png"))
    meta = {"size_m": lay.size_m, "pixels": PIXELS, "height_scale_z": 100.0, "min_m": float(h.min()), "max_m": float(h.max()),
            "village_height_m": build.village_height, "water_level_m": build.water_level}
    with open(os.path.join(out_dir, "terrain.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    return meta


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "World", "Terrain")
    print(write(os.path.abspath(out)))
