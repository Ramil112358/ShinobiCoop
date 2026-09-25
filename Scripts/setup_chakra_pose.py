"""Chakra charge pose: imports the Mixamo "Power Up" animation, retargets it onto the UE5 mannequin
(Manny), and builds the looping AM_ChakraCharge montage GA_ChakraCharge plays. Idempotent: each stage
skips (and logs "exists") if its output asset is already there.

Close the GUI editor before running: an editor holding these assets would overwrite them on its next
Save All.

Stages (see the plan, docs/superpowers/plans/2026-09-25-chakra-charge-visuals.md, Task 4):
  1. Import SourceArt/Mixamo/Power_Up_30fps.fbx into /Game/Shinobi/Mixamo (skeletal mesh + skeleton +
     animation; no physics asset, no materials).
  2. Build /Game/Shinobi/Retarget/IK_Mixamo (on the imported mesh) and IK_Manny (on SKM_Manny_Simple):
     auto-generated retarget definition + auto FBIK on each.
  3. Build /Game/Shinobi/Retarget/RTG_MixamoToManny: default op stack, chains fuzzy-auto-mapped, target
     bones auto-aligned (Mixamo's T-pose vs Manny's A-pose).
  4. Batch-retarget the imported animation to /Game/Shinobi/Anims/AS_PowerUp.
  5. Build /Game/Shinobi/Anims/AM_ChakraCharge (AnimMontageFactory, DefaultSlot, tuned blend in/out).
Then a read-only verification: AS_PowerUp uses Manny's skeleton, has a non-zero length and frame count,
AM_ChakraCharge references it, and a few key bones actually move between sampled times (not a frozen
T-pose). See verify_result()'s docstring for what this can and can't confirm.

Confirmed against the 5.8 engine source before writing this (see the plan's Task 4 for file citations):
- AssetImportTask + FbxImportUI still works as the FBX import route in 5.8: even though
  Interchange.FeatureFlags.Import.FBX defaults to true (InterchangeFbxTranslator.cpp), AssetTools's
  ImportAssetTasks (Engine/Source/Developer/AssetTools/Private/AssetTools.cpp) auto-detects Interchange
  is enabled and calls InterchangeManager.ConvertImportData() to translate the legacy FbxImportUI options
  into an Interchange pipeline automatically -- no separate Interchange-specific Python API is needed.
- UIKRigDefinitionFactory.create_new_ik_rig_asset(path, name) is a BlueprintCallable static convenience
  (IKRigDefinitionFactory.h). UIKRetargetFactory has no such static, so IK rig creation differs: IK_Manny
  and RTG_MixamoToManny go through AssetTools.create_asset with an explicit factory instance.
- FAutoCharacterizer (IKRigAutoCharacterizer.cpp) strips the skeleton's largest common bone-name prefix
  before matching a template (FAbstractHierarchy::GenerateCleanBoneNames), and it ships a "Mixamo"
  template built from un-prefixed bone names ("Hips", "Spine", ...), so a Mixamo skeleton's "mixamorig:"
  prefix is stripped automatically and the template still matches; Manny matches the built-in UE5
  mannequin template. Both templates set a pelvis bone and full bone chains, so
  apply_auto_generated_retarget_definition() is expected to return true for both meshes.
- AnimMontageFactory always names its one starting section "Default" (EnsureStartingSection,
  AnimMontageFactory.cpp) and its one slot "DefaultSlot" (UAnimMontage::UAnimMontage ->
  AddSlot(FAnimSlotGroup::DefaultSlotName)), matching ABP_Manny_Combat's DefaultSlot node and
  GA_ChakraCharge.cpp's CurrentMontageSetNextSectionName("Default", "Default") call -- same as every
  other montage Scripts/setup_m1.py builds, so no extra slot/section wiring is needed here.
- ERetargetSourceOrTarget, EAutoMapChainType and ERetargetAutoAlignMethod are plain UENUM()s (not all
  BlueprintType), which Python still binds: unreal.RetargetSourceOrTarget.SOURCE/TARGET,
  unreal.AutoMapChainType.EXACT/FUZZY/CLEAR, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN/...
"""
import unreal

MIXAMO_DIR = "/Game/Shinobi/Mixamo"
RETARGET_DIR = "/Game/Shinobi/Retarget"
ANIM_DIR = "/Game/Shinobi/Anims"
MANNY_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
FBX_SOURCE = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir() + "SourceArt/Mixamo/Power_Up_30fps.fbx"
)
AS_POWER_UP = f"{ANIM_DIR}/AS_PowerUp"
AM_CHAKRA_CHARGE = f"{ANIM_DIR}/AM_ChakraCharge"

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def find_imported_mixamo_assets():
    """Returns (mesh, skeleton, anim) already under MIXAMO_DIR, or (None, None, None) if nothing's there
    yet (list_assets on a not-yet-existing directory just returns an empty list)."""
    mesh = skeleton = anim = None
    for path in unreal.EditorAssetLibrary.list_assets(MIXAMO_DIR, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.SkeletalMesh):
            mesh = asset
        elif isinstance(asset, unreal.Skeleton):
            skeleton = asset
        elif isinstance(asset, unreal.AnimSequence):
            anim = asset
    return mesh, skeleton, anim


