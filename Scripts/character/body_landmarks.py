"""Blender (5.2) helpers shared by the outfit pipeline (skin_outfit.py, bake_hide_mask.py, check_outfit.py): a rigged
roster body's landmarks, its sections, its tail and the test poses (docs/character/outfit-steps.md).

Import from a script next to this file: sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))).
Distances are metres, in the body's own file space (Z up). The body is never turned: `facing` says which way along Y is
forward (+1 or -1), and "behind" is the opposite way.
"""
import math

import bpy
import numpy
from mathutils import Quaternion, Vector

TAIL_CLEARANCE = 0.14  # body vertices this far from every leg bone and from the hips joint, behind it, below it: tail
ELBOW_STRAIGHTEN_MIN = 20  # degrees: test poses straighten a bind pose's elbow bent more (Mixamo bodies: 11-14)
TAIL_MIN_VERTICES = 500  # smaller pieces out there are trouser folds (Hero, Pabu: under 100), not a tail (Cat, Misha: 2000+)

# Test poses: pose name -> [(bone suffix, world axis, degrees)] (world: Z up, "Left" bones on +X). About +Y a negative
# angle raises the left arm (+X turns towards +Z), so the right arm takes the opposite sign; the A-pose arms hang ~39
# degrees out from straight down.
POSES = {
    "idle": [],
    "arms_down": [("LeftArm", (0, 1, 0), 35), ("RightArm", (0, 1, 0), -35)],
    "arms_out": [("LeftArm", (0, 1, 0), -50), ("RightArm", (0, 1, 0), 50)],
    "arms_up": [("LeftArm", (0, 1, 0), -120), ("RightArm", (0, 1, 0), 120)],
    "arms_forward": [("LeftArm", (1, 0, 0), -80), ("RightArm", (1, 0, 0), -80)],
    "jump_tuck": [("LeftUpLeg", (1, 0, 0), -60), ("RightUpLeg", (1, 0, 0), -60), ("LeftLeg", (1, 0, 0), 120),
                  ("RightLeg", (1, 0, 0), 120), ("LeftArm", (0, 1, 0), -30), ("RightArm", (0, 1, 0), 30)],
}


# A MetaHuman (UE Manny) skeleton's bones -> the Mixamo bone whose role they play here: the pipeline names bones the
# Mixamo way, and a MetaHuman's twist and corrective bones count as the limb they sit in.
MANNY = {"pelvis": "Hips", "spine_01": "Spine", "spine_02": "Spine", "spine_03": "Spine1", "spine_04": "Spine1",
         "spine_05": "Spine2", "neck_01": "Neck", "neck_02": "Neck", "head": "Head"}
MANNY_SIDED = (("spine_04", "Shoulder"), ("clavicle", "Shoulder"), ("upperarm", "Arm"), ("lowerarm", "ForeArm"),
               ("wrist", "Hand"), ("hand", "Hand"), ("thumb", "HandThumb"), ("index", "HandIndex"), ("middle", "HandMiddle"),
               ("ring", "HandRing"), ("pinky", "HandPinky"), ("thigh", "UpLeg"), ("calf", "Leg"), ("ankle", "Foot"),
               ("foot", "Foot"), ("ball", "ToeBase"), ("bigtoe", "ToeBase"), ("littletoe", "ToeBase"))


def suffix(name):
    """The bone's Mixamo name: "mixamorig:LeftArm" -> LeftArm; a MetaHuman's upperarm_twist_01_l -> LeftArm."""
    name = name.split(":")[-1]
    if name in MANNY:
        return MANNY[name]
    side = {"_l": "Left", "_r": "Right"}.get(name[-2:])
    return next((side + role for prefix, role in MANNY_SIDED if side and name.startswith(prefix)), name)


def is_arm(group_name):
    return suffix(group_name) in ("LeftArm", "LeftForeArm", "RightArm", "RightForeArm")


def is_hand(group_name):
    return suffix(group_name).startswith(("LeftHand", "RightHand"))


