"""Rigs and animates the summon dragon, Tairn (Fourth Wing's black Morningstartail): SourceArt/Creatures/Tairn/rodin_tairn.glb (a Hyper3D
Rodin model) with a morningstar club grafted on the tail -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_dragon.py            (add  -- preview  to render pose previews to Saved/dragon_*.png)

Rodin's Tairn has the right look (black ridged scales, a mane of neck spikes, golden eyes, scalloped black wings) but ends its tail in a thin
curl, and the books give Tairn a morningstar tail: a heavy club studded with long spikes. So the curl is cut off and a spiked club is joined on:
a Rodin-generated spiked ball (rodin_tail_club2.glb) with long conical spikes added. Three materials: the body, the ball, the spikes (flat black).

The Rodin dragon sits upright on its haunches (tail on the ground, neck raised, wings up in a V). A 37-bone rig is built to its
proportions (spine, neck, head and jaw, a five-bone tail, two arms, two legs, and a wing of five bones a side), the mesh is weighted to
it by distance, and every flying clip tips the whole dragon nose-down about its hips so the back is level, with the neck and tail bent
back to level: Idle (a slow hover), Flap (the fast beat, a loop), Glide (wings out, a slow sway) and Breath (rears the head back, opens the
jaw and thrusts; played once). Writes SourceArt/Creatures/DragonRodin/Tairn.fbx (mesh + skeleton), Tairn_<Clip>.fbx per clip and
Tairn.png (the 2k base colour). Faces +X, feet on Z = 0 in the rest pose, centred over the origin; metres.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Tairn")
PREVIEW = "preview" in sys.argv
FPS = 30
PITCH = 62.0           # how far the flying dragon is tipped nose-down about its hips
HIPS = Vector((0.3, 0, 0.6))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "rodin_tairn.glb"))
scene = bpy.context.scene
scene.render.fps = FPS
body = next(o for o in bpy.data.objects if o.type == "MESH")
for o in list(bpy.data.objects):
    if o is not body:
        bpy.data.objects.remove(o)
body.name = "Body"
body.parent = None


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


# --- fit: turn to face +X (Rodin faces -Y), centre over the origin, feet on the ground -------------------------------------
body.rotation_mode = "XYZ"
body.rotation_euler.z = math.radians(90)
select(body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
points = [body.matrix_world @ v.co for v in body.data.vertices]
shift = Vector(((min(p.x for p in points) + max(p.x for p in points)) / 2, (min(p.y for p in points) + max(p.y for p in points)) / 2, min(p.z for p in points)))
body.location -= shift
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# --- the club --------------------------------------------------------------------------------------------------------------
CUT_X = -0.70
CLUB_DIAMETER = 0.36
SPIKE_COUNT = 26
SPIKE_LENGTH = 0.2
SPIKE_RADIUS = 0.035

body_materials = len(body.data.materials)
bm = bmesh.new()
bm.from_mesh(body.data)
doomed = [v for v in bm.verts if v.co.x < CUT_X]
bmesh.ops.delete(bm, geom=doomed, context="VERTS")
# The thin curl of the old tail tip leaves a stub standing above the cut: take out the pieces above the tail line behind x = -0.58.
stub = [v for v in bm.verts if v.co.x < -0.58 and v.co.z > 0.3]
print("[build_dragon] removing", len(stub), "stub vertices above the tail near the cut")
bmesh.ops.delete(bm, geom=stub, context="VERTS")
# Scarred wings (the books: "scarred black wings"): ragged tears torn out of each membrane's lower, trailing part. Candidate spots are
# faces low on the wing (z 0.5-0.85, |y| > 0.6); three well-separated ones per side are removed with a little of what surrounds them.
tears = []
for sign in (1, -1):
    chosen = []
    for f in sorted(bm.faces, key=lambda f: (f.calc_center_median().x * 7.0 + f.calc_center_median().z * 3.0)):
        c = f.calc_center_median()
        if c.y * sign > 0.6 and 0.5 < c.z < 0.85 and all((c - o).length > 0.22 for o in chosen):
            chosen.append(c)
        if len(chosen) == 3:
            break
    tears += [(c, 0.075) for c in chosen]
for centre, tear in tears:
    torn = [f for f in bm.faces if (f.calc_center_median() - centre).length < tear]
    print("[build_dragon] tear at", tuple(round(x, 2) for x in centre), "removes", len(torn), "faces")
    bmesh.ops.delete(bm, geom=torn, context="FACES_ONLY")
bm.to_mesh(body.data)
bm.free()
# where the tail now ends: its middle there (the median of the verts just inside the cut, spikes excluded)
near = [v.co for v in body.data.vertices if CUT_X <= v.co.x < CUT_X + 0.06]
tail_z = sorted(c.z for c in near)[len(near) // 2]
tail_y = sorted(c.y for c in near)[len(near) // 2]
radius = CLUB_DIAMETER / 2
centre = Vector((CUT_X - radius * 0.55, tail_y, tail_z))
print("[build_dragon] tail cut at x", CUT_X, "middle", round(tail_y, 2), round(tail_z, 2), "; club centre", tuple(round(c, 2) for c in centre))

before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, "rodin_tail_club2.glb"))
balls = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
for o in set(bpy.data.objects) - before - set(balls):
    if o.type == "EMPTY":
        for child in o.children:
            child.parent = None
        bpy.data.objects.remove(o)
select(*balls)
if len(balls) > 1:
    bpy.ops.object.join()
ball = bpy.context.view_layer.objects.active
ball.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# The ball's axis is Z with its bronze socket at the bottom (-Z): lay it along -X with the socket toward the body (+X).
ball.rotation_mode = "XYZ"
ball.rotation_euler.y = math.radians(-90)
select(ball)
bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
pts = [ball.matrix_world @ v.co for v in ball.data.vertices]
size = max(max(p.x for p in pts) - min(p.x for p in pts), max(p.y for p in pts) - min(p.y for p in pts), max(p.z for p in pts) - min(p.z for p in pts))
ball.scale = (CLUB_DIAMETER / size * 1.0,) * 3
select(ball)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
pts = [ball.matrix_world @ v.co for v in ball.data.vertices]
mid = Vector(((min(p.x for p in pts) + max(p.x for p in pts)) / 2, (min(p.y for p in pts) + max(p.y for p in pts)) / 2, (min(p.z for p in pts) + max(p.z for p in pts)) / 2))
ball.location = centre - mid
select(ball)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
ball.name = "Club"

# long conical spikes all round the ball (Fibonacci sphere), none pointing back into the tail
spike_mesh = bpy.data.meshes.new("SpikeMesh")
bmc = bmesh.new()
golden = math.pi * (3 - math.sqrt(5))
for i in range(SPIKE_COUNT):
    y = 1 - (i + 0.5) / SPIKE_COUNT * 2
    r = math.sqrt(max(0.0, 1 - y * y))
    direction = Vector((math.cos(golden * i) * r, y, math.sin(golden * i) * r))
    if direction.x > 0.55:
        continue  # toward the body
    base = centre + direction * (radius * 0.82)
    cone = bmesh.ops.create_cone(bmc, cap_ends=True, segments=7, radius1=SPIKE_RADIUS, radius2=0.002, depth=SPIKE_LENGTH + radius * 0.18)
    rotation = Vector((0, 0, 1)).rotation_difference(direction).to_matrix().to_4x4()
    bmesh.ops.transform(bmc, matrix=Matrix.Translation(base + direction * ((SPIKE_LENGTH + radius * 0.18) / 2)) @ rotation, verts=cone["verts"])
bmc.to_mesh(spike_mesh)
bmc.free()
spikes = bpy.data.objects.new("ClubSpikes", spike_mesh)
scene.collection.objects.link(spikes)
black = bpy.data.materials.new("ClubSpike")
black.diffuse_color = (0.02, 0.02, 0.025, 1)
spikes.data.materials.append(black)

# one mesh: body slot 0, the ball its own slot, the spikes a flat black one
club_first_vertex = len(body.data.vertices)
select(body, ball, spikes)
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
body.name = "Body"
club_vertices = set(range(club_first_vertex, len(body.data.vertices)))
print("[build_dragon] club vertices:", len(club_vertices), "materials:", [m.name for m in body.data.materials])

# --- the textures: the body's base colour with its emissive map (the golden eyes) added, so the eyes glow ----------------------------
def save_texture(material, path, add_emissive=False):
    tree = material.node_tree
    principled = next(n for n in tree.nodes if n.type == "BSDF_PRINCIPLED")
    base = principled.inputs["Base Color"].links[0].from_node.image if principled.inputs["Base Color"].links else None
    if base is None:
        return
    base.scale(2048, 2048)
    if add_emissive and principled.inputs["Emission Color"].links:
        glow = principled.inputs["Emission Color"].links[0].from_node.image
        if glow:
            glow.scale(2048, 2048)
            a = np.empty(len(base.pixels), dtype=np.float32)
            b = np.empty(len(glow.pixels), dtype=np.float32)
            base.pixels.foreach_get(a)
            glow.pixels.foreach_get(b)
            a[:] = np.clip(a + b, 0.0, 1.0)
            a[3::4] = 1.0
            base.pixels.foreach_set(a)
            print("[build_dragon] emissive added to", material.name)
    base.filepath_raw = path
    base.file_format = "PNG"
    base.save()


save_texture(body.data.materials[0], os.path.join(OUT, "Tairn.png"), add_emissive=True)
if len(body.data.materials) > 1:
    save_texture(body.data.materials[1], os.path.join(OUT, "TairnClub.png"))

# --- the rig ---------------------------------------------------------------------------------------------------------------
# name: (parent, head, tail). Y > 0 is the left side. Measured on the mesh (see the silhouettes in the build notes): hind feet at x 0.43,
# hips about (0.3, 0, 0.55), the neck rising to the head at x 0.8, z 1.17; the tail runs back along the ground to x -0.95.
BONES = {
    "root": (None, (0, 0, 0), (0, 0, 0.2)),
    "pelvis": ("root", (0.28, 0, 0.55), (0.33, 0, 0.75)),
    "chest": ("pelvis", (0.33, 0, 0.75), (0.42, 0, 1.0)),
    "neck1": ("chest", (0.42, 0, 1.0), (0.58, 0, 1.1)),
    "neck2": ("neck1", (0.58, 0, 1.1), (0.75, 0, 1.17)),
    "head": ("neck2", (0.75, 0, 1.17), (0.95, 0, 1.17)),
    "jaw": ("head", (0.8, 0, 1.1), (0.95, 0, 1.08)),
    "tail1": ("pelvis", (0.2, 0, 0.5), (-0.05, 0, 0.42)),
    "tail2": ("tail1", (-0.05, 0, 0.42), (-0.3, 0, 0.36)),
    "tail3": ("tail2", (-0.3, 0, 0.36), (-0.55, 0, 0.32)),
    "tail4": ("tail3", (-0.55, 0, 0.32), (-0.78, 0, 0.3)),
    "tail5": ("tail4", (-0.78, 0, 0.3), (CUT_X - radius * 0.55, 0, tail_z)),
}
for side, s in (("L", 1), ("R", -1)):
    BONES[f"thigh_{side}"] = ("pelvis", (0.3, s * 0.22, 0.5), (0.46, s * 0.27, 0.27))
    BONES[f"shin_{side}"] = (f"thigh_{side}", (0.46, s * 0.27, 0.27), (0.43, s * 0.26, 0.07))
    BONES[f"foot_{side}"] = (f"shin_{side}", (0.43, s * 0.26, 0.07), (0.62, s * 0.26, 0.02))
    BONES[f"arm_{side}"] = ("chest", (0.45, s * 0.2, 0.92), (0.58, s * 0.3, 0.75))
    BONES[f"forearm_{side}"] = (f"arm_{side}", (0.58, s * 0.3, 0.75), (0.75, s * 0.3, 0.88))
    BONES[f"hand_{side}"] = (f"forearm_{side}", (0.75, s * 0.3, 0.88), (0.84, s * 0.3, 0.96))
    BONES[f"wing1_{side}"] = ("chest", (0.4, s * 0.2, 1.1), (0.45, s * 0.46, 1.38))
    BONES[f"wing2_{side}"] = (f"wing1_{side}", (0.45, s * 0.46, 1.38), (0.45, s * 0.7, 1.15))
    BONES[f"finger1_{side}"] = (f"wing2_{side}", (0.45, s * 0.7, 1.15), (0.38, s * 0.95, 0.69))
    BONES[f"finger2_{side}"] = (f"wing2_{side}", (0.45, s * 0.7, 1.15), (0.3, s * 0.8, 0.58))
    BONES[f"finger3_{side}"] = (f"wing2_{side}", (0.45, s * 0.7, 1.15), (0.25, s * 0.6, 0.5))

data = bpy.data.armatures.new("Armature")
arm = bpy.data.objects.new("Armature", data)
scene.collection.objects.link(arm)
select(arm)
bpy.ops.object.mode_set(mode="EDIT")
for name, (parent, head, tail) in BONES.items():
    edit = data.edit_bones.new(name)
    edit.head, edit.tail = Vector(head), Vector(tail)
    if parent:
        edit.parent = data.edit_bones[parent]
        edit.use_connect = False
bpy.ops.object.mode_set(mode="OBJECT")

# Weight by distance: each vertex follows its four nearest bones, the nearer the stronger. Bone heat fails on meshes like this.
select(arm, body)
bpy.ops.object.parent_set(type="ARMATURE_NAME")
groups = {name: body.vertex_groups[name] for name in BONES}


def segment_distance(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


segments = {name: (Vector(head), Vector(tail)) for name, (_, head, tail) in BONES.items() if name != "root"}
for v in body.data.vertices:
    if v.index in club_vertices:
        groups["tail5"].add([v.index], 1.0, "REPLACE")
        continue
    nearest = sorted((segment_distance(v.co, a, b), name) for name, (a, b) in segments.items())[:4]
    weights = [1.0 / (d + 0.04) ** 3 for d, _ in nearest]
    total = sum(weights)
    for (d, name), w in zip(nearest, weights):
        groups[name].add([v.index], w / total, "REPLACE")
print("[build_dragon] weighted verts:", sum(1 for v in body.data.vertices if sum(g.weight for g in v.groups) > 0.99), "of", len(body.data.vertices))

# --- animation -------------------------------------------------------------------------------------------------------------
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
REST = {pb.name: pb.bone.matrix_local.to_3x3() for pb in arm.pose.bones}
ORDER = list(BONES)


def ry(deg):   # about world Y: positive tips the nose (+X) down
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def nose_up(deg):
    return ry(-deg)


def rx(deg):   # about world X: positive lifts the left (+Y) side
    return Matrix.Rotation(math.radians(deg), 3, "X")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def pose(frame, rot=None, root_rotation=None, pivot=Vector(), lift=Vector()):
    """Keys the whole pose at `frame`. rot: bone -> world-axis rotation added on top of its parent's (children follow parents).
    root_rotation turns the whole dragon about `pivot`; lift moves it."""
    rot = rot or {}
    world = {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent = BONES[name][0]
        parent_world = world[parent] if parent else Matrix.Identity(3)
        delta = rot.get(name, Matrix.Identity(3))
        if name == "root":
            delta = root_rotation or Matrix.Identity(3)
        world[name] = delta @ parent_world
        local = REST[name].inverted() @ parent_world.inverted() @ delta @ parent_world @ REST[name]
        pb.rotation_quaternion = local.to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == "root":
            turn = root_rotation or Matrix.Identity(3)
            pb.location = REST[name].inverted() @ ((pivot - turn @ pivot) + lift)
            pb.keyframe_insert("location", frame=frame)


def new_action(name, frames):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = action
    scene.frame_start, scene.frame_end = 0, frames
    return action


def use_action(action):
    """Blender 5 actions are slotted: making one current also needs its slot assigned."""
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]


def flying(flap=0.0, fold=0.0, neck=0.0, head=0.0, jaw=0.0, sway=0.0, tail_wave=0.0, legs=0.0, lift=0.0, extra_pitch=0.0):
    """The flying body: tipped nose-down about the hips, neck and tail bent back to level, legs trailing. flap: wings up (+) or down (-)
    in degrees; fold: how far the wing's outer part bends from the arm; neck/head: extra nose-up; jaw: mouth open in degrees."""
    rot = {}
    for i in range(1, 6):
        rot[f"tail{i}"] = nose_up(PITCH / 5.0 * 0.95) @ rx(0)
        rot[f"tail{i}"] = Matrix.Rotation(math.radians(tail_wave * math.sin(i * 0.9)), 3, "Z") @ rot[f"tail{i}"]
    rot["neck1"] = nose_up(PITCH * 0.34 + neck * 0.5)
    rot["neck2"] = nose_up(PITCH * 0.34 + neck * 0.5)
    rot["head"] = nose_up(PITCH * 0.22 + head)
    rot["jaw"] = ry(jaw)
    for side, s in (("L", 1), ("R", -1)):
        rot[f"wing1_{side}"] = rx(s * flap)
        rot[f"wing2_{side}"] = rx(s * (flap * 0.5 + fold)) @ Matrix.Rotation(math.radians(sway), 3, "Z")
        for i in (1, 2, 3):
            rot[f"finger{i}_{side}"] = rx(s * (flap * 0.4 + fold * 0.6))
        rot[f"thigh_{side}"] = nose_up(25 + legs)
        rot[f"shin_{side}"] = ry(20 + legs)
        rot[f"arm_{side}"] = nose_up(20)
        rot[f"forearm_{side}"] = ry(25)
    return rot, ry(PITCH + extra_pitch), lift


