"""Dump the head mesh of a MetaHuman face FBX: vertices (metres), triangles, per-corner UVs. -- <face.fbx> <out.npz>"""
import sys

import bpy
import numpy as np

fbx, out = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in meshes:
    print("MESH", o.name, len(o.data.vertices), [m.name for m in o.data.materials])
head = max(meshes, key=lambda o: len(o.data.vertices))
dg = bpy.context.evaluated_depsgraph_get()
ev = head.evaluated_get(dg)
me = ev.to_mesh()
me.calc_loop_triangles()
mw = head.matrix_world
V = np.array([mw @ v.co for v in me.vertices])
mats = [m.name for m in me.materials]
tris, uvs, tmat = [], [], []
uvl = me.uv_layers[0].data
for t in me.loop_triangles:
    tris.append(t.vertices[:])
    uvs.append([uvl[l].uv[:] for l in t.loops])
    tmat.append(t.material_index)
np.savez(out, V=V, F=np.array(tris), UV=np.array(uvs), M=np.array(tmat), mats=np.array(mats))
print("DUMP", head.name, V.shape, len(tris), mats, "uv layers", [l.name for l in me.uv_layers])
