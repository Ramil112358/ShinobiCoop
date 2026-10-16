"""Jutsu cast animations: imports the per-phase clips Scripts/character/build_jutsu_anims.py writes to
SourceArt/Mixamo/Jutsu (<Jutsu>_<Seal|Charge|Release>.fbx, on the Mixamo "Power Up" skeleton), retargets them onto
Manny with the RTG_MixamoToManny retargeter that setup_chakra_pose.py built, and wraps each in a montage,
AM_<Jutsu>_<Phase>, which UShinobiJutsuAbility plays phase by phase (SealMontage, ChargeMontage, ReleaseMontage).
Idempotent: re-running reimports, re-retargets (overwriting in place, so montage references survive) and re-tunes.
One montage per phase rather than sections in one montage: UE Python can't author montage sections.

Run setup_chakra_pose.py first (it creates the Mixamo skeleton/mesh and the retargeter this reuses), and
build_jutsu_anims.py after any pose change. Close the GUI editor before running.
"""
import math
import os
import shutil

import unreal

SOURCE_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Mixamo/Jutsu")
IMPORT_DIR = "/Game/Shinobi/Mixamo/Jutsu"
ANIM_DIR = "/Game/Shinobi/Anims"
MIXAMO_MESH_PATH = "/Game/Shinobi/Mixamo/Power_Up_30fps.Power_Up_30fps"
MIXAMO_SKELETON_PATH = "/Game/Shinobi/Mixamo/Power_Up_30fps_Skeleton.Power_Up_30fps_Skeleton"
MANNY_MESH_PATH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
RETARGETER_PATH = "/Game/Shinobi/Retarget/RTG_MixamoToManny.RTG_MixamoToManny"

ALL_PHASES = ("Seal", "Charge", "Release")
# Which phases each jutsu has a clip for (the new ones have no charge clip unless charging does something).
JUTSU = {
    "Fireball": ALL_PHASES, "Chidori": ALL_PHASES, "Rasengan": ALL_PHASES, "Amaterasu": ALL_PHASES,
    "AlmightyPush": ALL_PHASES,
    "Teleport": ("Seal", "Release"), "Kamui": ("Seal", "Release"), "EarthWall": ("Seal", "Release"), "WaterWall": ("Seal", "Release"),
    "FireMeteor": ("Seal", "Release"), "IceMeteor": ("Seal", "Release"), "WaterDragon": ("Seal", "Release"), "Susanoo": ("Seal", "Release"),
    "Summon": ("Seal", "Release"),  # shared by the toad, Misha, Pabu and dragon
    "Mount": ("Sit",),  # the rider's seated loop (AShinobiPlayerCharacter)
    "Land": ("Roll",),  # the roll after a long fall (AShinobiPlayerCharacter; Scripts/character/build_land_roll.py)
    "Block": ("Guard",),  # the guard pose (AShinobiPlayerCharacter; the Mixamo Standing Block Idle below)
}
# Phase -> (blend in, blend out) seconds. The seals and the charge hand over to the next phase quickly; the release
# blends back out to locomotion more slowly.
PHASES = {"Seal": (0.1, 0.1), "Charge": (0.1, 0.1), "Release": (0.05, 0.25), "Sit": (0.3, 0.3), "Roll": (0.1, 0.3), "Guard": (0.15, 0.2)}

# Clips whose body turns over (see retarget()): the root track carries the turn, so it is not locked.
ROOT_TURNS = {"Land_Roll"}

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# A Mixamo "Sitting Idle" (FBX, without skin, on the Power Up character) saved as SourceArt/Mixamo/Sitting Idle.fbx replaces the
# generated seated loop (build_jutsu_anims.py, timeline Mount).
MIXAMO_SIT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Mixamo/Sitting Idle.fbx")
# The guard pose: a Mixamo "Standing Block Idle" (FBX, without skin, on the Power Up character) as SourceArt/Mixamo/Standing Block Idle.fbx.
MIXAMO_BLOCK = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Mixamo/Standing Block Idle.fbx")


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"missing {path} (run setup_chakra_pose.py first)")
    return asset


