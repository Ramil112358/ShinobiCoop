"""Animates the crimson summon dragon (Tairn is build_dragon.py): the Tripo Studio model, auto-rigged there (SourceArt/Tripo/Dragon, the
FBX from dragon_rigged.zip) -> FBX for Unreal.

Run: blender -b -P Scripts/character/build_dragon_crimson.py            (add  -- preview  to render pose previews to Saved/dragon_*.png)

Tripo's rig (spine, a neck and head chain, a 14-bone tail, four legs with toes, and a wing a side: shoulder, arm, forearm and four
fingers) and its skin weights are kept; Tripo Studio has no animations for a rig like this (its presets and AI motions are humanoid),
so the clips are keyed here. The model stands on all fours with its wings raised in a V, its head turned a little to its left and its
tail curled round to the left, facing -Y. It is turned to face +X, centred on its spine and scaled up, and a jaw bone is added (the
mouth line was measured on the mesh; the lower jaw had no bone of its own). Every flying clip starts from one flying pose built from
that rest pose: neck reached forward, head straight, tail straightened out behind, legs tucked and trailing, wings swung down and out.
Clips: Idle (a slow hover), Flap (the fast beat, a loop), Glide (wings out, a slow sway) and Breath (rears the head back, opens the
jaw and thrusts; played once). Writes SourceArt/Creatures/CrimsonDragon/CrimsonDragon.fbx (mesh + skeleton), CrimsonDragon_<Clip>.fbx
per clip and CrimsonDragon.png (+ _Normal, _Roughness; 2k). Faces +X, feet on Z = 0 in the rest pose; metres.
"""
import glob
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = glob.glob(os.path.join(ROOT, "SourceArt", "Tripo", "Dragon", "fbx", "*.fbx"))[0]
OUT = os.path.join(ROOT, "SourceArt", "Creatures", "CrimsonDragon")
PREVIEW = "preview" in sys.argv
FPS = 30
SCALE = 1.4      # Tripo's model is 0.6 m tall; this makes it about 2 m nose to tail once the tail is straight (as the old Rodin dragon)
I3 = Matrix.Identity(3)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
scene = bpy.context.scene
scene.render.fps = FPS
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
body.name, arm.name, arm.data.name = "Body", "Armature", "Armature"
os.makedirs(OUT, exist_ok=True)
for bone in arm.data.bones:
    bone.name = bone.name.replace("tripo::", "")  # Unreal would read "tripo::" as an FBX namespace


