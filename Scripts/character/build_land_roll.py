"""Blender (5.2) headless: turns the Mixamo "Falling To Roll" (SourceArt/Mixamo/Falling To Roll.fbx) into the landing roll
AShinobiPlayerCharacter plays after a long fall or a double jump.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/build_land_roll.py

The clip falls for half a second before the feet touch, so it is trimmed to start at the touchdown and to end once the
character is back up. The roll travels ~3.6 m; the hips' horizontal travel is removed (in place) so the capsule's own
landing momentum carries the character instead of the mesh sliding off it. Writes SourceArt/Mixamo/Jutsu/Land_Roll.fbx,
which setup_jutsu_anims.py retargets onto Manny as AM_Land_Roll (FBX settings as build_jutsu_anims.py).
"""
import os

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "SourceArt", "Mixamo", "Falling To Roll.fbx")
OUT = os.path.join(ROOT, "SourceArt", "Mixamo", "Jutsu", "Land_Roll.fbx")
P = "mixamorig:"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SOURCE)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
scene = bpy.context.scene
scene.render.fps = 30
first, last = map(int, arm.animation_data.action.frame_range)
hips = arm.pose.bones[P + "Hips"]
feet = [arm.pose.bones[P + n] for n in ("LeftToeBase", "RightToeBase", "LeftFoot", "RightFoot")]


def world(bone, frame):
    scene.frame_set(frame)
    return arm.matrix_world @ bone.head


frames = range(first, last + 1)
ground = min(world(b, last).z for b in feet)
# Touchdown: the first frame a foot is within 5 cm of where the feet stand at the end.
touch = next(f for f in frames if min(world(b, f).z for b in feet) < ground + 0.05)
hips_z = {f: world(hips, f).z for f in frames}
low = min(range(touch, last + 1), key=hips_z.get)
# Back up: the hips within 3 cm of their final height after the roll's low point.
end = next(f for f in range(low, last + 1) if hips_z[f] > hips_z[last] - 0.03)

# In place: pin the hips over the origin (where the clip starts), keeping the roll's vertical motion.
anchor = world(hips, first)
targets = {}
for f in range(touch, end + 1):
    scene.frame_set(f)
    m = arm.matrix_world @ hips.matrix
    m.translation.x, m.translation.y = anchor.x, anchor.y
    targets[f] = arm.matrix_world.inverted() @ m
for f, m in targets.items():
    scene.frame_set(f)
    hips.matrix = m
    hips.keyframe_insert("location", frame=f)

print(f"[build_land_roll] touchdown {touch}, low {low}, up {end} of {first}-{last}: {(end - touch) / 30:.2f}s")
scene.frame_start, scene.frame_end = touch, end
for o in bpy.data.objects:
    o.select_set(o == arm)
bpy.context.view_layer.objects.active = arm
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, armature_nodetype="NULL")
