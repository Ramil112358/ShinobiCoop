"""Rigs and animates the summon toad: SourceArt/Creatures/Toad/rodin_toad.glb (a Hyper3D Rodin model) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_toad.py            (add  -- preview  to render pose previews to Saved/toad_*.png)

The Rodin toad is one unrigged mesh. A small quadruped rig is built to its proportions (root, body, head, two legs of three bones
a side), the mesh is weighted to it by automatic weights, and the clips are keyed here: Idle, Move (a hop), Slam (rears up and
crashes down; the ground is hit 0.7 s in, when UGA_ToadSlam strikes), Hit and Death. Writes SourceArt/Creatures/Toad/Toad.fbx
(mesh + skeleton), Toad_<Clip>.fbx per clip and Toad.png (the 2k base colour). The toad faces +X, stands on Z = 0, centred; metres.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Toad")
PREVIEW = "preview" in sys.argv
FPS = 30

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "rodin_toad.glb"))
scene = bpy.context.scene
scene.render.fps = FPS
body = next(o for o in bpy.data.objects if o.type == "MESH")
for o in list(bpy.data.objects):
    if o is not body:
        bpy.data.objects.remove(o)
body.name = "Body"
body.parent = None


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


# --- fit: turn to face +X (Rodin faces -Y), centre over the origin, feet on the ground -------------------------------------
body.rotation_mode = "XYZ"
body.rotation_euler.z = math.radians(90)
select(body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
points = [body.matrix_world @ v.co for v in body.data.vertices]
shift = Vector(((min(p.x for p in points) + max(p.x for p in points)) / 2, (min(p.y for p in points) + max(p.y for p in points)) / 2, min(p.z for p in points)))
body.location -= shift
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

bb = [body.matrix_world @ v.co for v in body.data.vertices]
print("[build_toad] fit bbox", [round(min(getattr(p, a) for p in bb), 2) for a in "xyz"], [round(max(getattr(p, a) for p in bb), 2) for a in "xyz"], "rot", tuple(body.rotation_euler), "loc", tuple(body.location))

# --- the texture ----------------------------------------------------------------------------------------------------------
image = next((i for i in bpy.data.images if i.size[0] > 0 and i.name != "Render Result"), None)
if image:
    image.scale(2048, 2048)
    image.filepath_raw = os.path.join(OUT, "Toad.png")
    image.file_format = "PNG"
    image.save()
    print("[build_toad] texture", image.name)

# --- the rig ---------------------------------------------------------------------------------------------------------------
# name: (parent, head, tail). Y > 0 is the left side. Measured on the mesh: front feet at x 0.25, hind feet at x -0.45.
BONES = {"root": (None, (0, 0, 0), (0, 0, 0.25)),
         "pelvis": ("root", (-0.35, 0, 0.8), (-0.05, 0, 0.85)),
         "spine": ("pelvis", (-0.05, 0, 0.85), (0.3, 0, 0.9)),
         "chest": ("spine", (0.3, 0, 0.9), (0.55, 0, 0.95)),
         "head": ("chest", (0.5, 0, 1.0), (0.95, 0, 1.1))}
for side, s in (("L", 1), ("R", -1)):
    BONES[f"front_upper_{side}"] = ("chest", (0.3, s * 0.65, 0.85), (0.28, s * 0.7, 0.45))
    BONES[f"front_lower_{side}"] = (f"front_upper_{side}", (0.28, s * 0.7, 0.45), (0.25, s * 0.62, 0.05))
    BONES[f"front_foot_{side}"] = (f"front_lower_{side}", (0.25, s * 0.62, 0.05), (0.5, s * 0.62, 0.03))
    BONES[f"hind_upper_{side}"] = ("pelvis", (-0.45, s * 0.6, 0.8), (-0.25, s * 0.8, 0.4))
    BONES[f"hind_lower_{side}"] = (f"hind_upper_{side}", (-0.25, s * 0.8, 0.4), (-0.45, s * 0.55, 0.08))
    BONES[f"hind_foot_{side}"] = (f"hind_lower_{side}", (-0.45, s * 0.55, 0.08), (-0.2, s * 0.55, 0.03))

data = bpy.data.armatures.new("Armature")
arm = bpy.data.objects.new("Armature", data)
scene.collection.objects.link(arm)
select(arm)
bpy.ops.object.mode_set(mode="EDIT")
for name, (parent, head, tail) in BONES.items():
    edit = data.edit_bones.new(name)
    edit.head, edit.tail = Vector(head), Vector(tail)
    if parent:
        edit.parent = data.edit_bones[parent]
        edit.use_connect = False
bpy.ops.object.mode_set(mode="OBJECT")

# Bone-heat automatic weights fail on this mesh (0 of 20924 vertices weighted), so weight by distance: each vertex follows its four
# nearest bones, the nearer the stronger.
select(arm, body)  # the first one is the active object: the armature
bpy.ops.object.parent_set(type="ARMATURE_NAME")
groups = {name: body.vertex_groups[name] for name in BONES}


def segment_distance(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


segments = {name: (Vector(head), Vector(tail)) for name, (_, head, tail) in BONES.items() if name != "root"}  # the root moves the whole toad
for v in body.data.vertices:
    nearest = sorted((segment_distance(v.co, a, b), name) for name, (a, b) in segments.items())[:4]
    weights = [1.0 / (d + 0.05) ** 3 for d, _ in nearest]
    total = sum(weights)
    for (d, name), w in zip(nearest, weights):
        groups[name].add([v.index], w / total, "REPLACE")
print("[build_toad] weighted verts:", sum(1 for v in body.data.vertices if sum(g.weight for g in v.groups) > 0.99), "of", len(body.data.vertices))

# --- animation -------------------------------------------------------------------------------------------------------------
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
REST = {pb.name: pb.bone.matrix_local.to_3x3() for pb in arm.pose.bones}
ORDER = list(BONES)  # parents come before children


def ry(deg):   # about world Y: positive tips the nose (+X) down
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def nose_up(deg):
    return ry(-deg)


def rx(deg):
    return Matrix.Rotation(math.radians(deg), 3, "X")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def pose(frame, rot=None, root_rotation=None, pivot=Vector(), lift=Vector()):
    """Keys the whole pose at `frame`. rot: bone -> world-axis rotation added on top of its parent's (children follow
    parents). root_rotation turns the whole toad about `pivot`; lift moves it."""
    rot = rot or {}
    world = {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent = BONES[name][0]
        parent_world = world[parent] if parent else Matrix.Identity(3)
        delta = rot.get(name, Matrix.Identity(3))
        if name == "root":
            delta = root_rotation or Matrix.Identity(3)
        world[name] = delta @ parent_world
        local = REST[name].inverted() @ parent_world.inverted() @ delta @ parent_world @ REST[name]
        pb.rotation_quaternion = local.to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == "root":
            turn = root_rotation or Matrix.Identity(3)
            move = (pivot - turn @ pivot) + lift
            pb.location = REST[name].inverted() @ move
            pb.keyframe_insert("location", frame=frame)


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


def idle():
    for f in range(0, 61, 3):
        s = math.sin(2 * math.pi * f / 60)
        pose(f, {"chest": nose_up(1.5 * s), "spine": nose_up(1.0 * s), "head": ry(1.0 * s)}, lift=Vector((0, 0, 0.012 * s)))


def move():
    for f in range(0, 31, 2):
        u = f / 30
        air = max(0.0, math.sin(math.pi * (u - 0.25) / 0.5)) if 0.25 <= u <= 0.75 else 0.0
        sq = 1 - min(1.0, u / 0.25) if u < 0.25 else (1 - min(1.0, (1 - u) / 0.25) if u > 0.75 else 0.0)
        rot = {"pelvis": nose_up(12 * air - 8 * sq), "head": ry(-6 * air)}
        for side in "LR":
            rot[f"hind_upper_{side}"] = ry(-35 * sq + 30 * air)
            rot[f"hind_lower_{side}"] = ry(45 * sq - 40 * air)
            rot[f"front_upper_{side}"] = ry(20 * sq - 25 * air)
            rot[f"front_lower_{side}"] = ry(-15 * sq + 20 * air)
        pose(f, rot, lift=Vector((0, 0, 0.5 * air - 0.12 * sq)))


def slam():
    hind_feet = Vector((-0.45, 0, 0))
    for f in range(0, 37):
        if f <= 18:
            r = smooth(f / 18)
        elif f <= 21:
            r = 1 - 1.15 * smooth((f - 18) / 3)
        else:
            r = -0.15 * (1 - smooth((f - 21) / 15))
        rise = max(r, 0.0)
        rot = {"head": ry(10 * rise + 6 * max(-r, 0)), "chest": nose_up(6 * rise)}
        for side in "LR":
            rot[f"front_upper_{side}"] = nose_up(55 * rise) if r >= 0 else ry(20 * -r)
            rot[f"front_lower_{side}"] = ry(-20 * rise)
            rot[f"hind_upper_{side}"] = ry(15 * rise)
        pose(f, rot, root_rotation=nose_up(30 * r), pivot=hind_feet, lift=Vector((0, 0, -0.04 if f in (21, 22) else 0)))


def hit():
    for f in range(0, 16):
        b = math.sin(math.pi * f / 15)
        pose(f, {"head": ry(-10 * b), "chest": nose_up(8 * b)}, root_rotation=nose_up(10 * b), pivot=Vector((-0.45, 0, 0)), lift=Vector((-0.1 * b, 0, 0)))


def death():
    for f in range(0, 46, 2):
        k = smooth(f / 24)
        pose(f, {"head": ry(-8 * k)}, root_rotation=rx(-95 * k), pivot=Vector((0, 0.85, 0)))


CLIPS = (("Idle", 60, idle), ("Move", 30, move), ("Slam", 36, slam), ("Hit", 15, hit), ("Death", 45, death))
actions = {}
for name, frames, build in CLIPS:
    action = new_action(name, frames)
    build()
    actions[name] = action
    print(f"[build_toad] clip {name}: {frames} frames")

# --- previews --------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 700, 500
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = (0.3, -5.5, 1.0)
    cam.rotation_euler = (math.radians(90), 0, 0)
    for clip, frame in (("Idle", 0), ("Move", 14), ("Move", 0), ("Slam", 18), ("Slam", 21), ("Hit", 7), ("Death", 44)):
        use_action(actions[clip])
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        print("[build_toad] preview", clip, frame, "slot", arm.animation_data.action_slot, "chest", tuple(round(c, 3) for c in arm.pose.bones["chest"].rotation_quaternion), "mods", [m.type + ":" + str(m.object) for m in body.modifiers], "pos", arm.data.pose_position)
        scene.render.filepath = os.path.join(ROOT, "Saved", f"toad_{clip}_{frame}.png")
        bpy.ops.render.render(write_still=True)
    raise SystemExit

# --- export ----------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL", path_mode="COPY")
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Toad.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, frames, _ in CLIPS:
    use_action(actions[name])
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Toad_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_toad] wrote Toad_{name}.fbx")
