"""Blender (5.2) headless fitting of an outfit master (a Tripo/Rodin cloak, prep_outfit.py) to one roster body: the copy
that body wears (AShinobiPlayerCharacter::Outfits; docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/skin_outfit.py -- <body_rigged.fbx> <master.glb> <out_dir> <Outfit> <Body>

Steps: import the rigged body (the skeleton and skin weights) and the master -> remember each vertex's master-space
position (UV channels 1 = (x, y) and 2 = (z, 0): Scripts/setup_hero.py's cloth tuning copy matches copies by it) ->
scale and place it on the body (collar at the head bone, hem just above the knees, deep enough front to back for this
body's chest, belly and hips) -> turn each sleeve about its shoulder onto the arm and lift the cloth smoothly off wherever
the body pokes through -> a tailed body (body_landmarks.find_tail): the skirt below where the tail leaves the cloak is
moved in front of the tail and an opening cut there -> copy the body's skin weights (the skirt onto the hips and spine
only, the sleeves onto the arm bones) -> paint the cloth regions into the vertex colours (R = hem, G = sleeve, 0..1; 0 is
pinned to the body), which UShinobiOutfitBuilderLibrary::BuildCloth turns into Max Distance -> <out_dir>/
<Outfit>_<Body>_outfit.fbx (skeleton + copy), <Outfit>_outfit.png (its base-colour texture, the same for every body) and
fit_<Body>_front.png / fit_<Body>_side.png renders of the copy on the body.
Exits with code 1 and a message when a landmark or the bone round trip is wrong.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy
from mathutils import Quaternion, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from body_landmarks import find_tail, outer_layer, skin_depth  # noqa: E402 (needs the path above)

GIRTH = 1.04  # the outfit is cut a little wide so it hangs loose
KNEE_DROP = 0.03  # hem this far below the knee bone
BODY_GIRTH = 0.04  # front to back, the coat is this much deeper than the body at its chest, belly and hips (m)
HEM_TOP = 0.05  # the swinging hem starts this far above the hip joint (m)
SLEEVE_SWING_FROM = 0.4  # the sleeve swings from this share of the shoulder-to-hand distance out to the cuff
TAIL_MARGIN = 0.04  # the tail opening is this much wider than the tail (m)
TAIL_EDGE_SWING = 0.2  # hem share at the opening's edge: it swings less than the skirt round it
UNDERARM_SWING = 0.5  # sleeve share of the cloth under the arm, below the shoulder joint
CAP_INBOARD, CAP_OUT, CAP_LOW, CAP_HIGH = -0.04, 0.10, -0.02, 0.14  # the shoulder cap round the joint (m: out / up)
CAP_NEAR, CAP_FADE = 0.03, 0.03  # ...cloth this near the skin, fading in over this (m)
CAP_CLEARANCE = 0.035  # cloth over the shoulder cap is lifted this far off the skin (m)
TAIL_FRONT = 0.02  # skirt below the opening is kept this far in front of the tail (m)
MAX_INFLUENCES = 4
CLEARANCE = 0.01  # cloth kept this far off the body's skin
MAX_LIFT = 0.1  # deeper than this it's not cloth caught in the body (the collar's inside, a lining): left alone
LIFT_RADIUS, LIFT_PASSES = 0.1, 24  # the lift field's smoothing (m) and most passes (it stops once the body is under LIFT_DONE)
LIFT_DONE = 0.005
SLEEVE_START, SLEEVE_FULL = 0.45, 0.8  # sleeve blend, as shares of the sleeve's angle from straight down at the shoulder
COAT_SEED = 0.15  # cloth hanging this close to straight down from the shoulder (share of the arm's angle) is surely coat
MEMBERSHIP_PASSES = 3000  # relaxation passes spreading the sleeve/coat label along the mesh
SMALL_ISLAND = 50  # loose pieces with fewer vertices than this move as one
LIMB_NEARER = 0.5  # low cloth this much nearer the arm/hand surface than the torso's is sleeve
SHOULDER_RAMP = 0.08  # sleeve share grows in over this far outboard of the shoulder joint (m)
ELBOW_BLEND = 0.06  # sleeve weights hand over from upper arm to forearm this far either side of the elbow (m)
ARMPIT_REACH, ARMPIT_RADIUS = 0.15, 0.05  # the armpit band (m out of / above the shoulder joint) and its smoothing
ARMPIT_FADE_TOP = 0.06  # the smoothing is full from this far below the shoulder joint up, none at the split line (m)
TEAR_DROP = 0.16  # sleeve cloth lower than this below the shoulder joint (m) is split from the coat (not the armpit seam)


def fail(message):
    print(f"[skin_outfit] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[skin_outfit] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 5:
    fail("usage: -- <body_rigged.fbx> <master.glb> <out_dir> <Outfit> <Body>")
body_fbx, outfit_glb, out_dir = (os.path.abspath(a) for a in args[:3])
name, body_name = args[3], args[4]
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=body_fbx)
armature = next((o for o in bpy.context.scene.objects if o.type == "ARMATURE"), None)
body = next((o for o in bpy.context.scene.objects if o.type == "MESH"), None)
if armature is None or body is None:
    fail(f"{body_fbx} needs an armature and a mesh")
known = set(bpy.context.scene.objects)
bpy.ops.import_scene.gltf(filepath=outfit_glb)
new_meshes = [o for o in bpy.context.scene.objects if o not in known and o.type == "MESH"]
if not new_meshes:
    fail(f"no mesh in {outfit_glb}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj in new_meshes)
bpy.context.view_layer.objects.active = new_meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(new_meshes) > 1:
    bpy.ops.object.join()
outfit = bpy.context.view_layer.objects.active
for obj in list(bpy.context.scene.objects):
    if obj not in (armature, body, outfit) and obj.parent is None:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
outfit.name = name
# glTF splits the vertices along every UV seam (a Tripo atlas is hundreds of islands): weld them back, or each island moves
# and shades on its own (faceted patches once the cloth is lifted), and recompute the normals for the moved cloth.
weld = bmesh.new()
weld.from_mesh(outfit.data)
bmesh.ops.remove_doubles(weld, verts=weld.verts, dist=1e-5)
weld.to_mesh(outfit.data)
weld.free()
bpy.ops.mesh.customdata_custom_splitnormals_clear()
bpy.ops.object.shade_smooth()
# Each vertex's place in the master, before fitting: every body's copy carries it, so cloth tuning can be copied between them.
master_xy, master_z = outfit.data.uv_layers.new(name="MasterXY"), outfit.data.uv_layers.new(name="MasterZ")
for loop in outfit.data.loops:
    co = outfit.data.vertices[loop.vertex_index].co
    master_xy.data[loop.index].uv = (co.x, co.y)
    master_z.data[loop.index].uv = (co.z, 0.0)


def bone_head(suffix):
    for bone in armature.data.bones:
        if bone.name.split(":")[-1] == suffix:
            return armature.matrix_world @ bone.head_local
    fail(f"skeleton has no {suffix} bone (is it a Mixamo rig?)")


def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
            Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))))


# Facing: the outfit comes out of glTF facing -Y; turn it when the body's toes point the other way.
foot, toe = bone_head("LeftFoot"), bone_head("LeftToeBase")
if toe.y > foot.y:
    outfit.rotation_euler = (0, 0, math.pi)
    bpy.ops.object.transform_apply(rotation=True)
    log("body faces +Y: turned the outfit around")

head, hips, knee = bone_head("Head"), bone_head("Hips"), bone_head("LeftLeg")
spine = bone_head("Spine2")
centre_x = (bone_head("LeftArm").x + bone_head("RightArm").x) / 2
low, high = bounds(outfit)
target_top, target_hem = head.z, knee.z - KNEE_DROP
scale = (target_top - target_hem) / (high.z - low.z)
outfit.scale = (scale * GIRTH, scale * GIRTH, scale)
bpy.ops.object.transform_apply(scale=True)
low, high = bounds(outfit)
outfit.location = (centre_x - (low.x + high.x) / 2, spine.y - (low.y + high.y) / 2, target_top - high.z)
bpy.ops.object.transform_apply(location=True)
# Deep enough front to back for this body: at its belly and hips the coat (its middle, sleeves left out) is
# BODY_GIRTH deeper than the body's own skin there (skin_depth: long hair down the back stays outside the coat),
# scaling about the coat's own middle.
co = numpy.array([v.co[:] for v in outfit.data.vertices])
middle = numpy.abs(co[:, 0] - centre_x) < 0.12
deepen = 1.0
for height in ((hips.z + bone_head("LeftArm").z) / 2, hips.z):  # (not the chest: a bust is local, the lift covers it)
    band = co[middle & (numpy.abs(co[:, 2] - height) < 0.03)]
    if len(band):
        skin, coat = skin_depth(armature, body, height), band[:, 1].max() - band[:, 1].min()
        deepen = max(deepen, (skin + BODY_GIRTH) / max(coat, 1e-3))
if deepen > 1.0:
    centre_y = (co[middle, 1].max() + co[middle, 1].min()) / 2
    for vertex in outfit.data.vertices:
        vertex.co.y = centre_y + (vertex.co.y - centre_y) * deepen
    log(f"deepened the coat {deepen:.2f}x front to back for {body_name}")
low, high = bounds(outfit)
log(f"outfit {high.z - low.z:.2f} m tall, {high.x - low.x:.2f} m wide, {high.y - low.y:.2f} m deep; collar z {high.z:.2f}, hem z {low.z:.2f}")

# The body as it lies in the file (no armature modifier): the weight source.
for modifier in list(body.modifiers):
    if modifier.type == "ARMATURE":
        body.modifiers.remove(modifier)

def arm_frame(side):
    """The shoulder joint, which way is out (+-1 along x) and the arm's angle out from straight down, seen from the front."""
    shoulder, hand = bone_head(f"{side}Arm"), bone_head(f"{side}Hand")
    return shoulder, (1.0 if hand.x > shoulder.x else -1.0), math.atan2(abs(hand.x - shoulder.x), shoulder.z - hand.z)


