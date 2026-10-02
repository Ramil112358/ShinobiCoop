"""Blender (5.2) headless: the London collage's buildings, streets and water as meshes, one set per 128 m chunk.

  blender -b -P Scripts/london/blender/make_city_meshes.py -- <city.json> <out_dir> [--chunks k1,k2,...]
        [--preview <png> --camera x,y,z,yaw,pitch[,lens]]  [--sheet <png>]

Reads SourceArt/World/London/city.json (Scripts/london/make_city.py). Writes per chunk key (x_y of the 128 m grid):
  B_<key>.glb  playable buildings: facades split into floors and bays, recessed sash/curtain windows with frames, sills,
               lintels, cornices, parapets, shopfronts with fascia signs, mansard attics, chimney stacks with pots
  K_<key>.glb  backdrop buildings: plain walls (the UE material paints windows on), flat roofs
  G_<key>.glb  ground: carriageway, pavement with 15 cm granite kerbs, road paint, embankment walls, bridge decks
  W_<key>.glb  water surfaces
  X_<key>.glb  collision-only walls along river and canal edges (hidden in game)
Map metres (+X east, +Y SOUTH) become Blender (x, -y, z), so north is +Y here; UE's importer flips Y back.
Material slots are palette names (Brick_Stock, Glass, ...) that Scripts/world/import_library.py maps to Poly Haven PBR
instances. UVs are world-scale (1 unit = 1 m). Vertex colour RGB = per-building tint, A = per-window random.
--preview renders the given chunks with Poly Haven textures from a street-level camera (review only, no export);
--sheet renders one sample building per style. Deterministic.
"""
import hashlib
import json
import math
import os
import sys

import bmesh  # noqa: F401  (kept for interactive debugging)
import bpy
from mathutils import Vector
from mathutils.geometry import tessellate_polygon

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PROJECT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
PH = os.path.join(PROJECT, "SourceArt", "World", "PolyHaven")

# style -> wall, trim, frame materials; bay width, window width fraction, window height fraction, recess depth
STYLE = {
    "georgian_terrace": dict(wall="Brick_Stock", trim="Trim_Stucco", frame="Frame_White", bay=2.7, ww=0.42, wh=0.62, depth=0.22, floor=3.2, sash=True),
    "victorian_terrace": dict(wall="Brick_Red", trim="Trim_Stucco", frame="Frame_White", bay=2.9, ww=0.45, wh=0.58, depth=0.2, floor=3.1, sash=True),
    "stucco": dict(wall="Stucco", trim="Trim_Stucco", frame="Frame_White", bay=2.8, ww=0.42, wh=0.62, depth=0.2, floor=3.3, sash=True),
    "portland_stone": dict(wall="Stone_Portland", trim="Trim_Portland", frame="Frame_White", bay=3.2, ww=0.45, wh=0.6, depth=0.3, floor=3.6, sash=True),
    "gothic": dict(wall="Stone_Gothic", trim="Trim_Gothic", frame="Frame_Dark", bay=1.9, ww=0.5, wh=0.72, depth=0.35, floor=4.2, sash=False),
    "warehouse": dict(wall="Brick_Warehouse", trim="Trim_Portland", frame="Frame_Dark", bay=3.0, ww=0.5, wh=0.55, depth=0.3, floor=3.4, sash=False),
    "modern_glass": dict(wall="Concrete", trim="Concrete", frame="Frame_Dark", bay=1.6, ww=0.94, wh=0.86, depth=0.06, floor=3.6, sash=False),
    "concrete_modern": dict(wall="Concrete", trim="Concrete", frame="Frame_Dark", bay=2.4, ww=0.88, wh=0.5, depth=0.12, floor=3.3, sash=False),
}
PARAPET = 0.9
SIGN_COLS, SIGN_ROWS = 2, 16  # signage atlas grid (Scripts/london/make_signage.py)
SIGNAGE = os.path.join(PROJECT, "SourceArt", "World", "London", "T_Signage.png")
# Poly Haven texture set per material (tile size in metres); None = flat colour.
PBR = {"Brick_Stock": ("brick_wall_02", 2.0), "Brick_Red": ("red_brick", 2.0), "Brick_Warehouse": ("brick_wall_001", 2.0),
       "Brick_Pale": ("brick_wall_09", 2.0), "Stone_Portland": ("white_sandstone_bricks_03", 3.0),
       "Stone_Gothic": ("sandstone_blocks_05", 3.0), "Stucco": ("plastered_wall", 3.0), "Concrete": ("concrete_wall_008", 3.0),
       "Trim_Portland": ("white_sandstone_bricks_03", 3.0), "Trim_Gothic": ("sandstone_blocks_05", 3.0),
       "Trim_Stucco": ("plastered_wall", 3.0), "Slate": ("grey_roof_tiles_02", 2.0), "Roof_Flat": ("concrete_floor", 4.0),
       "Asphalt": ("asphalt_02", 4.0), "Paving": ("large_grey_tiles", 2.4), "Kerb": ("granite_tile", 1.0),
       "Setts": ("cobblestone_floor_03", 2.0), "Embankment": ("stone_brick_wall_001", 3.0)}
