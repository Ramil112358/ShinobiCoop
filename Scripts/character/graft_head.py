"""Blender (5.2) headless: a roster character's head grafted onto the MetaHuman body as a static (rig-less) head.

  blender -b --factory-startup -P Scripts/character/graft_head.py -- <mh_body.fbx> <mh_face.fbx> <donor_rigged.fbx>
      <out.fbx> [--join <z m>] [--scale auto|<s>] [--texture <donor.png>] [--body-texture <mh_body_bc.png>] [--render <dir>]

The MetaHuman body ends at a wide open seam at the base of the neck (its face mesh carries the neck and trapezius), so
the head is two pieces welded at a horizontal join (--join; default the lowest height where the
donor's neck still closes a ring, between the body seam and the head bone, above any stretched texture: find_join):
  - below: the MetaHuman face's own neck skin, which meets the body seam exactly, in its own material slot with a
    texture <out>_neck.png baked round the neck from the donor's neck skin (--texture: the donor's base colour, default
    <donor>.png), shading down to the body's own skin colour at the seam (--body-texture: the MetaHuman body's base
    colour); the donor's throat below where its texture is usable (skin_start) takes that slot too;
  - above: the donor's head and neck (plus ears and hair cards: every piece reaching up to the head), scaled by stature
    (Head bone heights; --scale overrides) with its Head bone pinned on the MetaHuman's; its bottom JOIN_BAND is bent
    radially onto the MetaHuman neck's outline, with a short lip tucked inside it.
Weights: the neck keeps the face's (facial joints folded onto their nearest body-bone ancestor); the donor goes
neck_02 -> head by height, taking the neck's weights near the join so both sides move together. Output: the head alone on
the MetaHuman skeleton (unit-scale bones, clean_rig.export_unit_scale_fbx), worn in game with the body as leader pose.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from clean_rig import export_unit_scale_fbx  # noqa: E402

JOIN_BAND = 0.045  # m of donor neck above the join bent onto the MetaHuman neck's outline
LIP = 0.005  # m the donor neck reaches down inside the MetaHuman neck
SCALE_RANGE = (0.8, 1.25)  # the auto stature scale is clamped to this


def log(message):
    print(f"[graft_head] {message}")


def import_rig(path, tag):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for armature in (o for o in new if o.type == "ARMATURE" and o.parent and o.parent.type == "EMPTY"):
        world = armature.matrix_world.copy()
        empty = armature.parent
        armature.parent = None
        armature.matrix_world = world
        bpy.data.objects.remove(empty, do_unlink=True)
        new.remove(empty)
    armature = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    for o in meshes:  # (armatures keep their names: the exported rig's object name becomes its root bone)
        o.name = f"{tag}_{o.name}"
    return armature, meshes


def merge_meshes(meshes):
    """One mesh, unparented, with its world transform baked into the vertices."""
    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = meshes[0]
    world = obj.matrix_world.copy()
    obj.parent = None
    obj.data.transform(world)
    obj.matrix_world.identity()
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    return obj


def bone_head(armature, suffix):
    bone = next(b for b in armature.data.bones if b.name.split(":")[-1] == suffix)
    return np.array(tuple(armature.matrix_world @ bone.head_local))


def islands(bm):
    seen, out = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, island = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            island.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        out.append(island)
    return out


def cut(obj, z, keep_above, transform=None, keep=None):
    """Bisects obj at height z keeping one side; transform (a (N,3)->(N,3) function) is applied first; keep(bm, faces)
    filters what remains."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    if transform is not None:
        co = transform(np.array([tuple(v.co) for v in bm.verts]))
        for v, p in zip(bm.verts, co):
            v.co = p
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z), plane_no=(0, 0, 1),
                           clear_inner=keep_above, clear_outer=not keep_above)
    if keep is not None:
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep(f)], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def main_island(bm, z, head_top, neck_top):
    """The head-and-neck skin: the biggest piece spanning from under neck_top up past head_top (z: each vertex's placed
    height). A body the donor head sat on stays below the head, a hair shell starts above the neck."""
    def spans(island):
        heights = [z[v.index] for f in island for v in f.verts]
        return min(heights) < neck_top and max(heights) > head_top
    return max((i for i in islands(bm) if spans(i)), key=len)


