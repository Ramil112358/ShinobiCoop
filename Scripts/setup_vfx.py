"""Jutsu VFX materials. Idempotent: rebuilds M_ChakraBodyAura, M_Flame, M_FlameDark and M_Streak from scratch each run.

Close the GUI editor before running: an editor that has this material loaded would silently
overwrite the regenerated asset on its next Save All.

Task 3b (2026-09-25, user feedback): replaced the earlier two-shell-plus-ring look (M_ChakraAura /
M_ChakraRing -- too big, washed out to white) with a body-hugging overlay material applied via
UMeshComponent::SetOverlayMaterial on the character's own skeletal mesh, so the aura silhouette
always matches the current pose exactly. Deletes the old M_ChakraAura / M_ChakraRing if present.

M_ChakraBodyAura: unlit, additive, one-sided. Inflates along the vertex normal (World Position
Offset) so it reads as a thin shell around the body rather than a decal on the skin, glows only at
the silhouette (Fresnel), and adds faint upward-drifting noise wisps sampled in absolute world space
(so the pattern doesn't stretch or stick to the mesh's UVs as it animates). Parameters: Color
(vector), Intensity, Fade, Inflate, Core (scalars), driven by UShinobiChakraAuraComponent and, via
ShinobiVfx::MakeGlow, by every other M2b effect (Core 1 = solid glow for orbs and bursts). Substrate
routing (emissive -> SubstrateUnlitBSDF.EmissiveColor -> MP_FRONT_MATERIAL) is unchanged from the
previous version of this script.

Also bakes MATUSAGE_SkeletalMesh into the asset (`set_base_material_usage`, then asserts it stuck via
`has_material_usage`): an overlay material missing this flag is silently nulled out by
FSkeletalMeshSceneProxy outside the editor (UMaterial::SetMaterialUsage only auto-adds a missing flag
when GIsEditor && !bIsGame, which is false in PIE/-game/cooked), so it must be baked into the asset
here rather than left to auto-detection. Full engine-source evidence is in the plan's Task 3b
"Findings from building this task" block.

Pin/property names confirmed against the 5.8 engine source for this material's nodes (full evidence
in the plan, as above):
- World Position Offset is `unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET`.
- `MaterialExpressionVertexNormalWS` / `MaterialExpressionWorldPosition` take no inputs and expose no
  properties relevant here (WorldPositionShaderOffset's default `WPT_Default` is fine since this node
  only feeds the emissive/wisp branch, not the WPO branch).
- `MaterialExpressionConstant2Vector`'s float properties are named `r`/`g` in Python, not `x`/`y`,
  despite the editor's "X"/"Y" display names.
- `MaterialExpressionLinearInterpolate` connects via plain `A`/`B`/`Alpha`; its unconnected-fallback
  properties are `const_a`/`const_b`/`const_alpha`.
- `MaterialExpressionFresnel`'s unconnected-fallback properties are `exponent`/`base_reflect_fraction`.
- The Noise node's Position input pin is still `"World Position"`, not the property name `"Position"`
  (established in the previous version of this script, reconfirmed unchanged for 5.8).
- `set_base_material_usage` / `has_material_usage` are the current (5.8) MaterialEditingLibrary API
  for usage flags; the older `SetMaterialUsage` is deprecated in favour of `SetBaseMaterialUsage`.
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


def fresh_material(name, two_sided, blend=unreal.BlendMode.BLEND_ADDITIVE):
    path = f"{VFX_DIR}/{name}"
    remove_if_exists(path)
    material = TOOLS.create_asset(name, VFX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"failed to create {path}")
    material.set_editor_property("blend_mode", blend)
    # After DOF: composited after the toon post-process (M_ToonPost, Scene Color After DOF), whose lighting remap
    # would otherwise flatten every glow into the surface behind it (M5 bug: jutsu invisible).
    material.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
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


def require_skeletal_mesh_usage(material):
    """Bake MATUSAGE_SkeletalMesh into the asset; see the module docstring for why this can't be left
    to editor auto-detection. Raises if the flag didn't actually stick."""
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    if not MEL.has_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH):
        raise RuntimeError(f"{material.get_name()} did not accept MATUSAGE_SkeletalMesh")
    unreal.log(f"[setup_vfx] usage flag confirmed: {material.get_name()} MATUSAGE_SkeletalMesh")


def finish(material, emissive, transmittance=None):
    """Substrate: emissive (and, for translucent materials, transmittance) -> Unlit BSDF -> Front Material."""
    unlit = node(material, unreal.MaterialExpressionSubstrateUnlitBSDF, 400, 0)
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

    # Rim: glow only at the silhouette, not across the whole body. Core blends toward a solid glow
    # (0 = rim only, the body aura; 1 = glowing all over, for jutsu orbs and impact bursts, M2b).
    rim = node(m, unreal.MaterialExpressionFresnel, -1000, -450, exponent=3.0, base_reflect_fraction=0.0)
    core = node(m, unreal.MaterialExpressionScalarParameter, -1200, -600,
                parameter_name="Core", default_value=0.0)
    rim_or_core = node(m, unreal.MaterialExpressionLinearInterpolate, -800, -450, const_b=1.0)
    link(rim, rim_or_core, "A")
    link(core, rim_or_core, "Alpha")

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
    link(rim_or_core, cifr, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -200, 0)
    link(cifr, emissive, "A")
    link(wisps, emissive, "B")
    require_skeletal_mesh_usage(m)
    finish(m, emissive)