FLAT = {"Glass": (0.05, 0.07, 0.08, 0.05), "Frame_White": (0.85, 0.85, 0.82, 0.5), "Frame_Dark": (0.04, 0.045, 0.05, 0.4),
        "Shop_Paint": (0.8, 0.8, 0.8, 0.45), "Signage": (0.12, 0.2, 0.15, 0.5), "Door": (0.05, 0.06, 0.07, 0.4),
        "ChimneyPot": (0.55, 0.28, 0.17, 0.8), "Paint_White": (0.85, 0.85, 0.85, 0.6), "Paint_Yellow": (0.85, 0.65, 0.05, 0.6),
        "Water": (0.03, 0.08, 0.1, 0.05), "Metal_Green": (0.08, 0.18, 0.12, 0.4), "Collision": (1, 0, 1, 1)}
STUCCO_TINTS = [(0.95, 0.94, 0.9), (0.95, 0.9, 0.8), (0.93, 0.88, 0.86), (0.86, 0.9, 0.93), (0.88, 0.92, 0.86)]
CAMDEN_TINTS = [(0.75, 0.15, 0.15), (0.35, 0.2, 0.55), (0.9, 0.75, 0.2), (0.2, 0.45, 0.7), (0.15, 0.55, 0.4), (0.9, 0.45, 0.6)]
SHOP_TINTS = [(0.05, 0.2, 0.12), (0.06, 0.08, 0.2), (0.04, 0.04, 0.04), (0.35, 0.05, 0.08), (0.85, 0.82, 0.72), (0.15, 0.3, 0.35)]


def rnd(*keys):
    return int.from_bytes(hashlib.sha1("|".join(map(str, keys)).encode()).digest()[:4], "little") / 2**32


