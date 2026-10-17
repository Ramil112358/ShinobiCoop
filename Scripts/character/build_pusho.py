"""Rigs and animates Pusho, the summoned cat: the Tripo model (SourceArt/Tripo/Cat, FBX from cat_rigged.zip) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_pusho.py            (add  -- preview [Clip ...]  to render clips to Saved/pusho_*.png)

Tripo's quadruped auto-rig found good joints but its weights are unusable (one head bone owns half the body, the front legs hang
from the neck), so, as for Pabu (build_pabu.py), its joints are kept, renamed and re-parented (shoulders on the chest, ears
reaching their tips) and the mesh is weighted again by bone heat; the whiskers and other loose bits copy the weights of the
nearest skin. Tripo's model stands turned about 34 degrees off its axis: it is turned straight, then posed square (all four
paws planted under the body) and that pose becomes the rest pose. The legs are driven by IK (non-deforming ik_/pole_ bones, left
out of the export). Clips: Idle, Walk (a cat's walk: hind paw then the front paw on that side), Run (a gallop), Pounce (crouch
and wiggle, the leap, a claw swipe on landing: UGA_PushoPounce's WindUp + LeapSeconds + Recovery), Hit and Death. Writes
SourceArt/Creatures/Pusho/Pusho.fbx (mesh + skeleton), Pusho_<Clip>.fbx per clip and Pusho.png (the 2k base colour). Pusho
faces +X, stands on Z = 0, at true cat size; metres.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "SourceArt", "Tripo", "Cat", "fbx", "tripo_convert_a6bb6955-c1e2-4c7b-bc4d-45ecc782e24a.fbx")
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Pusho")
PREVIEW = "preview" in sys.argv
FPS = 30
SCALE = 0.5        # Tripo's model is 0.73 m to the ear tips; a big long-haired cat is about 0.36
GAME_SCALE = 2.75  # FCreatureLook::Scale for Pusho in ShinobiSummon.cpp: about 1 m to the ear tips
WALK_SPEED = 0.6   # m/s at true size
RUN_SPEED = 2.2    # m/s: the gallop
# UGA_PushoPounce: the crouch, the leap and the recovery after the claw, in seconds. The clip lasts their sum.
POUNCE_WINDUP, POUNCE_LEAP, POUNCE_RECOVERY = 0.45, 0.6, 0.45
I3 = Matrix.Identity(3)
T = lambda name: "tripo::" + name

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
scene = bpy.context.scene
scene.render.fps = FPS
tripo = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
body.name = "Body"
os.makedirs(OUT, exist_ok=True)


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def rx(deg):  # about world X: positive rolls the left side (+Y) up
    return Matrix.Rotation(math.radians(deg), 3, "X")


def ry(deg):  # about world Y: positive tips the nose (+X) down and raises a tail that points back (-X)
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):  # about world Z: positive turns the nose to Pusho's left (+Y)
    return Matrix.Rotation(math.radians(deg), 3, "Z")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# --- fit: face +X at cat size, levelled -------------------------------------------------------------------------------------
# Tripo's model faces -Y, its spine (hips to shoulders) turned about 34 degrees off that.
BASE = Matrix.Scale(SCALE, 4) @ Matrix.Rotation(math.radians(90), 4, "Z") @ tripo.matrix_world
raw = {b.name: (BASE @ b.head_local, BASE @ b.tail_local) for b in tripo.data.bones}
shoulders = (raw["bone_17"][0] + raw["bone_22"][0]) / 2
hips = (raw[T("1_Left_Limb_0")][0] + raw[T("1_Right_Limb_0")][0]) / 2
axis = shoulders - hips
yaw, pitch = math.atan2(axis.y, axis.x), math.atan2(axis.z, axis.xy.length)
FIT = Matrix.Rotation(pitch, 4, "Y") @ Matrix.Rotation(-yaw, 4, "Z") @ BASE
joints = {b.name: (FIT @ b.head_local, FIT @ b.tail_local) for b in tripo.data.bones}
LEG_TOPS = (T("0_Left_Limb_0"), T("0_Right_Limb_0"), T("1_Left_Limb_0"), T("1_Right_Limb_0"))
middle = (sum((joints[n][0] for n in LEG_TOPS), Vector()) / 4).xy
body.parent = None
body.matrix_world = FIT @ tripo.matrix_world.inverted() @ body.matrix_world
body.modifiers.clear()
body.vertex_groups.clear()
bpy.data.objects.remove(tripo)
select(body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
drop = min(v.co.z for v in body.data.vertices)
shift = Vector((-middle.x, -middle.y, -drop))
body.data.transform(Matrix.Translation(shift))
joints = {k: (h + shift, t + shift) for k, (h, t) in joints.items()}
print("[build_pusho] turned %.1f degrees, tipped %.1f degrees" % (math.degrees(yaw), math.degrees(pitch)))

# --- the texture ----------------------------------------------------------------------------------------------------------
if not PREVIEW:
    image = next(i for i in bpy.data.images if "basecolor" in i.name.lower())
    image.scale(2048, 2048)
    image.filepath_raw = os.path.join(OUT, "Pusho.png")
    image.file_format = "PNG"
    image.save()

# --- the skin and its loose bits ------------------------------------------------------------------------------------------
# The whiskers (and any other loose pieces) are separate islands: bone heat leaves them unweighted or weights them badly, and
# the nose and ear tips must be found on the skin, not on a whisker.
bm = bmesh.new()
bm.from_mesh(body.data)
bm.verts.ensure_lookup_table()
island, sizes = [-1] * len(bm.verts), []
for v in bm.verts:
    if island[v.index] >= 0:
        continue
    stack, island[v.index], n = [v], len(sizes), 0
    while stack:
        c = stack.pop()
        n += 1
        for e in c.link_edges:
            o = e.other_vert(c)
            if island[o.index] < 0:
                island[o.index] = len(sizes)
                stack.append(o)
    sizes.append(n)
bm.free()
main = max(range(len(sizes)), key=lambda i: sizes[i])
loose = [i for i, k in enumerate(island) if k != main]
print("[build_pusho] %d islands; %d loose vertices (whiskers)" % (len(sizes), len(loose)))

# --- the rig ---------------------------------------------------------------------------------------------------------------
skin = [v.co.copy() for v, k in zip(body.data.vertices, island) if k == main]
J = lambda name, end=0: joints[name][end].copy()
head_joint = J(T("Head_1"))
nose = max((p for p in skin if p.z > head_joint.z - 0.04), key=lambda p: p.x)
# The ears: the two upward bones on the head (Tripo's bone_15 and bone_16); left is +Y.
ear_bones = sorted(("bone_15", "bone_16"), key=lambda n: J(n).y)
EAR = {"R": ear_bones[0], "L": ear_bones[1]}
ear_tip = {s: max((p for p in skin if (p.xy - J(EAR[s], 1).xy).length < 0.035 and p.z > J(EAR[s]).z), key=lambda p: p.z) for s in "LR"}
# Tripo's limbs: N_Side_Limb_0..3, N = 0 front, 1 hind. Front legs hang from a shoulder blade (bone_17 left, bone_22 right).
front_side = lambda n: "L" if J(n).y > 0 else "R"
SHOULDER = {front_side("bone_17"): "bone_17", front_side("bone_22"): "bone_22"}
LEGS = {"FR": ("front", "R", T("0_Right_Limb_"), "shoulder_R"), "FL": ("front", "L", T("0_Left_Limb_"), "shoulder_L"),
        "HR": ("hind", "R", T("1_Right_Limb_"), "pelvis"), "HL": ("hind", "L", T("1_Left_Limb_"), "pelvis")}

# name: (parent, head, tail, deforms).
BONES = {"root": (None, Vector(), Vector((0.15, 0, 0)), False)}
hip_mid = (J(T("1_Left_Limb_0")) + J(T("1_Right_Limb_0"))) / 2
BONES["pelvis"] = ("root", hip_mid + Vector((0, 0, 0.01)), J(T("Spine_1")), True)
BONES["spine"] = ("pelvis", J(T("Spine_1")), J(T("Spine_1"), 1), True)
BONES["chest"] = ("spine", J(T("Spine_1"), 1), J(T("Spine_2"), 1), True)
BONES["neck"] = ("chest", J(T("Spine_2"), 1), head_joint, True)
BONES["head"] = ("neck", head_joint, head_joint.lerp(nose, 0.7), True)
for s in "LR":
    base = J(EAR[s])
    mid = base.lerp(ear_tip[s], 0.45)
    BONES[f"ear1_{s}"] = ("head", base, mid, True)
    BONES[f"ear2_{s}"] = (f"ear1_{s}", mid, ear_tip[s].lerp(mid, 0.08), True)
    BONES[f"shoulder_{s}"] = ("chest", J(SHOULDER[s]), J(SHOULDER[s], 1), True)
for leg, (kind, side, prefix, parent) in LEGS.items():
    for i, part in enumerate(("upper", "lower", "foot", "toe")):
        up = parent if i == 0 else f"{kind}_{('upper', 'lower', 'foot')[i - 1]}_{side}"
        head, tail = J(prefix + str(i)), J(prefix + str(i), 1)
        if part == "toe":
            tail.z = max(tail.z, 0.004)  # Tripo's hind toes reach below the ground
        BONES[f"{kind}_{part}_{side}"] = (up, head, tail, True)
for i in range(5):
    BONES[f"tail{i + 1}"] = ("pelvis" if i == 0 else f"tail{i}", J(f"bone_{35 + i}"), J(f"bone_{35 + i}", 1), True)
# The IK controls: a target where each paw's foot bone starts (its wrist or hock), shaped like the foot bone so the foot can copy
# its turn, and a pole the elbow (behind) or knee (in front) points at.
for leg, (kind, side, _, parent) in LEGS.items():
    _, head, tail, _ = BONES[f"{kind}_foot_{side}"]
    BONES[f"ik_{leg}"] = ("root", head, tail, False)
    joint = BONES[f"{kind}_lower_{side}"][1]
    pole = joint + Vector((-0.2 if kind == "front" else 0.2, 0, 0))
    BONES[f"pole_{leg}"] = (parent, pole, pole + Vector((0, 0, 0.03)), False)


def rig():
    """Builds the armature from BONES and weights the mesh to it by bone heat. The fur mesh itself defeats bone heat (it finds no
    solution for any bone), so the heat runs on a watertight voxel copy of the skin and the weights are copied back from it."""
    data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", data)
    scene.collection.objects.link(arm)
    select(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    for name, (parent, head, tail, deform) in BONES.items():
        edit = data.edit_bones.new(name)
        edit.head, edit.tail = head, tail
        edit.roll = 0.0
        if parent:
            edit.parent = data.edit_bones[parent]
        edit.use_deform = deform
    for leg, (kind, side, _, _) in LEGS.items():  # the target shares the foot's roll, so copying its turn changes nothing at rest
        data.edit_bones[f"ik_{leg}"].roll = data.edit_bones[f"{kind}_foot_{side}"].roll
    bpy.ops.object.mode_set(mode="OBJECT")
    proxy = bpy.data.objects.new("Proxy", body.data.copy())
    scene.collection.objects.link(proxy)
    pm = bmesh.new()
    pm.from_mesh(proxy.data)
    pm.verts.ensure_lookup_table()
    bmesh.ops.delete(pm, geom=[pm.verts[i] for i in loose], context="VERTS")
    pm.to_mesh(proxy.data)
    pm.free()
    remesh = proxy.modifiers.new("Remesh", "REMESH")
    remesh.mode, remesh.voxel_size = "VOXEL", 0.005
    select(proxy)
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    select(proxy, arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    for g in proxy.vertex_groups:
        body.vertex_groups.new(name=g.name)
    copy = body.modifiers.new("Weights", "DATA_TRANSFER")
    copy.object, copy.use_vert_data, copy.data_types_verts = proxy, True, {"VGROUP_WEIGHTS"}
    copy.vert_mapping = "POLYINTERP_NEAREST"
    copy.layers_vgroup_select_src, copy.layers_vgroup_select_dst = "ALL", "NAME"
    select(body)
    bpy.ops.object.modifier_apply(modifier=copy.name)
    print("[build_pusho] bone heat on a %d-vertex voxel copy: %d of its vertices weighted" % (
        len(proxy.data.vertices), sum(1 for v in proxy.data.vertices if v.groups)))
    bpy.data.objects.remove(proxy)
    body.parent = arm
    body.modifiers.new("Armature", "ARMATURE").object = arm
    return data, arm


# --- the face, turned to the front ------------------------------------------------------------------------------------------
# If Tripo's head looks off to one side, the mesh itself is turned (as for Pabu): about the nose, the skull and ears all the way,
# the neck less and less down to the chest, so the face points straight ahead (square to the line between the ears).
across = BONES["ear1_L"][1] - BONES["ear1_R"][1]
facing = math.degrees(math.atan2(across.y, across.x)) - 90
print("[build_pusho] the face looks %.1f degrees off straight ahead" % facing)
if abs(facing) > 4:
    pivot = nose.copy()
    neck_root, atlas = BONES["neck"][1], BONES["head"][1]
    along = atlas - neck_root
    spin = Matrix.Rotation(math.radians(-facing), 3, "Z")
    shares = [smooth(((v.co - neck_root).dot(along) / along.length_squared - 0.2) / 0.7) for v in body.data.vertices]
    skull = [pivot + spin @ (v.co - pivot) for v, share in zip(body.data.vertices, shares) if share > 0.99]
    slide = Vector((0, -sum(p.y for p in skull) / len(skull), 0))
    for v, share in zip(body.data.vertices, shares):
        if share > 0:
            v.co = pivot + Matrix.Rotation(math.radians(-facing * share), 3, "Z") @ (v.co - pivot) + slide * share
    turned = lambda p: pivot + spin @ (p - pivot) + slide
    for name in ("head", "ear1_L", "ear2_L", "ear1_R", "ear2_R"):
        parent, head, tail, deform = BONES[name]
        BONES[name] = (parent, turned(head), turned(tail), deform)
    BONES["neck"] = (BONES["neck"][0], BONES["neck"][1], BONES["head"][1].copy(), True)
    print("[build_pusho] face turned %.1f degrees to the front and slid %.3f m to the middle" % (-facing, slide.y))
data, arm = rig()

# --- weights ---------------------------------------------------------------------------------------------------------------
print("[build_pusho] bone-heat weighted", sum(1 for v in body.data.vertices if v.groups), "of", len(body.data.vertices))
# The loose bits and any vertex the bone heat missed copy their nearest weighted skin vertex.
for g in body.vertex_groups:
    g.remove(loose)
loose_set = set(loose)
weighted = [i not in loose_set and sum(g.weight for g in v.groups) > 0.01 for i, v in enumerate(body.data.vertices)]
tree = KDTree(sum(weighted))
for v, w in zip(body.data.vertices, weighted):
    if w:
        tree.insert(v.co, v.index)
tree.balance()
missed = [v for v, w in zip(body.data.vertices, weighted) if not w]
for v in missed:
    _, near, _ = tree.find(v.co)
    for g in body.data.vertices[near].groups:
        body.vertex_groups[g.group].add([v.index], g.weight, "REPLACE")
print("[build_pusho] %d vertices (loose or missed) copied their neighbours' weights" % len(missed))
data.bones["root"].use_deform = True  # kept in the export as the skeleton's root; it has no weights

# Each paw's vertices: mostly weighted to its foot and toe bones.
names = {g.index: g.name for g in body.vertex_groups}
paw_verts = {}
for leg, (kind, side, _, _) in LEGS.items():
    mine = {f"{kind}_foot_{side}", f"{kind}_toe_{side}"}
    paw_verts[leg] = [v.index for v in body.data.vertices if v.index not in loose_set and sum(g.weight for g in v.groups if names[g.group] in mine) > 0.5]
    print("[build_pusho] paw %s: %d vertices" % (leg, len(paw_verts[leg])))


# --- IK ---------------------------------------------------------------------------------------------------------------------
def pole_angle(base, tip, pole):
    """The IK pole angle that keeps the chain base->tip bending the way it does at rest (Blender's IK otherwise twists it)."""
    normal = (tip.tail_local - base.head_local).cross(pole - base.head_local)
    projected = normal.cross(base.tail_local - base.head_local)
    x_axis = base.matrix_local.to_3x3().col[0]
    angle = x_axis.angle(projected)
    return -angle if x_axis.cross(projected).dot(base.tail_local - base.head_local) < 0 else angle


def add_ik():
    for leg, (kind, side, _, _) in LEGS.items():
        lower = arm.pose.bones[f"{kind}_lower_{side}"]
        ik = lower.constraints.new("IK")
        ik.name = "IK"
        ik.target, ik.subtarget = arm, f"ik_{leg}"
        ik.pole_target, ik.pole_subtarget = arm, f"pole_{leg}"
        ik.chain_count = 2
        ik.pole_angle = pole_angle(data.bones[f"{kind}_upper_{side}"], data.bones[lower.name], data.bones[f"pole_{leg}"].head_local)
        turn = arm.pose.bones[f"{kind}_foot_{side}"].constraints.new("COPY_ROTATION")
        turn.name = "Paw"
        turn.target, turn.subtarget = arm, f"ik_{leg}"


for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
add_ik()


# --- the square stance, made the rest pose ----------------------------------------------------------------------------------
def rest_world(name):
    return data.bones[name].matrix_local.to_3x3()


def turn_bone(name, delta):
    """Adds a world-axis turn to one bone (its parents unposed)."""
    rest = rest_world(name)
    arm.pose.bones[name].rotation_quaternion = (rest.inverted() @ delta @ rest).to_quaternion()


def paw_bottom(leg):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = body.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    low = min((mesh.vertices[i].co.z for i in paw_verts[leg]))
    evaluated.to_mesh_clear()
    return low


front_x = (BONES["front_upper_R"][1].x + BONES["front_upper_L"][1].x) / 2
hind_x = (BONES["hind_lower_R"][2].x + BONES["hind_lower_L"][2].x) / 2
front_y = (BONES["front_foot_L"][1].y - BONES["front_foot_R"][1].y) / 2 * 0.85
hind_y = (BONES["hind_foot_L"][1].y - BONES["hind_foot_R"][1].y) / 2 * 0.85
STANCE = {"FR": Vector((front_x, -front_y, 0)), "FL": Vector((front_x, front_y, 0)), "HR": Vector((hind_x, -hind_y, 0)), "HL": Vector((hind_x, hind_y, 0))}
for leg in LEGS:
    STANCE[leg].z = data.bones[f"ik_{leg}"].head_local.z
for step in range(3):  # set each wrist/hock over its spot, then lower or raise it until the paw's sole is on the ground
    for leg in LEGS:
        pb = arm.pose.bones[f"ik_{leg}"]
        pb.location = rest_world(pb.name).inverted() @ (STANCE[leg] - data.bones[pb.name].head_local)
    bpy.context.view_layer.update()
    for leg in LEGS:
        STANCE[leg].z -= paw_bottom(leg)
print("[build_pusho] stance", {k: tuple(round(c, 3) for c in v) for k, v in STANCE.items()})

select(arm)
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.select_all(action="SELECT")
bpy.ops.pose.visual_transform_apply()
for pb in arm.pose.bones:
    for c in pb.constraints:
        pb.constraints.remove(c)
bpy.ops.object.mode_set(mode="OBJECT")
select(body)
bpy.ops.object.modifier_apply(modifier=body.modifiers[0].name)
select(arm)
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
body.modifiers.new("Armature", "ARMATURE").object = arm
add_ik()
low = min(v.co.z for v in body.data.vertices)
top = max(v.co.z for v in body.data.vertices)
print("[build_pusho] rest pose: lowest point %.3f m, top %.3f m (%.2f m in game), length %.3f m" % (
    low, top, top * GAME_SCALE, max(v.co.x for v in body.data.vertices) - min(v.co.x for v in body.data.vertices)))

# --- animation -------------------------------------------------------------------------------------------------------------
ORDER = list(BONES)  # parents come before children
PARENT = {n: b[0] for n, b in BONES.items()}
REST = {n: rest_world(n) for n in ORDER}
CONSTRAINED = [(arm.pose.bones[f"{k}_lower_{s}"].constraints["IK"], arm.pose.bones[f"{k}_foot_{s}"].constraints["Paw"]) for k, s, _, _ in LEGS.values()]


def key(frame, rot=None, pelvis=Vector(), feet=None, ik=1.0, root_rot=None, pivot=Vector()):
    """Keys the whole pose at `frame`. rot: bone -> world-axis turn added on top of its parent's (children follow parents).
    pelvis moves the body; feet: leg -> (move of the wrist/hock from its rest spot, turn of the paw); ik < 1 hands the legs back
    to their own (rot) turns; root_rot turns everything about `pivot` (the death roll)."""
    rot, feet = rot or {}, feet or {}
    world = {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent_world = world[PARENT[name]] if PARENT[name] else I3
        if name == "root":
            delta = root_rot or I3
        elif name.startswith("ik_"):
            delta = feet.get(name[3:], (None, I3))[1]
        else:
            delta = rot.get(name, I3)
        world[name] = delta @ parent_world
        pb.rotation_quaternion = (REST[name].inverted() @ parent_world.inverted() @ delta @ parent_world @ REST[name]).to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        move = None
        if name == "root":
            move = pivot - (root_rot or I3) @ pivot
        elif name == "pelvis":
            move = pelvis
        elif name.startswith("ik_"):
            move = feet.get(name[3:], (Vector(), None))[0]
        if move is not None:
            pb.location = REST[name].inverted() @ parent_world.inverted() @ move
            pb.keyframe_insert("location", frame=frame)
    for pair in CONSTRAINED:
        for c in pair:
            c.influence = ik
            c.keyframe_insert("influence", frame=frame)


def new_action(name, frames):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]
    scene.frame_start, scene.frame_end = 0, frames
    return action


def use_action(action):
    """Blender 5 actions are slotted: making one current also needs its slot assigned."""
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]


