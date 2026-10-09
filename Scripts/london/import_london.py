"""Imports the London map's art into /Game/Shinobi/London (UE Python commandlet, via Scripts/run_python.ps1).

  materials  M_LondonPBR (Poly Haven albedo/normal/roughness at world-scale UVs x vertex-colour tint; FakeWindows paints
             a window grid on the backdrop's plain walls), M_LondonGlass (dark panes, a few lit rooms from vertex alpha),
             M_LondonFlat (colour x vertex colour), M_LondonSign (the signage atlas); one instance per palette name of
             Scripts/london/blender/make_city_meshes.py
  chunks     SourceArt/World/London/Chunks/{B,K,G,W}_<x>_<y>.glb -> City/<kind>/: slots remapped by palette name, Nanite
             (not water), collision complex-as-simple on buildings (B) and ground (G), none on backdrop and water
  props      SourceArt/World/London/Props/*.glb (fetch_models.py + prep_models.py) and the Poly Haven street props
  catalog    Scripts/london/london_catalog.json: furniture/landmark key -> mesh, from those and the Fab packs in Content
Resumable: meshes already imported are kept unless SHINOBI_REIMPORT=1. SHINOBI_LONDON_IMPORT=materials,chunks,props,catalog
picks steps (default all).
"""
import json
import os
import re
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "world"))
import import_library as lib  # noqa: E402

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT, "SourceArt", "World", "London")
ROOT = "/Game/Shinobi/London"
MATS = f"{ROOT}/Materials"
CATALOG = os.path.join(HERE, "london_catalog.json")
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REIMPORT = os.environ.get("SHINOBI_REIMPORT") == "1"

# palette name -> (Poly Haven set, metres per texture tile, tint)
PBR = {"Brick_Stock": ("brick_wall_02", 2.0), "Brick_Red": ("red_brick", 2.0), "Brick_Warehouse": ("brick_wall_001", 2.0),
       "Brick_Pale": ("brick_wall_09", 2.0), "Stone_Portland": ("white_sandstone_bricks_03", 3.0),
       "Stone_Gothic": ("sandstone_blocks_05", 3.0), "Stucco": ("plastered_wall", 3.0), "Concrete": ("concrete_wall_008", 3.0),
       "Trim_Portland": ("white_sandstone_bricks_03", 3.0), "Trim_Gothic": ("sandstone_blocks_05", 3.0),
       "Trim_Stucco": ("plastered_wall", 3.0), "Slate": ("grey_roof_tiles_02", 2.0), "Roof_Flat": ("concrete_floor", 4.0),
       "Asphalt": ("asphalt_02", 4.0), "Paving": ("large_grey_tiles", 2.4), "Kerb": ("granite_tile", 1.0),
       "Setts": ("cobblestone_floor_03", 2.0), "Embankment": ("stone_brick_wall_001", 3.0)}
FLAT = {"Frame_White": ((0.82, 0.82, 0.78), 0.45, 0.0), "Frame_Dark": ((0.03, 0.035, 0.04), 0.35, 0.6),
        "Shop_Paint": ((1, 1, 1), 0.4, 0.0), "Door": ((0.04, 0.05, 0.06), 0.35, 0.0), "ChimneyPot": ((0.5, 0.25, 0.15), 0.8, 0.0),
        "Paint_White": ((0.8, 0.8, 0.78), 0.55, 0.0), "Paint_Yellow": ((0.85, 0.6, 0.03), 0.55, 0.0),
        "Metal_Green": ((0.06, 0.16, 0.1), 0.4, 0.5), "Steel_White": ((0.82, 0.84, 0.86), 0.3, 0.8),
        "Paint_Blue": ((0.2, 0.42, 0.68), 0.4, 0.2), "Curtain_Glass": ((0.16, 0.21, 0.27), 0.05, 0.9),
        "Capsule_Glass": ((0.22, 0.27, 0.3), 0.04, 0.8), "Lamp_Glass": ((0.95, 0.85, 0.6), 0.2, 0.0),
        "Beacon_Amber": ((1.0, 0.45, 0.03), 0.25, 0.0)}
CHUNK_KINDS = {"B": "complex", "G": "complex", "K": "none", "W": "none", "X": "complex"}

