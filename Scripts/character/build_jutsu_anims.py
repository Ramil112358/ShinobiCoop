"""Blender (5.2) headless builder for the jutsu cast animations (jutsu cinematics plan, Task 7).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/build_jutsu_anims.py -- [--only <Jutsu>] [--preview-only]

Poses the Mixamo "Power Up" rig (SourceArt/Mixamo/Power_Up_30fps.fbx, the source skeleton the UE retarget pipeline
already uses) key by key, and writes one animation per jutsu phase:
  SourceArt/Mixamo/Jutsu/<Jutsu>_<Seal|Charge|Release>.fbx  (armature only, 30 fps; imported by setup_jutsu_anims.py)
  Saved/JutsuPreviews/<Jutsu>_<Phase>.png                   (6 frames: a 3/4 front row over a side row, for review)

Poses are written as targets, not raw bone angles, since the Mixamo bones' local axes are hard to reason about:
- stance: pelvis drop / yaw / pitch, and a planted ankle target per foot (two-bone leg IK, knees bend to reach);
- arms: a wrist target and an elbow pole, solved with two-bone IK;
- hands: the fingers' direction and the palm's normal; fingers: a curl per finger (degrees per joint, towards the palm);
- spine, neck and head: pitch (negative leans back) and yaw (positive turns to the character's left).
Armature space is the Mixamo rig's own, in cm: +X = the character's left, +Y = up, +Z = forward, the ground at Y 0.

Motion follows the usual game-animation rules, per key: "snap" (hand seals: a fast exponential move, then a hold key),
"back" (strikes: overshoot and settle), "smooth" (Bezier ease in and out); every release has an anticipation key
before the action and a settle after it.

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
ANKLE_HEIGHT = 8.7

# ---------------------------------------------------------------------------------------------------------------
# Pose library. A pose is a dict with any of:
#   hips: (drop cm, yaw, pitch); feet: {"R"/"L": (x, z, yaw)} ankle targets on the ground
#   spine: (pitch, yaw) spread over Spine/Spine1/Spine2; neck, head: (pitch, yaw)
#   R, L: arm spec {wrist, pole, fingers, palm, curl: (thumb, index, middle, ring, pinky)}
# merge() combines poses: later keys win; arm and feet specs merge key by key.
# ---------------------------------------------------------------------------------------------------------------
V = Vector
UP, DOWN, FWD, BACK, LEFT, RIGHT = V((0, 1, 0)), V((0, -1, 0)), V((0, 0, 1)), V((0, 0, -1)), V((1, 0, 0)), V((-1, 0, 0))


def arm(wrist, pole, fingers, palm, curl=(10, 10, 10, 10, 10)):
    return {"wrist": V(wrist), "pole": V(pole), "fingers": V(fingers), "palm": V(palm), "curl": curl}


def merge(*poses):
    out = {}
    for pose in poses:
        for key, value in pose.items():
            if key in ("R", "L", "feet") and key in out:
                out[key] = {**out[key], **value}
            else:
                out[key] = value
    return out


def shift(pose, side, offset):
    """The pose with one wrist moved by offset (cm)."""
    return merge(pose, {side: {"wrist": pose[side]["wrist"] + V(offset)}})


OPEN = (5, 5, 5, 5, 5)
FIST = (40, 95, 95, 95, 95)
CLAW = (30, 40, 45, 50, 55)
TWO_UP = (15, 0, 0, 85, 85)  # index and middle straight, the rest folded (Tiger)
ELBOW_R, ELBOW_L = (-1, -0.6, 0.1), (1, -0.6, 0.1)  # elbows out and down, as when holding hands at the chest

# Stances. Right-handed: the right foot drops back for the ready stance and steps forward to strike.
IDLE = {"hips": (2, 0, 0), "feet": {"R": (-11, 0, -8), "L": (11, 0, 8)}}
# Kept moderate: on the hero's proportions a deeper Mixamo pelvis drop reads as a sumo squat.
READY = {"hips": (4, -12, 0), "feet": {"R": (-13, -8, -20), "L": (12, 9, 8)}}
BRACE = {"hips": (7, -16, 0), "feet": {"R": (-15, -15, -25), "L": (13, 13, 5)}}
CROUCH = {"hips": (12, -20, 10), "feet": {"R": (-17, -12, -25), "L": (15, 16, 8)}}
LUNGE = {"hips": (11, 16, 12), "feet": {"R": (-14, 36, 5), "L": (15, -32, 20)}}
RUN_A = {"hips": (6, 10, 14), "feet": {"R": (-12, 28, 0), "L": (12, -22, 10)}}
RUN_B = {"hips": (5, -10, 14), "feet": {"R": (-12, -22, -10), "L": (12, 28, 0)}}

ARMS_DOWN = {
    "R": arm((-21, 93, 3), (0, 0, -1), DOWN, LEFT, (15, 15, 20, 25, 30)),
    "L": arm((21, 93, 3), (0, 0, -1), DOWN, RIGHT, (15, 15, 20, 25, 30)),
}
NEUTRAL = merge({"spine": (0, 0), "neck": (0, 0), "head": (0, 0)}, IDLE, ARMS_DOWN)
# Arms loose but held a little forward and out, clear of the turned hips.
ARMS_READY = {
    "R": arm((-25, 94, 12), (-0.3, 0, -1), (0, -1, 0.3), LEFT, (20, 25, 30, 35, 40)),
    "L": arm((25, 94, 10), (0.3, 0, -1), (0, -1, 0.3), RIGHT, (20, 25, 30, 35, 40)),
}
READY_BODY = merge(READY, {"spine": (3, 9), "neck": (0, 2), "head": (-2, 3)}, ARMS_READY)  # shoulders turned back to the front

# Hand seals: both hands in front of the chest. For the clasped ones each palm faces the other hand.
SEALS = {
    # Tiger: palms together, index and middle up, the others interlaced.
    "Tiger": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, TWO_UP),
              "L": arm((4, 127, 20), ELBOW_L, (-0.05, 1, 0.25), RIGHT, TWO_UP)},
    # Ram: right hand as Tiger; the left hand wraps round its two raised fingers.
    "Ram": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, TWO_UP),
            "L": arm((5, 124, 22), ELBOW_L, (-0.7, 0.4, 0.6), (-0.5, 0, -0.8), (30, 70, 75, 80, 85))},
    # Snake: fingers interlaced, palms clasped.
    "Snake": {"R": arm((-4, 125, 21), ELBOW_R, (0.35, 0.6, 0.7), LEFT, (25, 55, 55, 55, 55)),
              "L": arm((4, 125, 21), ELBOW_L, (-0.35, 0.6, 0.7), RIGHT, (25, 55, 55, 55, 55))},
    # Ox: right hand upright, left hand across it horizontally.
    "Ox": {"R": arm((-3, 126, 19), ELBOW_R, (0, 1, 0.2), LEFT, (10, 10, 15, 20, 25)),
           "L": arm((7, 130, 22), ELBOW_L, (-1, 0.1, 0.1), BACK, (10, 20, 25, 30, 35))},
    # Monkey: palms together, the left hand's fingers folded over the right's.
    "Monkey": {"R": arm((-4, 127, 20), ELBOW_R, (0.05, 1, 0.25), LEFT, OPEN),
               "L": arm((4, 127, 20), ELBOW_L, (-0.05, 1, 0.25), RIGHT, (20, 45, 45, 45, 45))},
    # Rabbit: left fist, right hand flat on top of it.
    "Rabbit": {"R": arm((-3, 134, 19), ELBOW_R, (0.3, 0, 1), DOWN, OPEN),
               "L": arm((3, 124, 18), ELBOW_L, (-0.2, 0.2, 1), RIGHT, FIST)},
}


def seal(name, *body):
    return merge(NEUTRAL, READY_BODY, *body, SEALS[name])


# Fireball: breathe in leaning back on a braced stance, then throw the whole body into the blow, seal still held
# (as Sasuke does), which keeps the mouth clear for the fire.
INHALE = merge(BRACE, {"spine": (-8, 9), "neck": (-7, 2), "head": (-10, 3)})
INHALE_DEEP = merge(INHALE, {"spine": (-10, 9), "head": (-13, 3)})
BLOW = seal("Tiger", BRACE, {"hips": (8, -16, 8), "spine": (7, 9), "neck": (10, 2), "head": (-24, 3)})  # neck thrust, face level (spine pitch is per bone, x3)
BLOW_RECOIL = seal("Tiger", BRACE, {"hips": (7, -16, 3), "spine": (4, 9), "neck": (5, 2), "head": (-12, 3)})

# Chidori: crouched low, right hand down and forward in a claw, the left hand gripping the right wrist.
CHIDORI_HOLD = merge(NEUTRAL, CROUCH, {
    "spine": (10, 14), "neck": (-8, 4), "head": (-8, 6),
    "R": arm((-26, 84, 38), (-0.6, 0, -1), (-0.2, -0.3, 1), DOWN, CLAW),
    "L": arm((-18, 90, 33), ELBOW_L, (-1, -0.2, 0.2), BACK, (40, 60, 60, 60, 60)),
})
CHIDORI_DEEP = merge(CHIDORI_HOLD, {"hips": (16, -22, 14), "spine": (13, 14)})
# Rasengan: right palm up in front of the belly, the left hand shaping the ball above it.
RASENGAN_HOLD = merge(NEUTRAL, READY, {
    "spine": (6, 4), "neck": (-2, 3), "head": (-4, 3),
    "R": arm((-17, 110, 33), (-1, -0.4, -0.3), (0.2, 0, 1), UP, (20, 30, 30, 30, 30)),
    "L": arm((-12, 130, 33), ELBOW_L, (-0.3, -0.2, 1), DOWN, (20, 30, 30, 30, 30)),
})
# Amaterasu: two fingers raised beside the right eye, head bowed, then a sharp look up.
EYE_FOCUS = merge(NEUTRAL, READY_BODY, {
    "spine": (4, 9), "neck": (6, 2), "head": (10, 3),
    "R": arm((-12, 148, 14), (-1, -0.5, 0), (0.1, 1, 0.2), (0.3, 0, 1), TWO_UP),
})
EYE_BOWED = merge(EYE_FOCUS, {"neck": (10, 2), "head": (16, 3)})
STARE = merge(NEUTRAL, READY_BODY, {"spine": (-2, 9), "neck": (-3, 2), "head": (-6, 3),
                                    "R": arm((-16, 138, 45), (-1, -0.5, -0.3), FWD, DOWN, (40, 0, 90, 90, 90))})

# The dashes: wind up, run in (two strides), then strike on a lunge.
BACK_ARM = {"L": arm((27, 95, -26), (0.3, 0, 1), (0.2, -1, -0.4), RIGHT, (30, 40, 40, 40, 40))}
STRIKE_ARMS = merge(BACK_ARM, {"R": arm((-8, 130, 62), (-1, -0.5, 0), FWD, DOWN, (20, 15, 15, 15, 15))})
PALM_ARMS = merge(BACK_ARM, {"R": arm((-6, 126, 62), (-1, -0.5, 0), (0, 0.4, 1), FWD, (15, 10, 10, 10, 10))})
RUN_TORSO = {"spine": (14, -6), "neck": (-10, 4), "head": (-10, 4)}
LUNGE_TORSO = {"spine": (14, -14), "neck": (-8, 8), "head": (-8, 8)}


def dash_release(hold, windup_arms, strike_arms):
    windup = merge(hold, {"spine": (8, 26)}, windup_arms)
    run_a = merge(NEUTRAL, RUN_A, RUN_TORSO, strike_arms)
    run_b = merge(NEUTRAL, RUN_B, RUN_TORSO, strike_arms)
    lunge = merge(NEUTRAL, LUNGE, LUNGE_TORSO, strike_arms)
    return [(0.0, hold, "smooth"), (0.07, windup, "snap"), (0.16, run_a, "smooth"), (0.28, run_b, "smooth"),
            (0.42, lunge, "back"), (0.7, merge(lunge, {"spine": (11, -12)}), "smooth")]


# ---------------------------------------------------------------------------------------------------------------
# Timelines: [(seconds, pose, ease)] per phase; ease shapes the move from that key to the next.
# Charge is exactly 2.2 s (longer than the 2 s cap, so it never loops).
# ---------------------------------------------------------------------------------------------------------------
def seal_run(names, start, step=0.14, hold=0.07, body=()):
    """Snappy seal changes: each seal is reached fast, then held."""
    keys = []
    t = start
    for name in names:
        keys.append((round(t, 3), seal(name, *body), "snap"))
        keys.append((round(t + hold, 3), seal(name, *body), "snap"))
        t += step
    return keys, t


def tremble(pose, seconds, amount, seed, step, build=None):
    """A held pose with a seeded tremble on the right wrist and the spine; build(t 0-1) layers a slow change."""
    rng = random.Random(seed)
    keys = []
    count = int(round(seconds / step))
    for i in range(count + 1):
        t = i * seconds / count
        base = build(t / seconds) if build else pose
        jitter = V((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))) * amount
        moved = shift(base, "R", jitter)
        pitch, yaw = base.get("spine", (0, 0))
        moved["spine"] = (pitch + rng.uniform(-0.8, 0.8), yaw)
        keys.append((round(t, 3), moved, "smooth"))
    return keys


def lerp_pose(a, b, t):
    """Blends the numeric parts two poses share (stance, torso, wrists); the rest comes from b."""
    out = merge(b)
    for key in ("spine", "neck", "head", "hips"):
        if key in a and key in b:
            out[key] = tuple(x + (y - x) * t for x, y in zip(a[key], b[key]))
    for side in ("R", "L"):
        if side in a and side in b:
            out[side] = {**b[side], "wrist": a[side]["wrist"].lerp(b[side]["wrist"], t)}
    return out


def fireball():
    seals, t = seal_run(("Snake", "Ram", "Tiger"), 0.1)
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(0.95, seal("Tiger", INHALE), "smooth")],
        "Charge": [(round(i * 0.55, 3), seal("Tiger", INHALE if i % 2 == 0 else INHALE_DEEP), "smooth") for i in range(5)],
        "Release": [(0.0, seal("Tiger", INHALE), "smooth"),
                    (0.08, seal("Tiger", INHALE_DEEP), "snap"),      # the last gulp of air
                    (0.16, BLOW, "back"),                            # throw the body into the blow
                    (0.45, BLOW_RECOIL, "smooth"),
                    (0.85, merge(NEUTRAL, READY_BODY), "smooth")],
    }


def chidori():
    seals, t = seal_run(("Ox", "Rabbit", "Monkey"), 0.1)
    windup = {"R": arm((-24, 104, 4), (-0.6, 0, -1), (-0.2, -0.3, 1), DOWN, CLAW)}
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(0.85, CHIDORI_HOLD, "smooth")],
        "Charge": tremble(CHIDORI_HOLD, 2.2, 1.6, seed=2, step=0.1, build=lambda u: lerp_pose(CHIDORI_HOLD, CHIDORI_DEEP, u)),
        "Release": dash_release(CHIDORI_DEEP, windup, STRIKE_ARMS),
    }


def rasengan():
    seals, t = seal_run(("Ram", "Tiger"), 0.1)
    centre = RASENGAN_HOLD["L"]["wrist"]

    def charge_key(i):
        angle = i * math.pi / 2
        pose = merge(RASENGAN_HOLD, {"L": {"wrist": centre + V((math.cos(angle) * 6, math.sin(angle * 2) * 1.5, math.sin(angle) * 6))},
                                     "hips": (8, -12 + math.sin(angle) * 5, 0)})
        return (round(i * 2.2 / 12, 3), pose, "smooth")

    windup = {"R": arm((-22, 106, 6), (-1, -0.4, -0.3), (0.2, 0, 1), UP, (20, 30, 30, 30, 30))}
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(0.7, RASENGAN_HOLD, "smooth")],
        "Charge": [charge_key(i) for i in range(13)],
        "Release": dash_release(RASENGAN_HOLD, windup, PALM_ARMS),
    }


def amaterasu():
    seals, t = seal_run(("Tiger",), 0.12, hold=0.12)
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(0.7, EYE_FOCUS, "smooth")],
        "Charge": tremble(EYE_FOCUS, 2.2, 0.4, seed=4, step=0.275, build=lambda u: lerp_pose(EYE_FOCUS, EYE_BOWED, min(u * 2, 1))),
        "Release": [(0.0, EYE_BOWED, "smooth"), (0.05, merge(EYE_BOWED, {"head": (20, 3)}), "snap"),
                    (0.12, STARE, "back"), (0.6, merge(STARE, ARMS_READY), "smooth")],
    }


TIMELINES = {"Fireball": fireball(), "Chidori": chidori(), "Rasengan": rasengan(), "Amaterasu": amaterasu()}


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


def rest_rotation(arm_obj, name):
    return arm_obj.data.bones[name].matrix_local.to_3x3()


def swung(arm_obj, name, direction):
    """The bone's rest rotation swung (no twist) so its Y axis points along direction."""
    rest = rest_rotation(arm_obj, name)
    return rest.col[1].rotation_difference(direction.normalized()).to_matrix() @ rest


