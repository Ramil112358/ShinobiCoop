"""Chakra charge pose: imports the Mixamo "Power Up" animation, retargets it onto the UE5 mannequin
(Manny), and builds the looping AM_ChakraCharge montage GA_ChakraCharge plays. Idempotent: import and IK
rig creation skip (log "exists") once their output asset exists; the retargeter's op-stack configuration
and the batch retarget itself are re-applied on EVERY run so a tuning fix here takes effect on an asset
built by an earlier version of this script, not just on a fresh one.

Close the GUI editor before running: an editor holding these assets would overwrite them on its next
Save All.

Stages (see the plan, docs/superpowers/plans/2026-09-25-chakra-charge-visuals.md, Task 4):
  1. Import SourceArt/Mixamo/Power_Up_30fps.fbx into /Game/Shinobi/Mixamo (skeletal mesh + skeleton +
     animation; no physics asset, no materials). Skip if already imported.
  2. Build /Game/Shinobi/Retarget/IK_Mixamo (on the imported mesh) and IK_Manny (on SKM_Manny_Simple):
     auto-generated retarget definition + auto FBIK on each. Skip if already built.
  3. (Re)configure /Game/Shinobi/Retarget/RTG_MixamoToManny every run: op stack wiped and rebuilt, EXACT chain
     mapping, target bones auto-aligned, Run IK Rig op disabled, Root Motion op reconfigured. See
     configure_retargeter()'s docstring -- this matches Epic's own auto-retarget setup
     (SRetargetAnimAssetsWindow.cpp's FProceduralRetargetAssets::AutoGenerateIKRetargetAsset).
  4. Batch-retarget the imported animation to /Game/Shinobi/Anims/AS_PowerUp every run (overwriting any
     previous result in place, so references to it -- e.g. AM_ChakraCharge -- survive).
  5. Build /Game/Shinobi/Anims/AM_ChakraCharge (AnimMontageFactory, DefaultSlot, tuned blend in/out).
Then a read-only verification: AS_PowerUp uses Manny's skeleton, has a non-zero length and frame count,
AM_ChakraCharge references it via its real section/slot names, several key bones actually move across the
whole clip (compared against the source Mixamo bone, not just against themselves), and a rough loop-seam
indicator is logged for the Task 5 visual check. See verify_result()'s docstring for what this can and
can't confirm.

Confirmed against the 5.8 engine source (and, where noted, empirically against the actual imported/built
assets via one-off Python queries) before writing this:
- AssetImportTask + FbxImportUI still works as the FBX import route in 5.8: even though
  Interchange.FeatureFlags.Import.FBX defaults to true (InterchangeFbxTranslator.cpp), AssetTools's
  ImportAssetTasks (Engine/Source/Developer/AssetTools/Private/AssetTools.cpp) auto-detects Interchange
  is enabled and calls InterchangeManager.ConvertImportData() to translate the legacy FbxImportUI options
  into an Interchange pipeline automatically -- no separate Interchange-specific Python API is needed.
- UIKRigDefinitionFactory.create_new_ik_rig_asset(path, name) is a BlueprintCallable static convenience
  (IKRigDefinitionFactory.h), used for BOTH IK_Mixamo and IK_Manny. UIKRetargetFactory has no such static,
  so only RTG_MixamoToManny (a different asset type) goes through AssetTools.create_asset with an explicit
  factory instance.
- Mixamo's "mixamorig:" FBX namespace is stripped at IMPORT time, not by the IK Rig auto-characterizer:
  UInterchangeFbxTranslator's bKeepFbxNamespace defaults to false (InterchangeFbxTranslator.cpp ~line 309),
  so the FBX SDK-level parse strips the namespace before the skeleton is ever created in Unreal. Confirmed
  empirically: IKRigController.get_retarget_root() on the built IK_Mixamo returns "Hips", not
  "mixamorig:Hips". (FAutoCharacterizer::GenerateCleanBoneNames in IKRigAutoCharacterizer.cpp does ALSO
  strip a skeleton's largest common bone-name prefix before template-matching, as a fallback for skeletons
  that keep such a prefix -- e.g. an import with bKeepFbxNamespace=true -- but that fallback isn't what's
  operative on this particular import.) Manny matches the built-in "UE5 Mannequin" template directly.
- AnimMontageFactory always names its one starting section "Default" (EnsureStartingSection,
  AnimMontageFactory.cpp) and its one slot "DefaultSlot" (UAnimMontage::UAnimMontage ->
  AddSlot(FAnimSlotGroup::DefaultSlotName)), matching ABP_Manny_Combat's DefaultSlot node and
  GA_ChakraCharge.cpp's CurrentMontageSetNextSectionName("Default", "Default") call -- same as every
  other montage Scripts/setup_m1.py builds. verify_result() confirms this by reading the real
  section/slot names back rather than assuming them, per code review.
- ERetargetSourceOrTarget, EAutoMapChainType, ERootMotionSource, ERootMotionHeightSource and
  ERetargetAutoAlignMethod are plain UENUM()s (not all BlueprintType), which Python still binds. Every
  enum value used below was confirmed by listing it directly, e.g. list(unreal.RootMotionSource) ->
  [RootMotionSource.COPY_FROM_SOURCE_ROOT, RootMotionSource.GENERATE_FROM_TARGET_PELVIS].
- UIKRetargeterController.AddDefaultOps() (IKRetargeterController.cpp) adds exactly 5 ops (to an empty
  stack -- it does NOT skip non-singleton ops already present, hence remove_all_ops() first), in this order:
  Pelvis Motion, FK Chains, Run IK Rig, Root Motion, Remap Curves -- confirmed both by reading the source
  and by listing the built RTG_MixamoToManny's actual op stack. There's no "IK Chains" or "IK Solve" op in
  5.8; the UIKRetargeterController class header's own doc comment listing those is stale.
"""
import unreal