FACADE_HLSL = r"""
float3 c = Albedo * VC * Tint;
float w = 0;
if (Fake > 0.5 && abs(N.z) < 0.5) {
  float2 cell = float2(UV.x / 3.0, UV.y / 3.3);
  float2 g = frac(cell);
  w = step(0.28, g.x) * step(g.x, 0.72) * step(0.32, g.y) * step(g.y, 0.8) * step(4.0, UV.y);
  float lit = step(0.9, frac(sin(dot(floor(cell), float2(12.9898, 78.233))) * 43758.5453));
  c = lerp(c, lerp(float3(0.012, 0.014, 0.017), float3(0.55, 0.42, 0.25), lit * 0.35), w);
}
return float4(c, w);
"""
GLASS_HLSL = r"""
float room = A * A;
return lerp(float3(0.008, 0.009, 0.011), float3(0.16, 0.13, 0.09), room);
"""


def log(message):
    unreal.log(f"[import_london] {message}")


def connect(m, a, out, b, pin):
    if not MEL.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"{m.get_name()}: could not connect {a.get_name()}.{out} -> {b.get_name()}.{pin}")


def expr(m, cls, x, y, **props):
    node = MEL.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        node.set_editor_property(k, v)
    return node


def custom(m, code, inputs, out_type, x, y, desc):
    node = expr(m, unreal.MaterialExpressionCustom, x, y, code=code, output_type=out_type, description=desc)
    pins = []
    for name in inputs:
        e = unreal.CustomInput()
        e.set_editor_property("input_name", name)
        pins.append(e)
    node.set_editor_property("inputs", pins)
    return node


def fresh_material(name):
    path = f"{MATS}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return TOOLS.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())


def finish(m):
    MEL.set_base_material_usage(m, unreal.MaterialUsage.MATUSAGE_NANITE, True)
    MEL.set_base_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    errors = MEL.recompile_material(m)
    if errors:
        raise RuntimeError(f"{m.get_name()} failed to compile: {errors}")
    EAL.save_loaded_asset(m)
    return m


def slab_for(m, x=-100, y=0):
    return expr(m, unreal.MaterialExpressionSubstrateShadingModels, x, y)


def london_pbr(defaults):
    m = fresh_material("M_LondonPBR")
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    tiling = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 150, parameter_name="Tiling", default_value=0.5)
    scaled = expr(m, unreal.MaterialExpressionMultiply, -1400, 50)
    connect(m, uv, "", scaled, "A")
    connect(m, tiling, "", scaled, "B")
    tex = {}
    for i, (name, sampler) in enumerate((("Albedo", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                         ("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                         ("Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))):
        t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -300 + i * 300, parameter_name=name,
                 sampler_type=sampler, texture=defaults[name])
        connect(m, scaled, "", t, "UVs")
        tex[name] = t
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 700)
    normal_ws = expr(m, unreal.MaterialExpressionVertexNormalWS, -1100, 850)
    tint = expr(m, unreal.MaterialExpressionVectorParameter, -1100, 1000, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    fake = expr(m, unreal.MaterialExpressionScalarParameter, -1100, 1150, parameter_name="FakeWindows", default_value=0.0)
    facade = custom(m, FACADE_HLSL, ("Albedo", "VC", "Tint", "UV", "N", "Fake"), unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                    -700, 400, "Facade")
    connect(m, tex["Albedo"], "RGB", facade, "Albedo")
    connect(m, vc, "", facade, "VC")
    connect(m, tint, "", facade, "Tint")
    connect(m, uv, "", facade, "UV")
    connect(m, normal_ws, "", facade, "N")
    connect(m, fake, "", facade, "Fake")
    colour = expr(m, unreal.MaterialExpressionComponentMask, -450, 300, r=True, g=True, b=True, a=False)
    connect(m, facade, "", colour, "")
    window = expr(m, unreal.MaterialExpressionComponentMask, -450, 450, r=False, g=False, b=False, a=True)
    connect(m, facade, "", window, "")
    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, -300, 150, const_b=0.08)
    connect(m, tex["Roughness"], "R", rough, "A")
    connect(m, window, "", rough, "Alpha")
    slab = slab_for(m)
    connect(m, colour, "", slab, "BaseColor")
    connect(m, rough, "", slab, "Roughness")
    connect(m, tex["Normal"], "RGB", slab, "Normal")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    return finish(m)