def key(frame, **kwargs):
    rot, root_rotation, lift = flying(**kwargs)
    pose(frame, rot, root_rotation=root_rotation, pivot=HIPS, lift=Vector((0, 0, lift)))


def idle():   # a hover: slow, shallow beats
    for f in range(0, 61, 3):
        s = math.sin(2 * math.pi * f / 60)
        key(f, flap=12 * s, fold=8 * s, neck=3 * s, tail_wave=6 * s, lift=0.04 * s)


def flap():   # the working beat: wings high to low, the body rising on the downstroke
    for f in range(0, 19):
        s = math.sin(2 * math.pi * f / 18)
        key(f, flap=38 * s, fold=-26 * s, neck=-4 * s, legs=-6 * s, tail_wave=4 * s, lift=-0.12 * s)


def glide():  # wings out and steady, a slow sway
    for f in range(0, 49, 2):
        s = math.sin(2 * math.pi * f / 48)
        key(f, flap=4 * s - 2, fold=3 * s, tail_wave=8 * s, sway=2 * s)


def breath():  # rears the head back and opens wide (0-10), thrusts and holds the mouth open for the flames (10-40), then closes
    for f in range(0, 49, 2):
        draw = smooth(f / 10) * (1 - smooth((f - 40) / 8))
        thrust = smooth((f - 10) / 3) * (1 - smooth((f - 40) / 8))
        key(f, flap=3 * math.sin(f / 6.0), neck=-30 * (draw - thrust), head=-20 * draw + 38 * thrust, jaw=-26 * draw - 8 * thrust, lift=0.02 * thrust)


