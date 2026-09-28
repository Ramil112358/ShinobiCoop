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
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float3 col = SC.rgb;
// A stencil sample counts only where the character is actually visible (custom depth not behind the scene).
#define VISIBLE_STENCIL(p) (SceneTextureLookup(p, 13, false).r <= SceneTextureLookup(p, 1, false).r * 1.002 + 2.0 ? SceneTextureLookup(p, 25, false).r : 0.0)
float s0 = VISIBLE_STENCIL(uv);
float px = 1.5 * max(1.0, (1.0 / Texel.y) / 1080.0); // 1.5 px at 1080p, scaled with resolution
float2 o = Texel * px;
float2 offs[4] = { float2(o.x, 0), float2(-o.x, 0), float2(0, o.y), float2(0, -o.y) };
float nmax = 0;
for (int i = 0; i < 4; i++)
{
    nmax = max(nmax, VISIBLE_STENCIL(uv + offs[i]));
}
if (s0 < 0.5 && nmax > 0.5)
{
    bool enemy = nmax > 1.5;
    // Linear HDR here (before tonemapping): darken for friends, tint toward a deep red for enemies.
    col = enemy ? lerp(col, float3(0.5, 0.02, 0.01) * max(dot(col, float3(0.3, 0.59, 0.11)), 0.05) * 2.0, 0.6) : col * 0.12;
}
return col;
"""


def _custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def outline_material():
    """Builds (or rebuilds) M_OutlinePost."""
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        unreal.EditorAssetLibrary.delete_asset(MATERIAL)
    folder, name = MATERIAL.rsplit("/", 1)
    m = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    # Before the upscaler (TSR): scene colour, depth and stencil then share the render resolution. After tonemapping
    # the colour is at output resolution while the stencil is not, which shifted outlines off the characters at 4K.
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    scene = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 0)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    # Referencing the stencil and custom depth makes the material bind those scene textures for the HLSL lookups.
    stencil = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 200)
    stencil.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    depth = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -700, 350)
    depth.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    texel = MEL.create_material_expression(m, unreal.MaterialExpressionSceneTexelSize, -700, 500)
    custom = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", OUTLINE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "Outline")
    custom.set_editor_property("inputs", [_custom_input(n) for n in ("SC", "Texel", "CS", "CD")])
    MEL.connect_material_expressions(scene, "Color", custom, "SC")
    MEL.connect_material_expressions(texel, "", custom, "Texel")
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
