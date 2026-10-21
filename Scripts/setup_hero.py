"""Hero character assets (sub-project #2). Idempotent (re-imports and re-configures in place); close the GUI editor.

Source: SourceArt/Character/Models/Hero/Hero_rigged.fbx (the cloakless Mixamo-rigged hero, made like the other characters
so outfits can be worn over it), else SourceArt/Character/Mesh/hero_rigged.fbx (the first hero, Akatsuki cloak baked in), else the stand-in
SourceArt/Mixamo/Power_Up_30fps.fbx (a Mixamo mannequin), so the runtime side can be built before the real hero.

Builds:
  SK_Hero (+ SK_Hero_Skeleton, SK_Hero_PhysicsAsset for the death ragdoll)
  T_Hero_Diffuse + M_Hero  the base-colour texture on a plain lit Substrate material (M_ToonPost adds bands and outlines).
                   Mixamo packs textures without a file extension, which UE's FBX importer can't read, so the texture
                   comes from hero_rigged.png next to the FBX (Scripts/character/extract_textures.py writes it when missing).
                   The stand-in keeps its own FBX materials.
  IK_Hero          auto-characterised IK rig (Mixamo template: chains Spine, Neck, Head, LeftArm, ... as on IK_Manny)
  RTG_MannyToHero  source IK_Manny (from Scripts/setup_chakra_pose.py) -> target IK_Hero, used at runtime by
                   UShinobiRetargetAnimInstance to copy the hidden Manny's pose onto the hero every frame.
Then every other character (players' roster picks, AShinobiPlayerCharacter::Roster; the NPC cast, enemy types' BodyPool
and AShinobiVillager::Bodies; all worn through UShinobiBodyComponent), from two sources:
  - SourceArt/Character/Models/<Id>/<Id>_rigged.fbx: a Mixamo-rigged model, like the hero (docs/character/model-steps.md);
    its texture comes from <Id>_rigged.png (extracted like the hero's when missing).
  - SourceArt/Character/VRoid/<Id>.vrm made in VRoid Studio (docs/character/vroid-steps.md):
    Scripts/character/vroid_to_fbx.py turns each into <Id>/<Id>.fbx + one PNG per material (re-run when the .vrm is newer).
Each becomes /Game/Shinobi/Characters/<Id>/: SK_<Id> (+ skeleton, physics asset), T_/MI_<Id>_<Texture> on the shared
M_Character (masked by the texture alpha: eyelashes, brows, hair edges; two-sided: hair and cloth cards), IK_<Id> and
RTG_MannyTo<Id>, built exactly like the hero's.
Last, the outfits (SourceArt/Character/Outfits/<Id>/<Id>_outfit.fbx + .png from Scripts/character/skin_outfit.py): each becomes
/Game/Shinobi/Outfits/<Id>/SK_<Id> (+ skeleton, MI_<Id>_<Id>_outfit on M_Character), worn over any character through
leader-pose (AShinobiPlayerCharacter::Outfits, docs/character/outfit-steps.md). No retargeter: its bones are the body's by name.
Retargeter setup follows setup_chakra_pose.py's reasoning (exact chain mapping, auto-align the target chain to chain,
Run IK Rig op off). Root Motion op off too: the hero's root IS its pelvis (Hips), so "copy source root" would pin the
hips to Manny's floor-level root bone; the character moves by its capsule, not root motion.
"""
import glob
import math
import os
import re
import subprocess
import unreal

