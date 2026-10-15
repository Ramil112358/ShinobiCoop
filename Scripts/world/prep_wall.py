"""Blender (5.2) headless prep of a Rodin jutsu-wall GLB (Earth Wall / Water Wall) for Scripts/setup_walls.py.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/world/prep_wall.py -- <in.glb> <out_dir> [width_cm height_cm]

Steps: import -> join -> apply transforms -> turn the wall's long side onto X -> scale to width_cm (default 480) and
squash/stretch Z to height_cm (default 320) -> base at z = 0, centred -> drop specks -> decimate to 30k triangles ->
<out_dir>/<name>.fbx (no textures) + <name>_BaseColor.png, <name>_Normal.png, <name>_ORM.png (glTF metallic-roughness:
G = roughness) when the GLB has them -> preview_front.png, preview_side.png, preview_34.png (Workbench, texture colours).
The wall faces -Y in Blender (its front), which Scripts/setup_walls.py turns to the actor's +X.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

MAX_TRIS = 30_000
SPECK_SIZE = 0.03  # metres


def fail(message):
    print(f"[prep_wall] ERROR: {message}")
    sys.exit(1)


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 2:
    fail("usage: -- <in.glb> <out_dir> [width_cm height_cm]")
src, out_dir = os.path.abspath(args[0]), os.path.abspath(args[1])
width_m = (float(args[2]) if len(args) > 2 else 480.0) / 100.0
height_m = (float(args[3]) if len(args) > 3 else 320.0) / 100.0
depth_m = (float(args[4]) if len(args) > 4 else 140.0) / 100.0
flip = len(args) > 5 and args[5] == "flip"  # Water: Rodin's waterfall upside down, its drips a rising crest
name = os.path.basename(out_dir.rstrip("\\/"))
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    fail(f"no mesh in {src}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj.type == "MESH")
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(meshes) > 1:
    bpy.ops.object.join()
wall = bpy.context.view_layer.objects.active
wall.name = name
for obj in list(bpy.context.scene.objects):
    if obj is not wall:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def bounds():
    points = [wall.matrix_world @ v.co for v in wall.data.vertices]
    return (Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
            Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))))


def seat():
    low, high = bounds()
    wall.location = (-(low.x + high.x) / 2, -(low.y + high.y) / 2, -low.z)
    bpy.ops.object.transform_apply(location=True)


low, high = bounds()
if high.y - low.y > high.x - low.x:  # long side onto X (on the mesh itself: object transforms are already applied)
    wall.data.transform(Matrix.Rotation(math.radians(-90), 4, "Z"))
if flip:
    wall.data.transform(Matrix.Rotation(math.pi, 4, "Y"))
low, high = bounds()
# ponytail: non-uniform fit to the collision box's proportions; fine for rock and water, wrong for anything with a face
wall.data.transform(Matrix.Diagonal((width_m / (high.x - low.x), depth_m / (high.y - low.y), height_m / (high.z - low.z), 1)))
wall.data.update()
seat()

bm = bmesh.new()
bm.from_mesh(wall.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0001)
bm.verts.ensure_lookup_table()
seen, specks = set(), []
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
    co = [v.co for v in island]
    if max(max(c[i] for c in co) - min(c[i] for c in co) for i in range(3)) < SPECK_SIZE:
        specks += island
bmesh.ops.delete(bm, geom=specks, context="VERTS")
bm.to_mesh(wall.data)
bm.free()

triangles = sum(len(p.vertices) - 2 for p in wall.data.polygons)
if triangles > MAX_TRIS:
    decimate = wall.modifiers.new("Decimate", "DECIMATE")
    decimate.ratio = MAX_TRIS / triangles * 0.98
    bpy.ops.object.modifier_apply(modifier=decimate.name)
    triangles = sum(len(p.vertices) - 2 for p in wall.data.polygons)
seat()
low, high = bounds()

# Textures out as PNGs (UE's FBX importer and Rodin's packed images don't mix well); one material slot.
material = wall.data.materials[0] if wall.data.materials else None
if material and material.node_tree:
    bsdf = next((n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)

    def image_into(socket_name):
        stack = [bsdf.inputs[socket_name]] if bsdf else []
        while stack:
            socket = stack.pop()
            for link_ in socket.links:
                if link_.from_node.type == "TEX_IMAGE" and link_.from_node.image:
                    return link_.from_node.image
                stack += [i for i in link_.from_node.inputs if i.is_linked]
        return None

    for socket_name, suffix in (("Base Color", "BaseColor"), ("Normal", "Normal"), ("Roughness", "ORM")):
        image = image_into(socket_name)
        if image:
            image.filepath_raw = os.path.join(out_dir, f"{name}_{suffix}.png")
            image.file_format = "PNG"
            image.save()
            print(f"[prep_wall] {suffix}: {tuple(image.size)}")
while len(wall.data.materials) > 1:
    wall.data.materials.pop(index=len(wall.data.materials) - 1)
for poly in wall.data.polygons:
    poly.material_index = 0

bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, f"{name}.fbx"), use_selection=True, path_mode="STRIP",
                         apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE")
print(f"[prep_wall] exported {name}.fbx: {triangles} triangles, {(high.x - low.x) * 100:.0f} x {(high.y - low.y) * 100:.0f} x {(high.z - low.z) * 100:.0f} cm")

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 1024, 768
world = bpy.data.worlds.new("Preview")
world.color = (0.75, 0.75, 0.75)
scene.world = world
camera = bpy.data.objects.new("PreviewCamera", bpy.data.cameras.new("PreviewCamera"))
scene.collection.objects.link(camera)
scene.camera = camera
mid = height_m / 2
for view, location in (("front", (0, -12, mid)), ("side", (12, 0, mid)), ("34", (-7, -9, mid + 3))):
    camera.location = location
    direction = Vector((0, 0, mid)) - Vector(location)
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(out_dir, f"preview_{view}.png")
    bpy.ops.render.render(write_still=True)
