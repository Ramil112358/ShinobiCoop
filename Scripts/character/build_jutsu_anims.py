"""Blender (5.2) headless builder for the jutsu cast animations (jutsu cinematics plan, Task 7).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/build_jutsu_anims.py -- [--only <Jutsu>] [--preview-only]

Poses the Mixamo "Power Up" rig (SourceArt/Mixamo/Power_Up_30fps.fbx, the source skeleton the UE retarget pipeline
already uses) key by key, and writes one animation per jutsu phase:
  SourceArt/Mixamo/Jutsu/<Jutsu>_<Seal|Charge|Release>.fbx  (armature only, 30 fps; imported by setup_jutsu_anims.py)
  Saved/JutsuPreviews/<Jutsu>_<Phase>.png                   (4 frames, front row over side row, for review)

Poses are written as targets, not raw bone angles, since the Mixamo bones' local axes are hard to reason about:
- arms: a wrist target and an elbow pole (armature space), solved with two-bone IK;
- hands: the fingers' direction and the palm's normal;
- fingers: a curl per finger (degrees per joint, towards the palm);
- spine, neck and head: pitch (negative leans back) and yaw.
Armature space is the Mixamo rig's own, in cm: +X = the character's left, +Y = up, +Z = forward. The wrists meet the
chest at about Y 125-135, Z 20-30.

FBX round trip (checked by a spike: every bone within 5 mm of the source after export + re-import): export the armature
only, add_leaf_bones=False, armature_nodetype="NULL", default axes and scale. Blender's importer shifts animation by
one frame (anim_offset=1), so actions here start at frame 0 and the scene range is set to each action before export.
"""
import math
import os
import random
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "SourceArt", "Mixamo", "Power_Up_30fps.fbx")
OUT_DIR = os.path.join(ROOT, "SourceArt", "Mixamo", "Jutsu")
PREVIEW_DIR = os.path.join(ROOT, "Saved", "JutsuPreviews")
FPS = 30
P = "mixamorig:"
FINGERS = ("Thumb", "Index", "Middle", "Ring", "Pinky")

# ---------------------------------------------------------------------------------------------------------------
# Pose library. A pose is a dict with any of:
#   spine: (pitch, yaw) degrees, spread over Spine/Spine1/Spine2; neck, head: (pitch, yaw)
#   R, L: arm spec {wrist, pole, fingers, palm, curl: (thumb, index, middle, ring, pinky)}
# Poses combine with merge(): later keys win, arm specs merge key by key.
# ---------------------------------------------------------------------------------------------------------------
V = Vector
UP, DOWN, FWD, BACK, LEFT, RIGHT = V((0, 1, 0)), V((0, -1, 0)), V((0, 0, 1)), V((0, 0, -1)), V((1, 0, 0)), V((-1, 0, 0))


def arm(wrist, pole, fingers, palm, curl=(10, 10, 10, 10, 10)):
    return {"wrist": V(wrist), "pole": V(pole), "fingers": V(fingers), "palm": V(palm), "curl": curl}


def merge(*poses):
    out = {}
    for pose in poses:
        for key, value in pose.items():
            if key in ("R", "L") and key in out:
                out[key] = {**out[key], **value}
            else:
                out[key] = value
    return out


OPEN = (5, 5, 5, 5, 5)
FIST = (40, 95, 95, 95, 95)
TWO_UP = (15, 0, 0, 85, 85)  # index and middle straight, the rest folded (Tiger)
ELBOW_R, ELBOW_L = (-1, -0.6, 0.1), (1, -0.6, 0.1)  # elbows out and down, as when holding hands at the chest

NEUTRAL = {
    "spine": (0, 0), "neck": (0, 0), "head": (0, 0),
    "R": arm((-21, 93, 3), (0, 0, -1), DOWN, LEFT, (15, 15, 20, 25, 30)),
    "L": arm((21, 93, 3), (0, 0, -1), DOWN, RIGHT, (15, 15, 20, 25, 30)),
}

