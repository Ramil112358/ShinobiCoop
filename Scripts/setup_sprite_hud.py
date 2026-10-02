"""SpriteCook HUD kit and status icons -> UI textures. Idempotent; close the GUI editor.

Sources: SourceArt/SpriteCook/UI/hud_final/<name>.png (cropped from the UI-kit sheet and the status-icon sheet, then
resized near their on-screen size; kit id and asset ids are in SourceArt/SpriteCook/spritecook-assets.json).
Each becomes /Game/Shinobi/UI/Hud/T_Hud_<Name>, read by UShinobiHUDWidget (a missing one falls back to the old drawn look).
UI textures: sRGB, uncompressed (the frames have fine gold detail), no mips, clamped.
"""
import os
import unreal

DEST = "/Game/Shinobi/UI/Hud"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "SpriteCook", "UI", "hud_final")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

for png in sorted(os.listdir(SOURCE)):
    if not png.endswith(".png"):
        continue
    name = "T_Hud_" + "".join(part.capitalize() for part in png[:-4].split("_"))
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, png)
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"import of {png} produced no texture")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"[setup_sprite_hud] {png} -> {texture.get_path_name()}")
