"""Chakra charge aura materials. Idempotent: rebuilds both materials from scratch each run.

M_ChakraAura (flame shells): upward-scrolling procedural noise flames, fresnel rim, soft top fade.
M_ChakraRing (ground ring): a ring pulsing outward plus a faint centre glow.
Both: unlit, additive, two-sided; parameters Color (vector), Intensity, Fade (scalars) driven by
UShinobiChakraAuraComponent.

Substrate routing (r.Substrate=True, confirmed against the 5.8 engine source):
UMaterialExpressionSubstrateUnlitBSDF's Emissive Color input is connected, then the BSDF node is wired
into the material's Front Material slot (unreal.MaterialProperty.MP_FRONT_MATERIAL). Unlit + additive
still uses MP_FrontMaterial exclusively in 5.8 (UMaterial::IsPropertyActive's Substrate branch for the
unlit shading model returns true only for MP_FrontMaterial), so no different route is needed.

Two pin names differ from the naive assumption, both confirmed against the engine source:
- UMaterialExpressionSubstrateUnlitBSDF does not override GetInputName(), so
  MaterialEditingLibrary.connect_material_expressions must target the raw FExpressionInput property
  name "EmissiveColor" (no space) -- not the doc-comment phrase "Emissive Color".
  UMaterialGraphNode::GetShortenPinName (which is what would otherwise turn a long pin name into a
  shorter display name, e.g. Coordinates -> UVs) only remaps a fixed list of special-cased names and
  EmissiveColor is not one of them, so the property name passes through unchanged
  (Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionSubstrate.h and
  Engine/Source/Editor/UnrealEd/Private/MaterialGraphNode.cpp).
- UMaterialExpressionNoise DOES override GetInputName() for its Position input: it returns
  GetWorldPositionInputName(WorldPositionOriginType), which for the default WorldPositionOriginType
  (Absolute) is "World Position" (with a space), not the property name "Position"
  (Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressionNoise.cpp and MaterialShared.cpp).
- Every single-input node whose only FExpressionInput property is literally named "Input" (Abs, Frac,
  Length, OneMinus, Saturate, ComponentMask, ...) can't be targeted by the string "Input" either:
  UMaterialGraphNode::GetShortenPinName explicitly maps the pin name "Input" to NAME_None, and
  MaterialEditingLibrary's internal GetExpressionInputByName treats a None/empty ToInputName as "return
  the expression's first input" rather than "no match". So the correct dst_input for these nodes is the
  empty string "" (which also happens to be simpler and can't typo against a property name).

All three were found by running the script and reading the raised "failed to connect" error against the
engine source, exactly per this task's requirement to raise rather than silently skip.
"""
import unreal