def import_clip(jutsu, fbx_name, skeleton):
    """Animation-only import onto the existing Mixamo skeleton; returns the AnimSequence MX_<Jutsu>."""
    fbx = os.path.join(SOURCE_DIR, fbx_name + ".fbx")
    if not os.path.exists(fbx):
        raise RuntimeError(f"missing {fbx}")
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.skeleton = skeleton
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False

    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = IMPORT_DIR
    task.destination_name = f"MX_{jutsu}"
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    anims = [o for o in task.get_objects() if isinstance(o, unreal.AnimSequence)]
    if not anims:
        raise RuntimeError(f"import of {fbx} produced no animation: {[o.get_path_name() for o in task.get_objects()]}")
    unreal.log(f"[setup_jutsu_anims] imported {fbx_name} -> {anims[0].get_path_name()}")
    return anims[0]


def retarget(source_anim, jutsu, mixamo_mesh, manny_mesh, retargeter):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.EditorAssetLibrary.find_asset_data(source_anim.get_path_name())]
    inputs.source_mesh = mixamo_mesh
    inputs.target_mesh = manny_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = source_anim.get_name()
    inputs.replace = f"AS_{jutsu}"
    inputs.target_path = ANIM_DIR
    inputs.use_source_path = False
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    inputs.retain_additive_flags = True
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError(f"retarget produced nothing for {source_anim.get_path_name()}")
    path = f"{ANIM_DIR}/AS_{jutsu}"
    anim = load(path)
    # Played as a stationary cast: no root motion, and the root is pinned so a clip that steps doesn't drag
    # the mesh away from the capsule. Except the landing roll: the retargeter's Root Motion op (rotate_with_pelvis)
    # moves the pelvis rotation onto Manny's root, so pinning the root would keep the body upright through the roll.
    anim.set_editor_property("enable_root_motion", False)
    anim.set_editor_property("force_root_lock", jutsu not in ROOT_TURNS)
    if jutsu.rsplit("_", 1)[-1] in ALL_PHASES:  # the jutsu casts; the guard, roll and seat stay as they are
        keep_hand_contact(source_anim, anim)
    if not unreal.EditorAssetLibrary.save_loaded_asset(anim):
        raise RuntimeError(f"failed to save {path}")
    return anim


# --- Hand contact -------------------------------------------------------------------------------------------------
# The FK retarget copies rotations, and Manny's shoulders are wider than the Mixamo rig's: hands that touch in a seal end
# up ~9 cm apart on Manny. keep_hand_contact puts the left hand back where it was relative to the right one in the
# source, splitting the move between both arms (two-bone IK, each hand keeping its rotation). Full strength while the
# source wrists are within CONTACT_NEAR cm, fading out by CONTACT_FAR (the widest seal, Horse, has them ~24 cm apart).
# UShinobiRetargetAnimInstance does the same again, at runtime, from Manny onto each hero.
CONTACT_NEAR, CONTACT_FAR = 26.0, 36.0
MIXAMO_ARMS = {"l": ("LeftArm", "LeftForeArm", "LeftHand"), "r": ("RightArm", "RightForeArm", "RightHand")}
MANNY_ARMS = {"l": ("upperarm_l", "lowerarm_l", "hand_l"), "r": ("upperarm_r", "lowerarm_r", "hand_r")}
MANNY_PARENTS = {"upperarm_l": "clavicle_l", "upperarm_r": "clavicle_r"}


def q_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def q_inv(q):
    return (-q[0], -q[1], -q[2], q[3])


def q_rot(q, v):
    return q_mul(q_mul(q, (v[0], v[1], v[2], 0.0)), q_inv(q))[:3]


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_add(a, b, s=1.0):
    return (a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_len(a):
    return math.sqrt(v_dot(a, a))


def v_norm(a):
    n = v_len(a) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n)


def q_between(a, b):
    a, b = v_norm(a), v_norm(b)
    c = v_cross(a, b)
    q = (c[0], c[1], c[2], 1.0 + v_dot(a, b))
    n = math.sqrt(sum(x * x for x in q)) or 1.0
    return tuple(x / n for x in q)