# --------------------------------------------------------------------------------------------------------- builder
class Builder:
    """Accumulates faces (own vertices: flat shading), material indices, world-scale UVs and vertex colours."""

    def __init__(self):
        self.verts, self.faces, self.mats, self.uvs, self.cols = [], [], [], [], []
        self.mat_names = []

    def mat(self, name):
        if name not in self.mat_names:
            self.mat_names.append(name)
        return self.mat_names.index(name)

    def face(self, pts, mat, uvs, col=(1, 1, 1, 1), normal=None):
        if normal is not None:
            n = Vector((0, 0, 0))
            for i, a in enumerate(pts):  # Newell's method
                b = pts[(i + 1) % len(pts)]
                n.x += (a[1] - b[1]) * (a[2] + b[2])
                n.y += (a[2] - b[2]) * (a[0] + b[0])
                n.z += (a[0] - b[0]) * (a[1] + b[1])
            if n.dot(Vector(normal)) < 0:
                pts, uvs = pts[::-1], uvs[::-1]
        base = len(self.verts)
        self.verts.extend(tuple(p) for p in pts)
        self.faces.append(tuple(range(base, base + len(pts))))
        self.mats.append(self.mat(mat))
        self.uvs.extend(uvs)
        self.cols.extend([col] * len(pts))

    def tris(self, rings, z, mat, col=(1, 1, 1, 1), up=True):
        """Flat polygon (outer ring + holes, Blender x/y) at height z, triangulated, UV = x/y metres."""
        loops = [[Vector((x, y, z)) for x, y in r] for r in rings if len(r) >= 3]
        if not loops:
            return
        flat = [v for loop in loops for v in loop]
        try:
            tri = tessellate_polygon(loops)
        except Exception:
            return
        for t in tri:
            pts = [flat[i] for i in t]
            self.face(pts, mat, [(p.x, p.y) for p in pts], col, normal=(0, 0, 1 if up else -1))

    def quad_on(self, frame, s0, s1, z0, z1, d, mat, col, uv_scale=1.0, uvs=None):
        """Rectangle in a facade frame (origin, along, outward): s along the wall, z up, at depth d (+ = outward)."""
        o, u, n = frame
        pts = [o + u * s0 + n * d + Vector((0, 0, z0)), o + u * s1 + n * d + Vector((0, 0, z0)),
               o + u * s1 + n * d + Vector((0, 0, z1)), o + u * s0 + n * d + Vector((0, 0, z1))]
        uvs = uvs or [(s0 * uv_scale, z0 * uv_scale), (s1 * uv_scale, z0 * uv_scale), (s1 * uv_scale, z1 * uv_scale), (s0 * uv_scale, z1 * uv_scale)]
        self.face(pts, mat, uvs, col, normal=n)

    def box_on(self, frame, s0, s1, z0, z1, d0, d1, mat, col, skip_back=True, uvs_front=None):
        """Box in a facade frame: s0..s1 along, z0..z1 up, d0..d1 out from the wall."""
        o, u, n = frame
        P = lambda s, z, d: o + u * s + n * d + Vector((0, 0, z))  # noqa: E731
        front = [P(s0, z0, d1), P(s1, z0, d1), P(s1, z1, d1), P(s0, z1, d1)]
        self.face(front, mat, uvs_front or [(s0, z0), (s1, z0), (s1, z1), (s0, z1)], col, normal=n)
        self.face([P(s0, z1, d0), P(s1, z1, d0), P(s1, z1, d1), P(s0, z1, d1)], mat, [(s0, d0), (s1, d0), (s1, d1), (s0, d1)], col, normal=(0, 0, 1))
        self.face([P(s0, z0, d0), P(s1, z0, d0), P(s1, z0, d1), P(s0, z0, d1)], mat, [(s0, d0), (s1, d0), (s1, d1), (s0, d1)], col, normal=(0, 0, -1))
        self.face([P(s0, z0, d0), P(s0, z0, d1), P(s0, z1, d1), P(s0, z1, d0)], mat, [(d0, z0), (d1, z0), (d1, z1), (d0, z1)], col, normal=-u)
        self.face([P(s1, z0, d0), P(s1, z0, d1), P(s1, z1, d1), P(s1, z1, d0)], mat, [(d0, z0), (d1, z0), (d1, z1), (d0, z1)], col, normal=u)
        if not skip_back:
            self.face([P(s0, z0, d0), P(s1, z0, d0), P(s1, z1, d0), P(s0, z1, d0)], mat, [(s0, z0), (s1, z0), (s1, z1), (s0, z1)], col, normal=-n)

    def prism(self, centre, radius, z0, z1, sides, mat, col):
        """Upright n-gon prism (chimney pots, pinnacles), with top cap."""
        cx, cy = centre
        ring = [(cx + radius * math.cos(2 * math.pi * i / sides), cy + radius * math.sin(2 * math.pi * i / sides)) for i in range(sides)]
        for i in range(sides):
            a, b = ring[i], ring[(i + 1) % sides]
            out = Vector(((a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy, 0))
            self.face([Vector((a[0], a[1], z0)), Vector((b[0], b[1], z0)), Vector((b[0], b[1], z1)), Vector((a[0], a[1], z1))],
                      mat, [(i * 0.3, z0), ((i + 1) * 0.3, z0), ((i + 1) * 0.3, z1), (i * 0.3, z1)], col, normal=out)
        self.face([Vector((x, y, z1)) for x, y in ring], mat, [(x, y) for x, y in ring], col, normal=(0, 0, 1))

    def obj(self, name):
        if not self.faces:
            return None
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.verts, [], self.faces)
        mesh.update()
        uv = mesh.uv_layers.new(name="UVMap")
        for i, loop in enumerate(mesh.loops):
            uv.data[i].uv = self.uvs[loop.vertex_index]
        attr = mesh.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
        for i, loop in enumerate(mesh.loops):
            attr.data[i].color = self.cols[loop.vertex_index]
        for name_ in self.mat_names:
            mesh.materials.append(material(name_))
        for poly, m in zip(mesh.polygons, self.mats):
            poly.material_index = m
            poly.use_smooth = False
        o = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(o)
        return o


# -------------------------------------------------------------------------------------------------------- materials
_previews = "--preview" in ARGS or "--sheet" in ARGS


