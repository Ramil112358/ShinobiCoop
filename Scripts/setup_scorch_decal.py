"""M_SpriteScorchDecal: the scorch / dent / crater / crack marks as a deferred decal, so a mark follows the surface it lands on
(floor, slope, wall) and stops where the geometry stops, instead of a flat quad hanging in the air past a kerb or a wall's edge.
Same look and parameters as M_SpriteScorch (Atlas, Tint, Start, Linger; fades itself in over 0.4 s and out over the last 3 s from the
world clock): unlit-looking (the colour is Emissive, the base is black) with the sheet's alpha as opacity. Idempotent; close the GUI editor.
Run with Scripts/run_python.ps1.
"""
import unreal

VFX_DIR = "/Game/Shinobi/VFX"
NAME = "M_SpriteScorchDecal"
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


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


path = f"{VFX_DIR}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
    raise RuntimeError(f"could not delete {path} (is it open in the editor?)")
m = TOOLS.create_asset(NAME, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

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

black = node(m, unreal.MaterialExpressionConstant3Vector, 200, -300, constant=unreal.LinearColor(0, 0, 0, 1))
rough = node(m, unreal.MaterialExpressionConstant, 200, -400, r=1.0)
for expression, prop in ((emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR), (opacity, unreal.MaterialProperty.MP_OPACITY),
                         (black, unreal.MaterialProperty.MP_BASE_COLOR), (rough, unreal.MaterialProperty.MP_ROUGHNESS)):
    if not MEL.connect_material_property(expression, "", prop):
        raise RuntimeError(f"failed to connect {prop}")
errors = MEL.recompile_material(m)
if errors:
    raise RuntimeError(f"{NAME} failed to compile: {errors}")
if not unreal.EditorAssetLibrary.save_loaded_asset(m):
    raise RuntimeError(f"failed to save {NAME}")
unreal.log(f"built: {m.get_path_name()}")