HERO_DIR = "/Game/Shinobi/Hero"
IK_MANNY_PATH = "/Game/Shinobi/Retarget/IK_Manny"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MODELS_DIR = os.path.join(PROJECT, "SourceArt", "Character", "Models")
OUTFITS_DIR = os.path.join(PROJECT, "SourceArt", "Character", "Outfits")
FIRST_HERO_FBX = os.path.join(PROJECT, "SourceArt", "Character", "Mesh", "hero_rigged.fbx")
CLOAKLESS_HERO_FBX = os.path.join(MODELS_DIR, "Hero", "Hero_rigged.fbx")
HERO_FBX = CLOAKLESS_HERO_FBX if os.path.exists(CLOAKLESS_HERO_FBX) else FIRST_HERO_FBX
HERO_TEXTURE = os.path.splitext(HERO_FBX)[0] + ".png"
STAND_IN_FBX = os.path.join(PROJECT, "SourceArt", "Mixamo", "Power_Up_30fps.fbx")
VROID_DIR = os.path.join(PROJECT, "SourceArt", "Character", "VRoid")
CHARACTERS_DIR = "/Game/Shinobi/Characters"
OUTFITS_GAME_DIR = "/Game/Shinobi/Outfits"
BLENDER = "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"
MEL = unreal.MaterialEditingLibrary
# Every body chain a gameplay animation moves must reach the hero (fingers are optional: Mixamo's "Standard" skeleton
# has them, a "No fingers" download doesn't).
REQUIRED_CHAINS = ("Spine", "Neck", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
# How far each upper arm is swung out from the body on every character (spread_arms); tune by eye, then SETUP_HERO_ONLY=retargeters.
ARM_SPREAD_DEGREES = float(os.environ.get("SETUP_HERO_ARM_SPREAD", "6"))


def log(message):
    unreal.log(f"[setup_hero] {message}")


CLEAN_RIGS_DIR = os.path.join(PROJECT, "Saved", "CleanRigs")


def clean_rig(fbx):
    """The rig re-exported with unit-scale bones (Scripts/character/clean_rig.py; Saved/CleanRigs/, redone when the
    source changes): a Mixamo FBX imports with its Hips scaled 100, which throws Chaos cloth and physics assets off."""
    out = os.path.join(CLEAN_RIGS_DIR, os.path.basename(os.path.dirname(fbx)), os.path.basename(fbx))
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(fbx):
        script = os.path.join(PROJECT, "Scripts", "character", "clean_rig.py")
        run = subprocess.run([BLENDER, "-b", "--factory-startup", "--python-exit-code", "1", "-P", script, "--", fbx, out],
                             capture_output=True, text=True)
        if run.returncode != 0 or not os.path.exists(out):
            raise RuntimeError(f"clean_rig.py failed on {fbx}: {run.stdout[-2000:]}")
    return out


def import_hero(fbx, dest=HERO_DIR, name="SK_Hero", materials=True, physics=True):
    fbx = clean_rig(fbx)
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.create_physics_asset = physics
    options.import_animations = False
    options.import_materials = materials
    options.import_textures = materials
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, unreal.SkeletalMesh)), None)
    if mesh is None:
        raise RuntimeError(f"import of {fbx} produced no skeletal mesh: {[o.get_path_name() for o in task.get_objects()]}")
    if physics and mesh.get_editor_property("physics_asset") is None:
        # The automated FBX import skips create_physics_asset; build one from the mesh's bones instead.
        unreal.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, True)
        if mesh.get_editor_property("physics_asset") is None:
            raise RuntimeError(f"{name} has no physics asset (needed for the death ragdoll)")
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh.get_editor_property("physics_asset"))
    log(f"imported {fbx} -> {mesh.get_path_name()}")
    return mesh


def load_or_create(name, create, dest=HERO_DIR):
    path = f"{dest}/{name}"
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else create()
    if asset is None:
        raise RuntimeError(f"could not load or create {path}")
    return asset


def import_texture():
    if not os.path.exists(HERO_TEXTURE):
        script = os.path.join(PROJECT, "Scripts", "character", "extract_textures.py")
        subprocess.run([BLENDER, "-b", "-P", script, "--", HERO_FBX, HERO_TEXTURE], check=True, capture_output=True)
    task = unreal.AssetImportTask()
    task.filename = HERO_TEXTURE
    task.destination_path = HERO_DIR
    task.destination_name = "T_Hero_Diffuse"
    task.automated = True
    task.save = True
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"could not import {HERO_TEXTURE}")
    return texture


def build_material(texture):
    path = f"{HERO_DIR}/M_Hero"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = TOOLS.create_asset("M_Hero", HERO_DIR, unreal.Material, unreal.MaterialFactoryNew())
    sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    sample.set_editor_property("parameter_name", "BaseColor")
    sample.set_editor_property("texture", texture)
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 250)
    rough.set_editor_property("r", 0.8)
    spec = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 350)
    spec.set_editor_property("r", 0.3)
    shading = MEL.create_material_expression(material, unreal.MaterialExpressionSubstrateShadingModels, -200, 0)
    for src, pin in ((sample, "BaseColor"), (rough, "Roughness"), (spec, "Specular")):
        if not MEL.connect_material_expressions(src, "RGB" if src is sample else "", shading, pin):
            raise RuntimeError(f"M_Hero: could not connect {pin}")
    MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"M_Hero failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def use_material(mesh, material):
    # The returned array holds struct copies: build new slots rather than editing them in place.
    slots = [unreal.SkeletalMaterial(material_interface=material, material_slot_name=slot.get_editor_property("material_slot_name"))
             for slot in mesh.get_editor_property("materials")]
    mesh.set_editor_property("materials", slots)
    if any(slot.get_editor_property("material_interface") != material for slot in mesh.get_editor_property("materials")):
        raise RuntimeError("SK_Hero: M_Hero did not stick on its material slots")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log(f"M_Hero on {len(slots)} material slot(s)")