def material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    base = name[3:] if name.startswith("BD_") else name
    if base in FLAT:
        r, g, b, rough = FLAT[base]
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Roughness"].default_value = rough
        if base == "Glass":
            bsdf.inputs["Metallic"].default_value = 0.0
            bsdf.inputs["Specular IOR Level"].default_value = 1.0
    if not _previews:
        return mat  # exports carry names only: UE builds the real materials
    mat.use_backface_culling = "--cull" in ARGS  # match UE (one-sided): inside-out faces vanish
    tint = nt.nodes.new("ShaderNodeVertexColor")
    tint.layer_name = "Col"
    if base in PBR:
        tex_id, tile = PBR[base]
        uv = nt.nodes.new("ShaderNodeUVMap")
        mapping = nt.nodes.new("ShaderNodeMapping")
        mapping.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1)
        nt.links.new(uv.outputs["UV"], mapping.inputs["Vector"])

        def tex(suffix, colour):
            path = os.path.join(PH, tex_id, f"{tex_id}_{suffix}_2k.jpg")
            node = nt.nodes.new("ShaderNodeTexImage")
            if os.path.exists(path):
                node.image = bpy.data.images.load(path, check_existing=True)
                if not colour:
                    node.image.colorspace_settings.name = "Non-Color"
            nt.links.new(mapping.outputs["Vector"], node.inputs["Vector"])
            return node

        diff = tex("diff", True)
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        ca = next(i for i in mix.inputs if i.name == "A" and i.type == "RGBA")
        cb = next(i for i in mix.inputs if i.name == "B" and i.type == "RGBA")
        out = next(o for o in mix.outputs if o.name == "Result" and o.type == "RGBA")
        nt.links.new(diff.outputs["Color"], ca)
        nt.links.new(tint.outputs["Color"], cb)
        nt.links.new(out, bsdf.inputs["Base Color"])
        nt.links.new(tex("rough", False).outputs["Color"], bsdf.inputs["Roughness"])
        nmap = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(tex("nor_gl", False).outputs["Color"], nmap.inputs["Color"])
        nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
        if name.startswith("BD_"):
            bsdf.inputs["Roughness"].default_value = 0.9
    elif base == "Signage" and os.path.exists(SIGNAGE):
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(SIGNAGE, check_existing=True)
        nt.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
    elif base in ("Shop_Paint",):
        nt.links.new(tint.outputs["Color"], bsdf.inputs["Base Color"])
    elif base == "Glass":
        # Per-window variation: some panes lighter (blinds, lit rooms), most dark and reflective.
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].color = (0.006, 0.008, 0.01, 1)
        ramp.color_ramp.elements[1].color = (0.12, 0.1, 0.07, 1)
        ramp.color_ramp.elements[0].position = 0.75
        nt.links.new(tint.outputs["Alpha"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.04
    return mat


# -------------------------------------------------------------------------------------------------------- buildings
def b_xy(p):
    return (p[0], -p[1])


def signed_area(ring):
    return sum(ring[i][0] * ring[(i + 1) % len(ring)][1] - ring[(i + 1) % len(ring)][0] * ring[i][1] for i in range(len(ring))) / 2


def edges(ring, hole=False):
    """(a, b, along unit, outward unit, length) per edge of a Blender-space ring."""
    ccw = signed_area(ring) > 0
    out = []
    for i, a in enumerate(ring):
        b = ring[(i + 1) % len(ring)]
        d = Vector((b[0] - a[0], b[1] - a[1], 0))
        length = d.length
        if length < 1e-4:
            continue
        u = d / length
        n = Vector((u.y, -u.x, 0)) if ccw else Vector((-u.y, u.x, 0))
        if hole:
            n = -n
        out.append((Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0)), u, n, length))
    return out


def tint_for(b):
    s = b["seed"]
    if b["style"] == "stucco":
        tints = CAMDEN_TINTS + STUCCO_TINTS if b["district"] == "CAM" else STUCCO_TINTS
        t = tints[int(s * len(tints)) % len(tints)]
    else:
        k = 0.85 + 0.3 * rnd("tint", b["id"])
        t = (k, k * (0.97 + 0.06 * s), k * (0.95 + 0.08 * rnd("tb", b["id"])))
    return t


def window(B, frame, x0, x1, z0, z1, st, wall, col, seed):
    """Recessed window: reveals, glass, frame with glazing bars, sill and lintel."""
    d = st["depth"]
    o, u, n = frame
    gcol = (col[0], col[1], col[2], rnd("w", seed))
    # reveals
    for (sa, sb, za, zb, nn) in ((x0, x0, z0, z1, u), (x1, x1, z0, z1, -u)):
        pts = [o + u * sa + Vector((0, 0, za)), o + u * sa + n * -d + Vector((0, 0, za)),
               o + u * sa + n * -d + Vector((0, 0, zb)), o + u * sa + Vector((0, 0, zb))]
        B.face(pts, wall, [(0, za), (d, za), (d, zb), (0, zb)], col, normal=nn)
    for (zz, nn) in ((z0, Vector((0, 0, 1))), (z1, Vector((0, 0, -1)))):
        pts = [o + u * x0 + Vector((0, 0, zz)), o + u * x1 + Vector((0, 0, zz)),
               o + u * x1 + n * -d + Vector((0, 0, zz)), o + u * x0 + n * -d + Vector((0, 0, zz))]
        B.face(pts, wall, [(x0, 0), (x1, 0), (x1, d), (x0, d)], col, normal=nn)
    g = -d + 0.05
    B.quad_on(frame, x0, x1, z0, z1, g, "Glass", gcol)
    fr, ft = 0.07, 0.05
    fm = st["frame"]
    B.box_on(frame, x0, x1, z0, z0 + fr, g, g + ft, fm, col)
    B.box_on(frame, x0, x1, z1 - fr, z1, g, g + ft, fm, col)
    B.box_on(frame, x0, x0 + fr, z0, z1, g, g + ft, fm, col)
    B.box_on(frame, x1 - fr, x1, z0, z1, g, g + ft, fm, col)
    if st["sash"] and z1 - z0 > 1.0:
        zm = (z0 + z1) / 2
        B.box_on(frame, x0, x1, zm - 0.04, zm + 0.04, g, g + ft + 0.02, fm, col)
        xm = (x0 + x1) / 2
        B.box_on(frame, xm - 0.02, xm + 0.02, z0, z1, g, g + ft * 0.6, fm, col)
    if st["depth"] >= 0.15:
        B.box_on(frame, x0 - 0.08, x1 + 0.08, z0 - 0.09, z0, -0.02, 0.07, st["trim"], col)
        B.box_on(frame, x0 - 0.06, x1 + 0.06, z1, z1 + 0.22, -0.01, 0.035, st["trim"], col)


