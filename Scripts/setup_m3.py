"""M3 assets. Idempotent; close the GUI editor and build the C++ first.

1. AM_Downed: the template's MM_Death_Front_01 as a montage that doesn't auto blend out, so a downed player holds
   the final (lying) frame until revived (AShinobiPlayerCharacter::OnDownedTagChanged stops it).
2. Points both template maps' GameMode Override at the wave arena (AShinobiArenaGameMode). Supersedes step 4 of
   setup_m1.py; the dummy training mode stays reachable with ?game=/Script/ShinobiCoop.ShinobiGameMode.
"""
import unreal

DOWNED_SOURCE = "/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"
ANIM_DIR = "/Game/Shinobi/Anims"
GAME_MODE = "/Script/ShinobiCoop.ShinobiArenaGameMode"
MAPS = ["/Game/ThirdPerson/Lvl_ThirdPerson", "/Game/Variant_Combat/Lvl_Combat"]


def build_downed_montage():
    path = f"{ANIM_DIR}/AM_Downed"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        montage = unreal.load_asset(path)
    else:
        anim = unreal.load_asset(DOWNED_SOURCE)
        if anim is None:
            raise RuntimeError(f"missing {DOWNED_SOURCE}")
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", anim)
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset("AM_Downed", ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {path}")
    montage.set_editor_property("enable_auto_blend_out", False)
    blend = montage.get_editor_property("blend_in")
    blend.set_editor_property("blend_time", 0.15)
    montage.set_editor_property("blend_in", blend)
    if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_m3] {path}")


def set_game_mode(map_path, game_mode):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        raise RuntimeError(f"failed to load map {map_path}")
    settings = world.get_world_settings()
    current = settings.get_editor_property("default_game_mode")
    if current == game_mode:
        unreal.log(f"[setup_m3] {map_path}: GameMode Override already {GAME_MODE}")
        return
    settings.set_editor_property("default_game_mode", game_mode)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        raise RuntimeError(f"failed to save {map_path}")
    unreal.log(f"[setup_m3] {map_path}: GameMode Override {current.get_name() if current else None} -> {GAME_MODE}")


build_downed_montage()
arena = unreal.load_class(None, GAME_MODE)
if arena is None:
    raise RuntimeError(f"class not found: {GAME_MODE} (build the C++ first)")
for map_path in MAPS:
    set_game_mode(map_path, arena)