MIXAMO_DIR = "/Game/Shinobi/Mixamo"
RETARGET_DIR = "/Game/Shinobi/Retarget"
ANIM_DIR = "/Game/Shinobi/Anims"
MANNY_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
FBX_SOURCE = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir() + "SourceArt/Mixamo/Power_Up_30fps.fbx"
)
# Names Unreal's FBX import actually produced for this file (confirmed by running the import once and
# reading back AssetImportTask.get_objects(); Unreal names the mesh after the FBX file, and appends
# "_Skeleton" / "_Anim" for the skeleton and the (single, unnamed) animation take).
MIXAMO_MESH_PATH = f"{MIXAMO_DIR}/Power_Up_30fps.Power_Up_30fps"
MIXAMO_SKELETON_PATH = f"{MIXAMO_DIR}/Power_Up_30fps_Skeleton.Power_Up_30fps_Skeleton"
MIXAMO_ANIM_PATH = f"{MIXAMO_DIR}/Power_Up_30fps_Anim.Power_Up_30fps_Anim"
MIXAMO_ANIM_NAME = "Power_Up_30fps_Anim"
AS_POWER_UP = f"{ANIM_DIR}/AS_PowerUp"
AM_CHAKRA_CHARGE = f"{ANIM_DIR}/AM_ChakraCharge"