def london_glass():
    m = fresh_material("M_LondonGlass")
    vc = expr(m, unreal.MaterialExpressionVertexColor, -900, 0)
    glass = custom(m, GLASS_HLSL, ("A",), unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, 0, "Room")
    connect(m, vc, "A", glass, "A")
    slab = slab_for(m)
    connect(m, glass, "", slab, "BaseColor")
    connect(m, expr(m, unreal.MaterialExpressionConstant, -400, 200, r=0.04), "", slab, "Roughness")
    connect(m, expr(m, unreal.MaterialExpressionConstant, -400, 300, r=1.0), "", slab, "Specular")
    # A few lit rooms (vertex alpha > 0.93): warm emissive, so windows read as lived-in.
    lit = expr(m, unreal.MaterialExpressionCustom, -600, 400, code="return float3(1.0, 0.72, 0.42) * smoothstep(0.93, 1.0, A) * Strength;",
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Lit")
    pins = []
    for name in ("A", "Strength"):
        e = unreal.CustomInput()
        e.set_editor_property("input_name", name)
        pins.append(e)
    lit.set_editor_property("inputs", pins)
    connect(m, vc, "A", lit, "A")
    connect(m, expr(m, unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name="LitStrength", default_value=6.0), "", lit, "Strength")
    connect(m, lit, "", slab, "Emissive Color")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    return finish(m)


def london_flat():
    m = fresh_material("M_LondonFlat")
    vc = expr(m, unreal.MaterialExpressionVertexColor, -900, 0)
    col = expr(m, unreal.MaterialExpressionVectorParameter, -900, 200, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = expr(m, unreal.MaterialExpressionMultiply, -600, 100)
    connect(m, vc, "", mul, "A")
    connect(m, col, "", mul, "B")
    slab = slab_for(m)
    connect(m, mul, "", slab, "BaseColor")
    connect(m, expr(m, unreal.MaterialExpressionScalarParameter, -600, 300, parameter_name="Roughness", default_value=0.5), "", slab, "Roughness")
    connect(m, expr(m, unreal.MaterialExpressionScalarParameter, -600, 400, parameter_name="Metallic", default_value=0.0), "", slab, "Metallic")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    return finish(m)


SCREEN_HLSL = r"""
float idx = floor(fmod(Time / 6.0, 8.0));
float col = fmod(idx, 2.0);
float row = floor(idx / 2.0);
return float2((UV.x + col) * 0.5, (UV.y + row) * 0.25);
"""


def london_screen(texture):
    """M_LondonScreen: the Piccadilly LED screen, cycling the eight adverts of the 2 x 4 atlas every 6 s."""
    m = fresh_material("M_LondonScreen")
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    cycle = custom(m, SCREEN_HLSL, ("Time", "UV"), unreal.CustomMaterialOutputType.CMOT_FLOAT2, -750, 0, "AdCycle")
    connect(m, expr(m, unreal.MaterialExpressionTime, -1000, 150), "", cycle, "Time")
    connect(m, uv, "", cycle, "UV")
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0, parameter_name="Atlas", texture=texture)
    connect(m, cycle, "", t, "UVs")
    glow = expr(m, unreal.MaterialExpressionMultiply, -250, 100)
    connect(m, t, "RGB", glow, "A")
    connect(m, expr(m, unreal.MaterialExpressionScalarParameter, -500, 300, parameter_name="Nits", default_value=12.0), "", glow, "B")
    slab = slab_for(m)
    connect(m, expr(m, unreal.MaterialExpressionConstant3Vector, -250, -100, constant=unreal.LinearColor(0.01, 0.01, 0.01, 1)), "", slab, "BaseColor")
    connect(m, glow, "", slab, "Emissive Color")
    connect(m, expr(m, unreal.MaterialExpressionConstant, -250, 250, r=0.2), "", slab, "Roughness")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    return finish(m)


def london_sign(texture):
    m = fresh_material("M_LondonSign")
    t = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name="Atlas", texture=texture)
    slab = slab_for(m)
    connect(m, t, "RGB", slab, "BaseColor")
    connect(m, expr(m, unreal.MaterialExpressionConstant, -400, 200, r=0.45), "", slab, "Roughness")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    return finish(m)


def instance(name, parent, scalars=None, vectors=None, textures=None):
    path = f"{MATS}/{name}"
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1))
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, v)
    EAL.save_loaded_asset(mi)
    return mi