def sleeve_shares(shoulder, outward, angle):
    """Per outfit vertex, 0..1: how much it is sleeve. Seen from the front, sleeve cloth lies out along the arm from the
    shoulder joint and coat cloth hangs straight down from it, so the share ramps with the vertex's angle out from straight
    down, over SLEEVE_START..SLEEVE_FULL of the sleeve's own angle. (Outfit transforms are applied: co is world.)"""
    start, full = SLEEVE_START * angle, SLEEVE_FULL * angle
    shares = []
    for vertex in outfit.data.vertices:
        offset = vertex.co - shoulder
        out = offset.x * outward
        # (times a ramp over SHOULDER_RAMP outboard of the joint: cloth just above it is half shoulder, not all arm)
        shares.append(0.0 if out <= 0 else min(max((math.atan2(out, -offset.z) - start) / (full - start), 0.0), 1.0)
                      * min(out / SHOULDER_RAMP, 1.0))
    return shares


def is_arm(group_name):
    return group_name.split(":")[-1] in ("LeftArm", "LeftForeArm", "RightArm", "RightForeArm")


def is_hand(group_name):
    return group_name.split(":")[-1].startswith(("LeftHand", "RightHand"))


def body_part(keep):
    """A copy of the body keeping only the vertices whose strongest group passes keep(name)."""
    part = body.copy()
    part.data = body.data.copy()
    bpy.context.scene.collection.objects.link(part)
    mesh = bmesh.new()
    mesh.from_mesh(part.data)
    mesh.verts.ensure_lookup_table()
    drop = []
    for vertex in part.data.vertices:
        top = max(vertex.groups, key=lambda g: g.weight, default=None)
        if top is None or not keep(part.vertex_groups[top.group].name):
            drop.append(mesh.verts[vertex.index])
    bmesh.ops.delete(mesh, geom=drop, context="VERTS")
    mesh.to_mesh(part.data)
    mesh.free()
    return part