def stride(phase, duty, sweep, lift):
    """A paw over one cycle (phase 0-1, 0 = it touches down): on the ground for `duty` of it, sliding back `sweep` at the body's
    speed, then lifted `lift` and carried forward. Returns (forward offset, height, how far through the swing 0-1)."""
    phase %= 1.0
    if phase < duty:
        return sweep / 2 - sweep * phase / duty, 0.0, 0.0
    u = (phase - duty) / (1 - duty)
    return -sweep / 2 + sweep * smooth(u), lift * math.sin(math.pi * u), u


def gait(frame, frames, speed, duty, offsets, lift, flex, body_motion):
    t = frame / frames
    cycle = frames / FPS
    feet = {}
    for leg, offset in offsets.items():
        x, z, u = stride(t + offset, duty, speed * duty * cycle, lift[leg[0]])
        bend = math.sin(math.pi * u) * flex[leg[0]]
        feet[leg] = (Vector((x, 0, z)), ry(bend if leg[0] == "F" else -bend))
    rot, pelvis = body_motion(t)
    key(frame, rot, pelvis, feet)


def curve(t, keys):
    """Eases between (time, value) keys, t and the times 0-1; the first and last values should match for a loop."""
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            return v0 + (v1 - v0) * smooth((t - t0) / (t1 - t0))
    return keys[-1][1]


