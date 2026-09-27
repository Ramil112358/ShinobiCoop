"""Blender headless: re-exports tree glTFs as FBX with a trunk-only collision cylinder (UE's UCX_ convention), so the
canopy never blocks players (sub-project #3, W2).

  blender -b -P Scripts/world/blender/prep_trees.py -- <in_folder> <out_folder> <name_regex>

The trunk is found from the vertices in the lowest 2.5 m: its centre is their mean, its radius their 70th-percentile
distance from it (capped 0.15-0.8 m). The collision cylinder runs from the ground to 4 m.
"""
import math
import os
import re
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, dst, pattern = args[0], args[1], re.compile(args[2])
os.makedirs(dst, exist_ok=True)

for name in sorted(os.listdir(src)):
    if not name.lower().endswith(".gltf") or not pattern.search(name):
        continue
    base = os.path.splitext(name)[0]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(src, name))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    tree = bpy.context.active_object
    tree.name = base
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    for o in list(bpy.context.scene.objects):
        if o is not tree:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    points = [tree.matrix_world @ v.co for v in tree.data.vertices]
    low = [p for p in points if p.z < min(q.z for q in points) + 2.5] or points
    centre = sum(low, Vector()) / len(low)
    distances = sorted(math.hypot(p.x - centre.x, p.y - centre.y) for p in low)
    radius = min(0.8, max(0.15, distances[int(len(distances) * 0.7)]))
    ground = min(p.z for p in points)
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius, depth=4.0, location=(centre.x, centre.y, ground + 2.0))
    collision = bpy.context.active_object
    collision.name = f"UCX_{base}_00"

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.fbx(filepath=os.path.join(dst, f"{base}.fbx"), use_selection=True, path_mode="COPY",
                             embed_textures=True, apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE")
    print(f"[prep_trees] {base}: trunk r={radius:.2f} m")