def set_rotation(arm_obj, name, rotation, head=None):
    """Poses a bone to an armature-space rotation, keeping its (posed) head where it is unless head is given."""
    pb = arm_obj.pose.bones[name]
    pb.matrix = Matrix.Translation(head if head is not None else pb.head) @ rotation.to_4x4()
    if head is None:
        pb.location = (0, 0, 0)
    bpy.context.view_layer.update()


def two_bone(root, target, l1, l2, pole):
    """Returns (joint, end) for a two-bone chain from root reaching for target, bending towards pole."""
    to_target = target - root
    reach = max(min(to_target.length, (l1 + l2) * 0.995), abs(l1 - l2) + 1.0)
    direction = to_target.normalized()
    pole = (pole - direction * pole.dot(direction)).normalized()
    a = (l1 * l1 - l2 * l2 + reach * reach) / (2 * reach)
    h = math.sqrt(max(l1 * l1 - a * a, 0.0))
    return root + direction * a + pole * h, root + direction * reach, pole


def bone_length(arm_obj, name):
    bone = arm_obj.data.bones[name]
    return (bone.tail_local - bone.head_local).length


def pose_hips(arm_obj, pose):
    drop, yaw, pitch = pose.get("hips", (0, 0, 0))
    name = P + "Hips"
    rotation = Matrix.Rotation(math.radians(yaw), 3, "Y") @ Matrix.Rotation(math.radians(pitch), 3, "X") @ rest_rotation(arm_obj, name)
    set_rotation(arm_obj, name, rotation, arm_obj.data.bones[name].head_local - V((0, drop, 0)))