def build_flame(name, dark):
    """Flame tongues for jutsu (M_Flame: additive fire and lightning; M_FlameDark: Amaterasu's black flames).

    Rising world-space noise, eroded by Erode, so the surface breaks into flickering tongues. Stronger where the
    surface faces the camera and ragged at the silhouette, so a stretched sphere or cone reads as a flame, not a
    ball. Additive can't show black, so the dark variant is translucent: its body is a near-black core and only the
    rim carries Color; the tongue mask becomes coverage (Substrate: transmittance = 1 - coverage).
    Params: Color, Intensity, Erode, Speed, Fade.
    """
    blend = unreal.BlendMode.BLEND_TRANSLUCENT if dark else unreal.BlendMode.BLEND_ADDITIVE
    m = fresh_material(name, two_sided=True, blend=blend)
    color = node(m, unreal.MaterialExpressionVectorParameter, -1400, -400,
                 parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.22, 0.02, 1.0))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -1400, -250, parameter_name="Intensity", default_value=3.0)
    erode = node(m, unreal.MaterialExpressionScalarParameter, -1400, -100, parameter_name="Erode", default_value=0.35)
    speed = node(m, unreal.MaterialExpressionScalarParameter, -1400, 450, parameter_name="Speed", default_value=120.0)
    fade = node(m, unreal.MaterialExpressionScalarParameter, -1400, 50, parameter_name="Fade", default_value=1.0)

    # Rising noise: world position shifted down over time (the pattern climbs), sampled at ~1 cell per 40 units.
    world_pos = node(m, unreal.MaterialExpressionWorldPosition, -1400, 250)
    time = node(m, unreal.MaterialExpressionTime, -1400, 350)
    rise = node(m, unreal.MaterialExpressionMultiply, -1200, 400)
    link(time, rise, "A")
    link(speed, rise, "B")
    xy_zero = node(m, unreal.MaterialExpressionConstant2Vector, -1200, 500, r=0.0, g=0.0)
    offset = node(m, unreal.MaterialExpressionAppendVector, -1050, 450)
    link(xy_zero, offset, "A")
    link(rise, offset, "B")
    moved = node(m, unreal.MaterialExpressionSubtract, -900, 300)
    link(world_pos, moved, "A")
    link(offset, moved, "B")
    scaled = node(m, unreal.MaterialExpressionMultiply, -750, 300, const_b=0.025)
    link(moved, scaled, "A")
    noise = node(m, unreal.MaterialExpressionNoise, -600, 300, scale=1.0, levels=3, output_min=0.0, output_max=1.0)
    link(scaled, noise, "World Position")

    # Facing: 1 where the surface faces the camera, 0 at the silhouette (1 - Fresnel).
    fresnel = node(m, unreal.MaterialExpressionFresnel, -900, -250, exponent=2.0, base_reflect_fraction=0.0)
    facing = node(m, unreal.MaterialExpressionOneMinus, -750, -250)
    link(fresnel, facing, "")
    # Tongue mask = clamp((noise * facing - Erode) * 4, 0, 1)
    shaped = node(m, unreal.MaterialExpressionMultiply, -450, 0)
    link(noise, shaped, "A")
    link(facing, shaped, "B")
    eroded = node(m, unreal.MaterialExpressionSubtract, -300, 0)
    link(shaped, eroded, "A")
    link(erode, eroded, "B")
    sharp = node(m, unreal.MaterialExpressionMultiply, -150, 0, const_b=4.0)
    link(eroded, sharp, "A")
    floor = node(m, unreal.MaterialExpressionMax, 0, 0, const_b=0.0)
    link(sharp, floor, "A")
    mask = node(m, unreal.MaterialExpressionMin, 100, 0, const_b=1.0)
    link(floor, mask, "A")
    faded = node(m, unreal.MaterialExpressionMultiply, 200, 0)
    link(mask, faded, "A")
    link(fade, faded, "B")

    glow = node(m, unreal.MaterialExpressionMultiply, -450, -400)
    link(color, glow, "A")
    link(intensity, glow, "B")
    if not dark:
        emissive = node(m, unreal.MaterialExpressionMultiply, 300, -200)
        link(glow, emissive, "A")
        link(faded, emissive, "B")
        finish(m, emissive)
        return
    # Dark: near-black body, Color only toward the rim; the mask is coverage.
    core = node(m, unreal.MaterialExpressionConstant3Vector, -450, -550, constant=unreal.LinearColor(0.01, 0.0, 0.015, 1.0))
    body = node(m, unreal.MaterialExpressionLinearInterpolate, -150, -400)
    link(glow, body, "A")
    link(core, body, "B")
    link(facing, body, "Alpha")
    emissive = node(m, unreal.MaterialExpressionMultiply, 300, -200)
    link(body, emissive, "A")
    link(faded, emissive, "B")
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 300, 100)
    link(faded, transmittance, "")
    finish(m, emissive, transmittance)


def build_streak():
    """M_Streak: flat unlit Color (the blood under Amaterasu's eyes). Opaque. Param: Color."""
    m = fresh_material("M_Streak", two_sided=True, blend=unreal.BlendMode.BLEND_OPAQUE)
    color = node(m, unreal.MaterialExpressionVectorParameter, -400, 0,
                 parameter_name="Color", default_value=unreal.LinearColor(0.35, 0.0, 0.0, 1.0))
    finish(m, color)


remove_if_exists(f"{VFX_DIR}/M_ChakraAura")
remove_if_exists(f"{VFX_DIR}/M_ChakraRing")
build_body_aura()
build_flame("M_Flame", dark=False)
build_flame("M_FlameDark", dark=True)
build_streak()