def wall_with_windows(B, frame, length, rows, st, wall, col, seed, shop=None):
    """One street facade: rows of (z0, z1, kind) split into bays; wall quads around each window."""
    nb = max(1, round(length / st["bay"]))
    bw = length / nb
    modern = st["wall"] == "Concrete" and st["frame"] == "Frame_Dark"
    for r, (z0, z1, kind) in enumerate(rows):
        h = z1 - z0
        for i in range(nb):
            s0, s1 = i * bw, (i + 1) * bw
            if kind == "plain" or bw < 1.2 or h < 1.6:
                B.quad_on(frame, s0, s1, z0, z1, 0, wall, col)
                continue
            if kind == "shop":
                shop_bay(B, frame, s0, s1, z0, z1, i, nb, st, wall, col, f"{seed}|{i}", shop)
                continue
            ww = bw * st["ww"]
            wh = h * st["wh"]
            if kind == "attic":
                ww, wh = min(ww, 1.0), h * 0.5
            sill = (h - wh) * (0.55 if modern else 0.45)
            x0, x1 = s0 + (bw - ww) / 2, s0 + (bw + ww) / 2
            w0, w1 = z0 + sill, z0 + sill + wh
            B.quad_on(frame, s0, x0, z0, z1, 0, wall, col)
            B.quad_on(frame, x1, s1, z0, z1, 0, wall, col)
            B.quad_on(frame, x0, x1, z0, w0, 0, wall, col)
            B.quad_on(frame, x0, x1, w1, z1, 0, wall, col)
            window(B, frame, x0, x1, w0, w1, st, wall, col, f"{seed}|{r}|{i}")


def shop_bay(B, frame, s0, s1, z0, z1, i, nb, st, wall, col, seed, shop):
    """Ground floor shop bay: stall riser, plate glass, pilasters, a door in one bay; fascia added per run."""
    paint = shop["paint"]
    pil = 0.3
    door = i == shop["door_bay"]
    gx0, gx1 = s0 + pil, s1 - pil
    top = min(z1, z0 + 3.0)
    B.box_on(frame, s0, s0 + pil, z0, top, 0, 0.12, "Shop_Paint", paint)
    B.box_on(frame, s1 - pil, s1, z0, top, 0, 0.12, "Shop_Paint", paint)
    if top < z1:
        B.quad_on(frame, s0, s1, top, z1, 0, wall, col)
    gcol = (paint[0], paint[1], paint[2], 0.85 + 0.15 * rnd("shopglass", seed))  # shop glass reads lit
    if door:
        dx0, dx1 = (gx0 + gx1) / 2 - 0.55, (gx0 + gx1) / 2 + 0.55
        B.quad_on(frame, gx0, dx0, z0, top, 0.04, "Shop_Paint", paint)
        B.quad_on(frame, dx1, gx1, z0, top, 0.04, "Shop_Paint", paint)
        B.quad_on(frame, dx0, dx1, z0 + 2.4, top, 0.04, "Shop_Paint", paint)
        B.quad_on(frame, dx0, dx1, z0, z0 + 2.4, -0.25, "Door", paint)
        for xa, xb, nn in ((dx0, dx0, frame[1]), (dx1, dx1, -frame[1])):
            o, u, n = frame
            pts = [o + u * xa + n * 0.04 + Vector((0, 0, z0)), o + u * xa + n * -0.25 + Vector((0, 0, z0)),
                   o + u * xa + n * -0.25 + Vector((0, 0, z0 + 2.4)), o + u * xa + n * 0.04 + Vector((0, 0, z0 + 2.4))]
            B.face(pts, "Shop_Paint", [(0, 0), (0.3, 0), (0.3, 2.4), (0, 2.4)], paint, normal=nn)
        return
    B.box_on(frame, gx0, gx1, z0, z0 + 0.5, 0, 0.06, "Shop_Paint", paint)
    B.quad_on(frame, gx0, gx1, z0 + 0.5, top - 0.15, -0.08, "Glass", gcol)
    B.box_on(frame, gx0, gx1, top - 0.15, top, -0.08, 0.02, "Shop_Paint", paint)
    B.box_on(frame, gx0, gx1, z0 + 0.5, z0 + 0.58, -0.08, 0.1, "Shop_Paint", paint)  # sill


def fascia(B, frame, length, z, shop):
    """Shop sign board across the whole frontage; its face shows one cell of the signage atlas."""
    cell = shop["sign"]
    cu, cv = cell % SIGN_COLS, cell // SIGN_COLS
    u0, u1 = cu / SIGN_COLS, (cu + 1) / SIGN_COLS
    _, u, n = frame
    if n.x * u.y - n.y * u.x < 0:  # the wall runs right-to-left as seen from the street: mirror the atlas cell back
        u0, u1 = u1, u0
    v1, v0 = 1 - cv / SIGN_ROWS, 1 - (cv + 1) / SIGN_ROWS
    B.box_on(frame, 0.05, length - 0.05, z, z + 0.75, 0, 0.16, "Shop_Paint", shop["paint"])
    B.quad_on(frame, 0.25, length - 0.25, z + 0.1, z + 0.65, 0.17, "Signage", (1, 1, 1, 1), uvs=[(u0, v0), (u1, v0), (u1, v1), (u0, v1)])


