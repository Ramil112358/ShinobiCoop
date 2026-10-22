"""Dresses Lvl_MainMenu: the menu stage (AShinobiMenuStage) on a London rooftop at dusk, the city behind it.

Idempotent: deletes every actor tagged ShinobiMenu and places them again. Committed content only (London city tiles and
PolyHaven props under /Game/Shinobi/London). Close the GUI editor, then:
    powershell -File Scripts/run_python.ps1 -Script Scripts/setup_menu.py

The stage stands at the origin facing +X; its camera looks back along -X, so everything behind the character is at -X.
"""
import unreal

MAP = "/Game/Shinobi/Maps/Lvl_MainMenu"
TAG = "ShinobiMenu"
LONDON = "/Game/Shinobi/London"
# City tiles (Scripts/london): world-space chunks of buildings, ~160 m across, pivot at the London origin.
TILES = [(x, y) for x in (-3, -2, -1) for y in (-2, -1, 0, 1)]
# The tiles' patch sits this far behind the stage and this far down, so the rooftop looks over most roofs.
CITY_GAP_CM = 2500.0
CITY_DROP_CM = 3200.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log("[setup_menu] " + msg)


def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0), label=None):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), rot)
    a.tags = [TAG]
    if label:
        a.set_actor_label(label)
    return a


def mesh_actor(path, loc, rot=unreal.Rotator(0, 0, 0), scale=(1, 1, 1), material=None, label=None):
    mesh = unreal.load_asset(path)
    if not mesh:
        log("missing " + path)
        return None
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    comp = a.static_mesh_component
    comp.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        mat = unreal.load_asset(material)
        for i in range(comp.get_num_materials()):
            comp.set_material(i, mat)
    return a


unreal.EditorLoadingAndSavingUtils.load_map(MAP)
for a in actors.get_all_level_actors():
    if TAG in [str(t) for t in a.tags]:
        actors.destroy_actor(a)

stage_class = unreal.load_class(None, "/Script/ShinobiCoop.ShinobiMenuStage")
spawn(stage_class, (0, 0, 0), label="MenuStage")

# Rooftop: a flat roof slab, a low parapet behind the character, rooftop clutter at the sides.
CUBE = "/Engine/BasicShapes/Cube.Cube"
ROOF = LONDON + "/Materials/MI_L_BD_Roof_Flat.MI_L_BD_Roof_Flat"
PARAPET = LONDON + "/Materials/MI_L_BD_Brick_Stock.MI_L_BD_Brick_Stock"
mesh_actor(CUBE, (-200, 0, -50), scale=(24, 30, 1), material=ROOF, label="Roof")
mesh_actor(CUBE, (-1350, 0, 40), scale=(0.4, 30, 0.8), material=PARAPET, label="Parapet")
PH = LONDON + "/PolyHaven"
mesh_actor(PH + "/exterior_aircon_unit/exterior_aircon_unit_2k/StaticMeshes/exterior_aircon_unit_rusted.exterior_aircon_unit_rusted",
           (-700, 520, 0), unreal.Rotator(0, 0, 20), scale=(1.4, 1.4, 1.4), label="AirCon")
mesh_actor(PH + "/exterior_aircon_unit/exterior_aircon_unit_2k/StaticMeshes/exterior_aircon_unit.exterior_aircon_unit",
           (-900, 330, 0), unreal.Rotator(0, 0, 5), scale=(1.4, 1.4, 1.4), label="AirCon2")
mesh_actor(PH + "/utility_box_02/utility_box_02_2k/StaticMeshes/utility_box_02_2k.utility_box_02_2k",
           (-800, -560, 0), unreal.Rotator(0, 0, -30), label="UtilityBox")
mesh_actor(PH + "/street_lamp_01/street_lamp_01_2k/StaticMeshes/street_lamp_01_2k.street_lamp_01_2k",
           (-1250, -380, 0), label="Lamp")

# The city behind: the tiles' joint bounds put just past the parapet, centred on the stage, dropped below the roof.
meshes = []
lo = [1e12, 1e12]
hi = [-1e12, -1e12]
for x, y in TILES:
    name = "B_%d_%d" % (x, y)
    m = unreal.load_asset("%s/City/B/%s/StaticMeshes/%s.%s" % (LONDON, name, name, name))
    if not m:
        continue
    b = m.get_bounding_box()
    lo = [min(lo[0], b.min.x), min(lo[1], b.min.y)]
    hi = [max(hi[0], b.max.x), max(hi[1], b.max.y)]
    meshes.append((name, m))
offset = (-CITY_GAP_CM - 1350 - hi[0], -(lo[1] + hi[1]) / 2, -CITY_DROP_CM)
for name, m in meshes:
    a = spawn(unreal.StaticMeshActor, offset, label="City_" + name)
    a.static_mesh_component.set_static_mesh(m)
log("%d city tiles, offset %s" % (len(meshes), offset))

# Dusk: a low sun behind the character (rim light, sunset sky), atmosphere, fog, a warm key light from the front.
sun = spawn(unreal.DirectionalLight, (0, 0, 800), unreal.Rotator(0, -7, 0), "Sun")  # (roll, pitch, yaw): light travels +X
sc = sun.light_component
sc.set_intensity(6.0)
sc.set_light_color(unreal.LinearColor(1.0, 0.62, 0.38))
sc.set_editor_property("atmosphere_sun_light", True)
spawn(unreal.SkyAtmosphere, (0, 0, 0), label="Sky")
sky = spawn(unreal.SkyLight, (0, 0, 300), label="SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_intensity(1.2)
fog = spawn(unreal.ExponentialHeightFog, (0, 0, -3000), label="Fog")
fc = fog.component
fc.set_editor_property("fog_density", 0.012)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.9, 0.42, 0.2))
fc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(1.0, 0.55, 0.3))
key = spawn(unreal.RectLight, (260, 170, 210), unreal.Rotator(0, -25, -150), "KeyLight")
key.light_component.set_intensity(18.0)
key.light_component.set_light_color(unreal.LinearColor(1.0, 0.9, 0.8))
key.light_component.set_attenuation_radius(1200.0)
fill = spawn(unreal.PointLight, (-160, -220, 160), label="RimCool")
fill.light_component.set_intensity(6.0)
fill.light_component.set_light_color(unreal.LinearColor(0.45, 0.6, 1.0))
fill.light_component.set_attenuation_radius(500.0)
pp = spawn(unreal.PostProcessVolume, (0, 0, 0), label="Look")
pp.set_editor_property("unbound", True)
s = pp.settings
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 10.0)
s.set_editor_property("override_vignette_intensity", True)
s.set_editor_property("vignette_intensity", 0.6)
pp.set_editor_property("settings", s)

unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), MAP)
log("saved " + MAP)
