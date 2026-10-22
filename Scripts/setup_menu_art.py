"""Main-menu pictures: imports Saved/MenuArt/*.png (Scripts/menu_shots.ps1 -Portraits / -Maps) into /Game/Shinobi/UI/Menu.

  T_Portrait_<Name>.png -> T_Portrait_<Name>  (the SHINOBI tab's cards; ShinobiMenuArt::PortraitPath)
  T_Map_<Id>.png        -> T_Map_<Id>         (the PLAY tab's map rows and backdrop; ShinobiMenuArt::MapImagePath)

Idempotent (re-imports in place). Close the GUI editor, then:
    powershell -File Scripts/run_python.ps1 -Script Scripts/setup_menu_art.py
A map whose shot is poor gets a better view from a CameraActor tagged ShinobiMenuShot placed in that level, then
menu_shots.ps1 -Map <Id> and this script again.
"""
import glob
import os
import unreal

MENU_DIR = "/Game/Shinobi/UI/Menu"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "Saved", "MenuArt")
# Capped near twice their largest on-screen size: a 120x160 card, a full-screen map backdrop.
MAX_SIZE = {"T_Portrait_": 512, "T_Map_": 2048}


def log(message):
    unreal.log(f"[setup_menu_art] {message}")


def import_texture(png, name, max_size):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = MENU_DIR
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"import of {png} produced no texture")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    # Mips: the map image is both a 96 px thumbnail and a full-screen backdrop; a portrait shrinks to its card.
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("never_stream", True)  # menu art shows at once, never a blurry mip
    texture.set_editor_property("max_texture_size", max_size)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    log(f"{png} -> {texture.get_path_name()} ({max_size} px)")


pngs = sorted(glob.glob(os.path.join(SOURCE, "T_*.png")))
if not pngs:
    raise RuntimeError(f"no T_*.png in {SOURCE}: run Scripts/menu_shots.ps1 -Portraits -Maps first")
for png in pngs:
    name = os.path.splitext(os.path.basename(png))[0]
    prefix = next((p for p in MAX_SIZE if name.startswith(p)), None)
    if prefix:
        import_texture(png, name, MAX_SIZE[prefix])
    else:
        log(f"skipped {png}: not a T_Portrait_ / T_Map_ picture")