def remove_strays():
    """Deletes leftovers from earlier imports (the stand-in's materials, the FBX's own unreadable material)."""
    keep = {"SK_Hero", "SK_Hero_Skeleton", "SK_Hero_PhysicsAsset", "IK_Hero", "RTG_MannyToHero", "T_Hero_Diffuse", "M_Hero"}
    for path in unreal.EditorAssetLibrary.list_assets(HERO_DIR, recursive=True):
        name = path.rsplit("/", 1)[-1].split(".")[0]
        if name not in keep and fbx == HERO_FBX:
            if unreal.EditorAssetLibrary.delete_asset(path):
                log(f"removed stray {name}")


def build_ik_rig(mesh, dest=HERO_DIR, name="IK_Hero"):
    rig = load_or_create(name, lambda: unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(dest, name), dest)
    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh):
        raise RuntimeError(f"{name}: set_skeletal_mesh failed")
    if not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"{name}: auto-characterisation did not recognise the skeleton (is it a Mixamo rig?)")
    unreal.EditorAssetLibrary.save_loaded_asset(rig)
    return rig


def find_op(controller, op_class):
    for index in range(controller.get_num_retarget_ops()):
        if isinstance(controller.get_op_controller(index), op_class):
            return index
    raise RuntimeError(f"retargeter has no {op_class.__name__} op")


def retarget_pose(controller, ik_rig, side):
    """{bone: world transform} of the rig's mesh in the retarget pose: the mesh's own reference pose (as the retargeter
    uses it; its USkeleton's can be stale after a reimport, as SK_Hero's was by up to 33 degrees) with the retargeter's
    offsets applied. A pose delta is local (bone ref rotation * delta, IKRetargetProcessor.cpp). Rotations are
    normalised: the meshes carry a 100x scale and products drift off unit length, which reads as degrees of error."""
    bones = unreal.SkeletonModifier()
    bones.set_skeletal_mesh(unreal.IKRigController.get_controller(ik_rig).get_skeletal_mesh())
    world = {}
    for bone in bones.get_all_bone_names():  # parents first
        local = bones.get_bone_transform(bone, False)
        local.rotation = (local.rotation * controller.get_rotation_offset_for_retarget_pose_bone(bone, side)).normalized()
        parent = str(bones.get_parent_name(bone))
        world[str(bone)] = unreal.MathLibrary.compose_transforms(local, world[parent]) if parent in world else local
    return world


def turn_bone(controller, pose, bone, turn, side):
    """Sets the bone's retarget-pose offset so the bone turns by turn (a world rotation) from where pose has it."""
    bone_world = pose[bone].rotation.normalized()
    delta = controller.get_rotation_offset_for_retarget_pose_bone(bone, side)
    controller.set_rotation_offset_for_retarget_pose_bone(bone, (delta * bone_world.inversed() * turn * bone_world).normalized(), side)


def spread_arms(controller, ik_target):
    """Raises each upper arm ARM_SPREAD_DEGREES out from the body in the target retarget pose. Manny's arms swing close to
    his slim torso; copied 1:1 onto our bulkier bodies (and the cloaks over them) the hands sink into the hips. The FK chain
    op carries a retarget-pose offset into every animation, so this widens idle, run and jump alike."""
    target = unreal.RetargetSourceOrTarget.TARGET
    rig = unreal.IKRigController.get_controller(ik_target)
    for chain in ("LeftArm", "RightArm"):
        upper, hand = str(rig.get_retarget_chain_start_bone(chain)), str(rig.get_retarget_chain_end_bone(chain))
        pose = retarget_pose(controller, ik_target, target)  # the auto-aligned pose, so the arm's real direction is measured
        arm = pose[hand].translation - pose[upper].translation
        # Rotating the arm about (arm x up) turns it toward up: away from the body for a hanging arm on either side.
        turn_bone(controller, pose, upper, unreal.MathLibrary.rotator_from_axis_and_angle(arm.normal().cross(unreal.Vector(0, 0, 1)).normal(), ARM_SPREAD_DEGREES).quaternion(), target)
        # Check: re-pose the arm with the new delta; it must hang ARM_SPREAD_DEGREES further from straight down.
        pose = retarget_pose(controller, ik_target, target)
        spread = pose[hand].translation - pose[upper].translation
        down = lambda v: math.degrees(math.acos(-v.normal().z))
        if abs(down(spread) - down(arm) - ARM_SPREAD_DEGREES) > 1:
            raise RuntimeError(f"spread_arms: {chain} went {down(arm):.1f} -> {down(spread):.1f} deg from down, wanted +{ARM_SPREAD_DEGREES}")
        log(f"{ik_target.get_name()} {chain}: {down(arm):.1f} -> {down(spread):.1f} deg from straight down")


