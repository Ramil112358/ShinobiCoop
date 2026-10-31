"""Animates the summon dragon Tairn (Fourth Wing's black Morningstartail; the crimson dragon is build_dragon_crimson.py): the Tripo Studio
model, auto-rigged there (SourceArt/Tripo/Tairn, the FBX from tairn_rigged.zip) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_dragon.py            (add  -- preview  to render pose previews to Saved/dragon_*.png)

Tripo's rig (a spine, a neck and head chain, four legs with toes, a wing a side: arm, forearm, wrist and three three-bone fingers) and its
skin weights are kept, the clips are keyed here (Tripo Studio only animates humanoids). The model stands on all fours with its neck up,
its wings half raised and its tail curled round to its left, ending in the morningstar club, facing -Y. Tripo's tail bones stop at the
top of the curl, so the rest of the tail and the club get bones of their own, marched along the mesh. It is turned to face +X, centred
on its spine and scaled up, and a jaw bone is added (the mouth line was measured on the mesh). Every flying clip starts from one flying
pose built from that rest pose: chest level, neck reached forward, tail straightened out behind, legs tucked and trailing, wings opened
out flat. Clips: Idle (a slow hover), Flap (the fast beat, a loop), Glide (wings out, a slow sway) and Breath (rears the head back,
opens the jaw and thrusts; played once). Writes SourceArt/Creatures/Tairn/Tairn.fbx (mesh + skeleton), Tairn_<Clip>.fbx per clip and
Tairn.png (2k; Tripo gave only a base colour). Faces +X, feet on Z = 0 in the rest pose; metres.
"""
import glob
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = glob.glob(os.path.join(ROOT, "SourceArt", "Tripo", "Tairn", "fbx", "*.fbx"))[0]
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Tairn")
PREVIEW = "preview" in sys.argv
FPS = 30
SCALE = 1.4      # Tripo's model is 0.5 m tall
I3 = Matrix.Identity(3)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
scene = bpy.context.scene
scene.render.fps = FPS
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
body.name, arm.name, arm.data.name = "Body", "Armature", "Armature"
os.makedirs(OUT, exist_ok=True)
for bone in arm.data.bones:
    bone.name = bone.name.replace("tripo::", "")  # Unreal would read "tripo::" as an FBX namespace


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# --- the lower jaw (in Tripo's space: facing -Y, the head on the midline X = -0.037) ---------------------------------------------
# The teeth line rises from the snout (y -0.368, z 0.397) back to the mouth's corner (y -0.325); everything of the head below it is the jaw.
MID_X, CORNER_Y = -0.037, -0.325


def mouth_z(y):
    return 0.397 + 0.32 * (y + 0.368)


def in_jaw(p):
    return p.y < CORNER_Y and 0.30 < p.z < mouth_z(p.y) and abs(p.x - MID_X) < 0.06


# Tripo's mouth is one closed surface: opening the jaw would stretch the lips into a sheet. Cut it along the mouth line (each lip gets
# its own vertices) and leave the cut open: the material is one-sided, so the open mouth shows through the head, no dark sheet.
basecolor = next(i for i in bpy.data.images if i.name.lower().endswith("basecolor"))

world = body.matrix_world
bm = bmesh.new()
bm.from_mesh(body.data)
jaw_faces = {f for f in bm.faces if in_jaw(world @ f.calc_center_median())}
lips = []  # (s, t): the lip edge as the upper lip's face runs it
for e in bm.edges:
    if len(e.link_faces) == 2 and (e.link_faces[0] in jaw_faces) != (e.link_faces[1] in jaw_faces) and \
            all((world @ v.co).z > mouth_z((world @ v.co).y) - 0.006 and (world @ v.co).y < CORNER_Y for v in e.verts):
        upper = e.link_faces[0] if e.link_faces[1] in jaw_faces else e.link_faces[1]
        loop = next(l for l in upper.loops if l.edge == e)
        lips.append((loop.vert, loop.link_loop_next.vert))
lower = {v: bm.verts.new(v.co, v) for v in {v for pair in lips for v in pair}}  # the lower lip's copy, with the same weights for now
for f in [f for f in jaw_faces if any(v in lower for v in f.verts)]:
    rebuilt = bm.faces.new([lower.get(v, v) for v in f.verts], f)
    for old, new in zip(f.loops, rebuilt.loops):
        new.copy_from(old)
    jaw_faces.discard(f)
    jaw_faces.add(rebuilt)
    bm.faces.remove(f)
