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


def arm(wrist, pole, fingers, palm, curl=(10, 10, 10, 10, 10), spread=(0, 0, 0, 0, 0), thumb=None):
    """curl: per finger (thumb first) degrees towards the palm, one number for all three joints or a (j1, j2, j3) tuple;
    a single number on the thumb is scaled by 0.6, a tuple is used as is.
    spread: per finger degrees away from the middle finger (thumb: away from the index; middle: towards the thumb).
    thumb: where Thumb1 points, (along the fingers, out of the palm, thumbwards), overriding its j1 curl and spread."""
    spec = {"wrist": V(wrist), "pole": V(pole), "fingers": V(fingers), "palm": V(palm), "curl": curl, "spread": spread}
    return {**spec, "thumb": thumb} if thumb else spec


def mirror(spec, offset=(0, 0, 0)):
    """The other hand: the spec mirrored through the body's midplane (x -> -x), then moved by offset (cm)."""
    m = lambda v: V((-v.x, v.y, v.z))
    return {**spec, "wrist": m(spec["wrist"]) + V(offset), "pole": m(spec["pole"]), "fingers": m(spec["fingers"]), "palm": m(spec["palm"])}


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
TWO_UP = (15, 0, 0, 85, 85)  # index and middle straight, the rest folded
ELBOW_R, ELBOW_L = (-1, -0.6, 0.1), (1, -0.6, 0.1)  # elbows out and down, as when holding hands at the chest

# Stances. Right-handed: the right foot drops back for the ready stance and steps forward to strike.
IDLE = {"hips": (2, 0, 0), "feet": {"R": (-11, 0, -8), "L": (11, 0, 8)}}
# Kept moderate: on the hero's proportions a deeper Mixamo pelvis drop reads as a sumo squat.
READY = {"hips": (4, -12, 0), "feet": {"R": (-13, -8, -20), "L": (12, 9, 8)}}
BRACE = {"hips": (7, -16, 0), "feet": {"R": (-15, -15, -25), "L": (13, 13, 5)}}
CROUCH = {"hips": (18, -20, 10), "feet": {"R": (-21, -13, -25), "L": (19, 17, 8)}}
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