# A Mixamo mitten hand's thumb frame (see align_hand_roll) in its own bone space, per side: measured on the mitten rigs that
# have a thumb, within 6 degrees of this on each.
MIXAMO_MITTEN = {"l": unreal.Rotator(roll=180, pitch=0, yaw=-90).quaternion(), "r": unreal.Rotator(roll=0, pitch=0, yaw=-90).quaternion()}


def hand_frame(pose, hand, along, across_from, across_to):
    """Rotation whose X runs from the wrist to along and whose Y runs (square to X) from across_from to across_to."""
    at = lambda bone: pose[bone].translation
    return unreal.MathLibrary.make_rot_from_xy(at(along) - at(hand), at(across_to) - at(across_from)).quaternion()


def align_hand_roll(controller, ik_manny, ik_target):
    """Turns each hand in the target retarget pose so it lies as Manny's does. The chain-to-chain auto-align matches bone
    directions but not roll, which left every hero's hands turned up to ~100 degrees about the forearm (palms forward
    where Manny's face each other: pressed hand seals opened into a V). How a hand's lie is read depends on its fingers:
    a full hand by its knuckle row (little to index); a Mixamo mitten (one "index" for all four fingers) by the side its
    thumb is on, or, without a thumb, by its bone's roll, which Mixamo sets the same way on every mitten. The fingers are
    re-aligned after, since turning the hand turns them too."""
    source, target = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    rig = unreal.IKRigController.get_controller(ik_target)
    manny = retarget_pose(controller, ik_manny, source)
    names = list(retarget_pose(controller, ik_target, target))
    fingers = []
    for chain, s in (("LeftArm", "l"), ("RightArm", "r")):
        hand = str(rig.get_retarget_chain_end_bone(chain))
        index, middle, pinky, thumb = (f"{hand}{finger}1" for finger in ("Index", "Middle", "Pinky", "Thumb"))  # Mixamo: LeftHandIndex1, ...
        pose = retarget_pose(controller, ik_target, target)
        if middle in names and pinky in names:
            should = hand_frame(manny, f"hand_{s}", f"middle_01_{s}", f"pinky_01_{s}", f"index_01_{s}")
            frame = lambda p: hand_frame(p, hand, middle, pinky, index)
        else:
            should = hand_frame(manny, f"hand_{s}", f"middle_01_{s}", f"hand_{s}", f"thumb_01_{s}")
            if thumb in names:
                frame = lambda p: hand_frame(p, hand, index, hand, thumb)
            else:
                local = MIXAMO_MITTEN[s]
                frame = lambda p: p[hand].rotation.normalized() * local
        turn = (should * frame(pose).inversed()).normalized()
        turn_bone(controller, pose, hand, turn, target)
        # Check: the re-posed hand's frame must now be Manny's.
        left = math.degrees((should * frame(retarget_pose(controller, ik_target, target)).inversed()).normalized().get_angle())
        if left > 1:
            raise RuntimeError(f"align_hand_roll: {chain} still {left:.1f} deg off Manny's hand (was {math.degrees(turn.get_angle()):.1f})")
        log(f"{ik_target.get_name()} {chain}: hand turned {min(math.degrees(turn.get_angle()), 360 - math.degrees(turn.get_angle())):.1f} deg to Manny's")
        fingers += [n for n in names if n.startswith(hand) and n != hand]
    if fingers:
        controller.auto_align_bones(fingers, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN, target)


def build_retargeter(ik_manny, ik_hero, dest=HERO_DIR, name="RTG_MannyToHero"):
    retargeter = load_or_create(name, lambda: TOOLS.create_asset(name, dest, unreal.IKRetargeter, unreal.IKRetargetFactory()), dest)
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_manny)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    controller.remove_all_ops()
    controller.add_default_ops()
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, ik_manny)
    controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    pose = controller.get_current_retarget_pose_name(unreal.RetargetSourceOrTarget.TARGET)
    controller.reset_retarget_pose(pose, [], unreal.RetargetSourceOrTarget.TARGET)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    spread_arms(controller, ik_hero)
    align_hand_roll(controller, ik_manny, ik_hero)
    for op_class in (unreal.IKRetargetRunIKRigController, unreal.IKRetargetRootMotionController):
        index = find_op(controller, op_class)
        controller.set_retarget_op_enabled(index, False)
    missing = [chain for chain in REQUIRED_CHAINS if not str(controller.get_source_chain(chain))]
    if missing:
        raise RuntimeError(f"{name}: no Manny chain maps to hero chain(s) {missing}")
    unreal.EditorAssetLibrary.save_loaded_asset(retargeter)
    log(f"built {retargeter.get_path_name()} (chains {', '.join(REQUIRED_CHAINS)} mapped)")
    return retargeter


