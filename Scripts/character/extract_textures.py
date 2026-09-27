"""Blender headless: writes the base-colour texture packed inside a Mixamo FBX to a PNG (sub-project #2).

  blender -b -P Scripts/character/extract_textures.py -- <in.fbx> <out.png>

Mixamo packs textures without a file extension ("texture_diffuse"), which UE's Interchange FBX importer can't read,
so Scripts/setup_hero.py imports this PNG instead (and runs this script itself when the PNG is missing).
"""
import os
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = os.path.abspath(args[0]), os.path.abspath(args[1])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
images = [img for img in bpy.data.images if img.size[0] > 0]
diffuse = next((img for img in images if "diffuse" in img.name.lower() or "base" in img.name.lower()), images[0] if images else None)
if diffuse is None:
    print(f"[extract_textures] ERROR: no texture in {src}")
    sys.exit(1)
diffuse.filepath_raw = dst
diffuse.file_format = "PNG"
diffuse.save()
print(f"[extract_textures] {diffuse.name} {tuple(diffuse.size)} -> {dst}")
