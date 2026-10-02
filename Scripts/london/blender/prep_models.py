"""Blender (5.2) headless: normalises the downloaded London models (fetch_models.py) for UE.

  blender -b -P Scripts/london/blender/prep_models.py -- <in_dir> <out_dir> [key ...]

Per <key>.glb: joins every mesh (materials and textures kept), applies transforms, scales it to its real size (SIZE: the
named dimension in metres), turns vehicles and benches so their length runs along +X, puts the pivot at the bottom
centre, and writes <out_dir>/<key>.glb. Sketchfab exports come in any unit and up-axis; the importer already converts
glTF's +Y-up to Blender's +Z-up. Deterministic.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

# key -> (dimension, metres, turn long axis to +X)
SIZE = {
    "bus": ("z", 4.4, True), "black_cab": ("x", 4.55, True), "transit_van": ("x", 5.3, True),
    "sprinter_van": ("x", 5.9, True), "mini": ("x", 3.85, True), "prius": ("x", 4.48, True),
    "big_ben": ("z", 96.0, False), "london_eye": ("z", 135.0, False), "st_pauls": ("z", 111.0, False),
    "shard": ("z", 310.0, False), "gherkin": ("z", 180.0, False), "nelsons_column": ("z", 51.6, False),
    "narrowboat": ("x", 20.0, True), "traffic_light": ("z", 3.4, False), "camel_bench": ("x", 2.3, True),
    "cab_shelter": ("x", 7.0, True),
    # Hyper3D Rodin (Gen-2.5, text prompt): no free model existed
    "eros": ("z", 7.5, False), "white_tower": ("z", 36.0, False),
    "phone_box": ("z", 2.5, False),
}
# key -> extra turn (axis, degrees) applied after the long axis is laid along +X: the TX4 was modelled nose-up, and
# laying it down left it on its side (roof toward -Y).
EXTRA_TURN = {"black_cab": ("X", -90.0)}

# key -> {material name: base colour (linear)}; the texture link is dropped so the flat colour shows (London red bus,
# real-world branding painted over).
RECOLOUR = {}


def recolour(obj, key):
    for mat in obj.data.materials:
        colour = RECOLOUR.get(key, {}).get(mat.name)
        if colour is None or not mat.use_nodes:
            continue
        bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        for link in list(bsdf.inputs["Base Color"].links):
            mat.node_tree.links.remove(link)
        bsdf.inputs["Base Color"].default_value = (*colour, 1)


def bounds(objs):
    """From the vertices: bound_box is stale right after mesh.transform()."""
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def prep(src, dst, key):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        print(f"[prep_models] {key}: no meshes")
        return
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    for o in meshes:  # unparent keeping world transforms, then bake them
        o.parent = None if o.parent is None else o.parent
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    for o in [o for o in bpy.data.objects if o != obj]:
        bpy.data.objects.remove(o, do_unlink=True)
    axis, metres, along_x = SIZE[key]
    lo, hi = bounds([obj])
    size = hi - lo
    if along_x and size.z > max(size.x, size.y):  # modelled standing on its end
        obj.data.transform(Matrix.Rotation(-1.5707963, 4, "Y"))
        lo, hi = bounds([obj])
        size = hi - lo
    if key in EXTRA_TURN:
        axis_name, degrees = EXTRA_TURN[key]
        obj.data.transform(Matrix.Rotation(math.radians(degrees), 4, axis_name))
        lo, hi = bounds([obj])
        size = hi - lo
    if along_x and size.y > size.x:
        obj.data.transform(Matrix.Rotation(1.5707963, 4, "Z"))
        lo, hi = bounds([obj])
        size = hi - lo
    scale = metres / {"x": size.x, "y": size.y, "z": size.z}[axis]
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    obj.data.transform(Matrix.Scale(scale, 4) @ Matrix.Translation(-centre))
    obj.location = (0, 0, 0)
    obj.name = key
    recolour(obj, key)
    lo, hi = bounds([obj])
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print(f"[prep_models] {key}: {hi.x - lo.x:.2f} x {hi.y - lo.y:.2f} x {hi.z - lo.z:.2f} m, {tris} tris, "
          f"{len(obj.data.materials)} materials (scale x{scale:.4f})")
    bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", use_selection=False, export_apply=True)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    src_dir, out_dir, keys = args[0], args[1], args[2:] or list(SIZE)
    os.makedirs(out_dir, exist_ok=True)
    for key in keys:
        src = os.path.join(src_dir, f"{key}.glb")
        if os.path.exists(src):
            prep(src, os.path.join(out_dir, f"{key}.glb"), key)


if __name__ == "__main__":
    main()