def convert_vrms():
    """Runs vroid_to_fbx.py for every VRoid/<Id>.vrm whose <Id>/<Id>.fbx is missing or older than it."""
    script = os.path.join(PROJECT, "Scripts", "character", "vroid_to_fbx.py")
    for vrm in sorted(glob.glob(os.path.join(VROID_DIR, "*.vrm"))):
        character = os.path.splitext(os.path.basename(vrm))[0]
        out_dir = os.path.join(VROID_DIR, character)
        fbx_path = os.path.join(out_dir, f"{character}.fbx")
        if os.path.exists(fbx_path) and os.path.getmtime(fbx_path) >= os.path.getmtime(vrm):
            continue
        result = subprocess.run([BLENDER, "-b", "-P", script, "--", vrm, out_dir], capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"vroid_to_fbx.py failed on {vrm}:\n{result.stdout[-2000:]}")
        log(f"converted {vrm}")


_character_material = None


def build_character_material():
    """(Built once per run.) M_Character: the hero's lit Substrate recipe, masked by the texture's alpha and two-sided (VRoid eyelashes,
    brows, hair and cloth cards; an opaque texture shows whole); characters use instances of it."""
    global _character_material
    if _character_material is not None:
        return _character_material
    path = f"{CHARACTERS_DIR}/M_Character"
    if os.environ.get("SETUP_HERO_ONLY") in ("outfits", "cloth") and unreal.EditorAssetLibrary.does_asset_exist(path):
        _character_material = unreal.load_asset(path)  # the bodies' instances stay on it
        add_hide_mask(_character_material)
        return _character_material
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = TOOLS.create_asset("M_Character", CHARACTERS_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    sample.set_editor_property("parameter_name", "BaseColor")
    sample.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 250)
    rough.set_editor_property("r", 0.8)
    spec = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 350)
    spec.set_editor_property("r", 0.3)
    shading = MEL.create_material_expression(material, unreal.MaterialExpressionSubstrateShadingModels, -200, 0)
    for src, pin in ((sample, "BaseColor"), (rough, "Roughness"), (spec, "Specular")):
        if not MEL.connect_material_expressions(src, "RGB" if src is sample else "", shading, pin):
            raise RuntimeError(f"M_Character: could not connect {pin}")
    MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    if not MEL.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError("M_Character: could not connect the opacity mask")
    add_hide_mask(material, recompile=False)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"M_Character failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    _character_material = material
    return material


def add_hide_mask(material, recompile=True):
    """M_Character's opacity mask times a HideMask texture's red (default white): while an outfit is worn the game sets it
    to the body's hide mask for that outfit (AShinobiPlayerCharacter::ApplyOutfit), so the body under the cloak isn't
    drawn. Also marks it for clothing (the outfits' cloth). Done in place (an existing material keeps its instances).
    The sampler is Color, matching both the white default and the hide masks (import_hide_mask): a mismatch fails the
    material, and the game then draws every character on it in the grey default material."""
    current = MEL.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
    if isinstance(current, unreal.MaterialExpressionMultiply):
        # Already there: correct the sampler of a mask wired before it was Color.
        mask = next((n for n in MEL.get_inputs_for_material_expression(material, current)
                     if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property("parameter_name")) == "HideMask"), None)
        if mask is None:
            raise RuntimeError("M_Character: the opacity mask is multiplied by something other than HideMask")
        if (mask.get_editor_property("sampler_type") == unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
                and material.get_editor_property("used_with_clothing")):
            return
    else:
        output = MEL.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_OPACITY_MASK) or "A"
        mask = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 500)
        mask.set_editor_property("parameter_name", "HideMask")
        mask.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
        times = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 400)
        if not (MEL.connect_material_expressions(current, output, times, "A") and MEL.connect_material_expressions(mask, "R", times, "B")
                and MEL.connect_material_property(times, "", unreal.MaterialProperty.MP_OPACITY_MASK)):
            raise RuntimeError("M_Character: could not wire HideMask into the opacity mask")
    mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_CLOTHING, True)
    if recompile:
        errors = MEL.recompile_material(material)
        if errors:
            raise RuntimeError(f"M_Character failed to compile: {errors}")
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    log("M_Character: HideMask wired into the opacity mask")


def import_texture_file(png, dest, name):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"could not import {png}")
    return texture