def building(B, b, cheap=False):
    st = STYLE[b["style"]]
    base, top = b["base"], b["top"]
    col_rgb = tint_for(b)
    col = (*col_rgb, 1.0)
    wall = st["wall"]
    if b["style"] == "georgian_terrace" and b["seed"] < 0.3:
        wall = "Brick_Pale"
    if cheap:
        wall = "BD_" + wall
    rings = [[b_xy(p) for p in r] for r in b["rings"]]
    flat_roof = b["roof"] == "flat" or cheap
    roof_z = top - PARAPET if flat_roof and top - base > 6 and not cheap else top
    shop = None
    if b.get("shop") and not cheap:
        shop = {"paint": (*SHOP_TINTS[int(rnd("paint", b["id"]) * len(SHOP_TINTS))], 1.0),
                "sign": int(rnd("sign", b["id"]) * SIGN_COLS * SIGN_ROWS), "door_bay": 0}
    gh = 4.2 if shop else (3.6 if base == 0 else st["floor"])
    for hi, ring in enumerate(rings):
        flags = b.get("street", []) if hi == 0 else []
        for k, (a, bb, u, n, length) in enumerate(edges(ring, hole=hi > 0)):
            frame = (Vector((a.x, a.y, 0)), u, n)
            street = hi == 0 and not cheap and (flags[k] if k < len(flags) else True)
            if not street or length < 2.0:
                B.quad_on(frame, 0, length, base, top, 0, wall, col)
                continue
            rows, z = [], base
            if base == 0:
                rows.append((0, gh, "shop" if shop else "up"))
                z = gh
            body_top = roof_z if flat_roof else top
            n_up = max(0, int((body_top - z - 0.3) / st["floor"]))
            fh = (body_top - z - 0.3) / n_up if n_up else 0
            for f in range(n_up):
                rows.append((z, z + fh, "up"))
                z += fh
            rows.append((z, top, "plain"))
            if shop and length >= 3.0:
                shop["door_bay"] = int(rnd("door", b["id"], k) * max(1, round(length / st["bay"])))
            wall_with_windows(B, frame, length, rows if not (shop and length < 3.0) else
                              [(r[0], r[1], "up" if r[2] == "shop" else r[2]) for r in rows], st, wall, col, f"{b['id']}|{k}",
                              shop if length >= 3.0 else None)
            if st["sash"] and b["style"] != "victorian_terrace":
                for z0, z1, kind in rows[1:-1]:  # a projecting band at each floor line
                    B.box_on(frame, 0, length, z0 - 0.08, z0 + 0.06, 0, 0.05, st["trim"], col)
            if shop and length >= 3.0:
                fascia(B, frame, length, 3.2, shop)
                B.box_on(frame, 0, length, 3.95, 4.2, 0, 0.22, st["trim"], col)  # shop cornice
            if top - base > 7:
                zc = roof_z if flat_roof else top - 0.4
                B.box_on(frame, -0.05, length + 0.05, zc - 0.05, zc + 0.35, 0, 0.3 if st["depth"] >= 0.15 else 0.1, st["trim"], col)
                B.box_on(frame, 0, length, top - 0.12, top, -0.3, 0.08, st["trim"], col)  # coping
            if b["style"] == "gothic":
                nb = max(1, round(length / st["bay"]))
                for i in range(nb + 1):
                    s = i * length / nb
                    c = frame[0] + frame[1] * s + frame[2] * 0.15
                    B.prism((c.x, c.y), 0.22, top, top + 2.6, 4, st["trim"], col)
                    B.prism((c.x, c.y), 0.12, top + 2.6, top + 3.4, 4, st["trim"], col)
    # Roof deck (inside the parapet) and the parapet's inner face.
    B.tris(rings, roof_z, "Roof_Flat", col)
    if roof_z < top:
        for hi, ring in enumerate(rings):
            for a, bb, u, n, length in edges(ring, hole=hi > 0):
                frame = (Vector((a.x, a.y, 0)), u, n)
                B.quad_on(frame, 0, length, roof_z, top, -0.3, wall, col)
    if not cheap and b.get("attic"):
        attic = [[b_xy(p) for p in r] for r in b["attic"]]
        ah = 2.7
        for a, bb, u, n, length in edges(attic[0]):
            frame = (Vector((a.x, a.y, 0)), u, n)
            wall_with_windows(B, frame, length, [(roof_z, roof_z + ah, "attic")], STYLE[b["style"]] | {"wall": "Slate"},
                              "Slate", col, f"{b['id']}|attic")
        B.tris(attic, roof_z + ah, "Roof_Flat", col)
    if not cheap and b["style"] in ("georgian_terrace", "victorian_terrace", "stucco") and top - base > 6:
        chimneys(B, b, rings[0], top, wall, col)


