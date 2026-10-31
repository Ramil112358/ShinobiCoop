"""Shortens the soft-particle depth fade of the Fab fire pack's flame and smoke materials (/Game/M5VFXVOL2/Materials), so a fire stands ON
a wall instead of a radius away from it. The pack fades a sprite out over Fade_Depth (flames, 100-300 uu) / FadeDistance (smoke,
200-300 uu) of the geometry behind it; against a wall seen head-on every flame sprite is within that distance, so a fire placed on the
wall lost its flames. On the ground, where the floor is seen at a slant, it showed little, so the pack never needed less.
Idempotent. Close the GUI editor. Run with Scripts/run_python.ps1.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
FLAME_FADE = 12.0
SMOKE_FADE = 40.0
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.wait_for_completion()
count = 0
for folder, param, value in (("Fire_Inst", "Fade_Depth", FLAME_FADE), ("Fireloop_Inst", "Fade_Depth", FLAME_FADE), ("Smoke_Inst", "FadeDistance", SMOKE_FADE)):
    for a in reg.get_assets_by_path(f"/Game/M5VFXVOL2/Materials/{folder}", recursive=True):
        mi = a.get_asset()
        if not isinstance(mi, unreal.MaterialInstanceConstant):
            continue
        before = MEL.get_material_instance_scalar_parameter_value(mi, param)
        MEL.set_material_instance_scalar_parameter_value(mi, param, value)
        if MEL.get_material_instance_scalar_parameter_value(mi, param) == value and before != value:
            unreal.EditorAssetLibrary.save_loaded_asset(mi)
            count += 1
unreal.log(f"FADESET set the fade on {count} instances (assets seen: {sum(len(reg.get_assets_by_path(f'/Game/M5VFXVOL2/Materials/{f}', recursive=True)) for f in ('Fire_Inst', 'Fireloop_Inst', 'Smoke_Inst'))})")
