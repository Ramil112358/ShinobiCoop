"""Checks for the London collage. Run: python Scripts/london/test_london.py
The output checks read SourceArt/World/London/{city,placements}.json, so run make_city.py first."""
import copy
import json
import os
import sys
import tempfile

from shapely.geometry import Point, Polygon
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_city as mc  # noqa: E402

PIECES = {"lamp_modern", "lamp_heritage", "tree_plane", "black_cab", "van", "car_0", "car_1", "car_2", "car_3", "bus",
          "traffic_light", "phone_box", "post_box", "bike_rack", "bus_stop", "bin", "bench", "belisha_beacon",
          "road_barrier"}


def test_layout_rejects_overlap():
    with open(mc.LAYOUT_PATH, encoding="utf-8") as f:
        data = json.load(f)
    bad = copy.deepcopy(data)
    bad["districts"][1]["map"] = bad["districts"][0]["map"]
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as f:
        json.dump(bad, f)
    try:
        mc.load_layout(f.name)
        raise AssertionError("overlapping districts must be rejected")
    except ValueError as e:
        assert "overlap" in str(e)
    finally:
        os.unlink(f.name)


def test_projection_not_mirrored():
    lay = mc.load_layout()
    d, tower = mc.locate(51.50070, -0.12460, lay)  # Elizabeth Tower
    assert d == "WST"
    _, eye = mc.locate(51.50330, -0.11960, lay)
    assert eye[0] > tower[0], "the London Eye is east of Big Ben (+X)"
    assert eye[1] < tower[1], "and north of it (-Y: +Y is south)"
    _, white_tower = mc.locate(51.50810, -0.07590, lay)
    _, bridge = mc.locate(51.50550, -0.07540, lay)
    assert white_tower[1] < bridge[1], "the Tower of London is north of Tower Bridge"
    # Metres: Big Ben to the Eye is ~420 m as the crow flies.
    dist = ((eye[0] - tower[0]) ** 2 + (eye[1] - tower[1]) ** 2) ** 0.5
    assert 380 < dist < 460, dist


def test_heights_and_styles():
    assert mc.building_height({"height": "12 m"}, "stucco") == (0.0, 12.0)
    base, top = mc.building_height({"building:levels": "4"}, "stucco")
    assert base == 0 and abs(top - (4 * mc.LEVEL_M + 1)) < 1e-6
    base, top = mc.building_height({}, "victorian_terrace")
    assert abs(top - (3 * mc.LEVEL_M + 1)) < 1e-6
    assert mc.building_height({"min_height": "20", "height": "21"}, "stucco") == (20.0, 22.5)  # at least 2.5 m tall
    assert mc.parse_metres("40'") == 40 * 0.3048
    assert mc.building_style({"building:material": "glass"}, "CAM", "w1", 0, False) == "modern_glass"
    assert mc.building_style({"building": "church"}, "CAM", "w1", 0, False) == "portland_stone"
    assert mc.building_style({}, "WE", "w1", 80, False) == "modern_glass"
    assert mc.building_style({}, "WST", "w1", 10, True) == "gothic"
    assert mc.building_style({}, "CAM", "w7", 0, False) == mc.building_style({}, "CAM", "w7", 0, False)  # stable


def load_outputs():
    with open(os.path.join(mc.OUT_DIR, "city.json"), encoding="utf-8") as f:
        city = json.load(f)
    with open(os.path.join(mc.OUT_DIR, "placements.json"), encoding="utf-8") as f:
        placements = json.load(f)
    return city, placements


def union_of(city, kind_prefix):
    polys = []
    for chunk in city["chunks"].values():
        for kind, geoms in chunk.items():
            if kind.startswith(kind_prefix):
                polys += [Polygon(rings[0], rings[1:]).buffer(0) for rings in geoms]
    return unary_union(polys)


def test_outputs():
    city, placements = load_outputs()
    water = union_of(city, "water_")
    paving = union_of(city, "paving")
    road = union_of(city, "road")
    assert road.area > 100_000 and paving.area > 300_000
    # Kerbs: paving and road meet along long shared edges (the generator raises paving 15 cm there).
    assert paving.buffer(0.05).intersection(road).area > 1000
    for b in city["buildings"]:
        footprint = Polygon(b["rings"][0], b["rings"][1:]).buffer(0)
        assert footprint.intersection(water).area < 0.5 * footprint.area, f"building {b['id']} stands in water"
        assert b["top"] > b["base"] >= 0
        assert b["style"] in mc.STYLES
        if b["zone"] == "play":
            assert len(b["street"]) == len(b["rings"][0]), f"{b['id']}: one street flag per exterior edge"
    keys = {it["key"] for it in placements["items"]}
    assert keys <= PIECES, f"unknown furniture: {keys - PIECES}"
    m = placements["markers"]
    playable = Polygon(placements["edge_wall"])
    for name, p in [("hub", m["hub"]), ("boss", m["boss"])] + [(c["id"], c) for c in m["camps"]]:
        assert playable.contains(Point(p["x"], p["y"])), f"{name} is outside the playable area"
        assert not water.contains(Point(p["x"], p["y"])), f"{name} is in the water"
    assert len(m["camps"]) == 4 and len({c["id"] for c in m["camps"]}) == 4
    assert sorted(c["difficulty"] for c in m["camps"]) == [1, 2, 3, 4]
    assert len(m["villagers"]) >= 10 and all(len(r) >= 6 for r in m["villagers"])
    assert len(m["pedestrians"]) >= 30 and all(len(r) >= 6 for r in m["pedestrians"])
    for route in m["villagers"] + m["pedestrians"]:
        for x, y, _ in route:
            assert paving.buffer(0.1).contains(Point(x, y)), "villager route point off the pavement"
        for (x0, y0, _), (x1, y1, _) in zip(route, route[1:]):  # consecutive points: a short hop, never across a road
            assert ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5 < 15, "route hop too long"


def test_terrain():
    from PIL import Image
    import numpy as np
    with open(os.path.join(mc.OUT_DIR, "terrain.json"), encoding="utf-8") as f:
        meta = json.load(f)
    assert (meta["pixels"] - 1) % mc.QUADS == 0
    total = sum(np.asarray(Image.open(os.path.join(mc.OUT_DIR, f"weight_{n.lower()}.png")), dtype=np.int32) for n in mc.LAYERS)
    assert (total == 255).all(), "weightmaps must sum to 255"
    h = np.asarray(Image.open(os.path.join(mc.OUT_DIR, "height.png")), dtype=np.int32)
    assert h.shape == (meta["pixels"], meta["pixels"])
    assert ((h - 32768) / 128).min() <= -7.0, "the Thames bed is carved"


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print(f"ok {name}")