def chimneys(B, b, ring, top, wall, col):
    """A chimney stack on each party wall (non-street edge) of a terrace house, with clay pots."""
    flags = b.get("street", [])
    for k, (a, bb, u, n, length) in enumerate(edges(ring)):
        if (flags[k] if k < len(flags) else True) or length < 3:
            continue
        mid = length / 2
        sw = min(2.4, length * 0.5)
        frame = (Vector((a.x, a.y, 0)), u, n)
        B.box_on(frame, mid - sw / 2, mid + sw / 2, top - PARAPET, top + 1.5, -0.7, 0.0, wall, col, skip_back=False)
        pots = max(2, int(sw / 0.55))
        for p in range(pots):
            s = mid - sw / 2 + (p + 0.5) * sw / pots
            c = frame[0] + frame[1] * s + frame[2] * -0.35
            B.prism((c.x, c.y), 0.13, top + 1.5, top + 2.0, 6, "ChimneyPot", col)


# ----------------------------------------------------------------------------------------------------------- ground
def ground(B, chunk):
    for kind, mat, z in (("road", "Asphalt", 0.03), ("road_backdrop", "Asphalt", 0.03)):
        for poly in chunk.get(kind, []):
            B.tris([[b_xy(p) for p in r] for r in poly], z, mat)
    for kind in ("paving", "paving_backdrop"):
        for poly in chunk.get(kind, []):
            rings = [[b_xy(p) for p in r] for r in poly]
            B.tris(rings, 0.15, "Paving")
            for hi, ring in enumerate(rings):  # kerb faces all round (hidden where buildings stand)
                for a, bb, u, n, length in edges(ring, hole=hi > 0):
                    frame = (Vector((a.x, a.y, 0)), u, n)
                    B.quad_on(frame, 0, length, 0.0, 0.15, 0, "Kerb", (1, 1, 1, 1))
    for kind, mat in (("paint_white", "Paint_White"), ("paint_yellow", "Paint_Yellow")):
        for poly in chunk.get(kind, []):
            B.tris([[b_xy(p) for p in r] for r in poly], 0.036, mat)


def water(B, chunk):
    for kind, polys in chunk.items():
        if kind.startswith("water_"):
            level = float(kind[6:])
            for poly in polys:
                B.tris([[b_xy(p) for p in r] for r in poly], level, "Water")


def walls(Bs, city, Xs=None):
    """Embankment walls (both faces: the side is unknown per line) and a stone parapet along the top. Xs: per-chunk
    collision-only walls, 6 m tall, along the river and canal edges (players can't jump into the water)."""
    for w in city["water_walls"]:
        pts = [b_xy(p) for p in w["line"]]
        bed = w["level"] - 2.5
        for a, b in zip(pts, pts[1:]):
            mx, my = (a[0] + b[0]) / 2, -(a[1] + b[1]) / 2
            B = Bs(key_of(mx, my))
            d = Vector((b[0] - a[0], b[1] - a[1], 0))
            if d.length < 0.05:
                continue
            u = d.normalized()
            n = Vector((u.y, -u.x, 0))
            frame = (Vector((a[0], a[1], 0)), u, n)
            B.quad_on(frame, 0, d.length, bed, 0.15, 0, "Embankment", (1, 1, 1, 1))
            B.quad_on((Vector((b[0], b[1], 0)), -u, -n), 0, d.length, bed, 0.15, 0, "Embankment", (1, 1, 1, 1))
            if w["level"] <= -1.0:
                B.box_on(frame, 0, d.length, 0.15, 1.05, -0.25, 0.25, "Trim_Portland", (1, 1, 1, 1), skip_back=False)
                if Xs is not None:
                    X = Xs(key_of(mx, my))
                    X.quad_on(frame, 0, d.length, 0.15, 6.0, 0, "Collision", (1, 1, 1, 1))
                    X.quad_on((Vector((b[0], b[1], 0)), -u, -n), 0, d.length, 0.15, 6.0, 0, "Collision", (1, 1, 1, 1))


