"""Rigs and animates Misha, the summoned plush puppy: the Tripo model (SourceArt/Tripo/Misha, FBX in the zip) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_misha.py            (add  -- preview [Clip...]  to render the clips to Saved/misha_*.png,
                                                                 or  -- rig  to render the joints and weights at rest)

Misha is the user's plush toy and Tripo kept the photo's pose: he sits, front paws planted, hind paws tucked forward, tail along the
ground. He stays seated and gets about the way a toy brought to life would, in seated hops. Tripo's auto-rig is not used (18 joints,
one hind leg a single bone, no ears, three quarters of the vertices unweighted): a rig is built here to his proportions (pelvis, spine,
chest, neck, head, two bones an ear, three a front leg, two a hind leg, four in the tail) and the mesh weighted to it by bone heat. The
front legs are driven by IK (non-deforming ik_/pole_ bones, left out of the export) so planted paws stay put while the body bends.
Clips: Idle, Move (a hop), Bark (the bark goes out 0.5 s in, UGA_MishaBark's WindUp), Hit and Death. Writes
SourceArt/Creatures/Misha/Misha.fbx (mesh + skeleton), Misha_<Clip>.fbx per clip and Misha.png (+ _Normal, _Roughness; 2k).
Misha faces +X, sits on Z = 0 with his body centred over the origin, at the plush's own size (0.74 m to the top of his head); metres.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "SourceArt", "Tripo", "Misha", "fbx", "tripo_convert_043f2845-2be4-4bd2-87ca-b036b6d3ab61.fbx")
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Misha")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEW = "preview" in ARGS
RIG_CHECK = "rig" in ARGS
FPS = 30
GAME_SCALE = 4.5   # FCreatureLook::Scale for Misha in ShinobiSummon.cpp: a giant plush, 3.3 m to the top of his head
HOP = 0.42         # m a hop carries him at plush size
HOP_FRAMES = 18    # one hop: 0.6 s
I3 = Matrix.Identity(3)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
scene = bpy.context.scene
scene.render.fps = FPS
body = next(o for o in bpy.data.objects if o.type == "MESH")
body.name = "Body"
os.makedirs(OUT, exist_ok=True)


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def rx(deg):  # about world X: positive rolls the left side (+Y) up, swings a hanging left ear out (a right one in)
    return Matrix.Rotation(math.radians(deg), 3, "X")


def ry(deg):  # about world Y: positive tips the nose (+X) down, lifts the tail (lying out behind), swings a hanging ear back
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):  # about world Z: positive turns the nose to Misha's left (+Y)
    return Matrix.Rotation(math.radians(deg), 3, "Z")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# --- fit: face +X, body centred, on the ground -----------------------------------------------------------------------------
# Tripo's model faces -Y and already sits level. The tail (behind x = -0.15 before the shift) is left out of the centring so the
# capsule sits round the body, not halfway down the tail.
FIT = Matrix.Rotation(math.radians(90), 4, "Z")
for o in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    bpy.data.objects.remove(o)
body.parent = None
body.matrix_world = FIT @ body.matrix_world
body.modifiers.clear()
body.vertex_groups.clear()
select(body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
points = [v.co for v in body.data.vertices]
torso = [p for p in points if p.x > -0.15]
SHIFT = Vector((-(min(p.x for p in torso) + max(p.x for p in torso)) / 2, -(min(p.y for p in torso) + max(p.y for p in torso)) / 2, -min(p.z for p in points)))
body.data.transform(Matrix.Translation(SHIFT))
print("[build_misha] shifted by", tuple(round(c, 3) for c in SHIFT))

# --- the texture ----------------------------------------------------------------------------------------------------------
# The base colour (4k from Tripo, kept at 2k), its normal map (OpenGL-style, green up: setup_creatures.py flips it for Unreal) and
# its roughness, floored at 0.55: plush is not glossy, and the 0 between the UV islands would glint along the seams.
if not (PREVIEW or RIG_CHECK):
    for key, out, floor in (("basecolor", "Misha.png", None), ("normal", "Misha_Normal.png", None), ("roughness", "Misha_Roughness.png", 0.55)):
        image = next(i for i in bpy.data.images if key in i.name.lower())
        image.scale(2048, 2048)
        if floor is not None:
            image.pixels = [c if (i % 4) == 3 else max(c, floor) for i, c in enumerate(image.pixels[:])]
        image.filepath_raw = os.path.join(OUT, out)
        image.file_format = "PNG"
        image.save()

# --- the rig ---------------------------------------------------------------------------------------------------------------
# Joints are placed on the model as Tripo modelled it (measured before the shift; P() applies it). The tail's are found from the
# mesh: the middle of each slice of it.
P = lambda x, y, z: Vector((x, y, z)) + SHIFT
verts = [v.co.copy() for v in body.data.vertices]
nose = max(verts, key=lambda p: p.x)


def tail_point(x):
    near = [p for p in verts if abs(p.x - (x + SHIFT.x)) < 0.02]
    return Vector((x + SHIFT.x, sum(p.y for p in near) / len(near), (min(p.z for p in near) + max(p.z for p in near)) / 2))


TAIL = [tail_point(x) for x in (-0.13, -0.22, -0.31, -0.40, -0.475)]
SIDES = (("L", 1), ("R", -1))

# name: (parent, head, tail, deforms)
BONES = {"root": (None, Vector(), Vector((0.15, 0, 0)), False)}
BONES["pelvis"] = ("root", P(0.0, 0, 0.08), P(0.05, 0, 0.22), True)
BONES["spine"] = ("pelvis", P(0.05, 0, 0.22), P(0.11, 0, 0.34), True)
BONES["chest"] = ("spine", P(0.11, 0, 0.34), P(0.15, 0, 0.44), True)
BONES["neck"] = ("chest", P(0.15, 0, 0.44), P(0.18, 0, 0.50), True)
BONES["head"] = ("neck", P(0.18, 0, 0.50), P(0.18, 0, 0.50).lerp(nose, 0.75), True)
for s, k in SIDES:
    BONES[f"ear1_{s}"] = ("head", P(0.20, 0.15 * k, 0.64), P(0.21, 0.21 * k, 0.50), True)
    BONES[f"ear2_{s}"] = (f"ear1_{s}", P(0.21, 0.21 * k, 0.50), P(0.20, 0.22 * k, 0.36), True)
    BONES[f"front_upper_{s}"] = ("chest", P(0.24, 0.11 * k, 0.30), P(0.31, 0.12 * k, 0.16), True)
    BONES[f"front_lower_{s}"] = (f"front_upper_{s}", P(0.31, 0.12 * k, 0.16), P(0.35, 0.13 * k, 0.06), True)
    BONES[f"front_paw_{s}"] = (f"front_lower_{s}", P(0.35, 0.13 * k, 0.06), P(0.45, 0.13 * k, 0.04), True)
    BONES[f"hind_thigh_{s}"] = ("pelvis", P(0.0, 0.10 * k, 0.13), P(0.08, 0.17 * k, 0.10), True)
    BONES[f"hind_foot_{s}"] = (f"hind_thigh_{s}", P(0.08, 0.17 * k, 0.10), P(0.21, 0.16 * k, 0.04), True)
for i in range(4):
    BONES[f"tail{i + 1}"] = ("pelvis" if i == 0 else f"tail{i}", TAIL[i], TAIL[i + 1], True)
# The front paws' IK: a target where each paw bone starts (the wrist), shaped like it so the paw can copy its turn, and a pole
# behind the elbow.
LEGS = {"FL": "L", "FR": "R"}
for leg, s in LEGS.items():
    _, head, tail, _ = BONES[f"front_paw_{s}"]
    BONES[f"ik_{leg}"] = ("root", head, tail, False)
    pole = BONES[f"front_lower_{s}"][1] + Vector((-0.25, 0, 0))
    BONES[f"pole_{leg}"] = ("chest", pole, pole + Vector((0, 0, 0.03)), False)


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
    for leg, s in LEGS.items():  # the target shares the paw's roll, so copying its turn changes nothing at rest
        data.edit_bones[f"ik_{leg}"].roll = data.edit_bones[f"front_paw_{s}"].roll
    bpy.ops.object.mode_set(mode="OBJECT")
    select(body, arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    return data, arm


data, arm = rig()

# --- weights ---------------------------------------------------------------------------------------------------------------
# Vertices the bone heat missed copy their nearest weighted neighbour.
weighted = [sum(g.weight for g in v.groups) > 0.01 for v in body.data.vertices]
print("[build_misha] bone-heat weighted", sum(weighted), "of", len(weighted))
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
data.bones["root"].use_deform = True  # kept in the export as the skeleton's root; it has no weights
names = {g.index: g.name for g in body.vertex_groups}


def hand_over(src, dst, share):
    """Moves share(vertex position) 0-1 of each vertex's `src` weight to `dst`."""
    a, b = body.vertex_groups[src], body.vertex_groups[dst]
    for v in body.data.vertices:
        w = next((g.weight for g in v.groups if g.group == a.index), 0.0)
        k = share(v.co) * w
        if k > 0:
            a.add([v.index], w - k, "REPLACE")
            b.add([v.index], k, "ADD")


