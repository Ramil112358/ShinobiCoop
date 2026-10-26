"""<Name>: the hero's rigged body (Models/Hero/Hero_rigged.fbx) with a real head (a Tripo scan from photos,
Models/<Name>/<Name>_head.glb) in place of the hero's head, glasses and headband. Keeps the hero's Mixamo skeleton and weights,
so there is no Mixamo step. One entry in SCANS per head (Ramil, Elvin): its landmarks, measured on gridded renders.

Run: blender -b --factory-startup --python-exit-code 1 -P Scripts/character/build_head_swap.py -- <Name>
     (HEAD_CARDS=<n> in the environment overrides the scan's hair-card count)

  1. The hero's head goes: everything above HERO_CUT_Z, and the neck skin inside the turtleneck collar.
  2. Tripo's bust is cut back to the head and neck (its white T-shirt goes), scaled and placed on the hero's neck (the
     chin on the hero's chin, the neck centred in the collar), and the neck is drawn down into the collar so no gap shows
     between them; anything that would poke through the collar is pulled in.
  3. The head takes its weights from the hero's head and neck (nearest surface), and is welded and thinned to HEAD_FACES.
  4. Hair cards: Tripo's hair is one smooth shell, so CARDS strand-textured strips are grown over it along the hair's own
     flow (read from the strands painted in its texture), lifted off it towards their tips.
  5. One texture for the whole body (setup_hero.py gives every slot one PNG): a 4096 x 4096 atlas, the hero's 2k texture
     top left, the head's (4k, halved) top right, the cards' strand strips (alpha = the strands) along the bottom (no
     cards: just the top row, 4096 x 2048). The
     hero's hands and feet are tinted to the head's skin tone.

Writes SourceArt/Character/Models/<Name>/<Name>_rigged.fbx + <Name>_rigged.png (setup_hero.py imports them like any roster
character) and preview_front.png / preview_side.png. Metres, the hero faces -Y.
"""
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODELS = os.path.join(ROOT, "SourceArt", "Character", "Models")
HERO_FBX = os.path.join(MODELS, "Hero", "Hero_rigged.fbx")
HERO_PNG = os.path.join(MODELS, "Hero", "Hero_rigged.png")
NAME = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "Ramil"
HEAD_GLB = os.path.join(MODELS, NAME, f"{NAME}_head.glb")
PHOTO = os.path.join(ROOT, "SourceArt", "Character", "Photos", NAME, "front.jpeg")
OUT = os.path.join(MODELS, NAME)
sys.path.insert(0, os.path.dirname(__file__))
from clean_rig import export_unit_scale_fbx  # noqa: E402

# The hero (metres, measured on gridded renders): the head and hair start above HERO_CUT_Z; the turtleneck collar's rim is
# at 1.505 m in front, 1.535 m behind; the chin's underside is at HERO_CHIN_Z.
HERO_CUT_Z = 1.545
HERO_NECK_Z = 1.40      # below this nothing of the hero is touched
HERO_CHIN_Z = 1.525
COLLAR_CENTRE_Y = 0.015
COLLAR_RIM_FRONT, COLLAR_RIM_SLOPE = Vector((0.0, -0.05, 1.495)), 0.21  # the rim's plane: through here, rising backwards
# Each Tripo head (its own units, 1.0 tall, faces -Y): scale makes eye-to-chin 11 cm, the hero's; chin_z is the chin's
# underside, neck_y the neck's centre where it meets the shirt. Only below shirt_below_z (m, placed) is anything taken for
# the shirt, by its colour ("white": eye whites and teeth stay; "dark": hair at the nape stays). neck_slope: the neck is cut on
# a plane through the collar's rim in front, rising backwards this steeply (above a shirt neckline higher than the rim behind).
# cards: hair cards over a smooth hair shell (Ramil's); 0 keeps hair that already has strands (Elvin's) as it is.
SCANS = {
    "Ramil": dict(scale=0.42, chin_z=0.315, neck_y=0.025, shirt="white", shirt_below_z=1.57, neck_slope=0.21, cards=3000),
    "Elvin": dict(scale=0.36, chin_z=0.255, neck_y=0.045, shirt="dark", shirt_below_z=1.545, neck_slope=0.45, cards=0),
}
SCAN = SCANS[NAME]
HEAD_SCALE, HEAD_CHIN_Z, HEAD_NECK_Y, HEAD_SHIRT_BELOW_Z = SCAN["scale"], SCAN["chin_z"], SCAN["neck_y"], SCAN["shirt_below_z"]
NECK_UNDER_RIM = 0.006     # the head's neck is cut this far under the collar's rim
HEAD_FACES = 30000
NECK_BOTTOM_Z = 1.45     # the drawn-down neck ends here, well inside the collar
NECK_FIT = 0.9           # of the collar's inside, from the neck's centre
TILE = 2048  # the atlas: top row hero | head (a TILE each); below them (only with hair cards) a TILE of strand strips
CARDS = int(os.environ.get("HEAD_CARDS", SCAN["cards"]))
CARD_LENGTH = (0.035, 0.08)     # m
CARD_WIDTH = (0.005, 0.010)     # m at the root, tapering to the tip
CARD_LIFT = (0.0015, 0.007)     # m off the shell at the root and at the tip
CARD_STEP = 0.005               # m per traced segment
STRIPS = 16                     # strand-strip variants side by side in the atlas
ROOT_INSET = 2                  # face rings between a card's root and the edge of the hair
STRANDS = 26                    # per strip, bunched towards its middle so a card's edges are wisps, not a straight cut
HEAD_TONE = 0.5          # how far the head's skin moves towards the front photo's


