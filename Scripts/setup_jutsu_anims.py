"""Jutsu cast animations: imports the per-phase clips Scripts/character/build_jutsu_anims.py writes to
SourceArt/Mixamo/Jutsu (<Jutsu>_<Seal|Charge|Release>.fbx, on the Mixamo "Power Up" skeleton), retargets them onto
Manny with the RTG_MixamoToManny retargeter that setup_chakra_pose.py built, and wraps each in a montage,
AM_<Jutsu>_<Phase>, which UShinobiJutsuAbility plays phase by phase (SealMontage, ChargeMontage, ReleaseMontage).
Idempotent: re-running reimports, re-retargets (overwriting in place, so montage references survive) and re-tunes.
One montage per phase rather than sections in one montage: UE Python can't author montage sections.

Run setup_chakra_pose.py first (it creates the Mixamo skeleton/mesh and the retargeter this reuses), and
build_jutsu_anims.py after any pose change. Close the GUI editor before running.
"""
import os
import unreal

SOURCE_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Mixamo/Jutsu")
IMPORT_DIR = "/Game/Shinobi/Mixamo/Jutsu"
ANIM_DIR = "/Game/Shinobi/Anims"
MIXAMO_MESH_PATH = "/Game/Shinobi/Mixamo/Power_Up_30fps.Power_Up_30fps"
MIXAMO_SKELETON_PATH = "/Game/Shinobi/Mixamo/Power_Up_30fps_Skeleton.Power_Up_30fps_Skeleton"
MANNY_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
RETARGETER_PATH = "/Game/Shinobi/Retarget/RTG_MixamoToManny.RTG_MixamoToManny"

JUTSU = ("Fireball", "Chidori", "Rasengan", "Amaterasu")
# Phase -> (blend in, blend out) seconds. The seals and the charge hand over to the next phase quickly; the release
# blends back out to locomotion more slowly.
PHASES = {"Seal": (0.1, 0.1), "Charge": (0.1, 0.1), "Release": (0.05, 0.25)}

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"missing {path} (run setup_chakra_pose.py first)")
    return asset


def import_clip(jutsu, fbx_name, skeleton):
    """Animation-only import onto the existing Mixamo skeleton; returns the AnimSequence MX_<Jutsu>."""
    fbx = os.path.join(SOURCE_DIR, fbx_name + ".fbx")
    if not os.path.exists(fbx):
        raise RuntimeError(f"missing {fbx}")
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.skeleton = skeleton
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False

    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = IMPORT_DIR
    task.destination_name = f"MX_{jutsu}"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    anims = [o for o in task.get_objects() if isinstance(o, unreal.AnimSequence)]
    if not anims:
        raise RuntimeError(f"import of {fbx} produced no animation: {[o.get_path_name() for o in task.get_objects()]}")
    unreal.log(f"[setup_jutsu_anims] imported {fbx_name} -> {anims[0].get_path_name()}")
    return anims[0]


def retarget(source_anim, jutsu, mixamo_mesh, manny_mesh, retargeter):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(source_anim.get_path_name())]
    inputs.source_mesh = mixamo_mesh
    inputs.target_mesh = manny_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = source_anim.get_name()
    inputs.replace = f"AS_{jutsu}"
    inputs.target_path = ANIM_DIR
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    inputs.retain_additive_flags = True
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError(f"retarget produced nothing for {source_anim.get_path_name()}")
    path = f"{ANIM_DIR}/AS_{jutsu}"
    anim = load(path)
    # Played as a stationary cast: no root motion, and the root is pinned so a clip that steps doesn't drag
    # the mesh away from the capsule.
    anim.set_editor_property("enable_root_motion", False)
    anim.set_editor_property("force_root_lock", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(anim):
        raise RuntimeError(f"failed to save {path}")
    return anim


def remove_old_single_clip(jutsu):
    """The one-clip-per-jutsu assets this script made before the phase split."""
    for path in (f"{ANIM_DIR}/AM_{jutsu}", f"{ANIM_DIR}/AS_{jutsu}", f"{IMPORT_DIR}/MX_{jutsu}"):
        if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"could not delete {path}")


def build_montage(jutsu, anim, blends):
    path = f"{ANIM_DIR}/AM_{jutsu}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        montage = load(path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", anim)
        montage = TOOLS.create_asset(f"AM_{jutsu}", ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {path}")
    for prop, seconds in zip(("blend_in", "blend_out"), blends):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property("blend_time", seconds)
        montage.set_editor_property(prop, blend)
    if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_jutsu_anims] {path}: {anim.get_play_length():.2f}s")


if not unreal.EditorAssetLibrary.does_directory_exist(IMPORT_DIR):
    unreal.EditorAssetLibrary.make_directory(IMPORT_DIR)
mixamo_mesh = load(MIXAMO_MESH_PATH)
skeleton = load(MIXAMO_SKELETON_PATH)
manny_mesh = load(MANNY_MESH_PATH)
retargeter = load(RETARGETER_PATH)
for jutsu in JUTSU:
    remove_old_single_clip(jutsu)
    for phase, blends in PHASES.items():
        name = f"{jutsu}_{phase}"
        source = import_clip(name, name, skeleton)
        build_montage(name, retarget(source, name, mixamo_mesh, manny_mesh, retargeter), blends)
