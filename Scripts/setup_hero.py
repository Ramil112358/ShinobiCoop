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


def import_hero(fbx, dest=HERO_DIR, name="SK_Hero", materials=True, physics=True):
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


def spread_arms(controller, ik_target):
    """Raises each upper arm ARM_SPREAD_DEGREES out from the body in the target retarget pose. Manny's arms swing close to
    his slim torso; copied 1:1 onto our bulkier bodies (and the cloaks over them) the hands sink into the hips. The FK chain
    op carries a retarget-pose offset into every animation, so this widens idle, run and jump alike.
    A pose delta is local (bone ref rotation * delta, IKRetargetProcessor.cpp), so the mesh-space swing is converted per bone."""
    target = unreal.RetargetSourceOrTarget.TARGET
    rig = unreal.IKRigController.get_controller(ik_target)
    pose = unreal.AnimPoseExtensions.get_reference_pose(rig.get_skeletal_mesh().skeleton)
    for bone in unreal.AnimPoseExtensions.get_bone_names(pose):  # the auto-aligned pose, so the arm's real direction is measured
        local = unreal.AnimPoseExtensions.get_bone_pose(pose, bone)
        local.rotation = local.rotation * controller.get_rotation_offset_for_retarget_pose_bone(bone, target)
        pose = unreal.AnimPoseExtensions.set_bone_pose(pose, local, bone)  # returns the edited pose (UPARAM(ref)), never edits in place
    world = unreal.AnimPoseSpaces.WORLD
    for chain in ("LeftArm", "RightArm"):
        upper, hand = rig.get_retarget_chain_start_bone(chain), rig.get_retarget_chain_end_bone(chain)
        arm = unreal.AnimPoseExtensions.get_bone_pose(pose, hand, world).translation - unreal.AnimPoseExtensions.get_bone_pose(pose, upper, world).translation
        # Rotating the arm about (arm x up) turns it toward up: away from the body for a hanging arm on either side.
        swing = unreal.MathLibrary.rotator_from_axis_and_angle(arm.normal().cross(unreal.Vector(0, 0, 1)).normal(), ARM_SPREAD_DEGREES).quaternion()
        bone_world = unreal.AnimPoseExtensions.get_bone_pose(pose, upper, world).rotation
        delta = controller.get_rotation_offset_for_retarget_pose_bone(upper, target)
        controller.set_rotation_offset_for_retarget_pose_bone(upper, delta * bone_world.inversed() * swing * bone_world, target)  # bone_world already holds delta
        # Check: re-pose the arm with the new delta; it must hang ARM_SPREAD_DEGREES further from straight down.
        local = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, upper)
        local.rotation = local.rotation * controller.get_rotation_offset_for_retarget_pose_bone(upper, target)
        pose = unreal.AnimPoseExtensions.set_bone_pose(pose, local, upper)  # returns the edited pose (UPARAM(ref)), never edits in place
        spread = unreal.AnimPoseExtensions.get_bone_pose(pose, hand, world).translation - unreal.AnimPoseExtensions.get_bone_pose(pose, upper, world).translation
        down = lambda v: math.degrees(math.acos(-v.normal().z))
        if abs(down(spread) - down(arm) - ARM_SPREAD_DEGREES) > 1:
            raise RuntimeError(f"spread_arms: {chain} went {down(arm):.1f} -> {down(spread):.1f} deg from down, wanted +{ARM_SPREAD_DEGREES}")
        log(f"{ik_target.get_name()} {chain}: {down(arm):.1f} -> {down(spread):.1f} deg from straight down")


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
    if os.environ.get("SETUP_HERO_ONLY") == "outfits" and unreal.EditorAssetLibrary.does_asset_exist(path):
        _character_material = unreal.load_asset(path)  # the bodies' instances stay on it
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
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"M_Character failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    _character_material = material
    return material


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


def build_outfits():
    """Every Outfits/<Id>/<Id>_outfit.fbx (+ <Id>_outfit.png) -> /Game/Shinobi/Outfits/<Id>/SK_<Id> on M_Character."""
    found = sorted(glob.glob(os.path.join(OUTFITS_DIR, "*", "*_outfit.fbx")))
    if not found:
        log(f"no outfits in {OUTFITS_DIR}")
        return
    parent = build_character_material()
    for outfit_fbx in found:
        outfit = os.path.basename(os.path.dirname(outfit_fbx))
        if os.path.basename(outfit_fbx) != f"{outfit}_outfit.fbx":
            continue
        if not re.fullmatch(r"[A-Za-z0-9_]+", outfit):
            raise RuntimeError(f"outfit name {outfit!r}: use letters, digits and _ only (it becomes asset names)")
        dest = f"{OUTFITS_GAME_DIR}/{outfit}"
        for stale in (f"SK_{outfit}", f"SK_{outfit}_Skeleton"):
            if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stale}"):
                unreal.EditorAssetLibrary.delete_asset(f"{dest}/{stale}")
        mesh = import_hero(outfit_fbx, dest, f"SK_{outfit}", materials=False, physics=False)
        png = os.path.splitext(outfit_fbx)[0] + ".png"
        use_materials(mesh, outfit, dest, lambda slot, p=png: p, parent)


if os.environ.get("SETUP_HERO_ONLY") == "outfits":
    # Re-imports just the outfits (set SETUP_HERO_ONLY=outfits before Scripts/run_python.ps1), leaving every body alone.
    build_outfits()
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
