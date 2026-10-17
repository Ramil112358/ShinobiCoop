"""Rigs and animates the summoned owl: the Tripo model of the user's snowy-owl plush (SourceArt/Tripo/Owl) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_owl.py            (add  -- preview [Clip...]  to render the clips to Saved/owl_*.png,
                                                               or  -- rig  to render the joints and weights at rest)

Tripo's humanoid auto-rig (Mixamo names) fits her well: arms run out along the wings, legs down to the felt feet, and every vertex
is weighted. It is kept; the finger joints inside each wing tip are folded into the hand, so a wing is shoulder, arm, forearm, hand.
She never lands: she hovers for her whole stay. Clips: Hover (a 0.5 s flapping loop) and Strike (wings up, a hard down-beat as the
bolt lands 0.4 s in, AShinobiOwl::WindUp, and back to Hover's first pose). Writes SourceArt/Creatures/Owl/Owl.fbx (mesh +
skeleton), Owl_<Clip>.fbx per clip and Owl.png (the base colour, 2k; Tripo made no normal or roughness map).
She faces +X with her body centred over the origin, the lowest point of her feet at Z = 0, at the model's size (1.0 m tall,
0.89 m wingtip to wingtip); the game shows her at AShinobiOwl::Scale 0.45. Metres.
"""
import json
import math
import os
import struct
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "SourceArt", "Tripo", "Owl", "owl_humanoid_rigged.fbx")
GLB = os.path.join(ROOT, "SourceArt", "Tripo", "Owl", "owl_humanoid_rigged.glb")  # the only file with the texture
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "Owl")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PREVIEW = "preview" in ARGS
RIG_CHECK = "rig" in ARGS
FPS = 30
HOVER_FRAMES = 15   # one wing beat: 0.5 s
STRIKE_FRAMES = 24  # 0.8 s
STRIKE_AT = 12      # the down-beat, 0.4 s in: AShinobiOwl::WindUp
I3 = Matrix.Identity(3)
M = "mixamorig:"
SIDES = (("Left", 1), ("Right", -1))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
scene = bpy.context.scene
scene.render.fps = FPS
body = next(o for o in bpy.data.objects if o.type == "MESH")
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body.name, arm.name = "Body", "Armature"
os.makedirs(OUT, exist_ok=True)


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def rx(deg):  # about world X (her forward): positive rolls her left side (+Y) up, so it raises the left wing and lowers the right
    return Matrix.Rotation(math.radians(deg), 3, "X")


