"""Imports the Perfect Susanoo into /Game/Shinobi/Jutsu/Susanoo (run Scripts/character/build_susanoo.py and
Scripts/character/make_susanoo_sword.py first). Idempotent. Close the GUI editor first.

- T_Susanoo: the armour texture (Tripo's base colour, with the thin cyan panel lines M_Susanoo glows).
- SK_Susanoo (+ SK_Susanoo_Skeleton): the 180 cm figure, scaled up to 12 m by UShinobiSusanooComponent. No physics asset.
- A_Susanoo_<Clip>: Idle and Slash1..N on that skeleton.
- SM_SusanooSword: the chakra blade it holds in each hand (no collision).
- M_Susanoo (Scripts/setup_vfx.py's build_susanoo, rebuilt here so it picks up T_Susanoo) on the figure's slot.
Removes the old ribcage figure, SM_Susanoo.
"""
import glob
import os
import sys

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))
import setup_vfx  # noqa: E402  (helpers and build_susanoo; its own run is behind __main__)

BUILD = os.path.join(ROOT, "SourceArt", "Susanoo", "Build")
DEST = "/Game/Shinobi/Jutsu/Susanoo"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log(f"[setup_susanoo] {message}")


def import_file(path, name, options=None):
    if not os.path.exists(path):
        raise RuntimeError(f"missing {path}")
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    if options:
        task.options = options
    TOOLS.import_asset_tasks([task])
    return task.get_objects()


def skeletal_options(skeleton=None):
    options = unreal.FbxImportUI()
    options.import_mesh = skeleton is None
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeleton is None else unreal.FBXImportType.FBXIT_ANIMATION
    options.import_animations = skeleton is not None
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    if skeleton is not None:
        options.skeleton = skeleton
    return options


textures = glob.glob(os.path.join(ROOT, "SourceArt", "Susanoo", "Models", "tripo_rig", "*.fbm", "*basecolor*"))
if not textures:
    raise RuntimeError("no Tripo base colour texture in SourceArt/Susanoo/Models/tripo_rig/*.fbm")
import_file(textures[0], "T_Susanoo")
setup_vfx.build_susanoo()

import_file(os.path.join(BUILD, "SK_Susanoo.fbx"), "SK_Susanoo", skeletal_options())
mesh = unreal.load_asset(f"{DEST}/SK_Susanoo")
if not isinstance(mesh, unreal.SkeletalMesh):
    raise RuntimeError("SK_Susanoo was not imported")
slots = list(mesh.get_editor_property("materials"))
for slot in slots:
    slot.set_editor_property("material_interface", unreal.load_asset(f"{setup_vfx.VFX_DIR}/M_Susanoo"))
mesh.set_editor_property("materials", slots)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
skeleton = mesh.get_editor_property("skeleton")

clips = sorted(glob.glob(os.path.join(BUILD, "A_Susanoo_*.fbx")))
for clip in clips:
    name = os.path.splitext(os.path.basename(clip))[0]
    if not any(isinstance(o, unreal.AnimSequence) for o in import_file(clip, name, skeletal_options(skeleton))):
        raise RuntimeError(f"{clip} produced no animation")

import_file(os.path.join(ROOT, "SourceArt", "Susanoo", "susanoo_sword.fbx"), "SM_SusanooSword")
sword = unreal.load_asset(f"{DEST}/SM_SusanooSword")
if not isinstance(sword, unreal.StaticMesh):
    raise RuntimeError("SM_SusanooSword was not imported")
unreal.StaticMeshEditorSubsystem().remove_collisions(sword)  # the commandlet has no editor subsystem to get
unreal.EditorAssetLibrary.save_loaded_asset(sword)
if unreal.EditorAssetLibrary.does_asset_exist(f"{DEST}/M_SusanooBlade_Slot"):  # the FBX's placeholder slot; the component sets the glow
    unreal.EditorAssetLibrary.delete_asset(f"{DEST}/M_SusanooBlade_Slot")

old = "/Game/Shinobi/Jutsu/SM_Susanoo"
if unreal.EditorAssetLibrary.does_asset_exist(old):
    unreal.EditorAssetLibrary.delete_asset(old)
    log("removed the old ribcage " + old)
bounds = mesh.get_bounds()
log(f"imported SK_Susanoo (bounds origin {bounds.origin}, extent {bounds.box_extent}), {len(clips)} clips, SM_SusanooSword")
log("assets: " + ", ".join(sorted(os.path.basename(p) for p in unreal.EditorAssetLibrary.list_assets(DEST))))
