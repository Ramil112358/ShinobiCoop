"""M5 look & feel assets. Idempotent (rebuilds everything it owns); close the GUI editor and build the C++ first.

1. M_ShinobiSolid (flat colour, Substrate) + one instance per arena colour.
2. M_ToonPost: the toon post-process. One Custom HLSL node at Scene Color After DOF (linear HDR, before tonemapping).
   Its input excludes after-DOF translucency, where every jutsu glow renders (Scripts/setup_vfx.py), so the remap
   can't flatten the glows away (it did when it ran after tonemapping):
   - lighting = scene colour / diffuse colour, posterised into 3 soft bands and re-multiplied (pixels with no diffuse,
     i.e. emissive VFX, and the sky are left alone);
   - outlines from a cross-shaped depth + normal edge test;
   - a slight saturation boost.
   Tuning lives in the HLSL constants below; re-run to apply.
3. Lvl_TrainingGrounds: the M5 arena, from engine primitives. An 80 m grass clearing ringed by trees and rocks (an
   invisible wall just inside them), a torii gate, training posts, cover rocks, sun + sky + fog, the toon post-process
   volume, a PlayerStart in the centre, 4 ShinobiEnemySpawn target points and a NavMesh bounds volume (the navmesh
   itself is generated at runtime: RecastNavMesh RuntimeGeneration=Dynamic).
4. Lvl_Combat: removes the template's enemy spawners (the map picker offers it) and adds the toon volume there and on
   Lvl_ThirdPerson.
5. Imports the placeholder sounds from SourceArt/Audio (Scripts/make_sfx.py) as /Game/Shinobi/Audio/SFX_*.
"""
import math
import os
import random
import unreal

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAT_DIR = "/Game/Shinobi/Materials"
MAP = "/Game/Shinobi/Maps/Lvl_TrainingGrounds"
AUDIO_DIR = "/Game/Shinobi/Audio"
TOON_VOLUME_LABEL = "ShinobiToonPost"

COLORS = {
    "Grass": (0.20, 0.45, 0.12),
    "Dirt": (0.36, 0.26, 0.15),
    "Bark": (0.22, 0.13, 0.07),
    "Leaves": (0.06, 0.26, 0.08),
    "LeavesLight": (0.13, 0.36, 0.10),
    "Rock": (0.38, 0.38, 0.40),
    "Wood": (0.50, 0.33, 0.18),
    "ToriiRed": (0.75, 0.10, 0.05),
    "Black": (0.04, 0.04, 0.04),
}

TOON_HLSL = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float3 sc = SC.rgb;
float d0 = SceneTextureLookup(uv, 1, false).r;
float3 dc = SceneTextureLookup(uv, 2, false).rgb;
float3 n0 = SceneTextureLookup(uv, 8, false).rgb;
const float SkyDepth = 500000.0;
float3 col = sc;

// Toon bands: split the lighting out (scene / diffuse), posterise it into shadow / mid / lit, re-multiply. Linear,
// pre-exposed light levels measured in the Training Grounds: shadow ~0.15-0.3, shaded side ~0.5-1, sunlit ~2-2.6.
float dmax = max(max(dc.r, dc.g), dc.b);
if (d0 < SkyDepth && dmax > 0.03)
{
    float3 light = sc / max(dc, 0.03);
    float l = dot(light, float3(0.3, 0.59, 0.11));
    float band = 0.3 + 0.55 * smoothstep(0.3, 0.4, l) + 0.85 * smoothstep(1.1, 1.4, l);
    col = dc * band * lerp(1.0, light / max(l, 0.001), 0.35); // keep a little of the light's tint
}

// Outlines: this pixel is on an edge if a neighbour is much farther (silhouette) or faces another way (crease).
if (d0 < SkyDepth)
{
    float2 o = Texel * 1.5;
    float2 offs[4] = { float2(o.x, 0), float2(-o.x, 0), float2(0, o.y), float2(0, -o.y) };
    float edge = 0;
    for (int i = 0; i < 4; i++)
    {
        float di = SceneTextureLookup(uv + offs[i], 1, false).r;
        float3 ni = SceneTextureLookup(uv + offs[i], 8, false).rgb;
        float depthEdge = saturate(((di - d0) / max(d0, 1.0) - 0.06) * 12.0);
        float normalEdge = abs(di - d0) / max(d0, 1.0) < 0.05 ? saturate((0.75 - dot(n0, ni)) * 4.0) : 0;
        edge = max(edge, max(depthEdge, normalEdge));
    }
    edge *= saturate(1.6 - d0 / 5000.0); // fade out with distance so far detail doesn't turn to noise
    col = lerp(col, col * 0.12, edge);
}