def ry(deg):  # about world Y: positive tips her beak (+X) down and swings hanging legs back
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):  # about world Z: positive turns her beak to her left (+Y); on a spread wing it sweeps the left wing back
    return Matrix.Rotation(math.radians(deg), 3, "Z")


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# --- fit: face +X, body centred, feet on the ground -----------------------------------------------------------------------
# Tripo's model faces -Y with her left wing along +X. The wings (beyond 0.15 m out) are left out of the centring.
FIT = Matrix.Rotation(math.radians(90), 4, "Z")
arm.matrix_world = FIT @ arm.matrix_world
select(arm, body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.context.view_layer.update()
points = [body.matrix_world @ v.co for v in body.data.vertices]
torso = [p for p in points if abs(p.y) < 0.15]
SHIFT = Vector((-(min(p.x for p in torso) + max(p.x for p in torso)) / 2, -(min(p.y for p in torso) + max(p.y for p in torso)) / 2, -min(p.z for p in points)))
arm.location += SHIFT
select(arm, body)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
bpy.context.view_layer.update()
print("[build_owl] shifted by", tuple(round(c, 3) for c in SHIFT))

# --- the wing tips ----------------------------------------------------------------------------------------------------------
# Tripo's hand has finger joints spread through the wing tip; bending them would only crease the felt. Their weights go to the hand
# and the joints go.
data = arm.data
FINGERS = [b.name for b in data.bones if "Hand" in b.name and not b.name.endswith("Hand")]
for name in FINGERS:
    hand = name.split("Hand")[0] + "Hand"
    src = body.vertex_groups.get(name)
    if src is None:
        continue
    dst = body.vertex_groups.get(hand) or body.vertex_groups.new(name=hand)
    for v in body.data.vertices:
        w = next((g.weight for g in v.groups if g.group == src.index), 0.0)
        if w > 0:
            dst.add([v.index], w, "ADD")
    body.vertex_groups.remove(src)
select(arm)
bpy.ops.object.mode_set(mode="EDIT")
for name in FINGERS:
    data.edit_bones.remove(data.edit_bones[name])
bpy.ops.object.mode_set(mode="OBJECT")
weighted = sum(1 for v in body.data.vertices if sum(g.weight for g in v.groups) > 0.01)
print("[build_owl] bones", len(data.bones), "- weighted", weighted, "of", len(body.data.vertices))

# --- the texture ----------------------------------------------------------------------------------------------------------
# Only the GLB carries it (the FBX's material has none). Read straight out of the GLB's binary chunk: importing the GLB itself gives
# NaN bones in Blender.
def glb_image():
    blob = open(GLB, "rb").read()
    length = struct.unpack("<I", blob[12:16])[0]
    gltf = json.loads(blob[20:20 + length])
    binary = 20 + length + 8
    view = gltf["bufferViews"][gltf["images"][0]["bufferView"]]
    start = binary + view.get("byteOffset", 0)
    return blob[start:start + view["byteLength"]]


jpg = os.path.join(bpy.app.tempdir, "owl_basecolor.jpg")
with open(jpg, "wb") as f:
    f.write(glb_image())
image = bpy.data.images.load(jpg)
material = body.data.materials[0]
material.use_nodes = True
nodes = material.node_tree.nodes
texture = nodes.new("ShaderNodeTexImage")
texture.image = image
bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
material.node_tree.links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
if not (PREVIEW or RIG_CHECK):
    image.scale(2048, 2048)
    image.filepath_raw = os.path.join(OUT, "Owl.png")
    image.file_format = "PNG"
    image.save()

# --- rig check: joints and weights at rest ----------------------------------------------------------------------------------
def camera_rig(ortho):
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x, scene.render.resolution_y = 600, 500
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = ortho
    return cam


def shoot(cam, name, d, centre):
    d = Vector(d).normalized()
    cam.location = centre + d * 3
    cam.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    scene.render.filepath = os.path.join(ROOT, "Saved", name)
    bpy.ops.render.render(write_still=True)


VIEWS = (("front", (1, 0.0001, 0.15)), ("side", (0.0001, -1, 0.05)), ("quarter", (1, -1, 0.5)), ("back", (-1, 0.3, 0.6)))
CENTRE = Vector((0, 0, 0.5))
names = {g.index: g.name for g in body.vertex_groups}

if RIG_CHECK:
    palette = [(0.9, 0.1, 0.1), (0.1, 0.8, 0.1), (0.1, 0.3, 0.95), (0.95, 0.85, 0.1), (0.85, 0.1, 0.85), (0.1, 0.85, 0.85),
               (1, 0.5, 0), (0.5, 0.25, 0.1), (0.6, 0.6, 1), (0.3, 0.6, 0.3), (1, 0.6, 0.7), (0.4, 0.4, 0.4)]
    colours = body.data.color_attributes.new("Weights", "FLOAT_COLOR", "POINT")
    group_list = sorted(names.values())
    for v in body.data.vertices:
        top = max(v.groups, key=lambda g: g.weight, default=None)
        c = palette[group_list.index(names[top.group]) % len(palette)] if top else (0, 0, 0)
        colours.data[v.index].color = (*c, 1)
    print("[build_owl] colours:", {n: palette[i % len(palette)] for i, n in enumerate(group_list)})
    cam = camera_rig(1.15)
    scene.display.shading.color_type = "VERTEX"
    for view, d in VIEWS:
        shoot(cam, f"owl_rig_weights_{view}.png", d, CENTRE)
    raise SystemExit

# --- animation -------------------------------------------------------------------------------------------------------------
PARENT = {b.name: b.parent.name if b.parent else None for b in data.bones}
ORDER = []
while len(ORDER) < len(PARENT):  # parents before children
    ORDER += [n for n, p in PARENT.items() if n not in ORDER and (p is None or p in ORDER)]
REST = {n: data.bones[n].matrix_local.to_3x3() for n in ORDER}
HIPS = M + "Hips"
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"


def key(frame, rot, lift=Vector()):
    """Keys the whole pose at `frame`. rot: bone -> world-axis turn added on top of its parent's (children follow parents);
    lift moves the body."""
    world = {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent_world = world[PARENT[name]] if PARENT[name] else I3
        delta = rot.get(name, I3)
        world[name] = delta @ parent_world
        pb.rotation_quaternion = (REST[name].inverted() @ parent_world.inverted() @ delta @ parent_world @ REST[name]).to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == HIPS:
            pb.location = REST[name].inverted() @ lift
            pb.keyframe_insert("location", frame=frame)


def pose(wing, forearm, hand, sweep=0.0, pitch=0.0, legs=15.0, bob=0.0):
    """Her pose from a few numbers, so the clips can blend between poses: wing raises both wings at the shoulder (degrees, negative
    lowers them), forearm and hand bend the outer wing on top (positive up), sweep brings both wings forward (positive), pitch tips
    her head beak-down (negative: up), legs swings both legs back, bob lifts her body (m)."""
    rot = {M + "Neck": ry(pitch * 0.4), M + "Head": ry(pitch * 0.6), M + "Spine2": ry(-wing * 0.04)}
    for side, k in SIDES:
        rot[f"{M}{side}Arm"] = rx(wing * k) @ rz(-sweep * k)
        rot[f"{M}{side}ForeArm"] = rx(forearm * k)
        rot[f"{M}{side}Hand"] = rx(hand * k)
        rot[f"{M}{side}UpLeg"] = ry(legs)
        rot[f"{M}{side}Foot"] = ry(legs * 0.6)
    return rot, Vector((0, 0, bob))


def hover_numbers(t):
    """Hover at t 0-1 of one beat: the wings start at the top, the outer wing trails the stroke, she rises on the down-stroke."""
    beat = lambda lag: math.cos(2 * math.pi * (t - lag))
    return dict(wing=35 * beat(0), forearm=14 * beat(0.1), hand=16 * beat(0.18), sweep=6 * math.sin(2 * math.pi * t),
                pitch=-3 * beat(0.25), legs=15 + 4 * beat(0.3), bob=0.015 * math.sin(2 * math.pi * t))


def blend(a, b, u):
    return {k: a[k] + (b[k] - a[k]) * u for k in a}


def hover():
    for f in range(HOVER_FRAMES + 1):
        key(f, *pose(**hover_numbers(f / HOVER_FRAMES)))
    return HOVER_FRAMES


def strike():
    """Wings sweep up and hold with her head tipped up, a hard down-beat at STRIKE_AT as the bolt lands, then back to Hover's start."""
    start = hover_numbers(0)
    raised = dict(wing=70, forearm=22, hand=18, sweep=-8, pitch=-18, legs=10, bob=-0.01)
    down = dict(wing=-45, forearm=-12, hand=-18, sweep=14, pitch=8, legs=25, bob=0.05)
    for f in range(STRIKE_FRAMES + 1):
        if f <= 8:
            n = blend(start, raised, smooth(f / 8))
        elif f <= STRIKE_AT - 2:
            n = raised  # the hold
        elif f <= STRIKE_AT:
            n = blend(raised, down, smooth((f - STRIKE_AT + 2) / 2))
        else:
            n = blend(down, start, smooth((f - STRIKE_AT) / (STRIKE_FRAMES - STRIKE_AT)))
        key(f, *pose(**n))
    return STRIKE_FRAMES


def new_action(name):
    action = bpy.data.actions.new(name)
    arm.animation_data_create()
    use_action(action)
    return action


def use_action(action):
    """Blender 5 actions are slotted: making one current also needs its slot assigned."""
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]


CLIPS = (("Hover", hover), ("Strike", strike))
actions = {}
for name, build in CLIPS:
    action = new_action(name)
    actions[name] = (action, build())
    print(f"[build_owl] clip {name}: {actions[name][1] + 1} frames ({actions[name][1] / FPS:.2f} s)")

# --- previews --------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.display.shading.color_type = "TEXTURE"
    cam = camera_rig(1.15)
    only = [a for a in ARGS if a != "preview"]
    for clip, (action, frames) in actions.items():
        if only and clip not in only:
            continue
        use_action(action)
        for view, d in VIEWS[:3]:
            for f in sorted({0, frames // 4, frames // 2, 3 * frames // 4, frames} | ({8, STRIKE_AT} if clip == "Strike" else set())):
                scene.frame_set(f)
                shoot(cam, f"owl_{clip}_{view}_{f:02d}.png", d, CENTRE)
    raise SystemExit

# --- export ----------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL",
              path_mode="COPY", use_armature_deform_only=True)
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Owl.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, (action, frames) in actions.items():
    use_action(action)
    scene.frame_start, scene.frame_end = 0, frames
    select(arm)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Owl_{name}.fbx"), use_selection=True, object_types={"ARMATURE"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_owl] wrote Owl_{name}.fbx")
