"""Blender (5.2) headless: re-export a rigged FBX so Unreal imports it with unit-scale bones.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/clean_rig.py -- <in.fbx> <out.fbx>

A Mixamo FBX (and one Blender exports in metres) arrives in Unreal with its root bone (Hips) scaled 100 and every child
bone's offsets in hundredths: animation copes, but Chaos cloth and the physics-asset generator size things in bone
units and come out ~100x wrong (outfit cloth flew a metre off the body; the auto ragdoll got 50 cm capsules). Exported
from a centimetre scene with the rig scaled up to match, Unreal gets the same skeleton with every bone at scale 1.
Scripts/setup_hero.py runs this on every body before importing it; skin_outfit.py exports its copies the same way
(export_unit_scale_fbx below).
"""
import os
import sys

import bpy


def export_unit_scale_fbx(path, objects):
    """Exports objects (an armature and its meshes, already in a metre scene) so their bones import at scale 1:
    the scene goes to centimetre units with the rig scaled x100 and applied, then back."""
    scene = bpy.context.scene
    old_unit = scene.unit_settings.scale_length
    armatures = [o for o in objects if o.type == "ARMATURE"]
    scene.unit_settings.scale_length = 0.01
    for obj in bpy.context.scene.objects:
        obj.select_set(obj in objects)
    if armatures:
        bpy.context.view_layer.objects.active = armatures[0]
    for obj in objects:
        if obj.parent is None or obj.parent not in objects:
            obj.scale = obj.scale * 100.0
            obj.location = obj.location * 100.0
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             bake_anim=False, path_mode="STRIP", apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                             colors_type="SRGB")
    scene.unit_settings.scale_length = old_unit


if __name__ == "__main__":
    cli = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(cli) < 2:
        print("[clean_rig] ERROR: usage: -- <in.fbx> <out.fbx>")
        sys.exit(1)
    source, target = (os.path.abspath(p) for p in cli[:2])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=source)
    rig = [o for o in bpy.context.scene.objects if o.type in ("ARMATURE", "MESH")]
    if not any(o.type == "ARMATURE" for o in rig):
        print(f"[clean_rig] ERROR: {source} has no armature")
        sys.exit(1)
    # An FBX Unreal exported (a MetaHuman body) hangs the armature under a 0.01-scaled empty: the rig takes its transform.
    for armature in (o for o in rig if o.type == "ARMATURE" and o.parent and o.parent.type == "EMPTY"):
        world = armature.matrix_world.copy()
        empty = armature.parent
        armature.parent = None
        armature.matrix_world = world
        bpy.data.objects.remove(empty, do_unlink=True)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    export_unit_scale_fbx(target, rig)
    print(f"[clean_rig] wrote {target}")
