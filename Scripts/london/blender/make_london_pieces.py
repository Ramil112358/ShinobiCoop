"""Blender (5.2) headless: London landmarks built procedurally (no clean free model exists; real-world dimensions).

  blender -b -P Scripts/london/blender/make_london_pieces.py -- <out_dir> [key ...]

One <key>.glb each, metres, pivot at ground level under the landmark's centre, palette material names like the city
chunks (Scripts/london/import_london.py remaps them). Bridges run along +X with the origin at the deck centre, at
street level (z 0); the deck itself comes from the city chunks. Keys:
  london_eye          120 m wheel (twin-rim lattice, 32 spokes, 32 capsules), hub 75 m up, A-frame legs toward -Y
  tower_bridge        two gothic towers 61 m apart, upper walkways, suspension chains to the banks, river piers
  westminster_bridge  seven green cast-iron arches on granite piers under a 250 m deck
  shard, gherkin, walkie_talkie, bt_tower   skyline towers
  piccadilly_lights   a curved LED screen panel (faces +X), UV 0..1 for the screen atlas
"""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_city_meshes as cm  # noqa: E402  (Builder, materials, export)

WHITE = (1, 1, 1, 1)


def cyl_between(B, a, b, r, mat, sides=6, col=WHITE):
    """A thin tube from point a to b (struts, spokes, cables)."""
    a, b = Vector(a), Vector(b)
    axis = (b - a)
    length = axis.length
    if length < 1e-4:
        return
    axis.normalize()
    helper = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    u = axis.cross(helper).normalized()
    v = axis.cross(u)
    ring = [u * math.cos(2 * math.pi * i / sides) * r + v * math.sin(2 * math.pi * i / sides) * r for i in range(sides)]
    for i in range(sides):
        p0, p1 = ring[i], ring[(i + 1) % sides]
        quad = [a + p0, a + p1, b + p1, b + p0]
        B.face(quad, mat, [(i * 0.2, 0), ((i + 1) * 0.2, 0), ((i + 1) * 0.2, length), (i * 0.2, length)], col,
               normal=(p0 + p1) / 2)


def box(B, centre, size, mat, col=WHITE, rot_z=0.0):
    """Axis box (optionally turned about Z) with world-scale UVs."""
    c = Vector(centre)
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    cs, sn = math.cos(rot_z), math.sin(rot_z)
    R = lambda x, y, z: c + Vector((x * cs - y * sn, x * sn + y * cs, z))  # noqa: E731
    corners = {(sx, sy, sz): R(sx * hx, sy * hy, sz * hz) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)}
    faces = [((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)), ((-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)),
             ((1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)), ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),
             ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), ((-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1))]
    for f in faces:
        pts = [corners[k] for k in f]
        n = Vector((sum(k[0] for k in f), sum(k[1] for k in f), sum(k[2] for k in f))) / 4
        nw = Vector((n.x * cs - n.y * sn, n.x * sn + n.y * cs, n.z))
        e1, e2 = pts[1] - pts[0], pts[3] - pts[0]
        B.face(pts, mat, [(0, 0), (e1.length, 0), (e1.length, e2.length), (0, e2.length)], col, normal=nw)


