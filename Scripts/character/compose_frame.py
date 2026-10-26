"""Blender (5.2) headless: one fitting body for a City Sample crowd frame, for the outfit pipeline (build_outfits.ps1).

  blender -b --factory-startup -P Scripts/character/compose_frame.py -- <out.fbx> <hands.fbx> <face.fbx> <piece.fbx>...

A crowd frame has no whole body: its *_body mesh is the hands, and the skin of the rest lives inside the clothing pieces
(a top, trousers, shoes) and the face (head and neck: any MetaHuman face, moved onto the frame's head joint). All share metahuman_base_skel, so the pieces join onto the hands'
rig: the face's facial joints folded onto the nearest body bone (graft_head.fold_facial_weights), and the frame wears its
base outfit, which is what a cloak goes over. Written unit-scale like every body (clean_rig.export_unit_scale_fbx).
"""
import os
import sys

import bmesh
import bpy
from mathutils import Matrix
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from clean_rig import export_unit_scale_fbx  # noqa: E402
from graft_head import fold_facial_weights, import_rig, merge_meshes  # noqa: E402


def bone_world(armature, name):
    return armature.matrix_world @ armature.data.bones[name].head_local


ENCLOSED_REACH = 0.05  # m: a vertex this close under another part's surface is inside it


def drop_enclosed(parts):
    """Deletes each part's vertices that lie under another part's surface (the jeans' waist under the top, the neck under a
    turtleneck): with layers left in, a cloak vertex between two of them finds the inner one nearest and counts as outside,
    so skin_outfit.py's lift never pushes it off the outer one."""
    trees = [BVHTree.FromPolygons([p.matrix_world @ v.co for v in p.data.vertices], [f.vertices[:] for f in p.data.polygons]) for p in parts]
    for i, part in enumerate(parts):
        inside = set()
        for vertex in part.data.vertices:
            co = part.matrix_world @ vertex.co
            for j, tree in enumerate(trees):
                nearest, normal, _, distance = tree.find_nearest(co, ENCLOSED_REACH) if j != i else (None,) * 4
                if nearest is not None and (co - nearest).dot(normal) < 0:
                    inside.add(vertex.index)
                    break
        mesh = bmesh.new()
        mesh.from_mesh(part.data)
        mesh.verts.ensure_lookup_table()
        bmesh.ops.delete(mesh, geom=[mesh.verts[k] for k in inside], context="VERTS")
        mesh.to_mesh(part.data)
        mesh.free()
        print(f"[compose_frame] {part.name}: {len(inside)} enclosed vertices dropped")


def main():
    cli = sys.argv[sys.argv.index("--") + 1:]
    out, hands_fbx, face_fbx, pieces = os.path.abspath(cli[0]), cli[1], cli[2], cli[3:]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rig, meshes = import_rig(os.path.abspath(hands_fbx), "Body")
    body_bones = {b.name for b in rig.data.bones}
    parts = [merge_meshes(meshes)]
    face_rig, face_meshes = import_rig(os.path.abspath(face_fbx), "Face")
    face = merge_meshes(face_meshes)
    # a face from another MetaHuman: moved so its head joint is the frame's
    face.data.transform(Matrix.Translation(bone_world(rig, "head") - bone_world(face_rig, "head")))
    fold_facial_weights(face, face_rig, body_bones)
    bpy.data.objects.remove(face_rig, do_unlink=True)
    parts.append(face)
    for piece in pieces:
        piece_rig, piece_meshes = import_rig(os.path.abspath(piece), "Piece")
        parts.append(merge_meshes(piece_meshes))
        bpy.data.objects.remove(piece_rig, do_unlink=True)
    for part in parts:
        for group in [g for g in part.vertex_groups if g.name not in body_bones]:
            part.vertex_groups.remove(group)
    drop_enclosed(parts)
    body = merge_meshes(parts)  # (joins into the hands mesh: its world transform is already baked)
    body.parent = rig
    body.matrix_parent_inverse = rig.matrix_world.inverted()
    body.modifiers.new("Armature", "ARMATURE").object = rig
    body.name = os.path.splitext(os.path.basename(out))[0].replace("_rigged", "")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    export_unit_scale_fbx(out, [rig, body])
    print(f"[compose_frame] {len(body.data.vertices)} vertices -> {out}")


if __name__ == "__main__":
    main()