# The twelve hand seals, after the official databook chart (Hiden Rin no Sho, "the twelve signs"): both hands in front
# of the chest. curl tuples are (j1, j2, j3) degrees.
# Interlaced fingers can't really pass between each other here: they curl round the back of the other hand and the
# hands sit half a finger apart, which reads as interlaced from outside.
WRAP = (80, 85, 45)       # a finger folded round the back of the other hand
TUCK = (90, 100, 60)      # a finger folded into its own palm
THUMB_UP = (0, 0, 0)      # thumb straight (with thumb=ALONG)
THUMB_TUCK = (0, 15, 10)  # thumb across the folded fingers (with thumb=ACROSS)
ALONG = (1, 0, 0.15)      # thumb directions (along the fingers, out of the palm, thumbwards)
ACROSS = (0.8, 0.45, -0.4)
_tiger_r = arm((-1.4, 110, 24), ELBOW_R, (0, 1, 0.2), LEFT, (THUMB_UP, 0, 0, WRAP, WRAP), thumb=ALONG)
_snake_r = arm((-1.4, 111, 23), ELBOW_R, (0, 1, 0.5), LEFT, (THUMB_UP, (75, 70, 30), (75, 70, 30), (75, 70, 30), (75, 70, 30)), (0, 4, 0, 4, 8), ALONG)
_dragon_r = arm((-1.4, 118, 21), ELBOW_R, (0, -0.4, 1), LEFT, ((-10, 0, 0), WRAP, WRAP, WRAP, (0, 0, 0)), (10, 4, 0, 4, 0))
_horse_r = arm((-11.8, 117, 28), ELBOW_R, (0.75, 0.65, 0), BACK, ((0, 0, 0), (0, 0, 0), TUCK, TUCK, TUCK), (20, 0, 0, 0, 0))
_bird_r = arm((-7, 111, 24), ELBOW_R, (0.42, 0.9, 0.12), (1, -0.42, 0), ((-10, 0, 0), 0, 0, 0, 0), (25, 0, 0, 0, 0))
_boar_r = arm((-3.6, 127, 27), (-1, 0, -0.2), DOWN, BACK, ((20, 10, 0), (20, 30, 30), (20, 30, 30), (20, 30, 30), (20, 30, 30)), (-15, 0, 0, 0, 0))
SEALS = {
    # Rat: the left hand upright, index and middle up; the right fist closes round those two fingers.
    "Rat": {"R": arm((-5, 122, 25.5), ELBOW_R, (1, 0.15, 0.1), BACK, (THUMB_TUCK, 80, 90, 95, 100), thumb=ACROSS),
            "L": arm((0, 104, 22.5), ELBOW_L, (-0.05, 1, 0.05), BACK, ((20, 20, 10), 0, 0, TUCK, TUCK), (0, 0, 0, 0, 0))},
    # Ox: the right hand horizontal in front, fingers straight across, thumb up; the left hand upright behind it.
    "Ox": {"R": arm((-10, 121, 28), ELBOW_R, (1, 0, 0.1), BACK, ((-10, 0, 0), 0, 0, 0, 0), (45, 0, 0, 0, 0)),
           "L": arm((3, 106, 25), ELBOW_L, (-0.2, 1, 0.05), BACK, ((10, 0, 0), 5, 5, 10, 15), (10, 0, 0, 0, 0))},
    # Tiger: palms together, index and middle straight up and pressed together, ring and little fingers interlaced,
    # both thumbs straight up.
    "Tiger": {"R": _tiger_r, "L": mirror(_tiger_r, (0, 0, 0.9))},
    # Hare: the left hand upright, thumb out; the right hand across the front of its fingers, curled except the pinky.
    "Hare": {"R": arm((-10, 123, 28), ELBOW_R, (1, 0, 0.1), BACK, ((-10, 0, 0), 50, 55, 60, 0), (40, 0, 0, 0, 10)),
             "L": arm((3, 106, 25), ELBOW_L, (-0.2, 1, 0.05), BACK, ((-20, 0, 0), 10, 10, 15, 20), (30, 0, 0, 0, 0))},
    # Dragon: hands clasped pointing forward and down, fingers wrapped round each other's backs, little fingers straight
    # and pressed together, thumbs crossed on top (left on top).
    "Dragon": {"R": _dragon_r, "L": mirror(_dragon_r, (0, 0.6, 0.9))},
    # Snake: palms clasped, all fingers interlaced over the backs of the hands, left thumb over the right.
    "Snake": {"R": _snake_r, "L": {**mirror(_snake_r, (0, 0, 1.0)), "thumb": (0.8, 0.25, -0.3)}},
    # Horse: elbows out, palms to the chest, index fingers meeting in a peak, the other fingers folded with their
    # knuckles together, thumbs up.
    "Horse": {"R": _horse_r, "L": mirror(_horse_r)},
    # Ram: like Tiger, but the right hand is a fist below with its index and middle up; the left hand's two raised
    # fingers lie along them and its other fingers and thumb close over the right fist.
    "Ram": {"R": arm((-1.4, 107, 24), ELBOW_R, (0.15, 1, 0.2), LEFT, (THUMB_TUCK, 0, 0, TUCK, TUCK), thumb=ACROSS),
            "L": arm((1.8, 111, 24.5), ELBOW_L, (-0.15, 1, 0.2), RIGHT, ((40, 20, 10), 0, 0, WRAP, WRAP), (0, 0, 0, 0, 0))},
    # Monkey: hands flat and level, stacked palm to palm, fingers pointing opposite ways; each thumb lies flat along the
    # other hand's little finger.
    "Monkey": {"R": arm((-8, 123.3, 26), ELBOW_R, (1, 0, 0.1), DOWN, ((-15, 0, 0), 5, 5, 5, 5), (-15, 0, 0, 0, 0)),
               "L": arm((8, 120.7, 26), ELBOW_L, (-1, 0, 0.1), UP, ((-15, 0, 0), 15, 15, 15, 15), (-15, 0, 0, 0, 0))},
    # Bird: fingers straight and fingertips together in a peak, wrists apart, thumbs angled up and touching.
    "Bird": {"R": _bird_r, "L": mirror(_bird_r)},
    # Dog: the left hand flat on top of the right fist.
    "Dog": {"R": arm((-6, 117, 23), ELBOW_R, (0.6, 0, 1), DOWN, (THUMB_TUCK, 90, 95, 100, 105), thumb=ACROSS),
            "L": arm((1, 120.5, 24), ELBOW_L, (-0.75, 0, 0.6), DOWN, ((-10, 0, 0), 0, 0, 0, 0))},
    # Boar: forearms level, wrists bent so the fingers hang down, palms to the chest, the hands side by side.
    "Boar": {"R": _boar_r, "L": mirror(_boar_r)},
}


