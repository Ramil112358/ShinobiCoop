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
import make_placements as mp  # noqa: E402
import fetch_polyhaven as ph  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


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
        total = sum(np.asarray(Image.open(os.path.join(out, f"weight_{n}.png")), dtype=np.int32) for n in hm.LAYERS)
        assert (total == 255).all(), "layer weights must sum to 255"
        px, py = lay.paths[0][1]
        assert dirt[world_to_pixel(lay, px, py)] == 255, "path centre fully dirt"
        height = np.asarray(Image.open(os.path.join(out, "height.png")))
        assert height.dtype == np.uint16 and height.shape == (hm.PIXELS, hm.PIXELS)


def test_placements():
    lay = layout.load()
    data = mp.build(lay)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "library_catalog.json"), encoding="utf-8") as f:
        catalog = json.load(f)
    known = {f"{category}/{name}" for category, entries in catalog.items() for name in entries}
    keys = {b["mesh"] for b in data["buildings"] + data["walls"]} | {data["bridge"]["mesh"]}
    keys |= {d["mesh"] for camp in data["camps"] for d in camp["dressing"]} | {d["mesh"] for d in data["boss"]["dressing"]}
    keys |= {key for chunk in data["scatter"].values() for key in chunk}
    assert keys <= known, f"meshes missing from the catalog: {sorted(keys - known)}"
    assert len(data["camps"]) == 5 and data["boss"] and len(data["walls"]) > 40
    # Path-clear check: no tree, bush or rock within 3 m of a path centre line; none inside a building footprint.
    blocking = [(x / 100, y / 100) for chunk in data["scatter"].values() for key, entry in chunk.items()
                if not key.startswith("Grass/") for x, y, *_ in entry["items"]]
    bx = np.array([p[0] for p in blocking])
    by = np.array([p[1] for p in blocking])
    for path in lay.paths:
        near = hm.distance_to_polyline(bx, by, path)
        assert (near > 3.0).all(), f"{int((near <= 3.0).sum())} blockers on a path"
    for b in lay.buildings:
        if b["type"] != "gate":
            assert (np.hypot(bx - b["x"], by - b["y"]) > mp.FOOTPRINT_M[b["type"]]).all(), f"scatter inside {b['type']}"


def test_fetch_plan():
    with open(os.path.join(HERE, "testdata", "polyhaven_rock_07_files.json"), encoding="utf-8") as f:
        files = json.load(f)
    plan = ph.plan_downloads(files, "2k", "model")
    rel = {r for _, r, _ in plan}
    gltf = files["gltf"]["2k"]["gltf"]
    assert "rock_07_2k.gltf" in rel and all(name in rel for name in gltf["include"]), rel
    assert dict((r, s) for _, r, s in plan)["rock_07_2k.gltf"] == gltf["size"]
    textures = ph.plan_downloads(files, "2k", "texture")
    assert len(textures) == 4 and all(any(m in r for _, r, _ in textures) for m in ("_diff", "_nor_gl_", "_rough_", "_disp_")), textures
    # Foliage: the glTF's colour JPG has no alpha, so the separate alpha maps come along (Alpha, twig_alpha, ...).
    leafy = dict(files, twig_alpha={"2k": {"jpg": {"url": "https://x/tree_twig_alpha_2k.jpg", "size": 5}}})
    extra = [(u, r, s) for u, r, s in ph.plan_downloads(leafy, "2k", "model") if "alpha" in r]
    assert extra == [("https://x/tree_twig_alpha_2k.jpg", "textures/tree_twig_alpha_2k.jpg", 5)], extra
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "f.bin")
        with open(path, "wb") as f:
            f.write(b"x" * 10)
        assert not ph.needs_download(path, 10)
        assert ph.needs_download(path, 11)  # truncated by an interrupted run
        assert ph.needs_download(os.path.join(tmp, "missing.bin"), 10)


def test_trees_trunk_collision():
    with open(os.path.join(HERE, "library_catalog.json"), encoding="utf-8") as f:
        catalog = json.load(f)
    trees = {name: e for cat in ("PH_Tree", "Mega_Tree") for name, e in catalog.get(cat, {}).items()}
    assert trees, "no realistic trees imported"
    assert all(e.get("collision") == "trunk" and 10 <= e.get("trunk_radius_cm", 0) <= 80 for e in trees.values()), trees


def test_erosion_protects_playable():
    lay = layout.load()
    eroded, _, d_path, _ = hm.build(lay)
    plain, _, _, _ = hm.build(lay, erosion=False)
    x, y = hm.grid(lay.size_m)
    delta = np.abs(eroded - plain)
    vx, vy, vr = lay.areas["village"]
    protected = (np.hypot(x - vx, y - vy) < vr) | (np.hypot(x - lay.boss["x"], y - lay.boss["y"]) < lay.boss["r"]) | (d_path < 3)
    for camp in lay.camps:
        protected |= np.hypot(x - camp.x, y - camp.y) < camp.r
    assert delta[protected].max() < 0.05, f"erosion moved playable ground by {delta[protected].max():.2f} m"
    assert delta[~protected].mean() > 0.2, f"erosion barely changed the terrain ({delta[~protected].mean():.3f} m)"


def test_weights_five_layers():
    lay = layout.load()
    with tempfile.TemporaryDirectory() as tmp:
        hm.write(tmp, lay)
        w = {n: np.asarray(Image.open(os.path.join(tmp, f"weight_{n}.png"))).astype(int) for n in hm.LAYERS}
        meta = json.load(open(os.path.join(tmp, "terrain.json"), encoding="utf-8"))
    assert meta["layers"] == ["grass", "forest", "dirt", "rock", "bank"]
    assert (sum(w.values()) == 255).all()
    data = mp.build(lay)
    trees = [(x / 100, y / 100) for chunk in data["scatter"].values() for key, entry in chunk.items()
             if key.split("/")[0] in ("Tree", "PH_Tree", "Mega_Tree") for x, y, *_ in entry["items"]]
    rows, cols = zip(*(world_to_pixel(lay, x, y) for x, y in trees))
    assert (w["forest"][list(rows), list(cols)] > 128).mean() >= 0.6, "forest floor missing under the trees"
    for b in lay.buildings:
        if b["type"] != "gate":
            r, c = world_to_pixel(lay, b["x"], b["y"])
            assert w["dirt"][r, c] == 255, f"grass could grow inside {b['type']} at {b['x']},{b['y']}"
    for camp in lay.camps:
        r, c = world_to_pixel(lay, camp.x, camp.y)
        assert w["dirt"][r, c] == 255, f"grass could grow in {camp.id}"


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