def pose_leg(arm_obj, side, foot):
    x, z, yaw = foot
    names = [f"{P}{side}UpLeg", f"{P}{side}Leg", f"{P}{side}Foot"]
    hip = arm_obj.pose.bones[names[0]].head.copy()
    # Knees bend forward and a little out, following the foot's turn.
    pole = Matrix.Rotation(math.radians(yaw), 3, "Y") @ V((0.25 if side == "Left" else -0.25, 0, 1))
    knee, ankle, _ = two_bone(hip, V((x, ANKLE_HEIGHT, z)), bone_length(arm_obj, names[0]), bone_length(arm_obj, names[1]), pole)
    set_rotation(arm_obj, names[0], swung(arm_obj, names[0], knee - hip))
    set_rotation(arm_obj, names[1], swung(arm_obj, names[1], ankle - knee))
    # The foot stays flat, turned by its yaw.
    set_rotation(arm_obj, names[2], Matrix.Rotation(math.radians(yaw), 3, "Y") @ rest_rotation(arm_obj, names[2]))


def pose_arm(arm_obj, side, spec):
    names = [f"{P}{side}Arm", f"{P}{side}ForeArm", f"{P}{side}Hand"]
    shoulder = arm_obj.pose.bones[names[0]].head.copy()
    elbow, wrist, pole = two_bone(shoulder, spec["wrist"], bone_length(arm_obj, names[0]), bone_length(arm_obj, names[1]), spec["pole"].normalized())
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
    pose_hips(arm_obj, pose)
    for side, key in (("Right", "R"), ("Left", "L")):
        if key in pose.get("feet", {}):
            pose_leg(arm_obj, side, pose["feet"][key])
    pose_torso(arm_obj, pose)
    for side, key in (("Right", "R"), ("Left", "L")):
        if key in pose:
            pose_arm(arm_obj, side, pose[key])
    bpy.context.view_layer.update()


