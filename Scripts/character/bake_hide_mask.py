"""Blender (5.2) headless bake of a body's hide mask under one fitted outfit copy (docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/bake_hide_mask.py -- <body_rigged.fbx> <Outfit>_<Body>_outfit.fbx <out.png>

Writes an 8-bit greyscale PNG (MASK_SIZE square) in the body's first UV map: white = show, black = hide. The game feeds
it to the body's M_Character opacity while that outfit is worn (AShinobiPlayerCharacter::ApplyOutfit), so nothing of the
body shows through the cloak. A vertex is hidden only where the cloak encloses it: its normal and RAY_COUNT rays
RAY_SPREAD degrees round it all meet the copy within RAY_REACH, in the rest pose and in every body_landmarks.POSES pose.
Hands, feet, the head and neck, and a tail are never hidden. A face is hidden when all its vertices are; the hidden area
is then pulled ERODE metres back from what stays shown, so its edge stays under cloth.
"""
import math
import os
import sys

import bpy
import numpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from body_landmarks import POSES, find_tail, pose, suffix  # noqa: E402 (needs the path above)

MASK_SIZE = 1024
RAY_REACH = 0.20  # m: cloth further than this from the skin doesn't enclose it
RAY_COUNT, RAY_SPREAD = 8, 35.0  # rays round the normal, and their angle off it (degrees)
ERODE = 0.02  # m
NEVER_HIDDEN = ("Head", "HeadTop_End", "Neck", "LeftHand", "RightHand", "LeftFoot", "RightFoot", "LeftToeBase",
                "RightToeBase", "LeftToe_End", "RightToe_End")


def fail(message):
    print(f"[bake_hide_mask] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[bake_hide_mask] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 3:
    fail("usage: -- <body_rigged.fbx> <Outfit>_<Body>_outfit.fbx <out.png>")
body_fbx, copy_fbx, out_png = (os.path.abspath(a) for a in args[:3])


def import_rig(path):
    known = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.context.scene.objects if o not in known]
    armature = next((o for o in new if o.type == "ARMATURE"), None)
    mesh = next((o for o in new if o.type == "MESH"), None)
    if armature is None or mesh is None:
        fail(f"{path} needs an armature and a mesh")
    return armature, mesh


bpy.ops.wm.read_factory_settings(use_empty=True)
body_armature, body = import_rig(body_fbx)
cloak_armature, cloak = import_rig(copy_fbx)
count = len(body.data.vertices)

names = {g.index: suffix(g.name) for g in body.vertex_groups}
never = numpy.zeros(count, dtype=bool)
for vertex in body.data.vertices:
    top = max(vertex.groups, key=lambda g: g.weight, default=None)
    if top is None or names[top.group].startswith(NEVER_HIDDEN):
        never[vertex.index] = True
tail = find_tail(body_armature, body)
if tail:
    never[tail["indices"]] = True


def ray_fan(normal):
    """The normal and RAY_COUNT directions RAY_SPREAD degrees round it."""
    side = normal.orthogonal().normalized()
    tilt = Matrix.Rotation(math.radians(RAY_SPREAD), 3, side)
    fan = [normal]
    for k in range(RAY_COUNT):
        fan.append(Matrix.Rotation(2 * math.pi * k / RAY_COUNT, 3, normal) @ (tilt @ normal))
    return fan


enclosed = ~never
for pose_name in POSES:
    pose([body_armature, cloak_armature], pose_name)
    graph = bpy.context.evaluated_depsgraph_get()
    posed_body = body.evaluated_get(graph).to_mesh()
    posed_cloak = cloak.evaluated_get(graph).to_mesh()
    cloak_tree = BVHTree.FromPolygons([cloak.matrix_world @ v.co for v in posed_cloak.vertices], [p.vertices[:] for p in posed_cloak.polygons])
    turn = body.matrix_world.to_3x3()
    for index in numpy.nonzero(enclosed)[0].tolist():
        vertex = posed_body.vertices[index]
        origin = body.matrix_world @ vertex.co
        normal = (turn @ vertex.normal).normalized()
        start = origin + normal * 0.003
        for direction in ray_fan(normal):
            if cloak_tree.ray_cast(start, direction, RAY_REACH)[0] is None:
                enclosed[index] = False
                break
    body.evaluated_get(graph).to_mesh_clear()
    cloak.evaluated_get(graph).to_mesh_clear()
    log(f"{pose_name}: {int(enclosed.sum())} of {count} body vertices still enclosed")
pose([body_armature, cloak_armature], "idle")

# Erode: what lies within ERODE of anything shown is shown too.
rest = [body.matrix_world @ v.co for v in body.data.vertices]
shown_tree = KDTree(int((~enclosed).sum()))
for index in numpy.nonzero(~enclosed)[0].tolist():
    shown_tree.insert(rest[index], index)
shown_tree.balance()
hidden = enclosed.copy()
for index in numpy.nonzero(enclosed)[0].tolist():
    if shown_tree.find_range(rest[index], ERODE):
        hidden[index] = False
log(f"hidden: {int(hidden.sum())} of {count} body vertices ({hidden.mean():.0%})")

# Bake: one value per face corner (0 where the whole face is hidden), emitted into a white image through the first UV map.
colours = body.data.color_attributes.new("Hide", "BYTE_COLOR", "CORNER")
for polygon in body.data.polygons:
    value = 0.0 if all(hidden[v] for v in polygon.vertices) else 1.0
    for loop in polygon.loop_indices:
        colours.data[loop].color = (value, value, value, 1.0)
body.data.uv_layers.active_index = 0
image = bpy.data.images.new("Hide", MASK_SIZE, MASK_SIZE, alpha=False)
image.generated_color = (1.0, 1.0, 1.0, 1.0)
material = bpy.data.materials.new("HideBake")
material.use_nodes = True
tree = material.node_tree
for node in list(tree.nodes):
    if node.type != "OUTPUT_MATERIAL":
        tree.nodes.remove(node)
attribute = tree.nodes.new("ShaderNodeVertexColor")
attribute.layer_name = "Hide"
emission = tree.nodes.new("ShaderNodeEmission")
target = tree.nodes.new("ShaderNodeTexImage")
target.image = image
tree.links.new(attribute.outputs["Color"], emission.inputs["Color"])
tree.links.new(emission.outputs[0], next(n for n in tree.nodes if n.type == "OUTPUT_MATERIAL").inputs[0])
tree.nodes.active = target
body.data.materials.clear()
body.data.materials.append(material)
for obj in bpy.context.scene.objects:
    obj.select_set(obj == body)
bpy.context.view_layer.objects.active = body
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 1
scene.cycles.device = "CPU"
scene.render.bake.margin = 4
bpy.ops.object.bake(type="EMIT")
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "BW"
scene.render.image_settings.color_depth = "8"
image.save_render(out_png, scene=scene)
log(f"wrote {out_png}")
