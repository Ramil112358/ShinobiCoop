"""HUD art: the unit-frame portrait and the jutsu icons. Idempotent (re-imports in place); close the GUI editor.

Source PNGs in SourceArt/UI/ (painted in Gemini / ChatGPT from the prompts in the HUD commit; square, ~1024 px):
  portrait.png                 -> /Game/Shinobi/UI/T_Portrait        (UShinobiHUDWidget's PortraitPath)
  jutsu_<name>.png             -> /Game/Shinobi/UI/T_Jutsu_<Name>    (UShinobiJutsuAbility::Icon in each GA constructor)
A missing PNG is skipped: the HUD shows a kanji in the portrait and the jutsu's name in its slot instead.

UI textures: sRGB, UserInterface2D compression, capped at about twice their on-screen size (the portrait is 112 px,
a slot 80 px).
"""
import os
import unreal

UI_DIR = "/Game/Shinobi/UI"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "UI")
TEXTURES = {
    "portrait.png": ("T_Portrait", 256),
    "jutsu_fireball.png": ("T_Jutsu_Fireball", 128),
    "jutsu_chidori.png": ("T_Jutsu_Chidori", 128),
    "jutsu_rasengan.png": ("T_Jutsu_Rasengan", 128),
    "jutsu_amaterasu.png": ("T_Jutsu_Amaterasu", 128),
    "jutsu_teleport.png": ("T_Jutsu_Teleport", 128),
    "jutsu_kamui.png": ("T_Jutsu_Kamui", 128),
    "jutsu_almightypush.png": ("T_Jutsu_AlmightyPush", 128),
    "jutsu_earthwall.png": ("T_Jutsu_EarthWall", 128),
    "jutsu_waterwall.png": ("T_Jutsu_WaterWall", 128),
    "jutsu_firemeteor.png": ("T_Jutsu_FireMeteor", 128),
    "jutsu_icemeteor.png": ("T_Jutsu_IceMeteor", 128),
    "jutsu_waterdragon.png": ("T_Jutsu_WaterDragon", 128),
    "jutsu_susanoo.png": ("T_Jutsu_Susanoo", 128),
    "jutsu_summontoad.png": ("T_Jutsu_SummonToad", 128),
    "jutsu_summonmisha.png": ("T_Jutsu_SummonMisha", 128),
    "jutsu_summonpabu.png": ("T_Jutsu_SummonPabu", 128),
    "jutsu_summonpusho.png": ("T_Jutsu_SummonPusho", 128),
    "jutsu_summondragon.png": ("T_Jutsu_SummonDragon", 128),
    "jutsu_summoncrimson.png": ("T_Jutsu_SummonCrimson", 128),
}


def log(message):
    unreal.log(f"[setup_hud] {message}")


def import_texture(png, name, max_size):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = UI_DIR
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"import of {png} produced no texture")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    # Mips, unlike the UI group's default: a non-power-of-two source (the painted PNGs are ~1254 px) only shrinks to
    # max_texture_size by dropping top mips, and the mips keep the 80 px slots from shimmering.
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("max_texture_size", max_size)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    log(f"{png} -> {texture.get_path_name()} ({max_size} px)")


ONLY = [n for n in os.environ.get("HUD_ONLY", "").split(",") if n]  # e.g. HUD_ONLY=T_Jutsu_SummonPusho: import just those
for file, (name, max_size) in ((f, t) for f, t in TEXTURES.items() if not ONLY or t[0] in ONLY):
    png = os.path.join(SOURCE, file)
    if os.path.exists(png):
        import_texture(png, name, max_size)
    else:
        log(f"skipped {file}: not in {SOURCE}")