def use_materials(mesh, character, dest, png_for_slot, parent):
    """One MI_<Id>_<Png> per base-colour PNG (VRoid: one per slot, <Slot>.png; Mixamo: one texture for every slot)."""
    slots, instances = [], {}
    for slot in mesh.get_editor_property("materials"):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        png = png_for_slot(slot_name)
        if not os.path.exists(png):
            pngs = sorted(os.path.basename(p) for p in glob.glob(os.path.join(os.path.dirname(png), "*.png")))
            raise RuntimeError(f"SK_{character}: slot {slot_name} has no {os.path.basename(png)} (have {pngs})")
        stem = os.path.splitext(os.path.basename(png))[0]
        if stem not in instances:
            texture = import_texture_file(png, dest, f"T_{character}_{stem}")
            instance_name = f"MI_{character}_{stem}"
            instance = load_or_create(instance_name, lambda: TOOLS.create_asset(instance_name, dest, unreal.MaterialInstanceConstant,
                                                                                 unreal.MaterialInstanceConstantFactoryNew()), dest)
            MEL.set_material_instance_parent(instance, parent)
            MEL.set_material_instance_texture_parameter_value(instance, "BaseColor", texture)
            unreal.EditorAssetLibrary.save_loaded_asset(instance)
            instances[stem] = instance
        slots.append(unreal.SkeletalMaterial(material_interface=instances[stem], material_slot_name=slot_name))
    mesh.set_editor_property("materials", slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log(f"SK_{character}: {len(slots)} material slot(s) on M_Character")


def mixamo_texture(fbx_path):
    """<Id>_rigged.png next to the FBX, extracted when missing (Mixamo packs it where UE's importer can't read it)."""
    png = os.path.splitext(fbx_path)[0] + ".png"
    if not os.path.exists(png):
        script = os.path.join(PROJECT, "Scripts", "character", "extract_textures.py")
        subprocess.run([BLENDER, "-b", "-P", script, "--", fbx_path, png], check=True, capture_output=True)
    return png


def build_characters(ik_manny):
    convert_vrms()
    # (character, fbx, slot -> base-colour PNG): VRoid conversions, then Mixamo-rigged models (Rodin meshes etc.).
    found = []
    for fbx_path in sorted(glob.glob(os.path.join(VROID_DIR, "*", "*.fbx"))):
        character = os.path.basename(os.path.dirname(fbx_path))
        if os.path.splitext(os.path.basename(fbx_path))[0] == character:
            found.append((character, fbx_path, lambda slot, d=os.path.dirname(fbx_path): os.path.join(d, f"{slot}.png")))
    for fbx_path in sorted(glob.glob(os.path.join(MODELS_DIR, "*", "*_rigged.fbx"))):
        character = os.path.basename(os.path.dirname(fbx_path))
        # Models/Hero/Hero_rigged.fbx is the hero itself (imported above), not a second roster character.
        if os.path.basename(fbx_path) == f"{character}_rigged.fbx" and os.path.abspath(fbx_path) != os.path.abspath(HERO_FBX):
            png = mixamo_texture(fbx_path)
            found.append((character, fbx_path, lambda slot, p=png: p))
    if not found:
        log(f"no characters in {VROID_DIR} or {MODELS_DIR}")
        return
    parent = build_character_material()
    for character, character_fbx, png_for_slot in found:
        if not re.fullmatch(r"[A-Za-z0-9_]+", character):
            raise RuntimeError(f"character name {character!r}: use letters, digits and _ only (it becomes asset names)")
        dest = f"{CHARACTERS_DIR}/{character}"
        # A re-exported VRoid model can change its bones (hair, skirt), which UE refuses to merge into the old skeleton:
        # start the mesh fresh (IK_/RTG_ are rebuilt below and pick up the new one).
        for stale in (f"SK_{character}", f"SK_{character}_Skeleton", f"SK_{character}_PhysicsAsset"):
            if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stale}"):
                unreal.EditorAssetLibrary.delete_asset(f"{dest}/{stale}")
        mesh = import_hero(character_fbx, dest, f"SK_{character}", materials=False)
        use_materials(mesh, character, dest, png_for_slot, parent)
        build_retargeter(ik_manny, build_ik_rig(mesh, dest, f"IK_{character}"), dest, f"RTG_MannyTo{character}")


ROSTER = ("Hero", "Bika", "Cat", "Misha", "Pabu")  # AShinobiPlayerCharacter::Roster
HEM_MAX_DISTANCE_CM, SLEEVE_MAX_DISTANCE_CM = 25.0, 12.0  # how far the hem and sleeve cloth may swing (UShinobiOutfitBuilderLibrary)
TUNING_BODY = "Hero"  # the copy whose cloth the user tunes; its tuning is kept in Tuning/ across re-imports


def body_mesh_path(body):
    return f"{HERO_DIR}/SK_Hero" if body == "Hero" else f"{CHARACTERS_DIR}/{body}/SK_{body}"


