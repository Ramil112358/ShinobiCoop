"""Blender (5.2) headless prep of a Tripo Studio cloak FBX for skin_outfit.py (docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/prep_outfit.py -- <tripo.fbx> <out.glb>

Steps: import -> join -> open the hem (Tripo closes a garment into one surface: the downward-facing cap under the skirt is
deleted; the cuff and collar caps stay, they read as the lining) -> decimate to at most MAX_TRIS (an HD mesh is ~2M) ->
keep only the base-colour texture -> export GLB.
"""
import os
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

MAX_TRIS = 50_000
CAP_FACING = 0.5  # hem-cap faces point at least this much straight down
CAP_HEIGHT = 0.1  # ...and lie in this bottom share of the cloak (higher up it's a fold of an already open hem)


def fail(message):
    print(f"[prep_outfit] ERROR: {message}")
    sys.exit(1)


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 2:
    fail("usage: -- <tripo.fbx> <out.glb>")
src, dst = (os.path.abspath(a) for a in args[:2])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    fail(f"no mesh in {src}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj in meshes)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
cloak = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Hem cap: the faces straight below the cloak's middle (a few rays, one may hit a fold), grown over their
# downward-facing neighbours.
mesh = bmesh.new()
mesh.from_mesh(cloak.data)
mesh.faces.ensure_lookup_table()
low = min(v.co.z for v in mesh.verts)
high = max(v.co.z for v in mesh.verts)
centre = sum((v.co for v in mesh.verts), Vector()) / len(mesh.verts)
tree = BVHTree.FromBMesh(mesh)
hits = (tree.ray_cast(Vector((centre.x + dx, centre.y + dy, (low + high) / 2)), Vector((0, 0, -1)))[2]
        for dx in (-0.05, 0, 0.05) for dy in (-0.05, 0, 0.05))
cap_top = low + CAP_HEIGHT * (high - low)


def is_cap(face):
    return face.normal.z < -CAP_FACING and face.calc_center_median().z < cap_top


cap = {mesh.faces[i] for i in hits if i is not None and is_cap(mesh.faces[i])}
if cap:
    stack = list(cap)
    while stack:
        for edge in stack.pop().edges:
            for face in edge.link_faces:
                if face not in cap and is_cap(face):
                    cap.add(face)
                    stack.append(face)
    bmesh.ops.delete(mesh, geom=list(cap), context="FACES_ONLY")
    bmesh.ops.delete(mesh, geom=[v for v in mesh.verts if not v.link_faces], context="VERTS")
mesh.to_mesh(cloak.data)
mesh.free()
print(f"[prep_outfit] hem cap: {len(cap)} faces removed")

triangles = sum(len(p.vertices) - 2 for p in cloak.data.polygons)
if triangles > MAX_TRIS:
    decimate = cloak.modifiers.new("Decimate", "DECIMATE")
    decimate.ratio = MAX_TRIS / triangles * 0.98
    bpy.ops.object.modifier_apply(modifier=decimate.name)
print(f"[prep_outfit] {triangles} -> {sum(len(p.vertices) - 2 for p in cloak.data.polygons)} triangles")

# Only the base colour goes on (skin_outfit.py takes the first image it finds).
for material in cloak.data.materials:
    for node in list(material.node_tree.nodes if material and material.use_nodes else []):
        if node.type == "TEX_IMAGE" and node.image and "basecolor" not in node.image.name.lower():
            material.node_tree.nodes.remove(node)

bpy.ops.export_scene.gltf(filepath=dst, use_selection=True, export_format="GLB")
print(f"[prep_outfit] wrote {dst}")