bm.verts.index_update()
upper_lip = {v.index for v in lower}
jaw_verts = {v.index: v.co.copy() for f in jaw_faces for v in f.verts if v.index not in upper_lip}
bm.to_mesh(body.data)
bm.free()
jaw_weights = {}
for index, co in jaw_verts.items():
    w = smooth((CORNER_Y - (world @ co).y) / 0.015)  # the whole lower jaw, easing in from the mouth's corner
    if w > 0:
        jaw_weights[index] = w
print("[build_dragon] mouth cut along", len(lips), "edges")
JAW = (Vector((MID_X, -0.318, 0.41)), Vector((MID_X, -0.37, 0.392)))

# --- fit: face +X, scale, centre the spine on Y = 0 ---------------------------------------------------------------------------
FIT = Matrix.Diagonal((SCALE, SCALE, SCALE, 1)) @ Matrix.Rotation(math.radians(90), 4, "Z")
arm.matrix_world = FIT @ arm.matrix_world
select(arm, body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
centre = arm.data.bones["Spine_0"].head_local.y
arm.location.y -= centre
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
FIT = Matrix.Translation((0, -centre, 0)) @ FIT

# --- the rest of the tail: Tripo's last tail bone (Tail_3) stops at the top of the curl and owns the curl and the club, and Tail_2 and
# Tail_3 cut the curl's corner (off the tail's centre, straightening the tail would kink it). March along the mesh from Tail_1's tip:
# each step, the middle of the tail's cross-section a step further on; it stops in the club. Tail_2 and Tail_3 move onto the march.
tail_groups = {body.vertex_groups[n].index for n in ("Tail_2", "Tail_3")}
tail_points = [v.co.copy() for v in body.data.vertices if v.groups and max(v.groups, key=lambda g: g.weight).group in tail_groups]
STEP = 0.07
march = [arm.data.bones["Tail_1"].tail_local.copy()]
heading = (march[0] - arm.data.bones["Tail_1"].head_local).normalized()
while len(march) < 30:
    ahead = [p for p in tail_points if 0.5 * STEP < (p - march[-1]).dot(heading) < 1.5 * STEP and (p - march[-1]).length < 2.5 * STEP]
    if len(ahead) < 20:
        break
    nxt = sum(ahead, Vector()) / len(ahead)  # on the tail's centre line: off it, straightening the tail would kink it
    heading = (nxt - march[-1]).normalized()
    march.append(nxt)
print("[build_dragon] tail marched", len(march) - 1, "bones past Tail_1, ending at", tuple(round(c, 2) for c in march[-1]))

select(arm)
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
jaw = eb.new("jaw")
jaw.head, jaw.tail = FIT @ JAW[0], FIT @ JAW[1]
jaw.parent = eb["bone_9"]  # the bone the whole head is weighted to
parent = eb["Tail_1"]
NEW_TAIL = []
for i in range(1, len(march)):
    name = f"Tail_{1 + i}"
    bone = eb.get(name) or eb.new(name)
    bone.head, bone.tail, bone.parent, bone.use_connect = march[i - 1], march[i], parent, True
    if i > 2:
        NEW_TAIL.append(name)
    parent = bone
bpy.ops.object.mode_set(mode="OBJECT")
group = body.vertex_groups.new(name="jaw")
for index, w in jaw_weights.items():
    for g in body.data.vertices[index].groups:
        g.weight *= 1 - w
    group.add([index], w, "REPLACE")
print("[build_dragon] jaw verts:", len(jaw_weights))


def segment_distance(p, a, b):
    ab = b - a
    return (p - (a + ab * max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9))))).length


# The tail's vertices go to the chain from its root to the club, by distance (the nearest two bones share each one): Tripo's own tail
# weights followed its curl, so the straightened tail kinked where the curl began.
chain = ["bone_64", "Tail_0", "Tail_1", "Tail_2", "Tail_3"] + NEW_TAIL
chain_groups = [body.vertex_groups.get(n) or body.vertex_groups.new(name=n) for n in chain]
owned = {body.vertex_groups[n].index for n in chain[1:]}
for v in body.data.vertices:
    if not v.groups or max(v.groups, key=lambda g: g.weight).group not in owned:
        continue
    d = [segment_distance(v.co, arm.data.bones[n].head_local, arm.data.bones[n].tail_local) for n in chain]
    nearest = sorted(range(len(chain)), key=d.__getitem__)[:2]
    inv = [1 / max(d[i], 1e-4) ** 4 for i in nearest]
    for g in list(v.groups):
        body.vertex_groups[g.group].remove([v.index])
    for i, w in zip(nearest, inv):
        chain_groups[i].add([v.index], w / sum(inv), "REPLACE")

