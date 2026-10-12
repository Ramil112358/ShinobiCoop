"""Rigs and animates Pabu, the summoned corgi: the Tripo model (SourceArt/Tripo/Pabu, FBX in the zip) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_pabu.py            (add  -- preview  to render the clips to Saved/pabu_*.png)

Tripo's auto-rig placed its joints well but its skin weights are broken (two thirds of the vertices unweighted), so its joints
are kept, renamed and tidied (a pelvis added, the ears given two bones that reach the tips, the head bone aimed at the nose)
and the mesh is weighted again with Blender's automatic weights. Tripo's model stands mid-step, turned 20 degrees off its
axis and tipped nose-up, so it is levelled and then posed square (head forward, tail centred, all four paws planted) and that
pose becomes the rest pose. The legs are driven by IK (non-deforming ik_/pole_ bones, left out of the export) so planted paws
stay put; the clips are keyed here: Idle, Walk (a trot), Run (a gallop), Bite (the strike lands 0.4 s in, UGA_PabuBite's
WindUp), Hit and Death. Writes SourceArt/Creatures/Pabu/Pabu.fbx (mesh + skeleton), Pabu_<Clip>.fbx per clip and Pabu.png
(the 2k base colour). Pabu faces +X, stands on Z = 0, at true corgi size (0.33 m at the withers); metres.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "SourceArt", "Tripo", "Pabu", "fbx", "tripo_convert_2403f13e-24ee-4768-a8f4-22c8a9e0954d.fbx")
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Pabu")
PREVIEW = "preview" in sys.argv
FPS = 30
SCALE = 0.6       # Tripo's model is 0.55 m at the withers; a Pembroke is about 0.3
GAME_SCALE = 1.6  # FCreatureLook::Scale for Pabu in ShinobiSummon.cpp
WALK_SPEED = 0.9  # m/s at true size: the trot
RUN_SPEED = 3.0   # m/s: the gallop (Pabu runs at 650 cm/s in game, 1.35 times this at his 1.6 scale)
I3 = Matrix.Identity(3)

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


def ry(deg):  # about world Y: positive tips the nose (+X) down, swings a hanging tail back, an upright ear forward
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):  # about world Z: positive turns the nose to Pabu's left (+Y)
    return Matrix.Rotation(math.radians(deg), 3, "Z")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# --- fit: face +X at corgi size, levelled -----------------------------------------------------------------------------------
# Tripo's model faces -Y. Its spine runs from the hips to the shoulders 20 degrees off that and climbs 10 degrees: turned and
# tipped back so the shoulders are straight ahead of the hips and level with them.
BASE = Matrix.Scale(SCALE, 4) @ Matrix.Rotation(math.radians(90), 4, "Z") @ tripo.matrix_world
raw = {b.name: (BASE @ b.head_local, BASE @ b.tail_local) for b in tripo.data.bones}
axis = (raw["bone_9"][0] + raw["bone_13"][0]) / 2 - (raw["bone_17"][0] + raw["bone_21"][0]) / 2
yaw, pitch = math.atan2(axis.y, axis.x), math.atan2(axis.z, axis.xy.length)
FIT = Matrix.Rotation(pitch, 4, "Y") @ Matrix.Rotation(-yaw, 4, "Z") @ BASE
joints = {b.name: (FIT @ b.head_local, FIT @ b.tail_local) for b in tripo.data.bones}
middle = ((joints["bone_9"][0] + joints["bone_13"][0] + joints["bone_17"][0] + joints["bone_21"][0]) / 4).xy
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
print("[build_pabu] turned %.1f degrees, tipped %.1f degrees" % (math.degrees(yaw), math.degrees(pitch)))

# --- the texture ----------------------------------------------------------------------------------------------------------
# The base colour, Tripo's normal map (OpenGL-style, green up: setup_creatures.py flips it for Unreal) and its roughness, at 2k.
# Roughness is kept at 0.55 or more: the gaps between the UV islands are 0 and would glint along the seams, and fur is not glossy.
if not PREVIEW:
    for key, out, floor in (("basecolor", "Pabu.png", None), ("normal", "Pabu_Normal.png", None), ("roughness", "Pabu_Roughness.png", 0.55)):
        image = next(i for i in bpy.data.images if key in i.name.lower())
        image.scale(2048, 2048)
        if floor is not None:
            image.pixels = [c if (i % 4) == 3 else max(c, floor) for i, c in enumerate(image.pixels[:])]
        image.filepath_raw = os.path.join(OUT, out)
        image.file_format = "PNG"
        image.save()

# --- the rig ---------------------------------------------------------------------------------------------------------------
verts = [v.co.copy() for v in body.data.vertices]
J = lambda name, end=0: joints[name][end].copy()
nose = max((p for p in verts if p.z > 0.25), key=lambda p: p.x)
ear_tip = {s: max((p for p in verts if p.y * k > 0.02 and p.x > 0.05), key=lambda p: p.z) for s, k in (("L", 1), ("R", -1))}
LEGS = {"FR": ("front", "R", 9, "chest"), "FL": ("front", "L", 13, "chest"), "HR": ("hind", "R", 17, "pelvis"), "HL": ("hind", "L", 21, "pelvis")}

# name: (parent, head, tail, deforms). Tripo's bone_N are the joints it found.
BONES = {"root": (None, Vector(), Vector((0.15, 0, 0)), False)}
hips = (J("bone_17") + J("bone_21")) / 2
BONES["pelvis"] = ("root", hips + Vector((0, 0, 0.03)), J("bone_1"), True)
BONES["spine"] = ("pelvis", J("bone_1"), J("bone_1", 1), True)
BONES["chest"] = ("spine", J("bone_1", 1), J("bone_2", 1), True)
BONES["neck1"] = ("chest", J("bone_3"), J("bone_3", 1), True)
BONES["neck2"] = ("neck1", J("bone_4"), J("bone_4", 1), True)
BONES["head"] = ("neck2", J("bone_5"), J("bone_5").lerp(nose, 0.7), True)
BONES["ear1_R"] = ("head", J("bone_6"), J("bone_6", 1), True)
BONES["ear2_R"] = ("ear1_R", J("bone_6", 1), ear_tip["R"].lerp(J("bone_6", 1), 0.08), True)
ear_mid_L = J("bone_8").lerp(ear_tip["L"], 0.45)
BONES["ear1_L"] = ("head", J("bone_8"), ear_mid_L, True)
BONES["ear2_L"] = ("ear1_L", ear_mid_L, ear_tip["L"].lerp(ear_mid_L, 0.08), True)
for leg, (kind, side, first, parent) in LEGS.items():
    for i, part in enumerate(("upper", "lower", "foot", "toe")):
        up = parent if i == 0 else f"{kind}_{('upper', 'lower', 'foot')[i - 1]}_{side}"
        BONES[f"{kind}_{part}_{side}"] = (up, J(f"bone_{first + i}"), J(f"bone_{first + i}", 1), True)
for i in range(6):
    BONES[f"tail{i + 1}"] = ("pelvis" if i == 0 else f"tail{i}", J(f"bone_{25 + i}"), J(f"bone_{25 + i}", 1), True)
# The IK controls: a target where each paw's foot bone starts (its wrist or hock), shaped like the foot bone so the foot can copy
# its turn, and a pole the elbow (behind) or knee (in front) points at.
for leg, (kind, side, _, parent) in LEGS.items():
    _, head, tail, _ = BONES[f"{kind}_foot_{side}"]
    BONES[f"ik_{leg}"] = ("root", head, tail, False)
    joint = BONES[f"{kind}_lower_{side}"][1]
    pole = joint + Vector((-0.25 if kind == "front" else 0.25, 0, 0))
    BONES[f"pole_{leg}"] = (parent, pole, pole + Vector((0, 0, 0.03)), False)
DEFORM = [n for n, b in BONES.items() if b[3]] + ["root"]

def rig():
    """Builds the armature from BONES and weights the mesh to it by bone heat."""
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
    select(body, arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    return data, arm


data, arm = rig()

# --- the face, turned to the front ------------------------------------------------------------------------------------------
# Tripo's Pabu looks over his right shoulder, as in the photo, with his nose over the middle of his chest. Turning that through
# the skin smears the skull, so the mesh itself is turned: about the nose (which stays put), the skull and ears all the way, the
# neck less and less down to the chest, so the face points straight ahead. Where it points: square to the line between the ears.
# How far each vertex turns goes by how far along the neck it is, from its root (nothing) to just behind the head joint (all of
# it): by the bone weights instead, the eyes (shared with the neck bone) would lag behind the muzzle.
ear_mid = lambda side: (BONES[f"ear1_{side}"][1] + BONES[f"ear1_{side}"][2]) / 2
across = ear_mid("L") - ear_mid("R")
facing = math.degrees(math.atan2(across.y, across.x)) - 90
pivot = nose.copy()
neck_root, atlas = BONES["neck1"][1], BONES["head"][1]
along = atlas - neck_root
spin = Matrix.Rotation(math.radians(-facing), 3, "Z")
shares = [smooth(((v.co - neck_root).dot(along) / along.length_squared - 0.3) / 0.6) for v in body.data.vertices]
skull = [pivot + spin @ (v.co - pivot) for v, share in zip(body.data.vertices, shares) if share > 0.99]
slide = Vector((0, -sum(p.y for p in skull) / len(skull), 0))  # turned about the nose, the skull swings aside: slide it back over the middle
for v, share in zip(body.data.vertices, shares):
    if share > 0:
        v.co = pivot + Matrix.Rotation(math.radians(-facing * share), 3, "Z") @ (v.co - pivot) + slide * share
turned = lambda p: pivot + spin @ (p - pivot) + slide
for name in ("head", "ear1_L", "ear2_L", "ear1_R", "ear2_R"):
    parent, head, tail, deform = BONES[name]
    BONES[name] = (parent, turned(head), turned(tail), deform)
BONES["neck2"] = (BONES["neck2"][0], BONES["neck2"][1], BONES["head"][1].copy(), True)
print("[build_pabu] face turned %.1f degrees to the front and slid %.3f m to the middle" % (-facing, slide.y))
bpy.data.objects.remove(arm)
body.modifiers.clear()
body.vertex_groups.clear()
data, arm = rig()

# --- weights ---------------------------------------------------------------------------------------------------------------
print("[build_pabu] bone-heat weighted", sum(1 for v in body.data.vertices if v.groups), "of", len(body.data.vertices))

# The collar tag is a loose piece: it hangs from the chest. Vertices the bone heat missed copy their nearest weighted neighbour.
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
for g in body.vertex_groups:
    g.remove(loose)
body.vertex_groups["chest"].add(loose, 1.0, "REPLACE")
weighted = [sum(g.weight for g in v.groups) > 0.01 for v in body.data.vertices]
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
print("[build_pabu] collar tag (%d vertices) on the chest; %d unweighted vertices filled from their neighbours" % (len(loose), len(missed)))
data.bones["root"].use_deform = True  # kept in the export as the skeleton's root; it has no weights

# Each paw's vertices: mostly weighted to its foot and toe bones.
names = {g.index: g.name for g in body.vertex_groups}
paw_verts = {}
for leg, (kind, side, _, _) in LEGS.items():
    mine = {f"{kind}_foot_{side}", f"{kind}_toe_{side}"}
    paw_verts[leg] = [v.index for v in body.data.vertices if sum(g.weight for g in v.groups if names[g.group] in mine) > 0.5]

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


pelvis_y = BONES["pelvis"][1].y
tail_tip = BONES["tail6"][2]
tail_root = BONES["tail1"][1]
turn_bone("tail1", rx(math.degrees(math.atan2(tail_tip.y - pelvis_y, tail_root.z - tail_tip.z))))  # hang the tail straight down the middle
# The front legs are a little short for the levelled body: the spine tips down 6 degrees to bring the shoulders over the ground.
turn_bone("spine", ry(6))


turn_bone("head", ry(8))  # Tripo's Pabu gazes up: lower the nose a little
front_x = (BONES["front_upper_R"][1].x + BONES["front_upper_L"][1].x) / 2
hind_x = (BONES["hind_lower_R"][2].x + BONES["hind_lower_L"][2].x) / 2
STANCE = {"FR": Vector((front_x, -0.05, 0)), "FL": Vector((front_x, 0.05, 0)), "HR": Vector((hind_x, -0.055, 0)), "HL": Vector((hind_x, 0.055, 0))}
for leg in LEGS:
    head = data.bones[f"ik_{leg}"].head_local
    STANCE[leg].z = head.z
for step in range(3):  # set each wrist/hock over its spot, then lower or raise it until the paw's sole is on the ground
    for leg in LEGS:
        pb = arm.pose.bones[f"ik_{leg}"]
        pb.location = rest_world(pb.name).inverted() @ (STANCE[leg] - data.bones[pb.name].head_local)
    bpy.context.view_layer.update()
    for leg in LEGS:
        STANCE[leg].z -= paw_bottom(leg)
print("[build_pabu] stance", {k: tuple(round(c, 3) for c in v) for k, v in STANCE.items()})

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
for leg, (kind, side, _, _) in LEGS.items():
    print("[build_pabu] %s: wrist/hock %s, its IK target %s, sole %.3f" % (leg, tuple(round(c, 3) for c in data.bones[f"{kind}_foot_{side}"].head_local),
                                                                     tuple(round(c, 3) for c in data.bones[f"ik_{leg}"].head_local), paw_bottom(leg)))
low = min(v.co.z for v in body.data.vertices)
print("[build_pabu] rest pose: lowest point %.3f m, top %.3f m, length %.3f m" % (low, max(v.co.z for v in body.data.vertices), max(v.co.x for v in body.data.vertices) - min(v.co.x for v in body.data.vertices)))

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


def gait(frame, frames, speed, duty, offsets, lift, flex, body):
    t = frame / frames
    cycle = frames / FPS
    feet = {}
    for leg, offset in offsets.items():
        x, z, u = stride(t + offset, duty, speed * duty * cycle, lift[leg[0]])
        bend = math.sin(math.pi * u) * flex[leg[0]]
        feet[leg] = (Vector((x, 0, z)), ry(bend if leg[0] == "F" else -bend))
    rot, pelvis = body(t)
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


def idle():
    """Three seconds: he breathes, looks left, back, then right, tilts his head the corgi way, swivels and flicks his ears and
    wags his tail the whole time."""
    for f in range(0, 91, 2):
        t = f / 90
        b = math.sin(2 * math.pi * t * 3)  # three breaths
        look = curve(t, [(0, 0), (0.12, 0), (0.25, 28), (0.4, 28), (0.5, 0), (0.58, -22), (0.7, -22), (0.8, 0), (1, 0)])
        tilt = curve(t, [(0, 0), (0.78, 0), (0.84, 16), (0.94, 16), (1, 0)])
        nod = curve(t, [(0, 0), (0.25, -4), (0.4, -4), (0.5, 3), (0.58, 0), (1, 0)])
        listen = look / 28  # the ears swivel towards where he looks
        wag = lambda lag: math.sin(2 * math.pi * (t * 6 - lag))  # two wags a second, the tip a beat behind the root
        rot = {"chest": ry(-1.2 * b), "spine": ry(0.6 * b), "pelvis": rx(1.5 * math.sin(2 * math.pi * t)),
               "neck1": rz(0.4 * look), "neck2": rz(0.2 * look) @ ry(nod),
               "head": rz(0.4 * look) @ rx(tilt) @ ry(1.5 * math.sin(4 * math.pi * t)),
               "ear1_L": rz(10 * listen) @ rx(-22 * twitch(t, 0.2) - 14 * twitch(t, 0.66)) @ ry(-12 * twitch(t, 0.2)),
               "ear2_L": rx(-12 * twitch(t, 0.2, 0.06)),
               "ear1_R": rz(10 * listen) @ rx(20 * twitch(t, 0.42) + 16 * twitch(t, 0.86)) @ ry(-10 * twitch(t, 0.86) - 3 * b),
               "ear2_R": rx(12 * twitch(t, 0.42, 0.06)),
               "tail1": rx(18 * wag(0)), "tail2": rx(10 * wag(0.08)), "tail3": rx(12 * wag(0.16)), "tail5": rx(10 * wag(0.28))}
        key(f, rot, Vector((0, 0.004 * math.sin(2 * math.pi * t), 0.003 * b)))


def walk():
    frames = 12  # a trot: diagonal pairs together, 2.5 strides a second
    def body(t):
        bob = math.cos(4 * math.pi * t)
        sway = math.sin(2 * math.pi * t)
        return ({"pelvis": rx(2.5 * sway), "chest": rx(-2.5 * sway), "head": ry(2 * bob), "neck1": ry(-2 * bob),
                 "tail1": rx(12 * sway), "tail3": rx(8 * sway), "ear1_L": ry(-6 + 4 * bob), "ear1_R": ry(-6 + 4 * bob)},
                Vector((0, 0, -0.006 * bob)))
    for f in range(frames + 1):
        gait(f, frames, WALK_SPEED, 0.55, {"FL": 0.0, "HR": 0.0, "FR": 0.5, "HL": 0.5}, {"F": 0.045, "H": 0.04}, {"F": 70, "H": 25}, body)
    return frames


def run():
    frames = 9  # a gallop: the hind paws push off together, then the front paws reach, with time in the air
    def body(t):
        s = math.sin(2 * math.pi * t)  # + : gathered (back rounded), - : stretched out
        c = math.cos(2 * math.pi * t)
        return ({"pelvis": ry(-8 * s), "spine": ry(5 * s), "chest": ry(6 * s), "neck1": ry(-8 * s), "head": ry(-4 * c),
                 "ear1_L": ry(-35), "ear1_R": ry(-35), "ear2_L": ry(-15), "ear2_R": ry(-15), "tail1": ry(55 + 8 * c), "tail3": ry(12 * c),
                 "neck2": ry(12)},
                Vector((0.012 * s, 0, 0.025 * max(0.0, -c) - 0.012)))
    for f in range(frames + 1):
        gait(f, frames, RUN_SPEED, 0.3, {"HL": 0.0, "HR": 0.08, "FL": 0.45, "FR": 0.53}, {"F": 0.07, "H": 0.06}, {"F": 90, "H": 35}, body)
    return frames


def bite():
    frames = 22  # 0.73 s; the jaws close at frame 12 (0.4 s)
    for f in range(frames + 1):
        back = smooth(f / 9) if f <= 9 else 1 - smooth((f - 9) / 3)  # draws back, then throws itself forward
        lunge = smooth((f - 9) / 3) if f <= 14 else 1 - smooth((f - 14) / 8)
        shake = math.sin(2 * math.pi * (f - 12) / 5) * (1 - smooth((f - 12) / 8)) if f >= 12 else 0.0
        rot = {"pelvis": ry(-4 * back), "spine": ry(6 * back - 3 * lunge), "chest": ry(8 * back + 6 * lunge),
               "neck1": ry(-14 * back + 30 * lunge), "neck2": ry(12 * lunge), "head": ry(-12 * back + 14 * lunge) @ rx(22 * shake),
               "ear1_L": ry(-35 * back - 25 * lunge), "ear1_R": ry(-35 * back - 25 * lunge), "tail1": ry(15 * back + 25 * lunge)}
        pelvis = Vector((-0.06 * back + 0.09 * lunge, 0, -0.035 * back - 0.015 * lunge))
        hop = math.sin(math.pi * min(1.0, max(0.0, (f - 9) / 5)))
        key(f, rot, pelvis, {"FL": (Vector((0.07 * lunge, 0, 0.035 * hop)), ry(30 * hop)), "FR": (Vector((0.07 * lunge, 0, 0.035 * hop)), ry(30 * hop))})
    return frames


def hit():
    frames = 15
    for f in range(frames + 1):
        b = smooth(f / 3) if f < 3 else 1 - smooth((f - 3) / 12)
        rot = {"pelvis": rz(8 * b), "chest": ry(-12 * b) @ rx(10 * b), "neck1": ry(-16 * b) @ rz(-10 * b), "head": rz(-18 * b) @ ry(-10 * b),
               "ear1_L": ry(-50 * b), "ear1_R": ry(-50 * b), "ear2_L": ry(-20 * b), "ear2_R": ry(-20 * b), "tail1": ry(-35 * b)}
        lift = math.sin(math.pi * min(1.0, f / 6))
        key(f, rot, Vector((-0.06 * b, 0.025 * b, -0.03 * b)),
            {"FL": (Vector((-0.04 * b, 0.01 * b, 0.03 * lift)), I3), "FR": (Vector((-0.04 * b, 0.01 * b, 0.02 * lift)), I3)})
    return frames


def death():
    frames = 45  # the legs give, he rolls onto his right side and lies still
    for f in range(0, frames + 1, 2):
        sag = smooth(f / 10)
        roll = smooth((f - 6) / 18)
        settle = smooth((f - 24) / 12)
        rot = {"neck1": ry(6 * sag * (1 - roll) - 22 * roll), "head": ry(6 * sag * (1 - roll) - 12 * roll), "ear1_L": ry(-25 * sag), "ear1_R": ry(-25 * sag),
               "tail1": ry(30 * roll)}
        for leg, (kind, side, _, _) in LEGS.items():
            rot[f"{kind}_upper_{side}"] = ry((-30 if kind == "front" else 35) * settle)
            rot[f"{kind}_lower_{side}"] = ry((25 if kind == "front" else -30) * settle)
        key(f, rot, Vector((0, 0, -0.06 * sag * (1 - roll))), ik=1 - smooth((f - 12) / 12), root_rot=rx(88 * roll), pivot=Vector((0, -0.09, 0)))
    return frames


CLIPS = (("Idle", idle), ("Walk", walk), ("Run", run), ("Bite", bite), ("Hit", hit), ("Death", death))
actions = {}
for name, build in CLIPS:
    action = new_action(name, 90)
    frames = build() or 90
    scene.frame_end = frames
    actions[name] = (action, frames)
    print(f"[build_pabu] clip {name}: {frames + 1} frames ({frames / FPS:.2f} s)")
print("[build_pabu] ground speeds at play rate 1, in game cm/s (times the %.1f scale): walk %.0f, run %.0f" % (GAME_SCALE, WALK_SPEED * 100 * GAME_SCALE, RUN_SPEED * 100 * GAME_SCALE))

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
    cam.data.ortho_scale = 0.8
    bpy.ops.mesh.primitive_plane_add(size=0.002, location=(0, 0, 0))  # a ground line
    ground = bpy.context.object
    ground.scale = (600, 600, 1)
    centre = Vector((0, 0, 0.22))
    only = [a for a in sys.argv[sys.argv.index("--") + 1:] if a != "preview"] if "--" in sys.argv else []
    for clip, (action, frames) in actions.items():
        if only and clip not in only:
            continue
        use_action(action)
        for view, d in (("side", (0.0001, -1, 0.05)), ("quarter", (1, -1, 0.5)), ("front", (1, 0.0001, 0.15)), ("face", (1, 0.0001, 0.2))):
            for f in sorted({0, frames // 4, frames // 2, 3 * frames // 4, frames} | ({12} if clip == "Bite" else set()) | ({30, 57, 80} if clip == "Idle" else set())):
                scene.frame_set(f)
                cam.data.ortho_scale = 0.4 if view == "face" else 0.8
                cam.location = (Vector((0.2, 0, 0.33)) if view == "face" else centre) + Vector(d).normalized() * 3
                cam.rotation_euler = Vector(d).normalized().to_track_quat("Z", "Y").to_euler()
                scene.render.filepath = os.path.join(ROOT, "Saved", f"pabu_{clip}_{view}_{f:02d}.png")
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
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Pabu.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, (action, frames) in actions.items():
    use_action(action)
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Pabu_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_pabu] wrote Pabu_{name}.fbx")