def surface_tree(obj):
    return BVHTree.FromPolygons([obj.matrix_world @ v.co for v in obj.data.vertices], [p.vertices[:] for p in obj.data.polygons])


# The body's arms + hands and the rest of it, as separate surfaces (sleeve_membership asks which one cloth is nearer).
limbs_only = body_part(lambda name: is_arm(name) or is_hand(name))
torso_only = body_part(lambda name: not is_arm(name) and not is_hand(name))
limb_tree, torso_tree = surface_tree(limbs_only), surface_tree(torso_only)
bpy.data.objects.remove(limbs_only, do_unlink=True)


def sleeve_membership(shoulder, outward, angle):
    """Per outfit vertex, 0..1: how much it belongs to the sleeve, by the mesh's own connections. The angle test alone
    (sleeve_shares) half-binds the drape hanging under the sleeve near the armpit to the spine, and a raised arm then
    stretches it down to the waist. Seeded with the sure sleeve (sleeve_shares 1) and the sure coat (hanging within
    COAT_SEED of straight down from the shoulder, or inboard of it), the label spreads along the edges (Jacobi relaxation):
    the drape hangs off the sleeve and meets the coat only at the armpit, so it comes out all sleeve."""
    shares = numpy.array(sleeve_shares(shoulder, outward, angle))
    count = len(outfit.data.vertices)
    co = numpy.empty(count * 3)
    outfit.data.vertices.foreach_get("co", co)
    co = co.reshape(count, 3) - numpy.array(shoulder)
    out = co[:, 0] * outward
    coat = (out <= 0) | (numpy.arctan2(numpy.maximum(out, 0), -co[:, 2]) < COAT_SEED * angle) & (co[:, 2] < 0)
    # Also surely sleeve: cloth well below the armpit much nearer the arm and hand than the torso (the drape's tip, which
    # Rodin welds to the coat where it touches the hip next to the hanging hand).
    low = co[:, 2] < -TEAR_DROP
    near_limb = numpy.array([limb_tree.find_nearest(v.co)[3] if l else 0.0 for v, l in zip(outfit.data.vertices, low)])
    near_torso = numpy.array([torso_tree.find_nearest(v.co)[3] if l else 0.0 for v, l in zip(outfit.data.vertices, low)])
    sleeve = (shares >= 1.0) | (low & (out > 0) & (near_limb < LIMB_NEARER * near_torso))
    coat &= ~sleeve
    edges = numpy.empty(len(outfit.data.edges) * 2, dtype=numpy.int64)
    outfit.data.edges.foreach_get("vertices", edges)
    a, b = edges[0::2], edges[1::2]
    degree = numpy.maximum(numpy.bincount(a, minlength=count) + numpy.bincount(b, minlength=count), 1)
    label = shares.copy()
    for _ in range(MEMBERSHIP_PASSES):
        label = (numpy.bincount(a, weights=label[b], minlength=count) + numpy.bincount(b, weights=label[a], minlength=count)) / degree
        label[coat], label[sleeve] = 0.0, 1.0
    isolated = (numpy.bincount(a, minlength=count) + numpy.bincount(b, minlength=count)) == 0  # loose vertices keep the angle test
    label[isolated] = shares[isolated]
    # Sharpen: mostly-sleeve is sleeve, mostly-coat is coat; only the armpit seam blends.
    label = numpy.clip((label - 0.3) / 0.4, 0.0, 1.0)
    # Round the armpit, the white shell and the red lining aren't joined by edges, so the label can differ between them and a
    # raised arm pulls the lining out through the shell: there, every vertex takes the cone-weighted mean label of the cloth
    # within ARMPIT_RADIUS in space (both layers alike), which also widens the seam so it stretches as a band, not an edge.
    band = numpy.nonzero((out > 0) & (out < ARMPIT_REACH) & (co[:, 2] > -TEAR_DROP) & (co[:, 2] < ARMPIT_REACH))[0]  # (above the split line: below it sleeve and coat are cut apart and must not mix)
    if len(band):
        points = KDTree(count)
        for index, point in enumerate(co):
            points.insert(point, index)
        points.balance()
        smoothed = label.copy()
        for index in band.tolist():
            near = points.find_range(co[index], ARMPIT_RADIUS)
            weights = [1.0 - distance / ARMPIT_RADIUS for _, _, distance in near]
            mean = sum(label[other] * k for (_, other, _), k in zip(near, weights)) / sum(weights)
            # fading in from the split line up to the armpit, so cloth just above the split matches the coat cut below it
            fade = min(max((co[index, 2] + TEAR_DROP) / (TEAR_DROP - ARMPIT_FADE_TOP), 0.0), 1.0)
            smoothed[index] = label[index] + (mean - label[index]) * fade
        label = smoothed
    # Small loose pieces (slivers where the sleeve touches the coat) move as one, at their mean, or a raised arm tears them
    # into strings.
    parent = list(range(count))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, j in zip(a.tolist(), b.tolist()):
        parent[root(i)] = root(j)
    roots = numpy.array([root(i) for i in range(count)])
    sizes = numpy.bincount(roots, minlength=count)
    small = sizes[roots] < SMALL_ISLAND
    means = numpy.bincount(roots, weights=label, minlength=count) / numpy.maximum(sizes, 1)
    label[small] = means[roots[small]]
    return label.tolist()