def twitch(t, at, length=0.05):
    """A quick flick at time `at`: up and back over `length` of the clip."""
    return math.sin(math.pi * min(1.0, max(0.0, (t - at) / length)))


def tail(lift, curl, sway=0.0):
    """The tail: raised `lift` degrees at the root, the last three bones curling `curl` degrees each (positive: tip forward
    over a raised tail), swung `sway` degrees sideways."""
    return {"tail1": ry(lift) @ rz(sway), "tail2": rz(0.6 * sway), "tail3": ry(curl), "tail4": ry(curl), "tail5": ry(curl)}


def idle():
    """Three seconds: he breathes, looks left, back, then right, flicks his ears, and the tail lies low with its tip twitching."""
    for f in range(0, 91, 2):
        t = f / 90
        b = math.sin(2 * math.pi * t * 3)  # three breaths
        look = curve(t, [(0, 0), (0.15, 0), (0.28, 30), (0.42, 30), (0.52, 0), (0.6, -24), (0.74, -24), (0.84, 0), (1, 0)])
        nod = curve(t, [(0, 0), (0.28, -5), (0.42, -5), (0.52, 4), (0.6, 0), (1, 0)])
        listen = look / 30
        flick = math.sin(2 * math.pi * t * 4)
        rot = {"chest": ry(-1.2 * b), "spine": ry(0.6 * b), "neck": rz(0.4 * look) @ ry(nod),
               "head": rz(0.6 * look) @ ry(1.5 * math.sin(4 * math.pi * t)),
               "ear1_L": rz(12 * listen) @ rx(-20 * twitch(t, 0.2) - 14 * twitch(t, 0.7)),
               "ear1_R": rz(12 * listen) @ rx(18 * twitch(t, 0.45) + 14 * twitch(t, 0.9))}
        rot.update(tail(-25, 12, 6 * math.sin(2 * math.pi * t)))
        rot["tail5"] = ry(12) @ rz(25 * flick)  # the tip flicks
        key(f, rot, Vector((0, 0, 0.002 * b)))


