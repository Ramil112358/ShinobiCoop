"""Blender (5.2) headless repaint of the Akatsuki cloak's lower clouds (after prep_outfit.py, before skin_outfit.py).

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/fix_cloak_clouds.py -- <cloak.glb> <cloud_reference.png> <out.glb>

Tripo wraps the reference's lower clouds (cut off at the silhouette in the back view) round the cloak's sides, where they
run into the front ones. Its UV atlas is hundreds of small islands, so this works per texel by where it lies on the cloak:
bake each texel's position and normal -> on the cloth below the cuffs (away from the open front) everything that isn't
the black fabric is painted black -> four clean clouds (cut from the reference's upper back cloud, recoloured to the
texture's red and white) are projected flat onto the front and back, only onto cloth facing the projection, so none wraps.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy

LOWER_TOP = 0.42  # share of the cloak's height: the lower clouds lie below this, the sleeve cuffs above it...
BODY_TOP, BODY_HALF = 0.48, 0.25  # ...except on the coat's body (within this of the middle, m), where their tops reach higher
FRONT_GAP = 25.0  # degrees either side of the front centre left alone (the open front shows the red lining)
CLOUD_X = {-1.0: 0.15, 1.0: 0.12}  # stamped clouds' centres off the middle (m), front and back (the front clears its opening)
CLOUD_Z, CLOUD_W = 0.27, 0.17  # their height (share of the cloak) and width (m)
FACING = (0.2, 0.45)  # a stamp fades in as the cloth's normal turns from this much to this much along the projection
SMOOTH_PASSES, SMOOTH_FADE = 20, 0.05  # relief smoothing passes, and the band (share of the height) it fades out over at the top
REF_CROP = (0.28, 0.23, 0.68, 0.43)  # the reference image's upper back cloud (left, top, right, bottom as shares)


def fail(message):
    print(f"[fix_cloak_clouds] ERROR: {message}")
    sys.exit(1)


def log(message):
    print(f"[fix_cloak_clouds] {message}")


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 3:
    fail("usage: -- <cloak.glb> <cloud_reference.png> <out.glb>")
src, ref_path, dst = (os.path.abspath(a) for a in args[:3])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
cloak = next((o for o in bpy.context.scene.objects if o.type == "MESH"), None)
if cloak is None:
    fail(f"no mesh in {src}")
bpy.context.view_layer.objects.active = cloak
cloak.select_set(True)
tree = cloak.data.materials[0].node_tree
texture = next((n for n in tree.nodes if n.type == "TEX_IMAGE" and n.image), None)
if texture is None:
    fail("the cloak has no texture")
image = texture.image
W, H = image.size

# Tripo embosses its clouds into the cloth too: smooth the old lower clouds' relief out (folds are far wider, the hem's edge
# stays put), fading out above the repainted band.
# (welded first: glTF splits the vertices along every UV seam, and with hundreds of islands nearly all would be a pinned border)
weld = bmesh.new()
weld.from_mesh(cloak.data)
bmesh.ops.remove_doubles(weld, verts=weld.verts, dist=1e-5)
border = {v.index for v in weld.verts if v.is_boundary}  # (the hem and the open front's edges stay put)
weld.to_mesh(cloak.data)
weld.free()
heights = [v.co.z for v in cloak.data.vertices]
low, high = min(heights), max(heights)
relief = cloak.vertex_groups.new(name="relief")
for vertex in cloak.data.vertices:
    share = (vertex.co.z - low) / (high - low)
    if abs(math.degrees(math.atan2(vertex.co.x, -vertex.co.y))) > FRONT_GAP:
        top = BODY_TOP if abs(vertex.co.x) < BODY_HALF else LOWER_TOP
        weight = min(max((top + SMOOTH_FADE - share) / SMOOTH_FADE, 0.0), 1.0)
        if weight > 0 and vertex.index not in border:
            relief.add([vertex.index], weight, "REPLACE")
smooth = cloak.modifiers.new("relief", "SMOOTH")
smooth.iterations, smooth.factor, smooth.vertex_group = SMOOTH_PASSES, 0.8, relief.name
bpy.ops.object.modifier_apply(modifier=smooth.name)
cloak.vertex_groups.remove(cloak.vertex_groups["relief"])  # (the apply leaves the old handle stale)

# Bake each texel's position and normal (emission, so one sample is exact).
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 1
scene.cycles.device = "CPU"
scene.render.bake.margin = 4
geometry, emission = tree.nodes.new("ShaderNodeNewGeometry"), tree.nodes.new("ShaderNodeEmission")
output = next(n for n in tree.nodes if n.type == "OUTPUT_MATERIAL")
surface = output.inputs[0].links[0].from_socket if output.inputs[0].links else None
tree.links.new(emission.outputs[0], output.inputs[0])
target = tree.nodes.new("ShaderNodeTexImage")
maps = {}
for socket in ("Position", "Normal"):
    baked = bpy.data.images.new(socket, W, H, alpha=True, float_buffer=True)
    baked.colorspace_settings.name = "Non-Color"
    target.image = baked
    tree.nodes.active = target
    tree.links.new(geometry.outputs[socket], emission.inputs[0])
    bpy.ops.object.bake(type="EMIT")
    maps[socket] = numpy.array(baked.pixels[:], dtype=numpy.float32).reshape(H, W, 4)[..., :3]
    bpy.data.images.remove(baked)
for node in (geometry, emission, target):
    tree.nodes.remove(node)
if surface is not None:
    tree.links.new(surface, output.inputs[0])
position, normal = maps["Position"], maps["Normal"]
used = numpy.abs(position).sum(-1) > 1e-6

colour = numpy.array(image.pixels[:], dtype=numpy.float32).reshape(H, W, 4)
low, high = position[used][:, 2].min(), position[used][:, 2].max()
height = high - low
x, y, z = position[..., 0], position[..., 1], (position[..., 2] - low) / height
azimuth = numpy.degrees(numpy.arctan2(x, -y))  # 0 = front centre
red, green, blue = colour[..., 0], colour[..., 1], colour[..., 2]
fabric = used & (numpy.maximum(numpy.maximum(red, green), blue) < 0.2)
cloud_red = used & (red > 0.25) & (red > 2.2 * green) & (red > 2.2 * blue)
cloud_white = used & (numpy.minimum(numpy.minimum(red, green), blue) > 0.45)
if not fabric.any() or not cloud_red.any() or not cloud_white.any():
    fail("couldn't find the black fabric, the cloud red and the outline white in the texture")
black, red_tone, white_tone = (numpy.median(colour[m][:, :3], 0) for m in (fabric, cloud_red, cloud_white))
log(f"fabric {black.round(3)}, cloud red {red_tone.round(3)}, outline {white_tone.round(3)}")

# Paint the lower cloth black (two texels of growth catch the clouds' antialiased rims). The lining too: normals can't tell it
# from the shell (folds turn them inward), and it only shows down there through the open front, which is left alone.
region = used & ((z < LOWER_TOP) | (z < BODY_TOP) & (numpy.abs(x) < BODY_HALF)) & (numpy.abs(azimuth) > FRONT_GAP)
paint = region & ~fabric
for _ in range(2):
    grown = paint.copy()
    grown[1:] |= paint[:-1]
    grown[:-1] |= paint[1:]
    grown[:, 1:] |= paint[:, :-1]
    grown[:, :-1] |= paint[:, 1:]
    paint = grown & region
colour[paint, :3] = black
log(f"painted {paint.sum()} texels black")

# The clean cloud: the reference's upper back cloud, its red and white recoloured to the texture's own.
reference = bpy.data.images.load(ref_path)
rw, rh = reference.size
pixels = numpy.array(reference.pixels[:], dtype=numpy.float32).reshape(rh, rw, 4)[::-1]  # top row first
left, top, right, bottom = REF_CROP
crop = pixels[int(top * rh):int(bottom * rh), int(left * rw):int(right * rw), :3]
lum = crop.mean(-1)
is_white = lum > 0.6
is_red = (crop[..., 0] > 0.3) & (crop[..., 0] > 1.8 * crop[..., 1]) & ~is_white
rows, cols = numpy.nonzero(is_white | is_red)
crop, is_white, is_red = (a[rows.min():rows.max() + 1, cols.min():cols.max() + 1] for a in (crop, is_white, is_red))
decal = numpy.where(is_white[..., None], white_tone, red_tone)
alpha = (is_white | is_red).astype(numpy.float32)
dh, dw = alpha.shape
cloud_h = CLOUD_W * dh / dw
log(f"cloud {dw}x{dh} px from the reference, stamped {CLOUD_W:.2f} x {cloud_h:.2f} m")

stamped = numpy.zeros((H, W), dtype=bool)
for facing in (-1.0, 1.0):  # front (-Y), back (+Y)
    for side in (-1.0, 1.0):
        centre_x, centre_z = side * CLOUD_X[facing], low + CLOUD_Z * height
        mask = region & (normal[..., 1] * facing > FACING[0]) & (y * facing > 0)  # (the front's lining faces the back)
        u = (x[mask] - centre_x) * -facing / CLOUD_W + 0.5  # as seen from that side (from the back, its right is -x)
        v = (centre_z + cloud_h / 2 - position[mask][:, 2]) / cloud_h
        inside = (u >= 0) & (u < 1) & (v >= 0) & (v < 1)
        cols = numpy.clip((u[inside] * dw).astype(int), 0, dw - 1)
        rows = numpy.clip((v[inside] * dh).astype(int), 0, dh - 1)
        texels = numpy.argwhere(mask)[inside]
        turn = normal[texels[:, 0], texels[:, 1], 1] * facing
        a = alpha[rows, cols] * numpy.clip((turn - FACING[0]) / (FACING[1] - FACING[0]), 0.0, 1.0)
        tr, tc = texels[:, 0], texels[:, 1]
        colour[tr, tc, :3] = colour[tr, tc, :3] * (1 - a[:, None]) + decal[rows, cols] * a[:, None]
        stamped[tr[a > 0], tc[a > 0]] = True
log(f"stamped 4 clouds over {stamped.sum()} texels")

image.pixels[:] = colour.ravel()
image.update()
image.pack()
bpy.ops.export_scene.gltf(filepath=dst, use_selection=True, export_format="GLB")
log(f"wrote {dst}")