# The front legs are fused into the bib: bone heat hands them the whole chest. Above the shoulder joint the chest takes over. The
# ears' roots sit on the crown and take the top of the head with them: inside the ear flaps (nearer the middle) the head keeps it.
for s, k in SIDES:
    shoulder = BONES[f"front_upper_{s}"][1].z
    hand_over(f"front_upper_{s}", "chest", lambda p: max(smooth((p.z - shoulder + 0.03) / 0.07), 1 - smooth((abs(p.y) - 0.04) / 0.05)))
    hand_over(f"ear1_{s}", "head", lambda p: 1 - smooth((abs(p.y) - 0.125) / 0.045))

# --- rig check: joints and weights at rest ----------------------------------------------------------------------------------
def camera_rig(ortho):
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x, scene.render.resolution_y = 600, 500
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = ortho
    return cam


def shoot(cam, name, d, centre, ortho=None):
    if ortho:
        cam.data.ortho_scale = ortho
    d = Vector(d).normalized()
    cam.location = centre + d * 3
    cam.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    scene.render.filepath = os.path.join(ROOT, "Saved", name)
    bpy.ops.render.render(write_still=True)


VIEWS = (("side", (0.0001, -1, 0.05)), ("left", (0.0001, 1, 0.05)), ("quarter", (1, -1, 0.5)), ("front", (1, 0.0001, 0.15)), ("back", (-1, 0.3, 0.6)))