VFX_DIR = "/Game/Shinobi/VFX"
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def fresh_material(name):
    path = f"{VFX_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"could not delete {path} (is it open in the editor?)")
    material = TOOLS.create_asset(name, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"failed to create {path}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
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


def shared_params(material):
    color = node(material, unreal.MaterialExpressionVectorParameter, -1200, -300,
                 parameter_name="Color", default_value=unreal.LinearColor(0.15, 0.55, 1.0, 1.0))
    intensity = node(material, unreal.MaterialExpressionScalarParameter, -1200, -150,
                     parameter_name="Intensity", default_value=6.0)
    fade = node(material, unreal.MaterialExpressionScalarParameter, -1200, 0,
                parameter_name="Fade", default_value=1.0)
    # color * intensity * fade
    ci = node(material, unreal.MaterialExpressionMultiply, -1000, -250)
    link(color, ci, "A")
    link(intensity, ci, "B")
    cif = node(material, unreal.MaterialExpressionMultiply, -850, -200)
    link(ci, cif, "A")
    link(fade, cif, "B")
    return cif


def finish(material, emissive):
    """Substrate: emissive -> Unlit BSDF -> Front Material."""
    unlit = node(material, unreal.MaterialExpressionSubstrateUnlitBSDF, 200, 0)
    link(emissive, unlit, "EmissiveColor")
    if not MEL.connect_material_property(unlit, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError(f"failed to connect Front Material on {material.get_name()}")
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"{material.get_name()} failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"failed to save {material.get_name()}")
    unreal.log(f"[setup_vfx] built: {material.get_path_name()}")


def build_aura():
    m = fresh_material("M_ChakraAura")
    tint = shared_params(m)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1400, 250, u_tiling=4.0, v_tiling=1.5)
    pan = node(m, unreal.MaterialExpressionPanner, -1200, 250, speed_x=0.0, speed_y=-1.2)  # flames rise
    link(uv, pan, "Coordinate")
    time = node(m, unreal.MaterialExpressionTime, -1200, 400)
    slow = node(m, unreal.MaterialExpressionMultiply, -1050, 400, const_b=0.7)
    link(time, slow, "A")
    pos = node(m, unreal.MaterialExpressionAppendVector, -900, 300)
    link(pan, pos, "A")
    link(slow, pos, "B")
    noise = node(m, unreal.MaterialExpressionNoise, -750, 300, scale=3.0, levels=3, output_min=0.0, output_max=1.0)
    # MaterialExpressionNoise overrides GetInputName() to return GetWorldPositionInputName(WorldPositionOriginType)
    # for its Position input; the default WorldPositionOriginType (Absolute) yields "World Position", not the
    # property name "Position" (Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressionNoise.cpp and
    # MaterialShared.cpp's GetWorldPositionInputName). Confirmed the hard way: the naive "Position" name raised
    # "failed to connect ... -> MaterialExpressionNoise_0.Position" on the first run.
    link(pos, noise, "World Position")
    flame = node(m, unreal.MaterialExpressionPower, -600, 300, const_exponent=2.0)
    link(noise, flame, "Base")
    # Top fade: 1 at the bottom of the shell, 0 at the top. If the engine cylinder's V runs the other way,
    # the manual check (Task 5) flips this by swapping the OneMinus.
    v = node(m, unreal.MaterialExpressionComponentMask, -1200, 550, r=False, g=True, b=False, a=False)
    raw_uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1400, 550)
    link(raw_uv, v, "")
    top_fade = node(m, unreal.MaterialExpressionOneMinus, -1050, 550)
    link(v, top_fade, "")
    fresnel = node(m, unreal.MaterialExpressionFresnel, -750, 550, exponent=2.5)
    rim = node(m, unreal.MaterialExpressionAdd, -600, 550, const_b=0.35)
    link(fresnel, rim, "A")
    shape = node(m, unreal.MaterialExpressionMultiply, -450, 350)
    link(flame, shape, "A")
    link(top_fade, shape, "B")
    shaped = node(m, unreal.MaterialExpressionMultiply, -300, 400)
    link(shape, shaped, "A")
    link(rim, shaped, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -100, 0)
    link(tint, emissive, "A")
    link(shaped, emissive, "B")
    finish(m, emissive)


def build_ring():
    m = fresh_material("M_ChakraRing")
    tint = shared_params(m)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1400, 250)
    centred = node(m, unreal.MaterialExpressionSubtract, -1250, 250, const_b=0.5)
    link(uv, centred, "A")
    dist = node(m, unreal.MaterialExpressionLength, -1100, 250)
    link(centred, dist, "")
    radius = node(m, unreal.MaterialExpressionMultiply, -950, 250, const_b=2.0)  # 0 centre .. 1 edge
    link(dist, radius, "A")
    time = node(m, unreal.MaterialExpressionTime, -1250, 450)
    rate = node(m, unreal.MaterialExpressionMultiply, -1100, 450, const_b=1.2)
    link(time, rate, "A")
    pulse = node(m, unreal.MaterialExpressionFrac, -950, 450)  # 0..1 expanding
    link(rate, pulse, "")
    gap = node(m, unreal.MaterialExpressionSubtract, -800, 350)
    link(radius, gap, "A")
    link(pulse, gap, "B")
    gap_abs = node(m, unreal.MaterialExpressionAbs, -650, 350)
    link(gap, gap_abs, "")
    band = node(m, unreal.MaterialExpressionMultiply, -500, 350, const_b=8.0)  # ring width ~1/8
    link(gap_abs, band, "A")
    ring = node(m, unreal.MaterialExpressionOneMinus, -350, 350)
    link(band, ring, "")
    ring_sat = node(m, unreal.MaterialExpressionSaturate, -200, 350)
    link(ring, ring_sat, "")
    edge = node(m, unreal.MaterialExpressionOneMinus, -800, 550)  # fade toward the plane edge
    link(radius, edge, "")
    edge_sat = node(m, unreal.MaterialExpressionSaturate, -650, 550)
    link(edge, edge_sat, "")
    glow = node(m, unreal.MaterialExpressionMultiply, -500, 550, const_b=0.25)
    link(edge_sat, glow, "A")
    body = node(m, unreal.MaterialExpressionAdd, -350, 500)
    link(ring_sat, body, "A")
    link(glow, body, "B")
    shaped = node(m, unreal.MaterialExpressionMultiply, -200, 500)
    link(body, shaped, "A")
    link(edge_sat, shaped, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -100, 0)
    link(tint, emissive, "A")
    link(shaped, emissive, "B")
    finish(m, emissive)


build_aura()
build_ring()
