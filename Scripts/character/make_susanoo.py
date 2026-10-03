"""Builds Susanoo's ribcage figure and exports SourceArt/Props/susanoo.fbx.

Run: blender -b -P Scripts/character/make_susanoo.py

Itachi's first Susanoo stage, simplified: a spine, eight pairs of ribs round a sternum, shoulders, a skull and jaw, and two
long arms reaching forward with big hands. About 6 m tall, standing on the ground (the origin is between its feet) and
facing +X. Units are centimetres; the exporter scales by 0.01. One material slot: Unreal puts the glowing M_Susanoo on it.
Previews: Saved/SusanooPreview_front.png and _side.png.
"""
import math
import os

import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DST = os.path.join(ROOT, "SourceArt", "Props", "susanoo.fbx")
PREVIEW = os.path.join(ROOT, "Saved", "SusanooPreview")

bpy.ops.wm.read_factory_settings(use_empty=True)
parts = []
MATERIAL = bpy.data.materials.new("M_Susanoo_Slot")
MATERIAL.diffuse_color = (0.45, 0.22, 1.0, 1.0)


def keep(obj):
    obj.data.materials.append(MATERIAL)
    parts.append(obj)
    return obj


def sphere(centre, radius, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=radius, location=centre)
    obj = bpy.context.active_object
    obj.scale = scale
    return keep(obj)


def bone(a, b, radius, sides=10):
    a, b = Vector(a), Vector(b)
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=(b - a).length, location=(a + b) / 2)
    obj = bpy.context.active_object
    obj.rotation_euler = (b - a).to_track_quat("Z", "Y").to_euler()
    return keep(obj)


def rib(height, size, side):
    """One rib: from the spine round the side to the front, drooping a little."""
    curve = bpy.data.curves.new("rib", "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = 5.5 * size
    curve.bevel_resolution = 2
    spline = curve.splines.new("POLY")
    steps = 12
    spline.points.add(steps - 1)
    for i in range(steps):
        t = i / (steps - 1)
        a = math.pi * (1 - 0.88 * t)  # pi: at the spine, 0.12 pi: at the front
        spline.points[i].co = (10 + 55 * size * math.cos(a), side * 95 * size * math.sin(a), height - 28 * t, 1)
    obj = bpy.data.objects.new("rib", curve)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.convert(target="MESH")
    return keep(bpy.context.active_object)


# Spine, pelvis and two thick legs that stand it on the ground.
bone((-45, 0, 130), (-45, 0, 530), 10)
sphere((-25, 0, 140), 45, (0.8, 1.5, 0.55))
for side in (1, -1):
    bone((-25, side * 45, 150), (-20, side * 55, 25), 24)
    sphere((0, side * 55, 12), 30, (1.8, 0.9, 0.4))
# Ribs: eight levels, narrowing toward the shoulders, plus the sternum they meet at the front.
for level in range(8):
    height = 270 + level * 28
    size = 1.0 - 0.045 * level
    for side in (1, -1):
        rib(height, size, side)
bone((60, 0, 250), (45, 0, 480), 5)
# Shoulders, collar, skull, jaw.
for side in (1, -1):
    sphere((-10, side * 125, 505), 40)
    bone((-45, 0, 520), (-10, side * 125, 505), 9)
sphere((-5, 0, 590), 48, (1.0, 0.85, 1.15))
sphere((22, 0, 545), 30, (1.2, 0.9, 0.5))
# Arms reaching forward, each ending in a big hand.
for side in (1, -1):
    shoulder = (-10, side * 125, 505)
    elbow = (60, side * 160, 380)
    wrist = (160, side * 170, 330)
    bone(shoulder, elbow, 19)
    sphere(elbow, 17)
    bone(elbow, wrist, 15)
    sphere((185, side * 172, 320), 26, (1.4, 1.0, 0.8))

for obj in bpy.context.scene.objects:
    obj.select_set(obj in parts)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
figure = bpy.context.view_layer.objects.active
figure.name = "SM_Susanoo"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.shade_smooth()
xs = [v.co.x for v in figure.data.vertices]
zs = [v.co.z for v in figure.data.vertices]
print(f"[make_susanoo] {len(figure.data.polygons)} faces, X {min(xs):.0f}..{max(xs):.0f}, Z {min(zs):.0f}..{max(zs):.0f}")

# Previews (orthographic, workbench).
os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x, scene.render.resolution_y = 600, 800
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.1, 0.1, 0.13)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 760.0
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
centre = Vector((40, 0, 320))
for name, offset, rot in (("front", Vector((900, 0, 0)), (math.radians(90), 0, math.radians(90))), ("side", Vector((0, -900, 0)), (math.radians(90), 0, 0))):
    cam.location = centre + offset
    cam.rotation_euler = rot
    scene.render.filepath = f"{PREVIEW}_{name}.png"
    bpy.ops.render.render(write_still=True)

bpy.ops.object.select_all(action="DESELECT")
figure.select_set(True)
bpy.context.view_layer.objects.active = figure
bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, axis_forward="-Y", axis_up="Z", global_scale=0.01, apply_unit_scale=True, path_mode="COPY")
print("[make_susanoo] wrote " + DST)