if RIG_CHECK:
    palette = [(0.9, 0.1, 0.1), (0.1, 0.8, 0.1), (0.1, 0.3, 0.95), (0.95, 0.85, 0.1), (0.85, 0.1, 0.85), (0.1, 0.85, 0.85),
               (1, 0.5, 0), (0.5, 0.25, 0.1), (0.6, 0.6, 1), (0.3, 0.6, 0.3), (1, 0.6, 0.7), (0.4, 0.4, 0.4)]
    colours = body.data.color_attributes.new("Weights", "FLOAT_COLOR", "POINT")
    group_list = sorted(names.values())
    for v in body.data.vertices:
        top = max(v.groups, key=lambda g: g.weight, default=None)
        c = palette[group_list.index(names[top.group]) % len(palette)] if top else (0, 0, 0)
        colours.data[v.index].color = (*c, 1)
    print("[build_misha] colours:", {n: palette[i % len(palette)] for i, n in enumerate(group_list)})
    marks = []
    for name, (parent, head, tail, deform) in BONES.items():
        if not deform:
            continue
        for p, r in ((head, 0.012), (tail, 0.008)):
            bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=p)
            marks.append(bpy.context.object)
        bpy.ops.mesh.primitive_cylinder_add(radius=0.004, depth=(tail - head).length, location=(head + tail) / 2)
        cyl = bpy.context.object
        cyl.rotation_mode = "QUATERNION"
        cyl.rotation_quaternion = (tail - head).to_track_quat("Z", "Y")
    cam = camera_rig(1.2)
    centre = Vector((0, 0, 0.37))
    scene.display.shading.color_type = "VERTEX"
    for view, d in VIEWS:
        shoot(cam, f"misha_rig_weights_{view}.png", d, centre)
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_xray = True
    scene.display.shading.xray_alpha = 0.35
    for view, d in VIEWS:
        shoot(cam, f"misha_rig_joints_{view}.png", d, centre)
    raise SystemExit