def lathe(B, profile, segments, mat, col=WHITE, cap=True, colour_fn=None, centre=(0.0, 0.0), turn=0.0):
    """Surface of revolution about a vertical axis at centre: profile [(radius, z), ...] bottom to top."""
    cx, cy = centre
    rings = [[(cx + r * math.cos(turn + 2 * math.pi * i / segments), cy + r * math.sin(turn + 2 * math.pi * i / segments), z)
              for i in range(segments)] for r, z in profile]
    for j in range(len(rings) - 1):
        for i in range(segments):
            a, b = rings[j][i], rings[j][(i + 1) % segments]
            c, d = rings[j + 1][(i + 1) % segments], rings[j + 1][i]
            mid = Vector(((a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy, 0))
            fcol = colour_fn(i, j) if colour_fn else col
            s0 = 2 * math.pi * profile[j][0] * i / segments
            s1 = 2 * math.pi * profile[j][0] * (i + 1) / segments
            B.face([Vector(a), Vector(b), Vector(c), Vector(d)], mat, [(s0, a[2]), (s1, b[2]), (s1, c[2]), (s0, d[2])], fcol, normal=mid)
    if cap and profile[-1][0] > 0.01:
        top = rings[-1]
        B.face([Vector(p) for p in top], mat, [(p[0], p[1]) for p in top], col, normal=(0, 0, 1))


# ------------------------------------------------------------------------------------------------------- London Eye
def london_eye(B):
    R, hub_z, half_w = 60.0, 75.0, 2.2
    n = 64
    rim = []
    for side in (-1, 1):
        ring = [Vector((R * math.cos(2 * math.pi * i / n), side * half_w, hub_z + R * math.sin(2 * math.pi * i / n))) for i in range(n)]
        inner = [Vector((p.x * 0.94, p.y, hub_z + (p.z - hub_z) * 0.94)) for p in ring]
        for i in range(n):
            cyl_between(B, ring[i], ring[(i + 1) % n], 0.55, "Steel_White")
            cyl_between(B, inner[i], inner[(i + 1) % n], 0.4, "Steel_White")
            cyl_between(B, ring[i], inner[(i + 1) % n], 0.18, "Steel_White", 4)
            cyl_between(B, inner[i], ring[(i + 1) % n], 0.18, "Steel_White", 4)
        rim.append((ring, inner))
    for i in range(n):  # cross bracing between the two rims
        cyl_between(B, rim[0][0][i], rim[1][0][i], 0.25, "Steel_White", 4)
        if i % 2 == 0:
            cyl_between(B, rim[0][1][i], rim[1][0][i], 0.15, "Steel_White", 4)
    hub = Vector((0, 0, hub_z))
    for i in range(0, n, 2):  # cable spokes from both hub ends
        for side in (-1, 1):
            cyl_between(B, hub + Vector((0, side * 9, 0)), rim[0 if side < 0 else 1][1][i], 0.06, "Steel_White", 3)
    cyl_between(B, hub + Vector((0, -11, 0)), hub + Vector((0, 11, 0)), 1.2, "Steel_White", 12)  # spindle
    cyl_between(B, hub + Vector((0, -4, 0)), hub + Vector((0, 4, 0)), 3.5, "Steel_White", 16)  # hub
    for i in range(32):  # capsules on the outside of the rim, long axis across the wheel
        a = 2 * math.pi * (i + 0.5) / 32
        c = Vector(((R + 2.6) * math.cos(a), 0, hub_z + (R + 2.6) * math.sin(a)))
        cap = [(0.0, -4.2), (1.3, -3.6), (2.0, -2.2), (2.2, 0), (2.0, 2.2), (1.3, 3.6), (0.0, 4.2)]
        prev = None
        for r, y in cap:
            ring = [c + Vector((r * math.cos(2 * math.pi * k / 10), y, r * math.sin(2 * math.pi * k / 10))) for k in range(10)]
            if prev:
                for k in range(10):
                    q = [prev[k], prev[(k + 1) % 10], ring[(k + 1) % 10], ring[k]]
                    B.face(q, "Capsule_Glass", [(k, 0), (k + 1, 0), (k + 1, 1), (k, 1)], WHITE, normal=(q[0] + q[2]) / 2 - c)
            prev = ring
        cyl_between(B, c - Vector((0, 4.4, 0)), c + Vector((0, 4.4, 0)), 0.5, "Steel_White", 8)
    # A-frame: two raking legs from the spindle ends down to the land side (-Y), plus backstay cables.
    for side in (-1, 1):
        top = hub + Vector((0, side * 11, 0))
        foot = Vector((side * 22, -58, 0))
        cyl_between(B, top, foot, 1.4, "Steel_White", 10)
        cyl_between(B, top, Vector((side * 6, -95, 0)), 0.12, "Steel_White", 4)
    box(B, (0, -58, 0.6), (56, 10, 1.2), "Stone_Portland")


# ----------------------------------------------------------------------------------------------------- Tower Bridge
def gothic_tower(B, cx, deck_w):
    """One Tower Bridge tower centred at x = cx: two stone legs either side of the roadway up to the arch, the body
    above with gothic windows, four corner turrets with pyramid spires, a steep central roof."""
    L, W = 18.0, 26.0  # along the bridge, across it
    st = cm.STYLE["gothic"] | {"wall": "Stone_Portland", "trim": "Trim_Portland", "bay": 2.4}
    leg_w = (W - deck_w) / 2
    for side in (-1, 1):
        box(B, (cx, side * (deck_w / 2 + leg_w / 2), 4.0), (L, leg_w, 18.0), "Stone_Portland")
    body0, body1 = 9.5, 45.0
    corners = [(cx - L / 2, -W / 2), (cx + L / 2, -W / 2), (cx + L / 2, W / 2), (cx - L / 2, W / 2)]
    ring = corners  # CCW in Blender x/y
    for a, b, u, n, length in cm.edges(ring):
        frame = (Vector((a.x, a.y, 0)), u, n)
        rows, z = [], body0
        while z + 5 < body1:
            rows.append((z, z + 5, "up"))
            z += 5
        rows.append((z, body1, "plain"))
        cm.wall_with_windows(B, frame, length, rows, st, "Stone_Portland", WHITE, f"tb{cx}{a.x}{a.y}")
        B.quad_on(frame, 0, length, 4.0 + 9.0, body0, 0, "Stone_Portland", WHITE)
    B.tris([ring], body1, "Roof_Flat", WHITE)
    # steep slate roof and corner turrets
    lathe(B, [(W * 0.62, body1), (0.6, body1 + 16)], 4, "Slate", centre=(cx, 0.0), turn=math.pi / 4)
    for x, y in corners:
        cm.Builder.prism(B, (x, y), 2.4, body0, body1 + 4, 8, "Stone_Portland", WHITE)
        lathe(B, [(2.7, body1 + 4), (0.2, body1 + 13)], 8, "Slate", centre=(x, y))
    return body0, body1


def tower_bridge(B):
    deck_w = 18.0
    gap = 61.0  # bascule span between the towers' inner faces
    for side in (-1, 1):
        cx = side * (gap / 2 + 9)
        gothic_tower(B, cx, deck_w)
        # boat-shaped river pier from the bed to the deck
        box(B, (cx, 0, -4.5), (34, 30, 9), "Embankment")
    # Upper walkways at 42 m: two lattice girders (blue chords, white lattice) between the towers.
    for y in (-6.0, 6.0):
        x0, x1 = -gap / 2, gap / 2
        for z in (38.0, 44.0):
            box(B, ((x0 + x1) / 2, y, z), (gap, 3.6, 0.9), "Paint_Blue")
        for k in range(16):
            xa, xb = x0 + k * gap / 16, x0 + (k + 1) * gap / 16
            for dy in (-1.7, 1.7):
                cyl_between(B, (xa, y + dy, 38.4), (xb, y + dy, 43.6), 0.2, "Steel_White", 4)
                cyl_between(B, (xb, y + dy, 38.4), (xa, y + dy, 43.6), 0.2, "Steel_White", 4)
        box(B, (0, y, 41.0), (gap, 3.0, 4.8), "Glass")
    # Suspension chains: curved lattice girders from each tower top down to the shore abutments 82 m out.
    for side in (-1, 1):
        start = side * (gap / 2 + 18)
        for y in (-8.0, 8.0):
            pts = []
            for k in range(13):
                t = k / 12
                x = start + side * 82 * t
                z = 30.0 * (1 - t) ** 1.6 + 3.0 * t + 2.0
                pts.append(Vector((x, y, z)))
            for a, b in zip(pts, pts[1:]):
                cyl_between(B, a, b, 0.7, "Paint_Blue", 8)
                cyl_between(B, a + Vector((0, 0, -2.2)), b + Vector((0, 0, -2.2)), 0.5, "Paint_Blue", 8)
                cyl_between(B, a, b + Vector((0, 0, -2.2)), 0.2, "Steel_White", 4)
            for a in pts[1:-1]:  # hangers down to the deck edge
                cyl_between(B, a, Vector((a.x, a.y, 0.2)), 0.08, "Steel_White", 3)
        # shore abutment tower
        box(B, (start + side * 82, 0, 4.0), (10, 24, 8), "Stone_Portland")


# ---------------------------------------------------------------------------------------------- Westminster Bridge
def westminster_bridge(B):
    length, width, spans = 250.0, 26.0, 7
    pier_w = 6.0
    span = (length - (spans + 1) * pier_w) / spans
    x = -length / 2
    for i in range(spans + 1):
        box(B, (x + pier_w / 2, 0, -4.0), (pier_w, width + 2, 8.5), "Embankment")
        if i < spans:
            x0, x1 = x + pier_w, x + pier_w + span
            for side in (-1, 1):  # green cast-iron arch rib and spandrel along each face
                yy = side * (width / 2 - 0.3)
                pts = []
                for k in range(17):
                    t = k / 16
                    xx = x0 + (x1 - x0) * t
                    zz = -5.5 + 4.2 * math.sin(math.pi * t)
                    pts.append(Vector((xx, yy, zz)))
                for a, b in zip(pts, pts[1:]):
                    out = Vector((0, side, 0))
                    B.face([a, b, Vector((b.x, b.y, -0.2)), Vector((a.x, a.y, -0.2))], "Metal_Green",
                           [(a.x, a.z), (b.x, b.z), (b.x, -0.2), (a.x, -0.2)], WHITE, normal=out)
                    cyl_between(B, a, b, 0.35, "Metal_Green", 6)
                for k in range(1, 16, 2):  # gothic quatrefoil openwork, approximated by small round bosses
                    p = pts[k]
                    cm.Builder.prism(B, (p.x, yy + side * 0.2), 0.3, p.z + 0.5, -0.4, 6, "Paint_Yellow", WHITE)
            # underside soffit
            mid = (x0 + x1) / 2
            box(B, (mid, 0, -1.4), (span, width - 1, 0.4), "Metal_Green")
            x = x1
    # Gothic lamp standards (three-headed) along both parapets every 25 m.
    for i in range(11):
        xx = -length / 2 + 12 + i * (length - 24) / 10
        for side in (-1, 1):
            yy = side * (width / 2 - 0.6)
            cyl_between(B, (xx, yy, 1.1), (xx, yy, 6.5), 0.18, "Metal_Green", 8)
            for k in (-1, 0, 1):
                head = Vector((xx + k * 0.6, yy, 6.5 + (0.5 if k == 0 else 0)))
                cm.Builder.prism(B, (head.x, head.y), 0.28, head.z, head.z + 0.8, 6, "Lamp_Glass", WHITE)


# ---------------------------------------------------------------------------------------------------------- skyline
def floor_bands(B, profile_fn, z0, z1, step, mat, segments=8):
    """Thin horizontal mullion rings (floor lines) on a lathe-like tower."""
    z = z0
    while z < z1:
        r = profile_fn(z) + 0.15
        lathe(B, [(r, z), (r, z + 0.25)], segments, mat, cap=False)
        z += step


def shard(B):
    H = 310.0
    facets = 8
    base_r = 33.0
    tops = [290, 306, 296, 310, 300, 304, 292, 308]
    for i in range(facets):
        a0 = 2 * math.pi * i / facets
        a1 = 2 * math.pi * (i + 1) / facets
        mid = (a0 + a1) / 2
        off = Vector((math.cos(mid), math.sin(mid), 0)) * (0.6 if i % 2 else 0.0)  # the fissures between shards
        top = tops[i]
        r_top = 0.08 * base_r * (1 - top / H) + 1.2
        bl = Vector((base_r * math.cos(a0), base_r * math.sin(a0), 0)) + off
        br = Vector((base_r * math.cos(a1), base_r * math.sin(a1), 0)) + off
        tr = Vector((r_top * math.cos(a1), r_top * math.sin(a1), top)) + off
        tl = Vector((r_top * math.cos(a0), r_top * math.sin(a0), top)) + off
        steps = 40
        for k in range(steps):
            t0, t1 = k / steps, (k + 1) / steps
            q = [bl.lerp(tl, t0), br.lerp(tr, t0), br.lerp(tr, t1), bl.lerp(tl, t1)]
            col = (1, 1, 1, 0.3 + 0.4 * ((i * 7 + k * 3) % 5) / 5)
            B.face(q, "Curtain_Glass", [(0, q[0].z), (8, q[1].z), (8, q[2].z), (0, q[3].z)], col,
                   normal=Vector((math.cos(mid), math.sin(mid), 0.1)))
            box(B, (q[0] + q[1]) / 2, ((q[1] - q[0]).length, 0.3, 0.25), "Frame_Dark", rot_z=math.atan2((q[1] - q[0]).y, (q[1] - q[0]).x))


def gherkin(B):
    H = 180.0
    prof = lambda z: 24.5 + 3.5 * math.sin(math.pi * min(z, 120) / 140) if z < 120 else 26.5 * math.cos((z - 120) / 60 * math.pi / 2) ** 0.8  # noqa: E731
    segments, rows = 36, 48
    for j in range(rows):
        z0, z1 = H * j / rows, H * (j + 1) / rows
        r0, r1 = max(prof(z0), 0.2), max(prof(z1), 0.2)
        for i in range(segments):
            a0 = 2 * math.pi * (i + 0.5 * (j % 2)) / segments
            a1 = 2 * math.pi * (i + 1 + 0.5 * (j % 2)) / segments
            p = [Vector((r0 * math.cos(a0), r0 * math.sin(a0), z0)), Vector((r0 * math.cos(a1), r0 * math.sin(a1), z0)),
                 Vector((r1 * math.cos(a1 + math.pi / segments), r1 * math.sin(a1 + math.pi / segments), z1)),
                 Vector((r1 * math.cos(a0 + math.pi / segments), r1 * math.sin(a0 + math.pi / segments), z1))]
            spiral = ((i - j * 0.75) % (segments / 6)) < 1.6  # the six dark spiral light-wells
            mat = "Frame_Dark" if spiral else "Curtain_Glass"
            col = (1, 1, 1, 0.5 + 0.5 * ((i * 13 + j * 7) % 11) / 11)
            B.face(p, mat, [(a0 * r0, z0), (a1 * r0, z0), (a1 * r1, z1), (a0 * r1, z1)], col,
                   normal=Vector((math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2), 0)))


