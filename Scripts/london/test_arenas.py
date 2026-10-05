"""Checks for the PvP arena layout. Run: python Scripts/london/test_arenas.py

arena_layout.json is in the same map metres as london_layout.json (+X east, +Y south, origin_offset_m NOT applied;
the arena builder adds it, exactly like build_london.py does). The ground checks (starts and pickups on open street
or pavement, never in a building) read the untracked OSM output city.json from SHINOBI_LONDON_ART (default: the
repo's SourceArt/World/London) and are SKIPPED with a note when that data is absent."""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_city as mc  # noqa: E402

LAYOUT = os.path.join(HERE, "arena_layout.json")
IDS = ["WestEnd", "Camden", "Westminster", "Tower"]
DISTRICT = {"WestEnd": "WE", "Camden": "CAM", "Westminster": "WST", "Tower": "TWR"}
# Landmarks that must fall inside each rect (real lat/lon): catches a rect cut in the wrong part of the district.
MUST_CONTAIN = {
    "WestEnd": [("Piccadilly Circus", 51.5100, -0.1347), ("Leicester Square", 51.5103, -0.1301)],
    "Camden": [("Camden Lock", 51.5416, -0.1465)],
    "Westminster": [("Parliament Square", 51.5010, -0.1270), ("Westminster Bridge (west end)", 51.5008, -0.1235)],
    "Tower": [("Tower of London (White Tower)", 51.5081, -0.0759)],
}
EDGE_MARGIN_M = 20.0  # a rect must stay this far inside its district's cut-out (cut-outs have ragged edges)
START_MARGIN_M = 20.0
PICKUP_TYPES = {"Health": 2, "Attack": 1, "Speed": 1, "Shield": 1}
BUILDING_CLEARANCE_M = 2.0


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def load():
    with open(LAYOUT, encoding="utf-8") as f:
        return json.load(f)["arenas"]


def raw_districts():
    """District id -> raw (offset-free) cut-out rect [x0, y0, x1, y1], read straight from london_layout.json."""
    with open(mc.LAYOUT_PATH, encoding="utf-8") as f:
        lay = json.load(f)
    out = {}
    for d in lay["districts"]:
        (mx, my), (w, h) = d["map"], d["size_m"]
        out[d["id"]] = [mx - w / 2, my - h / 2, mx + w / 2, my + h / 2]
    return out, lay["origin_offset_m"]


def raw_locate(lat, lon):
    """lat/lon -> (district id, raw map point) using make_city's own locate, minus origin_offset_m."""
    lay = mc.load_layout()
    did, p = mc.locate(lat, lon, lay)
    off = lay["origin_offset_m"] if "origin_offset_m" in lay else raw_districts()[1]
    return did, (p[0] - off[0], p[1] - off[1])


def inside(rect, x, y, margin=0.0):
    return rect[0] + margin <= x <= rect[2] - margin and rect[1] + margin <= y <= rect[3] - margin


def centre(rect):
    return ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2)


def test_ids_and_maps():
    arenas = load()
    assert [a["id"] for a in arenas] == IDS, [a["id"] for a in arenas]
    for a in arenas:
        assert a["map"] == f"/Game/Shinobi/Maps/Lvl_Arena_{a['id']}", a["map"]


def test_rects():
    districts, _ = raw_districts()
    for a in load():
        x0, y0, x1, y1 = a["rect"]
        assert x1 > x0 and y1 > y0, f"{a['id']}: rect must be [x0, y0, x1, y1] with x1 > x0, y1 > y0"
        assert 250 <= x1 - x0 <= 350 and 250 <= y1 - y0 <= 350, f"{a['id']}: sides {x1 - x0} x {y1 - y0} m"
        hosts = [d for d, r in districts.items() if inside(r, x0, y0, EDGE_MARGIN_M) and inside(r, x1, y1, EDGE_MARGIN_M)]
        assert hosts == [DISTRICT[a["id"]]], f"{a['id']}: inside districts {hosts} (with {EDGE_MARGIN_M} m margin)"
        for name, lat, lon in MUST_CONTAIN[a["id"]]:
            did, (x, y) = raw_locate(lat, lon)
            assert did == DISTRICT[a["id"]], f"{name} is in {did}"
            assert inside(a["rect"], x, y), f"{a['id']}: rect misses {name} at ({x:.0f}, {y:.0f})"