def walk():
    frames = 24  # a cat's walk: hind paw, then the front paw on the same side; 1.25 strides a second
    def motion(t):
        bob = math.cos(4 * math.pi * t)
        sway = math.sin(2 * math.pi * t)
        rot = {"pelvis": rx(2 * sway), "chest": rx(-2 * sway), "neck": ry(-1.5 * bob), "head": ry(1.5 * bob) @ rz(-2 * sway)}
        rot.update(tail(55, 14, 6 * sway))  # walking, a cat holds its tail up, the tip curled forward
        return rot, Vector((0, 0, -0.004 * bob))
    for f in range(frames + 1):
        gait(f, frames, WALK_SPEED, 0.62, {"HL": 0.0, "FL": 0.25, "HR": 0.5, "FR": 0.75}, {"F": 0.04, "H": 0.035}, {"F": 60, "H": 25}, motion)
    return frames


def run():
    frames = 10  # a gallop: the hind paws push off together, the back stretches, the front paws reach, the back gathers
    def motion(t):
        s = math.sin(2 * math.pi * t)  # + : gathered (back rounded), - : stretched out
        c = math.cos(2 * math.pi * t)
        rot = {"pelvis": ry(-12 * s), "spine": ry(8 * s), "chest": ry(9 * s), "neck": ry(-10 * s), "head": ry(-6 * c),
               "ear1_L": ry(-25), "ear1_R": ry(-25)}
        rot.update(tail(15 + 10 * c, -6))
        return rot, Vector((0.015 * s, 0, 0.022 * max(0.0, -c) - 0.01))
    for f in range(frames + 1):
        gait(f, frames, RUN_SPEED, 0.3, {"HL": 0.0, "HR": 0.08, "FL": 0.45, "FR": 0.53}, {"F": 0.06, "H": 0.05}, {"F": 90, "H": 35}, motion)
    return frames