def select(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


# --- the lower jaw (in Tripo's space: facing -Y, the head turned towards +X) --------------------------------------------------
# The mouth line runs at z 0.432 from the snout (y -0.49) back to the corner (y -0.415); everything of the head below it is the jaw.
def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


MOUTH_Z, CORNER_Y, CHIN_Z = 0.432, -0.415, 0.38  # MOUTH_Z: just under the upper teeth tips


def in_jaw(p):
    return p.y < CORNER_Y and CHIN_Z < p.z < MOUTH_Z and -0.02 < p.x < 0.24


# Tripo's mouth is one closed surface: opening the jaw would stretch the lips into a sheet. Cut it along the mouth line (each lip gets
# its own vertices) and leave the cut open: the material is one-sided, so the open mouth shows through the head, no dark sheet.

world = body.matrix_world
bm = bmesh.new()
bm.from_mesh(body.data)
jaw_faces = {f for f in bm.faces if in_jaw(world @ f.calc_center_median())}
lips = []  # (s, t): the lip edge as the upper lip's face runs it
for e in bm.edges:
    if len(e.link_faces) == 2 and (e.link_faces[0] in jaw_faces) != (e.link_faces[1] in jaw_faces) and \
            all((world @ v.co).z > MOUTH_Z - 0.02 and (world @ v.co).y < CORNER_Y + 0.005 for v in e.verts):
        upper = e.link_faces[0] if e.link_faces[1] in jaw_faces else e.link_faces[1]
        loop = next(l for l in upper.loops if l.edge == e)
        lips.append((loop.vert, loop.link_loop_next.vert))
lower = {v: bm.verts.new(v.co, v) for v in {v for pair in lips for v in pair}}  # the lower lip's copy, with the same weights for now
for f in [f for f in jaw_faces if any(v in lower for v in f.verts)]:
    rebuilt = bm.faces.new([lower.get(v, v) for v in f.verts], f)
    for old, new in zip(f.loops, rebuilt.loops):
        new.copy_from(old)
    jaw_faces.discard(f)
    jaw_faces.add(rebuilt)
    bm.faces.remove(f)
bm.verts.index_update()
upper_lip = {v.index for v in lower}
jaw_verts = {v.index: v.co.copy() for f in jaw_faces for v in f.verts if v.index not in upper_lip}
bm.to_mesh(body.data)
bm.free()
jaw_weights = {}
for index, co in jaw_verts.items():
    w = smooth((CORNER_Y - (world @ co).y) / 0.03)  # the whole lower jaw, easing in from the mouth's corner
    if w > 0:
        jaw_weights[index] = w
print("[build_dragon] mouth cut along", len(lips), "edges")
JAW = (Vector((0.11, -0.42, 0.432)), Vector((0.12, -0.48, 0.422)))

# --- fit: face +X, scale, centre the spine on Y = 0 ---------------------------------------------------------------------------
FIT = Matrix.Diagonal((SCALE, SCALE, SCALE, 1)) @ Matrix.Rotation(math.radians(90), 4, "Z")
arm.matrix_world = FIT @ arm.matrix_world
select(arm, body)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
centre = (arm.data.bones["Spine_0"].head_local.y + arm.data.bones["Spine_3"].tail_local.y) / 2
arm.location.y -= centre
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
FIT = Matrix.Translation((0, -centre, 0)) @ FIT

select(arm)
bpy.ops.object.mode_set(mode="EDIT")
jaw = arm.data.edit_bones.new("jaw")
jaw.head, jaw.tail = FIT @ JAW[0], FIT @ JAW[1]
jaw.parent = arm.data.edit_bones["Head_5"]  # the bone the whole head is weighted to
bpy.ops.object.mode_set(mode="OBJECT")
group = body.vertex_groups.new(name="jaw")
for index, w in jaw_weights.items():
    for g in body.data.vertices[index].groups:
        g.weight *= 1 - w
    group.add([index], w, "REPLACE")
print("[build_dragon] jaw verts:", len(jaw_weights))

# Tripo's weights are mostly sound, but a few bones reach far: a toe of the left hind leg (which Tripo rigged twice) has weight in the
# head, the wings and the front feet, and the tail tip, curled beside that foot, shares weight with its toes. Each vertex keeps only the
# bones of its own part (the part it is mostly weighted to) and of the spine; a spine vertex also keeps its strongest other part.
PARTS = {"tail": ["bone_78"], "front_L": ["bone_13"], "front_R": ["bone_16"], "hind_L": ["bone_52", "bone_93"], "hind_R": ["bone_65"],
         "wing_L": ["bone_20"], "wing_R": ["bone_36"], "head": ["Head_0"]}
part_of = {}
for part, tops in PARTS.items():
    for top in tops:
        for bone in [arm.data.bones[top]] + list(arm.data.bones[top].children_recursive):
            part_of[bone.name] = part
part_of["jaw"] = "head"
group_part = {g.index: part_of.get(g.name, "spine") for g in body.vertex_groups}
# Some vertices belong outright to a bone far away (that toe owns a patch of the right wing, the right thigh the wing's trailing tip):
# a vertex much nearer a bone of another part than its own bone goes wholly to that one.
segments = [(body.vertex_groups[b.name].index, b.head_local, b.tail_local) for b in arm.data.bones]


def segment_distance(p, a, b):
    ab = b - a
    return (p - (a + ab * max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9))))).length