# --- IK ---------------------------------------------------------------------------------------------------------------------
def pole_angle(base, tip, pole):
    """The IK pole angle that keeps the chain base->tip bending the way it does at rest (Blender's IK otherwise twists it)."""
    normal = (tip.tail_local - base.head_local).cross(pole - base.head_local)
    projected = normal.cross(base.tail_local - base.head_local)
    x_axis = base.matrix_local.to_3x3().col[0]
    angle = x_axis.angle(projected)
    return -angle if x_axis.cross(projected).dot(base.tail_local - base.head_local) < 0 else angle


for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
for leg, s in LEGS.items():
    lower = arm.pose.bones[f"front_lower_{s}"]
    ik = lower.constraints.new("IK")
    ik.name = "IK"
    ik.target, ik.subtarget = arm, f"ik_{leg}"
    ik.pole_target, ik.pole_subtarget = arm, f"pole_{leg}"
    ik.chain_count = 2
    ik.pole_angle = pole_angle(data.bones[f"front_upper_{s}"], data.bones[lower.name], data.bones[f"pole_{leg}"].head_local)
    turn = arm.pose.bones[f"front_paw_{s}"].constraints.new("COPY_ROTATION")
    turn.name = "Paw"
    turn.target, turn.subtarget = arm, f"ik_{leg}"
low = min(v.co.z for v in body.data.vertices)
print("[build_misha] rest pose: lowest point %.3f m, top %.3f m, length %.3f m" % (low, max(v.co.z for v in body.data.vertices), max(v.co.x for v in body.data.vertices) - min(v.co.x for v in body.data.vertices)))

# --- animation -------------------------------------------------------------------------------------------------------------
ORDER = list(BONES)  # parents come before children
PARENT = {n: b[0] for n, b in BONES.items()}
REST = {n: data.bones[n].matrix_local.to_3x3() for n in ORDER}
CONSTRAINED = [(arm.pose.bones[f"front_lower_{s}"].constraints["IK"], arm.pose.bones[f"front_paw_{s}"].constraints["Paw"]) for s in LEGS.values()]


def key(frame, rot=None, pelvis=Vector(), feet=None, ik=1.0, root_rot=None, pivot=Vector()):
    """Keys the whole pose at `frame`. rot: bone -> world-axis turn added on top of its parent's (children follow parents).
    pelvis moves the body; feet: leg -> (move of the wrist from its rest spot, turn of the paw); ik < 1 hands the front legs back
    to their own (rot) turns; root_rot turns everything about `pivot` (the death topple)."""
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


def new_action(name):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    use_action(action)
    return action


def use_action(action):
    """Blender 5 actions are slotted: making one current also needs its slot assigned."""
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]


def curve(t, keys):
    """Eases between (time, value) keys, t and the times 0-1; the first and last values should match for a loop."""
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            return v0 + (v1 - v0) * smooth((t - t0) / (t1 - t0))
    return keys[-1][1]


def twitch(t, at, length=0.05):
    """A quick flick at time `at`: up and back over `length` of the clip."""
    return math.sin(math.pi * min(1.0, max(0.0, (t - at) / length)))


def ears(out, back=0.0, lag=0.6):
    """Both ears: `out` degrees flared away from the head (negative: pressed in), `back` swung back (negative: forward); the lower
    half follows with `lag` of it, so the flap curls."""
    rot = {}
    for s, k in SIDES:
        rot[f"ear1_{s}"] = rx(out * k) @ ry(back)
        rot[f"ear2_{s}"] = rx(out * k * lag) @ ry(back * lag)
    return rot