def q_from_frame(x, z):
    """The rotation taking +X, +Z to the given (orthonormalised) x and z."""
    x = v_norm(x)
    y = v_norm(v_cross(z, x))
    z = v_cross(x, y)
    m = (x, y, z)  # columns
    t = m[0][0] + m[1][1] + m[2][2]
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        return ((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25 * s)
    i = max(range(3), key=lambda k: m[k][k])
    j, k = (i + 1) % 3, (i + 2) % 3
    s = math.sqrt(1.0 + m[i][i] - m[j][j] - m[k][k]) * 2
    q = [0.0, 0.0, 0.0, (m[j][k] - m[k][j]) / s]
    q[i] = 0.25 * s
    q[j] = (m[i][j] + m[j][i]) / s
    q[k] = (m[i][k] + m[k][i]) / s
    return tuple(q)


def bone(pose, name, space=unreal.AnimPoseSpaces.WORLD):
    t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, space)
    r = t.rotation
    return (t.translation.x, t.translation.y, t.translation.z), (r.x, r.y, r.z, r.w), t


def body_frame(pose, shoulders, head, pelvis):
    """Rotation of the body in component space: x across the shoulders (left to right), z up the spine."""
    lateral = v_sub(bone(pose, shoulders[1])[0], bone(pose, shoulders[0])[0])
    up = v_sub(bone(pose, head)[0], bone(pose, pelvis)[0])
    return q_from_frame(lateral, up)


def solve_arm(shoulder, elbow, wrist, target):
    """Two-bone IK keeping the current bend plane: the new elbow and wrist positions."""
    a, b = v_len(v_sub(elbow, shoulder)), v_len(v_sub(wrist, elbow))
    to_target = v_sub(target, shoulder)
    d = min(max(v_len(to_target), abs(a - b) + 0.01), a + b - 0.01)
    along = v_norm(to_target)
    bend = v_sub(elbow, shoulder)
    pole = v_norm(v_add(bend, along, -v_dot(bend, along)))
    x = (a * a - b * b + d * d) / (2 * d)
    h = math.sqrt(max(a * a - x * x, 0.0))
    new_elbow = v_add(v_add(shoulder, along, x), pole, h)
    return new_elbow, v_add(shoulder, along, d)


def keep_hand_contact(source, anim):
    A = unreal.AnimPoseExtensions
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    ref_mx = A.get_anim_pose_at_frame(source, 0, options)  # any frame: only the body's facing is read from it
    ref_mn = A.get_anim_pose_at_frame(anim, 0, options)
    # Component space of the source onto Manny's (both face the same way after the retarget, but the axes may differ).
    to_manny = q_mul(body_frame(ref_mn, ("upperarm_l", "upperarm_r"), "head", "pelvis"),
                     q_inv(body_frame(ref_mx, ("LeftArm", "RightArm"), "Head", "Hips")))
    scale = v_len(v_sub(bone(ref_mn, "hand_r")[0], bone(ref_mn, "lowerarm_r")[0])) / \
        v_len(v_sub(bone(ref_mx, "RightHand")[0], bone(ref_mx, "RightForeArm")[0]))
    frames = unreal.AnimationLibrary.get_num_frames(anim) + 1
    keys = {name: ([], [], []) for side in MANNY_ARMS.values() for name in side}
    touched = 0
    for f in range(frames):
        src = A.get_anim_pose_at_frame(source, f, options)
        pose = A.get_anim_pose_at_frame(anim, f, options)
        sl, sr = bone(src, "LeftHand")[0], bone(src, "RightHand")[0]
        apart = v_len(v_sub(sl, sr))
        t = min(max((apart - CONTACT_NEAR) / (CONTACT_FAR - CONTACT_NEAR), 0.0), 1.0)
        weight = 1.0 - t * t * (3 - 2 * t)
        world = {name: bone(pose, name) for side in MANNY_ARMS.values() for name in side}
        local = {name: bone(pose, name, unreal.AnimPoseSpaces.LOCAL)[2] for name in world}
        rotations = {name: local[name].rotation for name in world}
        if weight > 0.001:
            touched += 1
            want = v_add(world["hand_r"][0], q_rot(to_manny, v_sub(sl, sr)), scale)
            gap = v_sub(want, world["hand_l"][0])
            targets = {"l": v_add(world["hand_l"][0], gap, 0.5 * weight), "r": v_add(world["hand_r"][0], gap, -0.5 * weight)}
            for side, (upper, lower, hand) in MANNY_ARMS.items():
                (s, s_rot, _), (e, e_rot, _), (w, w_rot, _) = world[upper], world[lower], world[hand]
                new_e, new_w = solve_arm(s, e, w, targets[side])
                swing_upper = q_between(v_sub(e, s), v_sub(new_e, s))
                upper_rot = q_mul(swing_upper, s_rot)
                lower_rot = q_mul(q_between(q_rot(swing_upper, v_sub(w, e)), v_sub(new_w, new_e)), q_mul(swing_upper, e_rot))
                parent_rot = bone(pose, MANNY_PARENTS[upper])[1]
                for name, value in ((upper, q_mul(q_inv(parent_rot), upper_rot)), (lower, q_mul(q_inv(upper_rot), lower_rot)),
                                    (hand, q_mul(q_inv(lower_rot), w_rot))):  # the hand keeps its rotation
                    rotations[name] = unreal.Quat(*value)
        for name in world:
            keys[name][0].append(local[name].translation)
            keys[name][1].append(rotations[name])
            keys[name][2].append(local[name].scale3d)
    if not touched:
        return
    controller = anim.controller
    controller.open_bracket("Keep hand contact", False)
    for name, (positions, rotations, scales) in keys.items():
        controller.set_bone_track_keys(name, positions, rotations, scales, False)
    controller.close_bracket(False)
    unreal.log(f"[setup_jutsu_anims] {anim.get_name()}: hand contact kept on {touched}/{frames} frames")