def walkie_talkie(B):
    H, depth = 160.0, 34.0
    rows = 32
    for j in range(rows):
        z0, z1 = H * j / rows, H * (j + 1) / rows
        w0 = 21 + 9 * (z0 / H) ** 1.6
        w1 = 21 + 9 * (z1 / H) ** 1.6
        for side in (-1, 1):  # the bulging east and west faces
            q = [Vector((side * w0, -depth / 2, z0)), Vector((side * w0, depth / 2, z0)),
                 Vector((side * w1, depth / 2, z1)), Vector((side * w1, -depth / 2, z1))]
            B.face(q, "Curtain_Glass", [(0, z0), (depth, z0), (depth, z1), (0, z1)], (1, 1, 1, 0.6), normal=(side, 0, 0))
        for side in (-1, 1):  # flat north and south faces, widening upward
            q = [Vector((-w0, side * depth / 2, z0)), Vector((w0, side * depth / 2, z0)),
                 Vector((w1, side * depth / 2, z1)), Vector((-w1, side * depth / 2, z1))]
            B.face(q, "Curtain_Glass", [(-w0, z0), (w0, z0), (w1, z1), (-w1, z1)], (1, 1, 1, 0.5), normal=(0, side, 0))
        for x in range(-28, 30, 4):  # vertical fins on the flat faces
            if abs(x) < w0:
                for side in (-1, 1):
                    box(B, (x, side * (depth / 2 + 0.3), (z0 + z1) / 2), (0.25, 0.6, z1 - z0), "Frame_Dark")
    wt = 21 + 9
    B.face([Vector((-wt, -depth / 2, H)), Vector((wt, -depth / 2, H)), Vector((wt, depth / 2, H + 8)), Vector((-wt, depth / 2, H + 8))],
           "Curtain_Glass", [(0, 0), (60, 0), (60, 34), (0, 34)], (1, 1, 1, 0.9), normal=(0, -1, 1))


