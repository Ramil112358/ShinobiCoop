"""Jutsu VFX materials. Idempotent: rebuilds M_ChakraBodyAura, M_Flame, M_FlameDark, M_Streak, M_Phased, M_WaterWall, M_Susanoo and M_PushDome from scratch each run.

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
    scaled = node(m, unreal.MaterialExpressionMultiply, -750, 300, const_b=0.06)
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


def build_phased():
    """M_Phased: Kamui's ghost. Additive and unlit: a glowing chakra-blue figure, brighter at the silhouette, swapped onto
    every visible body mesh while State.Phased is on. (A translucent material is nearly invisible at low coverage: Substrate
    scales its glow by the coverage.) Params: Color, Intensity, Opacity (0 = gone; the glow scales with it)."""
    m = fresh_material("M_Phased", two_sided=False)
    color = node(m, unreal.MaterialExpressionVectorParameter, -800, -300,
                 parameter_name="Color", default_value=unreal.LinearColor(0.4, 0.6, 1.0, 1.0))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -800, -150, parameter_name="Intensity", default_value=3.0)
    opacity = node(m, unreal.MaterialExpressionScalarParameter, -800, 0, parameter_name="Opacity", default_value=0.3)
    # Stealth-cloak look: an almost empty body (the world shows through), a bright thin edge, and a faint rising shimmer.
    rim = node(m, unreal.MaterialExpressionFresnel, -800, 150, exponent=3.0, base_reflect_fraction=0.0)
    rim_part = node(m, unreal.MaterialExpressionMultiply, -600, 150, const_b=1.6)
    link(rim, rim_part, "A")
    edge = node(m, unreal.MaterialExpressionAdd, -500, 150, const_b=0.06)
    link(rim_part, edge, "A")
    world_pos = node(m, unreal.MaterialExpressionWorldPosition, -1200, 350)
    time = node(m, unreal.MaterialExpressionTime, -1200, 450)
    rise = node(m, unreal.MaterialExpressionMultiply, -1050, 450, const_b=60.0)
    link(time, rise, "A")
    xy_zero = node(m, unreal.MaterialExpressionConstant2Vector, -1050, 550, r=0.0, g=0.0)
    offset = node(m, unreal.MaterialExpressionAppendVector, -900, 500)
    link(xy_zero, offset, "A")
    link(rise, offset, "B")
    moved = node(m, unreal.MaterialExpressionSubtract, -750, 400)
    link(world_pos, moved, "A")
    link(offset, moved, "B")
    scaled_pos = node(m, unreal.MaterialExpressionMultiply, -620, 400, const_b=0.08)
    link(moved, scaled_pos, "A")
    noise = node(m, unreal.MaterialExpressionNoise, -500, 400, scale=1.0, levels=2, output_min=0.0, output_max=1.0)
    link(scaled_pos, noise, "World Position")
    shimmer = node(m, unreal.MaterialExpressionMultiply, -380, 400, const_b=0.22)
    link(noise, shimmer, "A")
    body = node(m, unreal.MaterialExpressionAdd, -300, 250)
    link(edge, body, "A")
    link(shimmer, body, "B")
    glow = node(m, unreal.MaterialExpressionMultiply, -600, -250)
    link(color, glow, "A")
    link(intensity, glow, "B")
    scaled = node(m, unreal.MaterialExpressionMultiply, -400, -150)
    link(glow, scaled, "A")
    link(opacity, scaled, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, -200, -50)
    link(scaled, emissive, "A")
    link(body, emissive, "B")
    require_skeletal_mesh_usage(m)
    finish(m, emissive)


SUSANOO_TEXTURE = "/Game/Shinobi/Jutsu/Susanoo/T_Susanoo"  # imported by Scripts/setup_susanoo.py


def build_susanoo():
    """M_Susanoo: the Perfect Susanoo's armour, as in Ultimate Ninja Storm 4. Translucent (Opacity of the background hidden), unlit, two-sided (the inside of the
    shell shows through, as in the game). Layers:
    - deep blue body, brighter at the silhouette, shaded by the armour texture's blue (plates differ a little);
    - bright cyan panel lines: the texture's thin cyan lines, picked out by their green (LineLow, LineGain);
    - sparkles: small cyan points twinkling over the surface (world-space noise drifting up, thresholded);
    - a slow band of light climbing the figure.
    Params: Color, LineColor, Intensity, Fade, Opacity, LineLow, LineGain, Armour (texture). Skeletal-mesh usage is baked in."""
    m = fresh_material("M_Susanoo", two_sided=True, blend=unreal.BlendMode.BLEND_TRANSLUCENT)

    def mul(a, b=None, x=0, y=0, **k):
        out = node(m, unreal.MaterialExpressionMultiply, x, y, **k)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    def add(a, b=None, x=0, y=0, **k):
        out = node(m, unreal.MaterialExpressionAdd, x, y, **k)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    def scalar(name, value, y):
        return node(m, unreal.MaterialExpressionScalarParameter, -1600, y, parameter_name=name, default_value=value)

    color = node(m, unreal.MaterialExpressionVectorParameter, -1600, -500, parameter_name="Color",
                 default_value=unreal.LinearColor(0.015, 0.1, 1.0, 1.0))
    line_color = node(m, unreal.MaterialExpressionVectorParameter, -1600, -350, parameter_name="LineColor",
                      default_value=unreal.LinearColor(0.25, 0.8, 1.0, 1.0))
    intensity = scalar("Intensity", 0.7, -200)
    fade = scalar("Fade", 1.0, -100)
    line_low = scalar("LineLow", 0.045, 0)
    line_gain = scalar("LineGain", 40.0, 100)
    texture = unreal.load_asset(SUSANOO_TEXTURE) if unreal.EditorAssetLibrary.does_asset_exist(SUSANOO_TEXTURE) else None
    armour = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1600, 250, parameter_name="Armour",
                  **({"texture": texture} if texture else {}))

    # Body: 0.1 everywhere + 0.7 at the rim, times the plate shading (0.5 + the texture's blue). Dim on purpose: the two-sided
    # shell adds up front, back and inner faces, and the panel lines must stay brighter than the plates.
    rim = node(m, unreal.MaterialExpressionFresnel, -1300, -650, exponent=2.5, base_reflect_fraction=0.0)
    base = add(mul(rim, x=-1150, y=-650, const_b=0.7), x=-1000, y=-650, const_b=0.1)
    blue = node(m, unreal.MaterialExpressionComponentMask, -1300, 250, r=False, g=False, b=True, a=False)
    link(armour, blue, "", "RGB")
    shading = add(mul(blue, x=-1150, y=250, const_b=1.0), x=-1000, y=250, const_b=0.5)
    # A band of light climbing the figure: 0.12 x sin(height x 0.004 - time x 1.5) (the figure is 12 m tall).
    height = node(m, unreal.MaterialExpressionComponentMask, -1300, 650, r=False, g=False, b=True, a=False)
    link(node(m, unreal.MaterialExpressionWorldPosition, -1450, 650), height, "")
    phase = node(m, unreal.MaterialExpressionSubtract, -1000, 650)
    link(mul(height, x=-1150, y=650, const_b=0.004), phase, "A")
    link(mul(node(m, unreal.MaterialExpressionTime, -1300, 750), x=-1150, y=750, const_b=1.5), phase, "B")
    sine = node(m, unreal.MaterialExpressionSine, -850, 650)
    link(phase, sine, "")
    body = add(mul(base, shading, x=-850, y=-400), mul(sine, x=-700, y=650, const_b=0.12), x=-600, y=-300)
    blue_glow = mul(color, body, x=-450, y=-450)

    # Panel lines: the texture's green above LineLow, times LineGain, clamped.
    green = node(m, unreal.MaterialExpressionComponentMask, -1300, 400, r=False, g=True, b=False, a=False)
    link(armour, green, "", "RGB")
    above = node(m, unreal.MaterialExpressionSubtract, -1150, 400)
    link(green, above, "A")
    link(line_low, above, "B")
    lines = node(m, unreal.MaterialExpressionSaturate, -850, 400)
    link(mul(above, line_gain, x=-1000, y=400), lines, "")

    # Sparkles: world-space noise (about 1 m cells) drifting up, only its top 8% kept, sharpened.
    world = mul(node(m, unreal.MaterialExpressionWorldPosition, -1450, 900), x=-1300, y=900, const_b=0.012)
    drift = node(m, unreal.MaterialExpressionAppendVector, -1150, 1000)
    link(node(m, unreal.MaterialExpressionConstant2Vector, -1300, 1000, r=0.0, g=0.0), drift, "A")
    link(mul(node(m, unreal.MaterialExpressionTime, -1450, 1100), x=-1300, y=1100, const_b=-0.6), drift, "B")
    noise = node(m, unreal.MaterialExpressionNoise, -1000, 900, scale=1.0, levels=1, output_min=0.0, output_max=1.0)
    link(add(world, drift, x=-1150, y=900), noise, "World Position")
    peak = node(m, unreal.MaterialExpressionSubtract, -850, 900, const_b=0.92)
    link(noise, peak, "A")
    sparkles = node(m, unreal.MaterialExpressionSaturate, -550, 900)
    link(mul(peak, x=-700, y=900, const_b=30.0), sparkles, "")
    cyan = mul(line_color, add(mul(lines, x=-550, y=400, const_b=2.0), mul(sparkles, x=-400, y=900, const_b=4.0), x=-400, y=500), x=-300, y=-150)

    emissive = mul(mul(add(blue_glow, cyan, x=-200, y=-300), intensity, x=-100, y=-250), fade, x=0, y=-200)
    # Coverage: the armour hides Opacity of what is behind it (times Fade), so it reads as a body, not a ghost.
    opacity = scalar("Opacity", 0.35, 200)
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 100, 200)
    link(mul(opacity, fade, x=-100, y=200), transmittance, "")
    require_skeletal_mesh_usage(m)
    finish(m, emissive, transmittance)


def build_water_wall():
    """M_WaterWall: the Water Wall's slab. Translucent unlit blue, flowing noise rising through it, brighter at the edges.
    (Not the river's single-layer-water material: that one lights depth behind a surface, not a vertical slab.) Param: Color, Intensity."""
    m = fresh_material("M_WaterWall", two_sided=True, blend=unreal.BlendMode.BLEND_TRANSLUCENT)
    color = node(m, unreal.MaterialExpressionVectorParameter, -1400, -300,
                 parameter_name="Color", default_value=unreal.LinearColor(0.08, 0.38, 0.9, 1.0))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -1400, -150, parameter_name="Intensity", default_value=1.4)

    # Noise climbing through the slab: world position shifted down over time, ~1 cell per 33 units.
    world_pos = node(m, unreal.MaterialExpressionWorldPosition, -1400, 250)
    time = node(m, unreal.MaterialExpressionTime, -1400, 350)
    rise = node(m, unreal.MaterialExpressionMultiply, -1200, 400, const_b=90.0)
    link(time, rise, "A")
    xy_zero = node(m, unreal.MaterialExpressionConstant2Vector, -1200, 500, r=0.0, g=0.0)
    offset = node(m, unreal.MaterialExpressionAppendVector, -1050, 450)
    link(xy_zero, offset, "A")
    link(rise, offset, "B")
    moved = node(m, unreal.MaterialExpressionSubtract, -900, 300)
    link(world_pos, moved, "A")
    link(offset, moved, "B")
    scaled = node(m, unreal.MaterialExpressionMultiply, -750, 300, const_b=0.03)
    link(moved, scaled, "A")
    noise = node(m, unreal.MaterialExpressionNoise, -600, 300, scale=1.0, levels=3, output_min=0.0, output_max=1.0)
    link(scaled, noise, "World Position")
    rim = node(m, unreal.MaterialExpressionFresnel, -600, 100, exponent=2.0, base_reflect_fraction=0.0)

    # brightness = 0.5 + 0.5 * noise + 0.8 * rim; coverage = 0.45 + 0.35 * noise + 0.2 * rim
    noise_glow = node(m, unreal.MaterialExpressionMultiply, -400, 300, const_b=0.5)
    link(noise, noise_glow, "A")
    rim_glow = node(m, unreal.MaterialExpressionMultiply, -400, 100, const_b=0.8)
    link(rim, rim_glow, "A")
    glow_sum = node(m, unreal.MaterialExpressionAdd, -250, 200)
    link(noise_glow, glow_sum, "A")
    link(rim_glow, glow_sum, "B")
    brightness = node(m, unreal.MaterialExpressionAdd, -100, 200, const_b=0.5)
    link(glow_sum, brightness, "A")
    ci = node(m, unreal.MaterialExpressionMultiply, -400, -250)
    link(color, ci, "A")
    link(intensity, ci, "B")
    emissive = node(m, unreal.MaterialExpressionMultiply, 100, -100)
    link(ci, emissive, "A")
    link(brightness, emissive, "B")

    noise_cover = node(m, unreal.MaterialExpressionMultiply, -400, 500, const_b=0.35)
    link(noise, noise_cover, "A")
    rim_cover = node(m, unreal.MaterialExpressionMultiply, -400, 650, const_b=0.2)
    link(rim, rim_cover, "A")
    cover_sum = node(m, unreal.MaterialExpressionAdd, -250, 550)
    link(noise_cover, cover_sum, "A")
    link(rim_cover, cover_sum, "B")
    coverage = node(m, unreal.MaterialExpressionAdd, -100, 550, const_b=0.45)
    link(cover_sum, coverage, "A")
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 100, 550)
    link(coverage, transmittance, "")
    finish(m, emissive, transmittance)