# Hand seals: both hands in front of the chest. The palm normal of each hand faces the other hand for the clasped ones.
SEALS = {
    # Tiger: palms together, index and middle up, the others interlaced.
    "Tiger": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, TWO_UP),
              "L": arm((4, 127, 20), ELBOW_L, (-0.05, 1, 0.25), RIGHT, TWO_UP)},
    # Ram: right hand as Tiger; the left hand wraps round its two raised fingers.
    "Ram": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, TWO_UP),
            "L": arm((5, 124, 27), ELBOW_L, (-0.7, 0.4, 0.6), (-0.5, 0, -0.8), (30, 70, 75, 80, 85))},
    # Snake: fingers interlaced, palms clasped.
    "Snake": {"R": arm((-4, 125, 26), ELBOW_R, (0.35, 0.6, 0.7), LEFT, (25, 55, 55, 55, 55)),
              "L": arm((4, 125, 26), ELBOW_L, (-0.35, 0.6, 0.7), RIGHT, (25, 55, 55, 55, 55))},
    # Horse: index fingers and thumbs meet in a triangle pointing forward, the rest folded.
    "Horse": {"R": arm((-6, 127, 22), ELBOW_R, (0.3, 0.3, 0.9), (0.4, -0.9, 0), (0, 0, 90, 90, 90)),
              "L": arm((6, 127, 22), ELBOW_L, (-0.3, 0.3, 0.9), (-0.4, -0.9, 0), (0, 0, 90, 90, 90))},
    # Ox: right hand upright, left hand across it horizontally.
    "Ox": {"R": arm((-3, 126, 24), ELBOW_R, (0, 1, 0.2), LEFT, (10, 10, 15, 20, 25)),
           "L": arm((7, 130, 27), ELBOW_L, (-1, 0.1, 0.1), BACK, (10, 20, 25, 30, 35))},
    # Monkey: palms together, the left hand's fingers folded over the right's.
    "Monkey": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, OPEN),
               "L": arm((4, 127, 20), ELBOW_L, (-0.05, 1, 0.25), RIGHT, (20, 45, 45, 45, 45))},
    # Rabbit: left fist, right hand flat on top of it.
    "Rabbit": {"R": arm((-3, 134, 24), ELBOW_R, (0.3, 0, 1), DOWN, OPEN),
               "L": arm((3, 124, 23), ELBOW_L, (-0.2, 0.2, 1), RIGHT, FIST)},
}

def seal(name, body=None):
    return merge(NEUTRAL, body or {}, SEALS[name])


# Body moves.
LEAN_BACK = {"spine": (-7, 0), "neck": (-6, 0), "head": (-8, 0)}           # the Fireball inhale
LEAN_BACK_DEEP = {"spine": (-8.5, 0), "neck": (-7, 0), "head": (-9, 0)}
# Leaning into the blow with the Tiger seal still held, as Sasuke does, which keeps the mouth clear for the fire.
BLOW = seal("Tiger", {"spine": (10, 0), "neck": (8, 0), "head": (12, 0)})
CHIDORI_HOLD = merge(NEUTRAL, {
    "spine": (8, -8), "neck": (-4, 5), "head": (-4, 5),
    # Right hand low and forward, fingers clawed; the left hand grips the right wrist.
    "R": arm((-28, 98, 38), (-0.6, 0, -1), (-0.2, -0.3, 1), DOWN, (30, 35, 35, 35, 35)),
    "L": arm((-20, 103, 33), ELBOW_L, (-1, -0.2, 0.2), BACK, (40, 60, 60, 60, 60)),
})
RASENGAN_HOLD = merge(NEUTRAL, {
    "spine": (5, -6), "neck": (-2, 3), "head": (-2, 3),
    # Right palm up, in front of the belly; left hand hovering over it, palm down.
    "R": arm((-17, 112, 33), (-1, -0.4, -0.3), (0.2, 0, 1), UP, (20, 30, 30, 30, 30)),
    "L": arm((-12, 132, 33), ELBOW_L, (-0.3, -0.2, 1), DOWN, (20, 30, 30, 30, 30)),
})
EYE_FOCUS = merge(NEUTRAL, {
    "spine": (2, 0), "neck": (4, 0), "head": (6, 0),
    # Right hand raised beside the right eye, index and middle up.
    "R": arm((-12, 150, 14), (-1, -0.5, 0), (0.1, 1, 0.2), (0.3, 0, 1), TWO_UP),
})
DASH_STRIKE = merge(NEUTRAL, {
    "spine": (12, -10), "neck": (-6, 8), "head": (-6, 8),
    "R": arm((-12, 128, 52), (-1, -0.5, 0), FWD, DOWN, (30, 35, 35, 35, 35)),
    "L": arm((22, 104, -14), (0.3, 0, 1), DOWN, RIGHT, (30, 40, 40, 40, 40)),
})
PALM_THRUST = merge(DASH_STRIKE, {"R": arm((-10, 125, 54), (-1, -0.5, 0), (0, 0.3, 1), FWD, (15, 20, 20, 20, 20))})
STARE = merge(NEUTRAL, {"spine": (0, 0), "neck": (0, 0), "head": (-3, 0),
                        "R": arm((-16, 140, 45), (-1, -0.5, -0.3), FWD, DOWN, (40, 0, 90, 90, 90))})


