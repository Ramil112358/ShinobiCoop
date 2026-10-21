"""Blender (5.2) headless acceptance check of one fitted outfit copy on its body (docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/check_outfit.py -- <body_rigged.fbx> <Outfit>_<Body>_outfit.fbx <Outfit>_outfit.png <Outfit>_<Body>_hide.png <out_prefix>

In every body_landmarks.POSES pose, on the skinned copy (the game's cloth swings the hem and sleeves round that):
  stretched  pinned cloak edges longer than STRETCH x their rest length and STRETCH_MIN longer (the game simulates the
             hem, sleeves and underarms: their screenshots judge those);
  exposed    hidden body vertices (the hide mask) whose normal ray no longer meets the cloak within REACH: see-through;
  poke       shown torso, upper-arm and thigh vertices that were under pinned cloak at rest and are now through it in
             view (the pinned cloak lies behind them within POKE_DEPTH, and no cloak at all in front of them).
Prints "POSE <name> stretched=<n> exposed=<n> poke=<n>" per pose and "RESULT PASS" / "RESULT FAIL" (exit 1), and renders
<out_prefix>_<pose>_{front,side,back}.png with the hidden body faces removed, as the game shows it.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from body_landmarks import POSES, pose, suffix  # noqa: E402 (needs the path above)

STRETCH, STRETCH_MIN = 3.0, 0.02  # an edge counts as stretched past 3x its rest length and 2 cm longer (decimation
# leaves millimetre slivers whose ratio means nothing)
REACH = 0.25  # m
POKE_DEPTH = 0.05  # m
MAX_EXPOSED_SHARE = 0.005  # of the hidden vertices, per pose
# (stretched, poke) allowed per pose: the everyday poses must be clean; raising the arms far (120 deg up, 80 deg forward)
# always webs a skinned armpit a little, so those get room and the game's screenshots judge them (Task 9 of the plan).
LIMITS = {"idle": (50, 40), "arms_down": (50, 40), "arms_out": (50, 40), "jump_tuck": (50, 40),
          "arms_up": (300, 150), "arms_forward": (300, 100)}  # (arms_up, 120 deg: the collarbone rises under the collar)
POKE_GROUPS = ("Hips", "Spine", "Spine1", "Spine2", "LeftShoulder", "RightShoulder", "LeftArm", "RightArm", "LeftUpLeg", "RightUpLeg")


def fail(message):
    print(f"[check_outfit] ERROR: {message}")
    sys.exit(1)


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 5:
    fail("usage: -- <body_rigged.fbx> <copy.fbx> <outfit.png> <hide.png> <out_prefix>")
body_fbx, copy_fbx, texture_png, hide_png = (os.path.abspath(a) for a in args[:4])
out_prefix = os.path.abspath(args[4])


def import_rig(path):
    known = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.context.scene.objects if o not in known]
    return next(o for o in new if o.type == "ARMATURE"), next(o for o in new if o.type == "MESH")


bpy.ops.wm.read_factory_settings(use_empty=True)
body_armature, body = import_rig(body_fbx)
cloak_armature, cloak = import_rig(copy_fbx)

# The hide mask, per body vertex (read at its UVs), and which body faces the game hides.
mask = bpy.data.images.load(hide_png)
w, h = mask.size
pixels = numpy.array(mask.pixels[:], dtype=numpy.float32).reshape(h, w, mask.channels)[..., 0]
uv = body.data.uv_layers[0].data
hidden = numpy.zeros(len(body.data.vertices), dtype=bool)
for loop in body.data.loops:
    u, v = uv[loop.index].uv
    hidden[loop.vertex_index] = pixels[min(max(int(v * h), 0), h - 1), min(max(int(u * w), 0), w - 1)] < 0.5
names = {g.index: suffix(g.name) for g in body.vertex_groups}
pokeable = numpy.zeros(len(body.data.vertices), dtype=bool)
for vertex in body.data.vertices:
    top = max(vertex.groups, key=lambda g: g.weight, default=None)
    pokeable[vertex.index] = top is not None and names[top.group] in POKE_GROUPS
# The cloak's hem share (R) and pinned cloth (R = G = 0), from the copy's vertex colours.
colours = cloak.data.color_attributes.active_color
shares = numpy.zeros((len(cloak.data.vertices), 2))
if colours is not None:
    for i, value in enumerate(colours.data) if colours.domain == "POINT" else ():
        shares[i] = value.color_srgb[:2]
    if colours.domain == "CORNER":
        for loop in cloak.data.loops:
            shares[loop.vertex_index] = colours.data[loop.index].color_srgb[:2]
pinned_face = [all(shares[v, 0] < 0.01 and shares[v, 1] < 0.01 for v in p.vertices) for p in cloak.data.polygons]
edges = [(e.vertices[0], e.vertices[1]) for e in cloak.data.edges if shares[e.vertices[0]].max() < 0.1 and shares[e.vertices[1]].max() < 0.1]


def snapshot():
    graph = bpy.context.evaluated_depsgraph_get()
    posed_body = body.evaluated_get(graph).to_mesh()
    posed_cloak = cloak.evaluated_get(graph).to_mesh()
    body_co = [body.matrix_world @ v.co for v in posed_body.vertices]
    body_n = [(body.matrix_world.to_3x3() @ v.normal).normalized() for v in posed_body.vertices]
    cloak_co = [cloak.matrix_world @ v.co for v in posed_cloak.vertices]
    faces = [p.vertices[:] for p in posed_cloak.polygons]
    body.evaluated_get(graph).to_mesh_clear()
    cloak.evaluated_get(graph).to_mesh_clear()
    every = BVHTree.FromPolygons(cloak_co, faces)
    pinned = BVHTree.FromPolygons(cloak_co, [f for f, keep in zip(faces, pinned_face) if keep])
    return body_co, body_n, cloak_co, every, pinned


pose([body_armature, cloak_armature], "idle")
rest_body, rest_n, rest_cloak, _, rest_pinned = snapshot()
under = numpy.array([rest_pinned.ray_cast(c + n * 0.002, n, REACH)[0] is not None for c, n in zip(rest_body, rest_n)])

# Renders: the body with its hidden faces removed (as the game shows it), the cloak textured.
shown_body = body.copy()  # (a copy: the measured body keeps its faces and normals)
shown_body.data = body.data.copy()
scene_collection = bpy.context.scene.collection
scene_collection.objects.link(shown_body)
body.hide_render = True
cut = bmesh.new()
cut.from_mesh(shown_body.data)
bmesh.ops.delete(cut, geom=[f for f in cut.faces if all(hidden[v.index] for v in f.verts)], context="FACES_ONLY")
cut.to_mesh(shown_body.data)
cut.free()
texture = bpy.data.images.load(texture_png)
material = bpy.data.materials.new("Cloak")
material.use_nodes = True
node = material.node_tree.nodes.new("ShaderNodeTexImage")
node.image = texture
material.node_tree.links.new(node.outputs[0], material.node_tree.nodes["Principled BSDF"].inputs[0])
cloak.data.materials.clear()
cloak.data.materials.append(material)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 600, 800
camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = "ORTHO"
camera.data.ortho_scale = 2.2
centre = Vector((0.0, 0.0, 1.0))

failed = False
for pose_name in POSES:
    pose([body_armature, cloak_armature], pose_name)
    body_co, body_n, cloak_co, every, pinned = snapshot()
    long_edges = [(a, b) for a, b in edges if (rest_cloak[a] - rest_cloak[b]).length > 1e-6
                  and (cloak_co[a] - cloak_co[b]).length > STRETCH * (rest_cloak[a] - rest_cloak[b]).length
                  and (cloak_co[a] - cloak_co[b]).length - (rest_cloak[a] - rest_cloak[b]).length > STRETCH_MIN]
    stretched = len(long_edges)
    if long_edges:  # where they are (rest position, metres): the first thing to look at when a pose fails
        at = numpy.array([(rest_cloak[a] + rest_cloak[b]) / 2 for a, b in long_edges])
        sleeve = numpy.mean([max(shares[a, 1], shares[b, 1]) > 0.01 for a, b in long_edges])
        print(f"  {pose_name}: stretched edges around x {numpy.median(numpy.abs(at[:, 0])):.2f}, z {numpy.median(at[:, 2]):.2f} "
              f"(z {at[:, 2].min():.2f}..{at[:, 2].max():.2f}); {sleeve:.0%} on sleeve cloth")
    exposed = sum(1 for i in numpy.nonzero(hidden)[0].tolist() if every.ray_cast(body_co[i] + body_n[i] * 0.002, body_n[i], REACH)[0] is None)
    poked = [i for i in numpy.nonzero(~hidden & pokeable & under)[0].tolist()
             if pinned.ray_cast(body_co[i] - body_n[i] * 0.002, -body_n[i], POKE_DEPTH)[0] is not None
             and every.ray_cast(body_co[i] + body_n[i] * 0.002, body_n[i], REACH)[0] is None]
    poke = len(poked)
    if poked:
        at = numpy.array([rest_body[i] for i in poked])
        groups = {}
        for i in poked:
            top = max(body.data.vertices[i].groups, key=lambda g: g.weight)
            groups[names[top.group]] = groups.get(names[top.group], 0) + 1
        print(f"  {pose_name}: pokes around x {numpy.median(numpy.abs(at[:, 0])):.2f}, z {numpy.median(at[:, 2]):.2f}; "
              f"{', '.join(f'{k} {v}' for k, v in sorted(groups.items(), key=lambda t: -t[1])[:4])}")
    max_stretched, max_poke = LIMITS[pose_name]
    ok = stretched <= max_stretched and exposed <= MAX_EXPOSED_SHARE * max(int(hidden.sum()), 1) and poke <= max_poke
    failed |= not ok
    print(f"POSE {pose_name} stretched={stretched} exposed={exposed} poke={poke}{'' if ok else '  FAIL'}")
    for view, offset in (("front", (0, -4, 0)), ("side", (4, 0, 0)), ("back", (0, 4, 0))):
        camera.location = centre + Vector(offset)
        camera.rotation_euler = (centre - camera.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = f"{out_prefix}_{pose_name}_{view}.png"
        bpy.ops.render.render(write_still=True)
print("RESULT FAIL" if failed else "RESULT PASS")
sys.exit(1 if failed else 0)