moved = 0
for v in body.data.vertices:
    if not v.groups:
        continue
    own = max(v.groups, key=lambda g: g.weight).group
    distance = {index: segment_distance(v.co, a, b) for index, a, b in segments}
    nearest = min(distance, key=distance.get)
    if group_part[own] in ("hind_L", "hind_R", "front_L", "front_R") and abs(v.co.y) > 0.28 and v.co.z > 0.12:  # out past the feet, off the ground: a wing's trailing tip
        nearest = min((i for i in distance if group_part[i] == ("wing_L" if v.co.y > 0 else "wing_R")), key=distance.get)
        distance[nearest] = 0.0
    if group_part[nearest] != group_part[own] and distance[own] > 2.5 * distance[nearest] + 0.04:
        for g in list(v.groups):
            body.vertex_groups[g.group].remove([v.index])
        body.vertex_groups[nearest].add([v.index], 1.0, "REPLACE")
        moved += 1
print("[build_dragon] verts given to their nearest bone:", moved)
trimmed = 0
for v in body.data.vertices:
    totals = {}
    for g in v.groups:
        totals[group_part[g.group]] = totals.get(group_part[g.group], 0.0) + g.weight
    own = max(totals, key=totals.get)
    keep = {own, "spine"}
    if own == "spine" and len(totals) > 1:
        keep.add(max((p for p in totals if p != "spine"), key=totals.get))
    drop = [g.group for g in v.groups if group_part[g.group] not in keep]
    for index in drop:
        body.vertex_groups[index].remove([v.index])
    trimmed += bool(drop)
print("[build_dragon] weights trimmed on", trimmed, "verts")

for kind, suffix in (("basecolor", ""), ("normal", "_Normal"), ("roughness", "_Roughness")):
    image = next(i for i in bpy.data.images if i.name.lower().endswith(kind))
    image.scale(2048, 2048)
    image.filepath_raw = os.path.join(OUT, f"CrimsonDragon{suffix}.png")
    image.file_format = "PNG"
    image.save()

# --- the flying pose ----------------------------------------------------------------------------------------------------------
BONES = arm.data.bones
PARENT = {b.name: b.parent.name if b.parent else None for b in BONES}
ORDER = sorted(PARENT, key=lambda n: len(BONES[n].parent_recursive))
REST = {b.name: b.matrix_local.to_3x3() for b in BONES}
DIR = {b.name: (b.tail_local - b.head_local).normalized() for b in BONES}
HIPS = BONES["Spine_0"].head_local.copy()


def rx(deg):   # about world X: positive lifts the left (+Y) side
    return Matrix.Rotation(math.radians(deg), 3, "X")


def ry(deg):   # about world Y: positive tips the nose (+X) down
    return Matrix.Rotation(math.radians(deg), 3, "Y")


def rz(deg):   # about world Z: positive turns the nose to the left (+Y)
    return Matrix.Rotation(math.radians(deg), 3, "Z")


LEGS = {  # each leg's bones from the hip down (toes follow), with where each points in flight: front legs tucked, hind legs folded under the hips
    "front": (["bone_13", "bone_14", "bone_15"], ["bone_17", "bone_18", "bone_19"], ((-0.6, 0, -1), (1, 0, 0.15), (0.2, 0, -1))),
    "hind": (["bone_52", "bone_53", "bone_54", "bone_55"], ["bone_65", "bone_66", "bone_67", "bone_68"], ((0.35, 0, -1), (-1, 0, -0.55), (-0.35, 0, -1), (-1, 0, -0.15))),  # folded under the hips (trailing straight back, they reached the wings)
}
WINGS = {"L": ("bone_21", "bone_22", ["bone_25", "bone_28", "bone_31", "bone_34"]), "R": ("bone_37", "bone_38", ["bone_41", "bone_44", "bone_47", "bone_50"])}
TAIL = ["bone_78"] + [f"Tail_{i}" for i in range(13)] + ["bone_92"]
NECK = ["Head_0", "Head_1", "Head_2", "Head_3", "Head_4"]

AIMS = {}
for left, right, targets in LEGS.values():
    for bones in (left, right):
        AIMS.update(zip(bones, targets))