def skin_start(head, place, head_top, neck_top, axis):
    """Where the donor's neck texture becomes usable, going up: the top of the 1 cm bands near the neck whose texel
    density (UV area per surface area, median) is under half the head's (build_ramil.py drew Ramil's neck down into a
    collar with collapsed UVs). None when nothing is stretched."""
    bm = bmesh.new()
    bm.from_mesh(head.data)
    co = place(np.array([tuple(v.co) for v in bm.verts]))
    uv_layer = bm.loops.layers.uv.active
    density = []
    for f in main_island(bm, co[:, 2], head_top, neck_top):
        c = co[[v.index for v in f.verts]]
        if c[:, 2].mean() > head_top or np.hypot(*(c[:, :2].mean(axis=0) - axis)) > 0.09:
            continue
        area = np.linalg.norm(np.cross(c[1] - c[0], c[2] - c[0])) / 2
        uv = np.array([tuple(l[uv_layer].uv) for l in f.loops])
        if area > 1e-9:
            uv_area = abs((uv[1, 0] - uv[0, 0]) * (uv[2, 1] - uv[0, 1]) - (uv[1, 1] - uv[0, 1]) * (uv[2, 0] - uv[0, 0])) / 2
            density.append((c[:, 2].mean(), uv_area / area))
    bm.free()
    if not density:
        return None
    density = np.array(density)
    above = density[:, 0] > neck_top
    typical = np.median(density[above, 1] if above.any() else density[:, 1])
    bands = np.floor(density[:, 0] / 0.01)
    stretched = [b for b in np.unique(bands) if np.median(density[bands == b, 1]) < 0.5 * typical]
    return (max(stretched) + 1) * 0.01 + 0.002 if stretched else None


def find_join(head, place, head_top, neck_top, z_lo, z_hi, axis, step=0.005):
    """The lowest height in [z_lo, z_hi] where the donor's head-and-neck piece still closes a ring round the neck (its
    edges crossing that height cover >= 345 degrees about the axis, all within 9 cm of it): there the donor's own neck
    can meet the MetaHuman's, so as little of the MetaHuman neck shows as possible."""
    bm = bmesh.new()
    bm.from_mesh(head.data)
    co = place(np.array([tuple(v.co) for v in bm.verts]))
    main = main_island(bm, co[:, 2], head_top, neck_top)
    edges = np.array(sorted({tuple(sorted((v.index for v in e.verts))) for f in main for e in f.edges}))
    bm.free()
    a, b = co[edges[:, 0]], co[edges[:, 1]]
    for z in np.arange(z_lo, z_hi + 1e-6, step):
        crossing = (a[:, 2] - z) * (b[:, 2] - z) < 0
        if crossing.sum() < 8:
            continue
        t = ((z - a[crossing, 2]) / (b[crossing, 2] - a[crossing, 2]))[:, None]
        p = a[crossing] + t * (b[crossing] - a[crossing])
        r = np.hypot(p[:, 0] - axis[0], p[:, 1] - axis[1])
        if r.max() > 0.09:
            continue
        theta = np.sort(np.arctan2(p[:, 1] - axis[1], p[:, 0] - axis[0]))
        if angular_coverage(theta) >= 345.0:
            return float(z)
    return float(z_hi)


def cut_head(head, z, place, head_top, neck_top):
    """The donor placed, its head-and-neck piece cut at z keeping what's above; other pieces (hair cards, ears) stay
    whole unless they end below the head (collar scraps, specks), so hair isn't sliced flat at the nape."""
    bm = bmesh.new()
    bm.from_mesh(head.data)
    co = place(np.array([tuple(v.co) for v in bm.verts]))
    for v, p in zip(bm.verts, co):
        v.co = p
    main = main_island(bm, co[:, 2], head_top, neck_top)
    others = [i for i in islands(bm) if i is not main]
    stray = [f for island in others if max(v.co.z for f in island for v in f.verts) < head_top for f in island]
    main_geom = list({v for f in main for v in f.verts}) + list({e for f in main for e in f.edges}) + main
    bmesh.ops.delete(bm, geom=stray, context="FACES")
    bmesh.ops.bisect_plane(bm, geom=[g for g in main_geom if g.is_valid], plane_co=(0, 0, z), plane_no=(0, 0, 1),
                           clear_inner=True)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(head.data)
    bm.free()
    head.data.update()


