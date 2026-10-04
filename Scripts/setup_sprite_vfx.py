"""SpriteCook VFX sheets -> textures and sprite materials. Idempotent; close the GUI editor.

Sources (SourceArt/SpriteCook/VFX/, 1024 px, each a 4 x 2 atlas of 256 x 512 cells on pure black):
  flames_sheet.png        -> /Game/Shinobi/VFX/T_Flames        8 realistic fire tongues
  smoke_embers_sheet.png  -> /Game/Shinobi/VFX/T_SmokeEmbers   top row 4 smoke plumes, bottom row 4 ember sprays
  energy_sheet.png        -> /Game/Shinobi/VFX/T_Energy        top row 4 lightning bolts; bottom: ring, vortex, slash, burst
  scorch_rgba.png         -> /Game/Shinobi/VFX/T_Scorch        a charred ground mark (alpha cut from scorch.png, which was on white)
  tairn_fire_sheet.png    -> /Game/Shinobi/VFX/T_TairnFire     8 tall blue-white lightning-fire jets (ESprite::TairnFire)
  crimson_fire_sheet.png  -> /Game/Shinobi/VFX/T_CrimsonFire   8 tall orange dragon-fire jets with embers (ESprite::CrimsonFire)

Materials (params: Atlas, CellU 0-3, CellV 0-1, Tint, Intensity, Fade):
  M_Sprite       additive, unlit: fire, embers, lightning. Black is "nothing", so the sheets need no alpha.
  M_SpriteDark   translucent: Amaterasu's black flame. The flame's luminance is coverage; only its dim outer edge glows in Tint.
  M_SpriteSmoke  translucent, Tint is the smoke colour; the sheet's luminance is coverage.
  M_SpriteScorch translucent ground mark that fades itself in and out from the world clock (Start, Linger, FadeIn, FadeOut),
                 so it can outlive the actor that spawned it. Params: Tint, Start, Linger.
"""
import os
import unreal

VFX_DIR = "/Game/Shinobi/VFX"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "SpriteCook", "VFX")
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SHEETS = {
    "flames_sheet.png": "T_Flames",
    "smoke_embers_sheet.png": "T_SmokeEmbers",
    "energy_sheet.png": "T_Energy",
    "scorch_rgba.png": "T_Scorch",
    "tairn_fire_sheet.png": "T_TairnFire",      # 8 tall jets of blue-white lightning fire (Tairn's breath)
    "crimson_fire_sheet.png": "T_CrimsonFire",  # 8 tall jets of orange dragon fire with embers (the crimson dragon's breath)
}


def log(message):
    unreal.log(f"[setup_sprite_vfx] {message}")


def import_sheet(png, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, png)
    task.destination_path = VFX_DIR
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
    if texture is None:
        raise RuntimeError(f"import of {png} produced no texture")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    log(f"{png} -> {texture.get_path_name()}")
    return texture


