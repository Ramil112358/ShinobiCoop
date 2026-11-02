"""Ice Meteor hail assets from SourceArt/Fab/IceHail (made by Scripts/prep_ice_hail.py). Idempotent; close the GUI editor.
Run with Scripts/run_python.ps1.

  ice_shatter_sheet.png -> /Game/Shinobi/VFX/T_IceShatter  4 x 2 atlas for M_Sprite (ESprite::IceShatter, FrostCrack)
  ice_surface.png       -> /Game/Shinobi/VFX/T_IceSurface  frosted ice, tiled over the shards
  ice_shard_0..2.obj    -> /Game/Shinobi/VFX/SM_IceShard_0..2 (box collision: they are also the physics chips of a landing)
  ice_trail.obj         -> /Game/Shinobi/VFX/SM_IceTrail

Materials:
  M_IceShard  lit and opaque: deep blue ice clouded white where the frost texture is bright, glossy, with a faint cold rim
              (light caught in the edges). Params: Deep, Frost, Rim.
  M_IceTrail  additive, unlit, after DOF: a soft white-blue streak, brightest at the shard (U 0) and gone at the far end,
              faded at its silhouette so the tube never shows an edge. Params: Color, Intensity.
"""
import os
import unreal

VFX_DIR = "/Game/Shinobi/VFX"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "SourceArt", "Fab", "IceHail")
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MESHES = unreal.EditorStaticMeshLibrary  # deprecated, but StaticMeshEditorSubsystem is None under the commandlet


def log(message):
    unreal.log(f"[setup_ice_hail] {message}")


def import_file(filename, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, filename)
    task.destination_path = VFX_DIR
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(f"{VFX_DIR}/{name}")
    if asset is None:
        raise RuntimeError(f"import of {filename} produced no {name}")
    return asset


def import_texture(filename, name, clamp):
    texture = import_file(filename, name)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS if clamp else unreal.TextureGroup.TEXTUREGROUP_WORLD)
    address = unreal.TextureAddress.TA_CLAMP if clamp else unreal.TextureAddress.TA_WRAP
    texture.set_editor_property("address_x", address)
    texture.set_editor_property("address_y", address)
    EAL.save_loaded_asset(texture)
    log(f"{filename} -> {texture.get_path_name()}")
    return texture


def import_mesh(filename, name, collision):
    mesh = import_file(filename, name)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    MESHES.remove_collisions(mesh)
    if collision:
        MESHES.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    EAL.save_loaded_asset(mesh)
    box = mesh.get_bounding_box()
    log(f"{filename} -> {mesh.get_path_name()} tris={mesh.get_num_triangles(0)} bounds={box.min} {box.max}")
    return mesh