# ---------------------------------------------------------------------------------------------------------------
# Timelines: [(seconds, pose)] per phase. Charge is exactly 2.2 s (longer than the 2 s cap, so it never loops).
# ---------------------------------------------------------------------------------------------------------------
def charge_keys(base, wobble, seed, step=0.2):
    """A held pose with a small seeded tremble on the right wrist and the spine."""
    rng = random.Random(seed)
    keys = []
    t = 0.0
    while t <= 2.2 + 1e-6:
        jitter = V((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))) * wobble
        pose = merge(base, {"R": {**base["R"], "wrist": base["R"]["wrist"] + jitter}})
        pitch, yaw = base.get("spine", (0, 0))
        pose["spine"] = (pitch + rng.uniform(-0.6, 0.6), yaw)
        keys.append((round(t, 3), pose))
        t += step
    return keys


def rasengan_charge():
    """The left hand circles over the right palm, shaping the ball."""
    keys = []
    centre = RASENGAN_HOLD["L"]["wrist"]
    for i in range(9):
        t = i * 2.2 / 8
        angle = i * math.pi / 2
        offset = V((math.cos(angle) * 5, 0, math.sin(angle) * 5))
        keys.append((round(t, 3), merge(RASENGAN_HOLD, {"L": {"wrist": centre + offset}})))
    return keys


def breathing(low, high):
    return [(t, merge(seal("Tiger"), low if i % 2 == 0 else high)) for i, t in enumerate((0.0, 0.55, 1.1, 1.65, 2.2))]


TIMELINES = {
    "Fireball": {
        "Seal": [(0.0, NEUTRAL), (0.15, seal("Snake")), (0.35, seal("Ram")), (0.55, seal("Tiger")), (0.9, seal("Tiger", LEAN_BACK))],
        "Charge": breathing(LEAN_BACK, LEAN_BACK_DEEP),
        "Release": [(0.0, seal("Tiger", LEAN_BACK)), (0.12, BLOW), (0.45, BLOW), (0.8, NEUTRAL)],
    },
    "Chidori": {
        "Seal": [(0.0, NEUTRAL), (0.15, seal("Ox")), (0.35, seal("Rabbit")), (0.55, seal("Monkey")), (0.8, CHIDORI_HOLD)],
        "Charge": charge_keys(CHIDORI_HOLD, 1.5, seed=2),
        "Release": [(0.0, CHIDORI_HOLD), (0.15, DASH_STRIKE), (0.7, DASH_STRIKE)],
    },
    "Rasengan": {
        "Seal": [(0.0, NEUTRAL), (0.2, seal("Ram")), (0.45, seal("Tiger")), (0.7, RASENGAN_HOLD)],
        "Charge": rasengan_charge(),
        "Release": [(0.0, RASENGAN_HOLD), (0.15, PALM_THRUST), (0.7, PALM_THRUST)],
    },
    "Amaterasu": {
        "Seal": [(0.0, NEUTRAL), (0.25, seal("Tiger")), (0.7, EYE_FOCUS)],
        "Charge": charge_keys(EYE_FOCUS, 0.4, seed=4, step=0.275),
        "Release": [(0.0, EYE_FOCUS), (0.1, STARE), (0.6, merge(STARE, {"R": NEUTRAL["R"]}))],
    },
}