# Bones sanity-checked in verify_result(): (Manny/target bone, corresponding Mixamo/source bone).
CHECK_BONE_PAIRS = (
    ("upperarm_l", "LeftArm"),
    ("upperarm_r", "RightArm"),
    ("hand_l", "LeftHand"),
    ("hand_r", "RightHand"),
)

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def find_imported_mixamo_assets():
    """Returns (mesh, skeleton, anim) if all three of the expected, exact import output paths exist,
    else (None, None, None). Uses fixed paths (confirmed against a real import) rather than scanning
    MIXAMO_DIR for "the last asset of each type", which would silently pick up the wrong asset if an
    unrelated one were ever added to that folder."""
    if not (
        unreal.EditorAssetLibrary.does_asset_exist(MIXAMO_MESH_PATH)
        and unreal.EditorAssetLibrary.does_asset_exist(MIXAMO_SKELETON_PATH)
        and unreal.EditorAssetLibrary.does_asset_exist(MIXAMO_ANIM_PATH)
    ):
        return None, None, None
    return (
        unreal.load_asset(MIXAMO_MESH_PATH),
        unreal.load_asset(MIXAMO_SKELETON_PATH),
        unreal.load_asset(MIXAMO_ANIM_PATH),
    )


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
    if mesh.get_path_name() != MIXAMO_MESH_PATH:
        raise RuntimeError(
            f"import named the mesh {mesh.get_path_name()}, expected {MIXAMO_MESH_PATH} -- "
            f"update MIXAMO_MESH_PATH/MIXAMO_SKELETON_PATH/MIXAMO_ANIM_PATH to match"
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


def find_op_controller_index(controller, op_controller_class):
    """Returns the index of the first retarget op whose Python-exposed controller is an instance of
    op_controller_class (e.g. unreal.IKRetargetRunIKRigController), or -1 if none is found. C++ looks
    these up by type with the templated GetFirstRetargetOpOfType<T>() (used internally by Epic's own
    SRetargetAnimAssetsWindow.cpp), which isn't exposed to Python; this does the equivalent with
    isinstance() against get_op_controller()'s result, which carries the op's real (derived) class."""
    for index in range(controller.get_num_retarget_ops()):
        if isinstance(controller.get_op_controller(index), op_controller_class):
            return index
    return -1


def configure_retargeter(controller, ik_mixamo, ik_manny):
    """(Re)applies the retargeter's op-stack configuration. Called on every run, including against an
    already-configured asset from an earlier version of this script (e.g. one built before this fix used
    fuzzy chain mapping and left the Run IK Rig op enabled) -- every setting below is force-reapplied,
    not merely set-if-unset, so a tuning fix here takes effect on re-run rather than only on a fresh asset.

    Matches Epic's own auto-retarget setup for exactly this shape of problem
    (SRetargetAnimAssetsWindow.cpp, FProceduralRetargetAssets::AutoGenerateIKRetargetAsset, ~line
    669-751), which is what the in-editor "Auto Generate" button in the Retarget Manager runs:
    - EXACT (not fuzzy) chain auto-mapping: fuzzy string-matches Manny's per-finger metacarpal chains
      (IndexMetacarpal, MiddleMetacarpal, ...), which have no Mixamo counterpart, onto the closest-sounding
      Mixamo finger chains (Index, Middle, ...) instead of leaving them unmapped -- confirmed empirically:
      IK_Manny has LeftIndexMetacarpal/LeftMiddleMetacarpal/etc. chains that IK_Mixamo does not, so a fuzzy
      match cross-wires them onto the finger chains, crumpling a clenched fist. Auto-characterization gives
      matching bone chains identical canonical names on both skeletons (confirmed: IK_Mixamo and IK_Manny
      both have a "LeftArm" chain, and RTG_MixamoToManny.get_source_chain("LeftArm") returns "LeftArm"
      after an exact map), so exact matching is sufficient and correct -- Epic's own comment on this exact
      line: "use only exact chain mapping in the auto-retarget (default is fuzzy which can map toes to
      fingers and other chaos)".
    - Run IK Rig op disabled: Epic disables this by default too ("auto-IK is not yet robust enough, still
      useful for manual editing though") -- leaving it on was the likely cause of the arm/hand
      clipping/crumpling seen in the first pass of this script.
    - Root Motion op set to generate from the target pelvis rather than copy the source root: confirmed
      empirically (via the op's own get_source_root_bone/get_target_root_bone/get_target_pelvis_bone,
      which are auto-populated from each IK Rig's characterization the moment the ops are added) that
      Mixamo's root motion bone falls back to "Hips" (its IKRigAutoCharacterizer "Mixamo" template never
      calls .RootBone = ..., so it's unset and FIKRetargetRootMotionOp::OnAddedToStack falls back to the
      raw skeleton hierarchy root, which for Mixamo IS Hips) while Manny's "UE5 Mannequin" template sets a
      dedicated RootBone ("root") distinct from its PelvisBone ("pelvis"). So the default
      CopyFromSourceRoot would copy "root motion" from a source bone that's actually just the pelvis --
      meaningless. Epic's own code detects exactly this shape (TargetRoot != TargetPelvis but
      SourceRoot == SourcePelvis) and switches to generating root motion from the retargeted Pelvis
      instead; the same check is applied here via the already-Python-exposed getters (not hardcoded), so
      it stays correct if the source/target meshes ever change.
    Not replicated here: Epic's reference flow also adds a Pin Bone op for IK-goal bones, driven by
    FAutoCharacterizeResults.AutoRetargetDefinition.BonesToPin -- that results struct is only returned by
    the C++-only AutoGenerateRetargetDefinition(FAutoCharacterizeResults&), not by the simple
    apply_auto_generated_retarget_definition() bool this script uses, so the bones-to-pin list isn't
    available from Python without re-deriving it by hand. Left out per the plan's "add only if
    straightforward" allowance; the Run IK Rig op being disabled means IK goals aren't solved at all here,
    so there's nothing for a Pin Bone op to override in this specific setup anyway.
    """
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_mixamo)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_manny)
    # Rebuild the op stack from scratch every run, like Epic does (SRetargetAnimAssetsWindow.cpp): AddRetargetOp
    # only refuses duplicates of singleton ops (just Remap Curves), so add_default_ops() alone on an existing
    # asset appends a second Pelvis Motion/FK Chains/Run IK Rig/Root Motion with default settings each run.
    controller.remove_all_ops()
    controller.add_default_ops()  # Pelvis Motion, FK Chains, Run IK Rig, Root Motion, Remap Curves
    num_ops = controller.get_num_retarget_ops()
    if num_ops != 5:
        raise RuntimeError(f"[configure_retargeter] expected exactly 5 default ops, got {num_ops}")
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, ik_mixamo)
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_manny)

    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)

    # Mixamo exports in a T-pose; Manny's reference pose is an A-pose. Reset first so a stale offset from
    # a previous (e.g. pre-fix, fuzzy-mapped) run doesn't compound with the new alignment pass.
    target_pose_name = controller.get_current_retarget_pose_name(unreal.RetargetSourceOrTarget.TARGET)
    controller.reset_retarget_pose(target_pose_name, [], unreal.RetargetSourceOrTarget.TARGET)
    controller.auto_align_all_bones(
        unreal.RetargetSourceOrTarget.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN
    )
    upperarm_l_offset = controller.get_rotation_offset_for_retarget_pose_bone(
        "upperarm_l", unreal.RetargetSourceOrTarget.TARGET
    )
    if abs(upperarm_l_offset.w) > 0.999:
        raise RuntimeError(
            f"[configure_retargeter] auto-align left upperarm_l at an identity rotation offset "
            f"({upperarm_l_offset}) -- T-pose/A-pose alignment likely did not run"
        )

    run_ik_index = find_op_controller_index(controller, unreal.IKRetargetRunIKRigController)
    if run_ik_index < 0:
        raise RuntimeError("RTG_MixamoToManny has no Run IK Rig op to disable")
    controller.set_retarget_op_enabled(run_ik_index, False)
    if controller.get_retarget_op_enabled(run_ik_index):
        raise RuntimeError("RTG_MixamoToManny's Run IK Rig op did not disable")

    root_motion_index = find_op_controller_index(controller, unreal.IKRetargetRootMotionController)
    if root_motion_index < 0:
        raise RuntimeError("RTG_MixamoToManny has no Root Motion op to configure")
    root_motion_op = controller.get_op_controller(root_motion_index)
    ik_mixamo_controller = unreal.IKRigController.get_controller(ik_mixamo)
    source_pelvis = ik_mixamo_controller.get_retarget_root()
    target_root = root_motion_op.get_target_root_bone()
    target_pelvis = root_motion_op.get_target_pelvis_bone()
    source_root = root_motion_op.get_source_root_bone()
    if target_root != target_pelvis and source_root == source_pelvis:
        settings = root_motion_op.get_settings()
        settings.root_motion_source = unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS
        settings.root_height_source = unreal.RootMotionHeightSource.SNAP_TO_GROUND
        settings.rotate_with_pelvis = True
        settings.maintain_offset_from_pelvis = True
        root_motion_op.set_settings(settings)
        readback = root_motion_op.get_settings()
        if readback.root_motion_source != unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS:
            raise RuntimeError("[configure_retargeter] Root Motion op settings did not take")
        unreal.log(
            f"[setup_chakra_pose] root motion: generating from Manny's pelvis (source root "
            f"'{source_root}' has no motion of its own distinct from its pelvis '{source_pelvis}')"
        )
    else:
        unreal.log(
            f"[setup_chakra_pose] root motion: left at default (CopyFromSourceRoot) -- target "
            f"root/pelvis={target_root}/{target_pelvis}, source root/pelvis={source_root}/{source_pelvis}"
        )

    return run_ik_index, root_motion_index