def materials():
    """Palette name -> material instance (also BD_<name>: the backdrop's fake-window variant)."""
    sets = {}
    for tex_id, _ in set(PBR.values()):
        lib.texture_set(tex_id, "2k")  # imports T_<id>_A/N/R/D into the shared world library
        base = f"{lib.LIB}/Textures/{tex_id}"
        sets[tex_id] = {k: unreal.load_asset(f"{base}/T_{tex_id}_{s}") for k, s in (("Albedo", "A"), ("Normal", "N"), ("Roughness", "R"))}
    parent = london_pbr(next(iter(sets.values())))
    out = {}
    for name, (tex_id, tile) in PBR.items():
        tint = (1.08, 1.06, 1.02) if name.startswith("Trim_") else (0.42, 0.42, 0.44) if name == "Roof_Flat" else (1, 1, 1)
        out[name] = instance(f"MI_L_{name}", parent, {"Tiling": 1.0 / tile}, {"Tint": tint}, sets[tex_id])
        out["BD_" + name] = instance(f"MI_L_BD_{name}", parent, {"Tiling": 1.0 / tile, "FakeWindows": 1.0}, {"Tint": tint}, sets[tex_id])
    out["Glass"] = london_glass()
    flat = london_flat()
    for name, (rgb, rough, metal) in FLAT.items():
        out[name] = instance(f"MI_L_{name}", flat, {"Roughness": rough, "Metallic": metal}, {"Color": rgb})
    sign_tex = lib.texture(os.path.join(ART, "T_Signage.png"), f"{ROOT}/Textures", "T_Signage", "color")
    out["Signage"] = london_sign(sign_tex)
    ads = lib.texture(os.path.join(ART, "T_ScreenAds.png"), f"{ROOT}/Textures", "T_ScreenAds", "color")
    out["Screen"] = london_screen(ads)
    out["Water"] = unreal.load_asset("/Game/Shinobi/World/Water/M_WaterReal")
    out["Collision"] = out["Concrete"]  # never rendered: the X walls are hidden in game
    log(f"materials: {len(out)} palette entries")
    return out


def palette_from_assets():
    out = {}
    for name in list(PBR) + list(FLAT):
        for key in (name, "BD_" + name):
            path = f"{MATS}/MI_L_{key}"
            if EAL.does_asset_exist(path):
                out[key] = unreal.load_asset(path)
    flat = unreal.load_asset(f"{MATS}/M_LondonFlat")
    for name, (rgb, rough, metal) in FLAT.items():  # palette entries added after the last full material build
        if name not in out:
            out[name] = instance(f"MI_L_{name}", flat, {"Roughness": rough, "Metallic": metal}, {"Color": rgb})
    out["Glass"] = unreal.load_asset(f"{MATS}/M_LondonGlass")
    out["Signage"] = unreal.load_asset(f"{MATS}/M_LondonSign")
    out["Screen"] = unreal.load_asset(f"{MATS}/M_LondonScreen")
    out["Water"] = unreal.load_asset("/Game/Shinobi/World/Water/M_WaterReal")
    out["Collision"] = out.get("Concrete")
    return out


def remap(mesh, palette):
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        key = re.sub(r"(_\d+)+$", "", str(slot.get_editor_property("material_slot_name")).split(".")[0])
        if key in palette:
            mesh.set_material(index, palette[key])
        else:
            unreal.log_warning(f"[import_london] {mesh.get_name()}: unknown slot {key}")


def chunks(palette):
    folder = os.path.join(ART, "Chunks")
    only = [c for c in os.environ.get("SHINOBI_LONDON_CHUNKS", "").split(",") if c]  # e.g. B_-4_-5 (tests)
    files = sorted(f for f in os.listdir(folder) if f.endswith(".glb") and (not only or f[:-4] in only))
    done = 0
    for name in files:
        kind = name[0]
        dest = f"{ROOT}/City/{kind}"
        stem = os.path.splitext(name)[0]
        existing = [p for p in EAL.list_assets(dest, recursive=True) if p.split(".")[-1] == stem] if EAL.does_directory_exist(dest) else []
        if existing and not REIMPORT:
            continue
        meshes = lib.import_file(os.path.join(folder, name), dest)
        for mesh in meshes:
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("enabled", kind not in ("W", "X"))
            if kind == "G":  # collision uses the Nanite fallback: keep kerbs exact (AUTO decimated to ~2%; G chunks are small)
                nanite.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
                nanite.set_editor_property("fallback_percent_triangles", 1.0)
            mesh.set_editor_property("nanite_settings", nanite)
            lib.set_collision(mesh, CHUNK_KINDS[kind])
            remap(mesh, palette)
            EAL.save_loaded_asset(mesh)
        done += 1
        if done % 25 == 0:
            log(f"chunks: {done} imported")
    # Interchange makes a material per glTF slot; the palette replaced them all.
    for kind in CHUNK_KINDS:
        for p in EAL.list_assets(f"{ROOT}/City/{kind}", recursive=True):
            asset = EAL.find_asset_data(p).get_asset()
            if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
                EAL.delete_asset(p.split(".")[0])
    log(f"chunks: {done} imported this run, {len(files)} total")


