"""Earth Wall / Water Wall meshes and materials for AShinobiBarrier. Idempotent; close the GUI editor first.

  powershell -ExecutionPolicy Bypass -File Scripts\\run_python.ps1 -Script Scripts\\setup_walls.py

Sources: SourceArt/World/Walls/<EarthWall|WaterWall>/<name>.fbx, made from Rodin GLBs (SourceArt/World/Walls/Rodin) by
Scripts/world/prep_wall.py (EarthWall = EarthB.glb 480 320 150; WaterWall = WaterA.glb 480 320 90 flip).

- SM_EarthWall wears M_EarthWall: lit Substrate, the world library's Poly Haven rock_face albedo + roughness projected
  triplanar in the mesh's own space (Rodin's UV texture is soft and stylised; local space so the rock doesn't swim as the
  wall rises), darkened by Rodin's own texture in grey (its baked crevices), over Rodin's normal map (the carved pillars).
- SM_WaterWall wears M_WaterCurtain: unlit translucent after DOF like M_WaterWall (the toon post-process would flatten
  it), with streaks racing up the mesh, foam whitening toward the ragged crest, and a fresnel rim.
Output: /Game/Shinobi/VFX/Walls.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = os.path.join(ROOT, "SourceArt", "World", "Walls")
DEST = "/Game/Shinobi/VFX/Walls"
ROCK = "/Game/Shinobi/World/Library/Textures/rock_face"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(src, dst, dst_input, src_output=""):
    if not MEL.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")


def op(material, cls, a, b, x, y, a_out="", b_out=""):
    """Two-input math node (Multiply/Add/Subtract/Divide/Append/Dot) on a and b; b may be a float constant."""
    n = node(material, cls, x, y)
    link(a, n, "A", a_out)
    if isinstance(b, (int, float)):
        n.set_editor_property("const_b", float(b))
    else:
        link(b, n, "B", b_out)
    return n


def mask(material, src, channels, x, y):
    n = node(material, unreal.MaterialExpressionComponentMask, x, y, **{c: c in channels for c in "rgba"})
    link(src, n, "")
    return n


def fresh_material(name):
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path) and not EAL.delete_asset(path):
        raise RuntimeError(f"could not delete {path} (is it open in the editor?)")
    return TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def save(material):
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"{material.get_name()} failed to compile: {errors}")
    EAL.save_loaded_asset(material)
    unreal.log(f"[setup_walls] built: {material.get_path_name()}")


def import_texture(png, name, normal=False):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = png, DEST, name
    task.automated = task.save = task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(f"{DEST}/{name}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import {png}")
    if normal:
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", True)  # glTF normals are green-up, Unreal's green-down
        EAL.save_loaded_asset(texture)
    return texture


def import_mesh(name):
    fbx = os.path.join(SRC, name, f"{name}.fbx")
    if not os.path.exists(fbx):
        raise RuntimeError(f"{fbx} missing: run Scripts/world/prep_wall.py")
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = fbx, DEST, f"SM_{name}"
    task.automated = task.save = task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{DEST}/SM_{name}")
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"could not import {fbx}")
    return mesh


def build_earth(normal_tex, base_tex):
    """Triplanar rock in local space: weights = |local normal|^4, normalised; each plane samples LocalPosition / Scale."""
    m = fresh_material("M_EarthWall")
    albedo_tex, rough_tex = unreal.load_asset(f"{ROCK}/T_rock_face_A"), unreal.load_asset(f"{ROCK}/T_rock_face_R")
    if not albedo_tex or not rough_tex:
        raise RuntimeError(f"rock_face textures missing under {ROCK} (Scripts/world/import_library.py)")
    scale = node(m, unreal.MaterialExpressionScalarParameter, -2000, 0, parameter_name="Scale", default_value=220.0)
    pos = op(m, unreal.MaterialExpressionDivide, node(m, unreal.MaterialExpressionLocalPosition, -2000, -150), scale, -1800, -100)
    to_local = node(m, unreal.MaterialExpressionTransform, -2000, 300,
                    transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                    transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(node(m, unreal.MaterialExpressionVertexNormalWS, -2200, 300), to_local, "")
    n_abs = node(m, unreal.MaterialExpressionAbs, -1800, 300)
    link(to_local, n_abs, "")
    n2 = op(m, unreal.MaterialExpressionMultiply, n_abs, n_abs, -1650, 300)
    n4 = op(m, unreal.MaterialExpressionMultiply, n2, n2, -1500, 300)
    total = op(m, unreal.MaterialExpressionDotProduct, n4,
               node(m, unreal.MaterialExpressionConstant3Vector, -1650, 450, constant=unreal.LinearColor(1, 1, 1, 0)), -1350, 400)
    weights = op(m, unreal.MaterialExpressionDivide, n4, total, -1200, 300)
    planes = [mask(m, pos, "gb", -1500, -300), mask(m, pos, "rb", -1500, -150), mask(m, pos, "rg", -1500, 0)]  # seen along X, Y, Z
    w = [mask(m, weights, c, -1050, 300 + i * 100) for i, c in enumerate("rgb")]

    def triplanar(texture, sampler, out, y):
        result = None
        for i, uv in enumerate(planes):
            sample = node(m, unreal.MaterialExpressionTextureSample, -1000, y + i * 220, texture=texture, sampler_type=sampler)
            link(uv, sample, "UVs")
            weighted = op(m, unreal.MaterialExpressionMultiply, sample, w[i], -750, y + i * 220, a_out=out)
            result = weighted if result is None else op(m, unreal.MaterialExpressionAdd, result, weighted, -600, y + i * 220)
        return result

    albedo = triplanar(albedo_tex, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", -900)
    rough = triplanar(rough_tex, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", -200)
    tint = node(m, unreal.MaterialExpressionVectorParameter, -600, -1100, parameter_name="Tint", default_value=unreal.LinearColor(0.78, 0.7, 0.6, 1))
    # Rodin's own texture, grey: its baked crevices and grime darken the rock (moss and all reduced to shading).
    baked = node(m, unreal.MaterialExpressionTextureSample, -900, -1300, texture=base_tex)
    grey = node(m, unreal.MaterialExpressionDesaturation, -650, -1300)
    link(baked, grey, "", "RGB")
    shading = op(m, unreal.MaterialExpressionMultiply, grey, 1.9, -500, -1300)
    normal = node(m, unreal.MaterialExpressionTextureSample, -600, 600, texture=normal_tex,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    slab = node(m, unreal.MaterialExpressionSubstrateShadingModels, -150, 0)
    link(op(m, unreal.MaterialExpressionMultiply, op(m, unreal.MaterialExpressionMultiply, albedo, tint, -400, -900), shading, -300, -1000), slab, "BaseColor")
    link(rough, slab, "Roughness")
    link(normal, slab, "Normal", "RGB")
    MEL.connect_material_property(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    save(m)
    return m


def build_water():
    """Unlit translucent water: emissive = Color * Intensity * (0.45 + 0.6 streaks + 0.7 rim) lerped to foam white by
    height^8 * crest noise (the top fifth); coverage likewise higher at the rim and crest. Height is LocalPosition.z / 320 (the mesh's)."""
    m = fresh_material("M_WaterCurtain")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    color = node(m, unreal.MaterialExpressionVectorParameter, -1800, -500, parameter_name="Color", default_value=unreal.LinearColor(0.02, 0.16, 0.55, 1))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -1800, -350, parameter_name="Intensity", default_value=1.0)
    local = node(m, unreal.MaterialExpressionLocalPosition, -1800, 0)
    time = node(m, unreal.MaterialExpressionTime, -1800, 150)

    def flow(stretch, speed, scale, y, levels):
        """Noise on LocalPosition scaled by stretch (xyz) and scrolled up by speed units/s."""
        up = op(m, unreal.MaterialExpressionAppendVector,
                node(m, unreal.MaterialExpressionConstant2Vector, -1600, y + 80, r=0.0, g=0.0),
                op(m, unreal.MaterialExpressionMultiply, time, speed, -1600, y + 160), -1450, y + 100)
        moved = op(m, unreal.MaterialExpressionSubtract, local, up, -1300, y)
        stretched = op(m, unreal.MaterialExpressionMultiply, moved,
                       node(m, unreal.MaterialExpressionConstant3Vector, -1300, y + 120, constant=unreal.LinearColor(*stretch, 0)), -1150, y)
        noise = node(m, unreal.MaterialExpressionNoise, -1000, y, scale=scale, levels=levels, output_min=0.0, output_max=1.0)
        link(stretched, noise, "World Position")  # the pin keeps its old name
        return noise

    streaks = flow((0.06, 0.06, 0.008), 260.0, 1.0, 0, 3)  # long vertical streaks racing up
    churn = flow((0.03, 0.03, 0.03), 120.0, 1.0, 400, 2)    # slower rolling body
    rim = node(m, unreal.MaterialExpressionFresnel, -1000, -250, exponent=2.5, base_reflect_fraction=0.0)
    height = node(m, unreal.MaterialExpressionSaturate, -1000, 800)
    link(op(m, unreal.MaterialExpressionDivide, mask(m, local, "b", -1300, 800), 320.0, -1150, 800), height, "")
    h2 = op(m, unreal.MaterialExpressionMultiply, height, height, -850, 800)
    h4 = op(m, unreal.MaterialExpressionMultiply, h2, h2, -800, 850)
    h8 = op(m, unreal.MaterialExpressionMultiply, h4, h4, -750, 900)
    foam = node(m, unreal.MaterialExpressionSaturate, -550, 800)
    link(op(m, unreal.MaterialExpressionMultiply, op(m, unreal.MaterialExpressionMultiply, h8, churn, -700, 750), 2.0, -650, 800), foam, "")

    glow = op(m, unreal.MaterialExpressionAdd,
              op(m, unreal.MaterialExpressionAdd, op(m, unreal.MaterialExpressionMultiply, streaks, 0.6, -750, 0),
                 op(m, unreal.MaterialExpressionMultiply, rim, 0.7, -750, -250), -600, -100), 0.45, -450, -100)
    water = op(m, unreal.MaterialExpressionMultiply, op(m, unreal.MaterialExpressionMultiply, color, intensity, -1500, -450), glow, -300, -300)
    emissive = node(m, unreal.MaterialExpressionLinearInterpolate, -100, -200)
    link(water, emissive, "A")
    link(node(m, unreal.MaterialExpressionConstant3Vector, -300, -100, constant=unreal.LinearColor(1.2, 1.35, 1.5, 0)), emissive, "B")
    link(foam, emissive, "Alpha")

    coverage = node(m, unreal.MaterialExpressionSaturate, -100, 300)
    link(op(m, unreal.MaterialExpressionAdd,
            op(m, unreal.MaterialExpressionAdd, op(m, unreal.MaterialExpressionMultiply, churn, 0.3, -600, 400),
               op(m, unreal.MaterialExpressionMultiply, rim, 0.25, -600, 300), -450, 350),
            op(m, unreal.MaterialExpressionAdd, foam, 0.3, -450, 500), -300, 400), coverage, "")
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 100, 300)
    link(coverage, transmittance, "")
    unlit = node(m, unreal.MaterialExpressionSubstrateUnlitBSDF, 300, 0)
    link(emissive, unlit, "EmissiveColor")
    link(transmittance, unlit, "TransmittanceColor")
    MEL.connect_material_property(unlit, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)
    save(m)
    return m


earth = import_mesh("EarthWall")
earth.set_material(0, build_earth(import_texture(os.path.join(SRC, "EarthWall", "EarthWall_Normal.png"), "T_EarthWall_N", normal=True),
                                  import_texture(os.path.join(SRC, "EarthWall", "EarthWall_BaseColor.png"), "T_EarthWall_D")))
EAL.save_loaded_asset(earth)
water = import_mesh("WaterWall")
water.set_material(0, build_water())
EAL.save_loaded_asset(water)
unreal.log(f"[setup_walls] meshes: {earth.get_path_name()}, {water.get_path_name()}")