def build_push_dome():
    """M_PushDome: Almighty Push's sphere of force, as in Storm 4: a see-through shell with a brighter rim, turbulent
    clouds of dust churning round its foot only (the top stays clear), and the world behind its rim bent in moving waves
    (screen refraction, 2D offset; only at the rim, since the toon outline is drawn from undistorted depth and would come
    away from the caster seen through the middle).
    Two-sided, so it still reads once it has grown past the camera. Params: Color, Opacity, Distort (viewport UV), Fade."""
    m = fresh_material("M_PushDome", two_sided=True, blend=unreal.BlendMode.BLEND_TRANSLUCENT)
    mode = next(getattr(unreal.RefractionMode, k) for k in dir(unreal.RefractionMode) if "2D" in k.upper())  # RM_2DOffset
    m.set_editor_property("refraction_method", mode)
    color = node(m, unreal.MaterialExpressionVectorParameter, -1400, -400, parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.5, 0.49, 1.0))
    opacity = node(m, unreal.MaterialExpressionScalarParameter, -1400, -250, parameter_name="Opacity", default_value=1.0)
    distort = node(m, unreal.MaterialExpressionScalarParameter, -1400, -150, parameter_name="Distort", default_value=0.05)
    fade = node(m, unreal.MaterialExpressionScalarParameter, -1400, -50, parameter_name="Fade", default_value=1.0)

    def mul(a, b=None, x=0, y=0, **props):
        out = node(m, unreal.MaterialExpressionMultiply, x, y, **props)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    def add(a, b=None, x=0, y=0, **props):
        out = node(m, unreal.MaterialExpressionAdd, x, y, **props)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    # Clouds: turbulent noise in the dome's own space (so it is centred on the caster), squashed so the billows run
    # round the dome in bands, drifting up and round over time.
    local = node(m, unreal.MaterialExpressionSubtract, -1200, 300)
    link(node(m, unreal.MaterialExpressionWorldPosition, -1400, 250), local, "A")
    link(node(m, unreal.MaterialExpressionObjectPositionWS, -1400, 400), local, "B")
    squash = node(m, unreal.MaterialExpressionConstant3Vector, -1200, 450, constant=unreal.LinearColor(0.0035, 0.0035, 0.008, 1.0))
    drift_dir = node(m, unreal.MaterialExpressionConstant3Vector, -1200, 600, constant=unreal.LinearColor(0.35, -0.25, 0.6, 1.0))
    drift = mul(node(m, unreal.MaterialExpressionTime, -1400, 650), drift_dir, -1050, 600)
    p = add(mul(local, squash, -1050, 350), drift, -900, 450)
    noise = node(m, unreal.MaterialExpressionNoise, -750, 450, scale=1.0, levels=4, turbulence=True, output_min=0.0, output_max=1.0)
    link(p, noise, "World Position")
    # Contrast: thin haze between the billows, not an even fog. cloud = saturate((noise - 0.35) x 2.2)
    cloud = node(m, unreal.MaterialExpressionSaturate, -600, 450)
    link(mul(add(noise, None, -680, 520, const_b=-0.35), None, -640, 520, const_b=2.2), cloud, "")
    # Two more samples, offset, for the refraction's x and y waves.
    wave_x = node(m, unreal.MaterialExpressionNoise, -750, 650, scale=1.6, levels=2, output_min=-1.0, output_max=1.0)
    link(p, wave_x, "World Position")
    wave_y = node(m, unreal.MaterialExpressionNoise, -750, 850, scale=1.6, levels=2, output_min=-1.0, output_max=1.0)
    link(add(p, node(m, unreal.MaterialExpressionConstant3Vector, -900, 900, constant=unreal.LinearColor(17.3, 5.1, 9.7, 1.0)), -800, 900), wave_y, "World Position")
    rim = node(m, unreal.MaterialExpressionFresnel, -750, 150, exponent=2.5, base_reflect_fraction=0.0)
    # Foot: 1 up to 0.15 of the radius above the ground (the centre sits 0.2 R up), 0 from 0.45 R up.
    # foot = saturate((0.1 - z / ObjectRadius) / 0.3); the clouds and the haze live only there.
    up = node(m, unreal.MaterialExpressionComponentMask, -1150, 100, r=False, g=False, b=True, a=False)
    link(local, up, "")
    height = node(m, unreal.MaterialExpressionDivide, -1050, 150)
    link(up, height, "A")
    link(node(m, unreal.MaterialExpressionObjectRadius, -1150, 200), height, "B")
    below = node(m, unreal.MaterialExpressionSubtract, -900, 50, const_a=0.1)
    link(height, below, "B")
    foot = node(m, unreal.MaterialExpressionSaturate, -750, 50)
    link(mul(below, None, -820, 50, const_b=3.333), foot, "")
    cloud = mul(cloud, foot, -600, 350)

    # coverage = saturate(Opacity x Side x (0.03 x foot + 0.4 x cloud + 0.45 x rim)) x Fade. Side is 1 on the near (outer) face
    # and 0.6 on the inner face, so from inside the dome (the camera usually is, at a 6 m radius) the screen stays clear.
    raw = add(add(mul(cloud, None, -550, 450, const_b=0.4), mul(rim, None, -550, 250, const_b=0.45), -400, 350), mul(foot, None, -400, 250, const_b=0.03), -250, 350)
    side = add(mul(node(m, unreal.MaterialExpressionTwoSidedSign, -550, 600), None, -450, 600, const_b=0.2), None, -350, 600, const_b=0.8)
    sat = node(m, unreal.MaterialExpressionSaturate, -100, 350)
    link(mul(mul(raw, opacity, -200, 300), side, -150, 450), sat, "")
    coverage = mul(sat, fade, 50, 350)
    # colour = Color x (0.55 + 0.35 x cloud + 0.5 x rim), premultiplied by coverage
    shade = add(add(mul(cloud, None, -550, -50, const_b=0.35), mul(rim, None, -550, 50, const_b=0.5), -400, 0), None, -250, 0, const_b=0.55)
    emissive = mul(mul(color, shade, -100, -200), coverage, 100, -150)
    transmittance = node(m, unreal.MaterialExpressionOneMinus, 250, 350)
    link(coverage, transmittance, "")

    # Refraction: a 2D screen offset of up to Distort x rim^2, so nothing in the middle of the view moves, faded out with the dome.
    offset = node(m, unreal.MaterialExpressionAppendVector, -550, 750)
    link(wave_x, offset, "A")
    link(wave_y, offset, "B")
    bend = mul(mul(offset, mul(rim, rim, -550, 1000), -400, 800), mul(distort, fade, -400, 950), -250, 850)
    if not MEL.connect_material_property(bend, "", unreal.MaterialProperty.MP_REFRACTION):
        raise RuntimeError("failed to connect Refraction on M_PushDome")
    finish(m, emissive, transmittance)


