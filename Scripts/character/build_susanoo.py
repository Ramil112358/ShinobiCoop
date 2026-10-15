"""Builds the Perfect Susanoo for Unreal from the Tripo-rigged model and the Mixamo clips (Blender 5.2, headless).

  blender -b -P Scripts/character/build_susanoo.py [-- preview]

In:  SourceArt/Susanoo/Models/tripo_rig/*.fbx   Tripo Studio's Humanoid auto-rig (Mixamo bone names, 65 bones) of the model
     SourceArt/Susanoo/Anims/susanoo_*.fbx      Mixamo clips, downloaded Without Skin on the Mixamo rig of the same model
Out: SourceArt/Susanoo/Build/SK_Susanoo.fbx     the mesh and skeleton, 180 cm tall, feet at the origin, facing -Y
     SourceArt/Susanoo/Build/A_Susanoo_<clip>.fbx  each clip on that skeleton (mesh included, no texture), hands closed round the sword grips

Why retarget at all: the clips' bones are named like the Tripo rig's but rest in other orientations, so their local
rotations can't be copied across. Each frame, every bone takes the source bone's rotation away from its own rest, in world
space, on top of its own rest (both rigs stand in the same A-pose); the hips also take the source's height, scaled by
hip height, but not its travel (a clip without In Place would walk the figure away from its wearer). Finger bones then curl into a fist: the Mixamo rig had only an index finger, and a sword needs a closed hand.
`preview` renders each clip's middle frame to Saved/SusanooBuild_<clip>.png.
"""
import glob
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ART = os.path.join(ROOT, "SourceArt", "Susanoo")
OUT = os.path.join(ART, "Build")
PREVIEW = os.path.join(ROOT, "Saved", "SusanooBuild")
HEIGHT = 1.8  # metres
FINGER_CURL = {"1": 35.0, "2": 75.0, "3": 60.0}  # degrees per joint, knuckle to tip: a fist round a grip
THUMB_CURL = {"1": 15.0, "2": 30.0, "3": 30.0}
CURL_AXIS = "X"  # the bones' local axis the fingers bend about (Y runs along each bone)

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def fail(message):
    print(f"[build_susanoo] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[build_susanoo] {message}")


def imported(path):
    """Imports an FBX and returns the new objects."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    return [o for o in bpy.data.objects if o not in before]


bpy.ops.wm.read_factory_settings(use_empty=True)
models = glob.glob(os.path.join(ART, "Models", "tripo_rig", "*.fbx"))
if not models:
    fail("no Tripo-rigged FBX in SourceArt/Susanoo/Models/tripo_rig (unzip Tripo's export there)")
objects = imported(models[0])
rig = next((o for o in objects if o.type == "ARMATURE"), None)
body = next((o for o in objects if o.type == "MESH"), None)
if not rig or not body:
    fail(f"{models[0]} has no armature and mesh")
rig.name, body.name = "Susanoo", "SK_Susanoo"

# 180 cm, feet on the ground, transforms applied, so Unreal gets a clean skeleton.
zs = [(body.matrix_world @ v.co).z for v in body.data.vertices]
scale = HEIGHT / (max(zs) - min(zs))
rig.scale = [s * scale for s in rig.scale]
bpy.context.view_layer.update()
rig.location.z -= min((body.matrix_world @ v.co).z for v in body.data.vertices)
for obj in bpy.data.objects:
    obj.select_set(obj in (rig, body))
bpy.context.view_layer.objects.active = rig
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bones = sorted(rig.data.bones, key=lambda b: len(b.parent_recursive))  # parents before children
rest = {b.name: b.matrix_local.copy() for b in bones}
hips_name = "mixamorig:Hips"
if hips_name not in rest:
    fail("the rig has no mixamorig:Hips (export Tripo's rig with the Mixamo skeleton preset)")
log(f"{body.name}: {len(body.data.vertices)} vertices, {len(bones)} bones, {HEIGHT * 100:.0f} cm")


def curl(name):
    """The fist's rotation for a finger bone, or None for any other bone."""
    short = name.replace("mixamorig:", "")
    if "Hand" not in short or short.endswith("Hand") or short[-1] not in "123":
        return None
    angles = THUMB_CURL if "Thumb" in short else FINGER_CURL
    return Quaternion(Matrix.Rotation(math.radians(angles[short[-1]]), 3, CURL_AXIS).to_quaternion())