CLIPS = (("Idle", 60, idle), ("Flap", 18, flap), ("Glide", 48, glide), ("Breath", 48, breath))
actions = {}
for name, frames, build in CLIPS:
    action = new_action(name, frames)
    build()
    actions[name] = action
    print(f"[build_dragon] clip {name}: {frames} frames")

# --- previews --------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 560
    cams = {}
    for name, loc, rot in (("side", (0.3, -5.0, 0.9), (math.radians(90), 0, 0)), ("front", (5.5, 0.0, 0.9), (math.radians(90), 0, math.radians(90))),
                           ("top", (0.3, 0.0, 6.0), (0, 0, 0))):
        cam = bpy.data.objects.new("cam_" + name, bpy.data.cameras.new("cam_" + name))
        scene.collection.objects.link(cam)
        cam.location, cam.rotation_euler = loc, rot
        cams[name] = cam
    for clip, frame in (("Glide", 0), ("Flap", 5), ("Flap", 14), ("Breath", 8), ("Breath", 26)):
        use_action(actions[clip])
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for view, cam in cams.items():
            if view == "top" and (clip, frame) not in (("Glide", 0), ("Flap", 5)):
                continue
            scene.camera = cam
            scene.render.filepath = os.path.join(ROOT, "Saved", f"dragon_{clip}_{frame}_{view}.png")
            bpy.ops.render.render(write_still=True)
    # Where a rider sits: the topmost vertices of the torso in the glide pose, for the saddle constants in ShinobiDragonMount.cpp.
    use_action(actions["Glide"])
    scene.frame_set(0)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = body.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    back = [body.matrix_world @ v.co for v in mesh.vertices if 0.1 < (body.matrix_world @ v.co).x < 0.6 and abs(v.co.y) < 0.12]
    top = max(back, key=lambda p: p.z)
    print("[build_dragon] glide: back top", tuple(round(c, 2) for c in top), "mesh bounds",
          tuple(round(min(getattr(body.matrix_world @ v.co, a) for v in mesh.vertices), 2) for a in "xyz"),
          tuple(round(max(getattr(body.matrix_world @ v.co, a) for v in mesh.vertices), 2) for a in "xyz"))
    for lo, hi in ((0.1, 0.2), (0.2, 0.3), (0.3, 0.4), (0.4, 0.5), (0.5, 0.6)):
        column = sorted((body.matrix_world @ v.co).z for v in mesh.vertices if lo < (body.matrix_world @ v.co).x < hi and abs(v.co.y) < 0.1)
        if column:
            print(f"[build_dragon] glide: x {lo}-{hi}: z median {column[len(column) // 2]:.2f}, 85% {column[int(len(column) * 0.85)]:.2f}, top {column[-1]:.2f}")
    raise SystemExit

# --- export ----------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL", path_mode="COPY")
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Tairn.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, frames, _ in CLIPS:
    use_action(actions[name])
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Tairn_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_dragon] wrote Tairn_{name}.fbx")
