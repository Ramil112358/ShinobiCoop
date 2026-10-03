"""Exports a Mesh2Motion creature (CC0 GLB with a skinned mesh, armature and actions) as FBX for Unreal.

Run: blender -b -P Scripts/character/export_creature.py -- <name> <src.glb> <yaw-or-auto> <Action=Clip,Action=Clip,...>

Writes SourceArt/Creatures/<name>/<name>.fbx (mesh + skeleton, no animation) and <name>_<Clip>.fbx per clip (skeleton + that one
animation). The creature is turned to face +X (UE's forward), centred over the origin with its lowest point on the ground.
(A Rodin model is rigged by Scripts/character/build_toad.py instead.)
Units: glTF metres -> Blender metres; the FBX exporter makes them centimetres (so a 4 m snake is 400 UE units).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[sys.argv.index("--") + 1:]
NAME, SRC, YAW = args[0], args[1], args[2]
CLIPS = dict(pair.split("=") for pair in args[3].split(",")) if len(args) > 3 and args[3] else {}
OUT = os.path.join(ROOT, "SourceArt", "Creatures", NAME)
os.makedirs(OUT, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC if os.path.isabs(SRC) else os.path.join(ROOT, SRC))
scene = bpy.context.scene
scene.render.fps = 30

arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and not any(m.type == "ARMATURE" for m in o.modifiers):
        bpy.data.objects.remove(o)  # Mesh2Motion's bone-display icosphere
mesh = next(o for o in bpy.data.objects if o.type == "MESH")


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


# Which way does it face? From the middle of the skeleton towards the head, on the ground plane.
def head_direction():
    bpy.context.view_layer.update()
    bones = arm.data.bones
    head = next(b for b in bones if b.name.lower() == "head")
    centre = sum((b.head_local for b in bones), Vector()) / len(bones)  # the body's middle, in the rest pose
    v = arm.matrix_world @ head.head_local - arm.matrix_world @ centre
    return Vector((v.x, v.y, 0.0))


direction = head_direction()
yaw = math.degrees(math.atan2(direction.y, direction.x)) if YAW == "auto" else float(YAW)
rotate = -yaw  # head towards +X
print(f"[export_creature] head direction {tuple(round(c, 2) for c in direction)} -> turning {rotate:.0f} degrees")
arm.data.pose_position = "REST"  # fit the model in its rest pose; the clips are exported posed
mesh.parent = None  # glTF parents the mesh to the armature; the armature modifier is what binds it
mesh.matrix_world = mesh.matrix_world.copy()
for o in (arm, mesh):
    o.rotation_mode = "XYZ"
    o.rotation_euler.z += math.radians(rotate)
select(arm, mesh)
bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
after = head_direction()
print(f"[export_creature] head now {tuple(round(c, 2) for c in after)}")

# Centre the model over the origin on the ground plane, with its lowest point on Z = 0 (the capsule's floor).
bpy.context.view_layer.update()
points = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
shift = Vector(((min(p.x for p in points) + max(p.x for p in points)) / 2, (min(p.y for p in points) + max(p.y for p in points)) / 2, min(p.z for p in points)))
for o in (arm, mesh):
    o.location -= shift
select(arm, mesh)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
bpy.context.view_layer.update()
print(f"[export_creature] dimensions {tuple(round(c, 2) for c in mesh.dimensions)} (metres), shifted {tuple(round(c, 2) for c in shift)}")

common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL", path_mode="COPY")

# Mesh + skeleton, bound to the rest pose.
arm.animation_data_create()
arm.animation_data.action = None
for b in arm.pose.bones:
    b.location = (0, 0, 0)
    b.rotation_quaternion = (1, 0, 0, 0)
    b.rotation_euler = (0, 0, 0)
    b.scale = (1, 1, 1)
select(arm, mesh)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, NAME + ".fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, embed_textures=True, **common)

# One FBX per clip.
arm.data.pose_position = "POSE"
for action_name, clip in CLIPS.items():
    action = bpy.data.actions.get(action_name)
    if action is None:
        raise RuntimeError(f"no action {action_name!r}; have {[a.name for a in bpy.data.actions]}")
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]  # Blender 5 actions are slotted
    scene.frame_start, scene.frame_end = int(action.frame_range[0]), int(action.frame_range[1])
    select(arm)
    path = os.path.join(OUT, f"{NAME}_{clip}.fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE"}, bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[export_creature] {action_name} ({scene.frame_start}-{scene.frame_end}) -> {path}")