def build_retargeter(ik_mixamo, ik_manny):
    path = f"{RETARGET_DIR}/RTG_MixamoToManny"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        retargeter = unreal.load_asset(path)
        unreal.log(f"[setup_chakra_pose] exists: {path}")
    else:
        retargeter = TOOLS.create_asset(
            "RTG_MixamoToManny", RETARGET_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory()
        )
        if retargeter is None:
            raise RuntimeError(f"failed to create {path}")
        unreal.log(f"[setup_chakra_pose] created: {path}")

    controller = unreal.IKRetargeterController.get_controller(retargeter)
    configure_retargeter(controller, ik_mixamo, ik_manny)

    if not unreal.EditorAssetLibrary.save_loaded_asset(retargeter):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_chakra_pose] configured: {path}")
    return retargeter


def retarget_animation(mixamo_mesh, manny_mesh, retargeter, source_anim):
    """Runs on every call (not skip-if-exists): configure_retargeter()'s settings can change between runs
    and re-running must pick that up rather than leaving a stale AS_PowerUp from an earlier run in place.
    search/replace name the result AS_PowerUp directly (the source AnimSequence is named
    Power_Up_30fps_Anim; FString::Replace on the whole name, per EditorAnimUtils.cpp's
    FNameDuplicationRule::Rename, turns it into exactly "AS_PowerUp" with Prefix/Suffix left blank).
    overwrite_existing_files makes IKRetargetBatchOperation::OverwriteExistingAssets
    (IKRetargetBatchOperation.cpp) call ObjectTools::ForceReplaceReferences to reroute every existing
    reference to the old AS_PowerUp (e.g. AM_ChakraCharge's slot track) onto the freshly retargeted one
    before deleting the old asset and renaming the new one into place -- confirmed by reading that
    function; this is what makes re-running safe without breaking AM_ChakraCharge's reference."""
    source_data = unreal.EditorAssetLibrary.find_asset_data(source_anim.get_path_name())
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [source_data]
    inputs.source_mesh = mixamo_mesh
    inputs.target_mesh = manny_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = MIXAMO_ANIM_NAME
    inputs.replace = "AS_PowerUp"
    inputs.target_path = ANIM_DIR
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    inputs.retain_additive_flags = True

    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError(f"batch retarget produced no assets for {source_anim.get_path_name()}")

    result_path = str(results[0].package_name)
    if result_path != AS_POWER_UP:
        raise RuntimeError(
            f"batch retarget named its result {result_path}, expected {AS_POWER_UP} (search/replace on "
            f"{MIXAMO_ANIM_NAME!r} not an exact match against the source asset's actual name?)"
        )

    result = unreal.load_asset(AS_POWER_UP)
    if result is None:
        raise RuntimeError(f"retarget result not found at {AS_POWER_UP}")

    # The retarget op stack doesn't touch the resulting AnimSequence's own EnableRootMotion flag (that
    # flag controls in-game playback; the Root Motion retarget op above only affects how the retarget
    # itself is computed) -- GA_ChakraCharge plays this as a stationary charge pose, so make sure no root
    # motion is baked into how it gets played back.
    result.set_editor_property("enable_root_motion", False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(result):
        raise RuntimeError(f"failed to save {AS_POWER_UP}")
    unreal.log(f"[setup_chakra_pose] retargeted: {AS_POWER_UP}")
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
        # source_animation is only a property of AnimMontageFactory (used at creation time), not of
        # UAnimMontage itself -- there's no equivalent "re-point" setter on an existing montage. This is
        # fine: retarget_animation()'s overwrite_existing_files path replaces AS_PowerUp in place and
        # reroutes every existing reference (including this montage's slot track) onto the new asset via
        # ObjectTools::ForceReplaceReferences, confirmed in that function's docstring, so the montage
        # already points at the current AS_PowerUp by the time this runs. verify_result() checks this.
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


def bone_rotation(anim_sequence, bone_name, time):
    return unreal.AnimationLibrary.get_bone_pose_for_time(anim_sequence, bone_name, time, False).rotation


def quat_deviation(q0, q1):
    """1 - |dot(q0, q1)|: 0 for identical rotations, growing towards 1 as they diverge. Used instead of
    q.rotator() since FQuat's X/Y/Z/W are the confirmed-reflected properties (NoExportTypes.h) and
    Rotator() is a plain C++ method, not a UFUNCTION, so it isn't exposed to Python."""
    dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w
    return 1.0 - min(abs(dot), 1.0)


def max_rotation_deviation(anim_sequence, bone_name, num_samples=30):
    """Largest deviation (see quat_deviation) between bone_name's rotation at t=0 and at each of
    num_samples-1 further evenly-spaced times across the whole clip. Sampling across the whole clip
    (instead of a single midpoint sample) means a bone that's briefly still but moves elsewhere in the
    clip isn't misread as static."""
    length = anim_sequence.get_play_length()
    if length <= 0.0 or num_samples < 2:
        return 0.0
    first = bone_rotation(anim_sequence, bone_name, 0.0)
    return max(
        quat_deviation(first, bone_rotation(anim_sequence, bone_name, length * i / (num_samples - 1)))
        for i in range(1, num_samples)
    )


def verify_result(as_power_up, mixamo_anim, am_chakra_charge, manny_mesh):
    """Read-only sanity check, run every time (not just on first creation): confirms AS_PowerUp is on
    Manny's skeleton, has a real length/frame count, AM_ChakraCharge references it under its real
    (queried, not assumed) section/slot names, and that several key bones actually move across the whole
    clip -- each compared against the corresponding source (Mixamo) bone, so a static target bone can be
    told apart from a source clip that simply doesn't move that bone much. Raises only if EVERY sampled
    bone is static (a real sign the retarget produced a frozen pose); an individual static bone is logged,
    not raised on, since it may just reflect the source clip. Also logs a rough loop-seam indicator (how
    far arm/hand/spine/head rotations differ between the clip's first and last frame) for Task 5.
    What this CANNOT confirm headlessly: whether the retarget pose actually *looks* right (no twisted
    limbs, correct T-pose/A-pose alignment, no foot sliding, no visible pop on the loop seam) -- that
    needs the user's eyes, Task 5."""
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

    if am_chakra_charge.get_num_sections() < 1:
        raise RuntimeError("[verify] AM_ChakraCharge has no sections")
    section_name = str(am_chakra_charge.get_section_name(0))
    if section_name != "Default":
        raise RuntimeError(f"[verify] AM_ChakraCharge's starting section is {section_name!r}, expected 'Default'")
    slot_names = [str(n) for n in unreal.AnimationLibrary.get_montage_slot_names(am_chakra_charge)]
    if "DefaultSlot" not in slot_names:
        raise RuntimeError(f"[verify] AM_ChakraCharge has no DefaultSlot: {slot_names}")

    slot_tracks = am_chakra_charge.get_editor_property("slot_anim_tracks")
    referenced = None
    if slot_tracks:
        segments = slot_tracks[0].get_editor_property("anim_track").get_editor_property("anim_segments")
        if segments:
            referenced = segments[0].get_editor_property("anim_reference")
    if referenced != as_power_up:
        raise RuntimeError(f"[verify] AM_ChakraCharge does not reference AS_PowerUp (got {referenced})")

    any_moves = False
    deviation_log = []
    for target_bone, source_bone in CHECK_BONE_PAIRS:
        target_dev = max_rotation_deviation(as_power_up, target_bone)
        source_dev = max_rotation_deviation(mixamo_anim, source_bone)
        any_moves = any_moves or target_dev > 0.0005
        deviation_log.append(f"{target_bone}={target_dev:.4f}(src {source_bone}={source_dev:.4f})")
    if not any_moves:
        raise RuntimeError(f"[verify] every sampled bone is static on AS_PowerUp: {deviation_log}")

    # Loop seam on limb/spine rotations, not the pelvis: the Root Motion op makes root follow the pelvis, so
    # the pelvis's parent-relative transform barely changes whatever the body does (review finding).
    seam_bones = [target for target, _ in CHECK_BONE_PAIRS] + ["spine_03", "head"]
    seam_log = ", ".join(
        f"{bone}={quat_deviation(bone_rotation(as_power_up, bone, 0.0), bone_rotation(as_power_up, bone, play_length)):.4f}"
        for bone in seam_bones
    )

    unreal.log(
        f"[setup_chakra_pose] verify: skeleton=Manny length={play_length:.2f}s frames={num_frames} "
        f"section={section_name} slots={slot_names} bones=[{', '.join(deviation_log)}]"
    )
    unreal.log(
        f"[setup_chakra_pose] verify: loop seam (rotation deviation, first vs last frame) [{seam_log}] -- "
        f"rough indicator only; Task 5 decides whether the loop visibly pops"
    )
    unreal.log(
        "[setup_chakra_pose] verify: pose correctness (T-pose/A-pose alignment, no twisted limbs, foot "
        "placement, whether the loop seam pops) needs the user's eyes in Task 5 -- cannot be checked "
        "headlessly."
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
verify_result(as_power_up, mixamo_anim, am_chakra_charge, manny_mesh)