# Tripo's weights are mostly sound, but a few bones reach far: the front feet's toes own each other's toes and share the left hind
# toes, a wing's shoulder owns part of the thigh below it. Each vertex keeps only the bones of its own part and of the spine; a spine
# vertex also keeps its strongest other part. Its own part is, of the parts holding a good share of its weight, the one with the nearest
# bone (by weight alone, the left hind toes went with the front foot, a 0.5 m strip in flight).
PARTS = {"tail": ["bone_64"], "front_L": ["bone_13"], "front_R": ["bone_21"], "hind_L": ["bone_53"], "hind_R": ["bone_58"],
         "wing_L": ["bone_29"], "wing_R": ["bone_41"], "head": ["bone_4"]}
part_of = {}
for part, tops in PARTS.items():
    for top in tops:
        for bone in [arm.data.bones[top]] + list(arm.data.bones[top].children_recursive):
            part_of[bone.name] = part
group_part = {g.index: part_of.get(g.name, "spine") for g in body.vertex_groups}
# A vertex much nearer a bone of another part than its own bone goes wholly to that one.
segments = [(body.vertex_groups[b.name].index, b.head_local, b.tail_local) for b in arm.data.bones if b.name in body.vertex_groups]
bone_ends = {index: (a, b) for index, a, b in segments}
moved = 0
for v in body.data.vertices:
    if not v.groups:
        continue
    own = max(v.groups, key=lambda g: g.weight).group
    distance = {index: segment_distance(v.co, a, b) for index, a, b in segments}
    nearest = min(distance, key=distance.get)
    if group_part[nearest] != group_part[own] and distance[own] > 2.5 * distance[nearest] + 0.04:
        for g in list(v.groups):
            body.vertex_groups[g.group].remove([v.index])
        body.vertex_groups[nearest].add([v.index], 1.0, "REPLACE")
        moved += 1
print("[build_dragon] verts given to their nearest bone:", moved)
trimmed = 0
for v in body.data.vertices:
    totals = {}
    for g in v.groups:
        totals[group_part[g.group]] = totals.get(group_part[g.group], 0.0) + g.weight
    if not totals:
        continue
    strong = [g for g in v.groups if totals[group_part[g.group]] >= 0.3 * sum(totals.values()) and g.weight > 0.02]
    own = group_part[min(strong, key=lambda g: segment_distance(v.co, *bone_ends[g.group])).group] if strong else max(totals, key=totals.get)
    keep = {own, "spine"}
    if own == "spine" and len(totals) > 1:
        keep.add(max((p for p in totals if p != "spine"), key=totals.get))
    drop = [g.group for g in v.groups if group_part[g.group] not in keep]
    for index in drop:
        body.vertex_groups[index].remove([v.index])
    trimmed += bool(drop)
print("[build_dragon] weights trimmed on", trimmed, "verts")

WINGS = {"L": ("bone_29", "bone_30", ["bone_38", "bone_35", "bone_32"]), "R": ("bone_41", "bone_42", ["bone_50", "bone_47", "bone_44"])}  # fingers leading edge first
BONES = arm.data.bones
# The upper arms own back skin beside the spine behind the shoulders (the left one 10 cm off the bone): it rose in a lump as the arm swung
# out. Near the torso, an upper arm's vertices pass to the nearest spine bone the further they are from the arm.
spine_segments = [(index, a, b) for index, a, b in segments if group_part[index] == "spine"]
handed = 0
for side in WINGS:
    upper = BONES[WINGS[side][0]]
    group = body.vertex_groups[upper.name]
    for v in body.data.vertices:
        if not v.groups or abs(v.co.y) > 0.16 or body.vertex_groups[max(v.groups, key=lambda g: g.weight).group] != group:
            continue
        keep = smooth((0.1 - segment_distance(v.co, upper.head_local, upper.tail_local)) / 0.05)
        if keep < 1:
            spine = min(spine_segments, key=lambda s: segment_distance(v.co, s[1], s[2]))[0]
            for g in v.groups:
                g.weight *= keep
            body.vertex_groups[spine].add([v.index], 1 - keep, "ADD")
            handed += 1
print("[build_dragon] shoulder skin handed to the spine:", handed)