# Fit the sleeves round the arms: Rodin cuts them at its own angle, so the body's arms poke out through their tops (and
# show through in game). Turn each sleeve about the shoulder joint onto the arm (cuff centre onto the arm's line)...
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    # (picked against the cloth's own widest angle when that's narrower than the arm's: Tripo's Akatsuki sleeves hang ~20 deg)
    spread = numpy.percentile([math.atan2((v.co - shoulder).x * outward, shoulder.z - v.co.z) for v in outfit.data.vertices
                               if (v.co - shoulder).x * outward > 0 and v.co.z < shoulder.z], 98)
    pick_angle = min(arm_angle, spread)
    picked = [v.co.copy() for v, s in zip(outfit.data.vertices, sleeve_shares(shoulder, outward, pick_angle)) if s > 0.5]
    if len(picked) < 20:
        fail(f"no {side} sleeve found out along the arm")
    picked.sort(key=lambda p: (p - shoulder).length)
    cuff_points = picked[-max(len(picked) // 20, 1):]
    cuff = sum(cuff_points, Vector()) / len(cuff_points)
    sleeve_angle = math.atan2(abs(cuff.x - shoulder.x), shoulder.z - cuff.z)
    turn = (cuff - shoulder).rotation_difference(bone_head(f"{side}Hand") - shoulder)
    for vertex, share in zip(outfit.data.vertices, sleeve_shares(shoulder, outward, sleeve_angle)):
        if share > 0:
            vertex.co = shoulder + Quaternion().slerp(turn, share) @ (vertex.co - shoulder)
    log(f"{side} sleeve turned {math.degrees(turn.angle):.1f} deg onto the arm (sleeve {math.degrees(sleeve_angle):.0f}, "
        f"arm {math.degrees(arm_angle):.0f} deg from down)")

# ...then lift the cloth off the body where the body pokes through it (shoulders, upper back). Each vertex moves by a
# smooth average of the lifts round it (cone falloff over LIFT_RADIUS), repeated LIFT_PASSES times: per-vertex pushes crumple
# the cloth and pull the red lining out through the white, a smooth field moves both layers together.
# (the tail and any outer layer such as long hair down the back are left out: the cloth isn't lifted over them; the coat
# goes under the hair, and the opening below lets the tail through)
tail = find_tail(armature, body)
skip = set(tail["indices"]) if tail else set()
skip.update(outer_layer(armature, body))
body_tree = BVHTree.FromPolygons([body.matrix_world @ v.co for v in body.data.vertices],
                                 [p.vertices[:] for p in body.data.polygons if not skip.intersection(p.vertices)])
all_points = KDTree(len(outfit.data.vertices))
for vertex in outfit.data.vertices:
    all_points.insert(vertex.co, vertex.index)
all_points.balance()  # rest positions: neighbourhoods stay those of the cloth as Rodin made it
rest = [vertex.co.copy() for vertex in outfit.data.vertices]
# (more room over the shoulder caps: the deltoid rises when the arm does)
clearance = [CLEARANCE] * len(outfit.data.vertices)
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    for vertex in outfit.data.vertices:
        offset = vertex.co - shoulder
        if CAP_INBOARD < offset.x * outward < CAP_OUT and CAP_LOW < offset.z < CAP_HIGH:
            clearance[vertex.index] = CAP_CLEARANCE
for _ in range(LIFT_PASSES):
    lifts = {}
    for vertex in outfit.data.vertices:
        nearest, normal, _, _ = body_tree.find_nearest(vertex.co)
        if nearest is not None:
            lift = clearance[vertex.index] - (vertex.co - nearest).dot(normal)
            if 0 < lift < MAX_LIFT:
                lifts[vertex.index] = normal * lift
    log(f"lift pass: {len(lifts)} vertices in or near the body (deepest {max((v.length for v in lifts.values()), default=0) * 100:.1f} cm)")
    if max((v.length for v in lifts.values()), default=0) < LIFT_DONE:
        break
    touched = set()
    for index in lifts:
        touched.update(i for _, i, _ in all_points.find_range(rest[index], LIFT_RADIUS))
    moves = []
    for index in touched:
        total, weight = Vector(), 0.0
        for _, other, distance in all_points.find_range(rest[index], LIFT_RADIUS):
            k = 1.0 - distance / LIFT_RADIUS
            weight += k
            if other in lifts:
                total += lifts[other] * k
        moves.append((index, total / weight))
    for index, move in moves:
        outfit.data.vertices[index].co += move
outfit.data.update()


# Tripo and Rodin both weld the tip of each hanging sleeve to the coat where they touch at the hip: those few shared
# vertices would pull a web from a raised arm down to the hip. Split the sleeve's faces lower than TEAR_DROP below the
# shoulder off from everything else (bmesh.ops.split duplicates the boundary vertices), then any edge still joining
# sleeve to coat down there. Nothing is deleted: the split edges stay closed at rest and the cloth swings them apart.
torn = bmesh.new()
torn.from_mesh(outfit.data)
torn.faces.ensure_lookup_table()
drape, marks = set(), []
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    membership = sleeve_membership(shoulder, outward, arm_angle)
    marks.append((shoulder, membership))
    for face in torn.faces:
        centre = face.calc_center_median()
        if (centre.x - shoulder.x) * outward > 0 and centre.z < shoulder.z - TEAR_DROP and                 sum(membership[v.index] for v in face.verts) / len(face.verts) >= 0.5:
            drape.add(face)
joins = [e for e in torn.edges if not set(e.link_faces) & drape and any(
    abs(m[e.verts[0].index] - m[e.verts[1].index]) > 0.5 and max(v.co.z for v in e.verts) < shoulder.z - TEAR_DROP
    for shoulder, m in marks)]
before = len(torn.verts)
bmesh.ops.split(torn, geom=list(drape))
bmesh.ops.split_edges(torn, edges=[e for e in joins if e.is_valid])
torn.to_mesh(outfit.data)
torn.free()
outfit.data.update()
log(f"split {len(drape)} hanging-sleeve faces and {len(joins)} edges off the coat ({len(outfit.data.vertices) - before} vertices duplicated)")


# Tail: find where its axis leaves the coat (the first point along it, from the root, with no coat behind it). Skirt below
# there that the tail would push out is moved in front of it, and an opening is cut round that point for the tail to
# come out through, its edge smoothed.
forward = Vector((0.0, -1.0 if toe.y < foot.y else 1.0, 0.0))
tail_edge = set()
if tail:
    root, tip, radius = Vector(tail["root"]), Vector(tail["tip"]), tail["radius"]
    coat_tree = BVHTree.FromPolygons([v.co for v in outfit.data.vertices], [p.vertices[:] for p in outfit.data.polygons])
    crossing = None
    for step in range(51):
        at = root.lerp(tip, step / 50)
        if coat_tree.ray_cast(at, -forward, 2.0)[0] is None:
            crossing, crossing_t = at, step / 50
            break
    if crossing is None:
        log("the tail stays inside the coat: no opening")
    else:
        axis = tip - root
        moved = 0
        for vertex in outfit.data.vertices:
            t = max(min((vertex.co - root).dot(axis) / axis.length_squared, 1.0), crossing_t)
            centre = root + axis * t
            offset = vertex.co - centre
            reach = radius + TAIL_FRONT
            if offset.length < reach and vertex.co.z < crossing.z:
                ahead = offset.dot(forward)
                vertex.co += forward * (-ahead + math.sqrt(max(ahead * ahead - offset.length_squared + reach * reach, 0.0)))
                moved += 1
        cut = bmesh.new()
        cut.from_mesh(outfit.data)
        hole = [f for f in cut.faces if (f.calc_center_median() - crossing).length < radius + TAIL_MARGIN]
        bmesh.ops.delete(cut, geom=hole, context="FACES")
        bmesh.ops.delete(cut, geom=[v for v in cut.verts if not v.link_faces], context="VERTS")
        rim = [v for v in cut.verts if v.is_boundary and (v.co - crossing).length < radius + TAIL_MARGIN + 0.05]
        for _ in range(3):
            bmesh.ops.smooth_vert(cut, verts=rim, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
        cut.verts.index_update()
        tail_edge = {v.index for v in rim}
        cut.to_mesh(outfit.data)
        cut.free()
        outfit.data.update()
        log(f"tail: opening of {len(hole)} faces at z {crossing.z:.2f}, {moved} skirt vertices moved in front of the tail")


def transfer_weights(source, target):
    """Nearest-surface (interpolated) copy of every vertex group from source onto target."""
    for group in source.vertex_groups:
        if target.vertex_groups.get(group.name) is None:
            target.vertex_groups.new(name=group.name)
    transfer = target.modifiers.new("Weights", "DATA_TRANSFER")
    transfer.object = source
    transfer.use_vert_data = True
    transfer.data_types_verts = {"VGROUP_WEIGHTS"}
    transfer.vert_mapping = "POLYINTERP_NEAREST"
    transfer.layers_vgroup_select_src = "ALL"
    transfer.layers_vgroup_select_dst = "NAME"
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=transfer.name)


# Coat weights: nearest surface of the body WITHOUT its arms and hands (they hang at hip height in the bind pose, so the
# whole body would bind the coat beside them to the forearm and a raised hand would drag it up), then soften the legs'
# pull on the long skirt.
transfer_weights(torso_only, outfit)
bpy.data.objects.remove(torso_only, do_unlink=True)

# Wide sleeves hang nearer the torso than the arm, so the nearest-surface pass binds their lower half to the spine and
# they stay behind (hanging like wings) when the arms lift: blend the sleeves onto the arm bones (sleeve_membership).
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    shares = sleeve_membership(shoulder, outward, arm_angle)
    elbow = bone_head(f"{side}ForeArm")
    forearm_dir = (bone_head(f"{side}Hand") - elbow).normalized()
    upper_group, fore_group = (outfit.vertex_groups[next(b.name for b in armature.data.bones if b.name.split(":")[-1] == name)]
                               for name in (f"{side}Arm", f"{side}ForeArm"))
    for vertex, share in zip(outfit.data.vertices, shares):
        if share <= 0:
            continue
        for group in vertex.groups:
            group.weight *= 1.0 - share
        # A sleeve hangs off the arm bones like a rigid tube: upper arm to the elbow, forearm past it (blended over ELBOW_BLEND
        # round the elbow); nearest-surface weights would hand the armpit to the forearm hanging beside it in the bind pose.
        fore = min(max(((vertex.co - elbow).dot(forearm_dir) + ELBOW_BLEND) / (2 * ELBOW_BLEND), 0.0), 1.0)
        upper_group.add([vertex.index], (1.0 - fore) * share, "ADD")
        fore_group.add([vertex.index], fore * share, "ADD")
    log(f"{side} sleeve: {sum(1 for s in shares if s == 1.0)} vertices fully on the arm")
# The shoulder caps: cloth lying on the shoulder rides the deltoid as the skin does (Mixamo weights it to the arm), else the
# raised shoulder pokes up through cloth pinned to the spine. Cloth within CAP_NEAR of the skin there takes the full body's
# weights, fading back over CAP_FADE at the cap's edges (not the armpit: there the body's weights web the cloth).
skin_copy = outfit.copy()
skin_copy.data = outfit.data.copy()
bpy.context.scene.collection.objects.link(skin_copy)
for group in list(skin_copy.vertex_groups):
    skin_copy.vertex_groups.remove(group)
transfer_weights(body, skin_copy)
bpy.context.view_layer.objects.active = outfit
skin_names = {g.index: g.name for g in skin_copy.vertex_groups}
capped = 0
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    for vertex in outfit.data.vertices:
        offset = vertex.co - shoulder
        out, up = offset.x * outward, offset.z
        inside = min(out - CAP_INBOARD, CAP_OUT - out, up - CAP_LOW, CAP_HIGH - up)
        if inside <= 0 or body_tree.find_nearest(vertex.co)[3] > CAP_NEAR:
            continue
        share = min(inside / CAP_FADE, 1.0)
        target = {skin_names[g.group]: g.weight for g in skin_copy.data.vertices[vertex.index].groups}
        for group in vertex.groups:
            group.weight *= 1.0 - share
        for group_name, weight in target.items():
            outfit.vertex_groups[group_name].add([vertex.index], weight * share, "ADD")
        capped += 1
bpy.data.objects.remove(skin_copy, do_unlink=True)
bpy.context.view_layer.objects.active = outfit
log(f"shoulder caps: {capped} vertices on the body's own weights")
hips_group = outfit.vertex_groups.get(next((g.name for g in outfit.vertex_groups if g.name.split(":")[-1] == "Hips"), ""))
if hips_group is None:
    fail("no Hips vertex group after the weight transfer")
legs = [g for g in outfit.vertex_groups if any(k in g.name.split(":")[-1] for k in ("UpLeg", "Leg", "Foot", "Toe"))]
# The skirt is cloth: its skinned target hangs off the hips only (never stretched by a leg), the cloth swings round the legs.
for vertex in outfit.data.vertices:
    if vertex.co.z > hips.z + HEM_TOP:
        continue
    moved = 0.0
    for group in vertex.groups:
        if outfit.vertex_groups[group.group] in legs:
            moved += group.weight
            group.weight = 0.0
    if moved:
        hips_group.add([vertex.index], moved, "ADD")
# Cloth regions into the vertex colours (UShinobiOutfitBuilderLibrary::BuildCloth): R = hem share, ramping in from
# HEM_TOP above the hips down to the hem; G = sleeve share, from SLEEVE_SWING_FROM of the way to the hand out to the cuff,
# and UNDERARM_SWING under the arm;
# both 0 on the shoulders, chest, back and collar (pinned to the body).
count = len(outfit.data.vertices)
hem_top = hips.z + HEM_TOP
hem_share = numpy.array([min(max((hem_top - v.co.z) / max(hem_top - low.z, 0.01), 0.0), 1.0) for v in outfit.data.vertices])
sleeve_share = numpy.zeros(count)
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    member = numpy.array(sleeve_membership(shoulder, outward, arm_angle)) >= 0.5
    arm = bone_head(f"{side}Hand") - shoulder
    along = numpy.array([(v.co - shoulder).dot(arm) / arm.length_squared for v in outfit.data.vertices])
    if member.any():
        cuff = along[member].max()
        sleeve_share[member] = numpy.clip((along[member] - SLEEVE_SWING_FROM) / max(cuff - SLEEVE_SWING_FROM, 0.01), 0.0, 1.0)
        hem_share[member] = 0.0
# The underarm too: pinned there it can only stretch into a web when the arm rises; as short-swinging cloth it drapes.
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    for vertex in outfit.data.vertices:
        offset = vertex.co - shoulder
        if -0.03 < offset.x * outward < ARMPIT_REACH and -TEAR_DROP - 0.02 < offset.z < -0.06:
            sleeve_share[vertex.index] = max(sleeve_share[vertex.index], UNDERARM_SWING)
for index in tail_edge:
    hem_share[index] = min(hem_share[index], TAIL_EDGE_SWING)
span_x = abs(bone_head("LeftArm").x - bone_head("RightArm").x) / 2
for vertex in outfit.data.vertices:
    if vertex.co.z > bone_head("LeftArm").z - 0.05 and abs(vertex.co.x - centre_x) < span_x:
        hem_share[vertex.index] = sleeve_share[vertex.index] = 0.0
for attribute in list(outfit.data.color_attributes):
    outfit.data.color_attributes.remove(attribute)
colours = outfit.data.color_attributes.new("Col", "BYTE_COLOR", "POINT")
for index in range(count):
    colours.data[index].color_srgb = (float(hem_share[index]), float(sleeve_share[index]), 0.0, 1.0)
outfit.data.color_attributes.active_color = colours
log(f"cloth regions: {int((hem_share > 0).sum())} hem and {int((sleeve_share > 0).sum())} sleeve vertices of {count}")
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=MAX_INFLUENCES)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
unweighted = sum(1 for v in outfit.data.vertices if not v.groups)
if unweighted:
    fail(f"{unweighted} outfit vertices have no skin weights")


# Fit renders (outfit over the body), before the body leaves the scene.
def render_fit():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 768, 1024
    world = bpy.data.worlds.new("Fit")
    world.color = (0.8, 0.8, 0.8)
    scene.world = world
    camera_data = bpy.data.cameras.new("FitCamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 2.1
    camera = bpy.data.objects.new("FitCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    middle = Vector((centre_x, spine.y, 0.9))
    for view, offset, rotation in (("front", (0, -6, 0), (math.radians(90), 0, 0)),
                                   ("side", (6, 0, 0), (math.radians(90), 0, math.radians(90)))):
        camera.location = middle + Vector(offset)
        camera.rotation_euler = rotation
        scene.render.filepath = os.path.join(out_dir, f"fit_{body_name}_{view}.png")
        bpy.ops.render.render(write_still=True)


render_fit()

# Base-colour texture out, as PNG next to the FBX (UE's importer can't read embedded glTF textures).
image = None
for material in outfit.data.materials:
    if material and material.use_nodes:
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image and (image is None or "base" in node.label.lower()):
                image = node.image
if image is None:
    fail("the outfit has no texture image")
png = os.path.join(out_dir, f"{name}_outfit.png")  # the same texture for every body
image.filepath_raw = png
image.file_format = "PNG"
image.save()
log(f"wrote {png}")

# Skin to the skeleton and export only the skeleton and the outfit.
bpy.data.objects.remove(body, do_unlink=True)
outfit.parent = armature
outfit.matrix_parent_inverse = armature.matrix_world.inverted()
outfit.modifiers.new("Armature", "ARMATURE").object = armature
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
outfit.select_set(True)
bpy.context.view_layer.objects.active = armature
fbx_path = os.path.join(out_dir, f"{name}_{body_name}_outfit.fbx")
bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         bake_anim=False, path_mode="STRIP", apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE",
                         colors_type="SRGB")
log(f"exported {fbx_path}")

# Round trip: UE reads the FBX's bone orientations, and a worn outfit only lines up with the body when they match the
# body's (the outfit's bones follow the body's in component space).
reference = {b.name: (armature.matrix_world @ b.matrix_local).copy() for b in armature.data.bones}
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path)
again = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
worst = 0.0
for bone in again.data.bones:
    if bone.name in reference:
        delta = (again.matrix_world @ bone.matrix_local) - reference[bone.name]
        worst = max(worst, max(abs(x) for row in delta for x in row))
log(f"bone round trip: worst matrix difference {worst:.5f} over {len(again.data.bones)} bones")
if worst > 0.001:
    fail("the exported skeleton's bone orientations differ from the body's")