def load_body(fbx):
    """Empty scene, then the rigged body: (armature, mesh), the mesh in its rest shape (armature modifier removed)."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    armature = next((o for o in bpy.context.scene.objects if o.type == "ARMATURE"), None)
    mesh = next((o for o in bpy.context.scene.objects if o.type == "MESH"), None)
    if armature is None or mesh is None:
        raise RuntimeError(f"{fbx} needs an armature and a mesh")
    for modifier in list(mesh.modifiers):
        if modifier.type == "ARMATURE":
            mesh.modifiers.remove(modifier)
    return armature, mesh


def find_bone(bones, name):
    """The bone playing that Mixamo role: of several (a MetaHuman limb and its twist bones), the one nearest the root."""
    found = [b for b in bones if suffix(b.name) == name]
    return min(found, key=lambda b: len(b.parent_recursive)) if found else None


def bone_head(armature, name):
    bone = find_bone(armature.data.bones, name)
    if bone is None:
        raise RuntimeError(f"skeleton has no {name} bone (is it a Mixamo or MetaHuman rig?)")
    return armature.matrix_world @ bone.head_local


def straight_hand(armature, side):
    """The hand joint, or where it hangs with a bind pose's elbow bent more than ELBOW_STRAIGHTEN_MIN straightened (a
    MetaHuman's, bent ~37 deg forward: the game's arms hang straight)."""
    shoulder, elbow, hand = (bone_head(armature, side + b) for b in ("Arm", "ForeArm", "Hand"))
    if math.degrees((elbow - shoulder).angle(hand - elbow)) <= ELBOW_STRAIGHTEN_MIN:
        return hand
    return elbow + (elbow - shoulder).normalized() * (hand - elbow).length


def facing(armature):
    """+1 when the body faces +Y (its toes are ahead of its foot along +Y), else -1."""
    return 1.0 if bone_head(armature, "LeftToeBase").y > bone_head(armature, "LeftFoot").y else -1.0


def _points(mesh):
    count = len(mesh.data.vertices)
    co = numpy.empty(count * 3)
    mesh.data.vertices.foreach_get("co", co)
    co = co.reshape(count, 3)
    m = numpy.array(mesh.matrix_world)
    return co @ m[:3, :3].T + m[:3, 3]


def _torso_mask(mesh):
    """Per vertex: True unless its strongest group is an arm or a hand (A-pose hands hang beside the hips)."""
    names = {g.index: g.name for g in mesh.vertex_groups}
    keep = numpy.ones(len(mesh.data.vertices), dtype=bool)
    for vertex in mesh.data.vertices:
        top = max(vertex.groups, key=lambda g: g.weight, default=None)
        if top is not None and (is_arm(names[top.group]) or is_hand(names[top.group])):
            keep[vertex.index] = False
    return keep


def section(mesh, z, band=0.03, points=None, torso=None):
    """(width, depth, centre_x, centre_y) of the body's torso and legs within `band` of height z (arms and hands left out)."""
    points = _points(mesh) if points is None else points
    torso = _torso_mask(mesh) if torso is None else torso
    near = points[torso & (numpy.abs(points[:, 2] - z) < band)]
    if len(near) == 0:
        return (0.0, 0.0, 0.0, 0.0)
    low, high = near.min(0), near.max(0)
    return (float(high[0] - low[0]), float(high[1] - low[1]), float((low[0] + high[0]) / 2), float((low[1] + high[1]) / 2))


