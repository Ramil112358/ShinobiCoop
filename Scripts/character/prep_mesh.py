"""Blender (5.2) headless prep of an image-to-3D GLB for Mixamo auto-rigging (sub-project #2, plan Task 2).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/prep_mesh.py -- <in.glb> <out.fbx> [height_cm]

Steps: import -> join all meshes -> apply transforms -> scale to the hero height (default 180 cm) with feet at z = 0,
centred, facing -Y (Blender's front) -> merge vertices closer than 0.1 mm -> delete loose specks smaller than 1 cm
across (by size, not vertex count: small real parts such as glasses lenses must survive) -> decimate to at most
60,000 triangles -> keep a single material -> export FBX with the texture embedded ->
render preview_front.png / preview_side.png next to the output (Workbench, texture colours).
Exits with code 1 and a message if there's no mesh, the triangle budget can't be met or the height ends up wrong.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

MAX_TRIS = 60_000
MERGE_DISTANCE = 0.0001  # metres
SPECK_SIZE = 0.01  # metres: loose parts smaller than this across are generation noise


def fail(message):
    print(f"[prep_mesh] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[prep_mesh] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 2:
    fail("usage: -- <in.glb> <out.fbx> [height_cm]")
src, dst = os.path.abspath(args[0]), os.path.abspath(args[1])
height_m = (float(args[2]) if len(args) > 2 else 180.0) / 100.0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    fail(f"no mesh in {src}")

# One object, no parents, transforms applied.
for obj in bpy.context.scene.objects:
    obj.select_set(obj.type == "MESH")
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(meshes) > 1:
    bpy.ops.object.join()
hero = bpy.context.view_layer.objects.active
hero.name = "Hero"
for obj in list(bpy.context.scene.objects):
    if obj is not hero:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def bounds():
    """World-space min/max from the vertices themselves (Object.bound_box can be stale after mesh edits)."""
    points = [hero.matrix_world @ v.co for v in hero.data.vertices]
    return (Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
            Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))))


# Size and placement first (speck size is in metres): height, feet on the floor, centred on X/Y. glTF import already
# converts +Y-up to Blender's +Z-up with the model facing -Y (Blender's front), which is also what Mixamo expects.
low, high = bounds()
scale = height_m / (high.z - low.z)
hero.scale = (scale, scale, scale)
bpy.ops.object.transform_apply(scale=True)
low, high = bounds()
hero.location = (-(low.x + high.x) / 2, -(low.y + high.y) / 2, -low.z)
bpy.ops.object.transform_apply(location=True)

# Clean-up: weld duplicates, drop floating specks.
bm = bmesh.new()
bm.from_mesh(hero.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=MERGE_DISTANCE)
bm.verts.ensure_lookup_table()
seen, islands = set(), []
for vert in bm.verts:
    if vert in seen:
        continue
    island, stack = [], [vert]
    seen.add(vert)
    while stack:
        current = stack.pop()
        island.append(current)
        for edge in current.link_edges:
            other = edge.other_vert(current)
            if other not in seen:
                seen.add(other)
                stack.append(other)
    islands.append(island)
def span(island):
    xs, ys, zs = [v.co.x for v in island], [v.co.y for v in island], [v.co.z for v in island]
    return max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))


specks = [v for island in islands if span(island) < SPECK_SIZE for v in island]
bmesh.ops.delete(bm, geom=specks, context="VERTS")
bm.to_mesh(hero.data)
bm.free()
log(f"{len(islands)} parts, removed {len(specks)} vertices of specks")

# Budget.
triangles = sum(len(p.vertices) - 2 for p in hero.data.polygons)
if triangles > MAX_TRIS:
    decimate = hero.modifiers.new("Decimate", "DECIMATE")
    decimate.ratio = MAX_TRIS / triangles * 0.98
    bpy.ops.object.modifier_apply(modifier=decimate.name)
    triangles = sum(len(p.vertices) - 2 for p in hero.data.polygons)
if triangles > MAX_TRIS:
    fail(f"{triangles} triangles after decimation (budget {MAX_TRIS})")

# Removing specks can change the bounds a little: re-seat the feet and check the height.
low, high = bounds()
hero.location = (-(low.x + high.x) / 2, -(low.y + high.y) / 2, -low.z)
bpy.ops.object.transform_apply(location=True)
low, high = bounds()
final_height_cm = (high.z - low.z) * 100
if not 170 <= final_height_cm <= 190:
    fail(f"height {final_height_cm:.1f} cm after scaling (expected 170-190)")

# One material (image-to-3D output normally has exactly one; merge slots if not).
if len(hero.data.materials) > 1:
    log(f"{len(hero.data.materials)} materials: keeping the first for every face")
    for poly in hero.data.polygons:
        poly.material_index = 0
    while len(hero.data.materials) > 1:
        hero.data.materials.pop(index=len(hero.data.materials) - 1)

os.makedirs(os.path.dirname(dst), exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, path_mode="COPY", embed_textures=True,
                         apply_scale_options="FBX_SCALE_UNITS", add_leaf_bones=False, mesh_smooth_type="FACE")
log(f"exported {dst}: {triangles} triangles, {final_height_cm:.1f} cm")

# Previews: orthographic front and side, texture colours, flat studio light.
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 768, 1024
scene.render.film_transparent = False
world = bpy.data.worlds.new("Preview")
world.color = (0.8, 0.8, 0.8)
scene.world = world
camera_data = bpy.data.cameras.new("PreviewCamera")
camera_data.type = "ORTHO"
# Ortho scale covers the larger frame dimension (height here); widen it when the arm span needs more.
camera_data.ortho_scale = max(height_m, (high.x - low.x) * 1024 / 768, (high.y - low.y) * 1024 / 768) * 1.1
camera = bpy.data.objects.new("PreviewCamera", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
for name, location, rotation in (("front", (0, -5, height_m / 2), (math.radians(90), 0, 0)),
                                 ("side", (5, 0, height_m / 2), (math.radians(90), 0, math.radians(90)))):
    camera.location = location
    camera.rotation_euler = rotation
    scene.render.filepath = os.path.join(os.path.dirname(dst), f"preview_{name}.png")
    bpy.ops.render.render(write_still=True)
    log(f"rendered {scene.render.filepath}")