def seal(name, *body):
    return merge(NEUTRAL, READY_BODY, *body, SEALS[name])


# Fireball: breathe in leaning back on a braced stance, then throw the whole body into the blow, seal still held
# (as Sasuke does), which keeps the mouth clear for the fire.
INHALE = merge(BRACE, {"spine": (-8, 9), "neck": (-7, 2), "head": (-10, 3)})
INHALE_DEEP = merge(INHALE, {"spine": (-10, 9), "head": (-13, 3)})
BLOW = seal("Tiger", BRACE, {"hips": (8, -16, 8), "spine": (7, 9), "neck": (10, 2), "head": (-24, 3)})  # neck thrust, face level (spine pitch is per bone, x3)
BLOW_RECOIL = seal("Tiger", BRACE, {"hips": (7, -16, 3), "spine": (4, 9), "neck": (5, 2), "head": (-12, 3)})

# Chidori: a wide, low stance leaning forward, the right hand charging between the knees, clawed and pointing down,
# the left hand gripping the right wrist ("grip": placed after the right arm is solved, so it holds whatever the body
# does).
GRIP = {"grip": True, "curl": (45, 70, 75, 80, 85)}
WIDE = {"hips": (32, -6, 8), "feet": {"R": (-33, -4, -30), "L": (33, 6, 28)}}
CHIDORI_HOLD = merge(NEUTRAL, WIDE, {
    "spine": (12, 3), "neck": (-18, 1), "head": (-16, 2),
    "R": arm((-4, 52, 30), (-0.8, 0, -0.6), (0, -1, 0.35), (0, 0.35, 1), CLAW),
    "L": {**ARMS_DOWN["L"], **GRIP},
})
CHIDORI_DEEP = merge(CHIDORI_HOLD, {"hips": (36, -6, 10), "spine": (15, 3), "R": {"wrist": V((-4, 47, 32))}})
# Rasengan: right arm held out forward and to the side, palm up, so the ball above it stays clear of the body; the
# left hand frames the ball from the side.
RASENGAN_HOLD = merge(NEUTRAL, READY, {
    "spine": (6, 4), "neck": (-2, 3), "head": (-4, 3),
    "R": arm((-32, 118, 40), (-1, -0.4, -0.3), (0.2, 0, 1), UP, (20, 30, 30, 30, 30)),
    "L": arm((-20, 138, 38), ELBOW_L, (-0.2, 0.3, 1), RIGHT, (20, 30, 30, 30, 30)),
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
# The Rasengan thrust: hand upright, palm to the front, so the ball rides in front of it (UShinobiCastFxComponent).
PALM_ARMS = merge(BACK_ARM, {"R": arm((-6, 122, 58), (-1, -0.5, 0), (0, 1, 0.2), FWD, (15, 10, 10, 10, 10))})
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
def seal_run(names, start=0.06, step=0.12, hold=0.07, body=()):
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
    seals, t = seal_run(("Monkey", "Boar", "Horse", "Tiger"))  # canon: Snake, Ram, Monkey, Boar, Horse, Tiger
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.1, seal("Tiger", INHALE), "smooth")],
        "Charge": [(round(i * 0.55, 3), seal("Tiger", INHALE if i % 2 == 0 else INHALE_DEEP), "smooth") for i in range(5)],
        "Release": [(0.0, seal("Tiger", INHALE), "smooth"),
                    (0.08, seal("Tiger", INHALE_DEEP), "snap"),      # the last gulp of air
                    (0.16, BLOW, "back"),                            # throw the body into the blow
                    (0.45, BLOW_RECOIL, "smooth"),
                    (0.85, merge(NEUTRAL, READY_BODY), "smooth")],
    }