AIMS.update(zip(["bone_93", "bone_94", "bone_95", "bone_96"], LEGS["hind"][2]))  # Tripo gave the left hind leg two overlapping chains
for i, name in enumerate(TAIL):
    AIMS[name] = (-1, 0, -0.35 if i == 0 else -0.05)
AIMS.update(zip(NECK, ((1, 0, 0.45), (1, 0, 0.35), (1, 0, 0.25), (1, 0, 0.05), (1, 0, -0.05))))



def spread(side, s):
    """Turns a raised wing out flat: its leading edge (shoulder to wrist) out to the side and a little forward, its membrane (the plane
    through the leading edge and the fingertips) level, the fingers trailing behind."""
    arm_bone, forearm, fingers = WINGS[side]
    shoulder = BONES[arm_bone].head_local
    lead = (BONES[forearm].tail_local - shoulder).normalized()
    tips = sum((BONES[f"bone_{int(f[5:]) + 1}"].tail_local for f in fingers), Vector()) / len(fingers) - shoulder
    trail = (tips - lead * tips.dot(lead)).normalized()
    to_lead = Vector((0.2, s, 0.06)).normalized()
    to_trail = Vector((-1, 0.2 * s, 0))
    to_trail = (to_trail - to_lead * to_trail.dot(to_lead)).normalized()
    source = Matrix((lead, trail, lead.cross(trail))).transposed()
    target = Matrix((to_lead, to_trail, to_lead.cross(to_trail))).transposed()
    return target @ source.transposed()


def make_base(aims, spread_wings, like=None):
    """Each bone's world rotation in a base pose (the bones in `aims` pointed there, the wings spread or not, the rest as at rest), and
    the turn that takes it there from its parent's."""
    base, base_delta = {}, {}
    for name in ORDER:
        parent = base[PARENT[name]] if PARENT[name] else I3
        delta = I3
        if name in aims:
            delta = (parent @ DIR[name]).rotation_difference(Vector(aims[name]).normalized()).to_matrix()
            if like:  # turned from that base pose's bone (its twist kept), not from the rest pose's: the coiled tail's bones came out each
                # twisted its own way about its length, and the skin between them wrung
                delta = (like[name] @ DIR[name]).rotation_difference(Vector(aims[name]).normalized()).to_matrix() @ like[name] @ parent.inverted()
        for side, s in (("L", 1), ("R", -1)):
            if spread_wings and name == WINGS[side][0]:
                delta = spread(side, s)
        base_delta[name] = delta
        base[name] = delta @ parent
    return base, base_delta


BASE, BASE_DELTA = make_base(AIMS, True)  # flying
# Standing: Tripo's rest pose is a quadruped on all fours (legs under it, feet on the ground); its raised wings are folded back along the
# body, its turned head straightened and its coiled tail laid out behind, sloping down to the ground.
STAND_AIMS = {name: (-1, 0, -0.55 if i == 0 else -0.2 if i < 4 else -0.02) for i, name in enumerate(TAIL)}
STAND_AIMS.update(zip(NECK, ((1, 0, 1.2), (1, 0, 0.9), (1, 0, 0.6), (1, 0, 0.25), (1, 0, -0.1))))
STAND, STAND_DELTA = make_base(STAND_AIMS, False, like=BASE)

# --- animation ----------------------------------------------------------------------------------------------------------------
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"


def pose(frame, turns, root_turn=I3, lift=0.0, stand=0.0):
    """Keys the base pose at `frame` with `turns` (bone -> world rotation about its head) on top; children follow their parents and
    keep their own bend of the base pose. root_turn tips the whole dragon about the hips; lift raises it. The base pose is the flying
    one at stand 0 and the standing one at 1, each bone's turn from its parent's blended between the two."""
    world, base = {}, {}
    for name in ORDER:
        pb = arm.pose.bones[name]
        parent = world[PARENT[name]] if PARENT[name] else I3
        base_parent = base[PARENT[name]] if PARENT[name] else I3
        base_delta = BASE_DELTA[name].to_quaternion().slerp(STAND_DELTA[name].to_quaternion(), stand).to_matrix() if stand else BASE_DELTA[name]
        base[name] = base_delta @ base_parent
        extra = parent @ base_parent.inverted()
        delta = turns.get(name, I3) @ extra @ base_delta @ extra.inverted()
        if name == "Root":
            delta = root_turn
        world[name] = delta @ parent
        pb.rotation_quaternion = (REST[name].inverted() @ parent.inverted() @ delta @ parent @ REST[name]).to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if name == "Root":
            pivot = HIPS - BONES["Root"].head_local
            pb.location = REST[name].inverted() @ ((pivot - root_turn @ pivot) + Vector((0, 0, lift)))
            pb.keyframe_insert("location", frame=frame)