float lum = dot(col, float3(0.3, 0.59, 0.11));
return lerp(lum.xxx, col, 1.15);
"""


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(src, dst, dst_input, src_output=""):
    if not MEL.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError(f"failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")


def fresh(name, cls, factory):
    path = f"{MAT_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
        raise RuntimeError(f"could not delete {path}")
    asset = TOOLS.create_asset(name, MAT_DIR, cls, factory)
    if asset is None:
        raise RuntimeError(f"failed to create {path}")
    return asset


def compile_and_save(material):
    errors = MEL.recompile_material(material)
    if errors:
        raise RuntimeError(f"{material.get_name()} failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"failed to save {material.get_name()}")
    unreal.log(f"[setup_m5] built {material.get_path_name()}")


def build_solid_materials():
    # Instances first: deleting the parent while instances reference it would fail.
    for name in COLORS:
        path = f"{MAT_DIR}/MI_{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
    m = fresh("M_ShinobiSolid", unreal.Material, unreal.MaterialFactoryNew())
    color = node(m, unreal.MaterialExpressionVectorParameter, -600, 0, parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    rough = node(m, unreal.MaterialExpressionConstant, -600, 200, r=0.85)
    spec = node(m, unreal.MaterialExpressionConstant, -600, 300, r=0.3)
    shading = node(m, unreal.MaterialExpressionSubstrateShadingModels, -250, 0)
    link(color, shading, "BaseColor")
    link(rough, shading, "Roughness")
    link(spec, shading, "Specular")
    if not MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL):
        raise RuntimeError("failed to connect Front Material on M_ShinobiSolid")
    compile_and_save(m)
    instances = {}
    for name, (r, g, b) in COLORS.items():
        mi = TOOLS.create_asset(f"MI_{name}", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, m)
        MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1))
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        instances[name] = mi
    return instances


def custom_input(name):
    value = unreal.CustomInput()
    value.set_editor_property("input_name", name)
    return value


def build_toon_post():
    m = fresh("M_ToonPost", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    scene = node(m, unreal.MaterialExpressionSceneTexture, -700, 0, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    texel = node(m, unreal.MaterialExpressionSceneTexelSize, -700, 200)
    custom = node(m, unreal.MaterialExpressionCustom, -300, 0, code=TOON_HLSL, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  description="Toon", inputs=[custom_input("SC"), custom_input("Texel")])
    link(scene, custom, "SC", "Color")
    link(texel, custom, "Texel")
    if not MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("failed to connect Emissive on M_ToonPost")
    compile_and_save(m)
    return m


def spawn_mesh(mesh, mi, location, scale, yaw=0.0, label=None):
    # spawn_actor_from_object returns None in the pythonscript commandlet; spawning the class works.
    actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, yaw))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.static_mesh_component.set_material(0, mi)
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_class(cls, location, yaw=0.0, pitch=0.0):
    return EAS.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(0, pitch, yaw))


def add_toon_volume(toon):
    # Reuse the volume from an earlier run: recreating it would churn world-partition external actor files.
    existing = [a for a in EAS.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label() == TOON_VOLUME_LABEL]
    volume = existing[0] if existing else spawn_class(unreal.PostProcessVolume, (0, 0, 0))
    volume.set_actor_label(TOON_VOLUME_LABEL)
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(array=[unreal.WeightedBlendable(weight=1.0, object=toon)]))
    volume.set_editor_property("settings", settings)


def build_arena(mi, toon):
    if unreal.EditorAssetLibrary.does_asset_exist(MAP) and not unreal.EditorAssetLibrary.delete_asset(MAP):
        raise RuntimeError(f"could not delete {MAP}")
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    random.seed(7)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    cone = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
    # Engine basic shapes are 100 units across, centred on their origin.

    spawn_mesh(cylinder, mi["Grass"], (0, 0, -10), (240, 240, 0.2), label="Ground")
    spawn_mesh(cylinder, mi["Dirt"], (0, 0, -8.5), (22, 22, 0.2), label="TrainingCircle")

    # Tree and rock ring (the clearing is ~80 m across).
    for i in range(72):
        angle = 2 * math.pi * i / 72 + random.uniform(-0.03, 0.03)
        radius = random.uniform(3900, 4700)
        x, y = math.cos(angle) * radius, math.sin(angle) * radius
        height = random.uniform(4.5, 7.0)
        spawn_mesh(cylinder, mi["Bark"], (x, y, height * 50), (0.6, 0.6, height))
        leaves = mi["Leaves"] if i % 3 else mi["LeavesLight"]
        spawn_mesh(cone, leaves, (x, y, height * 100 + 150), (4.2, 4.2, 5.0))
        spawn_mesh(cone, leaves, (x, y, height * 100 + 450), (3.0, 3.0, 4.0))
        if i % 2 == 0:
            ra = angle + math.pi / 72
            rr = radius - 350
            spawn_mesh(sphere, mi["Rock"], (math.cos(ra) * rr, math.sin(ra) * rr, 40),
                       (random.uniform(2.5, 4.0), random.uniform(2.0, 3.5), random.uniform(1.5, 2.5)), yaw=random.uniform(0, 360))

    # Invisible wall just inside the ring, so nobody wanders into the trees.
    segments, wall_radius = 36, 3650
    length = 2 * math.pi * wall_radius / segments * 1.05
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        wall = spawn_mesh(cube, mi["Rock"], (math.cos(angle) * wall_radius, math.sin(angle) * wall_radius, 500),
                          (0.5, length / 100, 10), yaw=math.degrees(angle))
        wall.set_actor_hidden_in_game(True)
        wall.static_mesh_component.set_collision_profile_name("InvisibleWall")
        wall.set_actor_label(f"Wall{i:02d}")

    # Torii gate on the north edge, facing the centre.
    gx = 3000
    for side in (-350, 350):
        spawn_mesh(cylinder, mi["ToriiRed"], (gx, side, 275), (0.6, 0.6, 5.5))
    spawn_mesh(cube, mi["Black"], (gx, 0, 572), (0.9, 10.5, 0.45), label="ToriiKasagi")
    spawn_mesh(cube, mi["ToriiRed"], (gx, 0, 470), (0.5, 8.5, 0.35), label="ToriiNuki")

    # Training posts to the south.
    for i, y in enumerate((-600, -300, 0, 300, 600)):
        spawn_mesh(cylinder, mi["Wood"], (-1500, y, 85), (0.35, 0.35, 1.7), label=f"Post{i}")
        spawn_mesh(cube, mi["Wood"], (-1500, y, 130), (0.12, 0.9, 0.12))

    # Cover rocks inside the clearing.
    for i in range(9):
        angle = 2 * math.pi * i / 9 + random.uniform(-0.2, 0.2)
        radius = random.uniform(1300, 2600)
        spawn_mesh(sphere, mi["Rock"], (math.cos(angle) * radius, math.sin(angle) * radius, 60),
                   (random.uniform(2.5, 4.0), random.uniform(2.0, 3.0), random.uniform(1.8, 2.6)), yaw=random.uniform(0, 360))

    # Gameplay: player start in the middle facing the torii, enemy spawn points on the perimeter, navmesh bounds.
    spawn_class(unreal.PlayerStart, (0, 0, 100))
    for i in range(4):
        angle = math.radians(45 + 90 * i)
        point = spawn_class(unreal.TargetPoint, (math.cos(angle) * 3000, math.sin(angle) * 3000, 100))
        point.set_editor_property("tags", [unreal.Name("ShinobiEnemySpawn")])
        point.set_actor_label(f"EnemySpawn{i}")
    nav = spawn_class(unreal.NavMeshBoundsVolume, (0, 0, 300))
    nav.set_actor_scale3d(unreal.Vector(40, 40, 6))
    origin, extent = nav.get_actor_bounds(False)
    unreal.log(f"[setup_m5] nav bounds extent {extent}")

    # Sky, sun, fog, toon look.
    sun = spawn_class(unreal.DirectionalLight, (0, 0, 1000), yaw=-30, pitch=-50)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)
    spawn_class(unreal.SkyAtmosphere, (0, 0, 0))
    sky = spawn_class(unreal.SkyLight, (0, 0, 500))
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn_class(unreal.ExponentialHeightFog, (0, 0, 0))
    spawn_class(unreal.VolumetricCloud, (0, 0, 0))
    add_toon_volume(toon)

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/ShinobiCoop.ShinobiArenaGameMode"))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
    unreal.log(f"[setup_m5] built {MAP}")


def update_template_map(path, toon, remove_spawners):
    world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    if world is None:
        raise RuntimeError(f"failed to load {path}")
    if remove_spawners:
        for actor in EAS.get_all_level_actors():
            if "CombatEnemySpawner" in actor.get_class().get_name():
                unreal.log(f"[setup_m5] {path}: removing {actor.get_actor_label()}")
                EAS.destroy_actor(actor)
    add_toon_volume(toon)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
        raise RuntimeError(f"failed to save {path}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)  # world-partition external actors
    unreal.log(f"[setup_m5] updated {path}")


def import_sounds():
    source = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Audio")
    if not os.path.isdir(source):
        unreal.log_warning("[setup_m5] no SourceArt/Audio (run Scripts/make_sfx.py); skipping sounds")
        return
    tasks = []
    for name in sorted(os.listdir(source)):
        if name.lower().endswith(".wav"):
            task = unreal.AssetImportTask()
            task.filename = os.path.join(source, name)
            task.destination_path = AUDIO_DIR
            task.automated = True
            task.save = True
            task.replace_existing = True
            tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    unreal.log(f"[setup_m5] imported {len(tasks)} sounds into {AUDIO_DIR}")


for directory in (MAT_DIR, "/Game/Shinobi/Maps"):
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        unreal.EditorAssetLibrary.make_directory(directory)
instances = build_solid_materials()
toon_material = build_toon_post()
build_arena(instances, toon_material)
update_template_map("/Game/Variant_Combat/Lvl_Combat", toon_material, remove_spawners=True)
update_template_map("/Game/ThirdPerson/Lvl_ThirdPerson", toon_material, remove_spawners=False)
import_sounds()