def bridges(Bs, city):
    """Bridge decks: carriageway between pavements, a stone parapet each side, a deep beam underneath (the
    landmark pieces add towers, arches and lamps)."""
    for br in city["bridges"]:
        pts = [b_xy(p) for p in br["line"]]
        half = br["width"] / 2
        for a, b in zip(pts, pts[1:]):
            mx, my = (a[0] + b[0]) / 2, -(a[1] + b[1]) / 2
            B = Bs(key_of(mx, my))
            d = Vector((b[0] - a[0], b[1] - a[1], 0))
            if d.length < 0.05:
                continue
            u = d.normalized()
            n = Vector((u.y, -u.x, 0))
            o = Vector((a[0], a[1], 0)) - n * half
            frame = (o, u, n)  # "s" along the bridge; "d" across it (0..width)
            L, W = d.length, br["width"]
            road = W - 7.0
            for (d0, d1, mat, z) in ((0, 3.5, "Paving", 0.15), (3.5, 3.5 + road, "Asphalt", 0.03), (W - 3.5, W, "Paving", 0.15)):
                p = [o + u * 0 + n * d0, o + u * L + n * d0, o + u * L + n * d1, o + u * 0 + n * d1]
                B.face([Vector((q.x, q.y, z)) for q in p], mat, [(q.x, q.y) for q in p], (1, 1, 1, 1), normal=(0, 0, 1))
            B.box_on(frame, 0, L, -1.6, 0.0, 0, W, "Embankment", (1, 1, 1, 1))
            for d0 in (-0.2, W - 0.3):
                B.box_on(frame, 0, L, 0.15, 1.15, d0, d0 + 0.5, "Trim_Portland", (1, 1, 1, 1), skip_back=False)


def key_of(x, y_map):
    return f"{int(math.floor(x / 128.0))}_{int(math.floor(y_map / 128.0))}"


# ------------------------------------------------------------------------------------------------------------- main
def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)


def export(o, path):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_apply=True, export_yup=True,
              export_materials="EXPORT", export_texcoords=True, export_normals=True)
    try:
        bpy.ops.export_scene.gltf(**kw, export_vertex_color="ACTIVE")
    except TypeError:
        bpy.ops.export_scene.gltf(**kw)


def build_chunks(city, keys):
    """{key: {kind: Builder}} for the given chunk keys (None = all)."""
    out = {}

    def get(key, kind):
        return out.setdefault(key, {}).setdefault(kind, Builder())

    for b in city["buildings"]:
        if keys is None or b["chunk"] in keys:
            building(get(b["chunk"], "B" if b["zone"] == "play" else "K"), b, cheap=b["zone"] != "play")
    for key, chunk in city["chunks"].items():
        if keys is None or key in keys:
            ground(get(key, "G"), chunk)
            water(get(key, "W"), chunk)
    sel = lambda key: get(key, "G") if keys is None or key in keys else Builder()  # noqa: E731
    xsel = lambda key: get(key, "X") if keys is None or key in keys else Builder()  # noqa: E731
    walls(sel, city, xsel)
    bridges(sel, city)
    return out


def setup_preview_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    sky = world.node_tree.nodes.new("ShaderNodeTexSky")
    try:
        sky.sky_type = "MULTIPLE_SCATTERING" if "MULTIPLE_SCATTERING" in [e.identifier for e in sky.bl_rna.properties["sky_type"].enum_items] else "NISHITA"
        sky.sun_elevation = math.radians(28)
        sky.sun_rotation = math.radians(215)
    except Exception:
        pass
    world.node_tree.links.new(sky.outputs["Color"], world.node_tree.nodes["Background"].inputs["Color"])
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = 7.0
    sun.angle = math.radians(1.0)
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = (math.radians(55), 0, math.radians(140))
    scene.collection.objects.link(so)
    scene.view_settings.view_transform = "AgX" if "AgX" in [v.identifier for v in bpy.types.ColorManagedViewSettings.bl_rna.properties["view_transform"].enum_items] else "Filmic"


def camera(x, y_map, z, yaw, pitch, lens=24):
    cam = bpy.data.cameras.new("Cam")
    cam.lens = lens
    co = bpy.data.objects.new("Cam", cam)
    co.location = (x, -y_map, z)
    co.rotation_euler = (math.radians(90 + pitch), 0, math.radians(yaw - 90))
    bpy.context.scene.collection.objects.link(co)
    bpy.context.scene.camera = co


def main():
    city_path, out_dir = ARGS[0], ARGS[1]
    with open(city_path, encoding="utf-8") as f:
        city = json.load(f)
    keys = None
    if "--chunks" in ARGS:
        keys = set(ARGS[ARGS.index("--chunks") + 1].split(","))
    os.makedirs(out_dir, exist_ok=True)
    if "--preview" in ARGS:
        clear()
        for key, kinds in build_chunks(city, keys).items():
            for kind, B in kinds.items():
                B.obj(f"{kind}_{key}")
        setup_preview_scene()
        x, y, z, yaw, pitch, *lens = (float(v) for v in ARGS[ARGS.index("--camera") + 1].split(","))
        camera(x, y, z, yaw, pitch, *(lens or [24]))
        bpy.context.scene.render.filepath = ARGS[ARGS.index("--preview") + 1]
        bpy.ops.render.render(write_still=True)
        print(f"[make_city_meshes] preview {bpy.context.scene.render.filepath}")
        return
    count = 0
    for key, kinds in build_chunks(city, keys).items():
        for kind, B in kinds.items():
            clear()
            o = B.obj(f"{kind}_{key}")
            if o is None:
                continue
            export(o, os.path.join(out_dir, f"{kind}_{key}.glb"))
            count += 1
    print(f"[make_city_meshes] {count} chunk meshes written to {out_dir}")


if __name__ == "__main__":
    main()
