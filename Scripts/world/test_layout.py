"""Checks for the world layout and terrain generator. Run: python Scripts/world/test_layout.py"""
import copy
import json
import os
import sys
import tempfile

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout  # noqa: E402
import make_heightmap as hm  # noqa: E402


def world_to_pixel(lay, x, y):
    cell = lay.size_m / (hm.PIXELS - 1)
    return int(round((y + lay.size_m / 2) / cell)), int(round((x + lay.size_m / 2) / cell))  # row 0 = minimum Y


def test_layout_loads():
    lay = layout.load()
    assert len(lay.camps) == 5 and lay.boss, "5 camps and a boss"
    assert len({c.id for c in lay.camps}) == 5
    assert all(abs(c.x) <= 450 and abs(c.y) <= 450 for c in lay.camps)
    assert lay.village_name and lay.village_name.lower() != "konoha"


def test_layout_rejects_bad_camps():
    with open(layout.DEFAULT_PATH, encoding="utf-8") as f:
        data = json.load(f)
    outside = copy.deepcopy(data)
    outside["camps"][0]["x"] = 900
    duplicate = copy.deepcopy(data)
    duplicate["camps"][1]["id"] = duplicate["camps"][0]["id"]
    for bad, name in ((outside, "camp_1"), (duplicate, "camp_1")):
        try:
            layout.parse(bad)
        except ValueError as error:
            assert name in str(error), error
        else:
            raise AssertionError("bad layout accepted")


def test_terrain():
    lay = layout.load()
    h, weights, d_path, d_river = hm.build(lay)
    vx, vy, vr = lay.areas["village"]
    r0, c0 = world_to_pixel(lay, vx - vr * 0.7, vy + vr * 0.7)
    r1, c1 = world_to_pixel(lay, vx + vr * 0.7, vy - vr * 0.7)
    plateau = h[min(r0, r1):max(r0, r1), min(c0, c1):max(c0, c1)]
    assert plateau.max() - plateau.min() < 0.5, f"village plateau varies {plateau.max() - plateau.min():.2f} m"
    # River bed lower than the ground 30 m to either side, at several points along it.
    for (x, y) in lay.river[1:-1]:
        bed = h[world_to_pixel(lay, x, y)]
        for side in (-30, 30):
            assert bed < h[world_to_pixel(lay, x + side, y)] - 1.0, f"river at ({x}, {y}) not carved"
    with tempfile.TemporaryDirectory() as out:
        hm.write(out, lay)
        dirt = np.asarray(Image.open(os.path.join(out, "weight_dirt.png")))
        total = sum(np.asarray(Image.open(os.path.join(out, f"weight_{n}.png")), dtype=np.int32) for n in ("grass", "dirt", "rock", "bank"))
        assert (total == 255).all(), "layer weights must sum to 255"
        px, py = lay.paths[0][1]
        assert dirt[world_to_pixel(lay, px, py)] == 255, "path centre fully dirt"
        height = np.asarray(Image.open(os.path.join(out, "height.png")))
        assert height.dtype == np.uint16 and height.shape == (hm.PIXELS, hm.PIXELS)


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
