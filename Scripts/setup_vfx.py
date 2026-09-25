"""Chakra charge aura material. Idempotent: rebuilds M_ChakraBodyAura from scratch each run.

Close the GUI editor before running: an editor that has this material loaded would silently
overwrite the regenerated asset on its next Save All.

Task 3b (2026-09-25, user feedback): the earlier two-shell-plus-ring approach (M_ChakraAura /
M_ChakraRing, built by this script's previous version) read as "trapped in a glass capsule with a
lid" -- too big and washed out to white. Replaced with a body-hugging overlay material applied via
UMeshComponent::SetOverlayMaterial on the character's own skeletal mesh: the mesh is drawn a second
time with this material, so the aura silhouette always matches the current pose exactly (including
Task 4's power-up pose) with no separate meshes to size or align. M_ChakraAura and M_ChakraRing are
deleted by this script if present.

M_ChakraBodyAura: unlit, additive, one-sided (an overlay mesh batch reuses the character's own
front-facing triangles, so back-face culling behaves like the base mesh and two-sided is not
needed). Inflates along the vertex normal (World Position Offset) so it reads as a thin shell
around the body rather than a decal on the skin, glows only at the silhouette (Fresnel), and adds
faint upward-drifting noise wisps sampled in absolute world space (so the wisp pattern doesn't
stretch or stick to the mesh's UVs as it animates). Parameters: Color (vector), Intensity, Fade,
Inflate (scalars), driven by UShinobiChakraAuraComponent.

Overlay + World Position Offset, confirmed against the 5.8 engine source (this is the key risk this
task's plan called out -- an overlay that ignored WPO would have forced a different approach):
FSkeletalMeshSceneProxy::GetOverlayMeshBatch (Engine/Source/Runtime/Engine/Private/
SkeletalMeshSceneProxy.cpp) builds the overlay draw as `OutOverlayMeshBatch = FMeshBatch(MeshBatch)`
-- a copy of the SAME mesh batch used for the base draw (same vertex factory, same skinned/morphed
vertex data, same bone matrices) -- and only swaps `MaterialRenderProxy` to the overlay material's
proxy and sets `bOverlayMaterial = true`. `bOverlayMaterial` (Engine/Source/Runtime/Engine/Public/
MeshBatch.h) is read in exactly two places outside proxies that just copy it through
(FSkeletalMeshSceneProxy, StaticMeshBatch.cpp): SceneVisibility.cpp uses it only to exempt overlay
draws from distance culling. Nothing in the renderer special-cases `bOverlayMaterial` to strip World
Position Offset, and WPO is evaluated by the vertex shader compiled for whichever material the mesh
batch's `MaterialRenderProxy` points at -- which for the overlay batch is the overlay material, using
the same vertex factory (and therefore the same per-vertex normal/skinning inputs `VertexNormalWS`
needs) as the base draw. So an overlay material's WPO applies normally, including on a skinned mesh
following an animated pose. `GetOverlayMaterial()` / `SetOverlayMaterial()` live on UMeshComponent
(Engine/Source/Runtime/Engine/Classes/Components/MeshComponent.h): `GetOverlayMaterial(bool
bGetMaterialSlot = false, int32 SlotIndex = 0) const` and `SetOverlayMaterial(UMaterialInterface*
NewOverlayMaterial, bool bSetMaterialSlot = false, int32 SlotIndex = 0)`.

Substrate routing (r.Substrate=True, confirmed against the 5.8 engine source, unchanged from the
previous version of this script): UMaterialExpressionSubstrateUnlitBSDF's Emissive Color input is
connected, then the BSDF node is wired into the material's Front Material slot
(unreal.MaterialProperty.MP_FRONT_MATERIAL). Unlit + additive still uses MP_FrontMaterial
exclusively in 5.8, so no different route is needed.

Pin/property names confirmed against the engine source for this material's new nodes:
- World Position Offset is `unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET`
  (EMaterialProperty::MP_WorldPositionOffset in Engine/Source/Runtime/Engine/Public/SceneTypes.h),
  connected the same way as MP_FRONT_MATERIAL via `MaterialEditingLibrary.connect_material_property`.
- UMaterialExpressionVertexNormalWS (Engine/Source/Runtime/Engine/Public/Materials/
  MaterialExpressionVertexNormalWS.h) takes no inputs and exposes no editable properties -- it is
  wired only as a source, never a `node(..., **props)` target.
- UMaterialExpressionWorldPosition (.../MaterialExpressionWorldPosition.h) likewise takes no inputs;
  its one property, `WorldPositionShaderOffset` (python `world_position_shader_offset`), defaults to
  `WPT_Default` (absolute world position, offsets included), which is fine here since this node only
  feeds the emissive/wisp branch, not the WPO branch itself.
- UMaterialExpressionConstant2Vector's two float properties are named `R` and `G` in C++ (python
  `r`/`g`) even though the editor displays them as "X"/"Y"
  (Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionConstant2Vector.h) -- confirmed
  by reading the header rather than assumed from the display name.
- UMaterialExpressionLinearInterpolate's inputs are plain FExpressionInput properties `A`, `B`,
  `Alpha` with no GetInputName() override (.../MaterialExpressionLinearInterpolate.h), so (like the
  Add/Multiply/Subtract nodes already in this script) they connect by their literal property names;
  its unconnected-fallback float properties are `ConstA`/`ConstB`/`ConstAlpha` (python
  `const_a`/`const_b`/`const_alpha`).
- UMaterialExpressionFresnel's unconnected-fallback float properties are `Exponent`/
  `BaseReflectFraction` (python `exponent`/`base_reflect_fraction`); its input pins
  (`ExponentIn`/`BaseReflectFractionIn`/`Normal`) are left unconnected here, so only the properties
  are set (Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionFresnel.h).
- The Noise node's Position input is still targeted by the pin name "World Position" (not the
  property name "Position"), exactly as established in the previous version of this script and
  reconfirmed unchanged in MaterialExpressionNoise.h/.cpp for 5.8.

All were confirmed by reading the engine headers directly (not by trial-and-error against the
Python API), since this task called for verifying every expression/property name against source
before writing the graph code.
"""
import unreal