PH_PROPS = ["street_lamp_01", "street_lamp_02", "metal_trash_can", "concrete_road_barrier", "concrete_road_barrier_02",
            "utility_box_01", "utility_box_02", "exterior_aircon_unit", "water_manhole_cover", "modular_street_seating",
            "painted_wooden_bench", "outdoor_table_chair_set_01", "planter_box_01", "rollershutter_door",
            "rollershutter_window_01", "security_camera_01", "trashbag", "cardboard_box_01", "plastic_crate_01", "wooden_crate_01"]
# key -> {slot name substring: (colour, roughness, metallic)}: slots whose glTF materials import wrongly in UE
# (the bus body's metallic texture rendered yellow-green).
REPAINT = {"bus": {"london_bus_body": ((0.45, 0.012, 0.01), 0.3, 0.2), "london_bus_glass": ((0.02, 0.025, 0.03), 0.05, 0.6),
                   "london_bus_wheel": ((0.05, 0.05, 0.055), 0.6, 0.3)}}


def repaint(mesh, key):
    flat = unreal.load_asset(f"{MATS}/M_LondonFlat")
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        for part, (rgb, rough, metal) in REPAINT.get(key, {}).items():
            if part in name:
                mesh.set_material(index, instance(f"MI_L_{key}_{part}", flat, {"Roughness": rough, "Metallic": metal}, {"Color": rgb}))


LANDMARKS = {"big_ben", "st_pauls", "nelsons_column", "cab_shelter", "eros", "white_tower"}


def pieces(palette):
    """Procedural landmarks (blender/make_london_pieces.py): palette slots like the chunks, complex collision."""
    out = {}
    folder = os.path.join(ART, "Pieces")
    for name in sorted(f for f in os.listdir(folder) if f.endswith(".glb")):
        key = name[:-4]
        meshes = lib.import_file(os.path.join(folder, name), f"{ROOT}/Pieces/{key}")
        for mesh in meshes:
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", nanite)
            lib.set_collision(mesh, "none" if key == "piccadilly_lights" else "complex")
            remap(mesh, palette)
            EAL.save_loaded_asset(mesh)
        if meshes:
            out[key] = meshes[0].get_path_name()
    for p in EAL.list_assets(f"{ROOT}/Pieces", recursive=True):
        asset = EAL.find_asset_data(p).get_asset()
        if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            EAL.delete_asset(p.split(".")[0])
    log(f"pieces: {sorted(out)}")
    return out


def import_props():
    out = {}
    folder = os.path.join(ART, "Props")
    for name in sorted(os.listdir(folder)):
        key = os.path.splitext(name)[0]
        dest = f"{ROOT}/Props/{key}"
        found = [p for p in EAL.list_assets(dest, recursive=True) if "/StaticMeshes/" in p] if EAL.does_directory_exist(dest) else []
        meshes = [unreal.load_asset(p.split(".")[0]) for p in found] if found and not REIMPORT else lib.import_file(os.path.join(folder, name), dest)
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
        if not meshes:
            unreal.log_warning(f"[import_london] {name}: no mesh")
            continue
        for mesh in meshes:
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", nanite)
            lib.set_collision(mesh, "complex" if key in LANDMARKS else "box")
            repaint(mesh, key)
            EAL.save_loaded_asset(mesh)
        out[key] = meshes[0].get_path_name()
    for asset_id in PH_PROPS:
        src = os.path.join(lib.PH_DIR, asset_id)
        if not os.path.isdir(src):
            continue
        dest = f"{ROOT}/PolyHaven/{asset_id}"
        found = [p for p in EAL.list_assets(dest, recursive=True) if "/StaticMeshes/" in p] if EAL.does_directory_exist(dest) else []
        if found and not REIMPORT:
            meshes = [unreal.load_asset(p.split(".")[0]) for p in found]
        else:
            gltf = next(f for f in os.listdir(src) if f.endswith(".gltf"))
            meshes = lib.import_file(os.path.join(src, gltf), dest)
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh) and not re.search(r"_LOD[1-9]$", m.get_name())]
        for mesh in meshes:
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", nanite)
            lib.set_collision(mesh, "box")
            EAL.save_loaded_asset(mesh)
        if meshes:
            out[f"ph_{asset_id}"] = meshes[0].get_path_name()
    log(f"props: {len(out)} meshes")
    return out


