"""Imports Susanoo's figure: SourceArt/Props/susanoo.fbx (Scripts/character/make_susanoo.py) -> /Game/Shinobi/Jutsu/SM_Susanoo.

Idempotent. Close the GUI editor first. The figure has no collision (it is purely visual; UShinobiSusanooComponent shows it).
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX = os.path.join(ROOT, "SourceArt", "Props", "susanoo.fbx")
DEST = "/Game/Shinobi/Jutsu"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.destination_name = "SM_Susanoo"
task.automated = True
task.save = True
task.replace_existing = True
TOOLS.import_asset_tasks([task])

mesh = unreal.load_asset(f"{DEST}/SM_Susanoo")
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError("SM_Susanoo was not imported from " + FBX)
unreal.StaticMeshEditorSubsystem().remove_collisions(mesh)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
box = mesh.get_bounding_box()
unreal.log(f"[setup_susanoo] imported {mesh.get_path_name()}, bounds {box.min} .. {box.max}")