def remove_old_single_clip(jutsu):
    """The one-clip-per-jutsu assets this script made before the phase split."""
    for path in (f"{ANIM_DIR}/AM_{jutsu}", f"{ANIM_DIR}/AS_{jutsu}", f"{IMPORT_DIR}/MX_{jutsu}"):
        if unreal.EditorAssetLibrary.does_asset_exist(path) and not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"could not delete {path}")


def build_montage(jutsu, anim, blends):
    path = f"{ANIM_DIR}/AM_{jutsu}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        # A montage keeps the length its clip had when it was made, so a clip that changed length needs a new one.
        existing = load(path)
        if abs(existing.get_play_length() - anim.get_play_length()) < 0.01:
            montage = existing
        else:
            unreal.EditorAssetLibrary.delete_asset(path)
            montage = None
    else:
        montage = None
    if montage is None:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", anim)
        montage = TOOLS.create_asset(f"AM_{jutsu}", ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {path}")
    for prop, seconds in zip(("blend_in", "blend_out"), blends):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property("blend_time", seconds)
        montage.set_editor_property(prop, blend)
    if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
        raise RuntimeError(f"failed to save {path}")
    unreal.log(f"[setup_jutsu_anims] {path}: {anim.get_play_length():.2f}s")


if not unreal.EditorAssetLibrary.does_directory_exist(IMPORT_DIR):
    unreal.EditorAssetLibrary.make_directory(IMPORT_DIR)
mixamo_mesh = load(MIXAMO_MESH_PATH)
skeleton = load(MIXAMO_SKELETON_PATH)
manny_mesh = load(MANNY_MESH_PATH)
retargeter = load(RETARGETER_PATH)
if os.path.exists(MIXAMO_SIT):
    shutil.copyfile(MIXAMO_SIT, os.path.join(SOURCE_DIR, "Mount_Sit.fbx"))
    unreal.log("[setup_jutsu_anims] seated loop: using the Mixamo Sitting Idle")
if os.path.exists(MIXAMO_BLOCK):
    shutil.copyfile(MIXAMO_BLOCK, os.path.join(SOURCE_DIR, "Block_Guard.fbx"))
ONLY = os.environ.get("JUTSU_ONLY")  # e.g. Mount: re-import just that jutsu
for jutsu, phases in JUTSU.items():
    if ONLY and jutsu != ONLY:
        continue
    remove_old_single_clip(jutsu)
    for phase in phases:
        blends = PHASES[phase]
        name = f"{jutsu}_{phase}"
        source = import_clip(name, name, skeleton)
        build_montage(name, retarget(source, name, mixamo_mesh, manny_mesh, retargeter), blends)
