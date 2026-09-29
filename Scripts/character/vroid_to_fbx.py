"""Blender (5.2) headless: a VRoid Studio character (.vrm) -> an FBX that Scripts/setup_hero.py imports like the hero.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/vroid_to_fbx.py -- <in.vrm> <out_dir> [height_cm]

A .vrm is a GLB with VRM extensions, so Blender's own glTF importer reads it (it detects GLB by its magic bytes, not the
extension); no VRM add-on needed. Steps:
  - rename the VRM humanoid bones (read from the file's VRM / VRMC_vrm extension, VRM 0.x and 1.0) to Mixamo names
    (Hips, Spine, ..., RightHand, LeftHandPinky3) so the rig auto-characterises with UE's Mixamo template, like the hero,
    and every gameplay bone lookup (GetRightHandBone, body flames) works unchanged. Other bones (hair, skirt) keep their
    names. Bones above the hips (VRoid's Root) are removed: on a Mixamo rig Hips is the root.
  - face -Y (Blender's front; VRM 0.x models come in facing +Y), scale to the hero height (default 180 cm, the Manny's),
    feet at z = 0, all transforms applied, one mesh object.
  - materials renamed to [A-Za-z0-9_] and each one's base-colour image written to <out_dir>/<Material>.png (a flat
    colour swatch when it has none); setup_hero.py builds one material instance per slot from these.
  - export <out_dir>/<name of the .vrm>.fbx.
Exits with code 1 and a message when the file has no VRM humanoid or a required bone is missing.
"""
import json
import os
import re
import struct
import sys

import bpy
from mathutils import Matrix, Vector


