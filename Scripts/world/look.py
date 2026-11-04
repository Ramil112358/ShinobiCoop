"""The realistic look's post-process (sub-project #3b), shared by build_world.py (stage "mood", the valley) and
Scripts/setup_look.py (the arenas). UE Python.

M_OutlinePost draws a thin edge around characters only: they write a stencil ID into custom depth (ShinobiLook.h:
1 = friendly, 2 = enemy); a pixel outside a character with a visible character neighbour becomes the edge. Neighbours
hidden behind scenery don't count (their custom depth is behind the scene depth), so outlines never show through walls.
apply_post() puts it on a post-process volume with the realistic settings (mild bloom, no grain or chromatic
aberration, a slightly warm white balance)."""
import unreal

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MATERIAL = "/Game/Shinobi/Materials/M_OutlinePost"
VOLUME_LABEL = "ShinobiLookPost"

OUTLINE_HLSL = r"""
// After tonemapping: this runs after the upscaler (TSR / DLSS) at the output resolution, so the outline is drawn sharp at
// any upscaling quality and frame generation sees it as part of the picture. The colour (SC) is at output resolution,
// but custom depth, scene depth and the stencil are still the render-resolution scene textures: every lookup goes
// through ViewportUVToSceneTextureUV, or the outline slides off the characters when the two differ (4K, upscaling).
float2 vp = GetViewportUV(Parameters);
float2 uv = ViewportUVToSceneTextureUV(vp, 25);
float3 col = SC.rgb;
// A stencil sample counts only where the character is actually visible (custom depth not behind the scene).
#define VISIBLE_STENCIL(p) (SceneTextureLookup(p, 13, false).r <= SceneTextureLookup(p, 1, false).r * 1.002 + 2.0 ? SceneTextureLookup(p, 25, false).r : 0.0)
float s0 = VISIBLE_STENCIL(uv);
// 1.5 render pixels at a 1080-line render, scaled with the render height: the same thickness on screen at any upscaling
// quality, and never under one render pixel (or neighbours are missed at low render scales).
float px = max(1.0, 1.5 * View.ViewSizeAndInvSize.y / 1080.0);
float2 o = View.BufferSizeAndInvSize.zw * px;
float2 offs[4] = { float2(o.x, 0), float2(-o.x, 0), float2(0, o.y), float2(0, -o.y) };
float nmax = 0;
for (int i = 0; i < 4; i++)
{
    nmax = max(nmax, VISIBLE_STENCIL(uv + offs[i]));
}
if (s0 < 0.5 && nmax > 0.5)
{
    bool enemy = nmax > 1.5;
    // Display colour here (after tonemapping): darken for friends, a deep red for enemies.
    col = enemy ? lerp(col, float3(0.62, 0.04, 0.03), 0.65) : col * 0.15;
}
return col;
"""


def _custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def outline_material():
    """Builds M_OutlinePost, or rebuilds it in place (its graph is replaced, the asset kept, so the post-process volumes in
    the valley, London and the arenas keep pointing at it)."""
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        m = unreal.load_asset(MATERIAL)
        MEL.delete_all_material_expressions(m)
    else:
        folder, name = MATERIAL.rsplit("/", 1)
        m = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    # After tonemapping: after the upscaler, at output resolution (sharp under TSR / DLSS, no reconstruction shimmer on
    # a 1-2 px line). Its lookups map viewport UVs to the render-resolution scene textures (OUTLINE_HLSL); without that,
    # outlines slid off the characters at 4K when the material first tried this location.
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    scene = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 0)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    # Referencing the stencil and custom depth makes the material bind those scene textures for the HLSL lookups.
    stencil = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 200)
    stencil.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    depth = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 350)
    depth.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    custom = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", OUTLINE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "Outline")
    custom.set_editor_property("inputs", [_custom_input(n) for n in ("SC", "CS", "CD")])
    MEL.connect_material_expressions(scene, "Color", custom, "SC")
    MEL.connect_material_expressions(stencil, "Color", custom, "CS")
    MEL.connect_material_expressions(depth, "Color", custom, "CD")
    if not MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("failed to connect Emissive on M_OutlinePost")
    errors = MEL.recompile_material(m)
    if errors:
        raise RuntimeError(f"M_OutlinePost failed to compile: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def apply_post(volume):
    """The realistic post settings and the outline on an unbound post-process volume."""
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    # ponytail: default auto-exposure; an EV 8-13 clamp rendered near-black here (luminance units, not EV100).
    # Reused volumes may still carry that clamp: switch it off explicitly.
    for key in ("auto_exposure_min_brightness", "auto_exposure_max_brightness"):
        settings.set_editor_property(f"override_{key}", False)
    for key, value in (("bloom_intensity", 0.5), ("motion_blur_amount", 0.15),
                       ("film_grain_intensity", 0.0), ("scene_fringe_intensity", 0.0), ("white_temp", 6000.0)):
        settings.set_editor_property(f"override_{key}", True)
        settings.set_editor_property(key, value)
    outline = unreal.load_asset(MATERIAL)
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(array=[unreal.WeightedBlendable(weight=1.0, object=outline)]))
    volume.set_editor_property("settings", settings)