EASES = {"snap": ("EXPO", "EASE_OUT"), "back": ("BACK", "EASE_OUT"), "smooth": ("BEZIER", "AUTO")}


def build_action(arm_obj, name, keys):
    action = bpy.data.actions.new(name)
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = action
    eases = {}
    for seconds, pose, ease in keys:
        frame = round(seconds * FPS)
        eases[frame] = EASES[ease]
        apply_pose(arm_obj, pose)
        for pb in arm_obj.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("location", frame=frame)
    # Shape each move: the interpolation on a key governs the segment that starts at it.
    for fcurve in action_fcurves(action):
        for point in fcurve.keyframe_points:
            interpolation, easing = eases.get(round(point.co.x), EASES["smooth"])
            point.interpolation = interpolation
            point.easing = easing
    return action, round(keys[-1][0] * FPS)


def action_fcurves(action):
    """F-curves of an action, for both the legacy and the layered (Blender 4.4+) action layouts."""
    if hasattr(action, "fcurves") and len(action.fcurves):
        return list(action.fcurves)
    curves = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in strip.channelbags:
                curves.extend(bag.fcurves)
    return curves


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
    scene.display.shading.show_shadows = True
    scene.render.resolution_x, scene.render.resolution_y = 260, 400
    scene.world = bpy.data.worlds.new("Preview")
    scene.world.color = (0.05, 0.05, 0.06)
    floor = bpy.data.meshes.new("Floor")
    floor.from_pydata([(-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)], [], [(0, 1, 2, 3)])
    scene.collection.objects.link(bpy.data.objects.new("Floor", floor))
    cameras = {}
    # The rig faces world -Y (its +Z forward, turned by the import's X rotation). A 3/4 view from its front right,
    # and a side view from its right, both framing the whole body.
    centre = V((0, 0, 0.95))
    for label, location in (("front", V((-1.6, -2.4, 1.2))), ("side", V((-2.9, 0, 1.0)))):
        data = bpy.data.cameras.new(label)
        data.type = "ORTHO"
        data.ortho_scale = 2.1
        cam = bpy.data.objects.new(label, data)
        cam.location = location
        cam.rotation_euler = (centre - location).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(cam)
        cameras[label] = cam
    return cameras


def render_strip(cameras, last_frame, path, columns=6):
    scene = bpy.context.scene
    frames = [round(last_frame * i / (columns - 1)) for i in range(columns)]
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
            row.append(list(image.pixels))
            size = image.size[:]
            bpy.data.images.remove(image)
        tiles.append(row)
    w, h = size
    strip = bpy.data.images.new("strip", w * columns, h * 2, alpha=True)
    pixels = [0.0] * (w * columns * h * 2 * 4)
    for r, row in enumerate(reversed(tiles)):  # Blender images start at the bottom row: side row first
        for c, tile in enumerate(row):
            for y in range(h):
                src = y * w * 4
                dst = ((r * h + y) * w * columns + c * w) * 4
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