# Walking: a lateral-sequence walk (left hind, left front, right hind, right front, a quarter cycle apart); running (Shift): a trot
# (diagonal pairs together, half a cycle apart). Each foot is planted for `stance` of the cycle while its leg swings back about the hip,
# then lifted (the lower leg folds) and swung forward; every foot moves `stride` back along the ground while planted, so a longer leg
# swings through a smaller angle. The feet stay level.
LEG_LENGTH = [BONES[left[0]].head_local.z for left, _, _ in LEGS.values()]  # hip to ground, standing
WALK = dict(offsets=((0.25, 0.75), (0.0, 0.5)), stance=0.65, stride=0.8 * min(LEG_LENGTH), frames=48, fold=45)  # front left, right; hind left, right
RUN = dict(offsets=((0.0, 0.5), (0.5, 0.0)), stance=0.5, stride=0.8 * min(LEG_LENGTH), frames=24, fold=60)
for gait, label in ((WALK, "walk"), (RUN, "run")):
    gait["angles"] = [math.degrees(math.asin(gait["stride"] / 2 / length)) for length in LEG_LENGTH]
    print(f"[build_dragon] {label}: legs {[round(l, 3) for l in LEG_LENGTH]} m, stride {gait['stride']:.3f} m, swing {[round(a, 1) for a in gait['angles']]} deg; "
          f"at rate 1 the body moves {100 * gait['stride'] / (gait['stance'] * gait['frames'] / FPS):.2f} model cm/s")


def step(u, angle, gait):
    """A leg's swing about its hip (degrees, + back) and its lower leg's fold at `u` (0-1) through its own cycle."""
    stance = gait["stance"]
    if u < stance:
        swing, fold = 2 * u / stance - 1, 0.0
    else:
        v = (u - stance) / (1 - stance)
        swing, fold = 1 - 2 * smooth(v), gait["fold"] * math.sin(math.pi * v)
    return angle * swing, fold


def flying(flap=0.0, fold=0.0, sweep=0.0, neck=0.0, head=0.0, jaw=0.0, tail_wave=0.0, tail_lift=0.0, legs=0.0, leg_swing=0.0, lift=0.0,
           pitch=0.0, phase=0.0, stand=0.0, stride=None, gait=WALK):
    """flap: wings up (+) or down (-) in degrees; fold: the forearm bends up (+) from the arm; sweep: the fingers swing back (+);
    neck/head: extra nose-up; jaw: mouth open; tail_wave/tail_lift: the tail side to side / up and down, a wave running out to its tip
    with phase (radians); legs: tucked tighter (+); leg_swing: the legs swing loose with phase, the lower leg after the upper, the hind
    after the front; pitch: nose-down of the body; stand: 0 flying, 1 standing; stride: on foot, the gait's cycle fraction (0-1)."""
    turns = {"jaw": ry(jaw), NECK[0]: ry(-neck * 0.5), NECK[2]: ry(-neck * 0.5), NECK[4]: ry(-head)}
    bend = 2.5 / (len(TAIL) - 1)  # spread along the tail: the same sway whatever its bone count
    for i, name in enumerate(TAIL[1:]):
        wave = phase - 0.5 * i
        turns[name] = rz(tail_wave * bend * math.sin(wave)) @ ry(tail_lift * bend * math.sin(wave + 1.2))
    for side, s in (("L", 1), ("R", -1)):
        arm_bone, forearm, fingers = WINGS[side]
        turns[arm_bone] = rx(s * flap)
        turns[forearm] = rx(s * (flap * 0.5 + fold))
        for i, finger in enumerate(fingers):
            turns[finger] = rz(-s * sweep * (1 - i * 0.2)) @ rx(s * flap * 0.3)
    for k, (left, right, _) in enumerate(LEGS.values()):
        for j, bones in enumerate((left, right)):
            if stride is not None:
                swing, knee = step((stride + gait["offsets"][k][j]) % 1.0, gait["angles"][k], gait)
                turns[bones[0]], turns[bones[1]], turns[bones[-1]] = ry(swing), ry(knee), ry(-swing - knee)
            else:
                lag = phase - 0.9 * k - 0.4 * j
                turns[bones[0]] = ry(-legs + leg_swing * math.sin(lag))
                turns[bones[1]] = ry(leg_swing * 0.8 * math.sin(lag - 0.9))
    for twin, bone in zip(["bone_93", "bone_94", "bone_95", "bone_96"], LEGS["hind"][0]):  # Tripo gave the left hind leg two overlapping chains
        if bone in turns:
            turns[twin] = turns[bone]
    return dict(turns=turns, root_turn=ry(pitch), lift=lift, stand=stand)


