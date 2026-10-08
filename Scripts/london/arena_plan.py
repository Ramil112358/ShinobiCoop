"""Top-down review image of the four PvP arenas (arena_layout.json) over the London city footprints.

    python Scripts/london/arena_plan.py [--art <dir with city.json>] [--out arena_plan.png]

--art defaults to $SHINOBI_LONDON_ART, else the repo's SourceArt/World/London (untracked OSM output of make_city.py,
read only). --out defaults to arena_plan.png inside --art. Nothing is written next to the sources.
arena_layout.json is in raw map metres (origin_offset_m not applied); city.json is in offset-applied metres, so the
drawing shifts layout points by origin_offset_m."""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_city as mc  # noqa: E402

LAYOUT = os.path.join(HERE, "arena_layout.json")
TEAM_COL = {"A": (40, 110, 255), "B": (235, 40, 40)}
PICK_COL = {"Health": (40, 205, 70), "Attack": (255, 150, 0), "Speed": (0, 215, 225), "Shield": (190, 70, 230)}
PICK_TAG = {"Health": "HP", "Attack": "ATK", "Speed": "SPD", "Shield": "SHD"}
BASE_STYLE = {"georgian_terrace": (196, 160, 110), "victorian_terrace": (170, 85, 60), "stucco": (238, 232, 215),
              "portland_stone": (168, 176, 186), "gothic": (140, 130, 120), "warehouse": (150, 110, 90),
              "modern_glass": (110, 150, 175), "concrete_modern": (150, 150, 150)}


def art_dir(arg=None):
    if arg:
        return arg
    return os.environ.get("SHINOBI_LONDON_ART") or mc.OUT_DIR


def origin_offset():
    with open(mc.LAYOUT_PATH, encoding="utf-8") as f:
        return json.load(f)["origin_offset_m"]


def load_arenas(path=LAYOUT):
    with open(path, encoding="utf-8") as f:
        return json.load(f)["arenas"]


def _polys(geoms):
    from shapely.geometry import Polygon
    return [Polygon(r[0], r[1:]).buffer(0) for r in geoms]


class Ground:
    """City geometry for the arena rects, in RAW map metres (origin_offset_m removed)."""

    def __init__(self, art, rects, pad=60.0):
        from shapely import affinity
        from shapely.geometry import LineString, Polygon, box
        from shapely.ops import unary_union
        from shapely.strtree import STRtree
        self.art = art
        off = origin_offset()
        self.off = off
        with open(os.path.join(art, "city.json"), encoding="utf-8") as f:
            city = json.load(f)
        windows = [box(r[0] + off[0] - pad, r[1] + off[1] - pad, r[2] + off[0] + pad, r[3] + off[1] + pad) for r in rects]
        win = unary_union(windows)

        def shift(g):
            return affinity.translate(g, -off[0], -off[1])

        def gather(prefix):
            polys = []
            for key, chunk in city["chunks"].items():
                cx, cy = (int(v) for v in key.split("_"))
                if not win.intersects(box(cx * 128, cy * 128, cx * 128 + 128, cy * 128 + 128)):
                    continue
                for kind, geoms in chunk.items():
                    if kind == prefix or (prefix.endswith("_") and kind.startswith(prefix)):
                        polys += _polys(geoms)
            return shift(unary_union(polys).buffer(0.02)).intersection(shift(win)) if polys else Polygon()

        self.road = gather("road")
        self.paving = gather("paving")
        self.road_back = gather("road_backdrop")
        self.paving_back = gather("paving_backdrop")
        self.water = gather("water_")
        self.bridges = []
        for br in city["bridges"]:
            line = LineString([(x - off[0], y - off[1]) for x, y in br["line"]])
            self.bridges.append((br["name"], line, br["width"]))
        deck = unary_union([ln.buffer(w / 2, cap_style=2) for _, ln, w in self.bridges]) if self.bridges else Polygon()
        self.open_ground = unary_union([self.road, self.paving, deck])
        self.buildings = []
        for b in city["buildings"]:
            poly = Polygon(b["rings"][0], b["rings"][1:]).buffer(0)
            if win.intersects(poly):
                self.buildings.append((b["id"], b.get("name"), b["style"], b["zone"], shift(poly)))
        self._tree = STRtree([b[4] for b in self.buildings])
        with open(os.path.join(art, "placements.json"), encoding="utf-8") as f:
            pl = json.load(f)
        m = pl["markers"]
        marks = [(lm["id"], lm["x"], lm["y"]) for lm in m["landmarks"]]
        marks += [(c["name"], c["x"], c["y"]) for c in m["camps"]]
        marks += [(m["hub"]["name"], m["hub"]["x"], m["hub"]["y"]), (m["boss"]["name"], m["boss"]["x"], m["boss"]["y"])]
        self.landmarks = [(n, x - off[0], y - off[1]) for n, x, y in marks]

    def walkable(self, rect):
        """Everything inside the rect that is not a building footprint or water (streets, pavement, lawns), eroded
        0.7 m (a player is ~1.4 m wide): a list of connected pieces as shapely polygons. Starts and pickups of one
        arena must all sit in the same piece, so nobody starts sealed in a courtyard or across a canal."""
        from shapely.geometry import box
        from shapely.ops import unary_union
        window = box(*rect)
        blocks = unary_union([b[4] for b in self.buildings if b[4].intersects(window)] + [self.water]).buffer(0.3)
        return mc.polygons(window.difference(blocks).buffer(-0.7))

    def buildings_near(self, x, y, r):
        """Ids of buildings whose footprint is within r metres of the raw point."""
        from shapely.geometry import Point
        pt = Point(x, y)
        return [self.buildings[i][0] for i in self._tree.query(pt.buffer(r)) if self.buildings[i][4].distance(pt) <= r]