def import_mixamo_fbx():
    mesh, skeleton, anim = find_imported_mixamo_assets()
    if mesh and skeleton and anim:
        unreal.log(f"[setup_chakra_pose] exists: {mesh.get_path_name()}")
        return mesh, skeleton, anim

    if not unreal.EditorAssetLibrary.does_directory_exist(MIXAMO_DIR):
        unreal.EditorAssetLibrary.make_directory(MIXAMO_DIR)

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.create_physics_asset = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False

    task = unreal.AssetImportTask()
    task.filename = FBX_SOURCE
    task.destination_path = MIXAMO_DIR
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options

    TOOLS.import_asset_tasks([task])
    objects = task.get_objects()
    if not objects:
        raise RuntimeError(f"import produced no objects for {FBX_SOURCE}")

    mesh = next((o for o in objects if isinstance(o, unreal.SkeletalMesh)), None)
    skeleton = next((o for o in objects if isinstance(o, unreal.Skeleton)), None)
    anim = next((o for o in objects if isinstance(o, unreal.AnimSequence)), None)
    if not (mesh and skeleton and anim):
        raise RuntimeError(
            f"import of {FBX_SOURCE} did not produce mesh+skeleton+animation, got: "
            f"{[o.get_path_name() for o in objects]}"
        )
    unreal.log(
        f"[setup_chakra_pose] created: {mesh.get_path_name()}, {skeleton.get_path_name()}, "
        f"{anim.get_path_name()}"
    )
    return mesh, skeleton, anim


def build_ik_rig(name, mesh):
    path = f"{RETARGET_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"[setup_chakra_pose] exists: {path}")
        return unreal.load_asset(path)

    rig = unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(RETARGET_DIR, name)
    if rig is None:
        raise RuntimeError(f"failed to create {path}")

    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError(f"failed to set skeletal mesh on {path}")
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"auto-characterization did not recognize the skeleton for {path}")
    if not controller.apply_auto_fbik():
        raise RuntimeError(f"auto FBIK setup failed for {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(rig):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_chakra_pose] created: {path}")
    return rig


def build_retargeter(ik_mixamo, ik_manny):
    path = f"{RETARGET_DIR}/RTG_MixamoToManny"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f"[setup_chakra_pose] exists: {path}")
        return unreal.load_asset(path)

    retargeter = TOOLS.create_asset(
        "RTG_MixamoToManny", RETARGET_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory()
    )
    if retargeter is None:
        raise RuntimeError(f"failed to create {path}")

    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_mixamo)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_manny)
    controller.add_default_ops()  # Pelvis Motion, FK Chains, IK Chains, IK Solve, Root Motion
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, ik_mixamo)
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_manny)
    controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # Mixamo exports in a T-pose; Manny's reference pose is an A-pose. Align Manny's mapped-chain bones
    # to match Mixamo's chain directions so the retarget doesn't wind up with twisted/crossed arms.
    controller.auto_align_all_bones(
        unreal.RetargetSourceOrTarget.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN
    )

    if not unreal.EditorAssetLibrary.save_loaded_asset(retargeter):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_chakra_pose] created: {path}")
    return retargeter


def retarget_animation(mixamo_mesh, manny_mesh, retargeter, source_anim):
    if unreal.EditorAssetLibrary.does_asset_exist(AS_POWER_UP):
        unreal.log(f"[setup_chakra_pose] exists: {AS_POWER_UP}")
        return unreal.load_asset(AS_POWER_UP)

    source_data = unreal.EditorAssetLibrary.find_asset_data(source_anim.get_path_name())
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [source_data]
    inputs.source_mesh = mixamo_mesh
    inputs.target_mesh = manny_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.target_path = ANIM_DIR
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    inputs.retain_additive_flags = True

    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError(f"batch retarget produced no assets for {source_anim.get_path_name()}")

    # Search/Replace/Prefix/Suffix are all left blank, so the retargeted asset keeps the source
    # AnimSequence's own name (Mixamo FBX animation takes are typically named "mixamo.com", not the
    # destination filename) -- rename it to the plan's AS_PowerUp.
    result_path = str(results[0].package_name)
    if result_path != AS_POWER_UP:
        if not unreal.EditorAssetLibrary.rename_asset(result_path, AS_POWER_UP):
            raise RuntimeError(f"failed to rename {result_path} -> {AS_POWER_UP}")

    result = unreal.load_asset(AS_POWER_UP)
    if result is None:
        raise RuntimeError(f"retarget result not found at {AS_POWER_UP}")
    unreal.log(f"[setup_chakra_pose] created: {AS_POWER_UP}")
    return result


