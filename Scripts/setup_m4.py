"""M4 assets: creates Lvl_MainMenu, an empty level whose GameMode Override is AShinobiMenuGameMode (no pawn, just the
main menu widget). Idempotent; close the GUI editor and build the C++ first."""
import unreal

MAP = "/Game/Shinobi/Maps/Lvl_MainMenu"
GAME_MODE = "/Script/ShinobiCoop.ShinobiMenuGameMode"

game_mode = unreal.load_class(None, GAME_MODE)
if game_mode is None:
    raise RuntimeError(f"class not found: {GAME_MODE} (build the C++ first)")
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
else:
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if world is None:
    raise RuntimeError(f"failed to open or create {MAP}")
settings = world.get_world_settings()
if settings.get_editor_property("default_game_mode") != game_mode or not unreal.EditorAssetLibrary.does_asset_exist(MAP):
    settings.set_editor_property("default_game_mode", game_mode)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
unreal.log(f"[setup_m4] {MAP}: GameMode Override -> {GAME_MODE}")