def log(message):
    print(f"[build_head_swap {NAME}] {message}")


def image_array(image):
    pixels = np.empty(image.size[0] * image.size[1] * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    return pixels.reshape(image.size[1], image.size[0], 4)  # row 0 is v = 0


def sample(texture, uv):
    """Nearest texel of an (h, w, 4) array at UVs (n, 2)."""
    h, w = texture.shape[:2]
    x = np.clip((uv[:, 0] % 1.0) * w, 0, w - 1).astype(int)
    y = np.clip((uv[:, 1] % 1.0) * h, 0, h - 1).astype(int)
    return texture[y, x, :3]


def face_colours(mesh, texture):
    """Mean texture colour (sRGB) of each face of a mesh (its loops' UVs)."""
    uv = np.empty(len(mesh.loops) * 2, dtype=np.float32)
    mesh.uv_layers.active.data.foreach_get("uv", uv)
    colours = sample(texture, uv.reshape(-1, 2))
    starts = np.empty(len(mesh.polygons), dtype=np.int64)
    mesh.polygons.foreach_get("loop_start", starts)
    counts = np.empty(len(mesh.polygons), dtype=np.int64)
    mesh.polygons.foreach_get("loop_total", counts)
    return np.add.reduceat(colours, starts) / counts[:, None]


def is_skin(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (r > 0.4) & (r > g) & (g > b) & (r - b > 0.12)


def world_centres(obj):
    m = obj.matrix_world
    return np.array([tuple(m @ p.center) for p in obj.data.polygons])


def delete_faces(obj, doomed):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.flatnonzero(doomed)], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()


def drop_specks(obj, min_faces, region):
    """Deletes the connected pieces of fewer than min_faces faces whose verts all satisfy region(world point)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    m = obj.matrix_world
    seen, doomed = set(), []
    for start in bm.faces:
        if start.index in seen:
            continue
        piece, stack = [], [start]
        seen.add(start.index)
        while stack:
            f = stack.pop()
            piece.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if len(piece) < min_faces and all(region(m @ v.co) for f in piece for v in f.verts):
            doomed.extend(piece)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    return len(doomed)


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


bpy.ops.wm.read_factory_settings(use_empty=True)

# --- 1. The hero, headless ---------------------------------------------------------------------------------------------
bpy.ops.import_scene.fbx(filepath=HERO_FBX)
armature = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
hero_tex = image_array(bpy.data.images.load(HERO_PNG))
# The untouched hero, the head's weight source.
source = body.copy()
source.data = body.data.copy()
source.modifiers.clear()
bpy.context.scene.collection.objects.link(source)



def head_share(obj):
    """Per vertex: its weight on the head bones (Head, HeadTop_End, eyes)."""
    groups = {g.index for g in obj.vertex_groups if g.name.split(":")[-1] in ("Head", "HeadTop_End", "LeftEye", "RightEye")}
    return np.array([sum(g.weight for g in v.groups if g.group in groups) for v in obj.data.vertices])


centres = world_centres(body)
skin = is_skin(face_colours(body.data, hero_tex))
on_head = head_share(body)
face_head = np.array([on_head[list(p.vertices)].mean() for p in body.data.polygons])
# The head, its hair, glasses and headband ride the head bones; the collar the neck and spine.
doomed = (centres[:, 2] > HERO_CUT_Z) | ((centres[:, 2] > HERO_NECK_Z) & ((face_head > 0.5) | (skin & (np.abs(centres[:, 0]) < 0.12))))
delete_faces(body, doomed)
# The collar's rim is cut straight: Rodin left it toothed behind (1.52 to 1.55 m), which the hero's hair hid.
bm = bmesh.new()
bm.from_mesh(body.data)
bm.transform(body.matrix_world)
near = lambda v: v.co.z > HERO_NECK_Z and Vector((v.co.x, v.co.y - COLLAR_CENTRE_Y)).length < 0.13  # noqa: E731
region = [v for v in bm.verts if near(v)]
region += [e for e in bm.edges if all(near(v) for v in e.verts)] + [f for f in bm.faces if all(near(v) for v in f.verts)]
bmesh.ops.bisect_plane(bm, geom=region, dist=1e-5, plane_co=COLLAR_RIM_FRONT, plane_no=Vector((0.0, -COLLAR_RIM_SLOPE, 1.0)),
                       clear_outer=True)
bm.transform(body.matrix_world.inverted())
bm.to_mesh(body.data)
bm.free()
specks = drop_specks(body, 200, lambda p: p.z > HERO_NECK_Z)
log(f"hero: {int(doomed.sum())} head faces and {specks} loose ones gone, {len(body.data.polygons)} left")

# --- 2. The scanned head on the hero's neck ----------------------------------------------------------------------------
bpy.ops.import_scene.gltf(filepath=HEAD_GLB)
head = next(o for o in bpy.context.selected_objects if o.type == "MESH")
head.name = "Head"
head_image = head.active_material.node_tree.nodes["Image Texture"].image
head_tex = image_array(head_image)

# Weld the UV-seam splits (glTF), scale and place it, cut the neck just under the collar's rim, then drop what is left of the
# shirt (it rises above the rim beside the neck).
bm = bmesh.new()
bm.from_mesh(head.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
offset = Vector((0.0, COLLAR_CENTRE_Y - HEAD_NECK_Y * HEAD_SCALE, HERO_CHIN_Z - HEAD_CHIN_Z * HEAD_SCALE))
bm.transform(Matrix.Translation(offset) @ Matrix.Scale(HEAD_SCALE, 4))
bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-5, plane_co=COLLAR_RIM_FRONT - Vector((0, 0, NECK_UNDER_RIM)),
                       plane_no=Vector((0.0, -SCAN["neck_slope"], 1.0)), clear_inner=True)
bm.to_mesh(head.data)
bm.free()
head.matrix_world.identity()
colours = face_colours(head.data, head_tex)
top, low = colours.max(1), colours.min(1)
grey = (top - low) / np.maximum(top, 1e-6) < 0.18
shirt = (grey & (top > 0.38)) if SCAN["shirt"] == "white" else (top < 0.15)  # (near-black has no reliable hue)
z = np.array([p.center.z for p in head.data.polygons])
delete_faces(head, shirt & (z < HEAD_SHIRT_BELOW_Z))
specks = drop_specks(head, 500, lambda p: True)
log(f"head: shirt off ({int((shirt & (z < HEAD_SHIRT_BELOW_Z)).sum())} faces, {specks} loose), {len(head.data.polygons)} faces")

select(head)
dec = head.modifiers.new("thin", "DECIMATE")
dec.ratio = min(1.0, HEAD_FACES / len(head.data.polygons))
bpy.ops.object.modifier_apply(modifier=dec.name)

# The collar, for fitting the neck inside it.
collar = bmesh.new()
collar.from_mesh(body.data)
collar.transform(body.matrix_world)
collar_tree = BVHTree.FromBMesh(collar)


def fit_in_collar(p, centre):
    """p pulled towards the neck's axis so it stays NECK_FIT inside the collar at its height (unchanged above the rim)."""
    out = Vector((p.x - centre.x, p.y - centre.y, 0.0))
    if out.length < 1e-6:
        return p
    hit = collar_tree.ray_cast(Vector((centre.x, centre.y, p.z)), out.normalized(), 0.15)
    if hit[0] is None or out.length < NECK_FIT * hit[3]:
        return p
    out *= NECK_FIT * hit[3] / out.length
    return Vector((centre.x + out.x, centre.y + out.y, p.z))


# Draw the neck down: the open edge under the neck gets two rings below it (the second level at NECK_BOTTOM_Z) and a cap.
bm = bmesh.new()
bm.from_mesh(head.data)
uv_layer = bm.loops.layers.uv.active
edges = [e for e in bm.edges if e.is_boundary]
lowest = min(edges, key=lambda e: min(v.co.z for v in e.verts))
loop, stack, seen = [], [lowest], {lowest}
while stack:  # the open edge the lowest boundary edge is on
    e = stack.pop()
    loop.append(e)
    for v in e.verts:
        for f in v.link_edges:
            if f.is_boundary and f not in seen:
                seen.add(f)
                stack.append(f)
rim = {v for e in loop for v in e.verts}
centre = sum((v.co for v in rim), Vector()) / len(rim)
for v in rim:
    v.co = fit_in_collar(v.co, centre)
ring1, ring2 = {}, {}
for v in rim:
    ring1[v] = bm.verts.new(fit_in_collar(Vector((v.co.x, v.co.y, max(v.co.z - 0.012, NECK_BOTTOM_Z + 0.015))), centre))
    ring2[v] = bm.verts.new(fit_in_collar(Vector((v.co.x, v.co.y, NECK_BOTTOM_Z)), centre))
cap_centre = bm.verts.new(Vector((centre.x, centre.y, NECK_BOTTOM_Z)))
for e in loop:
    f = e.link_faces[0]
    face_uv = sum((l[uv_layer].uv for l in f.loops), Vector((0.0, 0.0))) / len(f.loops)
    rim_uv = {l.vert: l[uv_layer].uv.copy() for l in f.loops}
    a, b = e.verts
    # Keep the winding of the existing face: in f the edge runs a -> b, so the new face runs b -> a.
    if next(l for l in f.loops if l.vert is a).link_loop_next.vert is not b:
        a, b = b, a
    pulled = {v: rim_uv[v].lerp(face_uv, 0.6) for v in (a, b)}
    # Below the first ring one texel per face (inside the collar; a gradient there could cross into another UV island).
    for verts, uvs in (((b, a, ring1[a], ring1[b]), (rim_uv[b], rim_uv[a], pulled[a], pulled[b])),
                       ((ring1[b], ring1[a], ring2[a], ring2[b]), (pulled[a],) * 4),
                       ((ring2[b], ring2[a], cap_centre), (pulled[a],) * 3)):
        new = bm.faces.new(verts)
        for l, uv in zip(new.loops, uvs):
            l[uv_layer].uv = uv
bm.normal_update()
bm.to_mesh(head.data)
bm.free()
log(f"neck: {len(rim)} rim verts drawn down to {NECK_BOTTOM_Z} m")


# --- 4. Hair cards -------------------------------------------------------------------------------------------------------


def blur(a, radius):
    """About a Gaussian of sigma ~radius: three box passes along each axis (numpy only; Blender has no scipy)."""
    for axis in (0, 1):
        for _ in range(3):
            pad = [(radius + 1, radius) if k == axis else (0, 0) for k in range(2)]
            c = np.cumsum(np.pad(a, pad, mode="edge"), axis=axis)
            n = a.shape[axis]
            a = (np.take(c, np.arange(2 * radius + 1, 2 * radius + 1 + n), axis=axis) - np.take(c, np.arange(n), axis=axis)) / (2 * radius + 1)
    return a


def strand_flow(mesh, texture, faces):
    """Per face (of faces): the unit direction (world, along the surface) of the strands painted in the texture there, and how
    clear they are (0..1): the structure tensor of the texture's brightness, carried into 3D through each face's UVs."""
    lum = texture[..., :3].mean(2).astype(np.float32)
    gy, gx = np.gradient(lum)  # rows run along v, columns along u
    jxx, jxy, jyy = (blur(t, 4) for t in (gx * gx, gx * gy, gy * gy))
    h, w = lum.shape
    uv = mesh.uv_layers.active.data
    out, clear = np.zeros((len(faces), 3)), np.zeros(len(faces))
    for k, f in enumerate(faces):
        p = mesh.polygons[f]
        loops = list(p.loop_indices)[:3]
        P = [mesh.vertices[mesh.loops[l].vertex_index].co for l in loops]
        T = [Vector(uv[l].uv) * w for l in loops]  # pixels (square texture)
        c = (T[0] + T[1] + T[2]) / 3
        x, y = int(np.clip(c.x, 0, w - 1)), int(np.clip(c.y, 0, h - 1))
        a, b, d = jxx[y, x], jxy[y, x], jyy[y, x]
        spread = np.sqrt((a - d) ** 2 + 4 * b * b)
        angle = 0.5 * np.arctan2(2 * b, a - d) + np.pi / 2  # across the strongest gradient = along the strands
        e1, e2 = T[1] - T[0], T[2] - T[0]
        det = e1.x * e2.y - e1.y * e2.x
        if abs(det) < 1e-9:
            continue
        du, dv = np.cos(angle), np.sin(angle)
        # The UV direction (du, dv) as a combination of the two UV edges, then the same combination of the 3D edges.
        s1, s2 = (du * e2.y - dv * e2.x) / det, (dv * e1.x - du * e1.y) / det
        v3 = (P[1] - P[0]) * s1 + (P[2] - P[0]) * s2
        v3 -= p.normal * v3.dot(p.normal)
        if v3.length > 1e-9:
            out[k] = v3.normalized()
            clear[k] = spread / (a + d + 1e-9)
    return out, clear


def grow_cards(head, texture):
    """Hair cards over the head's hair: (verts, quads, UVs per quad corner) in world space with UVs in the atlas's bottom
    half, and the hair's colour."""
    mesh = head.data
    colours = face_colours(mesh, texture)
    c = world_centres(head)
    dark = colours.mean(1) < 0.2
    brows = (c[:, 1] < -0.04) & (c[:, 2] < 1.675)  # brows and lashes are dark too
    hair = dark & ~brows & (c[:, 2] > 1.60)
    faces = np.flatnonzero(hair)
    flow, clear = strand_flow(mesh, texture, faces)
    # A line has no head or tail: point every one away from the crown (down the hair), then smooth across neighbours,
    # leaning on clear strands, with a little of the away-from-crown direction where the texture says little.
    crown = Vector((0.0, 0.03, 1.80))
    index = {int(f): k for k, f in enumerate(faces)}
    normals = [mesh.polygons[f].normal.copy() for f in faces]
    away = []
    for k, f in enumerate(faces):
        a = Vector(c[f]) - crown
        a -= normals[k] * a.dot(normals[k])
        away.append(a.normalized() if a.length > 1e-6 else Vector((0, 0, -1)))
        if Vector(flow[k]).dot(away[k]) < 0:
            flow[k] = -flow[k]
    vertex_faces = {}
    for p in mesh.polygons:
        for v in p.vertices:
            vertex_faces.setdefault(v, []).append(p.index)
    by_vert = {}
    for k, f in enumerate(faces):
        for v in mesh.polygons[f].vertices:
            by_vert.setdefault(v, []).append(k)
    neighbours = [set() for _ in faces]
    for ks in by_vert.values():
        for k in ks:
            neighbours[k].update(j for j in ks if j != k)
    field = [Vector(flow[k]) * float(clear[k]) + away[k] * 0.15 for k in range(len(faces))]
    for _ in range(12):
        new = []
        for k in range(len(faces)):
            acc = Vector(field[k])
            for j in neighbours[k]:
                acc += field[j] if field[j].dot(field[k]) >= 0 else -field[j]
            acc -= normals[k] * acc.dot(normals[k])
            new.append(acc.normalized() if acc.length > 1e-9 else away[k])
        field = new

    # Roots stay ROOT_INSET face rings inside the hair, so no card starts at its edge and hangs over an ear or the brow.
    inner = np.ones(len(faces), dtype=bool)
    hair_faces = set(int(f) for f in faces)
    for k, f in enumerate(faces):
        for v in mesh.polygons[f].vertices:
            if any(g not in hair_faces for g in vertex_faces[v]):
                inner[k] = False
    for _ in range(ROOT_INSET - 1):
        inner = inner & np.array([all(inner[j] for j in neighbours[k]) for k in range(len(faces))])
    rooted = faces[inner]
    tree = BVHTree.FromPolygons([v.co for v in mesh.vertices], [p.vertices[:] for p in mesh.polygons])
    rng = np.random.default_rng(7)
    areas = np.array([mesh.polygons[f].area for f in rooted])
    verts, shell_normals, quads, uvs = [], [], [], []
    for f in rng.choice(rooted, CARDS, p=areas / areas.sum()):
        p = mesh.polygons[int(f)]
        corners = [mesh.vertices[v].co for v in p.vertices]
        weights = rng.dirichlet(np.ones(len(corners)))
        point = sum((co * float(wk) for co, wk in zip(corners, weights)), Vector())
        length = rng.uniform(*CARD_LENGTH)
        path, normals_along = [point], [p.normal.copy()]
        while len(path) * CARD_STEP < length:
            at = tree.find_nearest(path[-1])
            if at[2] is None or at[2] not in index:
                break
            nxt = tree.find_nearest(path[-1] + field[index[at[2]]] * CARD_STEP)
            if nxt[0] is None or nxt[2] not in index:
                break
            path.append(nxt[0])
            normals_along.append(nxt[1])
        if len(path) < 4:
            continue
        width = rng.uniform(*CARD_WIDTH)
        lift = CARD_LIFT[0] + rng.uniform(0.0, 0.002)
        tip_lift = rng.uniform(CARD_LIFT[0], CARD_LIFT[1]) * (2.5 if rng.random() < 0.08 else 1.0)  # a few fly away
        strip = int(rng.integers(STRIPS))
        u0, u1 = strip / STRIPS, (strip + 1) / STRIPS
        base, n = len(verts), len(path)
        for i in range(n):
            t = i / (n - 1)
            along = (path[min(i + 1, n - 1)] - path[max(i - 1, 0)]).normalized()
            across = normals_along[i].cross(along).normalized()
            centre = path[i] + normals_along[i] * (lift + (tip_lift - lift) * t * t)
            half = width * (1.0 - 0.6 * t) / 2
            verts += [centre - across * half, centre + across * half]
            shell_normals += [normals_along[i]] * 2
            if i:
                k = base + 2 * i
                quads.append((k - 2, k, k + 1, k - 1))  # wound to face out (two-sided shading flips a card seen from behind)
                # The strips fill the atlas's bottom half, root at v 0; kept off its top row (the hero's texture is next).
                v_prev, v_now = (i - 1) / (n - 1) * 0.49, t * 0.49
                uvs.append([(u0, v_prev), (u0, v_now), (u1, v_now), (u1, v_prev)])
    return verts, shell_normals, quads, uvs, np.median(colours[hair], 0)


def strand_strips(colour, height, width):
    """The cards' texture: STRIPS columns of strands running up from the root (row 0); alpha = strand coverage; the colour
    behind them is the hair's, so mips and filtering don't fringe."""
    rng = np.random.default_rng(11)
    rgb = np.empty((height, width, 3), dtype=np.float32)
    rgb[:] = colour
    alpha = np.zeros((height, width), dtype=np.float32)
    shade = np.ones((height, width), dtype=np.float32)
    column = width // STRIPS
    rows = np.arange(height)
    for strip in range(STRIPS):
        x_lo, x_hi = strip * column, (strip + 1) * column - 1
        for _ in range(STRANDS):
            end = int(height * rng.uniform(0.5, 1.0))
            start = int(height * rng.uniform(0.0, 0.12))  # ragged roots too
            r = rows[start:end]
            t = r / height
            centre = (x_lo + np.clip(rng.normal(0.5, 0.17), 0.08, 0.92) * column + rng.uniform(-0.1, 0.1) * column * t
                      + rng.uniform(1, 6) * np.sin(2 * np.pi * (t * rng.uniform(0.6, 2.0) + rng.random())))
            half = rng.uniform(0.9, 1.6) * (1.0 - 0.7 * (r - start) / (end - start))  # px
            tone = rng.uniform(0.75, 1.35)
            lo = np.floor(centre).astype(int) - 3
            for dx in range(8):
                x = np.clip(lo + dx, x_lo, x_hi)
                a = np.clip(1.0 - np.abs(x - centre) / (half + 0.5), 0.0, 1.0) * np.clip((r - start) / (0.04 * height), 0.0, 1.0)
                take = a > alpha[r, x]
                shade[r[take], x[take]] = tone
                alpha[r, x] = np.maximum(alpha[r, x], a)
    rgb *= shade[..., None]
    rgb *= (1.0 + 0.25 * np.exp(-((rows / height - 0.45) / 0.12) ** 2))[:, None, None]  # a soft sheen band
    return np.concatenate([np.clip(rgb, 0, 1), alpha[..., None]], axis=2)


card_verts, card_normals, card_quads, card_uvs, hair_colour = grow_cards(head, head_tex)
cards_mesh = bpy.data.meshes.new("HairCards")
cards_mesh.from_pydata([tuple(v) for v in card_verts], [], card_quads)
cards_mesh.uv_layers.new(name=head.data.uv_layers.active.name)
cards_mesh.uv_layers.active.data.foreach_set("uv", np.array(card_uvs, dtype=np.float32).ravel())
# Each card is lit like the hair under it (its tilt off the shell otherwise reads as a dark flake): the shell's normals.
cards_mesh.normals_split_custom_set_from_vertices([tuple(n) for n in card_normals])
cards = bpy.data.objects.new("HairCards", cards_mesh)
bpy.context.scene.collection.objects.link(cards)
log(f"hair: {len(card_quads)} card faces, hair colour {np.round(hair_colour * 255)}")


def render(path, centre, half_size, views=(("front", (0, -1, 0)), ("side", (1, 0, 0))), size=900):
    """Orthographic Eevee renders (the hair cards need their alpha), grey sky and a sun: <path>_<view>.png."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.world = scene.world or bpy.data.worlds.new("Grey")
    scene.world.color = (0.5, 0.5, 0.5)
    if "PreviewSun" not in bpy.data.objects:
        sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", "SUN"))
        sun.data.energy = 3.0
        sun.rotation_euler = (0.8, 0.0, 0.6)
        scene.collection.objects.link(sun)
    cam = bpy.data.objects.get("PreviewCam") or bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2 * half_size
    for name, direction in views:
        d = Vector(direction)
        cam.location = Vector(centre) + d * 3
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{path}_{name}.png"
        bpy.ops.render.render(write_still=True)


# --- 3. Weights: the hero's, nearest surface ------------------------------------------------------------------------------
for obj in (head, cards):
    select(obj)
    transfer = obj.modifiers.new("weights", "DATA_TRANSFER")
    transfer.object = source
    transfer.use_vert_data = True
    transfer.data_types_verts = {"VGROUP_WEIGHTS"}
    transfer.vert_mapping = "POLYINTERP_NEAREST"
    transfer.layers_vgroup_select_src = "ALL"
    transfer.layers_vgroup_select_dst = "NAME"
    bpy.ops.object.datalayout_transfer(modifier=transfer.name)
    bpy.ops.object.modifier_apply(modifier=transfer.name)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
share = head_share(head)
log(f"head weights: {len(head.vertex_groups)} bones, {np.mean(share > 0.99):.0%} of the verts all on the head")

# --- 5. One texture: hero | head, the hero's skin in the head's tone -------------------------------------------------------
face_rgb = face_colours(head.data, head_tex)
head_centres = world_centres(head)
cheeks = (head_centres[:, 1] < -0.05) & (head_centres[:, 2] > 1.56) & (head_centres[:, 2] < 1.63) & is_skin(face_rgb)
head_skin = np.median(face_rgb[cheeks], 0)
# Tripo's skin comes out a little pinker than the photos: the head's hue goes HEAD_TONE of the way to theirs (in multiples
# at the same brightness: the photos' shade and stubble make them darker overall; the hair is dark enough not to change).
photo = image_array(bpy.data.images.load(PHOTO))[..., :3]
photo_skin = np.median(photo[is_skin(photo)], 0)
tone = ((photo_skin / photo_skin.mean()) / (head_skin / head_skin.mean())) ** HEAD_TONE
head_tex[..., :3] = np.clip(head_tex[..., :3] * tone, 0, 1)
log(f"head: skin {np.round(head_skin * 255)}, photo {np.round(photo_skin * 255)} -> x {np.round(tone, 3)}")
# Tripo's hair has a purple cast; the photo's is a neutral near-black. Its dark texels (by how dark) get the photo's hue.
photo_hair = np.median(photo[photo.max(2) < 0.22], 0)
hair_tone = (photo_hair / photo_hair.mean()) / (hair_colour / hair_colour.mean())
darkness = np.clip((0.3 - head_tex[..., :3].mean(2)) / 0.12, 0, 1)[..., None]
head_tex[..., :3] = np.clip(head_tex[..., :3] * (1 + darkness * (hair_tone - 1)), 0, 1)
hair_colour = hair_colour * tone * hair_tone
log(f"hair: {np.round(photo_hair * 255)} in the photo -> x {np.round(hair_tone, 3)}")
head_skin = head_skin * tone
hero_skin_px = is_skin(hero_tex[..., :3])
hero_skin = np.median(hero_tex[..., :3][hero_skin_px], 0)
gain = head_skin / hero_skin
hero_tex[..., :3][hero_skin_px] = np.clip(hero_tex[..., :3][hero_skin_px] * gain, 0, 1)
log(f"skin: head {np.round(head_skin * 255)}, hero {np.round(hero_skin * 255)} -> hands and feet x {np.round(gain, 2)}")

ATLAS_W, ATLAS_H = 2 * TILE, (2 if CARDS else 1) * TILE
top = ATLAS_H - TILE  # first row of the hero | head row
atlas = np.ones((ATLAS_H, ATLAS_W, 4), dtype=np.float32)  # row 0 is v = 0 (the bottom)
atlas[top:, :TILE] = hero_tex
h, w = head_tex.shape[:2]
atlas[top:, TILE:] = head_tex.reshape(TILE, h // TILE, TILE, w // TILE, 4).mean((1, 3))
atlas[top:, :, 3] = 1.0
if CARDS:
    atlas[:top] = strand_strips(hair_colour, top, ATLAS_W)
image = bpy.data.images.new(f"{NAME}_rigged", ATLAS_W, ATLAS_H)
image.pixels.foreach_set(atlas.ravel())
image.filepath_raw = os.path.join(OUT, f"{NAME}_rigged.png")
image.file_format = "PNG"
image.save()
for obj, u0 in ((body, 0.0), (head, 0.5)):  # (the cards' UVs are already in the atlas)
    uv = np.empty(len(obj.data.loops) * 2, dtype=np.float32)
    obj.data.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    uv[:, 0] = u0 + np.clip(uv[:, 0], 0, 1) * 0.5
    uv[:, 1] = (top + np.clip(uv[:, 1], 0, 1) * TILE) / ATLAS_H
    obj.data.uv_layers.active.data.foreach_set("uv", uv.ravel())

material = bpy.data.materials.new(NAME)
material.use_nodes = True
tex_node = material.node_tree.nodes.new("ShaderNodeTexImage")
tex_node.image = image
material.node_tree.links.new(tex_node.outputs["Color"], material.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
for obj in (body, head, cards):
    obj.data.materials.clear()
    obj.data.materials.append(material)

# --- 6. One mesh, exported -----------------------------------------------------------------------------------------------
bpy.data.objects.remove(source)
select(body, head, cards)
bpy.ops.object.join()
body.name = body.data.name = NAME
bpy.ops.object.shade_smooth()
export_unit_scale_fbx(os.path.join(OUT, f"{NAME}_rigged.fbx"), [armature, body])
log(f"wrote {NAME}_rigged.fbx ({len(body.data.polygons)} faces) and {NAME}_rigged.png ({ATLAS_W} x {ATLAS_H})")

bpy.ops.wm.read_factory_settings(use_empty=True)  # previews of what was written, as Unreal will get it
bpy.ops.import_scene.fbx(filepath=os.path.join(OUT, f"{NAME}_rigged.fbx"))
look = bpy.data.materials.new("Look")
look.use_nodes = True
tex_node = look.node_tree.nodes.new("ShaderNodeTexImage")
tex_node.image = bpy.data.images.load(os.path.join(OUT, f"{NAME}_rigged.png"))
look.node_tree.links.new(tex_node.outputs["Color"], look.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
look.node_tree.links.new(tex_node.outputs["Alpha"], look.node_tree.nodes["Principled BSDF"].inputs["Alpha"])
look.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.8  # M_Character's
for obj in bpy.data.objects:
    if obj.type == "MESH":
        obj.data.materials.clear()
        obj.data.materials.append(look)
render(os.path.join(OUT, "preview"), (0, 0, 0.92), 0.95, size=1024)

if os.environ.get("HEAD_DEBUG"):
    render(os.environ["HEAD_DEBUG"], (0, 0, 1.58), 0.25, (("front", (0, -1, 0)), ("side", (1, 0, 0)), ("back", (0, 1, 0)), ("top", (0.0, 0.5, 1.0))))
    bpy.ops.wm.save_as_mainfile(filepath=os.environ["HEAD_DEBUG"] + ".blend")
