"""The London collage, computed offline from london_layout.json and the cached OSM data (fetch_osm.py).

  python Scripts/london/make_city.py [--plan] [--districts]

Coordinates: metres, +X east, +Y SOUTH (UE's left-handed axes, so the real north-up map isn't mirrored in game).
Each district is a real north-up cut-out: its OSM features are projected around its real centre and moved to its map
position. Buildings whose centroid lies inside a cut-out are playable; the margin ring outside the collage becomes the
cheap backdrop (each spot owned by the nearest district, so margins never overlap). Connector regions between the
cut-outs get generated roads lined with plots, the Thames bend, parks and paving; district roads that end at a cut-out
edge are extended to the nearest connector road.

Writes to SourceArt/World/London/ (rebuilt, not committed):
  city.json       buildings (footprints, heights, style, per-edge street detail), ground per 128 m chunk (road, paving,
                  water by level, white and yellow markings), bridge decks, walls along the playable edge
  placements.json markers (hub, camps, boss, villagers, tour points, landmarks, skyline) and street furniture
  height.png, weight_<layer>.png, terrain.json   the landscape (parks, river and canal beds), same encoding as
                  Scripts/world/make_heightmap.py
  plan.png        (--plan) top-down review image
Needs numpy, Pillow and shapely (system Python, not UE's). Deterministic.
"""
import hashlib
import json
import math
import os
import re
import sys
from collections import Counter

import numpy as np
import shapely
from shapely.geometry import LineString, MultiPoint, Point, Polygon, box
from shapely.geometry.polygon import orient
from shapely.ops import linemerge, nearest_points, polygonize, unary_union

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", ".."))
OSM_DIR = os.path.join(PROJECT, "SourceArt", "World", "London", "osm")
OUT_DIR = os.path.join(PROJECT, "SourceArt", "World", "London")
LAYOUT_PATH = os.path.join(HERE, "london_layout.json")
M_PER_DEG = 111_320.0
LEVEL_M = 3.3
CHUNK_M = 128.0
PAVEMENT_M = 3.5
QUADS = 63  # landscape quads per component (UShinobiWorldBuilderLibrary::QuadsPerSection)
BED_DEPTH_M = 2.5
LAYERS = ["Grass", "Soil", "Riverbed"]

# Carriageway width (m) by OSM highway class; others (footways, steps, ...) are covered by paving.
ROAD_WIDTH = {"motorway": 14, "trunk": 13, "primary": 12, "secondary": 10, "tertiary": 9, "unclassified": 7,
              "residential": 7, "living_street": 6, "service": 4.5, "motorway_link": 7, "trunk_link": 7,
              "primary_link": 7, "secondary_link": 7, "tertiary_link": 7, "busway": 7}
CENTRE_LINE = {"trunk", "primary", "secondary", "tertiary", "unclassified", "residential"}
DOUBLE_YELLOW = {"tertiary", "unclassified", "residential", "living_street", "secondary"}
PARKING = {"tertiary", "unclassified", "residential"}
EXTEND = {"trunk", "primary", "secondary", "tertiary"}  # side streets just end at a cut-out edge
PROMENADE_M = 6.0  # paved walk along water in parks
PLAZA_HIGHWAYS = {"pedestrian"}
GREEN = {("leisure", "park"), ("leisure", "garden"), ("leisure", "pitch"), ("leisure", "playground"),
         ("landuse", "grass"), ("landuse", "recreation_ground"), ("landuse", "village_green"), ("landuse", "flowerbed"),
         ("natural", "scrub"), ("natural", "wood")}
WATER_LEVEL = {"river": -5.0, "canal": -1.6, "lock": -1.6, "basin": -3.0, "dock": -3.0, "reflecting_pool": -0.4,
               "pond": -0.6, "lake": -0.6, "fountain": -0.4}
STYLES = ("georgian_terrace", "victorian_terrace", "stucco", "portland_stone", "gothic", "warehouse", "modern_glass",
          "concrete_modern")
DEFAULT_LEVELS = {"georgian_terrace": 4, "victorian_terrace": 3, "stucco": 4, "portland_stone": 5, "gothic": 6,
                  "warehouse": 5, "modern_glass": 8, "concrete_modern": 6}
# Frontage plots (connector roads): style -> (plot width range, depth range) in metres.
PLOT_SIZE = {"portland_stone": ((16, 28), (18, 26)), "concrete_modern": ((20, 34), (16, 24)),
             "modern_glass": ((22, 34), (18, 26)), "warehouse": ((14, 24), (14, 20))}
# District style mix for buildings OSM says nothing about (weights).
DISTRICT_MIX = {
    "WST": {"portland_stone": 5, "stucco": 1, "georgian_terrace": 2, "victorian_terrace": 1, "modern_glass": 1},
    "TWR": {"warehouse": 3, "modern_glass": 3, "portland_stone": 2, "victorian_terrace": 1, "concrete_modern": 1},
    "WE": {"portland_stone": 3, "stucco": 2, "georgian_terrace": 3, "victorian_terrace": 2},
    "CAM": {"victorian_terrace": 5, "georgian_terrace": 2, "stucco": 2, "warehouse": 1},
}
FURNITURE = {"bench": "bench", "telephone": "phone_box", "post_box": "post_box", "waste_basket": "bin",
             "bicycle_parking": "bike_rack", "street_lamp": "lamp_modern", "bus_stop": "bus_stop"}


# ------------------------------------------------------------------------------------------------------------ layout
def _shift(p, off):
    return [p[0] + off[0], p[1] + off[1]]