def fresh_material(name, blend, shading):
    path = f"{VFX_DIR}/{name}"
    if EAL.does_asset_exist(path) and not EAL.delete_asset(path):
        raise RuntimeError(f"could not delete {path} (is it open in the editor?)")
    material = TOOLS.create_asset(name, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", blend)
    material.set_editor_property("shading_model", shading)
    return material


def build(m, front):
    if not MEL.connect_material_property(front, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError(f"{m.get_name()}: could not connect Front Material")
    errors = MEL.recompile_material(m)
    if errors:
        raise RuntimeError(f"{m.get_name()} failed to compile: {errors}")
    EAL.save_loaded_asset(m)
    log(f"built: {m.get_path_name()}")


def graph(m):
    def node(cls, x, y, **props):
        expression = MEL.create_material_expression(m, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(src, dst, dst_input, src_output=""):
        if not MEL.connect_material_expressions(src, src_output, dst, dst_input):
            raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")

    def op(cls, a, b=None, x=0, y=0, **k):
        out = node(cls, x, y, **k)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    return node, link, op


def build_ice_shard(surface):
    m = fresh_material("M_IceShard", unreal.BlendMode.BLEND_OPAQUE, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    node, link, op = graph(m)
    deep = node(unreal.MaterialExpressionVectorParameter, -1200, -400, parameter_name="Deep", default_value=unreal.LinearColor(0.05, 0.22, 0.42, 1))
    frost = node(unreal.MaterialExpressionVectorParameter, -1200, -250, parameter_name="Frost", default_value=unreal.LinearColor(0.75, 0.88, 1.0, 1))
    rim_gain = node(unreal.MaterialExpressionScalarParameter, -1200, -100, parameter_name="Rim", default_value=0.6)
    uv = op(unreal.MaterialExpressionMultiply, node(unreal.MaterialExpressionTextureCoordinate, -1400, 100), None, -1250, 100, const_b=1.5)
    texture = node(unreal.MaterialExpressionTextureSample, -1100, 100, texture=surface)
    link(uv, texture, "UVs")
    lum = node(unreal.MaterialExpressionDotProduct, -900, 100)
    link(texture, lum, "A", "RGB")
    link(node(unreal.MaterialExpressionConstant3Vector, -1100, 300, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1)), lum, "B")
    # Frost: the texture's brightest third clouds the ice white; the rest stays clear deep blue.
    cloud = node(unreal.MaterialExpressionSaturate, -600, 100)
    link(op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionSubtract, lum, None, -800, 100, const_b=0.35), None, -700, 100, const_b=2.5), cloud, "")
    base = node(unreal.MaterialExpressionLinearInterpolate, -400, -300)
    link(deep, base, "A")
    link(frost, base, "B")
    link(cloud, base, "Alpha")
    roughness = node(unreal.MaterialExpressionLinearInterpolate, -400, 0, const_a=0.04, const_b=0.3)
    link(cloud, roughness, "Alpha")
    # Rim: light caught in the edges, in the frost colour (Rim x fresnel^3).
    rim = op(unreal.MaterialExpressionMultiply, node(unreal.MaterialExpressionFresnel, -800, -100, exponent=3.0, base_reflect_fraction=0.0), rim_gain, -600, -100)
    emissive = op(unreal.MaterialExpressionMultiply, frost, rim, -400, -150)
    shading = node(unreal.MaterialExpressionSubstrateShadingModels, 0, 0)
    for value, names in ((base, ("BaseColor", "Base Color")), (roughness, ("Roughness",)), (emissive, ("EmissiveColor", "Emissive Color"))):
        if not any(MEL.connect_material_expressions(value, "", shading, n) for n in names):
            raise RuntimeError(f"M_IceShard: no {names[0]} input")
    if not MEL.connect_material_expressions(node(unreal.MaterialExpressionConstant, -200, 200, r=0.8), "", shading, "Specular"):
        raise RuntimeError("M_IceShard: no Specular input")
    build(m, shading)


def build_ice_trail():
    m = fresh_material("M_IceTrail", unreal.BlendMode.BLEND_ADDITIVE, unreal.MaterialShadingModel.MSM_UNLIT)
    # After DOF: composited after the toon post-process like every other jutsu glow (see setup_vfx.py).
    m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    node, link, op = graph(m)
    color = node(unreal.MaterialExpressionVectorParameter, -900, -300, parameter_name="Color", default_value=unreal.LinearColor(0.7, 0.88, 1.0, 1))
    intensity = node(unreal.MaterialExpressionScalarParameter, -900, -150, parameter_name="Intensity", default_value=0.35)
    along = node(unreal.MaterialExpressionComponentMask, -900, 50, r=True, g=False, b=False, a=False)
    link(node(unreal.MaterialExpressionTextureCoordinate, -1100, 50), along, "")
    remaining = node(unreal.MaterialExpressionOneMinus, -750, 50)
    link(along, remaining, "")
    tail = node(unreal.MaterialExpressionPower, -600, 50, const_exponent=3.0)
    link(remaining, tail, "Base")
    facing = node(unreal.MaterialExpressionOneMinus, -600, 200)
    link(node(unreal.MaterialExpressionFresnel, -750, 200, exponent=1.0, base_reflect_fraction=0.0), facing, "")
    emissive = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, color, intensity, -600, -250),
                  op(unreal.MaterialExpressionMultiply, tail, facing, -400, 100), -200, 0)
    unlit = node(unreal.MaterialExpressionSubstrateUnlitBSDF, 0, 0)
    link(emissive, unlit, "EmissiveColor")
    build(m, unlit)


textures = {
    "T_IceShatter": import_texture("ice_shatter_sheet.png", "T_IceShatter", clamp=True),
    "T_IceSurface": import_texture("ice_surface.png", "T_IceSurface", clamp=False),
}
for index in range(3):
    import_mesh(f"ice_shard_{index}.obj", f"SM_IceShard_{index}", collision=True)
import_mesh("ice_trail.obj", "SM_IceTrail", collision=False)
build_ice_shard(textures["T_IceSurface"])
build_ice_trail()
for index in range(3):
    mesh = unreal.load_asset(f"{VFX_DIR}/SM_IceShard_{index}")
    mesh.set_material(0, unreal.load_asset(f"{VFX_DIR}/M_IceShard"))
    EAL.save_loaded_asset(mesh)
trail = unreal.load_asset(f"{VFX_DIR}/SM_IceTrail")
trail.set_material(0, unreal.load_asset(f"{VFX_DIR}/M_IceTrail"))
EAL.save_loaded_asset(trail)