# The tail, straightened for flight, was far longer than the body. Each tail bone from Tail_0 to the one before the club is squeezed
# along its own length (its own scale only, so the squeeze never compounds down the chain; the club keeps its size), the mesh is
# deformed with it, and that becomes the rest pose: Unreal gets a shorter tail with unscaled bones (scale keyed in the clips does not
# work there: Unreal composes bone scale axis by axis, ignoring the children's turn, and the tail went thin instead of short).
TAIL_SHORTEN = 0.5
select(arm)
bpy.ops.object.mode_set(mode="POSE")
squeezed = ["Tail_0", "Tail_1", "Tail_2", "Tail_3"] + NEW_TAIL[:-1]
tail_before = arm.data.bones[NEW_TAIL[-1]].tail_local.copy()
for name in squeezed:
    arm.data.bones[name].inherit_scale = "NONE"
    arm.pose.bones[name].scale = (1, TAIL_SHORTEN, 1)
bpy.context.view_layer.update()
evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
deformed = [v.co.copy() for v in evaluated.to_mesh().vertices]
evaluated.to_mesh_clear()
for v, co in zip(body.data.vertices, deformed):
    v.co = co
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
for name in squeezed:
    arm.data.bones[name].inherit_scale = "FULL"
print("[build_dragon] tail squeezed: the club from", tuple(round(c, 2) for c in tail_before), "to", tuple(round(c, 2) for c in arm.data.bones[NEW_TAIL[-1]].tail_local))


basecolor.scale(2048, 2048)
basecolor.filepath_raw = os.path.join(OUT, "Tairn.png")
basecolor.file_format = "PNG"
basecolor.save()

# --- the flying pose ----------------------------------------------------------------------------------------------------------
PARENT = {b.name: b.parent.name if b.parent else None for b in BONES}
ORDER = sorted(PARENT, key=lambda n: len(BONES[n].parent_recursive))
REST = {b.name: b.matrix_local.to_3x3() for b in BONES}
DIR = {b.name: (b.tail_local - b.head_local).normalized() for b in BONES}
HIPS = BONES["Spine_0"].head_local.copy()


def rx(deg):   # about world X: positive lifts the left (+Y) side
    return Matrix.Rotation(math.radians(deg), 3, "X")


def ry(deg):   # about world Y: positive tips the nose (+X) down
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):   # about world Z: positive turns the nose to the left (+Y)
    return Matrix.Rotation(math.radians(deg), 3, "Z")


LEGS = {  # each leg's bones from the shoulder or hip down (toes follow), with where each points in flight: thighs back and down, feet trailing
    "front": (["bone_14", "bone_15", "bone_16"], ["bone_22", "bone_23", "bone_24"], ((-0.6, 0, -1), (1, 0, 0.15), (0.2, 0, -1))),
    "hind": (["bone_53", "bone_54", "bone_55", "bone_56"], ["bone_58", "bone_59", "bone_60", "bone_61"], ((-0.5, 0, -1), (-1, 0, -0.5), (-1, 0, -0.1), (-1, 0, 0))),
}
WING_AIMS = ((0.1, 1, 0.05), (0.3, 1, 0.05), ((-0.1, 1, 0), (-0.7, 1, 0), (-1, 0.35, 0)))  # the left wing opened out flat: arm, forearm, fingers
TAIL = ["bone_64", "Tail_0", "Tail_1", "Tail_2", "Tail_3"] + NEW_TAIL
NECK = ["bone_4", "bone_5", "bone_6", "Head_0", "bone_8"]

AIMS = {"bone_2": (1, 0, 0.15), "bone_3": (1, 0, 0.25)}  # the chest, raised at rest, level in flight
for left, right, targets in LEGS.values():
    for bones in (left, right):
        AIMS.update(zip(bones, targets))
for i, name in enumerate(TAIL):
    AIMS[name] = (-1, 0, -0.3 if i == 0 else -0.05)
AIMS.update(zip(NECK, ((1, 0, 0.45), (1, 0, 0.35), (1, 0, 0.25), (1, 0, 0.05), (1, 0, -0.3))))
# Tairn's wings are folded at rest (the wrist high above the shoulder): each bone is aimed out, the fingers fanned flat behind the forearm.
for side, s in (("L", 1), ("R", -1)):
    arm_bone, forearm, fingers = WINGS[side]
    AIMS[arm_bone], AIMS[forearm] = [(x, s * y, z) for x, y, z in WING_AIMS[:2]]
    for finger, (x, y, z) in zip(fingers, WING_AIMS[2]):
        bone = BONES[finger]
        while bone:
            AIMS[bone.name] = (x, s * y, z)
            bone = bone.children[0] if bone.children else None