def chidori():
    seals, t = seal_run(("Ox", "Hare", "Monkey"))  # canon
    windup = {"R": arm((-24, 104, 4), (-0.6, 0, -1), (-0.2, -0.3, 1), DOWN, CLAW)}
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.18, CHIDORI_HOLD, "smooth")],
        "Charge": tremble(CHIDORI_HOLD, 2.2, 1.6, seed=2, step=0.1, build=lambda u: lerp_pose(CHIDORI_HOLD, CHIDORI_DEEP, u)),
        "Release": dash_release(CHIDORI_DEEP, windup, STRIKE_ARMS),
    }


# The Rasengan has no seals: the left hand stirs chakra into the right palm held below it, then the palm opens.
RASENGAN_GATHER = merge(NEUTRAL, READY, {"spine": (5, 4), "neck": (2, 3), "head": (4, 3),
                                         "R": arm((-8, 106, 30), ELBOW_R, (0.5, 0, 1), UP, (15, 25, 30, 35, 40)),
                                         "L": arm((4, 118, 30), ELBOW_L, (-0.5, 0, 1), DOWN, (15, 25, 30, 35, 40))})


def rasengan():
    centre = RASENGAN_HOLD["L"]["wrist"]

    def charge_key(i):
        angle = i * math.pi / 2
        pose = merge(RASENGAN_HOLD, {"L": {"wrist": centre + V((math.cos(angle) * 6, math.sin(angle * 2) * 1.5, math.sin(angle) * 6))},
                                     "hips": (8, -12 + math.sin(angle) * 5, 0)})
        return (round(i * 2.2 / 12, 3), pose, "smooth")

    windup = {"R": arm((-22, 110, 8), (-1, -0.4, -0.3), (0, 1, 0.2), FWD, (20, 30, 30, 30, 30))}
    return {
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.16, RASENGAN_GATHER, "smooth"), (0.26, shift(RASENGAN_GATHER, "L", (3, 0, 2)), "smooth"),
                 (0.42, RASENGAN_HOLD, "smooth")],
        "Charge": [charge_key(i) for i in range(13)],
        "Release": dash_release(RASENGAN_HOLD, windup, PALM_ARMS),
    }


def amaterasu():
    return {  # no seals in canon: the Mangekyo's own
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.22, EYE_FOCUS, "snap"), (0.42, EYE_FOCUS, "smooth")],
        "Charge": tremble(EYE_FOCUS, 2.2, 0.4, seed=4, step=0.275, build=lambda u: lerp_pose(EYE_FOCUS, EYE_BOWED, min(u * 2, 1))),
        "Release": [(0.0, EYE_BOWED, "smooth"), (0.05, merge(EYE_BOWED, {"head": (20, 3)}), "snap"),
                    (0.12, STARE, "back"), (0.6, merge(STARE, ARMS_READY), "smooth")],
    }


# Teleport (Flying Raijin): a flick of the Tiger seal, then the right arm cocked behind the head and thrown forward.
THROW_COCK = merge(NEUTRAL, BRACE, {"hips": (7, 14, 0), "spine": (-2, -24), "neck": (0, -10), "head": (-4, -14),
                                    "R": arm((-22, 152, -12), (-1, 0.2, -0.3), (0, 0.8, -0.6), LEFT, (30, 40, 40, 40, 40)),
                                    "L": arm((17, 116, 34), ELBOW_L, (0, 0.5, 1), RIGHT, OPEN)})
THROW_FOLLOW = merge(NEUTRAL, LUNGE, {"spine": (14, 12), "neck": (-6, 8), "head": (-6, 8),
                                      "R": arm((-12, 124, 64), (-1, -0.4, 0), FWD, DOWN, OPEN),
                                      "L": arm((24, 96, -20), (0.3, 0, 1), (0.2, -1, -0.4), RIGHT, (30, 40, 40, 40, 40))})