def link_any(src, dst, names, src_output=""):
    """link() to the first of several candidate pin names (5.8 exposes some pins under their display name)."""
    for name in names:
        if MEL.connect_material_expressions(src, src_output, dst, name):
            return
    raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()} (tried {names})")


def build_perfect_susanoo():
    """M_PerfectSusanoo: the Perfect Susanoo jutsu figure, as in Ultimate Ninja Storm 4: a near-solid royal-blue body, not a
    ghost. Masked and lit (real shading, correct sorting), its coverage dithered to Opacity x Fade, so it dissolves in and out
    (Opacity is 1: below about 0.95 the dither reads as grain, as Storm 4's figure does not). Layers on top of the lit deep blue:
    - an emissive blue floor (the shadow side stays blue, not black) and a fresnel rim of light;
    - clean cyan panel lines: only the armour texture's strongest green (LineLow 0.12 keeps the drawn lines, not the noise);
    - a few twinkling sparkles and a slow band of light climbing the figure.
    Params: Color, LineColor, Glow, Rim, LineBright, Fade, Opacity, LineLow, LineGain, Armour. M_Susanoo stays as the guard ribs'."""
    m = fresh_material("M_PerfectSusanoo", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("opacity_mask_clip_value", 0.5)

    def op(cls, a, b=None, x=0, y=0, **k):
        out = node(m, cls, x, y, **k)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    def scalar(name, value, y):
        return node(m, unreal.MaterialExpressionScalarParameter, -1600, y, parameter_name=name, default_value=value)

    mul = lambda a, b=None, x=0, y=0, **k: op(unreal.MaterialExpressionMultiply, a, b, x, y, **k)
    add = lambda a, b=None, x=0, y=0, **k: op(unreal.MaterialExpressionAdd, a, b, x, y, **k)
    color = node(m, unreal.MaterialExpressionVectorParameter, -1600, -600, parameter_name="Color",
                 default_value=unreal.LinearColor(0.012, 0.06, 0.85, 1.0))
    line_color = node(m, unreal.MaterialExpressionVectorParameter, -1600, -450, parameter_name="LineColor",
                      default_value=unreal.LinearColor(0.3, 0.75, 1.0, 1.0))
    glow = scalar("Glow", 0.25, -300)
    rim_gain = scalar("Rim", 1.6, -200)
    line_bright = scalar("LineBright", 4.0, -100)
    fade = scalar("Fade", 1.0, 0)
    line_low = scalar("LineLow", 0.12, 100)
    line_gain = scalar("LineGain", 6.0, 200)
    opacity = scalar("Opacity", 1.0, 300)
    texture = unreal.load_asset(SUSANOO_TEXTURE) if unreal.EditorAssetLibrary.does_asset_exist(SUSANOO_TEXTURE) else None
    armour = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1600, 450, parameter_name="Armour",
                  **({"texture": texture} if texture else {}))

    # Lit base colour: Color, plates a little lighter or darker with the texture's blue (0.6 + 0.6 x blue).
    blue = node(m, unreal.MaterialExpressionComponentMask, -1300, 450, r=False, g=False, b=True, a=False)
    link(armour, blue, "", "RGB")
    plates = add(mul(blue, x=-1150, y=450, const_b=0.6), x=-1000, y=450, const_b=0.6)
    base_color = mul(color, plates, x=-800, y=-700)

    # Panel lines: the texture's green above LineLow, times LineGain, clamped.
    green = node(m, unreal.MaterialExpressionComponentMask, -1300, 600, r=False, g=True, b=False, a=False)
    link(armour, green, "", "RGB")
    lines = node(m, unreal.MaterialExpressionSaturate, -850, 600)
    link(mul(op(unreal.MaterialExpressionSubtract, green, line_low, -1150, 600), line_gain, x=-1000, y=600), lines, "")

    # Band of light climbing the figure (0.08 x sin(height x 0.004 - time x 1.5)), and sparse sparkles: world-space noise
    # (~1 m cells) drifting up, only its top 4% kept.
    height = node(m, unreal.MaterialExpressionComponentMask, -1300, 800, r=False, g=False, b=True, a=False)
    link(node(m, unreal.MaterialExpressionWorldPosition, -1450, 800), height, "")
    phase = op(unreal.MaterialExpressionSubtract, mul(height, x=-1150, y=800, const_b=0.004),
               mul(node(m, unreal.MaterialExpressionTime, -1300, 900), x=-1150, y=900, const_b=1.5), -1000, 800)
    sine = node(m, unreal.MaterialExpressionSine, -850, 800)
    link(phase, sine, "")
    world = mul(node(m, unreal.MaterialExpressionWorldPosition, -1450, 1050), x=-1300, y=1050, const_b=0.012)
    drift = node(m, unreal.MaterialExpressionAppendVector, -1150, 1150)
    link(node(m, unreal.MaterialExpressionConstant2Vector, -1300, 1150, r=0.0, g=0.0), drift, "A")
    link(mul(node(m, unreal.MaterialExpressionTime, -1450, 1250), x=-1300, y=1250, const_b=-0.6), drift, "B")
    noise = node(m, unreal.MaterialExpressionNoise, -1000, 1050, scale=1.0, levels=1, output_min=0.0, output_max=1.0)
    link(add(world, drift, x=-1150, y=1050), noise, "World Position")
    sparkles = node(m, unreal.MaterialExpressionSaturate, -550, 1050)
    link(mul(op(unreal.MaterialExpressionSubtract, noise, None, -850, 1050, const_b=0.96), x=-700, y=1050, const_b=30.0), sparkles, "")

    # Emissive: Color x (Glow + Rim x fresnel + band) + LineColor x (LineBright x lines + 2 x sparkles), all x Fade.
    rim = mul(node(m, unreal.MaterialExpressionFresnel, -1300, -250, exponent=3.0, base_reflect_fraction=0.0), rim_gain, x=-1150, y=-250)
    body = mul(color, add(add(glow, rim, x=-1000, y=-300), mul(sine, x=-700, y=800, const_b=0.08), x=-850, y=-300), x=-650, y=-400)
    cyan = mul(line_color, add(mul(lines, line_bright, x=-650, y=600), mul(sparkles, x=-400, y=1050, const_b=2.0), x=-400, y=650), x=-300, y=-100)
    emissive = mul(add(body, cyan, x=-200, y=-300), fade, x=-50, y=-250)

    shading = node(m, unreal.MaterialExpressionSubstrateShadingModels, 300, 0)
    link_any(base_color, shading, ["BaseColor", "Base Color"])
    link_any(node(m, unreal.MaterialExpressionConstant, 0, 100, r=0.6), shading, ["Roughness"])
    link_any(node(m, unreal.MaterialExpressionConstant, 0, 200, r=0.25), shading, ["Specular"])
    link_any(emissive, shading, ["EmissiveColor", "Emissive Color"])
    if not MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError("M_PerfectSusanoo: could not connect Front Material")
    # Coverage: dithered (TSR smooths it) to Opacity x Fade, so the figure dissolves in and out.
    dither = node(m, unreal.MaterialExpressionMaterialFunctionCall, 0, 400, material_function=unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
    link_any(mul(opacity, fade, x=-150, y=400), dither, ["Alpha Threshold", "AlphaThreshold", ""])
    if not MEL.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError("M_PerfectSusanoo: could not connect the opacity mask")
    require_skeletal_mesh_usage(m)
    errors = MEL.recompile_material(m)
    if errors:
        raise RuntimeError(f"M_PerfectSusanoo failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log(f"[setup_vfx] built: {m.get_path_name()}")


def build_susanoo_blade():
    """M_SusanooBlade: the Perfect Susanoo's chakra swords (SM_SusanooSword), as in Ultimate Ninja Storm 4: a white-hot core
    fading to blue-cyan edges, bright enough to bloom into a halo. The core is the mesh's vertex colour (red 1 on the ridge,
    0 on the edges; Scripts/character/make_susanoo_sword.py). Translucent, unlit, after DOF, with a faint shimmer running up it.
    Params: CoreColor, EdgeColor, Intensity, Opacity, Fade."""
    m = fresh_material("M_SusanooBlade", two_sided=True, blend=unreal.BlendMode.BLEND_TRANSLUCENT)

    def op(cls, a, b=None, x=0, y=0, **k):
        out = node(m, cls, x, y, **k)
        link(a, out, "A")
        if b is not None:
            link(b, out, "B")
        return out

    mul = lambda a, b=None, x=0, y=0, **k: op(unreal.MaterialExpressionMultiply, a, b, x, y, **k)
    core_color = node(m, unreal.MaterialExpressionVectorParameter, -1200, -400, parameter_name="CoreColor",
                      default_value=unreal.LinearColor(0.6, 0.9, 1.0, 1.0))
    edge_color = node(m, unreal.MaterialExpressionVectorParameter, -1200, -250, parameter_name="EdgeColor",
                      default_value=unreal.LinearColor(0.02, 0.25, 1.0, 1.0))
    intensity = node(m, unreal.MaterialExpressionScalarParameter, -1200, -100, parameter_name="Intensity", default_value=3.0)
    opacity = node(m, unreal.MaterialExpressionScalarParameter, -1200, 0, parameter_name="Opacity", default_value=0.85)
    fade = node(m, unreal.MaterialExpressionScalarParameter, -1200, 100, parameter_name="Fade", default_value=1.0)

    ridge = node(m, unreal.MaterialExpressionComponentMask, -1000, 250, r=True, g=False, b=False, a=False)
    link(node(m, unreal.MaterialExpressionVertexColor, -1200, 250), ridge, "")
    core = node(m, unreal.MaterialExpressionPower, -850, 250, const_exponent=4.0)
    link(ridge, core, "Base")
    colour = node(m, unreal.MaterialExpressionLinearInterpolate, -650, -300)
    link(edge_color, colour, "A")
    link(core_color, colour, "B")
    link(core, colour, "Alpha")

    # Shimmer: 0.8 + 0.4 x world-space noise (~25 cm cells) rising through the blade.
    drift = node(m, unreal.MaterialExpressionAppendVector, -850, 500)
    link(node(m, unreal.MaterialExpressionConstant2Vector, -1000, 500, r=0.0, g=0.0), drift, "A")
    link(mul(node(m, unreal.MaterialExpressionTime, -1200, 600), x=-1000, y=600, const_b=-3.0), drift, "B")
    moved = op(unreal.MaterialExpressionAdd, mul(node(m, unreal.MaterialExpressionWorldPosition, -1200, 400), x=-1000, y=400, const_b=0.04), drift, -700, 400)
    noise = node(m, unreal.MaterialExpressionNoise, -550, 400, scale=1.0, levels=2, output_min=0.8, output_max=1.2)
    link(moved, noise, "World Position")

    emissive = mul(mul(mul(colour, intensity, x=-450, y=-300), noise, x=-300, y=-200), fade, x=-150, y=-200)
    # Coverage: Opacity x Fade, the core a little more solid than the edges (0.6 + 0.4 x ridge).
    cover = mul(mul(opacity, fade, x=-650, y=100), op(unreal.MaterialExpressionAdd, mul(ridge, x=-850, y=150, const_b=0.4), None, -700, 150, const_b=0.6), x=-450, y=100)
    transmittance = node(m, unreal.MaterialExpressionOneMinus, -300, 100)
    link(cover, transmittance, "")
    finish(m, emissive, transmittance)


if __name__ == "__main__":  # Scripts/setup_susanoo.py imports this module for build_susanoo
    remove_if_exists(f"{VFX_DIR}/M_ChakraAura")
    remove_if_exists(f"{VFX_DIR}/M_ChakraRing")
    build_body_aura()
    build_flame("M_Flame", dark=False)
    build_flame("M_FlameDark", dark=True)
    build_streak()
    build_phased()
    build_water_wall()
    build_susanoo()
    build_push_dome()