# Aimed the shortest way a bone keeps its roll, and the membrane beside it ended up above or below it (at rest the left wing's lower
# membrane hangs against the flank and thigh: in flight it hung down the leg like a curtain). So each wing bone is also rolled: the plane
# of the membrane around it (fitted to the wing's vertices within 10 cm: a bone's own vertices can be just the tube of an arm) is laid flat.
MEMBRANE = {}  # wing bone -> the normal of its membrane at rest
for side, s in (("L", 1), ("R", -1)):
    wing = np.array([v.co for v in body.data.vertices if v.groups and s * v.co.y > 0.13 and
                     group_part[max(v.groups, key=lambda g: g.weight).group] == "wing_" + side])
    for bone in [BONES[WINGS[side][0]]] + list(BONES[WINGS[side][0]].children_recursive):
        a, b = np.array(bone.head_local), np.array(bone.tail_local)
        t = np.clip((wing - a) @ (b - a) / max((b - a) @ (b - a), 1e-9), 0, 1)
        points = wing[np.linalg.norm(wing - (a + t[:, None] * (b - a)), axis=1) < 0.1]
        if len(points) > 20:
            MEMBRANE[bone.name] = Vector(np.linalg.eigh(np.cov((points - points.mean(axis=0)).T))[1][:, 0])


BASE, BASE_DELTA = {}, {}  # each bone's world rotation in the flying pose, and the turn that takes it there from its parent's
for name in ORDER:
    parent = BASE[PARENT[name]] if PARENT[name] else I3
    delta = I3
    if name in AIMS:
        delta = (parent @ DIR[name]).rotation_difference(Vector(AIMS[name]).normalized()).to_matrix()
    if name in MEMBRANE and name in AIMS:  # the bone onto its aim, its membrane level (the side of level nearest the shortest turn: no flips)
        d, n, aim = parent @ DIR[name], parent @ MEMBRANE[name], Vector(AIMS[name]).normalized()
        up = Vector((0, 0, 1))
        up = (up - aim * up.dot(aim)).normalized()
        if (delta @ n).dot(up) < 0:
            up = -up
        n = (n - d * n.dot(d)).normalized()
        delta = Matrix((aim, up, aim.cross(up))).transposed() @ Matrix((d, n, d.cross(n)))
    BASE_DELTA[name] = delta
    BASE[name] = delta @ parent

# --- animation ----------------------------------------------------------------------------------------------------------------
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"


def pose(frame, turns, root_turn=I3, lift=0.0):
    """Keys the flying pose at `frame` with `turns` (bone -> world rotation about its head) on top; children follow their parents and
    keep their own bend of the flying pose. root_turn tips the whole dragon about the hips; lift raises it."""
    world = {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent = world[PARENT[name]] if PARENT[name] else I3
        extra = parent @ (BASE[PARENT[name]] if PARENT[name] else I3).inverted()
        delta = turns.get(name, I3) @ extra @ BASE_DELTA[name] @ extra.inverted()
        if name == "Root":
            delta = root_turn
        world[name] = delta @ parent
        pb.rotation_quaternion = (REST[name].inverted() @ parent.inverted() @ delta @ parent @ REST[name]).to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == "Root":
            pivot = HIPS - BONES["Root"].head_local
            pb.location = REST[name].inverted() @ ((pivot - root_turn @ pivot) + Vector((0, 0, lift)))
            pb.keyframe_insert("location", frame=frame)


def flying(flap=0.0, fold=0.0, sweep=0.0, neck=0.0, head=0.0, jaw=0.0, tail_wave=0.0, legs=0.0, lift=0.0, pitch=0.0):
    """flap: wings up (+) or down (-) in degrees; fold: the forearm bends up (+) from the arm; sweep: the fingers swing back (+);
    neck/head: extra nose-up; jaw: mouth open; tail_wave: side to side; legs: tucked tighter (+); pitch: nose-down of the body."""
    turns = {"jaw": ry(jaw), NECK[0]: ry(-neck * 0.5), NECK[2]: ry(-neck * 0.5), NECK[4]: ry(-head)}
    for i, name in enumerate(TAIL[1:]):
        turns[name] = rz(tail_wave * math.sin(i * 0.6) / 4)
    for side, s in (("L", 1), ("R", -1)):
        arm_bone, forearm, fingers = WINGS[side]
        turns[arm_bone] = rx(s * flap)
        turns[forearm] = rx(s * (flap * 0.5 + fold))
        for i, finger in enumerate(fingers):
            turns[finger] = rz(-s * sweep * (1 - i * 0.2)) @ rx(s * flap * 0.3)
    for left, right, _ in LEGS.values():
        for bones in (left, right):
            turns[bones[0]] = ry(-legs)
    return dict(turns=turns, root_turn=ry(pitch), lift=lift)


def key(frame, **kwargs):
    pose(frame, **flying(**kwargs))


def new_action(name, frames):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = action
    scene.frame_start, scene.frame_end = 0, frames
    return action


def use_action(action):
    """Blender 5 actions are slotted: making one current also needs its slot assigned."""
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]