def boundary_ring(obj, z, axis, tolerance=0.002):
    """The longest boundary loop at height z whose centre is on the neck axis: (vertex indices, positions)."""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    links = {}
    for e in bm.edges:
        if e.is_boundary and all(abs(v.co.z - z) < tolerance for v in e.verts):
            a, b = (v.index for v in e.verts)
            links.setdefault(a, []).append(b)
            links.setdefault(b, []).append(a)
    bm.free()
    loops, seen = [], set()
    for start in links:
        if start in seen:
            continue
        stack, loop = [start], []
        seen.add(start)
        while stack:
            i = stack.pop()
            loop.append(i)
            for j in links[i]:
                if j not in seen:
                    seen.add(j)
                    stack.append(j)
        loops.append(loop)
    co = np.array([tuple(v.co) for v in me.vertices])
    near = [lp for lp in loops if np.hypot(*(co[lp, :2].mean(axis=0) - axis)) < 0.04]
    ring = max(near or loops, key=len)
    return ring, co[ring]


def by_angle(points, axis):
    theta = np.arctan2(points[:, 1] - axis[1], points[:, 0] - axis[0])
    order = np.argsort(theta)
    return theta[order], points[order]


def angular_coverage(theta):
    gaps = np.diff(np.concatenate([theta, [theta[0] + 2 * math.pi]]))
    return 360.0 - math.degrees(gaps.max())


def fold_facial_weights(neck, face_rig, body_bones):
    """Vertex groups for face-only joints (FACIAL_*) moved onto their nearest ancestor the body skeleton has."""
    target = {}
    for bone in face_rig.data.bones:
        b = bone
        while b is not None and b.name not in body_bones:
            b = b.parent
        target[bone.name] = b.name if b is not None else "head"
    weights = [{} for _ in neck.data.vertices]
    names = {g.index: g.name for g in neck.vertex_groups}
    for v in neck.data.vertices:
        for g in v.groups:
            name = target.get(names[g.group], names[g.group])
            weights[v.index][name] = weights[v.index].get(name, 0.0) + g.weight
    neck.vertex_groups.clear()
    groups = {}
    for i, w in enumerate(weights):
        for name, value in w.items():
            if value > 1e-4:
                if name not in groups:
                    groups[name] = neck.vertex_groups.new(name=name)
                groups[name].add([i], value, "REPLACE")
    return weights


NECK_TEXTURE = 512  # px round the neck, the baked neck texture


def image_pixels(path):
    image = bpy.data.images.load(path, check_existing=True)
    w, h = image.size
    px = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(px)
    return px.reshape(h, w, 4)