def retarget(clip_path):
    """Bakes a Mixamo clip onto the rig as a new action; returns it."""
    new = imported(clip_path)
    source = next((o for o in new if o.type == "ARMATURE"), None)
    if not source or not source.animation_data or not source.animation_data.action:
        fail(f"{clip_path} has no animated armature")
    clip = source.animation_data.action
    first, last = (int(round(f)) for f in clip.frame_range)
    world = source.matrix_world
    src_rest = {b.name: (world @ b.matrix_local) for b in source.data.bones}
    hip_scale = rest[hips_name].translation.z / src_rest[hips_name].translation.z
    action = bpy.data.actions.new(os.path.splitext(os.path.basename(clip_path))[0])
    rig.animation_data_create()
    rig.animation_data.action = action
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"
    scene = bpy.context.scene
    for frame in range(first, last + 1):
        scene.frame_set(frame)
        posed = {}  # bone -> its pose matrix in the rig's space this frame
        for bone in bones:
            name = bone.name
            parent = bone.parent
            if name in src_rest and name in source.pose.bones:
                src_pose = world @ source.pose.bones[name].matrix
                delta = src_pose.to_quaternion() @ src_rest[name].to_quaternion().inverted()
                rotation = delta @ rest[name].to_quaternion()
            else:
                rotation = None
            if parent is None:
                head = rest[name].translation
                if name == hips_name:  # the hips bob and sway with the clip but never travel: the figure stays round its wearer
                    moved = (world @ source.pose.bones[hips_name].head) * hip_scale
                    head = rest[name].translation.copy()
                    head.z = moved.z
            else:
                head = (posed[parent.name] @ rest[parent.name].inverted() @ rest[name]).translation
            if rotation is None:  # no source bone: follow the parent rigidly
                rotation = (posed[parent.name] @ rest[parent.name].inverted() @ rest[name]).to_quaternion() if parent else rest[name].to_quaternion()
            fist = curl(name)
            if fist is not None:  # the hand closes, whatever the clip's index finger did
                rotation = ((posed[parent.name] @ rest[parent.name].inverted() @ rest[name]).to_quaternion() if parent else rotation) @ fist
            matrix = Matrix.LocRotScale(head, rotation, None)
            posed[name] = matrix
            basis = rest[name].inverted() @ (rest[parent.name] @ posed[parent.name].inverted() if parent else Matrix()) @ matrix
            pb = rig.pose.bones[name]
            pb.location, pb.rotation_quaternion = basis.to_translation(), basis.to_quaternion()
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            if parent is None:
                pb.keyframe_insert("location", frame=frame)
    for obj in new:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.actions.remove(clip)
    action.frame_range = (first, last)
    log(f"{action.name}: frames {first}..{last}")
    return action


def export(path, with_mesh, action=None):
    for obj in bpy.data.objects:
        obj.select_set(obj is rig or (with_mesh and obj is body))
    bpy.context.view_layer.objects.active = rig
    rig.animation_data_create()
    rig.animation_data.action = action
    if action:  # the exporter bakes the scene's range, not the action's
        bpy.context.scene.frame_start, bpy.context.scene.frame_end = (int(f) for f in action.frame_range)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, add_leaf_bones=False, bake_anim=action is not None,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                             armature_nodetype="NULL", path_mode="COPY", embed_textures=with_mesh and action is None, mesh_smooth_type="FACE")
    log("wrote " + path)


os.makedirs(OUT, exist_ok=True)
export(os.path.join(OUT, "SK_Susanoo.fbx"), True)
clips = sorted(glob.glob(os.path.join(ART, "Anims", "susanoo_*.fbx")))
if not clips:
    fail("no Mixamo clips in SourceArt/Susanoo/Anims")
actions = [retarget(path) for path in clips]
for action in actions:
    name = action.name.replace("susanoo_", "").capitalize()
    export(os.path.join(OUT, f"A_Susanoo_{name}.fbx"), True, action)  # with the mesh: an armature alone is written in its current pose as rest

if "preview" in args:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x, scene.render.resolution_y = 700, 800
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.6, 0.6, 0.6)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 2.8
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = (-4.2, -4.2, 2.4)
    cam.rotation_euler = (math.radians(72), 0, math.radians(-45))
    for action in actions:
        rig.animation_data.action = action
        scene.frame_set(int(sum(action.frame_range) / 2))
        scene.render.filepath = f"{PREVIEW}_{action.name}.png"
        bpy.ops.render.render(write_still=True)