def test_starts():
    for a in load():
        starts = a["starts"]
        assert set(starts) == {"A", "B"}, a["id"]
        for team in "AB":
            assert len(starts[team]) == 4, f"{a['id']}: team {team} needs 4 starts"
            for x, y, yaw in starts[team]:
                assert inside(a["rect"], x, y, START_MARGIN_M), f"{a['id']} {team}: start ({x}, {y}) closer than {START_MARGIN_M} m to the edge"
                cx, cy = centre(a["rect"])
                want = math.degrees(math.atan2(cy - y, cx - x))
                diff = abs((yaw - want + 180) % 360 - 180)
                assert diff <= 12, f"{a['id']} {team}: yaw {yaw} does not face the centre (want {want:.0f})"
        pa, pb = [(s[0], s[1]) for s in starts["A"]], [(s[0], s[1]) for s in starts["B"]]
        pairs = [dist(p, q) for p in pa for q in pb]
        mean = sum(pairs) / len(pairs)
        assert mean >= 120, f"{a['id']}: A and B are only {mean:.0f} m apart on average"
        assert min(pairs) >= 60, f"{a['id']}: an A and a B start are only {min(pairs):.0f} m apart"
        for team in (pa, pb):
            for i, p in enumerate(team):
                for q in team[i + 1:]:
                    assert dist(p, q) >= 3, f"{a['id']}: two starts of one team stand {dist(p, q):.1f} m apart"
        # opposite sides: each team's centroid sits on its own side of the centre, on opposite sides of it.
        ca = (sum(p[0] for p in pa) / 4, sum(p[1] for p in pa) / 4)
        cb = (sum(p[0] for p in pb) / 4, sum(p[1] for p in pb) / 4)
        c = centre(a["rect"])
        dot = (ca[0] - c[0]) * (cb[0] - c[0]) + (ca[1] - c[1]) * (cb[1] - c[1])
        assert dot < 0, f"{a['id']}: the teams are not on opposite sides of the centre"


def test_pickups():
    for a in load():
        c = centre(a["rect"])
        ps = a["pickups"]
        counts = {}
        for p in ps:
            counts[p["type"]] = counts.get(p["type"], 0) + 1
            assert inside(a["rect"], p["x"], p["y"]), f"{a['id']}: {p['type']} pickup outside the rect"
        assert counts == PICKUP_TYPES, f"{a['id']}: pickups {counts}"
        for i, p in enumerate(ps):
            for q in ps[i + 1:]:
                assert dist((p["x"], p["y"]), (q["x"], q["y"])) >= 15, f"{a['id']}: {p['type']} and {q['type']} pickups bunched"
        # 180-degree symmetry about the rect centre: Health in partner pairs within 15 m.
        health = [(p["x"], p["y"]) for p in ps if p["type"] == "Health"]
        for x, y in health:
            mirror = (2 * c[0] - x, 2 * c[1] - y)
            assert min(dist(mirror, h) for h in health if h != (x, y)) <= 15, f"{a['id']}: Health ({x}, {y}) has no partner"
        assert dist(health[0], health[1]) >= 60, f"{a['id']}: the two Health pickups stand too close"
        # Attack/Speed/Shield are their own partners: they sit within 30 m of the centre line, the line halfway between
        # the teams (perpendicular to the A-B axis through the rect centre), so neither team is closer to them.
        ca = [sum(s[i] for s in a["starts"]["A"]) / 4 for i in (0, 1)]
        cb = [sum(s[i] for s in a["starts"]["B"]) / 4 for i in (0, 1)]
        axis = (cb[0] - ca[0], cb[1] - ca[1])
        n = math.hypot(*axis)
        for p in ps:
            if p["type"] == "Health":
                continue
            off_line = abs((p["x"] - c[0]) * axis[0] + (p["y"] - c[1]) * axis[1]) / n
            assert off_line <= 30, f"{a['id']}: {p['type']} is {off_line:.0f} m off the centre line"


def ground():
    """(footprints STRtree-ish list, open ground, water) in raw map metres, or None when the OSM data is absent."""
    import arena_plan
    art = arena_plan.art_dir()
    if not os.path.exists(os.path.join(art, "city.json")):
        return None
    return arena_plan.load_ground(art)


def test_open_ground():
    g = ground()
    if g is None:
        print("SKIP test_open_ground: no city.json (set SHINOBI_LONDON_ART or build SourceArt/World/London)")
        return
    from shapely.geometry import Point
    for a in load():
        spots = [(f"start {t}{i}", s[0], s[1]) for t in "AB" for i, s in enumerate(a["starts"][t])]
        spots += [(f"{p['type']} pickup", p["x"], p["y"]) for p in a["pickups"]]
        for what, x, y in spots:
            pt = Point(x, y)
            assert g.open_ground.contains(pt), f"{a['id']}: {what} ({x}, {y}) is not on street, plaza or pavement"
            assert not g.water.intersects(pt.buffer(1.0)), f"{a['id']}: {what} is at the water"
            near = g.buildings_near(x, y, BUILDING_CLEARANCE_M)
            assert not near, f"{a['id']}: {what} ({x}, {y}) is within {BUILDING_CLEARANCE_M} m of building {near[0]}"
        # one connected walkable piece (open ground minus buildings, eroded 0.7 m): no team starts sealed in a courtyard.
        pieces = g.walkable(a["rect"])
        homes = set()
        for what, x, y in spots:
            at = [i for i, poly in enumerate(pieces) if poly.contains(Point(x, y))]
            assert at, f"{a['id']}: {what} ({x}, {y}) is not on walkable ground"
            homes.add(at[0])
        assert len(homes) == 1, f"{a['id']}: starts and pickups sit in {len(homes)} disconnected pieces of ground"


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print(f"ok {name}")