# ---------------------------------------------------------------------------------------------------------------
# Posing
# ---------------------------------------------------------------------------------------------------------------
def basis(y, x_hint):
    """Rotation whose Y axis is y and whose X axis is as close to x_hint as possible (X = Y x Z, Blender bones)."""
    y = y.normalized()
    x = (x_hint - y * x_hint.dot(y))
    if x.length < 1e-4:
        x = y.orthogonal()
    x.normalize()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


def set_rotation(arm_obj, name, rotation):
    """Poses a bone to an armature-space rotation, keeping its (posed) head where it is."""
    pb = arm_obj.pose.bones[name]
    pb.matrix = Matrix.Translation(pb.head) @ rotation.to_4x4()
    pb.location = (0, 0, 0)
    bpy.context.view_layer.update()


def pose_arm(arm_obj, side, spec):
    names = [f"{P}{side}Arm", f"{P}{side}ForeArm", f"{P}{side}Hand"]
    upper, fore = (arm_obj.data.bones[n] for n in names[:2])
    l1 = (upper.tail_local - upper.head_local).length
    l2 = (fore.tail_local - fore.head_local).length
    shoulder = arm_obj.pose.bones[names[0]].head.copy()
    to_target = spec["wrist"] - shoulder
    reach = max(min(to_target.length, (l1 + l2) * 0.995), abs(l1 - l2) + 1.0)
    direction = to_target.normalized()
    pole = spec["pole"].normalized()
    pole = (pole - direction * pole.dot(direction)).normalized()
    a = (l1 * l1 - l2 * l2 + reach * reach) / (2 * reach)
    h = math.sqrt(max(l1 * l1 - a * a, 0.0))
    elbow = shoulder + direction * a + pole * h
    wrist = shoulder + direction * reach
    # Arm bones: X points to the inside of the bend (away from the elbow's pole), as in the rest pose.
    set_rotation(arm_obj, names[0], basis(elbow - shoulder, -pole))
    set_rotation(arm_obj, names[1], basis(wrist - elbow, -pole))
    # Hand: Y along the fingers, Z the palm normal, so X = Y x Z.
    fingers = spec["fingers"].normalized()
    palm = (spec["palm"] - fingers * spec["palm"].dot(fingers)).normalized()
    set_rotation(arm_obj, names[2], Matrix((fingers.cross(palm), fingers, palm)).transposed())
    for finger, curl in zip(FINGERS, spec["curl"]):
        for joint in (1, 2, 3):
            pb = arm_obj.pose.bones[f"{P}{side}Hand{finger}{joint}"]
            # Thumb joints bend less; each joint turns about its own X, towards the palm.
            angle = math.radians(curl * (0.6 if finger == "Thumb" else 1.0))
            pb.rotation_quaternion = Matrix.Rotation(angle, 3, "X").to_quaternion()


def pose_torso(arm_obj, pose):
    pitch, yaw = pose.get("spine", (0, 0))
    for name in ("Spine", "Spine1", "Spine2"):
        # The spine bones' X runs across the body: turning about +X leans forward.
        arm_obj.pose.bones[P + name].rotation_quaternion = (Matrix.Rotation(math.radians(pitch), 3, "X") @ Matrix.Rotation(math.radians(yaw / 3), 3, "Y")).to_quaternion()
    for name, key in (("Neck", "neck"), ("Head", "head")):
        p, y = pose.get(key, (0, 0))
        arm_obj.pose.bones[P + name].rotation_quaternion = (Matrix.Rotation(math.radians(p), 3, "X") @ Matrix.Rotation(math.radians(y), 3, "Y")).to_quaternion()
    bpy.context.view_layer.update()


