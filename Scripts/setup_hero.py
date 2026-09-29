"""Hero character assets (sub-project #2). Idempotent (re-imports and re-configures in place); close the GUI editor.

Source: SourceArt/Character/Mesh/hero_rigged.fbx (the Mixamo-rigged hero) when it exists, else the stand-in
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
Then the roster characters (AShinobiPlayerCharacter::Roster), one per SourceArt/Character/VRoid/<Id>.vrm made in VRoid
Studio (docs/character/vroid-steps.md): Scripts/character/vroid_to_fbx.py turns each into <Id>/<Id>.fbx + one PNG per
material (re-run when the .vrm is newer), and each FBX becomes /Game/Shinobi/Characters/<Id>/: SK_<Id> (+ skeleton,
physics asset), T_/MI_<Id>_<Slot> per material slot on the shared M_VRoid (masked by the texture alpha: eyelashes,
brows, hair edges; two-sided: hair and cloth cards), IK_<Id> and RTG_MannyTo<Id>, built exactly like the hero's.
Retargeter setup follows setup_chakra_pose.py's reasoning (exact chain mapping, auto-align the target chain to chain,
Run IK Rig op off). Root Motion op off too: the hero's root IS its pelvis (Hips), so "copy source root" would pin the
hips to Manny's floor-level root bone; the character moves by its capsule, not root motion.
"""
import glob
import os
import re
import subprocess
import unreal

HERO_DIR = "/Game/Shinobi/Hero"
IK_MANNY_PATH = "/Game/Shinobi/Retarget/IK_Manny"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
HERO_FBX = os.path.join(PROJECT, "SourceArt", "Character", "Mesh", "hero_rigged.fbx")
HERO_TEXTURE = os.path.splitext(HERO_FBX)[0] + ".png"
STAND_IN_FBX = os.path.join(PROJECT, "SourceArt", "Mixamo", "Power_Up_30fps.fbx")
VROID_DIR = os.path.join(PROJECT, "SourceArt", "Character", "VRoid")
CHARACTERS_DIR = "/Game/Shinobi/Characters"
BLENDER = "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"
MEL = unreal.MaterialEditingLibrary
# Every body chain a gameplay animation moves must reach the hero (fingers are optional: Mixamo's "Standard" skeleton
# has them, a "No fingers" download doesn't).
REQUIRED_CHAINS = ("Spine", "Neck", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log(f"[setup_hero] {message}")


def import_hero(fbx, dest=HERO_DIR, name="SK_Hero", materials=True):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.create_physics_asset = True
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
    if mesh.get_editor_property("physics_asset") is None:
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


def build_vroid_material():
    """M_VRoid: the hero's lit Substrate recipe, masked by the texture's alpha and two-sided; one instance per slot."""
    path = f"{CHARACTERS_DIR}/M_VRoid"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = TOOLS.create_asset("M_VRoid", CHARACTERS_DIR, unreal.Material, unreal.MaterialFactoryNew())
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
            raise RuntimeError(f"M_VRoid: could not connect {pin}")
    MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    if not MEL.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError("M_VRoid: could not connect the opacity mask")
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"M_VRoid failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
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


def use_vroid_materials(mesh, character, dest, png_dir, parent):
    """One MI_<Id>_<Slot> per material slot, its BaseColor the <Slot>.png vroid_to_fbx.py wrote."""
    slots = []
    for slot in mesh.get_editor_property("materials"):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        png = os.path.join(png_dir, f"{slot_name}.png")
        if not os.path.exists(png):
            pngs = sorted(os.path.basename(p) for p in glob.glob(os.path.join(png_dir, "*.png")))
            raise RuntimeError(f"SK_{character}: slot {slot_name} has no {slot_name}.png (have {pngs})")
        texture = import_texture_file(png, dest, f"T_{character}_{slot_name}")
        instance_name = f"MI_{character}_{slot_name}"
        instance = load_or_create(instance_name, lambda: TOOLS.create_asset(instance_name, dest, unreal.MaterialInstanceConstant,
                                                                             unreal.MaterialInstanceConstantFactoryNew()), dest)
        MEL.set_material_instance_parent(instance, parent)
        MEL.set_material_instance_texture_parameter_value(instance, "BaseColor", texture)
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
        slots.append(unreal.SkeletalMaterial(material_interface=instance, material_slot_name=slot_name))
    mesh.set_editor_property("materials", slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log(f"SK_{character}: {len(slots)} material slot(s) on M_VRoid")


def build_characters(ik_manny):
    convert_vrms()
    fbxs = sorted(glob.glob(os.path.join(VROID_DIR, "*", "*.fbx")))
    fbxs = [f for f in fbxs if os.path.splitext(os.path.basename(f))[0] == os.path.basename(os.path.dirname(f))]
    if not fbxs:
        log(f"no VRoid characters in {VROID_DIR}")
        return
    parent = build_vroid_material()
    for character_fbx in fbxs:
        character = os.path.basename(os.path.dirname(character_fbx))
        if not re.fullmatch(r"[A-Za-z0-9_]+", character):
            raise RuntimeError(f"character name {character!r}: use letters, digits and _ only (it becomes asset names)")
        dest = f"{CHARACTERS_DIR}/{character}"
        # A re-exported VRoid model can change its bones (hair, skirt), which UE refuses to merge into the old skeleton:
        # start the mesh fresh (IK_/RTG_ are rebuilt below and pick up the new one).
        for stale in (f"SK_{character}", f"SK_{character}_Skeleton", f"SK_{character}_PhysicsAsset"):
            if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stale}"):
                unreal.EditorAssetLibrary.delete_asset(f"{dest}/{stale}")
        mesh = import_hero(character_fbx, dest, f"SK_{character}", materials=False)
        use_vroid_materials(mesh, character, dest, os.path.dirname(character_fbx), parent)
        build_retargeter(ik_manny, build_ik_rig(mesh, dest, f"IK_{character}"), dest, f"RTG_MannyTo{character}")


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