def idle():   # a hover: slow, shallow beats
    for f in range(0, 61, 3):
        s = math.sin(2 * math.pi * f / 60)
        key(f, flap=14 * s, fold=-8 * s, sweep=4 * s, neck=3 * s, tail_wave=8 * s, lift=0.04 * s)


def flap():   # the working beat: wings high to low, the body rising on the downstroke
    for f in range(0, 19):
        s = math.sin(2 * math.pi * f / 18)
        key(f, flap=38 * s, fold=-22 * s, sweep=10 * s, neck=-4 * s, legs=-6 * s, tail_wave=5 * s, lift=-0.12 * s)


def glide():  # wings out and steady, a slow sway
    for f in range(0, 49, 2):
        s = math.sin(2 * math.pi * f / 48)
        key(f, flap=4 * s - 6, fold=3 * s - 4, sweep=-6, tail_wave=10 * s)


def breath():  # rears the head back and opens wide (0-10), thrusts and holds the mouth open for the flames (10-40), then closes
    for f in range(0, 49, 2):
        draw = smooth(f / 10) * (1 - smooth((f - 40) / 8))
        thrust = smooth((f - 10) / 3) * (1 - smooth((f - 40) / 8))
        key(f, flap=3 * math.sin(f / 6.0), neck=30 * (draw - thrust), head=20 * draw - 25 * thrust, jaw=22 * draw + 10 * thrust, lift=0.02 * thrust)


CLIPS = (("Idle", 60, idle), ("Flap", 18, flap), ("Glide", 48, glide), ("Breath", 48, breath))
actions = {}
for name, frames, build in CLIPS:
    action = new_action(name, frames)
    build()
    actions[name] = action
    print(f"[build_dragon] clip {name}: {frames} frames")

# --- the tail's shape ----------------------------------------------------------------------------------------------------------
# Tripo's tail pinched just behind the hips and bulged a third of the way along (where its curl began), and its club was a flattened
# disc. Measured in the flying pose, where the tail is straight (in the coiled rest pose, and by Tripo's off-centre bones, a bone's own
# vertices are a patch on one side, not a ring): rings of the mesh along the tail, each one's middle and thickness. Each vertex moves
# about its ring's middle to a thickness tapering evenly from the thickest of the tail's first half to the end before the club, and each
# move is taken back to the rest pose through the vertex's own skinning. Tripo's club is shrunk inside a new spiked ball (below).
CLUB = NEW_TAIL[-1]
use_action(actions["Glide"])
scene.frame_set(0)
bpy.context.view_layer.update()
evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
mesh = evaluated.to_mesh()
posed = [v.co.copy() for v in mesh.vertices]
area = [0.0] * len(posed)  # each vertex's share of the skin around it: the spikes' dense little faces must not outweigh the tail's skin
for face in mesh.polygons:
    for index in face.vertices:
        area[index] += face.area / len(face.vertices)
evaluated.to_mesh_clear()
names = {g.index: g.name for g in body.vertex_groups}
deform = {pb.name: pb.matrix @ pb.bone.matrix_local.inverted() for pb in arm.pose.bones}
chain = [(n, arm.pose.bones[n].head.copy(), arm.pose.bones[n].tail.copy()) for n in TAIL]
starts, along = {}, 0.0
for n, a, b in chain:
    starts[n] = along
    along += (b - a).length
club_from = starts[CLUB]


def on_tail(p):
    """How far along the posed tail p's nearest point is, and that point."""
    best = None
    for n, a, b in chain:
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
        foot = a + ab * t
        if best is None or (p - foot).length < best[0]:
            best = ((p - foot).length, starts[n] + t * ab.length, foot)
    return best[1], best[2]


share = []  # each vertex's share of weight on the tail's bones (0 off the tail) and on the club
for v in body.data.vertices:
    total = sum(g.weight for g in v.groups) or 1.0
    share.append((sum(g.weight for g in v.groups if names[g.group] in TAIL) / total, sum(g.weight for g in v.groups if names[g.group] == CLUB) / total))
STEP_S = 0.02
rings, place = {}, {}
for v, p in zip(body.data.vertices, posed):
    if share[v.index][0] > 0.02:
        s, foot = on_tail(p)
        place[v.index] = (s, foot)
        if share[v.index][0] > 0.9 and s < club_from:
            rings.setdefault(int(s / STEP_S), []).append((p - foot, area[v.index]))