def load_ground(art, rects=None, pad=60.0):
    if rects is None:
        rects = [a["rect"] for a in load_arenas()]
    return Ground(art, rects, pad)


# ----------------------------------------------------------------------------------------------------------- drawing
def font(size, bold=False):
    from PIL import ImageFont
    for name in (("arialbd.ttf" if bold else "arial.ttf"), "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def text(dr, xy, s, f, fill, anchor="la", halo=True):
    if halo:
        x, y = xy
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            dr.text((x + dx, y + dy), s, font=f, fill=(15, 15, 18), anchor=anchor)
    dr.text(xy, s, font=f, fill=fill, anchor=anchor)


def draw_map(g, view, scale, arenas=None, detail=True, only=None):
    """Render the raw-metre window view=[x0,y0,x1,y1] at `scale` px/m. detail adds buildings' edge, grid, marks."""
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = view
    w, h = int((x1 - x0) * scale), int((y1 - y0) * scale)
    img = Image.new("RGB", (w, h), (92, 112, 84))  # unpaved ground reads as grass
    dr = ImageDraw.Draw(img)

    def tr(ring):
        return [((x - x0) * scale, (y - y0) * scale) for x, y in ring]

    def fill(geom, colour):
        if geom.is_empty:
            return
        mask = Image.new("L", img.size, 0)
        md = ImageDraw.Draw(mask)
        for p in mc.polygons(geom):
            md.polygon(tr(list(p.exterior.coords)), fill=255)
            for hole in p.interiors:
                md.polygon(tr(list(hole.coords)), fill=0)
        img.paste(colour, (0, 0), mask)

    fill(g.paving_back, (112, 110, 105))
    fill(g.road_back, (78, 78, 82))
    fill(g.paving, (226, 222, 208))
    fill(g.road, (88, 88, 94))
    fill(g.water, (78, 128, 186))
    for _, line, width in g.bridges:
        dr.line(tr(list(line.coords)), fill=(150, 150, 150), width=max(1, int(width * scale)))
    for _, _, style, zone, poly in g.buildings:
        col = BASE_STYLE[style]
        if zone == "backdrop":
            col = tuple(int(c * 0.6) for c in col)
        for p in mc.polygons(poly):
            dr.polygon(tr(list(p.exterior.coords)), fill=col, outline=(35, 30, 30) if detail else None)
    if detail:
        f = font(13)
        step = 50
        for gx in range(int(math.ceil(x0 / step)) * step, int(x1) + 1, step):
            dr.line([((gx - x0) * scale, 0), ((gx - x0) * scale, 10)], fill=(255, 255, 255), width=2)
            text(dr, ((gx - x0) * scale + 3, 10), str(gx), f, (255, 255, 255))
        for gy in range(int(math.ceil(y0 / step)) * step, int(y1) + 1, step):
            dr.line([(0, (gy - y0) * scale), (10, (gy - y0) * scale)], fill=(255, 255, 255), width=2)
            text(dr, (12, (gy - y0) * scale - 6), str(gy), f, (255, 255, 255))
        for name, x, y in g.landmarks:
            if x0 < x < x1 and y0 < y < y1:
                cx, cy = (x - x0) * scale, (y - y0) * scale
                dr.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], outline=(255, 255, 255), width=2)
                text(dr, (cx + 7, cy - 7), name.replace("_", " "), font(14), (255, 255, 255))
    for a in arenas or []:
        r = a["rect"]
        hot = only is None or only == a["id"]
        col = (255, 235, 60) if hot else (255, 255, 255)
        dr.rectangle(tr([r[:2], r[2:]]), outline=col, width=4 if detail else 2)
        if detail and hot:
            m = 20
            xs = [(r[0] + m, r[1] + m), (r[2] - m, r[1] + m), (r[2] - m, r[3] - m), (r[0] + m, r[3] - m), (r[0] + m, r[1] + m)]
            pts = tr(xs)
            for p, q in zip(pts, pts[1:]):  # dashed 20 m start margin
                n = int(math.hypot(q[0] - p[0], q[1] - p[1]) // 14)
                for k in range(0, n, 2):
                    dr.line([(p[0] + (q[0] - p[0]) * k / n, p[1] + (q[1] - p[1]) * k / n),
                             (p[0] + (q[0] - p[0]) * (k + 1) / n, p[1] + (q[1] - p[1]) * (k + 1) / n)], fill=col, width=1)
        if detail and not hot:
            continue
        if not detail:
            text(dr, ((r[0] - x0) * scale, (r[1] - y0) * scale - 16), a["id"], font(15, True), (255, 235, 60), halo=True)
            continue
        cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
        px, py = (cx - x0) * scale, (cy - y0) * scale
        dr.line([(px - 8, py), (px + 8, py)], fill=(255, 235, 60), width=2)
        dr.line([(px, py - 8), (px, py + 8)], fill=(255, 235, 60), width=2)
        for team in "AB":
            for i, (sx, sy, yaw) in enumerate(a["starts"][team]):
                x, y = (sx - x0) * scale, (sy - y0) * scale
                ex, ey = x + math.cos(math.radians(yaw)) * 24, y + math.sin(math.radians(yaw)) * 24
                dr.line([(x, y), (ex, ey)], fill=(255, 255, 255), width=6)
                dr.line([(x, y), (ex, ey)], fill=TEAM_COL[team], width=3)
                dr.ellipse([x - 9, y - 9, x + 9, y + 9], fill=TEAM_COL[team], outline=(255, 255, 255), width=2)
                text(dr, (x, y), str(i + 1), font(12, True), (255, 255, 255), anchor="mm", halo=False)
        for p in a["pickups"]:
            x, y = (p["x"] - x0) * scale, (p["y"] - y0) * scale
            dr.ellipse([x - 10, y - 10, x + 10, y + 10], fill=PICK_COL[p["type"]], outline=(0, 0, 0), width=3)
            text(dr, (x + 13, y - 8), PICK_TAG[p["type"]], font(15, True), PICK_COL[p["type"]])
    if detail:  # scale bar, 50 m
        bx, by = 18, h - 22
        dr.rectangle([bx - 6, by - 22, bx + 50 * scale + 6, by + 14], fill=(15, 15, 18))
        dr.line([(bx, by), (bx + 50 * scale, by)], fill=(255, 255, 255), width=4)
        dr.line([(bx, by - 6), (bx, by + 6)], fill=(255, 255, 255), width=3)
        dr.line([(bx + 50 * scale, by - 6), (bx + 50 * scale, by + 6)], fill=(255, 255, 255), width=3)
        text(dr, (bx, by - 20), "50 m", font(13, True), (255, 255, 255), halo=False)
    return img


def compose(arenas, g, panel_px=1000):
    from PIL import Image, ImageDraw
    pad = 45.0
    panels = []
    for a in arenas:
        r = a["rect"]
        view = [r[0] - pad, r[1] - pad, r[2] + pad, r[3] + pad]
        scale = panel_px / max(view[2] - view[0], view[3] - view[1])
        panels.append(draw_map(g, view, scale, arenas, True, a["id"]))
    pw = max(p.width for p in panels)
    ph = max(p.height for p in panels)
    gap, head = 20, 150
    # overview: the whole collage at a thumbnail scale, with the four rects
    xs = [a["rect"][0] for a in arenas] + [a["rect"][2] for a in arenas]
    ys = [a["rect"][1] for a in arenas] + [a["rect"][3] for a in arenas]
    W = pw * 2 + gap * 3
    H = head + (ph + 60) * 2 + gap * 3
    img = Image.new("RGB", (W, H), (24, 24, 28))
    dr = ImageDraw.Draw(img)
    dr.text((gap, 14), "PvP arena plan: four ~300 m rects cut from Lvl_London", font=font(34, True), fill=(255, 255, 255))
    dr.text((gap, 58), "Map metres, +X east, +Y south, north is up (origin_offset_m not applied). Yellow frame = arena rect, "
            "dashed = 20 m start margin. Rects sit inside their district cut-outs.", font=font(18), fill=(200, 200, 205))
    x = gap
    legend = [("Team A start (yaw tick faces centre)", TEAM_COL["A"]), ("Team B start", TEAM_COL["B"])] + \
             [(f"{PICK_TAG[k]} {k} pickup", PICK_COL[k]) for k in PICK_COL]
    lx, ly = gap, 92
    for label, col in legend:
        dr.ellipse([lx, ly + 3, lx + 18, ly + 21], fill=col, outline=(255, 255, 255), width=2)
        dr.text((lx + 26, ly), label, font=font(18), fill=(235, 235, 240))
        lx += 36 + int(dr.textlength(label, font=font(18))) + 14
    lx, ly = gap, 120
    for label, col in [("building (colour = style)", (196, 160, 110)), ("pavement / plaza", (226, 222, 208)), ("road", (88, 88, 94)),
                       ("water", (78, 128, 186)), ("unpaved / park", (92, 112, 84)), ("grey strip = outside the district cut-out", (112, 112, 108))]:
        dr.rectangle([lx, ly + 3, lx + 18, ly + 21], fill=col, outline=(255, 255, 255))
        dr.text((lx + 26, ly), label, font=font(18), fill=(235, 235, 240))
        lx += 36 + int(dr.textlength(label, font=font(18))) + 14
    for i, (a, p) in enumerate(zip(arenas, panels)):
        px = gap + (i % 2) * (pw + gap)
        py = head + (i // 2) * (ph + 60 + gap)
        r = a["rect"]
        pa, pb = a["starts"]["A"], a["starts"]["B"]
        mean = sum(math.hypot(p[0] - q[0], p[1] - q[1]) for p in pa for q in pb) / 16
        dr.text((px, py + 4), f"{a['id']}   rect {r[0]:.0f},{r[1]:.0f} to {r[2]:.0f},{r[3]:.0f}  ({r[2] - r[0]:.0f} x {r[3] - r[1]:.0f} m)   A-B {mean:.0f} m apart",
                font=font(24, True), fill=(255, 235, 60))
        img.paste(p, (px, py + 40))
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--art", default=None)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    art = art_dir(args.art)
    if not os.path.exists(os.path.join(art, "city.json")):
        sys.exit(f"no city.json in {art}: set --art or SHINOBI_LONDON_ART (read-only OSM output of make_city.py)")
    arenas = load_arenas()
    g = load_ground(art, [a["rect"] for a in arenas])
    img = compose(arenas, g)
    out = args.out or os.path.join(art, "arena_plan.png")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    img.save(out)
    print(f"[arena_plan] {out} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