TREES = ["tree_small_02", "jacaranda_tree"]  # Poly Haven broadleaf trees standing in for London planes


def trees():
    """[{mesh, height_m}] of the street-tree variants (the valley's foliage path: masked leaves, trunk collision)."""
    out = []
    for asset_id in TREES:
        for entry in lib.realistic_model(asset_id, "Tree").values():
            out.append({"mesh": entry["mesh"], "height_m": entry["height_m"]})
    log(f"trees: {len(out)} variants")
    return out


def soft_glass():
    """MI_L_Glass: M_LondonGlass with the lit rooms and shops toned down for daylight (they blew out to white at 6),
    swapped into the building chunks' Glass slots in place (no reimport, no Nanite rebuild)."""
    parent = unreal.load_asset(f"{MATS}/M_LondonGlass")
    mi = instance("MI_L_Glass", parent, {"LitStrength": 1.2})
    swapped = 0
    for path in EAL.list_assets(f"{ROOT}/City/B", recursive=True):
        mesh = unreal.load_asset(path.split(".")[0])
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            if slot.get_editor_property("material_interface") == parent:
                mesh.set_material(index, mi)
                swapped += 1
        EAL.save_loaded_asset(mesh)
    log(f"glass: {swapped} slots now MI_L_Glass")


def no_distance_fields():
    """Backdrop (K), water (W) and invisible walls (X) need no mesh distance field (Lumen's software tracing uses them;
    these are far away, transparent or hidden). Each one cost ~5-10 s to build before Play in a fresh editor session."""
    off = 0
    tiny = unreal.load_asset("/Engine/BasicShapes/Cube")
    for kind in ("K", "W", "X"):
        for path in EAL.list_assets(f"{ROOT}/City/{kind}", recursive=True):
            mesh = unreal.load_asset(path.split(".")[0])
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            # No per-mesh off switch: build it from a tiny stand-in mesh at minimum resolution instead (near-free).
            if not unreal.ShinobiWorldBuilderLibrary.set_distance_field_settings(mesh, 0.05, tiny):
                continue
            EAL.save_loaded_asset(mesh)
            off += 1
    log(f"nodf: distance fields off on {off} meshes")


def usage():
    """Every catalog mesh's materials get the instanced-static-mesh and Nanite usage flags: the street furniture is
    placed as HISM instances, and a material without the flag renders as the default checker in game."""
    with open(CATALOG, encoding="utf-8") as f:
        cat = json.load(f)
    paths = [v for k, v in cat.items() if isinstance(v, str)] + [t["mesh"] for t in cat.get("_tree_variants", [])]
    done = set()
    for path in paths:
        mesh = unreal.load_asset(path.split(".")[0])
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        for slot in mesh.get_editor_property("static_materials"):
            mi = slot.get_editor_property("material_interface")
            base = mi.get_base_material() if mi else None
            if base is None or base.get_path_name() in done:
                continue
            done.add(base.get_path_name())
            changed = False
            for flag in (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, unreal.MaterialUsage.MATUSAGE_NANITE):
                needed = MEL.set_base_material_usage(base, flag, True)
                changed = changed or bool(needed)
            EAL.save_loaded_asset(base)
    log(f"usage: {len(done)} base materials flagged")
    gltf_parent()


GLTF_ENGINE = "/InterchangeAssets/gltf/Substrate/M_GLTF"


def gltf_parent():
    """glTF-imported props (cars, bus, phone box...) are instances of the engine's read-only M_GLTF, which can't take the
    instanced-static-mesh flag, so in game they render as the grey default checker. Reparent them to a project copy."""
    copy = f"{MATS}/M_GLTF"
    if not EAL.does_asset_exist(copy):
        EAL.duplicate_asset(GLTF_ENGINE, copy)
    parent = unreal.load_asset(copy)
    for flag in (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, unreal.MaterialUsage.MATUSAGE_NANITE):
        MEL.set_base_material_usage(parent, flag, True)
    MEL.recompile_material(parent)
    EAL.save_loaded_asset(parent)
    engine = unreal.load_asset(GLTF_ENGINE)
    moved = 0
    for path in EAL.list_assets("/Game", recursive=True):
        mi = unreal.load_asset(path.split(".")[0]) if "/Materials/" in path or "/Props/" in path else None
        if isinstance(mi, unreal.MaterialInstanceConstant) and mi.get_editor_property("parent") == engine:
            MEL.set_material_instance_parent(mi, parent)
            EAL.save_loaded_asset(mi, False)  # reparenting doesn't dirty the package
            moved += 1
    log(f"gltf: {moved} instances moved to {copy}")