def remove_if_exists(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
        raise RuntimeError(f"could not delete {path} (is it open in the editor?)")


def fresh_material(name, blend):
    remove_if_exists(f"{VFX_DIR}/{name}")
    material = TOOLS.create_asset(name, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", blend)
    # After DOF: composited after the toon post-process, as for every other jutsu effect (see setup_vfx.py).
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    return material


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(src, dst, dst_input, src_output=""):
    if not MEL.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")


def mul(m, a, b, x, y):
    out = node(m, unreal.MaterialExpressionMultiply, x, y)
    link(a, out, "A")
    link(b, out, "B")
    return out


def finish(material, emissive, transmittance=None):
    unlit = node(material, unreal.MaterialExpressionSubstrateUnlitBSDF, 900, 0)
    link(emissive, unlit, "EmissiveColor")
    if transmittance is not None:
        link(transmittance, unlit, "TransmittanceColor")
    if not MEL.connect_material_property(unlit, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError(f"failed to connect Front Material on {material.get_name()}")
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"{material.get_name()} failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"failed to save {material.get_name()}")
    log(f"built: {material.get_path_name()}")


def atlas_sample(m, default_texture):
    """Atlas sample at this cell: UV = TexCoord * (1/4, 1/2) + (CellU/4, CellV/2). Returns (sample, fade, tint, intensity)."""
    atlas = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0, parameter_name="Atlas", texture=default_texture)
    cell_u = node(m, unreal.MaterialExpressionScalarParameter, -1300, 300, parameter_name="CellU", default_value=0.0)
    cell_v = node(m, unreal.MaterialExpressionScalarParameter, -1300, 450, parameter_name="CellV", default_value=0.0)
    quarter = node(m, unreal.MaterialExpressionConstant, -1300, 600, r=0.25)
    half = node(m, unreal.MaterialExpressionConstant, -1300, 700, r=0.5)
    off_u = mul(m, cell_u, quarter, -1100, 300)
    off_v = mul(m, cell_v, half, -1100, 450)
    offset = node(m, unreal.MaterialExpressionAppendVector, -900, 380)
    link(off_u, offset, "A")
    link(off_v, offset, "B")
    scale = node(m, unreal.MaterialExpressionConstant2Vector, -900, 200, r=0.25, g=0.5)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, 100)
    scaled = mul(m, uv, scale, -700, 150)
    shifted = node(m, unreal.MaterialExpressionAdd, -500, 250)
    link(scaled, shifted, "A")
    link(offset, shifted, "B")
    link(shifted, atlas, "UVs")
    fade = node(m, unreal.MaterialExpressionScalarParameter, -300, 300, parameter_name="Fade", default_value=1.0)
    return atlas, fade


def luminance(m, atlas, x, y):
    weights = node(m, unreal.MaterialExpressionConstant3Vector, x, y + 100, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1.0))
    lum = node(m, unreal.MaterialExpressionDotProduct, x + 150, y)
    link(atlas, lum, "A", "RGB")
    link(weights, lum, "B")
    return lum


def build_additive(textures):
    m = fresh_material("M_Sprite", unreal.BlendMode.BLEND_ADDITIVE)
    atlas, fade = atlas_sample(m, textures["T_Flames"])
    tint = node(m, unreal.MaterialExpressionVectorParameter, 0, -250, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, 0, -100, parameter_name="Intensity", default_value=2.0)
    ti = mul(m, tint, intensity, 200, -200)
    tif = mul(m, ti, fade, 350, -100)
    emissive = node(m, unreal.MaterialExpressionMultiply, 550, 0)
    link(atlas, emissive, "A", "RGB")
    link(tif, emissive, "B")
    finish(m, emissive)


def build_dark(textures):
    """Black flame: the sheet's luminance is coverage, the dim outer edge picks up Tint, the body stays near black."""
    m = fresh_material("M_SpriteDark", unreal.BlendMode.BLEND_TRANSLUCENT)
    atlas, fade = atlas_sample(m, textures["T_Flames"])
    tint = node(m, unreal.MaterialExpressionVectorParameter, 0, -300, parameter_name="Tint", default_value=unreal.LinearColor(0.35, 0.02, 0.6, 1))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, 0, -150, parameter_name="Intensity", default_value=6.0)
    lum = luminance(m, atlas, 100, 250)
    gain = node(m, unreal.MaterialExpressionMultiply, 350, 250, const_b=5.0)
    link(lum, gain, "A")
    sat = node(m, unreal.MaterialExpressionSaturate, 500, 250)
    link(gain, sat, "")
    coverage = mul(m, sat, fade, 650, 250)
    # Amaterasu: black body, Tint only on the thin outer edge (low luminance) where the flame fades out.
    scaled_lum = node(m, unreal.MaterialExpressionMultiply, 350, 100, const_b=5.0)
    link(lum, scaled_lum, "A")
    inverse = node(m, unreal.MaterialExpressionOneMinus, 450, 100)
    link(scaled_lum, inverse, "")
    rim = node(m, unreal.MaterialExpressionSaturate, 550, 100)
    link(inverse, rim, "")
    glow = mul(m, tint, intensity, 200, -250)
    rim_glow = mul(m, glow, rim, 500, -100)
    core = node(m, unreal.MaterialExpressionConstant3Vector, 500, -300, constant=unreal.LinearColor(0.008, 0.0, 0.012, 1.0))
    body = node(m, unreal.MaterialExpressionAdd, 700, -200)
    link(rim_glow, body, "A")
    link(core, body, "B")
    emissive = mul(m, body, coverage, 850, -100)
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 850, 150)
    link(coverage, transmittance, "")
    finish(m, emissive, transmittance)


