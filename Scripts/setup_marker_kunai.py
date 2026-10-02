"""Imports Teleport's marker kunai: SourceArt/Props/marker_kunai.fbx (Scripts/character/make_marker_kunai.py) -> /Game/Shinobi/Jutsu/SM_MarkerKunai.

Idempotent. Close the GUI editor first. The kunai has no collision of its own (AShinobiMarkerKunai's sphere does the work).
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX = os.path.join(ROOT, "SourceArt", "Props", "marker_kunai.fbx")
DEST = "/Game/Shinobi/Jutsu"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.destination_name = "SM_MarkerKunai"
task.automated = True
task.save = True
task.replace_existing = True
TOOLS.import_asset_tasks([task])

mesh = unreal.load_asset(f"{DEST}/SM_MarkerKunai")
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError("SM_MarkerKunai was not imported from " + FBX)
unreal.StaticMeshEditorSubsystem().remove_collisions(mesh)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
box = mesh.get_bounding_box()
unreal.log(f"[setup_kunai] imported {mesh.get_path_name()}, bounds {box.min} .. {box.max}")
