"""A RealityScan head (OBJ + diffuse) made ready for the MetaHuman Creator's custom-mesh conform (setup_metahuman.py).

Run: blender -b --factory-startup --python-exit-code 1 -P Scripts/character/prep_mh_head.py -- <Name> [<scan folder> [<out folder>]]

  1. The scan (RealityScan: metres, Y up, face +X once Blender has made it Z up) is turned to face -Y (UE +Y, where the
     Creator's tracking camera looks from) and moved so the eyes' height is at EYE_Z and the head is centred on x 0.
  2. The hair goes: the MetaHuman gets a groom, and the scan's hair (clipped flat by RealityScan's crop box on top) would
     otherwise be fitted as skull. Dark or grey texels joined to the crown or the back of the head are hair; eyebrows, lashes and
     stubble aren't joined to it, so they stay. The shirt and necklace (non-skin joined to the bottom), the neck below
     NECK_CUT under the chin and stray islands go too.
  3. Thinned to FACES and written as <Name>_head_mh.fbx (cm in UE, like every other FBX here).
  4. A front portrait (flat albedo, so the tracker sees a photo) from where the Creator's auto-framing puts its camera, and
     <Name>_head_mh.json with that camera in UE space, for TrackFaceLandmarksFromImage. Plus preview renders.

Writes into SourceArt/Character/Models/<Name>/MetaHuman/ (or <out folder>: a second scan of the same person, e.g. MetaHumanScan1).
"""
import json
import math
import os
import shutil
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
NAME = args[0] if args else "Ramil"
SCAN = args[1] if len(args) > 1 else os.path.join(ROOT, "SourceArt", "Character", "Models", NAME, f"{NAME}_head_RealityScan")
OUT = args[2] if len(args) > 2 else os.path.join(ROOT, "SourceArt", "Character", "Models", NAME, "MetaHuman")
sys.path.insert(0, os.path.dirname(__file__))
from clean_rig import export_unit_scale_fbx  # noqa: E402

EYE_Z = 1.70          # metres; a 186 cm man's eyes. The conform aligns rigidly first, so this only has to be close.
HAIR_LUMA = 0.16      # texels darker than this, or less warm (red - blue) than SKIN_WARMTH, joined to the crown or the
SKIN_WARMTH = 0.07    # back of the head, are hair (black strands, grey highlights, the crop box's white smears)
NECK_CUT = 0.07       # metres under the chin
FACES = 120000
# Phone photogrammetry leaves the skin lumpy (mm-scale bumps), which the conform fits as shape: smoothed out first.
# PREP_MH_SMOOTH overrides (0 = raw); the output name gets the value so variants can be compared.
SMOOTH_ITERATIONS = int(os.environ.get("PREP_MH_SMOOTH", "0"))
PORTRAIT_PX = 1024
FOV_MIN_DEG = 35.0    # the Creator's auto-framing clamps its FOV to 35..110


def log(message):
    print(f"[prep_mh_head] {message}")