def pounce():
    """Crouches with his chest low and wiggles his rump, springs (the leap itself is the game's: here the body stretches out, front
    paws reaching), lands on his front paws and swipes with the right one: the claw lands at the end of the leap."""
    wind, leap, rec = (round(s * FPS) for s in (POUNCE_WINDUP, POUNCE_LEAP, POUNCE_RECOVERY))
    frames = wind + leap + rec
    land = wind + leap
    for f in range(frames + 1):
        crouch = smooth(f / (wind * 0.6)) * (1 - smooth((f - wind + 3) / 3))
        wiggle = math.sin(2 * math.pi * (f - wind * 0.5) / 5) * smooth((f - wind * 0.4) / 3) * (1 - smooth((f - wind + 3) / 2))
        air = smooth((f - wind + 2) / 4) * (1 - smooth((f - land + 4) / 4))
        swipe_up = smooth((f - land + 3) / 3) * (1 - smooth((f - land) / 2))
        swipe = smooth((f - land) / 2) * (1 - smooth((f - land - 3) / (rec - 3)))
        rot = {"pelvis": ry(8 * crouch - 6 * air) @ rz(4 * wiggle), "spine": ry(-6 * crouch + 8 * air), "chest": ry(10 * crouch - 6 * air),
               "neck": ry(-14 * crouch - 6 * air), "head": ry(4 * crouch + 6 * air - 8 * swipe), "ear1_L": ry(-30 * (crouch + air)),
               "ear1_R": ry(-30 * (crouch + air))}
        rot.update(tail(-10 + 25 * air - 10 * crouch, 6, 15 * wiggle))
        spread = 0.025 * crouch  # crouched, the elbows go out: paws set wider so the forearms do not cross
        feet = {"FL": (Vector((0.10 * air, spread, 0.05 * air)), ry(-40 * air)),
                "FR": (Vector((0.10 * air + 0.08 * swipe_up - 0.02 * swipe, -spread - 0.04 * swipe, 0.05 * air + 0.13 * swipe_up)), ry(-40 * air - 70 * swipe_up + 30 * swipe)),
                "HL": (Vector((-0.08 * air, 0, 0.04 * air)), ry(50 * air)), "HR": (Vector((-0.08 * air, 0, 0.04 * air)), ry(50 * air))}
        key(f, rot, Vector((-0.02 * crouch, 0, -0.045 * crouch + 0.02 * air)), feet)
    return frames


