"""Imports the open world's art library into /Game/Shinobi/World/Library (sub-project #3, W2). UE Python commandlet.

Sources (see docs/world/asset-credits.md):
  SourceArt/World/Trees/*.fbx        Quaternius trees with trunk-only UCX collision (Scripts/world/blender/prep_trees.py)
  SourceArt/World/QuaterniusNature   bushes, grass, flowers, rocks, path stones (glTF)
  SourceArt/World/QuaterniusFantasyProps  props for the village and camps (glTF)
  SourceArt/World/Generated/*.glb    Japanese architecture and camp pieces (Scripts/world/blender/make_gap_pieces.py)

Consistency pass: Nanite on every mesh (no hand-made LODs needed); collision by category (trees: trunk UCX, rocks:
18-DOP, props: box, architecture: complex-as-simple, small foliage: none); the generated pieces' palette materials
(named Plaster, WoodDark, ...) become shared MI_Palette_* instances of M_ShinobiSolid. The Quaternius kits keep their
own (already consistent) textured materials. Writes Scripts/world/library_catalog.json, the only thing the world build
and scatter read. Missing packs are skipped with a warning; the world falls back to placeholders.
"""
import json
import os
import re

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT, "SourceArt", "World")
LIB = "/Game/Shinobi/World/Library"
CATALOG = os.path.join(PROJECT, "Scripts", "world", "library_catalog.json")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SML = unreal.EditorStaticMeshLibrary
MEL = unreal.MaterialEditingLibrary

NATURE = os.path.join(ART, "QuaterniusNature", "glTF")
PROPS = os.path.join(ART, "QuaterniusFantasyProps", "Exports", "glTF")
PALETTE = {
    "Plaster": (0.86, 0.80, 0.67), "WoodDark": (0.17, 0.10, 0.06), "WoodLight": (0.46, 0.31, 0.17),
    "RoofTile": (0.17, 0.23, 0.31), "RoofTileRed": (0.46, 0.13, 0.08), "Stone": (0.46, 0.45, 0.42),
    "Red": (0.62, 0.08, 0.05), "Paper": (0.96, 0.92, 0.82), "Cloth": (0.72, 0.63, 0.47),
    "ClothRed": (0.62, 0.11, 0.08), "Black": (0.05, 0.05, 0.05), "Rope": (0.66, 0.56, 0.36),
}
# category -> (source folder, file regex, collision kind)
SOURCES = {
    "Tree": (os.path.join(ART, "Trees"), r"\.fbx$", "ucx"),
    "Bush": (NATURE, r"^Bush_.*\.gltf$", "none"),
    "Grass": (NATURE, r"^(Grass_|Clover|Fern|Plant_|Flower_|Petal_|Mushroom_).*\.gltf$", "none"),
    "Rock": (NATURE, r"^Rock_Medium.*\.gltf$", "kdop"),
    "Pebble": (NATURE, r"^(Pebble_|RockPath_).*\.gltf$", "none"),
    "Prop": (PROPS, r"^(Barrel|Barrel_Apples|Crate_Wooden|Crate_Metal|FarmCrate_\w+|Bench|Table_Large|Stool|Bucket_Wooden_1|Chest_Wood|WeaponStand|Workbench|Anvil|Anvil_Log|Banner_1|Banner_2|Torch_Metal|Lantern_Wall|Pot_1|Vase_2|Rope_1|Bag|Cauldron|Stall_Cart_Empty|Dummy|Shield_Wooden)\.gltf$", "box"),
    "Arch": (os.path.join(ART, "Generated"), r"^(house_\w+|shrine|torii|gate|wall_segment|stall|lantern|stone_lantern|well|training_post|bridge)\.glb$", "complex"),
    "Camp": (os.path.join(ART, "Generated"), r"^(tent|palisade|campfire)\.glb$", "complex"),
}


def log(message):
    unreal.log(f"[import_library] {message}")


def palette_materials():
    parent = unreal.load_asset("/Game/Shinobi/Materials/M_ShinobiSolid")
    folder = f"{LIB}/Materials"
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)
    out = {}
    for key, colour in PALETTE.items():
        path = f"{folder}/MI_Palette_{key}"
        mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else TOOLS.create_asset(
            f"MI_Palette_{key}", folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, parent)
        MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*colour, 1))
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        out[key] = mi
    return out


def import_file(path, dest):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    return [o for o in task.get_objects() if isinstance(o, unreal.StaticMesh)]


def set_collision(mesh, kind):
    body = mesh.get_editor_property("body_setup")
    if kind == "ucx":
        return  # the FBX's UCX_ trunk
    SML.remove_collisions(mesh)
    if kind == "kdop":
        SML.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.NDOP18)
    elif kind == "box":
        SML.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    if body:
        flag = {"complex": unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE}.get(kind, unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        body.set_editor_property("collision_trace_flag", flag)
        if kind == "none":
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)


def consistency(mesh, kind, palette):
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    set_collision(mesh, kind)
    if kind == "complex":  # generated pieces: palette slot names -> shared instances
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            key = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            key = re.sub(r"_\d+$", "", key)
            if key in palette:
                mesh.set_material(index, palette[key])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def main():
    palette = palette_materials()
    catalog = {}
    for category, (folder, pattern, kind) in SOURCES.items():
        if not os.path.isdir(folder):
            unreal.log_warning(f"[import_library] missing {folder}: {category} skipped (placeholders will be used)")
            continue
        dest = f"{LIB}/{category}"
        files = sorted(f for f in os.listdir(folder) if re.search(pattern, f))
        entries = catalog.setdefault(category, {})
        for name in files:
            meshes = import_file(os.path.join(folder, name), dest)
            if not meshes:
                unreal.log_warning(f"[import_library] {name}: no static mesh imported")
                continue
            for mesh in meshes:
                consistency(mesh, kind, palette)
            base = os.path.splitext(name)[0]
            bounds = meshes[0].get_bounding_box()
            height_m = (bounds.max.z - bounds.min.z) / 100.0
            entries[base] = {"mesh": meshes[0].get_path_name(), "height_m": round(height_m, 2)}
        log(f"{category}: {len(entries)} meshes")
    with open(CATALOG, "w", encoding="utf-8", newline="\n") as f:
        json.dump(catalog, f, indent=1, sort_keys=True)
    log(f"catalog -> {CATALOG}")


main()