def bt_tower(B):
    prof = [(8.5, 0), (7.6, 30), (7.0, 110), (9.0, 112), (9.0, 116), (7.0, 117), (9.2, 120), (9.2, 126), (7.0, 127),
            (8.4, 129), (8.4, 152), (7.4, 154), (7.4, 160), (4.0, 162), (3.0, 175), (0.4, 189)]
    lathe(B, prof, 24, "Concrete")
    lathe(B, [(7.75, 30), (7.15, 108)], 24, "Curtain_Glass", cap=False)


def piccadilly_lights(B):
    """Curved screen ~38 m wide x 11 m tall on a building corner, facing +X; UV 0..1 across the whole screen."""
    R, arc = 30.0, 38.0 / 30.0
    n = 24
    for i in range(n):
        a0 = -arc / 2 + arc * i / n
        a1 = -arc / 2 + arc * (i + 1) / n
        p0 = Vector((R * math.cos(a0) - R + 1.0, R * math.sin(a0), 7.0))
        p1 = Vector((R * math.cos(a1) - R + 1.0, R * math.sin(a1), 7.0))
        q = [p0, p1, p1 + Vector((0, 0, 11)), p0 + Vector((0, 0, 11))]
        B.face(q, "Screen", [(i / n, 0), ((i + 1) / n, 0), ((i + 1) / n, 1), (i / n, 1)], WHITE,
               normal=Vector((math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2), 0)))
        B.face([p0 - Vector((0.4, 0, 0)), p1 - Vector((0.4, 0, 0)), p1 + Vector((-0.4, 0, 11)), p0 + Vector((-0.4, 0, 11))],
               "Frame_Dark", [(0, 0), (1, 0), (1, 1), (0, 1)], WHITE,
               normal=-Vector((math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2), 0)))