def key(frame, **kwargs):
    pose(frame, **flying(**kwargs))


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


def straight():  # the flying pose, still (not exported: the saddle and the mouth are measured in it)
    key(0, flap=-6, fold=-4, sweep=-6)


def idle():   # a hover: slow, shallow beats, the legs hanging loose and the tail swaying
    for f in range(0, 61, 3):
        p = 2 * math.pi * f / 60
        s = math.sin(p)
        key(f, flap=14 * s, fold=-8 * s, sweep=4 * s, neck=3 * s, tail_wave=10, tail_lift=5, leg_swing=7, lift=0.04 * s, phase=p)


def flap():   # the working beat, three to a loop: wings high to low, the body rising on the downstroke; the legs and tail swing once
    for f in range(0, 55):
        p = 2 * math.pi * f / 54
        s = math.sin(3 * p)
        key(f, flap=38 * s, fold=-22 * s, sweep=10 * s, neck=-4 * s, legs=-6 * s, tail_wave=8, tail_lift=6, leg_swing=6, lift=-0.12 * s, phase=p)


def glide():  # wings out and steady, a slow sway; the tail steers in long waves, the legs trail
    for f in range(0, 49, 2):
        p = 2 * math.pi * f / 48
        s = math.sin(p)
        key(f, flap=4 * s - 6, fold=3 * s - 4, sweep=-6, tail_wave=12, tail_lift=4, leg_swing=4, phase=p)


def breath(stand=0.0):  # rears the head back and opens wide (0-10), thrusts and holds the mouth open for the flames (10-40), then closes
    for f in range(0, 49, 2):
        draw = smooth(f / 10) * (1 - smooth((f - 40) / 8))
        thrust = smooth((f - 10) / 3) * (1 - smooth((f - 40) / 8))
        key(f, flap=3 * math.sin(f / 6.0) * (1 - stand), neck=30 * (draw - thrust), head=20 * draw - 25 * thrust, jaw=10 * draw + 4 * thrust,
            lift=0.02 * thrust * (1 - stand), stand=stand)


def folded_wings(p):
    """The folded wings stir (each joint a little after the one before), like the tail: phase p."""
    return dict(flap=2.5 * math.sin(p), fold=3 * math.sin(p - 0.8), sweep=2.5 * math.sin(p - 1.6))


def stand():  # on all fours, wings folded: the head looks about, the tail sways, the wings stir
    for f in range(0, 91, 3):
        p = 2 * math.pi * f / 90
        key(f, stand=1, neck=4 * math.sin(p), head=-3 * math.sin(2 * p), tail_wave=6, tail_lift=2, phase=p, **folded_wings(p))


