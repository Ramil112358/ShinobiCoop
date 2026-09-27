"""Blender headless: renders a labelled contact sheet of every glTF in a folder (art-library review, sub-project #3).

  blender -b -P Scripts/world/blender/contact_sheet.py -- <folder> <out.png> [columns] [name_filter_regex]

Each model is imported into its own grid cell, scaled to fit the cell (true size is printed under it in metres), lit
flat and rendered orthographic from a 3/4 view with Workbench (texture colours).
"""
import math
import os
import re
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
folder, out = args[0], args[1]
columns = int(args[2]) if len(args) > 2 else 10
pattern = re.compile(args[3]) if len(args) > 3 else None
files = sorted(os.path.join(root, f) for root, _, names in os.walk(folder) for f in names
               if f.lower().endswith((".gltf", ".glb")) and (not pattern or pattern.search(f)))

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
cell = 3.0
for index, path in enumerate(files):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        continue
    corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    size = high - low
    scale = (cell * 0.8) / max(size.x, size.y, size.z, 1e-4)
    root = bpy.data.objects.new(f"cell_{index}", None)
    scene.collection.objects.link(root)
    for o in new:
        if o.parent is None:
            o.parent = root
    root.scale = (scale, scale, scale)
    col, row = index % columns, index // columns
    root.location = Vector((col * cell, -row * cell * 1.3, 0)) - (low + Vector((size.x / 2, size.y / 2, 0))) * scale
    label = bpy.data.curves.new(f"label_{index}", "FONT")
    label.body = f"{os.path.splitext(os.path.basename(path))[0][:22]}\n{size.z:.1f}m tall"
    label.size = 0.22
    text = bpy.data.objects.new(f"label_{index}", label)
    text.location = (col * cell - cell * 0.45, -row * cell * 1.3 - cell * 0.62, 0)
    text.rotation_euler = (math.radians(35), 0, 0)
    scene.collection.objects.link(text)

rows = math.ceil(len(files) / columns)
camera_data = bpy.data.cameras.new("Cam")
camera_data.type = "ORTHO"
camera_data.ortho_scale = max(columns * cell, rows * cell * 1.3) * 1.05
camera = bpy.data.objects.new("Cam", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
centre = Vector(((columns - 1) * cell / 2, -(rows - 1) * cell * 1.3 / 2, 0))
camera.rotation_euler = (math.radians(55), 0, 0)
camera.location = centre + Vector((0, -30, 21))
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x = 2000
scene.render.resolution_y = int(2000 * (rows * 1.3) / columns) + 200
world = bpy.data.worlds.new("W")
world.color = (0.75, 0.75, 0.75)
scene.world = world
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print(f"[contact_sheet] {len(files)} models -> {out}")