def painted_copy(source, name, colour, roughness=0.35, metallic=0.3, only_slots=None):
    """A copy of a Fab mesh repainted with one flat colour: the free London street props render white in this
    project (their materials lost their textures). only_slots: repaint just the slots whose name contains one of
    these (the bus shelter's glass), keeping the rest."""
    if not source:
        return None
    path = f"{ROOT}/Props/{name}/SM_{name}"
    if not EAL.does_asset_exist(path):
        EAL.duplicate_asset(source.split(".")[0], path)
    mesh = unreal.load_asset(path)
    paint = instance(f"MI_L_{name}", unreal.load_asset(f"{MATS}/M_LondonFlat"), {"Roughness": roughness, "Metallic": metallic},
                     {"Color": colour})
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        if only_slots is None or any(s.lower() in slot_name.lower() for s in only_slots):
            mesh.set_material(index, paint)
    EAL.save_loaded_asset(mesh)
    return mesh.get_path_name()


def find_mesh(folder, pattern):
    """First static mesh under a Content folder whose name matches the regex (Fab packs as the user added them)."""
    if not EAL.does_directory_exist(folder):
        return None
    for p in sorted(EAL.list_assets(folder, recursive=True)):
        name = p.split(".")[-1]
        if re.search(pattern, name):
            data = EAL.find_asset_data(p)
            if str(data.asset_class_path.asset_name) == "StaticMesh":
                return p
    return None


