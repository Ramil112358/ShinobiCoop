"""M1 asset setup. Idempotent: safe to re-run.

1. Duplicates the template's dash animation with root motion disabled (see create_dash_sequence).
2. Creates melee/dodge montages from the template's Manny animations (DefaultSlot, matches ABP_Manny_Combat).
3. Tunes each montage's blend-in time and (for the dodge) playback rate (see tune_montages).
4. Points Lvl_ThirdPerson's GameMode Override (which the template sets to BP_ThirdPersonGameMode) at ShinobiGameMode.
"""
import unreal

ANIM_DIR = "/Game/Shinobi/Anims"
DASH_SOURCE = "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash"
DASH_SEQUENCE = f"{ANIM_DIR}/AS_Dodge"
MONTAGES = {
    "AM_Melee_1": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01",
    "AM_Melee_2": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02",
    "AM_Melee_3": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03",
    "AM_Dodge": DASH_SEQUENCE,
}
# blend_in: UAnimMontage.BlendIn is an FAlphaBlend struct (Engine/Public/AlphaBlend.h), not a plain
# float, so it has to be read, have its blend_time set, and written back -- setting a property on the
# copy get_editor_property() returns does not touch the montage until it's assigned back.
# rate_scale: UAnimSequenceBase.RateScale (Engine/Classes/Animation/AnimSequenceBase.h). MM_Dash is
# ~0.97 s and GA_Dodge plays AM_Dodge with bStopWhenAbilityEnds=false (see GA_Dodge.cpp) -- it doesn't
# stop the montage when the 0.25 s dash (GA_Dodge::Duration) ends, so without this the character keeps
# playing the dash pose for ~0.7 s after it has physically stopped moving. At 3x it finishes in ~0.32 s,
# close to when the dash itself ends.
MONTAGE_TUNING = {
    "AM_Dodge": {"blend_in": 0.05, "rate_scale": 3.0},
    "AM_Melee_1": {"blend_in": 0.1},
    "AM_Melee_2": {"blend_in": 0.1},
    "AM_Melee_3": {"blend_in": 0.1},
}
MAP = "/Game/ThirdPerson/Lvl_ThirdPerson"
GAME_MODE = "/Script/ShinobiCoop.ShinobiGameMode"


def create_dash_sequence():
    # GA_Dodge drives the dash with an AbilityTask_ApplyRootMotionConstantForce. MM_Dash has its own
    # baked root motion (bEnableRootMotion=True, ~0.97 s), and ABP_Manny_Combat uses
    # RootMotionFromMontagesOnly; CharacterMovementComponent::ApplyRootMotionToVelocity treats anim
    # root motion as taking precedence over any other root motion source while it plays
    # (CharacterMovementComponent.cpp, ApplyRootMotionToVelocity / the ~2952 HasAnimRootMotion() branch:
    # "Animation root motion overrides Velocity and currently doesn't allow any other root motion
    # sources"), so playing MM_Dash directly would silently replace our 600-unit/0.25 s dash with the
    # template's own translation. Duplicate it and turn root motion off so the montage is visual only;
    # force_root_lock keeps the pelvis from sliding relative to the capsule. Melee montages keep their
    # own root motion (a forward step on the attack is fine and not fighting another root motion task).
    if unreal.EditorAssetLibrary.does_asset_exist(DASH_SEQUENCE):
        sequence = unreal.load_asset(DASH_SEQUENCE)
        unreal.log(f"[setup_m1] exists: {DASH_SEQUENCE}")
    else:
        sequence = unreal.EditorAssetLibrary.duplicate_asset(DASH_SOURCE, DASH_SEQUENCE)
        if sequence is None:
            raise RuntimeError(f"failed to duplicate {DASH_SOURCE} -> {DASH_SEQUENCE}")
        unreal.log(f"[setup_m1] created: {DASH_SEQUENCE}")

    # Re-applied on every run, not just at creation: this is the property that makes the whole dodge
    # work (see above), so if it's ever reset -- a reimport, a manual edit, someone re-duplicating the
    # source by hand -- re-running this script must be enough to put it right again rather than silently
    # leaving the dodge broken until someone remembers to check two flags on an animation asset.
    sequence.set_editor_property("enable_root_motion", False)
    sequence.set_editor_property("force_root_lock", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence):
        raise RuntimeError(f"failed to save {DASH_SEQUENCE}")


def create_montages():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, source_path in MONTAGES.items():
        target = f"{ANIM_DIR}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            unreal.log(f"[setup_m1] exists: {target}")
            continue
        source = unreal.load_asset(source_path)
        if source is None:
            raise RuntimeError(f"missing source animation {source_path}")
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", source)
        montage = tools.create_asset(name, ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {target}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
            raise RuntimeError(f"failed to save {target}")
        unreal.log(f"[setup_m1] created: {target}")


def tune_montages():
    # Applied on every run, not just at creation, for the same reason as create_dash_sequence's
    # root-motion flags: re-running this script should be enough to put the tuning back if it's ever
    # reset by hand or by re-creating a montage from its source animation.
    for name, settings in MONTAGE_TUNING.items():
        target = f"{ANIM_DIR}/{name}"
        montage = unreal.load_asset(target)
        if montage is None:
            raise RuntimeError(f"missing montage to tune: {target}")
        if "blend_in" in settings:
            blend_in = montage.get_editor_property("blend_in")
            blend_in.set_editor_property("blend_time", settings["blend_in"])
            montage.set_editor_property("blend_in", blend_in)
        if "rate_scale" in settings:
            montage.set_editor_property("rate_scale", settings["rate_scale"])
        if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
            raise RuntimeError(f"failed to save {target}")
        unreal.log(f"[setup_m1] tuned: {target}")


def set_game_mode():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if world is None:
        raise RuntimeError(f"failed to load map {MAP}")
    game_mode = unreal.load_class(None, GAME_MODE)
    if game_mode is None:
        raise RuntimeError(f"class not found: {GAME_MODE} (build the C++ first)")
    world_settings = world.get_world_settings()
    # Unreal's Python bindings intern one wrapper object per underlying UObject, so this equality
    # check is reliable (verified empirically: two independently-obtained wrappers for the same class
    # are `is`-identical). Skipping the save when it's already correct avoids resaving the map -- and
    # so a new LFS blob -- on every single run, which SaveMap did unconditionally before.
    if world_settings.get_editor_property("default_game_mode") == game_mode:
        unreal.log(f"[setup_m1] GameMode Override already {GAME_MODE}")
        return
    world_settings.set_editor_property("default_game_mode", game_mode)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
    unreal.log(f"[setup_m1] {MAP} GameMode Override -> {GAME_MODE}")


create_dash_sequence()
create_montages()
tune_montages()
set_game_mode()
