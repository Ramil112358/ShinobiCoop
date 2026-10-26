"""Blender (5.2) headless: a body's shape keys copied onto something worn over it (an outfit copy, a grafted head).

  blender -b --factory-startup -P Scripts/character/transfer_morphs.py -- <body.fbx> <worn.fbx> <out.fbx> [--reach 0.15]

Each worn vertex takes, for every body shape key, the key's offset at the nearest point on the body's basis surface
(barycentric over that face), fading out between --reach/2 and --reach metres from the body. The worn mesh otherwise goes
out as it came in (vertex colours, materials, weights) on its own rig, exported like every other copy
(clean_rig.export_unit_scale_fbx), so UE imports the keys as morph targets with the body's names: setting a body morph
on the worn mesh too keeps it on the body (UShinobiBodyComponent).
"""
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from clean_rig import export_unit_scale_fbx  # noqa: E402


def import_objects(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for armature in (o for o in new if o.type == "ARMATURE" and o.parent and o.parent.type == "EMPTY"):
        world = armature.matrix_world.copy()
        empty = armature.parent
        armature.parent = None
        armature.matrix_world = world
        bpy.data.objects.remove(empty, do_unlink=True)
        new.remove(empty)
    return new


def main():
    cli = sys.argv[sys.argv.index("--") + 1:]
    body_fbx, worn_fbx, out = (os.path.abspath(p) for p in cli[:3])
    opts = dict(zip(cli[3::2], cli[4::2]))
    reach = float(opts.get("--reach", 0.15))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    body = next(o for o in import_objects(body_fbx) if o.type == "MESH" and o.data.shape_keys)
    keys = body.data.shape_keys.key_blocks
    world = body.matrix_world
    basis = [world @ k.co for k in keys[0].data]
    shaped = {k.name: [world @ p.co for p in k.data] for k in keys[1:]}
    tree = BVHTree.FromPolygons(basis, [tuple(p.vertices) for p in body.data.polygons])
    tris = [tuple(p.vertices[:3]) for p in body.data.polygons]  # MetaHuman/Rodin bodies: quads and tris, near-planar
    worn_objects = import_objects(worn_fbx)
    for obj in worn_objects:
        if obj.type == "ARMATURE" or obj == body:
            continue
        if obj.type != "MESH":
            continue
        if not obj.data.shape_keys:
            obj.shape_key_add(name="Basis")
        inv = obj.matrix_world.inverted()
        points = [obj.matrix_world @ v.co for v in obj.data.vertices]
        hits = [tree.find_nearest(p) for p in points]
        for name, moved in shaped.items():
            key = obj.shape_key_add(name=name)
            for i, (p, (hit, _, face, dist)) in enumerate(zip(points, hits)):
                if hit is None or dist > reach:
                    continue
                a, b, c = tris[face]
                offset = barycentric_transform(hit, basis[a], basis[b], basis[c], moved[a], moved[b], moved[c]) - hit
                fade = min(1.0, max(0.0, (reach - dist) / (reach / 2)))
                key.data[i].co = inv @ (p + offset * fade)
        print(f"[transfer_morphs] {obj.name}: {len(shaped)} keys {list(shaped)}")
    bpy.data.objects.remove(body, do_unlink=True)
    rig_objects = [o for o in worn_objects if o.name in bpy.data.objects]
    export_unit_scale_fbx(out, rig_objects)
    print(f"[transfer_morphs] wrote {out}")


if __name__ == "__main__":
    main()