def build_montage(as_power_up):
    if not unreal.EditorAssetLibrary.does_asset_exist(AM_CHAKRA_CHARGE):
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", as_power_up)
        montage = TOOLS.create_asset("AM_ChakraCharge", ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {AM_CHAKRA_CHARGE}")
        unreal.log(f"[setup_chakra_pose] created: {AM_CHAKRA_CHARGE}")
    else:
        montage = unreal.load_asset(AM_CHAKRA_CHARGE)
        unreal.log(f"[setup_chakra_pose] exists: {AM_CHAKRA_CHARGE}")

    # Re-applied every run, like setup_m1.py's tune_montages: FAlphaBlend has to be read, have its
    # blend_time set, and written back -- setting a property on the copy get_editor_property() returns
    # does not touch the montage until it's assigned back.
    blend_in = montage.get_editor_property("blend_in")
    blend_in.set_editor_property("blend_time", 0.15)
    montage.set_editor_property("blend_in", blend_in)
    blend_out = montage.get_editor_property("blend_out")
    blend_out.set_editor_property("blend_time", 0.2)
    montage.set_editor_property("blend_out", blend_out)
    if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
        raise RuntimeError(f"failed to save {AM_CHAKRA_CHARGE}")
    unreal.log(f"[setup_chakra_pose] tuned: {AM_CHAKRA_CHARGE}")
    return montage


def verify_result(as_power_up, am_chakra_charge, manny_mesh):
    """Read-only sanity check, run every time (not just on first creation): confirms AS_PowerUp is on
    Manny's skeleton, has a real length/frame count, AM_ChakraCharge references it, and that a few key
    bones actually move over the clip (not a frozen pose -- would indicate a broken chain mapping).
    What this CANNOT confirm headlessly: whether the retarget pose actually *looks* right (no twisted
    limbs, correct T-pose/A-pose alignment, no foot sliding) -- that needs the user's eyes, Task 5."""
    manny_skeleton = manny_mesh.get_editor_property("skeleton")
    anim_skeleton = as_power_up.get_editor_property("skeleton")
    if anim_skeleton != manny_skeleton:
        raise RuntimeError(
            f"[verify] AS_PowerUp skeleton is "
            f"{anim_skeleton.get_path_name() if anim_skeleton else None}, expected Manny's "
            f"{manny_skeleton.get_path_name()}"
        )

    play_length = as_power_up.get_play_length()
    num_frames = unreal.AnimationLibrary.get_num_frames(as_power_up)
    if play_length <= 0.0 or num_frames <= 1:
        raise RuntimeError(f"[verify] AS_PowerUp has no length: {play_length}s, {num_frames} frames")

    slot_tracks = am_chakra_charge.get_editor_property("slot_anim_tracks")
    referenced = None
    if slot_tracks:
        segments = slot_tracks[0].get_editor_property("anim_track").get_editor_property("anim_segments")
        if segments:
            referenced = segments[0].get_editor_property("anim_reference")
    if referenced != as_power_up:
        raise RuntimeError(f"[verify] AM_ChakraCharge does not reference AS_PowerUp (got {referenced})")

    moved_bones = []
    for bone in ("upperarm_l", "upperarm_r", "hand_l", "hand_r"):
        start = unreal.AnimationLibrary.get_bone_pose_for_time(as_power_up, bone, 0.0, False)
        end = unreal.AnimationLibrary.get_bone_pose_for_time(as_power_up, bone, play_length * 0.5, False)
        q0, q1 = start.rotation, end.rotation
        # |dot| close to 1 means near-identical rotation; use this rather than .rotator() since FQuat's
        # X/Y/Z/W are the confirmed-reflected properties (NoExportTypes.h) and Rotator() is not a UFUNCTION.
        dot = abs(q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w)
        moved_bones.append(f"{bone}={'moves' if dot < 0.999 else 'STATIC'}")

    unreal.log(
        f"[setup_chakra_pose] verify: skeleton=Manny length={play_length:.2f}s frames={num_frames} "
        f"section=Default slot=DefaultSlot bones=[{', '.join(moved_bones)}]"
    )
    unreal.log(
        "[setup_chakra_pose] verify: pose correctness (T-pose/A-pose alignment, no twisted limbs, foot "
        "placement) needs the user's eyes in Task 5 -- cannot be checked headlessly."
    )


mixamo_mesh, mixamo_skeleton, mixamo_anim = import_mixamo_fbx()
ik_mixamo = build_ik_rig("IK_Mixamo", mixamo_mesh)
manny_mesh = unreal.load_asset(MANNY_MESH_PATH)
if manny_mesh is None:
    raise RuntimeError(f"missing {MANNY_MESH_PATH}")
ik_manny = build_ik_rig("IK_Manny", manny_mesh)
retargeter = build_retargeter(ik_mixamo, ik_manny)
as_power_up = retarget_animation(mixamo_mesh, manny_mesh, retargeter, mixamo_anim)
am_chakra_charge = build_montage(as_power_up)
verify_result(as_power_up, am_chakra_charge, manny_mesh)
