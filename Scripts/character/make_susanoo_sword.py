"""Builds the Perfect Susanoo's chakra sword and exports SourceArt/Susanoo/susanoo_sword.fbx.

Run: blender -b -P Scripts/character/make_susanoo_sword.py

A long straight blade with jagged, chipped edges like the swords in Ultimate Ninja Storm 4: a diamond cross-section (sharp
edges, a ridge down the middle), wide near the fist and narrowing to a point, plus a short grip. Sized for the 180 cm Susanoo
skeleton it is held by (the figure is scaled up in Unreal, and the sword with it): the origin is the middle of the grip, the
blade points up +Z. Units are centimetres; the exporter scales by 0.01. One material slot (Unreal puts M_SusanooBlade on it).
Preview: Saved/SusanooSwordPreview.png.
"""
import math
import os
import random

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DST = os.path.join(ROOT, "SourceArt", "Susanoo", "susanoo_sword.fbx")
PREVIEW = os.path.join(ROOT, "Saved", "SusanooSwordPreview.png")

BLADE_START = 9.0  # cm above the grip's middle: where the blade leaves the fist
BLADE_LENGTH = 110.0
STEPS = 60
TEETH = 11  # chips along each edge
random.seed(7)  # the same sword every run
CHIP_DEPTHS = [random.uniform(0.15, 0.5) for _ in range(TEETH)]  # each chip a different size: crystal, not a saw

bpy.ops.wm.read_factory_settings(use_empty=True)
material = bpy.data.materials.new("M_SusanooBlade_Slot")
material.diffuse_color = (0.2, 0.75, 1.0, 1.0)


def half_width(t, phase):
    """Half the blade's width at t (0 at the fist, 1 at the tip) on one edge: a taper with sawtooth chips cut into it."""
    # Nearly full width (13 -> 10 cm) for most of the length, then a short point over the last 12%.
    taper = 13.0 - 3.0 * t if t < 0.88 else 10.36 * ((1.0 - t) / 0.12) ** 0.8
    tooth = (t * TEETH + phase) % 1.0  # 0..1 inside one chip
    depth = CHIP_DEPTHS[int(t * TEETH + phase) % TEETH]
    chip = 1.0 + depth * tooth if tooth < 0.75 else 1.0 + depth - 4.0 * depth * (tooth - 0.75)
    return taper * chip


verts, faces = [], []
for i in range(STEPS + 1):
    t = i / STEPS
    z = BLADE_START + BLADE_LENGTH * t
    if i == STEPS:
        verts += [(0.0, 0.0, z + 6.0)] * 4  # the tip: all four corners meet
        continue
    ridge = 2.2 * (1.0 - 0.8 * t)
    jitter = random.uniform(-0.4, 0.4)
    verts += [
        (-half_width(t, 0.0) + jitter, 0.0, z),  # left edge
        (0.0, ridge, z),  # front ridge
        (half_width(t, 0.45) - jitter, 0.0, z),  # right edge
        (0.0, -ridge, z),  # back ridge
    ]
for i in range(STEPS):
    a, b = 4 * i, 4 * (i + 1)
    for k in range(4):
        faces.append((a + k, a + (k + 1) % 4, b + (k + 1) % 4, b + k))
faces.append((3, 2, 1, 0))  # close the blade's base inside the fist

mesh = bpy.data.meshes.new("SM_SusanooSword")
mesh.from_pydata(verts, [], faces)
mesh.validate()
blade = bpy.data.objects.new("SM_SusanooSword", mesh)
bpy.context.scene.collection.objects.link(blade)
bpy.context.view_layer.objects.active = blade
blade.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.remove_doubles(threshold=0.01)  # merge the tip's four corners
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.object.mode_set(mode="OBJECT")
blade.data.materials.append(material)

# The grip: a short octagonal rod through the fist, ending in a small pommel.
bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=2.6, depth=22.0, location=(0, 0, -2.0))
grip = bpy.context.active_object
bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=6, radius=3.6, location=(0, 0, -14.0))
pommel = bpy.context.active_object
for part in (grip, pommel):
    part.data.materials.append(material)
for obj in bpy.context.scene.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = blade
bpy.ops.object.join()
sword = bpy.context.view_layer.objects.active
sword.name = "SM_SusanooSword"
bpy.ops.object.shade_flat()  # faceted crystal, not smooth
zs = [v.co.z for v in sword.data.vertices]
print(f"[make_susanoo_sword] {len(sword.data.polygons)} faces, Z {min(zs):.0f}..{max(zs):.0f}")

# Preview (orthographic, workbench), looking at the blade's flat.
os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x, scene.render.resolution_y = 300, 900
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.1, 0.1, 0.13)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 150.0
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
cam.location = (0, -400, 55)
cam.rotation_euler = (math.radians(90), 0, 0)
scene.render.filepath = PREVIEW
bpy.ops.render.render(write_still=True)

os.makedirs(os.path.dirname(DST), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, axis_forward="-Y", axis_up="Z", global_scale=0.01, apply_unit_scale=True, path_mode="COPY", mesh_smooth_type="FACE")
print("[make_susanoo_sword] wrote " + DST)
