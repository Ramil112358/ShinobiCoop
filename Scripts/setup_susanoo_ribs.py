"""Imports the guard's ribcage: SourceArt/Props/susanoo_ribs.fbx (Scripts/character/prep_susanoo_ribs.py) ->
/Game/Shinobi/Jutsu/SM_SusanooRibs. Idempotent. Close the GUI editor first. No collision: AShinobiPlayerCharacter's guard figure
(UShinobiSusanooComponent) only shows it.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX = os.path.join(ROOT, "SourceArt", "Props", "susanoo_ribs.fbx")
DEST = "/Game/Shinobi/Jutsu"
NAME = "SM_SusanooRibs"

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

mesh = unreal.load_asset(f"{DEST}/{NAME}")
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError(f"{NAME} was not imported from {FBX}")
unreal.StaticMeshEditorSubsystem().remove_collisions(mesh)  # get_editor_subsystem() is None in the commandlet
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
box = mesh.get_bounding_box()
unreal.log(f"[setup_susanoo_ribs] imported {mesh.get_path_name()}, bounds {box.min} .. {box.max}")