def drop_dark_faces(obj, png, darkest=0.06):
    """Removes faces whose texture is near black at their UV centre (a shirt collar the neck runs into)."""
    px = image_pixels(png)
    h, w = px.shape[:2]
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    layer = bm.loops.layers.uv.active
    dark = []
    for f in bm.faces:
        u = sum(l[layer].uv.x for l in f.loops) / len(f.loops)
        v = sum(l[layer].uv.y for l in f.loops) / len(f.loops)
        r, g, b, _ = px[int(min(max(v, 0), 1) * (h - 1)), int(min(max(u, 0), 1) * (w - 1))]
        if 0.2126 * r + 0.7152 * g + 0.0722 * b < darkest:
            dark.append(f)
    bmesh.ops.delete(bm, geom=dark, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()


def seam_outline(body, png, centre, width):
    """The body's open neck seam as (heights, colours) at width angles round centre (u = angle as in cylindrical_uvs):
    each angle takes the nearest seam vertex's height and its body base colour (png; MetaHuman body UVs sit in UDIM
    tile 1002, so u wraps). Without png the colours are None."""
    me = body.data
    world = body.matrix_world
    top = max((world @ v.co).z for v in me.vertices)
    bm = bmesh.new()
    bm.from_mesh(me)
    ring = sorted({v.index for e in bm.edges if e.is_boundary for v in e.verts if (world @ v.co).z > top - 0.1})
    bm.free()
    points = np.array([tuple(world @ me.vertices[i].co) for i in ring])
    theta = np.arctan2(points[:, 1] - centre[1], points[:, 0] - centre[0])
    angles = (np.arange(width) + 0.5) / width * 2 * math.pi - math.pi
    gap = np.abs(angles[:, None] - theta[None, :])
    nearest = np.argmin(np.minimum(gap, 2 * math.pi - gap), axis=1)
    heights = points[nearest, 2]
    if not png:
        return heights, None
    px = image_pixels(png)
    h, w = px.shape[:2]
    uv = me.uv_layers.active.data
    vertex_uv = {}
    for loop in me.loops:
        vertex_uv.setdefault(loop.vertex_index, tuple(uv[loop.index].uv))
    colours = np.array([px[int((vertex_uv[ring[i]][1] % 1.0) * (h - 1)), int((vertex_uv[ring[i]][0] % 1.0) * (w - 1))]
                        for i in nearest])
    k = 6
    colours = sum(np.roll(colours, d, 0) for d in range(-k, k + 1)) / (2 * k + 1)
    return heights, colours


def face_tone(head, png, z_lo, z_hi):
    """The donor's skin tone: median colour of its front-facing (-Y) skin between z_lo and z_hi (cheeks and jaw)."""
    px = image_pixels(png)
    h, w = px.shape[:2]
    me = head.data
    uv = me.uv_layers.active.data
    colours = []
    for poly in me.polygons:
        if poly.normal.y > -0.5 or not z_lo < poly.center.z < z_hi:
            continue
        u = sum(uv[li].uv.x for li in poly.loop_indices) / poly.loop_total
        v = sum(uv[li].uv.y for li in poly.loop_indices) / poly.loop_total
        c = px[int(min(max(v, 0), 1) * (h - 1)), int(min(max(u, 0), 1) * (w - 1))]
        if 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2] > 0.1 and c[0] > c[2] + 0.03:
            colours.append(c)
    return np.median(np.array(colours), axis=0) if colours else None


def bake_neck(samples, donor_png, out_png, centre, z_skin, tone=None, seam_colours=None):
    """The neck texture, laid out round the neck (cylindrical_uvs: u = angle, v = height): each column takes the colour of
    the donor's neck just above z_skin at that angle (the nearest point on its usable skin seen from 20 cm out), blurred
    round the neck, so the neck's skin tone carries straight down to the body with no stretched or projected streaks."""
    from mathutils.bvhtree import BVHTree
    from mathutils.geometry import barycentric_transform
    donor_px = image_pixels(donor_png)
    dh, dw = donor_px.shape[:2]
    sm = samples.data
    tree = BVHTree.FromPolygons([tuple(v.co) for v in sm.vertices], [tuple(p.vertices) for p in sm.polygons])
    s_uv = sm.uv_layers.active.data
    width, height = NECK_TEXTURE, 32
    column = np.zeros((width, 4), dtype=np.float32)
    for i in range(width):
        theta = (i + 0.5) / width * 2 * math.pi - math.pi
        eye = Vector((centre[0] + 0.2 * math.cos(theta), centre[1] + 0.2 * math.sin(theta), z_skin + 0.008))
        hit, _, face, _ = tree.find_nearest(eye)
        poly = sm.polygons[face]
        corners = [Vector(tuple(sm.vertices[v].co)) for v in poly.vertices[:3]]
        corner_uv = [Vector((*s_uv[li].uv, 0)) for li in poly.loop_indices[:3]]
        duv = barycentric_transform(hit, *corners, *corner_uv)
        column[i] = donor_px[int(min(max(duv.y, 0.0), 1.0) * (dh - 1)), int(min(max(duv.x, 0.0), 1.0) * (dw - 1))]
    # hair painted on the nape (or a hair shell) isn't neck skin: those angles take the nearest skin angle's colour
    r, g, b = column[:, 0], column[:, 1], column[:, 2]
    skin = (0.2126 * r + 0.7152 * g + 0.0722 * b > 0.1) & (r > b + 0.03)
    if skin.any():
        index = np.arange(width)
        good = index[skin]
        gap = np.abs(index[:, None] - good[None, :])
        nearest = good[np.argmin(np.minimum(gap, width - gap), axis=1)]
        column = column[nearest]
    if tone is not None:  # the throat under the jaw is shaded and stubbly: lift it halfway to the face's tone
        column = 0.5 * column + 0.5 * tone
    log(f"neck texture: {skin.mean() * 100:.0f}% of the way round sampled skin")
    k = 6  # blur round the neck (np.roll wraps, so the back seam matches)
    column = sum(np.roll(column, d, 0) for d in range(-k, k + 1)) / (2 * k + 1)
    out = np.repeat(column[None, :, :], height, axis=0)
    if seam_colours is not None:  # from the body's skin at the seam (v 0) up to the donor's neck (v 1)
        v = np.linspace(0.0, 1.0, height)[:, None, None]
        v = v * v * (3 - 2 * v)
        out = (1 - v) * seam_colours[None, :, :] + v * column[None, :, :]
    out[..., 3] = 1.0
    image = bpy.data.images.new("neck_bake", width, height, alpha=True)
    image.pixels.foreach_set(out.astype(np.float32).ravel())
    image.filepath_raw = out_png
    image.file_format = "PNG"
    image.save()
    log(f"neck texture -> {out_png}")