def cloth_collision(body):
    """PA_<Body>_Cloth next to the body: what its outfits' cloth collides with (UShinobiOutfitBuilderLibrary::
    BuildClothCollision; not the ragdoll's physics asset), with the tail capsule from SourceArt/Character/Models/<Body>/
    <Body>_tail.json (build_outfits.ps1; null for a body without a tail)."""
    import json
    mesh = unreal.load_asset(body_mesh_path(body))
    if mesh is None:
        return None
    folder = body_mesh_path(body).rsplit("/", 1)[0]
    physics = unreal.ShinobiOutfitBuilderLibrary.build_cloth_collision(mesh, f"{folder}/PA_{body}_Cloth")
    if physics is None:
        raise RuntimeError(f"could not build {body}'s cloth collision")
    path = os.path.join(MODELS_DIR, body, f"{body}_tail.json")
    tail = None
    if os.path.exists(path):
        with open(path, encoding="utf-8") as handle:
            tail = json.load(handle)
    if tail:
        hips = unreal.Vector(*tail["hips"])
        root, tip = (unreal.Vector(*tail[k]) - hips for k in ("root", "tip"))
        if not unreal.ShinobiOutfitBuilderLibrary.add_tail_capsule(mesh, physics, root, tip, tail["radius"]):
            raise RuntimeError(f"could not add {body}'s tail capsule")
    unreal.EditorAssetLibrary.save_loaded_asset(physics)
    log(f"{body}: cloth collision{' with a tail capsule' if tail else ''}")
    return physics


def import_outfit_copy(fbx, dest, name):
    """An outfit copy keeps its vertex colours (skin_outfit.py's cloth regions) and gets no physics asset of its own."""
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.create_physics_asset = False
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = fbx, dest, name
    task.automated = task.save = task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, unreal.SkeletalMesh)), None)
    if mesh is None:
        raise RuntimeError(f"import of {fbx} produced no skeletal mesh")
    return mesh


def import_hide_mask(png, dest, name):
    texture = import_texture_file(png, dest, name)
    # A plain colour texture, as M_Character's HideMask sampler expects (black and white survive sRGB unchanged).
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def keep_tuning(outfit):
    """A copy of the tuned copy (SK_<Outfit>_<TUNING_BODY>) in Tuning/, so a re-import can put the user's tuning back."""
    dest = f"{OUTFITS_GAME_DIR}/{outfit}"
    source, kept = f"{dest}/SK_{outfit}_{TUNING_BODY}", f"{dest}/Tuning/SK_{outfit}_Tuning"
    if not unreal.EditorAssetLibrary.does_asset_exist(source):
        return
    if unreal.EditorAssetLibrary.does_asset_exist(kept):
        unreal.EditorAssetLibrary.delete_asset(kept)
    unreal.EditorAssetLibrary.duplicate_asset(source, kept)
    unreal.EditorAssetLibrary.save_asset(kept)


def build_outfits():
    """Every Outfits/<Outfit>/<Outfit>_<Body>_outfit.fbx (skin_outfit.py, one per roster body) -> /Game/Shinobi/Outfits/
    <Outfit>/SK_<Outfit>_<Body> with Chaos cloth on its hem and sleeves (colliding with PA_<Body>_Cloth, tail included), its
    hide mask <Outfit>_<Body>_hide.png -> T_<Outfit>_<Body>_Hide, all copies on one MI of <Outfit>_outfit.png. A tuning
    kept in Tuning/ (copy_cloth_tuning) is put back on every copy."""
    parent = build_character_material()
    collision = {body: cloth_collision(body) for body in ROSTER}
    for folder in sorted(glob.glob(os.path.join(OUTFITS_DIR, "*"))):
        outfit = os.path.basename(folder)
        copies = [(body, os.path.join(folder, f"{outfit}_{body}_outfit.fbx")) for body in ROSTER]
        copies = [(body, fbx) for body, fbx in copies if os.path.exists(fbx)]
        if not copies:
            continue
        if not re.fullmatch(r"[A-Za-z0-9_]+", outfit):
            raise RuntimeError(f"outfit name {outfit!r}: use letters, digits and _ only (it becomes asset names)")
        dest = f"{OUTFITS_GAME_DIR}/{outfit}"
        for stale in (f"SK_{outfit}", f"SK_{outfit}_Skeleton"):  # the single copy worn by every body before per-body fitting
            if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stale}"):
                unreal.EditorAssetLibrary.delete_asset(f"{dest}/{stale}")
        png = os.path.join(folder, f"{outfit}_outfit.png")
        meshes = []
        for body, fbx in copies:
            name = f"SK_{outfit}_{body}"
            for stale in (name, f"{name}_Skeleton"):
                if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stale}"):
                    unreal.EditorAssetLibrary.delete_asset(f"{dest}/{stale}")
            mesh = import_outfit_copy(fbx, dest, name)
            use_materials(mesh, outfit, dest, lambda slot, p=png: p, parent)
            if not unreal.ShinobiOutfitBuilderLibrary.build_cloth(mesh, collision[body], HEM_MAX_DISTANCE_CM, SLEEVE_MAX_DISTANCE_CM):
                raise RuntimeError(f"{name}: could not build its cloth")
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            hide = os.path.join(folder, f"{outfit}_{body}_hide.png")
            if os.path.exists(hide):
                import_hide_mask(hide, dest, f"T_{outfit}_{body}_Hide")
            meshes.append(mesh)
            log(f"{name}: imported with cloth")
        kept = unreal.load_asset(f"{dest}/Tuning/SK_{outfit}_Tuning") if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/Tuning/SK_{outfit}_Tuning") else None
        if kept is not None:
            if not unreal.ShinobiOutfitBuilderLibrary.copy_cloth_tuning(kept, meshes):
                raise RuntimeError(f"{outfit}: could not put the kept cloth tuning back")
            for mesh in meshes:
                unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            log(f"{outfit}: the kept cloth tuning is back on {len(meshes)} copies")


