"""Blender (5.2) headless skinning of an outfit (a cloak made by Rodin) to the Mixamo skeleton, so it can be worn over
any rigged character (AShinobiPlayerCharacter::Outfits; docs/character/outfit-steps.md).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/skin_outfit.py -- <body_rigged.fbx> <outfit.glb> <out_dir> [Name]

Steps: import the rigged body (the reference skeleton and skin weights) and the outfit GLB -> scale and place the outfit
on the body from the skeleton's landmarks (collar at the head bone, hem just above the knees, centred on the spine) ->
copy the body's skin weights to the nearest
surface (the long skirt then follows the legs only part way, blending into the hips, so it doesn't tear when they
walk) -> parent to the skeleton -> <out_dir>/<Name>_outfit.fbx (skeleton + skinned outfit) and <Name>_outfit.png (its
base-colour texture), plus fit_front.png / fit_side.png renders of the outfit on the body.
Exits with code 1 and a message when a landmark or the bone round trip is wrong.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

GIRTH = 1.04  # the outfit is cut a little wide so it hangs loose
KNEE_DROP = 0.03  # hem this far below the knee bone
LEG_KEEP_AT_HEM = 0.35  # share of the leg weights kept at the hem (the rest goes to the hips)
MAX_INFLUENCES = 4


def fail(message):
    print(f"[skin_outfit] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[skin_outfit] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 3:
    fail("usage: -- <body_rigged.fbx> <outfit.glb> <out_dir> [Name]")
body_fbx, outfit_glb, out_dir = (os.path.abspath(a) for a in args[:3])
name = args[3] if len(args) > 3 else os.path.splitext(os.path.basename(outfit_glb))[0]
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=body_fbx)
armature = next((o for o in bpy.context.scene.objects if o.type == "ARMATURE"), None)
body = next((o for o in bpy.context.scene.objects if o.type == "MESH"), None)
if armature is None or body is None:
    fail(f"{body_fbx} needs an armature and a mesh")
known = set(bpy.context.scene.objects)
bpy.ops.import_scene.gltf(filepath=outfit_glb)
new_meshes = [o for o in bpy.context.scene.objects if o not in known and o.type == "MESH"]
if not new_meshes:
    fail(f"no mesh in {outfit_glb}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj in new_meshes)
bpy.context.view_layer.objects.active = new_meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(new_meshes) > 1:
    bpy.ops.object.join()
outfit = bpy.context.view_layer.objects.active
for obj in list(bpy.context.scene.objects):
    if obj not in (armature, body, outfit) and obj.parent is None:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
outfit.name = name


def bone_head(suffix):
    for bone in armature.data.bones:
        if bone.name.split(":")[-1] == suffix:
            return armature.matrix_world @ bone.head_local
    fail(f"skeleton has no {suffix} bone (is it a Mixamo rig?)")


def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
            Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))))


# Facing: the outfit comes out of glTF facing -Y; turn it when the body's toes point the other way.
foot, toe = bone_head("LeftFoot"), bone_head("LeftToeBase")
if toe.y > foot.y:
    outfit.rotation_euler = (0, 0, math.pi)
    bpy.ops.object.transform_apply(rotation=True)
    log("body faces +Y: turned the outfit around")

head, hips, knee = bone_head("Head"), bone_head("Hips"), bone_head("LeftLeg")
spine = bone_head("Spine2")
centre_x = (bone_head("LeftArm").x + bone_head("RightArm").x) / 2
low, high = bounds(outfit)
target_top, target_hem = head.z, knee.z - KNEE_DROP
scale = (target_top - target_hem) / (high.z - low.z)
outfit.scale = (scale * GIRTH, scale * GIRTH, scale)
bpy.ops.object.transform_apply(scale=True)
low, high = bounds(outfit)
outfit.location = (centre_x - (low.x + high.x) / 2, spine.y - (low.y + high.y) / 2, target_top - high.z)
bpy.ops.object.transform_apply(location=True)
low, high = bounds(outfit)
log(f"outfit {high.z - low.z:.2f} m tall, {high.x - low.x:.2f} m wide; collar z {high.z:.2f}, hem z {low.z:.2f}")

# The body as it lies in the file (no armature modifier): the weight source.
for modifier in list(body.modifiers):
    if modifier.type == "ARMATURE":
        body.modifiers.remove(modifier)

# Skin weights: nearest body surface (interpolated), then soften the legs' pull on the long skirt.
for group in body.vertex_groups:
    outfit.vertex_groups.new(name=group.name)
transfer = outfit.modifiers.new("Weights", "DATA_TRANSFER")
transfer.object = body
transfer.use_vert_data = True
transfer.data_types_verts = {"VGROUP_WEIGHTS"}
transfer.vert_mapping = "POLYINTERP_NEAREST"
transfer.layers_vgroup_select_src = "ALL"
transfer.layers_vgroup_select_dst = "NAME"
bpy.context.view_layer.objects.active = outfit
bpy.ops.object.modifier_apply(modifier=transfer.name)
hips_group = outfit.vertex_groups.get(next((g.name for g in outfit.vertex_groups if g.name.split(":")[-1] == "Hips"), ""))
if hips_group is None:
    fail("no Hips vertex group after the weight transfer")
legs = [g for g in outfit.vertex_groups if any(k in g.name.split(":")[-1] for k in ("UpLeg", "Leg", "Foot", "Toe"))]
span = max(hips.z - low.z, 0.01)
for vertex in outfit.data.vertices:
    fade = min(max((hips.z - (outfit.matrix_world @ vertex.co).z) / span, 0.0), 1.0)
    keep = 1.0 - (1.0 - LEG_KEEP_AT_HEM) * fade
    moved = 0.0
    for group in vertex.groups:
        if outfit.vertex_groups[group.group] in legs:
            moved += group.weight * (1.0 - keep)
            group.weight *= keep
    if moved:
        hips_group.add([vertex.index], moved, "ADD")
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=MAX_INFLUENCES)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
unweighted = sum(1 for v in outfit.data.vertices if not v.groups)
if unweighted:
    fail(f"{unweighted} outfit vertices have no skin weights")


# Fit renders (outfit over the body), before the body leaves the scene.
def render_fit():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 768, 1024
    world = bpy.data.worlds.new("Fit")
    world.color = (0.8, 0.8, 0.8)
    scene.world = world
    camera_data = bpy.data.cameras.new("FitCamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 2.1
    camera = bpy.data.objects.new("FitCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    middle = Vector((centre_x, spine.y, 0.9))
    for view, offset, rotation in (("front", (0, -6, 0), (math.radians(90), 0, 0)),
                                   ("side", (6, 0, 0), (math.radians(90), 0, math.radians(90)))):
        camera.location = middle + Vector(offset)
        camera.rotation_euler = rotation
        scene.render.filepath = os.path.join(out_dir, f"fit_{view}.png")
        bpy.ops.render.render(write_still=True)


render_fit()

# Base-colour texture out, as PNG next to the FBX (UE's importer can't read embedded glTF textures).
image = None
for material in outfit.data.materials:
    if material and material.use_nodes:
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image and (image is None or "base" in node.label.lower()):
                image = node.image
if image is None:
    fail("the outfit has no texture image")
png = os.path.join(out_dir, f"{name}_outfit.png")
image.filepath_raw = png
image.file_format = "PNG"
image.save()
log(f"wrote {png}")

# Skin to the skeleton and export only the skeleton and the outfit.
bpy.data.objects.remove(body, do_unlink=True)
outfit.parent = armature
outfit.matrix_parent_inverse = armature.matrix_world.inverted()
outfit.modifiers.new("Armature", "ARMATURE").object = armature
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
outfit.select_set(True)
bpy.context.view_layer.objects.active = armature
fbx_path = os.path.join(out_dir, f"{name}_outfit.fbx")
bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         bake_anim=False, path_mode="STRIP", apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE")
log(f"exported {fbx_path}")

# Round trip: UE reads the FBX's bone orientations, and a worn outfit only lines up with the body when they match the
# body's (the outfit's bones follow the body's in component space).
reference = {b.name: (armature.matrix_world @ b.matrix_local).copy() for b in armature.data.bones}
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path)
again = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
worst = 0.0
for bone in again.data.bones:
    if bone.name in reference:
        delta = (again.matrix_world @ bone.matrix_local) - reference[bone.name]
        worst = max(worst, max(abs(x) for row in delta for x in row))
log(f"bone round trip: worst matrix difference {worst:.5f} over {len(again.data.bones)} bones")
if worst > 0.001:
    fail("the exported skeleton's bone orientations differ from the body's")
