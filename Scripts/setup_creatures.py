"""Imports the summon creatures built by Scripts/character/export_creature.py (SourceArt/Creatures/<Name>/*.fbx) into
/Game/Shinobi/Creatures/<Name>: SK_<Name> (+ its skeleton), ANIM_<Name>_<Clip> per clip, and MI_<Name>, an instance of
M_ShinobiSolid in the creature's colour.

Idempotent. Close the GUI editor first. Add a creature by adding it to CREATURES.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CREATURES = {
    # name: (colour or None, clips, base-colour PNG, or None, or a list with one entry per material slot: a PNG, or a flat colour).
    # With a PNG the creature wears M_Character (the heroes' lit material), or M_Creature when <png>_Normal.png and <png>_Roughness.png are beside it.
    # The files are SourceArt/Creatures/<SOURCE.get(name, name)>/<name>*.fbx.
    "Misha": (None, ["Idle", "Move", "Bark", "Hit", "Death"], "Misha.png"),  # Scripts/character/build_misha.py; he replaced the snake (still in /Game/Shinobi/Creatures/Snake, unused)
    "Tairn": (None, ["Idle", "Flap", "Glide", "Breath"], ["Tairn.png", "TairnClub.png", (0.02, 0.02, 0.025)]),  # body, club ball, club spikes
    "CrimsonDragon": (None, ["Idle", "Flap", "Glide", "Breath"], "CrimsonDragon.png"),
    "Toad": (None, ["Idle", "Move", "Slam", "Hit", "Death"], "Toad.png"),
    "Pabu": (None, ["Idle", "Walk", "Run", "Bite", "Hit", "Death"], "Pabu.png"),  # Scripts/character/build_pabu.py
}
SOURCE = {"CrimsonDragon": "DragonRodin"}  # the first Rodin dragon (Scripts/character/build_dragon_crimson.py); Tairn is in SourceArt/Creatures/Tairn
LEGACY = ["/Game/Shinobi/Creatures/Dragon"]  # the one dragon before there were two: now Tairn and CrimsonDragon
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary


def import_fbx(fbx, dest, name, options):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    return task.get_objects()


def skeletal_options(mesh):
    options = unreal.FbxImportUI()
    options.import_mesh = mesh
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if mesh else unreal.FBXImportType.FBXIT_ANIMATION
    options.import_animations = not mesh
    options.import_materials = False
    options.import_textures = False
    if mesh:
        options.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    return options


def import_texture(png, dest, name, kind="color"):
    """Imports a PNG as T_<name>. kind: "color", "normal" (a green-up map from Blender, flipped for Unreal) or "mask" (linear grey)."""
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = f"T_{name}"
    task.automated = True
    task.save = True
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(f"{dest}/T_{name}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import {png}")
    if kind != "color":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP if kind == "normal" else unreal.TextureCompressionSettings.TC_GRAYSCALE)
        texture.set_editor_property("flip_green_channel", kind == "normal")
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def creature_material(maps):
    """M_Creature: lit Substrate with base colour, normal and roughness maps (fur that has depth and is not glossy). Built if missing,
    with `maps` (param -> texture, the first creature's) as its defaults: each sampler needs a texture of its own kind."""
    path = "/Game/Shinobi/Creatures/M_Creature"
    material = unreal.load_asset(path)
    if material:
        return material
    material = TOOLS.create_asset("M_Creature", "/Game/Shinobi/Creatures", unreal.Material, unreal.MaterialFactoryNew())
    samples = {}
    for row, (param, sampler) in enumerate((("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                            ("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                            ("Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE))):
        sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, row * 260)
        sample.set_editor_property("parameter_name", param)
        sample.set_editor_property("sampler_type", sampler)
        sample.set_editor_property("texture", maps[param])
        samples[param] = sample
    spec = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -600, 800)
    spec.set_editor_property("r", 0.3)
    shading = MEL.create_material_expression(material, unreal.MaterialExpressionSubstrateShadingModels, -200, 0)
    for src, out, pin in ((samples["BaseColor"], "RGB", "BaseColor"), (samples["Normal"], "RGB", "Normal"), (samples["Roughness"], "R", "Roughness"), (spec, "", "Specular")):
        if not MEL.connect_material_expressions(src, out, shading, pin):
            raise RuntimeError(f"M_Creature: could not connect {pin}")
    MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"M_Creature failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def textured_material(name, png, dest):
    """MI_<name> on the base colour PNG. With <png>_Normal.png and <png>_Roughness.png beside it, on M_Creature with all three maps;
    else on M_Character (the heroes' lit material)."""
    stem = os.path.splitext(png)[0]
    maps = {"BaseColor": import_texture(png, dest, name)}
    if os.path.exists(f"{stem}_Normal.png") and os.path.exists(f"{stem}_Roughness.png"):
        maps["Normal"] = import_texture(f"{stem}_Normal.png", dest, f"{name}_Normal", "normal")
        maps["Roughness"] = import_texture(f"{stem}_Roughness.png", dest, f"{name}_Roughness", "mask")
    parent = creature_material(maps) if len(maps) == 3 else unreal.load_asset("/Game/Shinobi/Characters/M_Character")
    if parent is None:
        raise RuntimeError("M_Character is missing (Scripts/setup_hero.py)")
    path = f"{dest}/MI_{name}"
    mi = unreal.load_asset(path) or TOOLS.create_asset(f"MI_{name}", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for param, texture in maps.items():
        MEL.set_material_instance_texture_parameter_value(mi, param, texture)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def material_for(name, colour, dest):
    solid = unreal.load_asset("/Game/Shinobi/Materials/M_ShinobiSolid")
    if solid is None:
        raise RuntimeError("M_ShinobiSolid is missing (Scripts/setup_m5.py)")
    # An instance on a skeletal mesh needs the parent to carry the skeletal-mesh usage flag, baked in for cooked builds.
    MEL.set_base_material_usage(solid, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    unreal.EditorAssetLibrary.save_loaded_asset(solid)
    path = f"{dest}/MI_{name}"
    mi = unreal.load_asset(path) or TOOLS.create_asset(f"MI_{name}", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, solid)
    MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*colour, 1.0))
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


for legacy in LEGACY:
    if unreal.EditorAssetLibrary.does_directory_exist(legacy):
        unreal.EditorAssetLibrary.delete_directory(legacy)
ONLY = [n for n in os.environ.get("CREATURES_ONLY", "").split(",") if n]  # e.g. CREATURES_ONLY=Pabu: import just those
for name, (colour, clips, texture) in ((n, c) for n, c in CREATURES.items() if not ONLY or n in ONLY):
    source = os.path.join(ROOT, "SourceArt", "Creatures", SOURCE.get(name, name))
    dest = f"/Game/Shinobi/Creatures/{name}"
    unreal.EditorAssetLibrary.make_directory(dest)
    import_fbx(os.path.join(source, f"{name}.fbx"), dest, f"SK_{name}", skeletal_options(True))
    mesh = unreal.load_asset(f"{dest}/SK_{name}")
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"SK_{name} was not imported")
    skeleton = mesh.get_editor_property("skeleton")
    slots = list(mesh.get_editor_property("materials"))
    if not slots:
        raise RuntimeError(f"SK_{name} has no material slot")
    per_slot = texture if isinstance(texture, list) else [texture] * len(slots)
    if len(per_slot) != len(slots):
        raise RuntimeError(f"SK_{name} has {len(slots)} material slots, CREATURES lists {len(per_slot)}")
    for index, (slot, entry) in enumerate(zip(slots, per_slot)):
        suffix = f"{name}_{index}" if isinstance(texture, list) else name
        if isinstance(entry, str):
            mi = textured_material(suffix, os.path.join(source, entry), dest)
        else:
            mi = material_for(suffix, entry or colour, dest)
        slot.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials", slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    for clip in clips:
        options = skeletal_options(False)
        options.skeleton = skeleton
        objects = import_fbx(os.path.join(source, f"{name}_{clip}.fbx"), dest, f"ANIM_{name}_{clip}", options)
        if not any(isinstance(o, unreal.AnimSequence) for o in objects):
            raise RuntimeError(f"{name}_{clip}.fbx produced no animation")
    box = mesh.get_bounds()
    unreal.log(f"[setup_creatures] {name}: {len(clips)} clips, bounds origin {box.origin} extent {box.box_extent}")