def fail(message):
    print(f"[vroid_to_fbx] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[vroid_to_fbx] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 2:
    fail("usage: -- <in.vrm> <out_dir> [height_cm]")
src, out_dir = os.path.abspath(args[0]), os.path.abspath(args[1])
height_m = (float(args[2]) if len(args) > 2 else 180.0) / 100.0
name = os.path.splitext(os.path.basename(src))[0]

# --- VRM humanoid: bone name per humanoid slot, from the GLB's JSON chunk ---
with open(src, "rb") as f:
    data = f.read()
if data[:4] != b"glTF":
    fail(f"{src} is not a GLB/VRM file")
gltf = json.loads(data[20:20 + struct.unpack_from("<I", data, 12)[0]])
extensions = gltf.get("extensions", {})
if "VRMC_vrm" in extensions:
    vrm0 = False
    human = {slot: gltf["nodes"][bone["node"]]["name"] for slot, bone in extensions["VRMC_vrm"]["humanoid"]["humanBones"].items()}
elif "VRM" in extensions:
    vrm0 = True
    human = {bone["bone"]: gltf["nodes"][bone["node"]]["name"] for bone in extensions["VRM"]["humanoid"]["humanBones"]}
else:
    fail(f"{src} has no VRM humanoid (VRM or VRMC_vrm extension)")

MIXAMO = {"hips": "Hips", "spine": "Spine", "chest": "Spine1", "upperChest": "Spine2", "neck": "Neck", "head": "Head"}
for side, Side in (("left", "Left"), ("right", "Right")):
    MIXAMO.update({f"{side}Shoulder": f"{Side}Shoulder", f"{side}UpperArm": f"{Side}Arm", f"{side}LowerArm": f"{Side}ForeArm",
                   f"{side}Hand": f"{Side}Hand", f"{side}UpperLeg": f"{Side}UpLeg", f"{side}LowerLeg": f"{Side}Leg",
                   f"{side}Foot": f"{Side}Foot", f"{side}Toes": f"{Side}ToeBase"})
    for finger, Finger in (("Index", "Index"), ("Middle", "Middle"), ("Ring", "Ring"), ("Little", "Pinky")):
        for joint, number in (("Proximal", 1), ("Intermediate", 2), ("Distal", 3)):
            MIXAMO[f"{side}{finger}{joint}"] = f"{Side}Hand{Finger}{number}"
    # VRM 1.0 renamed the thumb joints: its Metacarpal/Proximal/Distal are VRM 0.x's Proximal/Intermediate/Distal.
    thumb = ("Proximal", "Intermediate", "Distal") if vrm0 else ("Metacarpal", "Proximal", "Distal")
    for number, joint in enumerate(thumb, 1):
        MIXAMO[f"{side}Thumb{joint}"] = f"{Side}HandThumb{number}"
REQUIRED = ("Hips", "Spine", "Neck", "Head", "LeftArm", "LeftForeArm", "LeftHand", "RightArm", "RightForeArm", "RightHand",
            "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot")

# --- import ---
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
if len(armatures) != 1:
    fail(f"expected one armature, found {len(armatures)}")
armature = armatures[0]
# UE's FBX import drops a Blender root node named "Armature" (FbxScene.cpp, Internal_GetRootSkeleton), so Hips becomes
# the root bone as on the hero; under any other name the node becomes an extra, 100x-scaled root bone, and the retargeted
# body shrinks to nothing.
armature.name = "Armature"
armature.data.name = name

# --- VRM 1.0 node constraints -> plain parenting (nothing downstream evaluates them) ---
# Newer VRoid rigs hang helper bones (J_Aim_*_Shoulder, the sleeves' J_Aim_*_TopsUpperArm, J_Aim_*_UpperLeg) off the
# shoulder/hips and aim them at the next limb bone; left alone they stay in the T-pose while the arm moves. An aim helper
# becomes a child of its target's parent (the limb bone it stands in for), a rotation helper a child of its source.
# ponytail: roll helpers already hang off the right bone and just lose their partial twist; bake twist if forearms pinch.
bpy.context.view_layer.objects.active = armature
bpy.ops.object.mode_set(mode="EDIT")
edit = armature.data.edit_bones
baked = 0
for node in gltf["nodes"]:
    constraint = node.get("extensions", {}).get("VRMC_node_constraint", {}).get("constraint", {})
    kind = next((k for k in ("aim", "rotation") if k in constraint), None)
    bone = edit.get(node.get("name", ""))
    source = edit.get(gltf["nodes"][constraint[kind]["source"]]["name"]) if kind else None
    new_parent = (source.parent if kind == "aim" else source) if source else None
    if bone is None or new_parent is None or new_parent == bone or bone in new_parent.parent_recursive:
        continue
    bone.use_connect = False
    bone.parent = new_parent
    baked += 1
bpy.ops.object.mode_set(mode="OBJECT")
if baked:
    log(f"re-parented {baked} constraint-driven helper bones")

# --- rename humanoid bones (bone renames carry the meshes' vertex groups along) ---
bones = armature.data.bones
renames = {human[slot]: MIXAMO[slot] for slot in human if slot in MIXAMO}
missing = [old for old in renames if old not in bones]
if missing:
    fail(f"humanoid bones not in the armature: {missing}")
for bone in list(bones):  # free the Mixamo names first (a non-humanoid bone may already hold one)
    if bone.name in renames.values() and bone.name not in renames:
        bone.name = bone.name + "_vrm"
for index, old in enumerate(renames):
    bones[old].name = f"__humanoid_{index}"
for index, new in enumerate(renames.values()):
    bones[f"__humanoid_{index}"].name = new
lacking = [bone for bone in REQUIRED if bone not in bones]
if lacking:
    fail(f"required humanoid bones missing: {lacking}")
log(f"{'VRM 0.x' if vrm0 else 'VRM 1.0'}: renamed {len(renames)} humanoid bones to Mixamo names")

# --- Hips becomes the root: drop the bones above it (VRoid's Root), re-hanging any other children on the hips ---
bpy.context.view_layer.objects.active = armature
bpy.ops.object.mode_set(mode="EDIT")
edit = armature.data.edit_bones
ancestors = []
parent = edit["Hips"].parent
while parent:
    ancestors.append(parent)
    parent = parent.parent
for ancestor in ancestors:
    for child in ancestor.children:
        if child not in ancestors and child.name != "Hips":
            child.parent = edit["Hips"]
edit["Hips"].parent = None
removed = [ancestor.name for ancestor in ancestors]
for ancestor in ancestors:
    edit.remove(ancestor)
bpy.ops.object.mode_set(mode="OBJECT")
if removed:
    log(f"removed bones above the hips: {removed}")

# --- one mesh object, no parents, no helper empties ---
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and any(m.type == "ARMATURE" for m in o.modifiers)]
if not meshes:
    fail("no skinned mesh")