def catalog(props):
    """catalog.pieces: the procedural landmarks of this run (or from the earlier one in london_catalog.json)."""
    with open(os.path.join(PROJECT, "Scripts", "world", "library_catalog.json"), encoding="utf-8") as f:
        world = json.load(f)
    maples = catalog.trees
    fab = "/Game/Fab"
    entries = {
        "lamp_modern": painted_copy(find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_StreetLight01$"), "LampDark",
                                    (0.03, 0.032, 0.035), 0.4, 0.6),
        "lamp_heritage": find_mesh(f"{fab}/Victorian_Street_Lamp", r"."),
        "phone_box": props.get("phone_box"),  # Sketchfab K6, door closed (the Fab booth's door is modelled open)
        "post_box": painted_copy(find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_PostBox01$"), "PostBoxRed",
                                 (0.5, 0.012, 0.01), 0.35, 0.1),
        "bin": painted_copy(find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_Bin01$"), "BinBlack", (0.015, 0.015, 0.016)),
        "belisha_beacon": find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_ZebraPillar01$"),
        "bollard": find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_Bollard01$"),
        "railing": find_mesh(f"{fab}/London_Street_Props__Free_", r"^S_Fence01$"),
        "bus_stop": painted_copy(find_mesh("/Game/BusStop_Shelter_A", r"^BusStop_Shelter_A$"), "BusShelter",
                                 (0.12, 0.15, 0.17), 0.05, 0.8, only_slots=("Glass",)),  # its translucent glass didn't show
        "road_barrier": find_mesh("/Game/LondonSamplePack", r"^SM_Rail_Roadworks_01$"),
        "traffic_cone": find_mesh("/Game/LondonSamplePack", r"^SM_TrafficCone_01$"),
        "car_0": find_mesh("/Game/VehicleVarietyPack", r"^SM_Hatchback$"),
        "car_3": find_mesh("/Game/VehicleVarietyPack", r"^SM_SUV$"),
        "a_board": find_mesh("/Game/Deko_MatrixDemo", r"^SM_StandingSignAd_A01_N1$"),
        "scaffolding": find_mesh("/Game/Deko_MatrixDemo", r"^SM_Scaffolding_A01_N1$"),
        "car_1": props.get("transit_van"), "car_2": props.get("prius"), "black_cab": props.get("black_cab"),  # no Mini: its textures map wrongly in UE
        "van": props.get("sprinter_van"), "bus": props.get("bus"), "traffic_light": props.get("traffic_light"),
        "bench": props.get("camel_bench"), "narrowboat": props.get("narrowboat"), "cab_shelter": props.get("cab_shelter"),
        "big_ben": props.get("big_ben"), "nelsons_column": props.get("nelsons_column"), "st_pauls": props.get("st_pauls"),
        "tree_plane": maples[0]["mesh"] if maples else None,
        "elizabeth_tower": props.get("big_ben"), "eros": props.get("eros"), "white_tower": props.get("white_tower"),
    }
    entries.update(catalog.pieces)
    entries.update({k: v for k, v in props.items() if k.startswith("ph_")})
    entries["_tree_variants"] = maples
    missing = sorted(k for k, v in entries.items() if not v)
    with open(CATALOG, "w", encoding="utf-8", newline="\n") as f:
        json.dump(entries, f, indent=1, sort_keys=True)
    log(f"catalog: {len(entries)} keys -> {CATALOG}; missing: {missing}")


def main():
    steps = [s for s in os.environ.get("SHINOBI_LONDON_IMPORT", "materials,chunks,pieces,trees,props,catalog").split(",") if s]
    palette = materials() if "materials" in steps else palette_from_assets()
    if "tints" in steps:  # quick tweaks without rebuilding the parents (that recompiles every chunk's shaders)
        roof = palette["Roof_Flat"]  # grey bitumen felt rather than sandy concrete
        base = f"{lib.LIB}/Textures/asphalt_02"
        for param, suffix in (("Albedo", "A"), ("Normal", "N"), ("Roughness", "R")):
            MEL.set_material_instance_texture_parameter_value(roof, param, unreal.load_asset(f"{base}/T_asphalt_02_{suffix}"))
        MEL.set_material_instance_vector_parameter_value(roof, "Tint", unreal.LinearColor(0.75, 0.75, 0.78, 1))
        EAL.save_loaded_asset(roof)
    if "usage" in steps:
        usage()
    if "fallback" in steps:  # existing chunks: ground lossless (kerbs), buildings AUTO, without reimporting
        # Buildings must stay AUTO: a lossless fallback (~120k tris per chunk) makes every editor session rebuild the
        # mesh distance fields for ~6 min before Play, and the navmesh export of their collision huge. Their walls are
        # planar, so the decimated fallback collides the same.
        fixed = 0
        for kind in ("B",):
            for path in EAL.list_assets(f"{ROOT}/City/{kind}", recursive=True):
                mesh = unreal.load_asset(path.split(".")[0])
                if not isinstance(mesh, unreal.StaticMesh):
                    continue
                nanite = mesh.get_editor_property("nanite_settings")
                if nanite.get_editor_property("fallback_target") == unreal.NaniteFallbackTarget.AUTO:
                    continue
                nanite.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.AUTO)
                mesh.set_editor_property("nanite_settings", nanite)
                EAL.save_loaded_asset(mesh)
                fixed += 1
        log(f"fallback: {fixed} building meshes back to AUTO")
        fixed = 0
        for kind in ("G",):
            for path in EAL.list_assets(f"{ROOT}/City/{kind}", recursive=True):
                mesh = unreal.load_asset(path.split(".")[0])
                if not isinstance(mesh, unreal.StaticMesh):
                    continue
                nanite = mesh.get_editor_property("nanite_settings")
                if nanite.get_editor_property("fallback_target") == unreal.NaniteFallbackTarget.PERCENT_TRIANGLES:
                    continue
                nanite.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
                nanite.set_editor_property("fallback_percent_triangles", 1.0)
                mesh.set_editor_property("nanite_settings", nanite)
                EAL.save_loaded_asset(mesh)
                fixed += 1
                if fixed % 25 == 0:
                    log(f"fallback: {fixed} meshes")
        log(f"fallback: {fixed} ground meshes made lossless")
    if "glass" in steps:
        soft_glass()
    if "nodf" in steps:
        no_distance_fields()
    if "chunks" in steps:
        chunks(palette)
    if "pieces" in steps:
        catalog.pieces = pieces(palette)
    else:
        try:
            with open(CATALOG, encoding="utf-8") as f:
                old = json.load(f)
            catalog.pieces = {k: old[k] for k in ("london_eye", "tower_bridge", "westminster_bridge", "shard", "gherkin",
                                                  "walkie_talkie", "bt_tower", "piccadilly_lights", "belisha_beacon") if old.get(k)}
        except (OSError, ValueError):
            catalog.pieces = {}
    catalog.trees = trees() if "trees" in steps or "catalog" in steps else []
    props = import_props() if "props" in steps or "catalog" in steps else {}
    if "catalog" in steps:
        catalog(props)


if __name__ == "__main__":
    main()