def apply_pose(arm_obj, pose):
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    bpy.context.view_layer.update()
    pose_torso(arm_obj, pose)
    for side, key in (("Right", "R"), ("Left", "L")):
        if key in pose:
            pose_arm(arm_obj, side, pose[key])
    bpy.context.view_layer.update()


def build_action(arm_obj, name, keys):
    action = bpy.data.actions.new(name)
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = action
    for seconds, pose in keys:
        frame = round(seconds * FPS)
        apply_pose(arm_obj, pose)
        for pb in arm_obj.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("location", frame=frame)
    return action, round(keys[-1][0] * FPS)


# ---------------------------------------------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------------------------------------------
def export(arm_obj, path, last_frame):
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = 0, last_frame
    for o in bpy.data.objects:
        o.select_set(o == arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                             bake_anim_force_startend_keying=True, armature_nodetype="NULL")


def setup_preview_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "SINGLE"
    scene.render.resolution_x, scene.render.resolution_y = 300, 420
    scene.render.film_transparent = False
    scene.world = bpy.data.worlds.new("Preview")
    scene.world.color = (0.05, 0.05, 0.06)
    cameras = {}
    # The rig faces world -Y (its +Z forward, turned by the import's X rotation); these frame the upper body.
    for label, location, rotation in (("front", (0, -2.6, 1.3), (math.radians(90), 0, 0)),
                                      ("side", (-2.6, 0, 1.3), (math.radians(90), 0, math.radians(-90)))):
        data = bpy.data.cameras.new(label)
        data.type = "ORTHO"
        data.ortho_scale = 1.35
        cam = bpy.data.objects.new(label, data)
        cam.location, cam.rotation_euler = location, rotation
        scene.collection.objects.link(cam)
        cameras[label] = cam
    return cameras


def render_strip(cameras, last_frame, path):
    scene = bpy.context.scene
    frames = [round(last_frame * i / 3) for i in range(4)]
    tiles = []
    for label in ("front", "side"):
        scene.camera = cameras[label]
        row = []
        for frame in frames:
            scene.frame_set(frame)
            tmp = os.path.join(PREVIEW_DIR, "_tile.png")
            scene.render.filepath = tmp
            bpy.ops.render.render(write_still=True)
            image = bpy.data.images.load(tmp)
            row.append((list(image.pixels), image.size[0], image.size[1]))
            bpy.data.images.remove(image)
        tiles.append(row)
    w, h = tiles[0][0][1], tiles[0][0][2]
    strip = bpy.data.images.new("strip", w * 4, h * 2, alpha=True)
    pixels = [0.0] * (w * 4 * h * 2 * 4)
    for r, row in enumerate(reversed(tiles)):  # Blender images start at the bottom row: side row first
        for c, (tile, _, _) in enumerate(row):
            for y in range(h):
                src = y * w * 4
                dst = ((r * h + y) * w * 4 + c * w) * 4
                pixels[dst:dst + w * 4] = tile[src:src + w * 4]
    strip.pixels = pixels
    strip.filepath_raw = path
    strip.file_format = "PNG"
    strip.save()
    os.remove(os.path.join(PREVIEW_DIR, "_tile.png"))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    preview_only = "--preview-only" in argv
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    arm_obj = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    if arm_obj.animation_data:
        arm_obj.animation_data.action = None
    bpy.context.scene.render.fps = FPS
    cameras = setup_preview_scene()

    for jutsu, phases in TIMELINES.items():
        if only and jutsu != only:
            continue
        for phase, keys in phases.items():
            action, last_frame = build_action(arm_obj, f"{jutsu}_{phase}", keys)
            if not preview_only:
                export(arm_obj, os.path.join(OUT_DIR, f"{jutsu}_{phase}.fbx"), last_frame)
            render_strip(cameras, last_frame, os.path.join(PREVIEW_DIR, f"{jutsu}_{phase}.png"))
            print(f"[build_jutsu_anims] {jutsu}_{phase}: {last_frame + 1} frames")


main()