for obj in bpy.context.scene.objects:
    obj.select_set(obj is armature or obj in meshes)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
for obj in list(bpy.context.scene.objects):
    if obj is not armature and obj not in meshes:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
body.name = body.data.name = f"{name}_Body"


def bone_point(bone_name):
    return armature.matrix_world @ armature.data.bones[bone_name].head_local


def mesh_bounds():
    points = [body.matrix_world @ v.co for v in body.data.vertices]
    return (Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
            Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))))


# --- face -Y, hero height, centred, feet on the floor; then apply everything ---
if "LeftToeBase" in armature.data.bones:
    flip = bone_point("LeftToeBase").y > bone_point("LeftFoot").y  # toes ahead of the ankle = the facing direction
else:
    flip = vrm0  # VRM 0.x faces glTF -Z, which imports as Blender +Y
low, high = mesh_bounds()
scale = height_m / (high.z - low.z)
fix = Matrix.Scale(scale, 4) @ (Matrix.Rotation(3.141592653589793, 4, "Z") if flip else Matrix.Identity(4))
for obj in (armature, body):
    obj.matrix_world = fix @ obj.matrix_world
bpy.context.view_layer.update()
low, high = mesh_bounds()
offset = Vector((-(low.x + high.x) / 2, -(low.y + high.y) / 2, -low.z))
for obj in (armature, body):
    obj.location += offset
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
body.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# The armature modifier stays on the body; parenting makes the FBX exporter skin it to this armature.
body.parent = armature
low, high = mesh_bounds()
log(f"{'turned 180 degrees, ' if flip else ''}height {(high.z - low.z) * 100:.1f} cm")

# --- materials: safe names + one base-colour PNG each ---
os.makedirs(out_dir, exist_ok=True)


def base_color_image(material):
    """The image nearest the material output, breadth-first: the Principled BSDF's Base Color on a lit material (its
    first input), the Emission colour on the unlit one the importer builds for VRoid's MToon materials."""
    output = next((n for n in material.node_tree.nodes if n.type == "OUTPUT_MATERIAL"), None) if material.node_tree else None
    queue = [output.inputs["Surface"]] if output else []
    while queue:
        for link in queue.pop(0).links:
            if link.from_node.type == "TEX_IMAGE" and link.from_node.image:
                return link.from_node.image
            queue.extend(link.from_node.inputs)
    return None


used = set()
for slot in body.material_slots:
    material = slot.material
    if material is None:
        continue
    safe = re.sub(r"[^A-Za-z0-9_]", "_", material.name).strip("_") or "Material"
    while safe in used:
        safe += "_"
    used.add(safe)
    material.name = safe
    png = os.path.join(out_dir, f"{safe}.png")
    image = base_color_image(material)
    if image is None:
        # ponytail: untextured slot becomes a flat swatch of its base colour (VRoid slots are always textured)
        principled = next((n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if material.node_tree else None
        colour = list(principled.inputs["Base Color"].default_value) if principled else list(material.diffuse_color)
        image = bpy.data.images.new(f"{safe}_swatch", 4, 4, alpha=True)
        image.pixels = colour * 16
    image.filepath_raw = png
    image.file_format = "PNG"
    image.save()
log(f"wrote {len(used)} material textures to {out_dir}")

# --- export ---
fbx = os.path.join(out_dir, f"{name}.fbx")
bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, bake_anim=False,
                         apply_scale_options="FBX_SCALE_UNITS", add_leaf_bones=False, mesh_smooth_type="FACE")
log(f"exported {fbx}: {len(body.data.polygons)} faces, {len(armature.data.bones)} bones, {len(body.material_slots)} materials")