def copy_cloth_tuning(source):
    """SETUP_HERO_CLOTH_SOURCE=<Outfit>_<Body>: that copy's cloth tuning onto the outfit's other copies, and kept in
    Tuning/ for later re-imports."""
    outfit, _, body = source.partition("_")
    dest = f"{OUTFITS_GAME_DIR}/{outfit}"
    tuned = unreal.load_asset(f"{dest}/SK_{outfit}_{body}") if outfit and body else None
    if tuned is None:
        raise RuntimeError(f"SETUP_HERO_CLOTH_SOURCE={source!r}: no {dest}/SK_{outfit}_{body} (use <Outfit>_<Body>, e.g. Hokage_Hero)")
    others = [unreal.load_asset(f"{dest}/SK_{outfit}_{b}") for b in ROSTER if b != body]
    others = [m for m in others if m is not None]
    if not unreal.ShinobiOutfitBuilderLibrary.copy_cloth_tuning(tuned, others):
        raise RuntimeError(f"could not copy {source}'s cloth tuning")
    for mesh in others:
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    if body == TUNING_BODY:
        keep_tuning(outfit)
    else:
        log(f"(kept tuning is from SK_{outfit}_{TUNING_BODY}: tune that copy to have it survive re-imports)")
    log(f"{source}: cloth tuning copied onto {len(others)} copies")


if os.environ.get("SETUP_HERO_ONLY") == "outfits":
    # Re-imports just the outfits (set SETUP_HERO_ONLY=outfits before Scripts/run_python.ps1), leaving every body alone.
    build_outfits()
elif os.environ.get("SETUP_HERO_ONLY") == "cloth":
    # After tuning one copy's cloth in the Skeletal Mesh editor: SETUP_HERO_CLOTH_SOURCE=<Outfit>_<Body> copies it onto
    # that outfit's other copies (docs/character/outfit-steps.md, "Tuning the cloth").
    copy_cloth_tuning(os.environ.get("SETUP_HERO_CLOTH_SOURCE", ""))
elif os.environ.get("SETUP_HERO_ONLY") == "retargeters":
    # Re-configures every RTG_MannyTo<Id> in place on its existing IK rigs (e.g. after changing ARM_SPREAD_DEGREES).
    for path in unreal.EditorAssetLibrary.list_assets("/Game/Shinobi", recursive=True):
        name = path.rsplit(".", 1)[-1]
        if name.startswith("RTG_MannyTo"):
            controller = unreal.IKRetargeterController.get_controller(unreal.load_asset(path))
            build_retargeter(controller.get_ik_rig(unreal.RetargetSourceOrTarget.SOURCE), controller.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET),
                             path.rsplit("/", 1)[0], name)
else:
    fbx = HERO_FBX if os.path.exists(HERO_FBX) else STAND_IN_FBX
    log(f"source: {'hero' if fbx == HERO_FBX else 'STAND-IN'} {fbx}")
    ik_manny = unreal.load_asset(IK_MANNY_PATH)
    if ik_manny is None:
        raise RuntimeError(f"missing {IK_MANNY_PATH} (run Scripts/setup_chakra_pose.py first)")
    if not unreal.EditorAssetLibrary.does_directory_exist(HERO_DIR):
        unreal.EditorAssetLibrary.make_directory(HERO_DIR)
    hero_mesh = import_hero(fbx)
    if fbx == HERO_FBX:
        use_material(hero_mesh, build_material(import_texture()))
    build_retargeter(ik_manny, build_ik_rig(hero_mesh))
    remove_strays()
    build_characters(ik_manny)
    build_outfits()