# The one-handed Seal of Confrontation: index and middle up in front of the face.
CONFRONT = merge(NEUTRAL, READY_BODY, {"R": arm((-5, 130, 22), ELBOW_R, (0, 1, 0.15), LEFT, (THUMB_TUCK, 0, 0, TUCK, TUCK), thumb=ACROSS)})


def teleport():
    return {
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.1, CONFRONT, "snap"), (0.24, CONFRONT, "snap"), (0.42, THROW_COCK, "smooth")],
        "Release": [(0.0, THROW_COCK, "smooth"), (0.1, THROW_FOLLOW, "back"),
                    (0.4, merge(THROW_FOLLOW, {"spine": (10, 8)}), "smooth"),
                    (0.8, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Kamui: no seals in canon; two fingers at the eye while the body sinks, as if slipping into the ground.
SINK_START = merge(EYE_FOCUS, {"hips": (8, -14, 3)})
SINK = merge(EYE_BOWED, {"hips": (14, -14, 6), "spine": (6, 9)})


def kamui():
    return {
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.16, EYE_FOCUS, "snap"), (0.32, SINK_START, "smooth")],
        "Release": [(0.0, SINK_START, "smooth"), (0.12, SINK, "smooth"), (0.45, SINK, "smooth"),
                    (0.9, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Almighty Push: one palm held up at the chest, pushed out flat.
PUSH_READY = merge(NEUTRAL, BRACE, {"spine": (-2, 6), "neck": (0, 2), "head": (-3, 3),
                                    "R": arm((-17, 122, 22), (-1, -0.4, 0), (0, 1, 0.3), FWD, OPEN),
                                    "L": arm((22, 106, 14), ELBOW_L, (0, -0.6, 0.8), RIGHT, (20, 20, 25, 30, 35))})
PUSH_THRUST = merge(NEUTRAL, LUNGE, {"spine": (10, 10), "neck": (-4, 6), "head": (-4, 6),
                                     "R": arm((-9, 128, 62), (-1, -0.4, 0), (0, 1, 0.25), FWD, OPEN),
                                     "L": arm((24, 98, -16), (0.3, 0, 1), (0.2, -1, -0.4), RIGHT, (30, 40, 40, 40, 40))})


def almighty_push():
    return {  # no seals in canon
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.3, PUSH_READY, "back"), (0.5, PUSH_READY, "smooth")],
        "Charge": tremble(PUSH_READY, 2.2, 0.7, seed=5, step=0.2),
        "Release": [(0.0, PUSH_READY, "smooth"), (0.08, merge(PUSH_READY, {"spine": (-5, 4)}), "snap"),
                    (0.18, PUSH_THRUST, "back"), (0.5, merge(PUSH_THRUST, {"spine": (7, 8)}), "smooth"),
                    (0.9, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Earth and Water Wall: the seals, hands raised high, then both palms slammed down to the ground in a crouch.
HANDS_HIGH = merge(NEUTRAL, BRACE, {"spine": (-3, 9), "neck": (-3, 2), "head": (-6, 3),
                                    "R": arm((-12, 154, 24), (-1, -0.2, -0.3), (0.1, 1, 0.3), LEFT, OPEN),
                                    "L": arm((12, 154, 24), (1, -0.2, -0.3), (-0.1, 1, 0.3), RIGHT, OPEN)})
PALMS_DOWN = merge(NEUTRAL, CROUCH, {"spine": (14, 6), "neck": (-8, 2), "head": (-10, 3),
                                     "R": arm((-17, 22, 62), (-1, 0.3, -0.2), (0, -0.3, 1), DOWN, OPEN),
                                     "L": arm((17, 22, 62), (1, 0.3, -0.2), (0, -0.3, 1), DOWN, OPEN)})


def wall(names):
    seals, t = seal_run(names)
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.12, HANDS_HIGH, "smooth")],
        "Release": [(0.0, HANDS_HIGH, "smooth"), (0.12, merge(HANDS_HIGH, {"spine": (-6, 9)}), "snap"),
                    (0.26, PALMS_DOWN, "back"), (0.75, PALMS_DOWN, "smooth"), (1.2, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Fire Meteor: one arm thrown straight up to call the rock down, then swept down to point at the spot.
SKY_POINT = merge(NEUTRAL, BRACE, {"spine": (-6, 9), "neck": (-8, 2), "head": (-16, 3),
                                   "R": arm((-14, 178, 8), (-1, 0, -0.2), UP, LEFT, (10, 0, 0, 80, 80)),
                                   "L": arm((22, 100, -10), ELBOW_L, (0.3, -1, -0.3), RIGHT, (30, 40, 40, 40, 40))})
METEOR_POINT = merge(NEUTRAL, LUNGE, {"spine": (14, 10), "neck": (-4, 6), "head": (-4, 6),
                                      "R": arm((-12, 108, 64), (-1, -0.4, 0), (0, -0.45, 1), LEFT, (10, 0, 0, 80, 80)),
                                      "L": arm((24, 96, -18), (0.3, 0, 1), (0.2, -1, -0.4), RIGHT, (30, 40, 40, 40, 40))})


def fire_meteor():
    seals, t = seal_run(("Snake", "Horse", "Tiger"))  # fire ends on Tiger
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.15, SKY_POINT, "smooth")],
        "Release": [(0.0, SKY_POINT, "smooth"), (0.08, merge(SKY_POINT, {"spine": (-9, 9)}), "snap"),
                    (0.2, METEOR_POINT, "back"), (0.6, merge(METEOR_POINT, {"spine": (10, 8)}), "smooth"),
                    (1.0, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Ice Meteor Shower: both hands drawn wide at the hips, palms up, then thrown to the sky together.
HANDS_WIDE = merge(NEUTRAL, BRACE, {"spine": (-3, 9), "neck": (-4, 2), "head": (-8, 3),
                                    "R": arm((-40, 96, 14), (-1, -0.3, -0.4), (-1, 0.2, 0.4), UP, OPEN),
                                    "L": arm((40, 96, 14), (1, -0.3, -0.4), (1, 0.2, 0.4), UP, OPEN)})
HANDS_SKY = merge(NEUTRAL, BRACE, {"spine": (-9, 9), "neck": (-9, 2), "head": (-18, 3),
                                   "R": arm((-22, 176, 6), (-1, 0, -0.3), UP, LEFT, OPEN),
                                   "L": arm((22, 176, 6), (1, 0, -0.3), UP, RIGHT, OPEN)})


def ice_meteor():
    seals, t = seal_run(("Rat", "Ox", "Dog"))  # not canon; Dog is the water/ice seal
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.12, HANDS_WIDE, "smooth")],
        "Release": [(0.0, HANDS_WIDE, "smooth"), (0.1, merge(HANDS_WIDE, {"hips": (9, -16, 0)}), "snap"),
                    (0.22, HANDS_SKY, "back"), (0.9, HANDS_SKY, "smooth"),
                    (1.3, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Water Dragon: the dragon seals, then both hands swept out together in front, palms flat, on a lunge.
DRAGON_GATHER = merge(NEUTRAL, BRACE, {"spine": (-3, 6), "neck": (-2, 2), "head": (-4, 3),
                                       "R": arm((-12, 112, 12), (-1, -0.4, 0), (0.3, 0.1, 1), LEFT, OPEN),
                                       "L": arm((12, 112, 12), (1, -0.4, 0), (-0.3, 0.1, 1), RIGHT, OPEN)})
DRAGON_SWEEP = merge(NEUTRAL, LUNGE, {"spine": (12, 8), "neck": (-4, 6), "head": (-4, 6),
                                      "R": arm((-10, 118, 66), (-1, -0.3, 0), FWD, LEFT, OPEN),
                                      "L": arm((10, 118, 66), (1, -0.3, 0), FWD, RIGHT, OPEN)})


def water_dragon():
    seals, t = seal_run(("Ox", "Monkey", "Hare", "Rat"))  # the opening of the canon 44
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.12, DRAGON_GATHER, "smooth")],
        "Release": [(0.0, DRAGON_GATHER, "smooth"), (0.1, merge(DRAGON_GATHER, {"spine": (-6, 6)}), "snap"),
                    (0.22, DRAGON_SWEEP, "back"), (0.7, merge(DRAGON_SWEEP, {"spine": (9, 8)}), "smooth"),
                    (1.1, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Susanoo: the eye (no seals in canon), then both arms thrown wide and the head back as the armour rises.
ARMS_WIDE = merge(NEUTRAL, BRACE, {"spine": (-8, 9), "neck": (-8, 2), "head": (-18, 3),
                                   "R": arm((-50, 166, 14), (-1, -0.2, -0.3), (-1, 0.4, 0.2), FWD, OPEN),
                                   "L": arm((50, 166, 14), (1, -0.2, -0.3), (1, 0.4, 0.2), FWD, OPEN)})


def susanoo():
    focus = merge(EYE_BOWED, BRACE)
    return {  # no seals in canon: the Mangekyo's own
        "Seal": [(0.0, NEUTRAL, "smooth"), (0.22, EYE_FOCUS, "snap"), (0.6, focus, "smooth")],
        "Release": [(0.0, focus, "smooth"), (0.12, merge(focus, {"hips": (9, -16, 0)}), "snap"),
                    (0.3, ARMS_WIDE, "back"), (1.0, ARMS_WIDE, "smooth"), (1.5, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Summoning: the seals, then a crouch with the right fist drawn up (the bitten thumb) and the left hand low; the release slams
# both palms flat on the ground, the contract signed in blood.
SUMMON_GATHER = merge(NEUTRAL, CROUCH, {"spine": (10, 6), "neck": (-6, 2), "head": (-8, 3),
                                        "R": arm((-17, 74, 38), (-1, 0.3, 0), (0, -0.4, 1), LEFT, FIST),
                                        "L": arm((17, 80, 26), (1, 0.3, 0), (0, -0.2, 1), RIGHT, OPEN)})


def summon():
    seals, t = seal_run(("Boar", "Dog", "Bird", "Monkey", "Ram"), step=0.11)  # canon Kuchiyose
    return {
        "Seal": [(0.0, NEUTRAL, "smooth")] + seals + [(t + 0.17, SUMMON_GATHER, "smooth")],
        "Release": [(0.0, SUMMON_GATHER, "smooth"), (0.1, merge(SUMMON_GATHER, {"spine": (-4, 6)}), "snap"),
                    (0.24, PALMS_DOWN, "back"), (0.8, PALMS_DOWN, "smooth"), (1.25, merge(NEUTRAL, READY_BODY), "smooth")],
    }


# Riding the dragon: kneeling in seiza on its back, sitting on the heels, shins flat behind, fists forward holding the reins (AShinobiDragonMount lays them from the collar to the fists). A dragon's back is
# about a metre across, too wide to straddle: every part of a kneeling rider stays on top of it. Posed on the floor (Y 0), which
# the retarget keeps at the floor for every hero; AShinobiDragonMount puts the dragon's back there. A loop, not a jutsu: held while
# State.Mounted lasts (AShinobiPlayerCharacter replays it).
SEATED = merge(NEUTRAL, {"hips": (67, 0, 0), "feet": {"R": (-7, -6, 0, True), "L": (7, -6, 0, True)}, "spine": (8, 0), "neck": (-4, 0), "head": (-6, 0),
                         "R": arm((-12, 58, 32), (-1, -0.3, -0.4), (0, -0.2, 1), (0.4, -0.9, 0), (THUMB_TUCK, 95, 95, 95, 95), thumb=ACROSS),
                         "L": arm((12, 58, 32), (1, -0.3, -0.4), (0, -0.2, 1), (-0.4, -0.9, 0), (THUMB_TUCK, 95, 95, 95, 95), thumb=ACROSS)})
SEATED_BREATH = merge(SEATED, {"spine": (10, 0), "neck": (-5, 0), "head": (-5, 0)})


def mount():
    return {"Sit": [(0.0, SEATED, "smooth"), (1.2, SEATED_BREATH, "smooth"), (2.4, SEATED, "smooth")]}


TIMELINES = {"Fireball": fireball(), "Chidori": chidori(), "Rasengan": rasengan(), "Amaterasu": amaterasu(),
             "Teleport": teleport(), "Kamui": kamui(), "AlmightyPush": almighty_push(), "EarthWall": wall(("Hare", "Boar", "Dog")),  # canon Tiger, Hare, Boar, Dog
             "WaterWall": wall(("Ox", "Dog")),  # canon has its own seal; Dog is the water seal
             "FireMeteor": fire_meteor(), "IceMeteor": ice_meteor(), "WaterDragon": water_dragon(), "Susanoo": susanoo(), "Summon": summon(), "Mount": mount()}


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
    """foot: (x, z, yaw), or (x, z, yaw, True) for a kneel: the ankle a little lower and the foot pointed along the shin, its top
    on the ground."""
    x, z, yaw, pointed = (*foot, False)[:4]
    names = [f"{P}{side}UpLeg", f"{P}{side}Leg", f"{P}{side}Foot"]
    hip = arm_obj.pose.bones[names[0]].head.copy()
    # Knees bend forward and a little out, following the foot's turn.
    pole = Matrix.Rotation(math.radians(yaw), 3, "Y") @ V((0.25 if side == "Left" else -0.25, 0, 1))
    knee, ankle, _ = two_bone(hip, V((x, 6.0 if pointed else ANKLE_HEIGHT, z)), bone_length(arm_obj, names[0]), bone_length(arm_obj, names[1]), pole)
    set_rotation(arm_obj, names[0], swung(arm_obj, names[0], knee - hip))
    set_rotation(arm_obj, names[1], swung(arm_obj, names[1], ankle - knee))
    if pointed:
        shin = (ankle - knee).normalized()
        set_rotation(arm_obj, names[2], swung(arm_obj, names[2], V((shin.x, min(shin.y, 0.0) - 0.15, shin.z))))
        return
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
    # Finger bones: X across the hand (thumbwards on the right hand, pinkywards on the left), Z out of the palm. Turning
    # about X curls towards the palm; turning about Z by +a swings the finger towards -X.
    thumbward = -1 if side == "Right" else 1
    for i, (finger, curl, spread) in enumerate(zip(FINGERS, spec["curl"], spec.get("spread", (0,) * 5))):
        joints = curl if isinstance(curl, tuple) else (curl * (0.6 if finger == "Thumb" else 1.0),) * 3
        for joint, angle in zip((1, 2, 3), joints):
            pb = arm_obj.pose.bones[f"{P}{side}Hand{finger}{joint}"]
            rotation = Matrix.Rotation(math.radians(angle), 3, "X")
            if joint == 1 and spread:
                rotation = Matrix.Rotation(math.radians(thumbward * spread * (-1 if i > 2 else 1)), 3, "Z") @ rotation
            pb.rotation_quaternion = rotation.to_quaternion()
    if "thumb" in spec:
        # Thumb1 swung to point along (along the fingers, out of the palm, thumbwards), hand terms; Thumb2/3 keep their curl.
        along, out, thumbwise = spec["thumb"]
        hand = arm_obj.pose.bones[names[2]].matrix.to_3x3()
        target = hand.col[1] * along + hand.col[2] * out + hand.col[0] * thumbwise * (1 if side == "Right" else -1)
        bpy.context.view_layer.update()
        pb = arm_obj.pose.bones[f"{P}{side}HandThumb1"]
        rotation = pb.matrix.to_3x3()
        set_rotation(arm_obj, pb.name, rotation.col[1].rotation_difference(target.normalized()).to_matrix() @ rotation)


def grip_spec(arm_obj, spec):
    """The left hand closed over the top of the right forearm, just behind the wrist (the right arm is posed)."""
    elbow = arm_obj.pose.bones[P + "RightForeArm"].head
    wrist = arm_obj.pose.bones[P + "RightHand"].head
    along = (wrist - elbow).normalized()
    grip = elbow.lerp(wrist, 0.9)
    # Fingers wrap across the forearm towards the character's right and down; the palm faces the forearm.
    across = along.cross(UP).normalized()
    if across.x > 0:
        across = -across
    fingers = (across * 0.7 + DOWN * 0.7).normalized()
    palm = DOWN
    wrist_target = grip + UP * 5 - fingers * 6
    return {**spec, "wrist": wrist_target, "pole": V(ELBOW_L), "fingers": fingers, "palm": palm}


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
            spec = pose[key]
            if spec.get("grip"):
                spec = grip_spec(arm_obj, spec)
            pose_arm(arm_obj, side, spec)
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