VFX_DIR = "/Game/Shinobi/VFX"
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def remove_if_exists(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"could not delete {path} (is it open in the editor?)")
        unreal.log(f"[setup_vfx] removed: {path}")


def fresh_material(name, two_sided=True):
    path = f"{VFX_DIR}/{name}"
    remove_if_exists(path)
    material = TOOLS.create_asset(name, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"failed to create {path}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", two_sided)
    return material


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(src, dst, dst_input, src_output=""):
    if not MEL.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")


def finish(material, emissive):
    """Substrate: emissive -> Unlit BSDF -> Front Material."""
    unlit = node(material, unreal.MaterialExpressionSubstrateUnlitBSDF, 400, 0)
    link(emissive, unlit, "EmissiveColor")
    if not MEL.connect_material_property(unlit, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError(f"failed to connect Front Material on {material.get_name()}")
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"{material.get_name()} failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"failed to save {material.get_name()}")
    unreal.log(f"[setup_vfx] built: {material.get_path_name()}")


def build_body_aura():
    m = fresh_material("M_ChakraBodyAura", two_sided=False)

    color = node(m, unreal.MaterialExpressionVectorParameter, -1200, -300,
                 parameter_name="Color", default_value=unreal.LinearColor(0.1, 0.45, 1.0, 1.0))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -1200, -150,
                     parameter_name="Intensity", default_value=2.5)
    fade = node(m, unreal.MaterialExpressionScalarParameter, -1200, 0,
                parameter_name="Fade", default_value=1.0)
    inflate = node(m, unreal.MaterialExpressionScalarParameter, -1200, 700,
                   parameter_name="Inflate", default_value=2.5)

    # World Position Offset: push the overlay shell out along the vertex normal so it hugs the body
    # instead of z-fighting with the skin.
    normal_ws = node(m, unreal.MaterialExpressionVertexNormalWS, -1000, 700)
    wpo = node(m, unreal.MaterialExpressionMultiply, -800, 700)
    link(normal_ws, wpo, "A")
    link(inflate, wpo, "B")
    if not MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        raise RuntimeError(f"failed to connect World Position Offset on {m.get_name()}")

    # Rim: glow only at the silhouette, not across the whole body.
    rim = node(m, unreal.MaterialExpressionFresnel, -1000, -450, exponent=3.0, base_reflect_fraction=0.0)

    # Wisps: faint upward-drifting noise. Sampled in absolute world space (not mesh UVs) so the
    # pattern doesn't stretch or stick as the pose animates.
    world_pos = node(m, unreal.MaterialExpressionWorldPosition, -1200, 200)
    scaled_pos = node(m, unreal.MaterialExpressionMultiply, -1000, 200, const_b=0.04)
    link(world_pos, scaled_pos, "A")
    time = node(m, unreal.MaterialExpressionTime, -1200, 350)
    rise = node(m, unreal.MaterialExpressionMultiply, -1050, 350, const_b=-1.2)
    link(time, rise, "A")
    xy_zero = node(m, unreal.MaterialExpressionConstant2Vector, -1000, 400, r=0.0, g=0.0)
    time_offset = node(m, unreal.MaterialExpressionAppendVector, -900, 380)
    link(xy_zero, time_offset, "A")
    link(rise, time_offset, "B")
    wisp_pos = node(m, unreal.MaterialExpressionAdd, -800, 300)
    link(scaled_pos, wisp_pos, "A")
    link(time_offset, wisp_pos, "B")
    noise = node(m, unreal.MaterialExpressionNoise, -650, 300, scale=1.0, levels=2, output_min=0.0, output_max=1.0)
    # See module docstring: the Position input's pin name is "World Position", not "Position".
    link(wisp_pos, noise, "World Position")
    wisps = node(m, unreal.MaterialExpressionLinearInterpolate, -500, 300, const_a=0.55, const_b=1.0)
    link(noise, wisps, "Alpha")

    # Emissive = Color * Intensity * Fade * Rim * Wisps
    ci = node(m, unreal.MaterialExpressionMultiply, -700, -300)
    link(color, ci, "A")
    link(intensity, ci, "B")
    cif = node(m, unreal.MaterialExpressionMultiply, -550, -250)
    link(ci, cif, "A")
    link(fade, cif, "B")
    cifr = node(m, unreal.MaterialExpressionMultiply, -400, -150)
    link(cif, cifr, "A")
    link(rim, cifr, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -200, 0)
    link(cifr, emissive, "A")
    link(wisps, emissive, "B")
    finish(m, emissive)


remove_if_exists(f"{VFX_DIR}/M_ChakraAura")
remove_if_exists(f"{VFX_DIR}/M_ChakraRing")
build_body_aura()