def cylindrical_uvs(mesh, faces, centre, seam_heights, z_max, layer):
    """UVs round the neck for faces: u = angle about centre, v = height from the body seam at that angle (seam_heights,
    one per texture column) up to z_max; a face across the back seam keeps one side."""
    uv = mesh.uv_layers[layer].data
    for f in faces:
        poly = mesh.polygons[f]
        us, vs = [], []
        for li in poly.loop_indices:
            p = mesh.vertices[mesh.loops[li].vertex_index].co
            u = (math.atan2(p.y - centre[1], p.x - centre[0]) + math.pi) / (2 * math.pi)
            z_min = seam_heights[min(int(u * len(seam_heights)), len(seam_heights) - 1)]
            us.append(u)
            vs.append(min(max((p.z - z_min) / max(z_max - z_min, 1e-4), 0.0), 1.0))
        if max(us) - min(us) > 0.5:
            us = [u + 1.0 if u < 0.5 else u for u in us]
        for li, u, v in zip(poly.loop_indices, us, vs):
            uv[li].uv = (u, v)


def preview_material(material, png):
    nodes = material.node_tree.nodes
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(png, check_existing=True)
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    material.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])


def main():
    cli = sys.argv[sys.argv.index("--") + 1:]
    body_fbx, face_fbx, donor_fbx, out = (os.path.abspath(p) for p in cli[:4])
    opts = dict(zip(cli[4::2], cli[5::2]))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rig, body_meshes = import_rig(body_fbx, "MH")
    body = merge_meshes(body_meshes)
    face_rig, face_meshes = import_rig(face_fbx, "Face")
    neck = merge_meshes(face_meshes)
    donor_rig, donor_meshes = import_rig(donor_fbx, "Donor")
    head = merge_meshes(donor_meshes)

    mh_head, mh_neck2 = bone_head(rig, "head"), bone_head(rig, "neck_02")
    axis = bone_head(rig, "neck_01")[:2]
    d_head = bone_head(donor_rig, "Head")
    scale = float(np.clip(mh_head[2] / d_head[2], *SCALE_RANGE)) if opts.get("--scale", "auto") == "auto" \
        else float(opts["--scale"])
    place = lambda co: mh_head + scale * (co - d_head)
    seam_top = max((body.matrix_world @ v.co).z for v in body.data.vertices)
    z_join = float(opts["--join"]) if "--join" in opts else find_join(head, place, mh_head[2], mh_neck2[2], seam_top + 0.012, mh_neck2[2], axis)
    z_skin = max(z_join, skin_start(head, place, mh_head[2], mh_neck2[2], axis) or z_join)
    log(f"join at z {z_join:.3f}, donor skin texture usable from {z_skin:.3f} (body seam top {seam_top:.3f}, "
        f"neck_02 {mh_neck2[2]:.3f})")

    # the MetaHuman neck: the face's skin section (its biggest) below the join
    skin = max(range(len(neck.data.materials)), key=lambda i: sum(p.material_index == i for p in neck.data.polygons))
    cut(neck, z_join, keep_above=False, keep=lambda f: f.material_index == skin)
    # donor UV samples from its usable neck skin just above z_skin, for the neck bake
    samples = head.copy()
    samples.data = head.data.copy()
    bpy.context.scene.collection.objects.link(samples)
    cut(samples, z_skin + 0.02, keep_above=False, transform=place)
    cut(samples, z_skin, keep_above=True)
    drop_dark_faces(samples, os.path.abspath(opts.get("--texture") or os.path.splitext(donor_fbx)[0] + ".png"))
    # the donor head: what's above the join and reaches up to the head (drops collar scraps and specks)
    cut_head(head, z_join, place, mh_head[2], mh_neck2[2])

    # bend the donor's bottom band onto the MetaHuman neck's outline at the join
    ring, ring_co = boundary_ring(head, z_join, axis)
    _, neck_ring_co = boundary_ring(neck, z_join, axis)
    # first slide the band so the two rings share a centre, then match their outlines about it
    centre = neck_ring_co[:, :2].mean(axis=0)
    co = np.array([tuple(v.co) for v in head.data.vertices])
    t = np.clip(1 - (co[:, 2] - z_join) / JOIN_BAND, 0, 1)
    t = t * t * (3 - 2 * t)
    co[:, :2] += t[:, None] * (centre - ring_co[:, :2].mean(axis=0))
    ring_co = co[ring]
    r_theta, r_pts = by_angle(ring_co, centre)
    n_theta, n_pts = by_angle(neck_ring_co, centre)
    log(f"join z {z_join:.3f}: donor ring {len(ring)} verts covering {angular_coverage(r_theta):.0f} deg, "
        f"MetaHuman neck ring {len(n_pts)} verts covering {angular_coverage(n_theta):.0f} deg")
    radius = lambda th, t, p: np.interp(th, t, np.hypot(p[:, 0] - centre[0], p[:, 1] - centre[1]), period=2 * math.pi)
    theta = np.arctan2(co[:, 1] - centre[1], co[:, 0] - centre[0])
    factor = 1 + t * (radius(theta, n_theta, n_pts) / np.maximum(radius(theta, r_theta, r_pts), 1e-4) - 1)
    co[:, :2] = centre + (co[:, :2] - centre) * factor[:, None]
    for v, p in zip(head.data.vertices, co):
        v.co = p
    bm = bmesh.new()
    bm.from_mesh(head.data)
    ring_set = set(ring)
    made = bmesh.ops.extrude_edge_only(bm, edges=[e for e in bm.edges if e.is_boundary and
                                                  all(v.index in ring_set for v in e.verts)])["geom"]
    for v in (g for g in made if isinstance(g, bmesh.types.BMVert)):
        inward = Vector((axis[0] - v.co.x, axis[1] - v.co.y, 0)).normalized()
        v.co += Vector((0, 0, -LIP)) + inward * 0.002
    bm.to_mesh(head.data)
    bm.free()

    neck_png = os.path.splitext(os.path.abspath(out))[0] + "_neck.png"
    donor_png = os.path.abspath(opts.get("--texture") or os.path.splitext(donor_fbx)[0] + ".png")
    seam_heights, seam_colours = seam_outline(body, opts.get("--body-texture"), centre, NECK_TEXTURE)
    bake_neck(samples, donor_png, neck_png, centre, z_skin, face_tone(head, donor_png, z_skin + 0.03, z_skin + 0.12),
              seam_colours)
    bpy.data.objects.remove(samples, do_unlink=True)

    # weights
    body_bones = {b.name for b in rig.data.bones}
    neck_weights = fold_facial_weights(neck, face_rig, body_bones)
    neck_ring = [i for i, v in enumerate(neck.data.vertices) if abs(v.co.z - z_join) < 0.002]
    ring_tree = KDTree(len(neck_ring))
    for i in neck_ring:
        ring_tree.insert(neck.data.vertices[i].co, i)
    ring_tree.balance()
    z2 = mh_neck2[2]
    head.vertex_groups.clear()
    groups = {}
    for v in head.data.vertices:
        a = float(np.clip((v.co.z - z2) / (mh_head[2] - z2), 0, 1))
        own = {"neck_02": 1 - a, "head": a}
        band = float(np.clip(1 - (v.co.z - z_join) / JOIN_BAND, 0, 1)) ** 2
        if band > 0:
            _, nearest, _ = ring_tree.find(v.co)
            total = sum(neck_weights[nearest].values()) or 1.0
            own = {k: w * (1 - band) for k, w in own.items()}
            for name, w in neck_weights[nearest].items():
                own[name] = own.get(name, 0.0) + band * w / total
        for name, w in own.items():
            if w > 1e-4:
                if name not in groups:
                    groups[name] = head.vertex_groups.new(name=name)
                groups[name].add([v.index], w, "REPLACE")

    # one mesh, the donor's material(s), on the MetaHuman rig
    # one UV channel shared with the head (UE reads channel 0 for both sections)
    layers = neck.data.uv_layers
    keep = layers.active.name
    for layer in [l for l in layers if l.name != keep]:
        layers.remove(layer)
    layers[0].name = head.data.uv_layers[0].name
    for layer in [l for l in head.data.uv_layers][1:]:
        head.data.uv_layers.remove(layer)
    neck_material = bpy.data.materials.new(f"{os.path.splitext(os.path.basename(out))[0]}_Neck")
    head.data.materials.append(neck_material)
    neck.data.materials.clear()
    for m in head.data.materials:
        neck.data.materials.append(m)
    neck_slot = len(head.data.materials) - 1
    for p in neck.data.polygons:
        p.material_index = neck_slot
    # the donor's neck below z_skin (stretched texture) and the lip take the neck material too
    bm = bmesh.new()
    bm.from_mesh(head.data)
    ring_vert = ring[0]
    neck_piece = next(i for i in islands(bm) if any(v.index == ring_vert for f in i for v in f.verts))
    throat = [f.index for f in neck_piece if sum(v.co.z for v in f.verts) / len(f.verts) < z_skin + 0.003]
    bm.free()
    for f in throat:
        head.data.polygons[f].material_index = neck_slot
    uv_name = head.data.uv_layers[0].name
    cylindrical_uvs(neck.data, range(len(neck.data.polygons)), centre, seam_heights, z_skin, uv_name)
    cylindrical_uvs(head.data, throat, centre, seam_heights, z_skin, uv_name)
    log(f"neck material: {len(neck.data.polygons)} MetaHuman neck faces + {len(throat)} donor throat faces")
    preview_material(neck_material, neck_png)
    bpy.ops.object.select_all(action="DESELECT")
    neck.select_set(True)
    head.select_set(True)
    bpy.context.view_layer.objects.active = head
    bpy.ops.object.join()
    head.parent = rig
    head.matrix_parent_inverse = rig.matrix_world.inverted()
    head.modifiers.new("Armature", "ARMATURE").object = rig
    for o in (donor_rig, face_rig):
        bpy.data.objects.remove(o, do_unlink=True)
    log(f"head: {len(head.data.vertices)} verts, scale {scale:.3f}")
    if "--render" in opts:
        render([head, body], opts["--render"], (axis[0], axis[1], mh_head[2] - 0.02), 0.32)
    bpy.data.objects.remove(body, do_unlink=True)
    head.name = os.path.splitext(os.path.basename(out))[0]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    export_unit_scale_fbx(os.path.abspath(out), [rig, head])
    log(f"wrote {out}")


def render(objects, out_dir, centre, half):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x = scene.render.resolution_y = 900
    camera = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(camera)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 2 * half
    scene.camera = camera
    for o in scene.objects:
        o.hide_render = o.type == "MESH" and o not in objects
    os.makedirs(out_dir, exist_ok=True)
    for name, direction in (("front", (0, -1, 0)), ("side", (1, 0, 0)), ("back", (0, 1, 0)), ("quarter", (0.7, -0.7, 0))):
        camera.location = Vector(centre) + Vector(direction).normalized() * 3
        camera.rotation_euler = (-Vector(direction)).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(out_dir, f"graft_{name}.png")
        bpy.ops.render.render(write_still=True)
    scene.collection.objects.unlink(camera)


if __name__ == "__main__":
    main()