def idle():
    """Three seconds: he breathes, looks left, back, then right, tilts his head, his ears sway with it and give one shake, and his
    tail sweeps along the ground."""
    for f in range(0, 91, 2):
        t = f / 90
        b = math.sin(2 * math.pi * t * 3)  # three breaths
        look = curve(t, [(0, 0), (0.12, 0), (0.25, 24), (0.4, 24), (0.5, 0), (0.58, -20), (0.7, -20), (0.8, 0), (1, 0)])
        tilt = curve(t, [(0, 0), (0.78, 0), (0.84, 14), (0.94, 14), (1, 0)])
        nod = curve(t, [(0, 0), (0.25, -4), (0.4, -4), (0.5, 3), (0.58, 0), (1, 0)])
        shake = math.sin(2 * math.pi * (t - 0.5) * 12) * twitch(t, 0.5, 0.12)  # the ear shake as he turns back from the left
        wag = lambda lag: math.sin(2 * math.pi * (t * 3 - lag))  # one sweep a second, the tip a beat behind the root
        rot = {"chest": ry(-1.5 * b), "spine": ry(0.8 * b), "neck": rz(0.4 * look) @ ry(nod),
               "head": rz(0.6 * look) @ rx(tilt) @ ry(1.5 * math.sin(4 * math.pi * t)),
               "tail1": rz(10 * wag(0)), "tail2": rz(10 * wag(0.08)), "tail3": rz(12 * wag(0.16)), "tail4": rz(10 * wag(0.24))}
        rot.update(ears(4 * b + 14 * shake - tilt * 0.4, -look * 0.25))
        rot["ear1_L"] = rot["ear1_L"] @ rx(-tilt * 0.6)  # a tilted head lets the ears hang plumb
        rot["ear1_R"] = rot["ear1_R"] @ rx(-tilt * 0.6)
        key(f, rot, Vector((0, 0, 0.002 * b)))


def hop_offsets(u, duty=0.35):
    """Where the body is during a hop, u 0-1 from touchdown: sitting (sliding back at the ground's speed, as the capsule moves on)
    for `duty` of it, then in the air, carried forward and up. Returns (forward, height, how far through the air 0-1)."""
    sweep = HOP * duty
    if u < duty:
        return sweep / 2 - sweep * u / duty, 0.0, 0.0
    a = (u - duty) / (1 - duty)
    return -sweep / 2 + sweep * smooth(a), 0.11 * math.sin(math.pi * a), a


def move():
    """One seated hop: the landing squashes him, he gathers and springs, the ears lag behind, flaring as he drops, and flop."""
    frames = HOP_FRAMES
    for f in range(frames + 1):
        u = f / frames
        x, z, a = hop_offsets(u)
        ground = u < 0.35
        squash = math.sin(math.pi * u / 0.35) if ground else 0.0
        air = math.sin(math.pi * a)
        # Ears: pressed in on the way up, flared on the way down, then they flop past and settle once he has landed.
        if ground:
            g = u / 0.35
            flare = 30 * (1 - smooth(g * 1.6)) - 10 * math.sin(math.pi * min(1.0, g * 1.3))
        else:
            flare = -8 + 38 * smooth(a)
        rot = {"pelvis": ry(6 * squash - 5 * air), "spine": ry(5 * squash), "chest": ry(5 * squash + 3 * air),
               "neck": ry(-6 * squash), "head": ry(-4 * squash + 4 * air),
               "tail1": ry(4 + 16 * air - 4 * squash), "tail2": ry(8 * air), "tail3": ry(6 * air)}
        rot.update(ears(flare, 10 * air))
        dangle = Vector((0.02 * air, 0, -0.015 * air))
        foot = (Vector((x, 0, z)) + dangle, ry(18 * air))
        key(f, rot, Vector((x, 0, z - 0.01 * squash)), {"FL": foot, "FR": foot})
    return frames


def bark():
    """One second: draws back with his nose up, then throws his head forward and barks (frame 15, 0.5 s), front paws bouncing
    off the ground, shakes with it and settles."""
    frames = 30
    for f in range(frames + 1):
        draw = smooth(f / 12) if f <= 12 else 1 - smooth((f - 12) / 3)
        thrust = smooth((f - 12) / 3) if f <= 17 else 1 - smooth((f - 17) / 13)
        shake = math.sin(2 * math.pi * (f - 15) / 4) * (1 - smooth((f - 15) / 9)) if f >= 15 else 0.0
        bounce = math.sin(math.pi * min(1.0, max(0.0, (f - 13) / 6)))
        rot = {"pelvis": ry(-5 * draw + 2 * thrust), "spine": ry(-4 * draw + 3 * thrust), "chest": ry(-8 * draw + 6 * thrust),
               "neck": ry(-8 * draw + 2 * thrust), "head": ry(-16 * draw - 8 * thrust) @ rx(4 * shake),  # thrust: the nose stays about level
               "tail1": ry(14 * thrust) @ rz(8 * shake), "tail2": ry(8 * thrust)}
        rot.update(ears(-10 * draw + 30 * thrust + 8 * shake, 20 * draw - 35 * thrust))
        feet = (Vector((0.015 * thrust, 0, 0.03 * bounce)), ry(15 * bounce))
        key(f, rot, Vector((0.01 * thrust - 0.01 * draw, 0, 0.012 * bounce)), {"FL": feet, "FR": feet})
    return frames