def by_area(points, keep):
    """The skin-weighted middle of the offsets nearest it, `keep` of the skin (without the spikes)."""
    middle = sum((q * a for q, a in points), Vector()) / sum(a for _, a in points)
    nearest, kept = [], 0.0
    for q, a in sorted(points, key=lambda qa: (qa[0] - middle).length):
        if kept > keep * sum(a for _, a in points):
            break
        nearest.append((q, a))
        kept += a
    return sum((q * a for q, a in nearest), Vector()) / kept


def median_size(points, middle):
    half, seen = sum(a for _, a in points) / 2, 0.0
    for q, a in sorted(points, key=lambda qa: (qa[0] - middle).length):
        seen += a
        if seen >= half:
            return (q - middle).length


ring = {}  # bin -> (its middle, off the bones, and its thickness)
for k, points in rings.items():
    middle = by_area(points, 0.8)
    ring[k] = (middle, median_size(points, middle))
bins = sorted(ring)
first, last = bins[0], bins[-1]
thickest = max(ring[k][1] for k in bins if k <= (first + last) // 2)
end = ring[last][1]


def ring_at(s):
    k = min(max(int(s / STEP_S), first), last)
    near = [ring[j] for j in (k - 1, k, k + 1) if j in ring]  # smoothed over its neighbours
    return sum((m for m, _ in near), Vector()) / len(near), sum(t for _, t in near) / len(near)


def wanted(s):
    return thickest + (end - thickest) * max(0.0, min(1.0, (s - first * STEP_S) / ((last - first) * STEP_S)))


club_axis = (chain[-1][2] - chain[-1][1]).normalized()
core = [(p, area[v.index]) for v, p in zip(body.data.vertices, posed) if share[v.index][1] > 0.9]
club_middle = by_area(core, 0.8)
for v, p in zip(body.data.vertices, posed):
    if v.index not in place:
        continue
    tail_share, club_share = share[v.index]
    s, foot = place[v.index]
    middle, size = ring_at(min(s, club_from))
    q = p - foot
    shaped = foot + middle + (q - middle) * max(0.6, min(2.0, wanted(min(s, club_from)) / max(size, 1e-4)))
    hidden = club_middle + (p - club_middle) * 0.4  # Tripo's club, shrunk inside the new ball
    move = (shaped - p) * (tail_share - club_share) + (hidden - p) * club_share
    total = sum(g.weight for g in v.groups) or 1.0
    skin = sum((deform[names[g.group]] * (g.weight / total) for g in v.groups if names[g.group] in deform), Matrix.Diagonal((0, 0, 0, 0)))
    v.co += skin.to_3x3().inverted_safe() @ move
print(f"[build_dragon] tail: {thickest:.3f} thick at its start tapering to {end:.3f}")
print("[build_dragon] tail rings (thickness):", " ".join(f"{ring[k][1]:.3f}" for k in bins))

# The club: a ball BALL_RADIUS across (the tail's start is 0.046 thick) set with conical spikes, one from each face of an icosahedron
# and each of its corners (none into the tail), on the club bone alone, textured with the middle of Tripo's club's patch of the skin.
BALL_RADIUS, SPIKE_LENGTH, SPIKE_WIDTH = 0.07, 0.06, 0.016
club_bone = arm.data.bones[CLUB]
centre = deform[CLUB].inverted() @ club_middle  # in the rest pose
to_rest = club_bone.matrix_local.to_3x3() @ arm.pose.bones[CLUB].matrix.to_3x3().inverted()  # posed directions into rest ones
inward = (to_rest @ -club_axis).normalized()  # towards the tail
uv_name = body.data.uv_layers.active.name
club_uvs = [body.data.uv_layers.active.data[loop.index].uv.copy() for face in body.data.polygons for loop in (body.data.loops[i] for i in face.loop_indices)
            if share[loop.vertex_index][1] > 0.9]
us, vs = sorted(uv.x for uv in club_uvs), sorted(uv.y for uv in club_uvs)
u0, u1, v0, v1 = us[len(us) * 2 // 5], us[len(us) * 3 // 5], vs[len(vs) * 2 // 5], vs[len(vs) * 3 // 5]  # the middle: its edges are pale
bm = bmesh.new()
bmesh.ops.create_icosphere(bm, subdivisions=3, radius=BALL_RADIUS)
for face in bm.faces:
    face.smooth = True
ico = bmesh.new()
bmesh.ops.create_icosphere(ico, subdivisions=1, radius=1.0)
ico.faces.ensure_lookup_table()
aims = [v.co.normalized() for v in ico.verts] + [f.calc_center_median().normalized() for f in ico.faces]
ico.free()
spikes = 0
for aim in aims:
    if aim.dot(Vector((0, 0, -1))) > 0.8:  # the ball is built about +Z towards the tail: that one would stab into it
        continue
    turn = Vector((0, 0, 1)).rotation_difference(aim).to_matrix().to_4x4()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=SPIKE_WIDTH, radius2=0.0, depth=SPIKE_LENGTH,
                          matrix=turn @ Matrix.Translation((0, 0, BALL_RADIUS * 0.85 + SPIKE_LENGTH / 2)))
    spikes += 1
uv_layer = bm.loops.layers.uv.new(uv_name)
for face in bm.faces:
    for loop in face.loops:
        d = loop.vert.co.normalized()
        loop[uv_layer].uv = (u0 + (u1 - u0) * (math.atan2(d.y, d.x) / (2 * math.pi) + 0.5), v0 + (v1 - v0) * (math.acos(max(-1.0, min(1.0, d.z))) / math.pi))
bm.transform(Matrix.Translation(centre) @ Vector((0, 0, -1)).rotation_difference(inward).to_matrix().to_4x4())
ball_mesh = bpy.data.meshes.new("Club")
bm.to_mesh(ball_mesh)
bm.free()
for slot in body.data.materials:
    ball_mesh.materials.append(slot)
ball = bpy.data.objects.new("Club", ball_mesh)
scene.collection.objects.link(ball)
ball.vertex_groups.new(name=CLUB).add(list(range(len(ball_mesh.vertices))), 1.0, "REPLACE")
select(body, ball)
bpy.ops.object.join()
print(f"[build_dragon] club: a ball {BALL_RADIUS} across with {spikes} spikes {SPIKE_LENGTH} long")

# --- previews -----------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 560
    cams = {}
    for name, loc, rot, ortho in (("side", (0.0, -5.0, 0.3), (math.radians(90), 0, 0), 3.2), ("front", (5.5, 0.0, 0.3), (math.radians(90), 0, math.radians(90)), 2.0),
                                  ("top", (0.0, 0.0, 6.0), (0, 0, 0), 3.2), ("three", (2.6, -2.6, 1.8), (math.radians(65), 0, math.radians(45)), 0),
                                  ("head", (0.6, -3.0, 0.45), (math.radians(90), 0, 0), 0.6)):
        cam = bpy.data.objects.new("cam_" + name, bpy.data.cameras.new("cam_" + name))
        scene.collection.objects.link(cam)
        cam.location, cam.rotation_euler = loc, rot
        if ortho:
            cam.data.type, cam.data.ortho_scale = "ORTHO", ortho
        cams[name] = cam
    shots = [s for s in os.environ.get("DRAGON_SHOTS", "Glide:0,Flap:5,Flap:14,Breath:8,Breath:26").split(",") if s]
    for shot in shots:
        clip, frame = shot.split(":")
        use_action(actions[clip])
        scene.frame_set(int(frame))
        bpy.context.view_layer.update()
        for view, cam in cams.items():
            scene.camera = cam
            scene.render.filepath = os.path.join(ROOT, "Saved", f"dragon_{clip}_{frame}_{view}.png")
            bpy.ops.render.render(write_still=True)
    # Where a rider sits and where the mouth is, in the glide pose, for the constants in ShinobiDragonMount.cpp.
    use_action(actions["Glide"])
    scene.frame_set(0)
    mesh = body.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    points = [body.matrix_world @ v.co for v in mesh.vertices]
    print("[build_dragon] glide: mesh bounds", tuple(round(min(p[a] for p in points), 2) for a in range(3)), tuple(round(max(p[a] for p in points), 2) for a in range(3)))
    for lo in [x / 10 for x in range(-3, 6)]:
        column = sorted(p.z for p in points if lo < p.x < lo + 0.1 and abs(p.y) < 0.06)
        if column:
            print(f"[build_dragon] glide: x {lo:.1f}: z median {column[len(column) // 2]:.2f}, 85% {column[int(len(column) * 0.85)]:.2f}, top {column[-1]:.2f}")
    snout = max(points, key=lambda p: p.x)
    print("[build_dragon] glide: snout", tuple(round(c, 2) for c in snout))
    raise SystemExit

# --- export -------------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL", path_mode="STRIP",
              mesh_smooth_type="FACE")
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Tairn.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, frames, _ in CLIPS:
    use_action(actions[name])
    scene.frame_start, scene.frame_end = 0, frames
    select(arm, body)  # with the mesh: an armature exported alone is written with its current pose as its rest pose
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Tairn_{name}.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_dragon] wrote Tairn_{name}.fbx")