def hit():
    frames = 15
    for f in range(frames + 1):
        b = smooth(f / 3) if f < 3 else 1 - smooth((f - 3) / 12)
        rot = {"pelvis": rz(8 * b), "chest": ry(-12 * b) @ rx(10 * b), "neck": ry(-16 * b) @ rz(-10 * b), "head": rz(-18 * b) @ ry(-10 * b),
               "ear1_L": ry(-50 * b), "ear1_R": ry(-50 * b), "ear2_L": ry(-20 * b), "ear2_R": ry(-20 * b)}
        rot.update(tail(-20 + 40 * b, 5))
        lift = math.sin(math.pi * min(1.0, f / 6))
        key(f, rot, Vector((-0.05 * b, 0.02 * b, -0.025 * b)),
            {"FL": (Vector((-0.03 * b, 0.01 * b, 0.025 * lift)), I3), "FR": (Vector((-0.03 * b, 0.01 * b, 0.018 * lift)), I3)})
    return frames


def death():
    frames = 45  # the legs give, he rolls onto his right side and lies still
    for f in range(0, frames + 1, 3):
        sag = smooth(f / 10)
        roll = smooth((f - 6) / 18)
        settle = smooth((f - 24) / 12)
        rot = {"neck": ry(6 * sag * (1 - roll) - 22 * roll), "head": ry(6 * sag * (1 - roll) - 12 * roll), "ear1_L": ry(-25 * sag), "ear1_R": ry(-25 * sag)}
        rot.update(tail(-20 * (1 - roll), 4))
        for leg, (kind, side, _, _) in LEGS.items():
            rot[f"{kind}_upper_{side}"] = ry((-30 if kind == "front" else 35) * settle)
            rot[f"{kind}_lower_{side}"] = ry((25 if kind == "front" else -30) * settle)
        key(f, rot, Vector((0, 0, -0.05 * sag * (1 - roll))), ik=1 - smooth((f - 12) / 12), root_rot=rx(88 * roll), pivot=Vector((0, -0.11, 0)))
    return frames