def load_layout(path=LAYOUT_PATH):
    """The layout with origin_offset_m applied to every map coordinate; validates districts and regions."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    off = data.get("origin_offset_m", [0, 0])
    for d in data["districts"]:
        d["map"] = _shift(d["map"], off)
    for r in data["regions"]:
        x0, y0, x1, y1 = r["rect"]
        r["rect"] = [x0 + off[0], y0 + off[1], x1 + off[0], y1 + off[1]]
    data["river"]["line"] = [_shift(p, off) for p in data["river"]["line"]]
    data["green"] = [[g[0] + off[0], g[1] + off[1], g[2] + off[0], g[3] + off[1]] for g in data.get("green", [])]
    for road in data["roads"]:
        road["line"] = [_shift(p, off) for p in road["line"]]
    for s in data.get("skyline", []):
        s["map"] = _shift(s["map"], off)
    areas = [(d["id"], cut_rect(d)) for d in data["districts"]] + [(r["id"], box(*r["rect"])) for r in data["regions"]]
    seen = set()
    for i, (a, ga) in enumerate(areas):
        if a in seen:
            raise ValueError(f"duplicate id {a}")
        seen.add(a)
        for b, gb in areas[i + 1:]:
            if ga.intersection(gb).area > 1.0:
                raise ValueError(f"{a} and {b} overlap on the map")
    return data


def cut_rect(d):
    (mx, my), (w, h) = d["map"], d["size_m"]
    return box(mx - w / 2, my - h / 2, mx + w / 2, my + h / 2)


def project(lat, lon, d):
    """Real lat/lon -> map metres for district d (+X east, +Y south)."""
    east = (lon - d["lon"]) * M_PER_DEG * math.cos(math.radians(d["lat"]))
    north = (lat - d["lat"]) * M_PER_DEG
    return d["map"][0] + east, d["map"][1] - north


def locate(lat, lon, lay):
    """Real lat/lon -> (district id, map point) for the district whose cut-out contains it."""
    for d in lay["districts"]:
        p = project(lat, lon, d)
        if cut_rect(d).contains(Point(p)):
            return d["id"], p
    raise ValueError(f"{lat}, {lon} is in no district's cut-out")


# --------------------------------------------------------------------------------------------------------- geometry
def coords(el, d):
    return [project(p["lat"], p["lon"], d) for p in el.get("geometry") or [] if p]


def way_polygon(el, d):
    pts = coords(el, d)
    if len(pts) < 4 or pts[0] != pts[-1]:
        return None
    poly = Polygon(pts)
    return poly if poly.is_valid else poly.buffer(0)


def relation_polygon(el, d):
    """Multipolygon relation -> (multi)polygon: member ways merged into rings, outers minus inners."""
    rings = {"outer": [], "inner": []}
    for m in el.get("members", []):
        if m.get("type") == "way" and m.get("geometry"):
            role = "inner" if m.get("role") == "inner" else "outer"
            pts = [project(p["lat"], p["lon"], d) for p in m["geometry"] if p]
            if len(pts) >= 2:
                rings[role].append(LineString(pts))
    if not rings["outer"]:
        return None
    outer = unary_union(list(polygonize(linemerge(rings["outer"]))))
    if rings["inner"]:
        outer = outer.difference(unary_union(list(polygonize(linemerge(rings["inner"])))))
    return None if outer.is_empty else outer.buffer(0)


def area_of(el, d):
    return way_polygon(el, d) if el["type"] == "way" else relation_polygon(el, d)


def stable(*keys):
    """Deterministic 0..1 from keys (Python's hash() is salted per run)."""
    h = hashlib.sha1("|".join(str(k) for k in keys).encode()).digest()
    return int.from_bytes(h[:4], "little") / 2**32


def polygons(g):
    """Polygons of any geometry (collections from clipping included)."""
    if g is None or g.is_empty:
        return []
    if g.geom_type == "Polygon":
        return [g]
    return [p for part in getattr(g, "geoms", []) for p in polygons(part)]


def lines(g):
    if g is None or g.is_empty:
        return []
    if g.geom_type == "LineString":
        return [g]
    return [ln for part in getattr(g, "geoms", []) for ln in lines(part)]


def rings_of(p, precision=2):
    """Polygon -> [exterior, hole, ...], each [[x, y], ...] (open ring), exterior counter-clockwise in x/y numbers."""
    p = orient(p, 1.0)
    return [[[round(x, precision), round(y, precision)] for x, y in r.coords[:-1]] for r in [p.exterior, *p.interiors]]


def export_geom(g, min_area=0.5):
    return [rings_of(p) for p in polygons(g) if p.area >= min_area]


# -------------------------------------------------------------------------------------------------------- buildings
def parse_metres(text):
    """OSM length ("12", "12 m", "12.5m", "40'") -> metres, None if unparsable."""
    if text is None:
        return None
    m = re.match(r"\s*([0-9]+(?:\.[0-9]+)?)", str(text).replace(",", "."))
    if not m:
        return None
    value = float(m.group(1))
    return value * 0.3048 if "'" in str(text) or "ft" in str(text) else value


def building_height(tags, style):
    """(base, top) in metres: height tag, else levels x 3.3 m (+ roof levels), else the style default."""
    base = parse_metres(tags.get("min_height"))
    if base is None:
        base = (parse_metres(tags.get("building:min_level")) or 0.0) * LEVEL_M
    top = parse_metres(tags.get("height"))
    if top is None:
        levels = parse_metres(tags.get("building:levels"))
        if levels is None:
            levels = DEFAULT_LEVELS[style]
        top = base + (levels + (parse_metres(tags.get("roof:levels")) or 0) * 0.7) * LEVEL_M + 1.0
    return base, max(top, base + 2.5)


def building_style(tags, district_id, osm_id, top_m, in_gothic):
    if in_gothic:
        return "gothic"
    material = tags.get("building:material", tags.get("building:facade:material", "")).lower()
    colour = tags.get("building:colour", "").lower()
    kind = tags.get("building", tags.get("building:part", "")).lower()
    if material in ("glass", "metal", "steel", "glass;metal"):
        return "modern_glass"
    if material in ("concrete", "reinforced_concrete"):
        return "concrete_modern"
    if material in ("stone", "sandstone", "limestone", "granite", "marble"):
        return "portland_stone"
    if material in ("plaster", "render", "stucco", "cement_block"):
        return "stucco"
    if top_m > 40:
        return "modern_glass"
    if kind in ("cathedral", "church", "chapel", "government", "civic", "public", "palace", "museum", "train_station"):
        return "portland_stone"
    if kind in ("warehouse", "industrial"):
        return "warehouse"
    if material == "brick":
        return "victorian_terrace" if colour.startswith(("#8", "#9", "#a", "red", "brown", "maroon")) else "georgian_terrace"
    if colour in ("white", "#ffffff", "cream", "beige", "ivory"):
        return "stucco"
    mix = DISTRICT_MIX[district_id]
    pick = stable("style", osm_id) * sum(mix.values())
    for style, weight in mix.items():
        pick -= weight
        if pick < 0:
            return style
    return next(iter(mix))


def roof_kind(tags):
    shape = tags.get("roof:shape", "")
    return shape if shape in ("gabled", "hipped", "pyramidal", "mansard", "dome", "skillion", "half-hipped") else "flat"


# ------------------------------------------------------------------------------------------------------- districts
def district_features(d, margin_m):
    """Every feature of one district in map metres, unclipped (the cut-out plus margin): buildings with style and
    heights, roads, plazas, water, green, trees, furniture points."""
    with open(os.path.join(OSM_DIR, f"{d['id']}.json"), encoding="utf-8") as f:
        elements = json.load(f)["elements"]
    outer = cut_rect(d).buffer(margin_m, join_style=2)
    outlines, parts, roads, plazas, water, green, trees, points, gothic = [], [], [], [], [], [], [], [], []
    for el in elements:
        tags = el.get("tags", {})
        if el["type"] == "node":
            p = project(el["lat"], el["lon"], d)
            if tags.get("natural") == "tree":
                trees.append(p)
            else:
                kind = tags.get("highway") or tags.get("amenity") or ("shop" if "shop" in tags else None)
                points.append({"kind": kind, "x": p[0], "y": p[1], "crossing": tags.get("crossing", ""),
                               "name": tags.get("name", "")})
            continue
        is_building = tags.get("building", "no") != "no"
        is_part = tags.get("building:part", "no") != "no"
        if is_building or is_part:
            if tags.get("location") == "underground" or tags.get("layer", "0").startswith("-"):
                continue
            poly = area_of(el, d)
            if poly is None or poly.area < 4.0:
                continue
            item = {"id": f"{el['type'][0]}{el['id']}", "poly": poly, "tags": tags}
            (outlines if is_building else parts).append(item)
            if tags.get("name", "").startswith("Palace of Westminster") or tags.get("wikidata") == "Q62408":
                gothic.append(poly)
            continue
        hw = tags.get("highway")
        if hw and el["type"] == "way":
            if tags.get("tunnel") in ("yes", "building_passage") or tags.get("layer", "0").startswith("-"):
                continue
            if hw in PLAZA_HIGHWAYS and tags.get("area") == "yes":
                poly = way_polygon(el, d)
                if poly is not None:
                    plazas.append(poly)
            elif hw in ROAD_WIDTH:
                pts = coords(el, d)
                if len(pts) >= 2:
                    width = ROAD_WIDTH[hw]
                    lanes = parse_metres(tags.get("lanes"))
                    if lanes:
                        width = max(4.5, min(width + 3, lanes * 3.2 + 1.0))
                    roads.append({"line": LineString(pts), "width": width, "class": hw, "name": tags.get("name", ""),
                                  "bridge": tags.get("bridge", "no") != "no", "oneway": tags.get("oneway") == "yes"})
            elif hw in PLAZA_HIGHWAYS:
                pts = coords(el, d)
                if len(pts) >= 2:
                    plazas.append(LineString(pts).buffer(3.0))
            continue
        if tags.get("natural") == "water" or tags.get("waterway") in ("riverbank", "dock"):
            poly = area_of(el, d)
            if poly is not None and poly.area > 50:
                kind = tags.get("water") or tags.get("waterway") or "pond"
                water.append({"poly": poly, "kind": kind})
            continue
        if tags.get("place") == "square":
            poly = area_of(el, d)
            if poly is not None:
                plazas.append(poly)
            continue
        if any((k, tags.get(k)) in GREEN for k in ("leisure", "landuse", "natural")):
            poly = area_of(el, d)
            if poly is not None:
                green.append(poly)
    gothic_zone = unary_union(gothic).buffer(2.0) if gothic else None
    # OSM simple 3D buildings: an outline drawn with parts is shown by its parts only.
    part_union = unary_union([p["poly"] for p in parts]) if parts else None
    buildings = []
    for item in outlines + parts:
        poly, tags = item["poly"], item["tags"]
        is_part = item in parts
        if not is_part and part_union is not None and poly.intersection(part_union).area > 0.5 * poly.area:
            continue
        c = poly.representative_point()
        if not outer.contains(c):
            continue
        in_gothic = gothic_zone is not None and gothic_zone.contains(c)
        style = building_style(tags, d["id"], item["id"], parse_metres(tags.get("height")) or 0, in_gothic)
        base, top = building_height(tags, style)
        buildings.append({"id": item["id"], "district": d["id"], "poly": poly, "base": base, "top": top, "style": style,
                          "roof": roof_kind(tags), "colour": (tags.get("building:colour") or "").lower() or None,
                          "name": tags.get("name", ""), "part": is_part})
    return {"buildings": buildings, "roads": roads, "plazas": plazas, "water": water, "green": green, "trees": trees,
            "points": points}


def territories(lay, playable, backdrop):
    """District id -> the area it owns: its cut-out plus the backdrop spots nearer to it than to any other cut-out."""
    rects = {d["id"]: cut_rect(d) for d in lay["districts"]}
    seeds, owners = [], []
    for did, rect in rects.items():
        ring = rect.exterior
        for s in np.arange(0, ring.length, 10.0):
            seeds.append(ring.interpolate(s))
            owners.append(did)
    cells = shapely.voronoi_polygons(MultiPoint(seeds), extend_to=backdrop.buffer(500).envelope, ordered=True)
    own = {did: [] for did in rects}
    for cell, did in zip(cells.geoms, owners):
        own[did].append(cell)
    return {did: unary_union([rects[did], unary_union(own[did]).intersection(backdrop)]) for did in rects}


# ------------------------------------------------------------------------------------------------------------ build
def frontage_plots(road, width, side, style, blocked, playable, seed):
    """Plots along one side of a connector road: rectangles from the back of the pavement, skipping blocked ground."""
    line = LineString(road)
    dist = width / 2 + PAVEMENT_M
    edge = line.offset_curve(dist if side == "left" else -dist)
    if edge.is_empty or edge.geom_type != "LineString":
        return []
    (wmin, wmax), (dmin, dmax) = PLOT_SIZE.get(style, ((6, 11), (12, 16)))
    out, s, i = [], 0.0, 0
    taken = blocked
    while s < edge.length - wmin:
        w = wmin + (wmax - wmin) * stable(seed, side, i, "w")
        depth = dmin + (dmax - dmin) * stable(seed, side, i, "d")
        a, b = edge.interpolate(s), edge.interpolate(min(s + w, edge.length))
        dx, dy = b.x - a.x, b.y - a.y
        n = math.hypot(dx, dy) or 1.0
        # Away from the road: the side of the edge opposite the centreline.
        nx, ny = -dy / n, dx / n
        mid = Point((a.x + b.x) / 2 + nx, (a.y + b.y) / 2 + ny)
        if line.distance(mid) < dist:
            nx, ny = -nx, -ny
        quad = Polygon([(a.x, a.y), (b.x, b.y), (b.x + nx * depth, b.y + ny * depth), (a.x + nx * depth, a.y + ny * depth)])
        if quad.is_valid and playable.contains(quad) and not quad.intersects(taken):
            out.append(quad)
            taken = taken.union(quad.buffer(0.05))
        s += w
        i += 1
    return out


def extend_ends(play_roads, rect, targets, max_m=160.0):
    """Lengthens each road that stops at the cut-out edge, straight on, to the first connector road it meets."""
    edge = rect.exterior
    for r in play_roads:
        pts = list(r["line"].coords)
        for end in (0, -1):
            p = Point(pts[end])
            if edge.distance(p) > 0.5:
                continue
            q = Point(pts[1] if end == 0 else pts[-2])
            dx, dy = p.x - q.x, p.y - q.y
            n = math.hypot(dx, dy)
            if n < 1e-6:
                continue
            ray = LineString([(p.x, p.y), (p.x + dx / n * max_m, p.y + dy / n * max_m)])
            hit = ray.intersection(targets)
            if hit.is_empty:
                continue
            nearest = min((g for g in getattr(hit, "geoms", [hit])), key=lambda g: p.distance(g))
            point = nearest_points(p, nearest)[1]
            if p.distance(point) < 0.5:
                continue
            if end == 0:
                pts.insert(0, (point.x, point.y))
            else:
                pts.append((point.x, point.y))
        r["line"] = LineString(pts)


def junctions(road_lines):
    """Points where three or more road segments meet."""
    merged = unary_union(road_lines)
    count = Counter()
    for seg in lines(merged):
        for c in (seg.coords[0], seg.coords[-1]):
            count[(round(c[0], 1), round(c[1], 1))] += 1
    return [Point(c) for c, n in count.items() if n >= 3]


def dashes(line, on, off):
    out, s = [], 0.0
    while s < line.length:
        seg = shapely.ops.substring(line, s, min(s + on, line.length))
        if seg.length > 0.2:
            out.append(seg)
        s += on + off
    return out


def yaw_of(line, at_point):
    """Direction (degrees, atan2 in map axes) of a line near a point."""
    s = line.project(at_point)
    a, b = line.interpolate(max(0, s - 1)), line.interpolate(min(line.length, s + 1))
    return math.degrees(math.atan2(b.y - a.y, b.x - a.x))


def build(lay):
    rects = {d["id"]: cut_rect(d) for d in lay["districts"]}
    regions = {r["id"]: box(*r["rect"]) for r in lay["regions"]}
    playable = unary_union(list(rects.values()) + list(regions.values()))
    margin = lay["margin_m"]
    backdrop = playable.buffer(margin, join_style=2).difference(playable)
    terr = territories(lay, playable, backdrop)
    clear = []
    for lm in lay["landmarks"]:
        if lm["clear_m"] > 0:
            clear.append(Point(locate(lm["lat"], lm["lon"], lay)[1]).buffer(lm["clear_m"]))
    for s in lay.get("skyline", []):
        clear.append(Point(s["map"]).buffer(s["clear_m"]))
    clear = unary_union(clear)

    buildings, play_roads, back_roads, water, green, back_green, plazas, trees, points = [], [], [], [], [], [], [], [], []
    for d in lay["districts"]:
        f = district_features(d, margin)
        rect, own = rects[d["id"]], terr[d["id"]]
        own_back = own.difference(playable)
        for w in f["water"]:
            g = w["poly"].intersection(own)
            if not g.is_empty:
                water.append((g, WATER_LEVEL.get(w["kind"], -0.6), w["kind"], d["id"]))
        water_here = unary_union([w["poly"] for w in f["water"]]) if f["water"] else Polygon()
        for b in f["buildings"]:
            c = b["poly"].representative_point()
            if b["poly"].intersection(water_here).area > 0.4 * b["poly"].area or clear.contains(c):
                continue  # ships, piers, bridge towers (landmark pieces), cleared landmark ground
            if rect.contains(c):
                b["zone"] = "play"
                b["poly"] = b["poly"].intersection(rect.buffer(-0.5)) if not rect.contains(b["poly"]) else b["poly"]
            elif own_back.contains(c) and not b["poly"].intersects(playable):
                b["zone"] = "backdrop"
            else:
                continue
            if polygons(b["poly"]):
                buildings.append(b)
        for r in f["roads"]:
            for ln in lines(r["line"].intersection(rect)):
                play_roads.append({**r, "line": ln, "district": d["id"]})
            for ln in lines(r["line"].intersection(own_back)):
                back_roads.append({**r, "line": ln})
        green += [g.intersection(rect) for g in f["green"]]
        back_green += [g.intersection(own_back) for g in f["green"]]
        plazas += [p.intersection(rect) for p in f["plazas"]]
        trees += [t for t in f["trees"] if own.contains(Point(t))]
        points += [{**p, "district": d["id"]} for p in f["points"] if rect.contains(Point(p["x"], p["y"]))]

    # Connectors: the Thames bend, parks, generated roads.
    river = lay["river"]
    bend = LineString(river["line"]).buffer(river["width_m"] / 2, cap_style=2).intersection(unary_union(list(regions.values())))
    water.append((bend, WATER_LEVEL["river"], "river", "connector"))
    # A river piece left in the backdrop must flow off the map (reach the backdrop's outer edge); otherwise it's a
    # stray stub of real Thames the collage skipped: drop it.
    outer_edge = playable.buffer(margin, join_style=2).exterior.buffer(3)
    kept = []
    for g, level, kind, owner in water:
        if kind == "river" and owner != "connector":
            g = unary_union([p for p in polygons(g) if p.intersection(playable).area > 1 or p.intersects(outer_edge)])
        if not g.is_empty:
            kept.append((g, level, kind))
    water = kept
    connector_roads = []
    for road in lay["roads"]:
        cls = road["class"]
        width = ROAD_WIDTH.get(cls, 0)
        connector_roads.append({"line": LineString(road["line"]), "width": width, "class": cls, "name": road["id"],
                                "bridge": False, "oneway": False, "district": "connector", "spec": road})
    targets = unary_union([r["line"] for r in connector_roads if r["width"] > 0])
    for did, rect in rects.items():
        extend_ends([r for r in play_roads if r["district"] == did and r["class"] in EXTEND], rect, targets)

    water_all = unary_union([w[0] for w in water])
    all_play = [r for r in play_roads + connector_roads if r["width"] > 0]
    carriage = unary_union([r["line"].buffer(r["width"] / 2) for r in all_play]).intersection(playable)
    bridges = []
    for r in all_play:
        if r["bridge"]:
            deck = r["line"].intersection(water_all.buffer(6))
            for ln in lines(deck):
                if ln.length > 20:
                    bridges.append({"name": r["name"], "line": [[round(x, 2), round(y, 2)] for x, y in ln.coords],
                                    "width": r["width"] + 2 * PAVEMENT_M})
    carriage = carriage.difference(water_all)
    back_carriage = unary_union([r["line"].buffer(r["width"] / 2) for r in back_roads]).intersection(backdrop).difference(water_all)

    # Frontage plots along connector roads.
    blocked = unary_union([carriage.buffer(PAVEMENT_M - 0.1), water_all.buffer(2), clear,
                           *[rect.buffer(1.0) for rect in rects.values()]])
    for r in connector_roads:
        for side, style in r["spec"].get("frontage", {}).items():
            for i, quad in enumerate(frontage_plots(r["line"].coords, r["width"], side, style, blocked, playable, r["name"])):
                levels = DEFAULT_LEVELS[style] + int(stable(r["name"], side, i, "lv") * 3) - 1
                buildings.append({"id": f"g_{r['name'].replace(' ', '_')}_{side}_{i}", "district": "connector",
                                  "poly": quad, "base": 0.0, "top": levels * LEVEL_M + 1.0, "style": style, "roof": "flat",
                                  "colour": None, "name": "", "part": False, "zone": "play"})
            blocked = blocked.union(unary_union([b["poly"] for b in buildings if b["district"] == "connector"]))

    for b in buildings:  # OSM traces carry many tiny jogs: each would be its own facade
        b["poly"] = b["poly"].simplify(0.15)
    play_b = unary_union([b["poly"] for b in buildings if b["zone"] == "play"])
    back_b = unary_union([b["poly"] for b in buildings if b["zone"] == "backdrop"])
    parks = [regions[r["id"]] for r in lay["regions"] if r["surface"] == "park"] + [box(*g) for g in lay["green"]]
    kerbside = carriage.buffer(PAVEMENT_M)
    green_play = unary_union(green + parks).difference(kerbside).difference(water_all.buffer(PROMENADE_M)).difference(play_b)
    green_play = green_play.difference(unary_union(plazas)) if plazas else green_play
    paving = playable.difference(carriage).difference(water_all).difference(green_play).difference(play_b)
    green_back = unary_union(back_green).difference(back_carriage.buffer(PAVEMENT_M)).difference(water_all).difference(back_b)
    paving_back = backdrop.difference(back_carriage).difference(water_all).difference(green_back).difference(back_b)

    city = {"lay": lay, "rects": rects, "regions": regions, "playable": playable, "backdrop": backdrop,
            "buildings": buildings, "play_roads": play_roads, "connector_roads": connector_roads, "back_roads": back_roads,
            "carriage": carriage, "back_carriage": back_carriage, "paving": paving, "paving_back": paving_back,
            "green": green_play, "green_back": green_back, "water": water, "water_all": water_all, "bridges": bridges,
            "trees": trees, "points": points, "clear": clear}
    city["street_detail"] = street_detail(buildings)
    city["markings"], city["zebras"], city["junctions"] = markings(city)
    city["placements"] = placements(city)
    return city


def street_detail(buildings):
    """Per playable building, per exterior edge: True if open ground (street, yard, park) lies 3 m in front of it,
    False for party walls against a neighbour (those get no windows)."""
    play = [b for b in buildings if b["zone"] == "play"]
    union = unary_union([b["poly"] for b in play])
    shapely.prepare(union)
    out = {}
    for b in play:
        flags = []
        for p in polygons(b["poly"]):
            ring = rings_of(p)[0]
            xs, ys = [], []
            for i, (x0, y0) in enumerate(ring):
                x1, y1 = ring[(i + 1) % len(ring)]
                ex, ey = x1 - x0, y1 - y0
                n = math.hypot(ex, ey) or 1.0
                # Counter-clockwise exterior (x/y numbers): the outward normal is (ey, -ex).
                xs.append((x0 + x1) / 2 + ey / n * 3.0)
                ys.append((y0 + y1) / 2 - ex / n * 3.0)
            flags.append([not v for v in shapely.contains_xy(union, np.array(xs), np.array(ys))])
        out[b["id"]] = flags
    return out


def markings(city):
    """White and yellow road paint as thin polygons: dashed centre lines, double yellow lines along the kerb, zebra
    crossings; all kept off junction mouths and inside the carriageway."""
    roads = [r for r in city["play_roads"] + city["connector_roads"] if r["width"] > 0]
    junction_pts = junctions([r["line"] for r in roads])
    mouth = unary_union([p.buffer(11) for p in junction_pts]) if junction_pts else Polygon()
    paintable = city["carriage"].difference(mouth)
    white, yellow = [], []
    for r in roads:
        if r["class"] in CENTRE_LINE and not r["oneway"] and r["width"] >= 7:
            for ln in lines(r["line"].difference(mouth)):
                white += [d.buffer(0.07, cap_style=2) for d in dashes(ln, 4.0, 5.0)]
        if r["class"] in DOUBLE_YELLOW:
            for side in (1, -1):
                for off in (r["width"] / 2 - 0.35, r["width"] / 2 - 0.55):
                    ln = r["line"].offset_curve(side * off)
                    if not ln.is_empty:
                        yellow.append(ln.buffer(0.05, cap_style=2))
    zebras = []
    tree = shapely.STRtree([r["line"] for r in roads])
    for p in city["points"]:
        if p["kind"] != "crossing" or p["crossing"] not in ("zebra", "uncontrolled", "marked", "traffic_signals"):
            continue
        pt = Point(p["x"], p["y"])
        if not city["carriage"].contains(pt):
            continue
        r = roads[int(tree.nearest(pt))]
        yaw = math.radians(yaw_of(r["line"], pt))
        ax, ay = math.cos(yaw), math.sin(yaw)  # along the road
        cx, cy = -ay, ax  # across
        half = r["width"] / 2
        if p["crossing"] == "traffic_signals":
            for k in (-1, 1):  # two rows of studs: stop lines either side
                q = Point(pt.x + ax * k * 2.0, pt.y + ay * k * 2.0)
                white.append(LineString([(q.x - cx * half, q.y - cy * half), (q.x + cx * half, q.y + cy * half)]).buffer(0.15, cap_style=2))
            continue
        n = int(r["width"] / 1.2)
        for k in range(n):
            o = -half + 0.6 + k * 1.2
            q = (pt.x + cx * o, pt.y + cy * o)
            white.append(LineString([(q[0] - ax * 1.25, q[1] - ay * 1.25), (q[0] + ax * 1.25, q[1] + ay * 1.25)]).buffer(0.3, cap_style=2))
        zebras.append({"x": pt.x, "y": pt.y, "yaw": math.degrees(yaw), "width": r["width"],
                       "beacon": p["crossing"] == "zebra" or p["district"] in ("WE", "CAM")})
    white_g = unary_union(white).intersection(paintable) if white else Polygon()
    yellow_g = unary_union(yellow).intersection(paintable) if yellow else Polygon()
    zebra_zone = unary_union([Point(z["x"], z["y"]).buffer(z["width"] / 2 + 2) for z in zebras]) if zebras else Polygon()
    yellow_g = yellow_g.difference(zebra_zone)
    return {"white": white_g, "yellow": yellow_g}, zebras, junction_pts


# ------------------------------------------------------------------------------------------------------- placements
def placements(city):
    """Markers and street furniture: {"markers": {...}, "items": [{"key", "x", "y", "z", "yaw"}]} (metres, degrees)."""
    lay = city["lay"]
    paving, carriage, green = city["paving"], city["carriage"], city["green"]
    walk = unary_union([paving, green])
    for g in (paving, carriage, walk):
        shapely.prepare(g)
    items = []

    def put(key, x, y, yaw=0.0, z=None, scale=1.0):
        if z is None:
            z = 0.15 if paving.contains(Point(x, y)) else 0.0
        items.append({"key": key, "x": round(x, 2), "y": round(y, 2), "z": z, "yaw": round(yaw, 1), "scale": round(scale, 3)})

    roads = [r for r in city["play_roads"] + city["connector_roads"]]
    mouth = unary_union([p.buffer(8) for p in city["junctions"]]) if city["junctions"] else Polygon()
    zebra_zone = unary_union([Point(z["x"], z["y"]).buffer(10) for z in city["zebras"]]) if city["zebras"] else Polygon()
    for r in roads:
        spec = r.get("spec", {})
        heritage = spec.get("heritage_lamps", False)
        half = r["width"] / 2 if r["width"] else 3.0
        step = 26.0
        for side in (1, -1):
            edge = r["line"].offset_curve(side * (half + 0.7))
            if edge.is_empty or edge.geom_type != "LineString":
                continue
            s = step / 2 if side == 1 else step
            while s < edge.length:
                p = edge.interpolate(s)
                if paving.contains(p) and not mouth.contains(p):
                    put("lamp_heritage" if heritage else "lamp_modern", p.x, p.y, yaw_of(r["line"], p) + 90 * side)
                s += step
            if spec.get("trees"):
                tree_edge = r["line"].offset_curve(side * (half + 2.2))
                if tree_edge.is_empty or tree_edge.geom_type != "LineString":
                    continue
                s = 6.0
                while s < tree_edge.length:
                    p = tree_edge.interpolate(s)
                    if walk.contains(p) and not mouth.contains(p):
                        put("tree_plane", p.x, p.y, 360 * stable("ct", r["name"], side, s), scale=0.85 + 0.3 * stable("cs", r["name"], side, s))
                    s += 12.0
        # Parked cars and cabs on quieter streets, off junctions and crossings.
        if r["class"] in PARKING and r["width"] >= 7:
            for side in (1, -1):
                lane = r["line"].offset_curve(side * (half - 1.1))
                if lane.is_empty or lane.geom_type != "LineString":
                    continue
                s = 4.0
                while s < lane.length - 4:
                    p = lane.interpolate(s)
                    roll = stable("car", r["name"], r["line"].coords[0], side, s)
                    if roll < 0.5 and carriage.contains(p) and not mouth.contains(p) and not zebra_zone.contains(p):
                        key = "black_cab" if roll < 0.1 else ("van" if roll < 0.16 else f"car_{int(roll * 100) % 4}")
                        put(key, p.x, p.y, yaw_of(r["line"], p) + (0 if side == 1 else 180), z=0.03)
                    s += 6.5
    for r in lay["regions"]:
        if r["surface"] != "park":
            continue
        x0, y0, x1, y1 = r["rect"]
        for gx in np.arange(x0 + 7, x1, 14.0):
            for gy in np.arange(y0 + 7, y1, 14.0):
                x, y = gx + 5 * (stable("pk", gx, gy) - 0.5), gy + 5 * (stable("pk2", gx, gy) - 0.5)
                if stable("park", r["id"], gx, gy) < 0.3 and green.contains(Point(x, y)):
                    put("tree_plane", x, y, 360 * stable("pt", gx, gy), scale=0.8 + 0.5 * stable("ps", gx, gy))
    for t in city["trees"]:
        p = Point(t)
        if walk.contains(p) and city["playable"].contains(p):
            put("tree_plane", t[0], t[1], 360 * stable("tree", t), scale=0.8 + 0.4 * stable("ts", t))
    road_tree = shapely.STRtree([r["line"] for r in roads])
    kerb_roads = [r for r in roads if r["width"] > 0]
    kerb_tree = shapely.STRtree([r["line"] for r in kerb_roads])
    phone_boxes = []
    inset = paving.buffer(-0.6)
    inset_edge = inset.boundary
    shapely.prepare(inset_edge)
    for pt in city["points"]:
        key = FURNITURE.get(pt["kind"])
        p = Point(pt["x"], pt["y"])
        if pt["kind"] == "traffic_signals":
            r = roads[int(road_tree.nearest(p))]
            yaw = yaw_of(r["line"], p)
            for side in (1, -1):
                q = r["line"].offset_curve(side * (r["width"] / 2 + 0.5))
                if not q.is_empty:
                    k = nearest_points(q, p)[0]
                    if paving.contains(k):
                        put("traffic_light", k.x, k.y, yaw + (0 if side == 1 else 180))
            continue
        if key is None:
            continue
        kerb = kerbside(key, p, kerb_roads, kerb_tree, paving, phone_boxes, inset_edge)
        if kerb:
            p, yaw = kerb
            put(key, p.x, p.y, yaw)
            r = kerb_roads[int(kerb_tree.nearest(p))]
        else:
            if not paving.contains(p):
                if carriage.contains(p) or not walk.contains(p):
                    k = nearest_points(inset, p)[0]
                    if k.distance(p) > 15:
                        continue
                    p = k
            yaw = 0.0
            r = roads[int(road_tree.nearest(p))]
            if r["line"].distance(p) < 25:
                yaw = yaw_of(r["line"], p)
            put(key, p.x, p.y, yaw)
        if key == "bus_stop" and r["class"] in ("primary", "secondary", "trunk") and stable("bus", pt["x"]) < 0.5:
            lane = r["line"].offset_curve(r["width"] / 2 - 1.6)
            lane2 = r["line"].offset_curve(-(r["width"] / 2 - 1.6))
            best = min((ln for ln in (lane, lane2) if not ln.is_empty), key=lambda ln: ln.distance(p), default=None)
            if best is not None:
                q = nearest_points(best, p)[0]
                if carriage.contains(q):
                    put("bus", q.x, q.y, yaw_of(r["line"], q) + (0 if best is lane else 180), z=0.03)
    for z in city["zebras"]:
        if not z["beacon"]:
            continue
        a = math.radians(z["yaw"])
        cx, cy = -math.sin(a), math.cos(a)
        for side in (1, -1):
            off = z["width"] / 2 + 0.6
            x, y = z["x"] + cx * off * side, z["y"] + cy * off * side
            if paving.contains(Point(x, y)):
                put("belisha_beacon", x, y, z["yaw"])
    # The playable edge: a "ROAD CLOSED" barrier where a road leaves, an invisible wall all round.
    edge = city["playable"].exterior
    for r in city["back_roads"]:
        hit = r["line"].intersection(edge)
        for g in getattr(hit, "geoms", [hit]):
            if g.geom_type == "Point":
                # A line of 1.85 m roadworks rails (long along their local Y) right across carriageway and pavements.
                a = math.radians(yaw_of(r["line"], g))
                nx, ny = -math.sin(a), math.cos(a)
                span = r["width"] + 2 * PAVEMENT_M
                count = max(2, math.ceil(span / 1.85))
                for k in range(count):
                    o = -span / 2 + (k + 0.5) * span / count
                    put("road_barrier", g.x + nx * o, g.y + ny * o, math.degrees(a), z=0.03)
    items = dedupe(items)
    city["placements_items"] = items
    markers = make_markers(city)
    return {"markers": markers, "items": items,
            "edge_wall": [[round(x, 2), round(y, 2)] for x, y in edge.coords]}


# Street furniture that stands at the kerb, and how far back from the carriageway edge (m). OSM points for these sit
# anywhere on the pavement (or in the road); real ones line the kerb.
KERBSIDE = {"bin": 0.55, "post_box": 0.7, "phone_box": 1.2, "bus_stop": 1.6}


def kerbside(key, p, roads, tree, paving, phone_boxes, edge=None):
    """(point, yaw) at the kerb of the nearest road on the side the OSM point is on, or None (no road within 20 m:
    a square or plaza, where the OSM point stands). Phone boxes face their door (+Y of the K6 model) away from the road,
    and a second box near a first lines up beside it."""
    if key not in KERBSIDE:
        return None
    r = roads[int(tree.nearest(p))]
    if r["line"].distance(p) > 20:
        # A square or plaza: stand against its nearest edge (a wall, railing or lawn border), 0.6 m off it, not in
        # the open where OSM put the point. edge = boundary of the paving inset by 0.6 m.
        if edge is None or key not in ("bin", "post_box"):
            return None
        k = nearest_points(edge, p)[0]
        if k.distance(p) > 25:
            return None
        e = nearest_points(paving.boundary, k)[0]
        return k, math.degrees(math.atan2(k.y - e.y, k.x - e.x))
    q = r["line"].interpolate(r["line"].project(p))
    a = math.radians(yaw_of(r["line"], q))
    dx, dy = math.cos(a), math.sin(a)
    side = 1 if dx * (p.y - q.y) - dy * (p.x - q.x) > 0 else -1
    nx, ny = -dy * side, dx * side  # unit normal from the road toward the point's side
    off = r["width"] / 2 + KERBSIDE[key]
    k = Point(q.x + nx * off, q.y + ny * off)
    yaw = math.degrees(a)
    if key == "bus_stop":  # the shelter's length (local Y) runs along the kerb, its open side (+X) to the road
        yaw = math.degrees(math.atan2(-ny, -nx))
    if key == "phone_box":
        yaw = math.degrees(math.atan2(ny, nx)) - 90.0  # door (+Y) faces the pavement
        mate = next((b for b in phone_boxes if b.distance(k) < 2.2), None)
        if mate is not None:  # K6 pairs stand side by side along the kerb
            k = Point(mate.x + dx * 1.15, mate.y + dy * 1.15)
        phone_boxes.append(k)
    if not paving.contains(k):
        return None
    return k, yaw


def dedupe(items, radius=1.2):
    """Drops furniture standing on another (OSM duplicates, lamps on top of OSM lamps)."""
    kept, grid = [], {}
    for it in items:
        cell = (int(it["x"] // radius), int(it["y"] // radius))
        clash = any(grid.get((cell[0] + i, cell[1] + j)) for i in (-1, 0, 1) for j in (-1, 0, 1))
        if clash and not it["key"].startswith(("car_", "black_cab", "van", "bus", "phone_box", "road_barrier")):
            continue
        grid[cell] = True
        kept.append(it)
    return kept


def random_walk_points(paving, centre, radius, count, seed):
    out, i = [], 0
    while len(out) < count and i < 400:
        a, r = 2 * math.pi * stable(seed, i, "a"), radius * math.sqrt(stable(seed, i, "r"))
        p = Point(centre[0] + r * math.cos(a), centre[1] + r * math.sin(a))
        if paving.contains(p):
            out.append([round(p.x, 2), round(p.y, 2), 0.15])
        i += 1
    return out


PEDESTRIANS = 30  # City Sample crowd Londoners (each is a MetaHuman-grade character: keep the count modest), on top of the 11 named villagers


def pavement_route(city, near, seed, length=110.0, step=10.0):
    """A walk along one pavement: the line down the middle of the pavement of the road nearest `near` (on near's side),
    sampled every `step` m over up to `length` m while it stays on paving; then back the same way (out-and-back), so the
    walker never cuts across a road or a square. None if no road is within 25 m or the run is under 30 m."""
    roads, tree = city["route_roads"], city["route_tree"]
    p = Point(near)
    r = roads[int(tree.nearest(p))]
    line = r["line"]
    if line.distance(p) > 25 or line.length < 30:
        return None
    q = line.interpolate(line.project(p))
    a = math.radians(yaw_of(line, q))
    side = 1 if math.cos(a) * (p.y - q.y) - math.sin(a) * (p.x - q.x) > 0 else -1
    walk = line.offset_curve(side * (r["width"] / 2 + PAVEMENT_M / 2))
    if walk.is_empty or walk.geom_type != "LineString":
        return None
    centre = walk.project(p)
    start = max(0.0, centre - length * (0.3 + 0.4 * stable(seed, "s")))
    runs, run = [], []
    s = start
    while s <= min(walk.length, start + length):
        pt = walk.interpolate(s)
        if city["paving"].contains(pt):
            run.append([round(pt.x, 2), round(pt.y, 2), 0.15])
        elif run:
            runs.append(run)
            run = []
        s += step
    runs.append(run)
    best = max(runs, key=len)
    if len(best) < 4:
        return None
    return best + best[-2:0:-1]


def pedestrian_routes(city, count, spacing=55.0):
    """Walking routes for the crowd: one pavement walk (pavement_route) from each of `count` kerbside lamps spread over
    the playable streets, at least spacing apart."""
    lamps = sorted((it for it in city["placements_items"] if it["key"] in ("lamp_modern", "lamp_heritage")),
                   key=lambda it: stable("ped", it["x"], it["y"]))
    starts, routes = [], []
    for lamp in lamps:
        if len(routes) >= count:
            break
        if any(math.hypot(lamp["x"] - x, lamp["y"] - y) < spacing for x, y in starts):
            continue
        pts = pavement_route(city, (lamp["x"], lamp["y"]), f"ped{lamp['x']:.1f}{lamp['y']:.1f}")
        if pts:
            starts.append((lamp["x"], lamp["y"]))
            routes.append(pts)
    return routes


def make_markers(city):
    lay, paving = city["lay"], city["paving"]
    city["route_roads"] = [r for r in city["play_roads"] + city["connector_roads"] if r["width"] > 0]
    city["route_tree"] = shapely.STRtree([r["line"] for r in city["route_roads"]])
    hub_d, hub = locate(lay["hub"]["lat"], lay["hub"]["lon"], lay)
    hub_p = nearest_points(paving.buffer(-1.0), Point(hub))[0]
    camps = []
    for c in lay["camps"]:
        did, p = locate(c["lat"], c["lon"], lay)
        camps.append({"id": c["id"], "name": c["name"], "district": did, "x": round(p[0], 2), "y": round(p[1], 2), "z": 0.15,
                      "r": c["r"], "difficulty": c["difficulty"], "waves": c["waves"]})
    bd, bp = locate(lay["boss"]["lat"], lay["boss"]["lon"], lay)
    landmarks = []
    for lm in lay["landmarks"]:
        did, p = locate(lm["lat"], lm["lon"], lay)
        landmarks.append({"id": lm["id"], "district": did, "x": round(p[0], 2), "y": round(p[1], 2)})
    villagers = []
    centres = [(hub_p.x, hub_p.y)] * 4 + [(c["x"], c["y"]) for c in camps] + \
              [tuple(cut_rect(d).centroid.coords[0]) for d in lay["districts"]] * 1
    lamps = [(it["x"], it["y"]) for it in city["placements_items"] if it["key"] in ("lamp_modern", "lamp_heritage")]
    used = []
    for i, centre in enumerate(centres):
        # The nearest kerbside lamps to the centre (squares and plazas have no pavement line of their own), skipping
        # ones within 30 m of another villager's start, so the hub's four walk different pavements.
        for lamp in sorted(lamps, key=lambda l: (l[0] - centre[0]) ** 2 + (l[1] - centre[1]) ** 2)[:40]:
            if any(math.dist(lamp, u) < 30 for u in used):
                continue
            pts = pavement_route(city, lamp, f"villager{i}")
            if pts:
                used.append(lamp)
                villagers.append(pts)
                break
    pedestrians = pedestrian_routes(city, PEDESTRIANS)
    tour = {"Hub": [hub_p.x, hub_p.y]}
    tour.update({f"Camp_{c['id']}": [c["x"], c["y"]] for c in camps})
    tour.update({f"Landmark_{lm['id']}": [lm["x"] + 40, lm["y"] + 40] for lm in landmarks})
    for name, (lat, lon) in {"Soho": (51.5128, -0.1318), "CamdenHighSt": (51.5395, -0.1427),
                             "ParliamentSq": (51.5005, -0.1270), "ShadThames": (51.5035, -0.0725)}.items():
        tour[name] = list(locate(lat, lon, lay)[1])
    for r in lay["roads"]:
        a = r["line"][len(r["line"]) // 2]
        tour[f"Road_{r['id'].replace(' ', '_').replace(chr(39), '')}"] = list(a)
    return {"hub": {"name": lay["hub"]["name"], "district": hub_d, "x": round(hub_p.x, 2), "y": round(hub_p.y, 2), "z": 0.15},
            "camps": camps,
            "boss": {"name": lay["boss"]["name"], "district": bd, "x": round(bp[0], 2), "y": round(bp[1], 2), "z": 0.15, "r": lay["boss"]["r"]},
            "villagers": villagers, "pedestrians": pedestrians, "landmarks": landmarks,
            "skyline": [{"id": s["id"], "x": s["map"][0], "y": s["map"][1]} for s in lay.get("skyline", [])],
            "tour": {k: [round(v[0], 2), round(v[1], 2)] for k, v in tour.items()}}


# ----------------------------------------------------------------------------------------------------------- export
def landscape_pixels(city):
    """Pixels per side (N * 63 + 1, 1 m each) of a landscape centred on the origin that covers the backdrop."""
    x0, y0, x1, y1 = city["backdrop"].bounds
    half = max(abs(x0), abs(x1), abs(y0), abs(y1)) + 20
    n = math.ceil((2 * half) / QUADS)
    return n * QUADS + 1


def terrain(city, out_dir):
    """height.png (16-bit, 32768 + 128/m), weight_<layer>.png (8-bit, sum 255) and terrain.json: flat street level,
    river/canal/dock beds BED_DEPTH_M below their water level, grass where the parks are."""
    from PIL import Image, ImageDraw
    px = landscape_pixels(city)
    half = (px - 1) / 2

    def tr(ring):
        return [(x + half, y + half) for x, y in ring]

    height = Image.new("F", (px, px), 0.0)
    hd = ImageDraw.Draw(height)
    bed = Image.new("L", (px, px), 0)
    bd = ImageDraw.Draw(bed)
    for g, level, _ in sorted(city["water"], key=lambda w: -w[1]):
        for p in polygons(g):
            rings = rings_of(p)
            hd.polygon(tr(rings[0]), fill=level - BED_DEPTH_M)
            bd.polygon(tr(rings[0]), fill=255)
            for hole in rings[1:]:
                hd.polygon(tr(hole), fill=0.0)
                bd.polygon(tr(hole), fill=0)
    grass = Image.new("L", (px, px), 0)
    gd = ImageDraw.Draw(grass)
    for g in (city["green"], city["green_back"]):
        for p in polygons(g):
            rings = rings_of(p)
            gd.polygon(tr(rings[0]), fill=255)
            for hole in rings[1:]:
                gd.polygon(tr(hole), fill=0)
    h = np.asarray(height, dtype=np.float64)
    Image.fromarray(np.clip(32768 + h * 128, 0, 65535).astype(np.uint16)).save(os.path.join(out_dir, "height.png"))
    b = np.asarray(bed, dtype=np.int32)
    g = np.minimum(np.asarray(grass, dtype=np.int32), 255 - b)
    weights = {"Grass": g, "Riverbed": b, "Soil": 255 - g - b}
    for name, w in weights.items():
        Image.fromarray(w.astype(np.uint8)).save(os.path.join(out_dir, f"weight_{name.lower()}.png"))
    meta = {"size_m": px - 1, "pixels": px, "height_scale_z": 100.0, "layers": LAYERS,
            "min_m": float(h.min()), "max_m": float(h.max()), "street_level_m": 0.0}
    with open(os.path.join(out_dir, "terrain.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    return meta


def chunk_of(x, y):
    return f"{int(math.floor(x / CHUNK_M))}_{int(math.floor(y / CHUNK_M))}"


def chunked(g, bounds):
    """Geometry -> {chunk key: geometry clipped to that 128 m cell}."""
    out = {}
    x0, y0, x1, y1 = bounds
    for cx in range(int(math.floor(x0 / CHUNK_M)), int(math.floor(x1 / CHUNK_M)) + 1):
        for cy in range(int(math.floor(y0 / CHUNK_M)), int(math.floor(y1 / CHUNK_M)) + 1):
            cell = box(cx * CHUNK_M, cy * CHUNK_M, (cx + 1) * CHUNK_M, (cy + 1) * CHUNK_M)
            part = g.intersection(cell)
            if not part.is_empty and polygons(part):
                out[f"{cx}_{cy}"] = part
    return out


def export(city, out_dir=OUT_DIR):
    os.makedirs(out_dir, exist_ok=True)
    bounds = city["backdrop"].bounds
    chunks = {}

    def add(kind, g, min_area=0.05):
        for key, part in chunked(g, bounds).items():
            geoms = export_geom(part, min_area)
            if geoms:
                chunks.setdefault(key, {}).setdefault(kind, []).extend(geoms)

    add("road", city["carriage"])
    add("road_backdrop", city["back_carriage"])
    add("paving", city["paving"])
    add("paving_backdrop", city["paving_back"])
    by_level = {}
    for g, level, _ in city["water"]:
        by_level.setdefault(level, []).append(g)
    water = {}
    for level, gs in by_level.items():
        water[str(level)] = unary_union(gs)
        add(f"water_{level}", water[str(level)])
    add("paint_white", city["markings"]["white"], 0.01)
    add("paint_yellow", city["markings"]["yellow"], 0.01)
    buildings = []
    for b in city["buildings"]:
        for k, p in enumerate(polygons(b["poly"])):
            if p.area < 4.0:
                continue
            c = p.representative_point()
            entry = {"id": b["id"] if k == 0 else f"{b['id']}_{k}", "zone": b["zone"], "district": b["district"],
                     "chunk": chunk_of(c.x, c.y), "rings": rings_of(p), "base": round(b["base"], 2),
                     "top": round(b["top"], 2), "style": b["style"], "roof": b["roof"], "colour": b["colour"],
                     "name": b["name"], "seed": round(stable("seed", b["id"], k), 5)}
            if b["zone"] == "play":
                flags = city["street_detail"].get(b["id"], [])
                entry["street"] = flags[k] if k < len(flags) else [True] * len(entry["rings"][0])
                entry["shop"] = shopfront(b)
                attic = attic_ring(b, p)
                if attic:
                    entry["attic"] = attic
            buildings.append(entry)
    # Water edges: embankment walls everywhere but under bridge decks.
    bridge_zone = unary_union([LineString(br["line"]).buffer(br["width"] / 2 + 1) for br in city["bridges"]]) if city["bridges"] else Polygon()
    walls = []
    for level, g in water.items():
        for p in polygons(g):
            for ring in [p.exterior, *p.interiors]:
                for seg in lines(ring.difference(bridge_zone)) if not bridge_zone.is_empty else [LineString(ring.coords)]:
                    walls.append({"level": float(level), "line": [[round(x, 2), round(y, 2)] for x, y in seg.coords]})
    city_json = {"meta": {"chunk_m": CHUNK_M, "pavement_kerb_m": 0.15, "road_top_m": 0.03, "styles": STYLES,
                          "axes": "metres, +X east, +Y south (UE axes)"},
                 "buildings": buildings, "chunks": chunks, "bridges": city["bridges"], "water_walls": walls}
    with open(os.path.join(out_dir, "city.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(city_json, f, separators=(",", ":"))
    with open(os.path.join(out_dir, "placements.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(city["placements"], f, indent=1)
    meta = terrain(city, out_dir)
    return city_json, meta


SHOP_ODDS = {"WE": 0.8, "CAM": 0.85, "connector": 0.55, "WST": 0.25, "TWR": 0.35}


def shopfront(b):
    """Ground-floor shops: most of the West End and Camden, some of Westminster and the City side."""
    if b["base"] > 0 or b["style"] in ("gothic", "modern_glass", "concrete_modern") or b["top"] - b["base"] < 7:
        return False
    return stable("shop", b["id"]) < SHOP_ODDS.get(b["district"], 0.3)


def attic_ring(b, p):
    """A slate mansard storey set back from the walls on about half the terraces (London's roofline)."""
    if b["style"] not in ("georgian_terrace", "victorian_terrace", "stucco", "portland_stone") or b["roof"] != "flat":
        return None
    if b["top"] - b["base"] < 9 or stable("attic", b["id"]) > 0.5:
        return None
    inset = p.buffer(-1.6, join_style=2)
    if inset.geom_type != "Polygon" or inset.area < 25:
        return None
    return rings_of(inset.simplify(0.15))


STYLE_COLOUR = {"georgian_terrace": (196, 160, 110), "victorian_terrace": (170, 85, 60), "stucco": (238, 232, 215),
                "portland_stone": (222, 210, 180), "gothic": (200, 180, 120), "warehouse": (130, 85, 55),
                "modern_glass": (110, 150, 180), "concrete_modern": (160, 160, 155)}


def plan(city, path, scale=1.0):
    """Top-down review image: ground, buildings by style, markings, furniture, markers, cut-outs and connectors."""
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = city["backdrop"].bounds
    w, h = int((x1 - x0) * scale), int((y1 - y0) * scale)
    img = Image.new("RGB", (w, h), (40, 40, 44))
    dr = ImageDraw.Draw(img)

    def tr(ring):
        return [((x - x0) * scale, (y - y0) * scale) for x, y in ring]

    def fill(g, colour, outline=None):
        mask = Image.new("L", img.size, 0)
        md = ImageDraw.Draw(mask)
        for p in polygons(g):
            rings = rings_of(p)
            md.polygon(tr(rings[0]), fill=255)
            for hole in rings[1:]:
                md.polygon(tr(hole), fill=0)
        img.paste(colour, (0, 0), mask)
        if outline:
            for p in polygons(g):
                dr.polygon(tr(rings_of(p)[0]), outline=outline)

    fill(city["paving_back"], (120, 118, 112))
    fill(city["back_carriage"], (85, 85, 88))
    fill(city["green_back"], (95, 130, 85))
    fill(city["paving"], (200, 196, 186))
    fill(city["carriage"], (95, 95, 100))
    fill(city["green"], (140, 190, 120))
    fill(city["water_all"], (80, 130, 185))
    fill(city["markings"]["white"], (250, 250, 250))
    fill(city["markings"]["yellow"], (240, 200, 40))
    for b in city["buildings"]:
        col = STYLE_COLOUR[b["style"]]
        if b["zone"] == "backdrop":
            col = tuple(int(c * 0.55) for c in col)
        for p in polygons(b["poly"]):
            dr.polygon(tr(rings_of(p)[0]), fill=col, outline=(30, 30, 30))
    for br in city["bridges"]:
        dr.line(tr(br["line"]), fill=(150, 150, 150), width=max(1, int(br["width"] * scale)))
    colours = {"tree_plane": (40, 110, 40), "lamp_modern": (20, 20, 20), "lamp_heritage": (20, 20, 20),
               "road_barrier": (255, 80, 0), "bus": (210, 20, 20), "black_cab": (0, 0, 0), "phone_box": (230, 20, 20),
               "post_box": (230, 20, 20), "belisha_beacon": (255, 160, 0)}
    for it in city["placements"]["items"]:
        c = colours.get(it["key"], (60, 60, 160))
        x, y = (it["x"] - x0) * scale, (it["y"] - y0) * scale
        r = 2.5 if it["key"] == "tree_plane" else 1.2
        dr.ellipse([x - r, y - r, x + r, y + r], fill=c)
    for g in list(city["rects"].values()):
        dr.rectangle(tr([g.bounds[:2], g.bounds[2:]]), outline=(255, 40, 40), width=3)
    for g in list(city["regions"].values()):
        dr.rectangle(tr([g.bounds[:2], g.bounds[2:]]), outline=(255, 200, 0), width=2)
    m = city["placements"]["markers"]

    def mark(x, y, r, colour, label):
        cx, cy = (x - x0) * scale, (y - y0) * scale
        dr.ellipse([cx - r * scale, cy - r * scale, cx + r * scale, cy + r * scale], outline=colour, width=4)
        dr.text((cx + r * scale + 4, cy - 6), label, fill=colour)

    mark(m["hub"]["x"], m["hub"]["y"], 12, (0, 255, 255), "HUB " + m["hub"]["name"])
    for c in m["camps"]:
        mark(c["x"], c["y"], c["r"], (255, 60, 60), f"CAMP {c['name']} ({c['difficulty']})")
    mark(m["boss"]["x"], m["boss"]["y"], m["boss"]["r"], (200, 0, 255), "BOSS " + m["boss"]["name"])
    for lm in m["landmarks"] + m["skyline"]:
        mark(lm["x"], lm["y"], 6, (255, 255, 255), lm["id"])
    for route in m["villagers"]:
        dr.line([((p[0] - x0) * scale, (p[1] - y0) * scale) for p in route], fill=(0, 255, 120), width=2)
    for d in city["lay"]["districts"]:
        r = city["rects"][d["id"]]
        dr.text(((r.bounds[0] - x0) * scale + 6, (r.bounds[1] - y0) * scale + 6), d["name"], fill=(255, 80, 80))
    img.save(path)


def summary(city):
    play = [b for b in city["buildings"] if b["zone"] == "play"]
    styles = Counter(b["style"] for b in play)
    keys = Counter(it["key"] for it in city["placements"]["items"])
    return (f"{len(play)} playable buildings ({dict(styles)}), {len(city['buildings']) - len(play)} backdrop; "
            f"playable {city['playable'].area / 1e6:.2f} km2; road {city['carriage'].area / 1e3:.0f}k m2; "
            f"{len(city['bridges'])} bridge decks; {len(city['zebras'])} crossings; furniture {dict(keys)}")


def main():
    lay = load_layout()
    city = build(lay)
    export(city)
    print("[make_city]", summary(city))
    if "--plan" in sys.argv:
        plan(city, os.path.join(OUT_DIR, "plan.png"))
        print("[make_city] plan.png written")


if __name__ == "__main__":
    main()