def on_foot(gait, nod):  # a walk or run cycle (above); the head nods with each front step
    for f in range(0, gait["frames"] + 1):
        u = f / gait["frames"]
        key(f, stand=1, stride=u, gait=gait, neck=nod * math.sin(4 * math.pi * u), tail_wave=8, tail_lift=2, phase=2 * math.pi * u,
            **folded_wings(2 * math.pi * u))


def land():  # flight onto all fours (played once, then Stand): the legs reach down and forward, two braking beats, the wings fold
    for f in range(0, 31):
        w = smooth(f / 26)
        s = math.sin(2 * math.pi * f / 15)
        key(f, stand=w, flap=(30 * s + 10) * (1 - w), fold=-15 * s * (1 - w), sweep=-10 * (1 - w), legs=20 * math.sin(math.pi * w),
            tail_wave=6, tail_lift=6 * (1 - w), phase=2 * math.pi * f / 30)


def take_off():  # all fours into flight (played once, then Flap): the wings open and beat hard, the legs push off and trail
    for f in range(0, 25):
        w = 1 - smooth(f / 14)
        s = math.sin(2 * math.pi * f / 12)
        key(f, stand=w, flap=40 * s * (1 - w), fold=-22 * s * (1 - w), sweep=10 * s * (1 - w), tail_wave=8 * (1 - w), tail_lift=6 * (1 - w),
            leg_swing=6 * (1 - w), phase=2 * math.pi * f / 24)


CLIPS = (("Idle", 60, idle), ("Flap", 54, flap), ("Glide", 48, glide), ("Breath", 48, breath), ("BreathStand", 48, lambda: breath(1.0)),
         ("Stand", 90, stand), ("Walk", WALK["frames"], lambda: on_foot(WALK, 3)), ("Run", RUN["frames"], lambda: on_foot(RUN, 5)), ("Land", 30, land), ("TakeOff", 24, take_off))
actions = {}
for name, frames, build in CLIPS:
    action = new_action(name, frames)
    build()
    actions[name] = action
    print(f"[build_dragon] clip {name}: {frames} frames")
actions["Straight"] = new_action("Straight", 0)
straight()

SADDLE = Vector((0.22, 0.0, 0.50))  # ShinobiDragonMount.cpp's FDragonLook Saddle, in metres
# --- numbers for ShinobiDragonMount.cpp -----------------------------------------------------------------------------------------
# The rider sits at SADDLE in the flying pose. The saddle rides the spine bone nearest it, so it follows the back in every clip (the
# rest pose's chest is raised: standing, the seat is higher). Printed: that bone, the seat's height above the feet when standing, and
# the box round the dragon about the seat (lowest corner, highest corner) in flight and on the ground: the room it needs. Model cm.
def posed(clip, frame):
    use_action(actions[clip])
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    return {pb.name: arm.matrix_world @ pb.matrix for pb in arm.pose.bones}


def posed_points():
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    co = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get("co", co)
    evaluated.to_mesh_clear()
    return co.reshape(-1, 3) @ np.array(body.matrix_world.to_3x3()).T + np.array(body.matrix_world.translation)


fly = posed("Straight", 0)
saddle_bone = min((b.name for b in BONES[NECK[0]].parent_recursive),
                  key=lambda n: segment_distance(SADDLE, fly[n].translation, fly[n] @ Vector((0, BONES[n].length, 0))))
saddle_local = fly[saddle_bone].inverted() @ SADDLE
seat = posed("Stand", 0)[saddle_bone] @ saddle_local
feet = posed_points()[:, 2].min()
print(f"[build_dragon] saddle bone {saddle_bone}; standing, the seat is {100 * (seat.z - feet):.1f} above the feet (lowest {100 * feet:.1f}), "
      f"at {tuple(round(100 * c, 1) for c in seat)}")