CLIPS = (("Idle", idle), ("Walk", walk), ("Run", run), ("Pounce", pounce), ("Hit", hit), ("Death", death))
actions = {}
for name, build in CLIPS:
    action = new_action(name, 90)
    frames = build() or 90
    scene.frame_end = frames
    actions[name] = (action, frames)
    print(f"[build_pusho] clip {name}: {frames + 1} frames ({frames / FPS:.2f} s)")
print("[build_pusho] ground speeds at play rate 1, in game cm/s (times the %.2f scale): walk %.0f, run %.0f" % (GAME_SCALE, WALK_SPEED * 100 * GAME_SCALE, RUN_SPEED * 100 * GAME_SCALE))

# --- previews --------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 600, 500
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    bpy.ops.mesh.primitive_plane_add(size=0.002, location=(0, 0, 0))  # a ground line
    ground = bpy.context.object
    ground.scale = (600, 600, 1)
    centre = Vector((0.05, 0, 0.18))
    only = [a for a in sys.argv[sys.argv.index("--") + 1:] if a != "preview"] if "--" in sys.argv else []
    for clip, (action, frames) in actions.items():
        if only and clip not in only:
            continue
        use_action(action)
        for view, d in (("side", (0.0001, -1, 0.05)), ("quarter", (1, -1, 0.5)), ("front", (1, 0.0001, 0.15))):
            for f in sorted({0, frames // 4, frames // 2, 3 * frames // 4, frames}
                            | ({round((POUNCE_WINDUP - 0.15) * FPS), round(POUNCE_WINDUP * FPS) + 6, round((POUNCE_WINDUP + POUNCE_LEAP) * FPS) + 1} if clip == "Pounce" else set())):
                scene.frame_set(f)
                cam.data.ortho_scale = 0.75
                cam.location = centre + Vector(d).normalized() * 3
                cam.rotation_euler = Vector(d).normalized().to_track_quat("Z", "Y").to_euler()
                scene.render.filepath = os.path.join(ROOT, "Saved", f"pusho_{clip}_{view}_{f:02d}.png")
                bpy.ops.render.render(write_still=True)
    raise SystemExit

# --- export ----------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL",
              path_mode="COPY", use_armature_deform_only=True)
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Pusho.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, (action, frames) in actions.items():
    use_action(action)
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Pusho_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_pusho] wrote Pusho_{name}.fbx")