def colour_per_vertex(obj):
    """Diffuse RGB at each vertex (mean of its loops' UV lookups)."""
    mat = obj.data.materials[0]
    img = next(n.image for n in mat.node_tree.nodes if n.type == "TEX_IMAGE" and "diffuse" in n.image.name.lower())
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[..., :3]
    me = obj.data
    uv = np.empty(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    lv = np.empty(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    xs = np.clip((uv[:, 0] % 1.0) * w, 0, w - 1).astype(np.int32)
    ys = np.clip((uv[:, 1] % 1.0) * h, 0, h - 1).astype(np.int32)
    acc = np.zeros((len(me.vertices), 3)); cnt = np.zeros(len(me.vertices))
    np.add.at(acc, lv, px[ys, xs]); np.add.at(cnt, lv, 1)
    return acc / np.maximum(cnt, 1)[:, None]


def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=os.path.join(SCAN, f"{NAME}_Head.obj"))
    head = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    head.name = f"{NAME}_head_mh"
    bpy.context.view_layer.objects.active = head
    head.select_set(True)
    # Face +X -> -Y.
    head.rotation_euler.z -= math.pi / 2
    bpy.ops.object.transform_apply(rotation=True, scale=True, location=False)

    me = head.data
    co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    rgb = colour_per_vertex(head)
    luma = rgb @ np.array([0.2126, 0.7152, 0.0722])
    warmth = rgb[:, 0] - rgb[:, 2]
    log(f"scan: {len(me.vertices)} verts, luminance p50 {np.percentile(luma, 50):.2f}, warmth p50 {np.percentile(warmth, 50):.2f}")

    bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
    zmin, zmax, ymax = co[:, 2].min(), co[:, 2].max(), co[:, 1].max()
    dark = (luma < HAIR_LUMA) | (warmth < SKIN_WARMTH)  # black, grey or white: not skin
    # Seeds: non-skin vertices on the crown (clipped top), the back of the head and the bottom (shirt, necklace).
    seeds = np.where(dark & ((co[:, 2] > zmax - 0.03) | (co[:, 1] > ymax - 0.04) | (co[:, 2] < zmin + 0.03)))[0]
    hair = np.zeros(len(co), bool)
    stack = list(seeds)
    while stack:
        i = stack.pop()
        if hair[i]:
            continue
        hair[i] = True
        for e in bm.verts[i].link_edges:
            j = e.other_vert(bm.verts[i]).index
            if dark[j] and not hair[j]:
                stack.append(j)
    # Chin: the lowest point of the front of the face (y < face front + 6 cm) above the neck stump.
    front = co[:, 1] < co[:, 1].min() + 0.06
    chin_z = co[front & (np.abs(co[:, 0]) < 0.02), 2].min() if np.any(front) else co[:, 2].min()
    neck = co[:, 2] < chin_z - NECK_CUT
    drop = hair | neck
    log(f"hair {hair.sum()} verts, neck {neck.sum()} verts, chin z {chin_z:.3f}")
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.where(drop)[0]], context="VERTS")
    # Keep the largest connected piece (the face); hair tufts and debris go.
    bm.verts.ensure_lookup_table()
    islands, seen = [], set()
    for v in bm.verts:
        if v.index in seen:
            continue
        comp, st = [], [v]
        while st:
            a = st.pop()
            if a.index in seen:
                continue
            seen.add(a.index); comp.append(a)
            st.extend(e.other_vert(a) for e in a.link_edges)
        islands.append(comp)
    islands.sort(key=len, reverse=True)
    for comp in islands[1:]:
        bmesh.ops.delete(bm, geom=comp, context="VERTS")
    bm.to_mesh(me); bm.free()

    dec = head.modifiers.new("dec", "DECIMATE")
    dec.ratio = min(1.0, FACES / max(1, len(me.polygons)))
    bpy.ops.object.modifier_apply(modifier=dec.name)
    if SMOOTH_ITERATIONS:
        smooth = head.modifiers.new("smooth", "LAPLACIANSMOOTH")
        smooth.iterations, smooth.lambda_factor, smooth.use_volume_preserve = SMOOTH_ITERATIONS, 0.5, True
        bpy.ops.object.modifier_apply(modifier=smooth.name)

    # Eyes' height: the eye sockets sit ~45% of the way from chin to the scan's top on a face this framed; refined by the
    # tracker anyway, this only places the head near the MetaHuman's.
    co = np.array([v.co[:] for v in me.vertices])
    eye_z = chin_z + 0.12
    off = Vector((-(co[:, 0].max() + co[:, 0].min()) / 2, -(co[:, 1].max() + co[:, 1].min()) / 2, EYE_Z - eye_z))
    me.transform(Matrix.Translation(off))
    me.update()
    log(f"kept {len(me.vertices)} verts / {len(me.polygons)} faces, moved by {tuple(round(c, 3) for c in off)}")

    render_portrait(head)  # before the export, which scales the object x100
    fbx = os.path.join(OUT, f"{NAME}_head_mh.fbx")
    export_unit_scale_fbx(fbx, [head])
    # The scan's photo texture beside it (the FBX keeps the UVs): setup_metahuman.py puts it on the head so the Identity
    # editor's tracker sees a face.
    shutil.copyfile(os.path.join(SCAN, "tex_u1_v1_diffuse.jpg"), os.path.join(OUT, f"{NAME}_head_mh_diffuse.jpg"))
    log(f"wrote {fbx}")


def render_portrait(head):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = PORTRAIT_PX
    sc.view_settings.view_transform = "Standard"
    sc.world = bpy.data.worlds.new("w"); sc.world.color = (0.5, 0.5, 0.5)
    for m in head.data.materials:  # flat albedo: the texture is already a photo
        nt = m.node_tree
        tex = next(n for n in nt.nodes if n.type == "TEX_IMAGE" and "diffuse" in n.image.name.lower())
        em = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(tex.outputs[0], em.inputs[0])
        nt.links.new(em.outputs[0], next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL").inputs[0])
    co = np.array([head.matrix_world @ v.co for v in head.data.vertices])
    lo, hi = co.min(0), co.max(0)
    centre, size = Vector((lo + hi) / 2), float(np.linalg.norm((hi - lo) / 2))
    # Same framing as UMeshTargetContourMechanic::TrackFaceWithAutoFraming, mirrored into Blender (-Y is UE +Y).
    dist = size * 1.8
    cam_pos = centre + Vector((0, -dist, size * 0.2))
    fov = max(FOV_MIN_DEG, math.degrees(2 * math.atan(size / (dist * 2))))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    cam.data.sensor_fit = "HORIZONTAL"; cam.data.angle = math.radians(fov)
    cam.data.clip_start = 0.01
    cam.location = cam_pos
    cam.rotation_euler = (centre - cam_pos).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = os.path.join(OUT, f"{NAME}_portrait.png")
    bpy.ops.render.render(write_still=True)
    look = centre - cam_pos
    # UE: x, -y, z in cm; FRotator from the look vector.
    ue_loc = [cam_pos.x * 100, -cam_pos.y * 100, cam_pos.z * 100]
    ue_dir = Vector((look.x, -look.y, look.z)).normalized()
    pitch = math.degrees(math.asin(ue_dir.z)); yaw = math.degrees(math.atan2(ue_dir.y, ue_dir.x))
    meta = {"portrait": sc.render.filepath, "image_size": PORTRAIT_PX, "camera_location": ue_loc,
            "camera_rotation": {"pitch": pitch, "yaw": yaw, "roll": 0.0}, "fov": fov}
    with open(os.path.join(OUT, f"{NAME}_head_mh.json"), "w") as f:
        json.dump(meta, f, indent=2)
    log(f"portrait + camera {meta}")
    # Previews: side and back, to check what was cut.
    for tag, d in (("side", Vector((1, 0, 0))), ("back", Vector((0, 1, 0))), ("top", Vector((0, 0, 1)))):
        cam.location = centre + d * dist
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y" if tag == "top" else "Z").to_euler()
        sc.render.resolution_x = sc.render.resolution_y = 512
        sc.render.filepath = os.path.join(OUT, f"preview_{tag}.png")
        bpy.ops.render.render(write_still=True)


main()