def build_smoke(textures):
    m = fresh_material("M_SpriteSmoke", unreal.BlendMode.BLEND_TRANSLUCENT)
    atlas, fade = atlas_sample(m, textures["T_SmokeEmbers"])
    tint = node(m, unreal.MaterialExpressionVectorParameter, 0, -250, parameter_name="Tint", default_value=unreal.LinearColor(0.07, 0.07, 0.08, 1))
    gain_param = node(m, unreal.MaterialExpressionScalarParameter, 0, -100, parameter_name="Gain", default_value=4.0)
    lum = luminance(m, atlas, 100, 250)
    gained = mul(m, lum, gain_param, 350, 250)
    sat = node(m, unreal.MaterialExpressionSaturate, 500, 250)
    link(gained, sat, "")
    coverage = mul(m, sat, fade, 650, 250)
    emissive = mul(m, tint, coverage, 800, -100)
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 800, 150)
    link(coverage, transmittance, "")
    finish(m, emissive, transmittance)


def build_scorch():
    m = fresh_material("M_SpriteScorch", unreal.BlendMode.BLEND_TRANSLUCENT)
    tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0, parameter_name="Atlas", texture=unreal.load_asset(f"{VFX_DIR}/T_Scorch"))
    tint = node(m, unreal.MaterialExpressionVectorParameter, -300, -250, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    start = node(m, unreal.MaterialExpressionScalarParameter, -900, 300, parameter_name="Start", default_value=0.0)
    linger = node(m, unreal.MaterialExpressionScalarParameter, -900, 450, parameter_name="Linger", default_value=10.0)
    time = node(m, unreal.MaterialExpressionTime, -900, 150)
    age = node(m, unreal.MaterialExpressionSubtract, -700, 200)
    link(time, age, "A")
    link(start, age, "B")
    # fade in over 0.4 s: saturate(age / 0.4)
    fade_in = node(m, unreal.MaterialExpressionSaturate, -300, 250)
    scaled_in = node(m, unreal.MaterialExpressionMultiply, -500, 250, const_b=2.5)
    link(age, scaled_in, "A")
    link(scaled_in, fade_in, "")
    # fade out over the last 3 s: saturate((Linger - age) / 3)
    left = node(m, unreal.MaterialExpressionSubtract, -700, 400)
    link(linger, left, "A")
    link(age, left, "B")
    scaled_out = node(m, unreal.MaterialExpressionMultiply, -500, 400, const_b=0.3333)
    link(left, scaled_out, "A")
    fade_out = node(m, unreal.MaterialExpressionSaturate, -300, 400)
    link(scaled_out, fade_out, "")
    fades = mul(m, fade_in, fade_out, 0, 300)
    mark = node(m, unreal.MaterialExpressionMultiply, 0, 150, const_b=0.9)
    link(tex, mark, "A", "A")
    opacity = mul(m, mark, fades, 200, 250)
    emissive = node(m, unreal.MaterialExpressionMultiply, 200, -100)
    link(tex, emissive, "A", "RGB")
    link(tint, emissive, "B")
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 400, 250)
    link(opacity, transmittance, "")
    finish(m, emissive, transmittance)


textures = {name: import_sheet(png, name) for png, name in SHEETS.items()}
build_additive(textures)
build_dark(textures)
build_smoke(textures)
build_scorch()