def _segment_distance(points, a, b):
    a, b = numpy.array(a), numpy.array(b)
    ab = b - a
    t = numpy.clip(((points - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
    return numpy.linalg.norm(points - (a + t[:, None] * ab), axis=1)


def find_tail(armature, mesh, points=None):
    """The tail as a capsule {"root", "tip", "radius", "behind", "vertices", "indices"} (file space, metres), or None.
    Tail = the biggest connected piece of body vertices below the hips joint, behind it, and further than TAIL_CLEARANCE
    from every leg bone and from the hips joint itself. Root = its end nearest the hips."""
    points = _points(mesh) if points is None else points
    forward = facing(armature)
    hips = numpy.array(bone_head(armature, "Hips"))
    far = numpy.linalg.norm(points - hips, axis=1) > TAIL_CLEARANCE
    for side in ("Left", "Right"):
        joints = [bone_head(armature, f"{side}{b}") for b in ("UpLeg", "Leg", "Foot")]
        for a, b in zip(joints, joints[1:]):
            far &= _segment_distance(points, a, b) > TAIL_CLEARANCE
    behind = (hips[1] - points[:, 1]) * forward
    candidates = far & (behind > 0.0) & (points[:, 2] < hips[2] + 0.05) & _torso_mask(mesh)
    if candidates.sum() < TAIL_MIN_VERTICES:
        return None
    # Connected pieces among the candidates.
    edges = numpy.empty(len(mesh.data.edges) * 2, dtype=numpy.int64)
    mesh.data.edges.foreach_get("vertices", edges)
    a, b = edges[0::2], edges[1::2]
    both = candidates[a] & candidates[b]
    parent = numpy.arange(len(points))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, j in zip(a[both].tolist(), b[both].tolist()):
        ri, rj = root(i), root(j)
        if ri != rj:
            parent[ri] = rj
    ids = numpy.array([root(i) for i in numpy.nonzero(candidates)[0]])
    labels, counts = numpy.unique(ids, return_counts=True)
    if counts.max() < TAIL_MIN_VERTICES:
        return None
    biggest = labels[counts.argmax()]
    tail = numpy.nonzero(candidates)[0][ids == biggest]
    pts = points[tail]
    centre = pts.mean(0)
    axis = numpy.linalg.svd(pts - centre)[2][0]
    t = (pts - centre) @ axis
    ends = [centre + t.min() * axis, centre + t.max() * axis]
    ends.sort(key=lambda e: numpy.linalg.norm(e - hips))
    radius = float(numpy.percentile(numpy.linalg.norm(numpy.cross(pts - centre, axis), axis=1), 80))
    return {"root": tuple(float(v) for v in ends[0]), "tip": tuple(float(v) for v in ends[1]), "radius": radius,
            "behind": float(behind[tail].max()), "vertices": int(len(tail)), "indices": tail.tolist()}


SPINE_CHAIN = ("Hips", "Spine", "Spine1", "Spine2", "Neck", "Head")


def spine_point(armature, z):
    """The point on the spine bones' line (Hips up to Head) at height z (clamped to its ends)."""
    heads = sorted((bone_head(armature, name) for name in SPINE_CHAIN), key=lambda v: v.z)
    if z <= heads[0].z:
        return Vector((heads[0].x, heads[0].y, z))
    for low, high in zip(heads, heads[1:]):
        if z <= high.z:
            t = (z - low.z) / max(high.z - low.z, 1e-6)
            return Vector((low.x + (high.x - low.x) * t, low.y + (high.y - low.y) * t, z))
    return Vector((heads[-1].x, heads[-1].y, z))


def _body_tree(mesh):
    from mathutils.bvhtree import BVHTree
    return BVHTree.FromPolygons([mesh.matrix_world @ v.co for v in mesh.data.vertices], [p.vertices[:] for p in mesh.data.polygons])


def skin_depth(armature, mesh, z, tree=None):
    """Front-to-back depth of the body's own skin at height z: rays from the spine line forward and backward stop at the
    first surface they meet, so long hair down the back (Bika) or a tail behind it isn't counted. Widest of five rays
    across the spine line (4 cm apart); 0 if none meets skin both ways."""
    tree = _body_tree(mesh) if tree is None else tree
    forward = Vector((0.0, facing(armature), 0.0))
    centre = spine_point(armature, z)
    best = 0.0
    for dx in (-0.08, -0.04, 0.0, 0.04, 0.08):
        origin = centre + Vector((dx, 0.0, 0.0))
        ahead, behind = tree.ray_cast(origin, forward, 1.0), tree.ray_cast(origin, -forward, 1.0)
        if ahead[0] is not None and behind[0] is not None:
            best = max(best, ahead[3] + behind[3])
    return best


def outer_layer(armature, mesh, tree=None):
    """Indices of torso vertices between the hips and the shoulders that lie outside another layer of the body (long hair
    down the back; above the shoulders the head and the shirt collar fold over themselves, which doesn't matter here):
    a ray from the vertex straight in towards the spine line meets another body surface before reaching it. The cloak
    goes under such a layer: the fitter doesn't lift cloth over it."""
    tree = _body_tree(mesh) if tree is None else tree
    hips_z, shoulder_z = bone_head(armature, "Hips").z, bone_head(armature, "LeftArm").z
    torso = _torso_mask(mesh)
    points = _points(mesh)
    out = []
    for index in numpy.nonzero(torso & (points[:, 2] > hips_z + 0.05) & (points[:, 2] < shoulder_z))[0].tolist():
        co = Vector(points[index])
        target = spine_point(armature, co.z)
        inward = target - co
        reach = inward.length
        if reach < 0.02:
            continue
        inward.normalize()
        hit = tree.ray_cast(co + inward * 0.003, inward, reach - 0.003)
        if hit[0] is not None:
            out.append(index)
    return out


def measure(fbx):
    """Landmarks of a rigged body file (metres): chest/belly/hips (width, depth) sections, shoulder span, joint heights,
    facing and the tail (find_tail(), or None)."""
    armature, mesh = load_body(fbx)
    points, torso = _points(mesh), _torso_mask(mesh)
    shoulder = bone_head(armature, "LeftArm")
    hips = bone_head(armature, "Hips")
    out = {
        "chest": section(mesh, shoulder.z - 0.12, points=points, torso=torso)[:2],
        "belly": section(mesh, (hips.z + shoulder.z) / 2, points=points, torso=torso)[:2],
        "hips": section(mesh, hips.z, points=points, torso=torso)[:2],
        "shoulder_span": float(abs(shoulder.x - bone_head(armature, "RightArm").x)),
        "shoulder_z": float(shoulder.z),
        "head_z": float(bone_head(armature, "Head").z),
        "hips_z": float(hips.z),
        "knee_z": float(bone_head(armature, "LeftLeg").z),
        "facing": facing(armature),
        "tail": find_tail(armature, mesh, points),
    }
    return out


def pose(armatures, name):
    """Applies POSES[name] to every armature given (a body and the outfit skinned to it pose alike): each rotation is
    about a world axis (turned into the armature's space: an FBX armature object is itself rotated), written into the
    pose bone as M^-1 R M (M = the bone's rest matrix in armature space). A bind pose's elbow bent more than
    ELBOW_STRAIGHTEN_MIN (a MetaHuman's, ~37 deg forward) is straightened first in every pose: the game's arms hang straight."""
    for armature in armatures:
        for bone in armature.pose.bones:
            bone.rotation_mode = "QUATERNION"
            bone.rotation_quaternion = (1, 0, 0, 0)

        def turn(bone_name, axis, radians):
            bone = armature.pose.bones[find_bone(armature.data.bones, bone_name).name]
            rest = bone.bone.matrix_local.to_3x3()
            local_axis = (armature.matrix_world.to_3x3().inverted() @ Vector(axis)).normalized()
            delta = (rest.inverted() @ Quaternion(local_axis, radians).to_matrix() @ rest).to_quaternion()
            bone.rotation_quaternion = delta @ bone.rotation_quaternion

        for side in ("Left", "Right"):
            shoulder, elbow, hand = (bone_head(armature, side + b) for b in ("Arm", "ForeArm", "Hand"))
            upper, fore = elbow - shoulder, hand - elbow
            if math.degrees(upper.angle(fore)) > ELBOW_STRAIGHTEN_MIN:
                turn(f"{side}ForeArm", fore.cross(upper), upper.angle(fore))
        for bone_name, axis, degrees in POSES[name]:
            turn(bone_name, axis, math.radians(degrees))
    bpy.context.view_layer.update()


if __name__ == "__main__":
    # blender -b -P body_landmarks.py -- <body_rigged.fbx> <out_tail.json>: the tail capsule and the Hips joint (metres,
    # file space) or null,
    # for Scripts/setup_hero.py (UShinobiOutfitBuilderLibrary::AddTailCapsule).
    import json
    import sys
    cli = sys.argv[sys.argv.index("--") + 1:]
    landmarks = measure(cli[0])
    found = landmarks["tail"]
    if found is not None:
        found = {k: found[k] for k in ("root", "tip", "radius")}
        found["hips"] = tuple(float(v) for v in bone_head(load_body(cli[0])[0], "Hips"))  # setup_hero.py works from the hips
    with open(cli[1], "w", encoding="utf-8") as handle:
        json.dump(found, handle, indent=1)
    print(f"[body_landmarks] wrote {cli[1]}: {'no tail' if found is None else 'tail'}")