def belisha_beacon(B):
    """Zebra crossing beacon: a pole in black and white bands, an amber globe on top (the Fab one renders white)."""
    bands = 8
    for i in range(bands):
        z0, z1 = 0.3 * i, 0.3 * (i + 1)
        lathe(B, [(0.055, z0), (0.055, z1)], 12, "Paint_White" if i % 2 else "Frame_Dark", cap=False)
    lathe(B, [(0.12, -0.05), (0.12, 0.05)], 12, "Frame_Dark")
    globe = [(0.02, 2.42), (0.11, 2.47), (0.16, 2.56), (0.17, 2.65), (0.16, 2.74), (0.11, 2.83), (0.02, 2.88)]
    lathe(B, globe, 16, "Beacon_Amber")


PIECES = {"belisha_beacon": belisha_beacon, "london_eye": london_eye, "tower_bridge": tower_bridge, "westminster_bridge": westminster_bridge,
          "shard": shard, "gherkin": gherkin, "walkie_talkie": walkie_talkie, "bt_tower": bt_tower,
          "piccadilly_lights": piccadilly_lights}


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out_dir, keys = args[0], args[1:] or list(PIECES)
    os.makedirs(out_dir, exist_ok=True)
    for key in keys:
        cm.clear()
        B = cm.Builder()
        PIECES[key](B)
        o = B.obj(key)
        cm.export(o, os.path.join(out_dir, f"{key}.glb"))
        print(f"[make_london_pieces] {key}: {len(B.faces)} faces")


if __name__ == "__main__":
    main()
