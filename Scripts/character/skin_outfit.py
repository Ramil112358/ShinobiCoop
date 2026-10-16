"""Blender (5.2) headless skinning of an outfit (a cloak made by Rodin) to the Mixamo skeleton, so it can be worn over
any rigged character (AShinobiPlayerCharacter::Outfits; docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/skin_outfit.py -- <body_rigged.fbx> <outfit.glb> <out_dir> [Name]

Steps: import the rigged body (the reference skeleton and skin weights) and the outfit GLB -> scale and place the outfit
on the body from the skeleton's landmarks (collar at the head bone, hem just above the knees, centred on the spine) ->
turn each sleeve about its shoulder onto the arm and lift the cloth smoothly off wherever the body pokes through ->
copy the body's skin weights to the nearest
surface (the long skirt then follows the legs only part way, blending into the hips, so it doesn't tear when they
walk; the sleeves take the arms' weights so they move with them) -> parent to the skeleton -> <out_dir>/<Name>_outfit.fbx (skeleton + skinned outfit) and <Name>_outfit.png (its
base-colour texture), plus fit_front.png / fit_side.png renders of the outfit on the body.
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

GIRTH = 1.04  # the outfit is cut a little wide so it hangs loose
KNEE_DROP = 0.03  # hem this far below the knee bone
LEG_KEEP_AT_HEM = 0.35  # share of the leg weights kept at the hem (the rest goes to the hips)
MAX_INFLUENCES = 4
CLEARANCE = 0.01  # cloth kept this far off the body's skin
MAX_LIFT = 0.1  # deeper than this it's not cloth caught in the body (the collar's inside, a lining): left alone
LIFT_RADIUS, LIFT_PASSES = 0.1, 6  # the lift field's smoothing (m) and passes
SLEEVE_START, SLEEVE_FULL = 0.45, 0.8  # sleeve blend, as shares of the sleeve's angle from straight down at the shoulder
COAT_SEED = 0.15  # cloth hanging this close to straight down from the shoulder (share of the arm's angle) is surely coat
MEMBERSHIP_PASSES = 3000  # relaxation passes spreading the sleeve/coat label along the mesh
SMALL_ISLAND = 50  # loose pieces with fewer vertices than this move as one
LIMB_NEARER = 0.5  # low cloth this much nearer the arm/hand surface than the torso's is sleeve
SHOULDER_RAMP = 0.08  # sleeve share grows in over this far outboard of the shoulder joint (m)
ELBOW_BLEND = 0.06  # sleeve weights hand over from upper arm to forearm this far either side of the elbow (m)
SPECK_SIZE = 30  # loose pieces with fewer vertices than this out by the hands are weld crumbs
SLIVER_REACH = 0.08  # coat cloth this far off the torso beside the hanging hand is weld debris (m)
SLIVER_TIE = 0.01  # ...when no further than this (m) from the hand than from the torso
ARMPIT_REACH, ARMPIT_RADIUS = 0.15, 0.05  # the armpit band (m out of / above the shoulder joint) and its smoothing
LAYER_RADIUS = 0.04  # armpit weights are evened over this radius (m), wider than the shell-to-lining gap
LAYER_ALIKE, LAYER_BAND_LOW = 0.4, 0.14  # ...only with cloth this alike in arm weight; down to this far below TEAR_DROP (m)
LAYER_REACH, ARMPIT_INBOARD = 0.3, 0.06  # ...and from this far inboard of the shoulder joint to this far out (m)
LAYER_PASSES = 3
SKIRT_ABOVE_HIPS = 0.05  # the skirt's weights are evened from this far above the hip joint down (m)
SEAM_SPLIT = 0.35  # below the split line, edges whose ends' arm weights differ more than this are split
TEAR_DROP = 0.16  # sleeve cloth lower than this below the shoulder joint (m) is split from the coat (not the armpit seam)


def fail(message):
    print(f"[skin_outfit] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[skin_outfit] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 3:
    fail("usage: -- <body_rigged.fbx> <outfit.glb> <out_dir> [Name]")
body_fbx, outfit_glb, out_dir = (os.path.abspath(a) for a in args[:3])
name = args[3] if len(args) > 3 else os.path.splitext(os.path.basename(outfit_glb))[0]
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
low, high = bounds(outfit)
log(f"outfit {high.z - low.z:.2f} m tall, {high.x - low.x:.2f} m wide; collar z {high.z:.2f}, hem z {low.z:.2f}")

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
    band = numpy.nonzero((out > 0) & (out < ARMPIT_REACH) & (co[:, 2] > -TEAR_DROP - ARMPIT_RADIUS) & (co[:, 2] < ARMPIT_REACH))[0]
    if len(band):
        points = KDTree(count)
        for index, point in enumerate(co):
            points.insert(point, index)
        points.balance()
        smoothed = label.copy()
        for index in band.tolist():
            near = points.find_range(co[index], ARMPIT_RADIUS)
            weights = [1.0 - distance / ARMPIT_RADIUS for _, _, distance in near]
            smoothed[index] = sum(label[other] * k for (_, other, _), k in zip(near, weights)) / sum(weights)
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
    picked = [v.co.copy() for v, s in zip(outfit.data.vertices, sleeve_shares(shoulder, outward, arm_angle)) if s > 0.5]
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
body_tree = BVHTree.FromPolygons([body.matrix_world @ v.co for v in body.data.vertices], [p.vertices[:] for p in body.data.polygons])
all_points = KDTree(len(outfit.data.vertices))
for vertex in outfit.data.vertices:
    all_points.insert(vertex.co, vertex.index)
all_points.balance()  # rest positions: neighbourhoods stay those of the cloth as Rodin made it
for _ in range(LIFT_PASSES):
    lifts = {}
    for vertex in outfit.data.vertices:
        nearest, normal, _, _ = body_tree.find_nearest(vertex.co)
        if nearest is not None:
            lift = CLEARANCE - (vertex.co - nearest).dot(normal)
            if 0 < lift < MAX_LIFT:
                lifts[vertex.index] = normal * lift
    log(f"lift pass: {len(lifts)} vertices in or near the body (deepest {max((v.length for v in lifts.values()), default=0) * 100:.1f} cm)")
    if not lifts:
        break
    touched = set()
    for index in lifts:
        touched.update(i for _, i, _ in all_points.find_range(outfit.data.vertices[index].co, LIFT_RADIUS))
    moves = []
    for index in touched:
        total, weight = Vector(), 0.0
        for _, other, distance in all_points.find_range(outfit.data.vertices[index].co, LIFT_RADIUS):
            k = 1.0 - distance / LIFT_RADIUS
            weight += k
            if other in lifts:
                total += lifts[other] * k
        moves.append((index, total / weight))
    for index, move in moves:
        outfit.data.vertices[index].co += move
outfit.data.update()


# Rodin welds the tip of each hanging sleeve to the coat where they touch at the hip: those few shared vertices would pull
# strings from a raised arm down to the hip. Split the sleeve's faces lower than TEAR_DROP below the shoulder off from
# everything else (bmesh.ops.split duplicates the boundary vertices), so the drape hangs free of the coat.
torn = bmesh.new()
torn.from_mesh(outfit.data)
torn.faces.ensure_lookup_table()
drape = set()
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    membership = sleeve_membership(shoulder, outward, arm_angle)
    for face in torn.faces:
        centre = face.calc_center_median()
        if (centre.x - shoulder.x) * outward > 0 and centre.z < shoulder.z - TEAR_DROP and                 sum(membership[v.index] for v in face.verts) / len(face.verts) >= 0.5:
            drape.add(face)
# The coat's half of the weld (slivers beside the hanging hand, well off the torso) would show as loose threads once the
# drape lifts away from it: drop those faces.
slivers = []
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    for face in torn.faces:
        centre = face.calc_center_median()
        if face not in drape and (centre.x - shoulder.x) * outward > 0 and centre.z < shoulder.z - TEAR_DROP:
            to_torso = torso_tree.find_nearest(centre)[3]
            if to_torso > SLIVER_REACH and limb_tree.find_nearest(centre)[3] < to_torso + SLIVER_TIE:
                slivers.append(face)
bmesh.ops.delete(torn, geom=slivers, context="FACES")
# ...and the crumbs it leaves: loose pieces under SPECK_SIZE vertices out there.
seen, specks = set(), []
for start in torn.verts:
    if start in seen:
        continue
    piece, stack = [], [start]
    seen.add(start)
    while stack:
        vert = stack.pop()
        piece.append(vert)
        for edge in vert.link_edges:
            other = edge.other_vert(vert)
            if other not in seen:
                seen.add(other)
                stack.append(other)
    if len(piece) < SPECK_SIZE:
        centre = sum((v.co for v in piece), Vector()) / len(piece)
        if any((centre.x - arm_frame(side)[0].x) * arm_frame(side)[1] > 0 and centre.z < arm_frame(side)[0].z - TEAR_DROP and
               SLIVER_REACH < torso_tree.find_nearest(centre)[3] > limb_tree.find_nearest(centre)[3] - SLIVER_TIE for side in ("Left", "Right")):
            specks += piece
bmesh.ops.delete(torn, geom=specks, context="VERTS")
drape = {face for face in drape if face.is_valid}
log(f"dropped {len(slivers)} weld slivers beside the hands ({len(specks)} loose vertices with them)")
before = len(torn.verts)
bmesh.ops.split(torn, geom=list(drape))
torn.to_mesh(outfit.data)
torn.free()
outfit.data.update()
log(f"split {len(drape)} hanging-sleeve faces off the coat ({len(outfit.data.vertices) - before} vertices duplicated)")
# Then any edge still joining sleeve to coat down there (a welded corner on a coat-side face).
marks = []
for side in ("Left", "Right"):
    shoulder, outward, arm_angle = arm_frame(side)
    marks.append((shoulder, sleeve_membership(shoulder, outward, arm_angle)))
torn = bmesh.new()
torn.from_mesh(outfit.data)
joins = [e for e in torn.edges if any(abs(m[e.verts[0].index] - m[e.verts[1].index]) > 0.5 and
                                      max(v.co.z for v in e.verts) < shoulder.z - TEAR_DROP for shoulder, m in marks)]
bmesh.ops.split_edges(torn, edges=joins)
torn.to_mesh(outfit.data)
torn.free()
outfit.data.update()
log(f"split {len(joins)} sleeve-to-coat edges")


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
hips_group = outfit.vertex_groups.get(next((g.name for g in outfit.vertex_groups if g.name.split(":")[-1] == "Hips"), ""))
if hips_group is None:
    fail("no Hips vertex group after the weight transfer")
legs = [g for g in outfit.vertex_groups if any(k in g.name.split(":")[-1] for k in ("UpLeg", "Leg", "Foot", "Toe"))]
span = max(hips.z - low.z, 0.01)
for vertex in outfit.data.vertices:
    fade = min(max((hips.z - (outfit.matrix_world @ vertex.co).z) / span, 0.0), 1.0)
    keep = 1.0 - (1.0 - LEG_KEEP_AT_HEM) * fade
    moved = 0.0
    for group in vertex.groups:
        if outfit.vertex_groups[group.group] in legs:
            moved += group.weight * (1.0 - keep)
            group.weight *= keep
    if moved:
        hips_group.add([vertex.index], moved, "ADD")
# Round the armpit the weights blend arm and spine, and with the arm raised high a weight difference of a few hundredths
# between the white shell and the red lining (1-2 cm apart) moves them further apart than that: the lining crosses out
# through the shell. There every vertex takes the cone-weighted mean weights of the cloth within LAYER_RADIUS in space,
# both layers alike, over LAYER_PASSES passes, which also spreads the armpit seam into a band that stretches evenly. Below
# the split line it counts only cloth whose arm weight is within LAYER_ALIKE of its own (the coat and the drape split off
# beside it, ~0 and ~1, never mix).
count, groups = len(outfit.data.vertices), len(outfit.vertex_groups)
weights = numpy.zeros((count, groups))
for vertex in outfit.data.vertices:
    for group in vertex.groups:
        weights[vertex.index, group.group] = group.weight
co = numpy.empty(count * 3)
outfit.data.vertices.foreach_get("co", co)
co = co.reshape(count, 3)
points = KDTree(count)
for index, point in enumerate(co):
    points.insert(point, index)
points.balance()
original, band = weights.copy(), set()
for _ in range(LAYER_PASSES):
    evened = weights.copy()
    for side in ("Left", "Right"):
        shoulder, outward, _ = arm_frame(side)
        rel = co - numpy.array(shoulder)
        out = rel[:, 0] * outward
        arm = weights[:, [i for i, g in enumerate(outfit.vertex_groups) if is_arm(g.name) and side in g.name]].sum(axis=1)
        for index in numpy.nonzero((out > -ARMPIT_INBOARD) & (out < LAYER_REACH) & (rel[:, 2] > -TEAR_DROP - LAYER_BAND_LOW) &
                                   (rel[:, 2] < ARMPIT_REACH))[0].tolist():
            above_split = rel[index, 2] > -TEAR_DROP
            near = [(other, 1.0 - distance / LAYER_RADIUS) for _, other, distance in points.find_range(co[index], LAYER_RADIUS)
                    if above_split or abs(arm[other] - arm[index]) < LAYER_ALIKE]
            k = numpy.array([w for _, w in near])
            evened[index] = (weights[[other for other, _ in near]] * k[:, None]).sum(axis=0) / k.sum()
            band.add(index)
    # The skirt the same way: a lifted leg drags the hem by its leg weights, and shell and lining a few hundredths apart
    # cross there too (ragged red streaks along the hem). Only between cloth alike in arm weight (the drape hangs there).
    arm = weights[:, [i for i, g in enumerate(outfit.vertex_groups) if is_arm(g.name)]].sum(axis=1)
    for index in numpy.nonzero(co[:, 2] < hips.z + SKIRT_ABOVE_HIPS)[0].tolist():
        near = [(other, 1.0 - distance / LAYER_RADIUS) for _, other, distance in points.find_range(co[index], LAYER_RADIUS)
                if abs(arm[other] - arm[index]) < LAYER_ALIKE]
        k = numpy.array([w for _, w in near])
        evened[index] = (weights[[other for other, _ in near]] * k[:, None]).sum(axis=0) / k.sum()
        band.add(index)
    weights = evened
for index in band:
    for group_index in range(groups):
        if weights[index, group_index] > 0.001:
            outfit.vertex_groups[group_index].add([index], float(weights[index, group_index]), "REPLACE")
        elif original[index, group_index] > 0:
            outfit.vertex_groups[group_index].remove([index])
log(f"evened the weights of {len(band)} armpit and skirt vertices across the cloth layers")
# Last, with the weights final: any edge below the split line still joining cloth that follows the arm to cloth that
# doesn't (arm weights more than SEAM_SPLIT apart) would stretch into a string when the arm rises. Split those too
# (bmesh copies the weights onto the duplicated vertices).
torn = bmesh.new()
torn.from_mesh(outfit.data)
deform = torn.verts.layers.deform.active
arm_groups = {side: [g.index for g in outfit.vertex_groups if is_arm(g.name) and side in g.name] for side in ("Left", "Right")}
joins = []
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    joins += [e for e in torn.edges if all((v.co.x - shoulder.x) * outward > 0 and v.co.z < shoulder.z - TEAR_DROP for v in e.verts)
              and abs(sum(e.verts[0][deform].get(g, 0.0) - e.verts[1][deform].get(g, 0.0) for g in arm_groups[side])) > SEAM_SPLIT]
bmesh.ops.split_edges(torn, edges=joins)
# A face whose own corners disagree that much (a sliver bridging the two) can't be split apart: drop it.
bridges = []
for side in ("Left", "Right"):
    shoulder, outward, _ = arm_frame(side)
    for face in torn.faces:
        if all((v.co.x - shoulder.x) * outward > 0 and v.co.z < shoulder.z - TEAR_DROP for v in face.verts):
            arms = [sum(v[deform].get(g, 0.0) for g in arm_groups[side]) for v in face.verts]
            if max(arms) - min(arms) > SEAM_SPLIT:
                bridges.append(face)
bmesh.ops.delete(torn, geom=bridges, context="FACES")
# The splits leave inner-facing scraps of the coat's side under each arm, whose outer half went with the drape: small loose
# coat-bound pieces between the hips and the split line show as hairlines once the arm lifts the drape away. Drop them.
all_arm = [g.index for g in outfit.vertex_groups if is_arm(g.name)]
seen, orphans = set(), []
for start in torn.verts:
    if start in seen:
        continue
    piece, stack = [], [start]
    seen.add(start)
    while stack:
        vert = stack.pop()
        piece.append(vert)
        for edge in vert.link_edges:
            if edge.other_vert(vert) not in seen:
                seen.add(edge.other_vert(vert))
                stack.append(edge.other_vert(vert))
    if len(piece) >= SPECK_SIZE:
        continue
    centre = sum((v.co for v in piece), Vector()) / len(piece)
    coat_bound = sum(sum(v[deform].get(g, 0.0) for g in all_arm) for v in piece) / len(piece) < 0.5
    under_arm = any((centre.x - arm_frame(side)[0].x) * arm_frame(side)[1] > 0 and hips.z < centre.z < arm_frame(side)[0].z - TEAR_DROP
                    for side in ("Left", "Right"))
    if coat_bound and under_arm:
        orphans += piece
bmesh.ops.delete(torn, geom=orphans, context="VERTS")
torn.to_mesh(outfit.data)
torn.free()
outfit.data.update()
log(f"split {len(joins)} edges and dropped {len(bridges)} sliver faces between arm-bound and coat-bound cloth, "
    f"and {len(orphans)} vertices of loose scraps under the arms")
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
        scene.render.filepath = os.path.join(out_dir, f"fit_{view}.png")
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
png = os.path.join(out_dir, f"{name}_outfit.png")
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
fbx_path = os.path.join(out_dir, f"{name}_outfit.fbx")
bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         bake_anim=False, path_mode="STRIP", apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE")
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