def room(clips, mask=None):
    lo, hi = np.full(3, 1e9), np.full(3, -1e9)
    for clip in clips:
        for frame in range(0, int(actions[clip].frame_range[1]) + 1, 3):
            seat = np.array(posed(clip, frame)[saddle_bone] @ saddle_local)
            points = posed_points() - seat
            points = points if mask is None else points[mask]
            lo, hi = np.minimum(lo, points.min(axis=0)), np.maximum(hi, points.max(axis=0))
    return tuple(round(100 * c, 1) for c in lo), tuple(round(100 * c, 1) for c in hi)


print("[build_dragon] room flying (about the seat):", *room(["Idle", "Flap", "Glide", "Breath", "Land", "TakeOff"]))
print("[build_dragon] room standing (about the seat):", *room(["Stand", "Walk", "BreathStand"]))

# --- previews -----------------------------------------------------------------------------------------------------------------
if PREVIEW:
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 560
    cams = {}
    for name, loc, rot, ortho in (("side", (-0.25, -5.0, 0.3), (math.radians(90), 0, 0), 2.6), ("front", (5.5, 0.0, 0.3), (math.radians(90), 0, math.radians(90)), 1.8),
                                  ("top", (-0.25, 0.0, 6.0), (0, 0, 0), 2.6), ("three", (2.2, -2.2, 1.6), (math.radians(65), 0, math.radians(45)), 0),
                                  ("head", (0.75, -3.0, 0.45), (math.radians(90), 0, 0), 0.6)):
        cam = bpy.data.objects.new("cam_" + name, bpy.data.cameras.new("cam_" + name))
        scene.collection.objects.link(cam)
        cam.location, cam.rotation_euler = loc, rot
        if ortho:
            cam.data.type, cam.data.ortho_scale = "ORTHO", ortho
        cams[name] = cam
    shots = [s for s in os.environ.get("DRAGON_SHOTS", "Glide:0,Flap:5,Flap:14,Breath:8,Breath:26").split(",") if s]
    for shot in shots:
        clip, frame = shot.split(":")
        use_action(actions[clip])
        scene.frame_set(int(frame))
        bpy.context.view_layer.update()
        for view, cam in cams.items():
            scene.camera = cam
            scene.render.filepath = os.path.join(ROOT, "Saved", f"dragon_{clip}_{frame}_{view}.png")
            bpy.ops.render.render(write_still=True)
    # Where a rider sits and where the mouth is, in the glide pose, for the constants in ShinobiDragonMount.cpp.
    use_action(actions["Straight"])
    scene.frame_set(0)
    mesh = body.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    points = [body.matrix_world @ v.co for v in mesh.vertices]
    print("[build_dragon] glide: mesh bounds", tuple(round(min(p[a] for p in points), 2) for a in range(3)), tuple(round(max(p[a] for p in points), 2) for a in range(3)))
    for lo in [x / 10 for x in range(-2, 6)]:
        column = sorted(p.z for p in points if lo < p.x < lo + 0.1 and abs(p.y) < 0.06)
        if column:
            print(f"[build_dragon] glide: x {lo:.1f}: z median {column[len(column) // 2]:.2f}, 85% {column[int(len(column) * 0.85)]:.2f}, top {column[-1]:.2f}")
    snout = max(points, key=lambda p: p.x)
    print("[build_dragon] glide: snout", tuple(round(c, 2) for c in snout))
    raise SystemExit

# --- export -------------------------------------------------------------------------------------------------------------------
common = dict(axis_forward="-Y", axis_up="Z", global_scale=1.0, apply_unit_scale=True, add_leaf_bones=False, armature_nodetype="NULL", path_mode="STRIP",
              mesh_smooth_type="FACE")
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.location = (0, 0, 0)
    pb.rotation_quaternion = (1, 0, 0, 0)
scene.frame_set(0)
select(arm, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "CrimsonDragon.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False, **common)
for name, frames, _ in CLIPS:
    use_action(actions[name])
    scene.frame_start, scene.frame_end = 0, frames
    select(arm, body)  # with the mesh: an armature exported alone is written with its current pose as its rest pose
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"CrimsonDragon_{name}.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=True,
                             bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **common)
    print(f"[build_dragon] wrote CrimsonDragon_{name}.fbx")