def hit():
    frames = 15
    for f in range(frames + 1):
        b = smooth(f / 3) if f < 3 else 1 - smooth((f - 3) / 12)
        rot = {"pelvis": ry(-10 * b) @ rz(6 * b), "chest": ry(-8 * b) @ rx(6 * b), "neck": ry(-10 * b), "head": rz(-14 * b) @ ry(-12 * b),
               "tail1": ry(20 * b)}
        rot.update(ears(35 * b, -30 * b))
        lift = math.sin(math.pi * min(1.0, f / 7))
        feet = (Vector((-0.02 * b, 0, 0.035 * lift)), ry(20 * lift))
        key(f, rot, Vector((-0.03 * b, 0, 0.01 * lift)), {"FL": feet, "FR": feet})
    return frames


def death():
    frames = 45  # he wobbles, topples onto his right side and lies still, ears spilled on the ground
    for f in range(0, frames + 1, 3):
        wobble = math.sin(2 * math.pi * f / 12) * (1 - smooth(f / 14))
        roll = smooth((f - 8) / 16)
        settle = smooth((f - 22) / 14)
        rot = {"chest": ry(8 * roll) @ rx(6 * wobble), "neck": ry(10 * roll), "head": ry(14 * roll) @ rx(-10 * settle),
               "tail1": ry(-6 * settle)}
        for s, k in SIDES:
            rot[f"front_upper_{s}"] = ry(-35 * settle)
            rot[f"front_lower_{s}"] = ry(20 * settle)
        rot.update(ears(-20 * roll + 25 * settle, 15 * settle))
        bounce = 0.04 * math.sin(math.pi * smooth((f - 24) / 8)) * (1 - settle)
        pose = lambda lift: key(f, rot, Vector((0, 0, bounce + lift)), ik=1 - smooth((f - 10) / 10), root_rot=rx(86 * roll + 4 * wobble), pivot=Vector((0, -0.23, 0)))
        pose(0.0)
        scene.frame_set(f)
        sink = lowest()
        if sink < 0:  # lying down, an ear or a cheek would go through the ground: lift him clear
            pose(-sink)
    return frames


def lowest():
    """The lowest point of the posed mesh."""
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    low = min(v.co.z for v in mesh.vertices)
    evaluated.to_mesh_clear()
    return low


CLIPS = (("Idle", idle), ("Move", move), ("Bark", bark), ("Hit", hit), ("Death", death))
actions = {}
for name, build in CLIPS:
    action = new_action(name)
    frames = build() or 90
    actions[name] = (action, frames)
    print(f"[build_misha] clip {name}: {frames + 1} frames ({frames / FPS:.2f} s)")
print("[build_misha] hop speed at play rate 1, in game cm/s (times the %.1f scale): %.0f" % (GAME_SCALE, HOP / (HOP_FRAMES / FPS) * 100 * GAME_SCALE))

# --- previews --------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.display.shading.color_type = "TEXTURE"
    cam = camera_rig(1.2)
    bpy.ops.mesh.primitive_plane_add(size=0.002, location=(0, 0, 0))  # a ground line
    bpy.context.object.scale = (900, 900, 1)
    centre = Vector((0, 0, 0.37))
    only = [a for a in ARGS if a != "preview"]
    for clip, (action, frames) in actions.items():
        if only and clip not in only:
            continue
        use_action(action)
        for view, d in VIEWS[:4]:
            for f in sorted({0, frames // 4, frames // 2, 3 * frames // 4, frames} | ({15} if clip == "Bark" else set()) | ({6, 13} if clip == "Move" else set())):
                scene.frame_set(f)
                shoot(cam, f"misha_{clip}_{view}_{f:02d}.png", d, centre)
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
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Misha.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, (action, frames) in actions.items():
    use_action(action)
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Misha_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_misha] wrote Misha_{name}.fbx")
