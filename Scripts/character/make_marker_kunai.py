"""Builds Teleport's marker kunai (the Hiraishin kunai) and exports SourceArt/Props/marker_kunai.fbx.

Run: blender -b -P Scripts/character/make_marker_kunai.py

Built from simple parts, a few hundred triangles: a leaf-shaped blade, two backward-swept barbs at its base (the third
and fourth "prongs" of Minato's kunai), a corded handle and a ring pommel. Units are centimetres; the blade tip is the
origin and points along +X (AShinobiMarkerKunai flies along +X, so the blade leads). Previews are rendered to
Saved/KunaiPreview_side.png and _top.png.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DST = os.path.join(ROOT, "SourceArt", "Props", "marker_kunai.fbx")
PREVIEW = os.path.join(ROOT, "Saved", "KunaiPreview")

bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, color, metallic, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    mat.diffuse_color = (*color, 1.0)
    return mat


STEEL = material("M_MarkerKunai_Steel", (0.30, 0.32, 0.36), 0.9, 0.3)
CORD = material("M_MarkerKunai_Cord", (0.035, 0.03, 0.03), 0.0, 0.8)


def flat_part(name, outline, thickness, mat):
    """A polygon outline in the XY plane (a list of (x, y)), given thickness along Z, centred on Z = 0."""
    mesh = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    verts = [bm.verts.new((x, y, 0.0)) for x, y in outline]
    bm.faces.new(verts)
    bm.to_mesh(mesh)
    bm.free()
    solid = obj.modifiers.new("solid", "SOLIDIFY")
    solid.thickness = thickness
    solid.offset = 0.0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier="solid")
    obj.data.materials.append(mat)
    return obj


parts = []

# Blade: tip at the origin, widest about a third of the way back, then tapering to the neck.
blade = [(0, 0), (-5, 1.7), (-11, 2.9), (-17, 2.3), (-22, 1.0), (-22, -1.0), (-17, -2.3), (-11, -2.9), (-5, -1.7)]
parts.append(flat_part("blade", blade, 0.35, STEEL))

# Barbs: from the blade's base, sweeping forward and out like a three-pronged Hiraishin kunai.
for side in (1, -1):
    barb = [(-21.5, 0.6 * side), (-18, 3.4 * side), (-11, 7.4 * side), (-9.5, 7.6 * side), (-13.5, 3.4 * side), (-19, 0.9 * side)]
    parts.append(flat_part("barb_%d" % side, barb, 0.3, STEEL))

# Handle: a tapered, corded cylinder from the neck back.
bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=1.25, depth=20.0, rotation=(0, math.radians(90), 0), location=(-32.0, 0, 0))
handle = bpy.context.active_object
handle.name = "handle"
handle.data.materials.append(CORD)
parts.append(handle)

# Collar between blade and handle.
bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=1.8, depth=1.6, rotation=(0, math.radians(90), 0), location=(-22.4, 0, 0))
collar = bpy.context.active_object
collar.name = "collar"
collar.data.materials.append(STEEL)
parts.append(collar)

# Ring pommel, in the blade's plane.
bpy.ops.mesh.primitive_torus_add(major_radius=3.4, minor_radius=0.65, major_segments=18, minor_segments=6, location=(-45.0, 0, 0))
ring = bpy.context.active_object
ring.name = "ring"
ring.data.materials.append(STEEL)
parts.append(ring)

for obj in bpy.context.scene.objects:
    obj.select_set(obj in parts)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
kunai = bpy.context.view_layer.objects.active
kunai.name = "SM_MarkerKunai"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.shade_smooth()
print(f"[make_kunai] {len(kunai.data.polygons)} faces, length {max(v.co.x for v in kunai.data.vertices) - min(v.co.x for v in kunai.data.vertices):.1f} cm")

# Previews.
os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x, scene.render.resolution_y = 900, 360
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.15, 0.15, 0.17)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 62.0
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
centre = Vector((-24.0, 0, 0))
for name, offset, rot in (("top", Vector((0, 0, 100)), (0, 0, 0)), ("side", Vector((0, -100, 0)), (math.radians(90), 0, 0))):
    cam.location = centre + offset
    cam.rotation_euler = rot
    scene.render.filepath = f"{PREVIEW}_{name}.png"
    bpy.ops.render.render(write_still=True)

bpy.ops.object.select_all(action="DESELECT")
kunai.select_set(True)
bpy.context.view_layer.objects.active = kunai
bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, axis_forward="-Y", axis_up="Z", global_scale=0.01, apply_unit_scale=True, path_mode="COPY")  # 1 Blender unit is 1 cm here, a metre to the exporter
print("[make_kunai] wrote " + DST)
