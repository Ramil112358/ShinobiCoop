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
TAIL_MIN_VERTICES = 500  # smaller pieces out there are trouser folds (Hero, Pabu: under 100), not a tail (Cat, Misha: 2000+)

# Test poses: pose name -> [(bone suffix, axis in armature space, degrees)]. "Left" bones are on +X.
POSES = {
    "idle": [],
    "arms_down": [("LeftArm", (0, 1, 0), 50), ("RightArm", (0, 1, 0), -50)],
    "arms_out": [("LeftArm", (0, 1, 0), -50), ("RightArm", (0, 1, 0), 50)],
    "arms_up": [("LeftArm", (0, 1, 0), -120), ("RightArm", (0, 1, 0), 120)],
    "arms_forward": [("LeftArm", (1, 0, 0), -80), ("RightArm", (1, 0, 0), -80)],
    "jump_tuck": [("LeftUpLeg", (1, 0, 0), -60), ("RightUpLeg", (1, 0, 0), -60), ("LeftLeg", (1, 0, 0), 120),
                  ("RightLeg", (1, 0, 0), 120), ("LeftArm", (0, 1, 0), -30), ("RightArm", (0, 1, 0), 30)],
}


def suffix(name):
    return name.split(":")[-1]


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


def bone_head(armature, name):
    for bone in armature.data.bones:
        if suffix(bone.name) == name:
            return armature.matrix_world @ bone.head_local
    raise RuntimeError(f"skeleton has no {name} bone (is it a Mixamo rig?)")


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
    """The tail as a capsule {"root", "tip", "radius", "behind", "vertices"} (file space, metres), or None.
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
            "behind": float(behind[tail].max()), "vertices": int(len(tail))}


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
    about an armature-space axis, written into the pose bone as M^-1 R M (M = the bone's rest matrix)."""
    for armature in armatures:
        for bone in armature.pose.bones:
            bone.rotation_mode = "QUATERNION"
            bone.rotation_quaternion = (1, 0, 0, 0)
        for bone_name, axis, degrees in POSES[name]:
            bone = next(b for b in armature.pose.bones if suffix(b.name) == bone_name)
            rest = bone.bone.matrix_local.to_3x3()
            turn = Quaternion(Vector(axis), math.radians(degrees)).to_matrix()
            bone.rotation_quaternion = (rest.inverted() @ turn @ rest).to_quaternion()
    bpy.context.view_layer.update()
