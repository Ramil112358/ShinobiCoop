"""Blender (5.2) headless prep of the guard ribcage GLB (Tripo or Rodin) for Scripts/setup_susanoo_ribs.py.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/prep_susanoo_ribs.py -- <in.glb> [height_cm] [clear_radius_cm] [width_cm]

Steps: import -> join -> apply transforms -> front (-Y in Blender, as glTF exports it) turned to +X -> scale to height_cm
(default 175: the top stays below the player's head) -> feet at z = 0, centred -> delete faces inside a vertical cylinder
of clear_radius_cm (default 0.165 x height) round the axis between the shins and the head, so the player fits inside and
shows through -> drop specks -> squeeze X and Y alike to width_cm across (default 100: close round the 42 cm capsule) -> decimate to
20k triangles -> one material slot -> SourceArt/Props/susanoo_ribs.fbx (centimetres; the exporter scales by 0.01) and
Saved/SusanooRibsPreview_front.png / _side.png.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DST = os.path.join(ROOT, "SourceArt", "Props", "susanoo_ribs.fbx")
PREVIEW = os.path.join(ROOT, "Saved", "SusanooRibsPreview")
MAX_TRIS = 20_000
SPECK_FACES = 30


def fail(message):
    print(f"[prep_susanoo_ribs] ERROR: {message}")
    sys.exit(1)


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not args:
    fail("usage: -- <in.glb> [height_cm] [clear_radius_cm] [width_cm]")
src = args[0]
height = float(args[1]) if len(args) > 1 else 175.0
clear = float(args[2]) if len(args) > 2 else 0.165 * height  # the core the generator fills in, at its own proportions
width = float(args[3]) if len(args) > 3 else 100.0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    fail(f"no mesh in {src}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj in meshes)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
rig = bpy.context.view_layer.objects.active
rig.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Work in centimetres: Blender metres x 100, the exporter's 0.01 brings it back.
mesh = rig.data
mesh.transform(Matrix.Rotation(math.radians(90.0), 4, "Z"))  # front -Y -> +X
zs = [v.co.z for v in mesh.vertices]
mesh.transform(Matrix.Scale(height / (max(zs) - min(zs)), 4))
xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
mesh.transform(Matrix.Translation(Vector((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs)))))

bm = bmesh.new()
bm.from_mesh(mesh)
# Hollow it: anything inside the player's own space goes (generators tend to fill the cage with a body or a solid core).
inside = []
for face in bm.faces:
    centre = face.calc_center_median()
    if math.hypot(centre.x, centre.y) < clear and 25.0 < centre.z < height - 15.0:
        inside.append(face)
bmesh.ops.delete(bm, geom=inside, context="FACES")
# Specks: tiny islands left over from the cut.
seen = set()
for face in list(bm.faces):
    if face in seen or not face.is_valid:
        continue
    island, stack = [], [face]
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        island.append(f)
        stack.extend(n for e in f.edges for n in e.link_faces if n not in seen)
    if len(island) < SPECK_FACES:
        bmesh.ops.delete(bm, geom=island, context="FACES")
bm.to_mesh(mesh)
bm.free()
mesh.update()
print(f"[prep_susanoo_ribs] cut {len(inside)} faces inside a {clear:.0f} cm radius")

# Then hug the body: the cut works on the model's own proportions, the squeeze brings the ribs in.
ys = [v.co.y for v in mesh.vertices]
squeeze = width / (max(ys) - min(ys))
mesh.transform(Matrix.Diagonal(Vector((squeeze, squeeze, 1.0, 1.0))))

tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
if tris > MAX_TRIS:
    mod = rig.modifiers.new("decimate", "DECIMATE")
    mod.ratio = MAX_TRIS / tris
    bpy.ops.object.modifier_apply(modifier=mod.name)
mesh.materials.clear()
mesh.materials.append(bpy.data.materials.new("M_SusanooRibs_Slot"))
rig.name = "SM_SusanooRibs"
bpy.ops.object.shade_smooth()

xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
print(f"[prep_susanoo_ribs] {sum(len(p.vertices) - 2 for p in mesh.polygons)} tris, X {min(xs):.0f}..{max(xs):.0f}, Y {min(ys):.0f}..{max(ys):.0f}, Z {min(zs):.0f}..{max(zs):.0f}")

os.makedirs(os.path.dirname(DST), exist_ok=True)
for obj in bpy.context.scene.objects:
    obj.select_set(obj == rig)
# The same settings as make_susanoo.py, whose SM_Susanoo faces +X in Unreal.
bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, axis_forward="-Y", axis_up="Z", global_scale=0.01, apply_unit_scale=True, path_mode="COPY")

os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x, scene.render.resolution_y = 600, 800
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.1, 0.1, 0.13)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = height * 1.3
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
centre = Vector((0, 0, height / 2))
for name, offset, rot in (("front", Vector((900, 0, 0)), (math.radians(90), 0, math.radians(90))), ("side", Vector((0, -900, 0)), (math.radians(90), 0, 0))):
    cam.location = centre + offset
    cam.rotation_euler = rot
    scene.render.filepath = f"{PREVIEW}_{name}.png"
    bpy.ops.render.render(write_still=True)
