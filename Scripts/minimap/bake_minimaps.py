"""Import validated tile sets (make_minimap_art.py) and create each level's map data asset.
The HUD finds Lvl_<key>'s data by name (UShinobiMiniMapData::ForLevel). UE Python commandlet:
  $env:SHINOBI_MINIMAP = "Arena_Tower,Arena_Camden"   # optional; default: every map in minimap_maps.json
  Scripts/run_python.ps1 Scripts/minimap/bake_minimaps.py"""
import json
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
CONFIG_PATH = os.path.join(ROOT, "Scripts", "minimap", "minimap_maps.json")
ART_ROOT = os.path.join(ROOT, "SourceArt", "World", "Minimaps")
CONFIG = json.load(open(CONFIG_PATH, encoding="utf-8"))
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


def validate_manifest(name, config, manifest):
    if manifest["world_min"] != config["world_min"] or manifest["world_max"] != config["world_max"]:
        raise RuntimeError(f"{name}: art bounds differ from minimap_maps.json")
    columns, rows = manifest["tile_count"]
    if columns <= 0 or rows <= 0 or len(manifest["tiles"]) != columns * rows:
        raise RuntimeError(f"{name}: incomplete tile grid in manifest")
    seen = set()
    for tile in manifest["tiles"]:
        coord = (tile["x"], tile["y"])
        if coord in seen or not (0 <= coord[0] < columns and 0 <= coord[1] < rows):
            raise RuntimeError(f"{name}: duplicate or out-of-range tile coordinate {coord}")
        seen.add(coord)
        if tile["size"][0] <= 0 or tile["size"][1] <= 0:
            raise RuntimeError(f"{name}: empty tile {coord}")
        if not os.path.isfile(os.path.join(ART_ROOT, name, tile["path"])):
            raise RuntimeError(f"{name}: missing tile source {tile['path']}")


def import_tiles(name, manifest, asset_dir):
    tasks = []
    object_paths = {}
    for tile in manifest["tiles"]:
        asset_name = f"T_{name}_{tile['x']}_{tile['y']}"
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(ART_ROOT, name, tile["path"]))
        task.set_editor_property("destination_path", asset_dir)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", True)
        task.set_editor_property("save", True)
        tasks.append(task)
        object_paths[(tile["x"], tile["y"])] = f"{asset_dir}/{asset_name}.{asset_name}"
    ASSET_TOOLS.import_asset_tasks(tasks)

    imported = {}
    for coord, object_path in object_paths.items():
        texture = unreal.load_asset(object_path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"{name}: imported texture missing at {object_path}")
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        # The HUD shows a 2048 px map in ~250 px: without mips the streets alias.
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        EAL.save_loaded_asset(texture, only_if_is_dirty=False)
        imported[coord] = texture
    return imported


def create_map_data(name, config, manifest, textures, asset_dir):
    asset_name = f"DA_{name}MiniMap"
    data_path = f"{asset_dir}/{asset_name}"
    data = EAL.load_asset(data_path) if EAL.does_asset_exist(data_path) else None
    if data is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ShinobiMiniMapData)
        data = ASSET_TOOLS.create_asset(asset_name, asset_dir, unreal.ShinobiMiniMapData, factory)
    if data is None or not isinstance(data, unreal.ShinobiMiniMapData):
        raise RuntimeError(f"{name}: could not create {data_path}")

    tiles = []
    for entry in manifest["tiles"]:
        coord = (entry["x"], entry["y"])
        tile = unreal.ShinobiMiniMapTile()
        tile.set_editor_property("x", coord[0])
        tile.set_editor_property("y", coord[1])
        tile.set_editor_property("uv_min", unreal.Vector2D(*entry["uv_min"]))
        tile.set_editor_property("uv_max", unreal.Vector2D(*entry["uv_max"]))
        tile.set_editor_property("base", textures[coord])
        tiles.append(tile)
    data.set_editor_property("world_min", unreal.Vector2D(*config["world_min"]))
    data.set_editor_property("world_max", unreal.Vector2D(*config["world_max"]))
    data.set_editor_property("tile_count", unreal.IntPoint(*manifest["tile_count"]))
    data.set_editor_property("tiles", tiles)
    if not data.has_valid_bounds() or not data.has_valid_tile_grid():
        raise RuntimeError(f"{name}: generated UShinobiMiniMapData failed native validation")
    EAL.save_loaded_asset(data, only_if_is_dirty=False)
    return data


def import_arrow():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(ROOT, "SourceArt", "UI", "minimap_arrow.png"))
    task.set_editor_property("destination_path", "/Game/Shinobi/UI/Minimaps")
    task.set_editor_property("destination_name", "T_MiniMap_Arrow")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    ASSET_TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset("/Game/Shinobi/UI/Minimaps/T_MiniMap_Arrow")
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    EAL.save_loaded_asset(texture, only_if_is_dirty=False)


wanted = [m.strip() for m in os.environ.get("SHINOBI_MINIMAP", "").split(",") if m.strip()] or list(CONFIG["maps"])
for name in wanted:
    config = CONFIG["maps"][name]
    art_dir = os.path.join(ART_ROOT, name)
    with open(os.path.join(art_dir, "manifest.json"), encoding="utf-8") as stream:
        manifest = json.load(stream)
    validate_manifest(name, config, manifest)
    asset_dir = f"/Game/Shinobi/UI/Minimaps/{name}"
    ensure_dir(asset_dir)
    textures = import_tiles(name, manifest, asset_dir)
    create_map_data(name, config, manifest, textures, asset_dir)
    unreal.log(f"[minimap_bake] {name}: {manifest['tile_count']} tiles, bounds {config['world_min']}..{config['world_max']} cm")
import_arrow()
unreal.log(f"[minimap_bake] baked {len(wanted)} maps")
