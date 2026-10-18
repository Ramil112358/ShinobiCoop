"""Moves one building inside the city chunk meshes: the triangles of chunk B_<cx>_<cy> (and of its collision mesh
City/BCollision/B_<cx>_<cy>_Col) that lie wholly inside BOX are translated by OFFSET. The chunks are shared by Lvl_London
and the arenas, so both maps change. UE Python commandlet (editor closed):
  Scripts/run_python.ps1 -TimeoutSec 1800 Scripts/london/move_building.py

2026-10-18: the 2.8 x 5.1 m, 14 m Portland-stone tower that stood in front of the Piccadilly screen (build_london.screen_wrap)
goes to the free plaza north of Eros (found by a search for paving 3 m clear of any building, inside the WestEnd arena
and off the line from Eros to the screen); the paving of G_-6_-2 gets the hole it stood in closed. Each step skips
itself once done.
"""
import unreal

ROOT = "/Game/Shinobi/London/City"
CHUNK = "B_-6_-2"
BOX = ((-71503, -22457, -100), (-71206, -21924, 2000))  # cm, the tower's bounds + 10 cm
OFFSET = (-2946, 3791, 0)  # cm
HOLE = ((-71463, -22417), (-71246, -21964))  # cm, its cut-out in the paving of G_-6_-2
GS, SEL, XF = unreal.GeometryScript_AssetUtils, unreal.GeometryScript_MeshSelection, unreal.GeometryScript_MeshTransforms


def log(message):
    unreal.log(f"[move_building] {message}")


def first(result, cls):
    return next(v for v in result if isinstance(v, cls)) if isinstance(result, tuple) else result


def move(path):
    mesh = unreal.load_asset(path)
    dyn = read(mesh)
    box = unreal.Box(unreal.Vector(*BOX[0]), unreal.Vector(*BOX[1]))
    sel = first(SEL.select_mesh_elements_in_box(dyn, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, 3),
                unreal.GeometryScriptMeshSelection)
    info = SEL.get_mesh_selection_info(sel)
    count = next(v for v in info if isinstance(v, int)) if isinstance(info, tuple) else info
    if not count:
        log(f"{path}: nothing inside {BOX}, already moved")
        return
    XF.translate_mesh_selection(dyn, sel, unreal.Vector(*OFFSET))
    GS.copy_mesh_to_static_mesh(dyn, mesh, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD())
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log(f"{path}: moved {count} triangles by {OFFSET} cm")


def read(mesh):
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    return first(GS.copy_mesh_from_static_mesh_v2(mesh, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), lod),
                 unreal.DynamicMesh)


def fill(path, hole):
    """The ground chunk's paving has a hole the building stood in, rimmed by a 15 cm kerb: drop the rim and pave it over
    with two triangles that take their material, UVs (an affine map of x, y), colour and normal from the paving beside."""
    mesh = unreal.load_asset(path)
    dyn = read(mesh)
    (x0, y0), (x1, y1) = hole
    box = unreal.Box(unreal.Vector(x0 - 20, y0 - 20, -5), unreal.Vector(x1 + 20, y1 + 20, 20))
    rim = first(SEL.select_mesh_elements_in_box(dyn, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, 3),
                unreal.GeometryScriptMeshSelection)
    info = SEL.get_mesh_selection_info(rim)
    if not (next(v for v in info if isinstance(v, int)) if isinstance(info, tuple) else info):
        log(f"{path}: no hole at {hole}, already paved")
        return
    unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(dyn, rim)
    q = unreal.GeometryScript_MeshQueries
    bvh = first(unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(dyn), unreal.GeometryScriptDynamicMeshBVH)
    near = first(unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(dyn, bvh, unreal.Vector(x0 - 100, (y0 + y1) / 2, 15),
                                                                              unreal.GeometryScriptSpatialQueryOptions()),
                 unreal.GeometryScriptTrianglePoint)
    t = near.triangle_id
    pos = [v for v in q.get_triangle_positions(dyn, t) if isinstance(v, unreal.Vector)]
    uvs = [v for v in q.get_triangle_u_vs(dyn, 0, t) if isinstance(v, unreal.Vector2D)]
    normal = next(v for v in q.get_triangle_normals(dyn, t) if isinstance(v, unreal.Vector))
    color = next(v for v in q.get_triangle_vertex_colors(dyn, t) if isinstance(v, unreal.LinearColor))
    material = first(unreal.GeometryScript_Materials.get_triangle_material_id(dyn, t), int)
    # uv = a * x + b * y + c, solved on the neighbour triangle
    (ax, ay), (bx, by), (cx, cy) = [(p.x, p.y) for p in pos]
    det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)

    def uv(x, y):
        s = ((x - ax) * (cy - ay) - (cx - ax) * (y - ay)) / det
        r = ((bx - ax) * (y - ay) - (x - ax) * (by - ay)) / det
        return unreal.Vector2D(uvs[0].x + (uvs[1].x - uvs[0].x) * s + (uvs[2].x - uvs[0].x) * r,
                               uvs[0].y + (uvs[1].y - uvs[0].y) * s + (uvs[2].y - uvs[0].y) * r)
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    ids = [first(unreal.GeometryScript_MeshEdits.add_vertex_to_mesh(dyn, unreal.Vector(x, y, pos[0].z)), int) for x, y in corners]
    up = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay) > 0  # the neighbour's winding: keep the face up the same way
    for tri in ((0, 1, 2), (0, 2, 3)):
        a, b, c = tri if up else tri[::-1]
        new = first(unreal.GeometryScript_MeshEdits.add_triangle_to_mesh(dyn, unreal.IntVector(ids[a], ids[b], ids[c]), 0), int)
        unreal.GeometryScript_Materials.set_triangle_material_id(dyn, new, material)
        tri_uv = unreal.GeometryScriptUVTriangle()
        tri_uv.uv0, tri_uv.uv1, tri_uv.uv2 = (uv(*corners[k]) for k in (a, b, c))
        unreal.GeometryScript_UVs.set_mesh_triangle_u_vs(dyn, 0, new, tri_uv)
        n = unreal.GeometryScriptTriangle()
        n.vector0 = n.vector1 = n.vector2 = normal
        unreal.GeometryScript_Normals.set_mesh_triangle_normals(dyn, new, n)
    patch = first(SEL.select_mesh_elements_in_box(dyn, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, 3),
                  unreal.GeometryScriptMeshSelection)
    unreal.GeometryScript_VertexColors.set_mesh_selection_vertex_color(dyn, patch, color, unreal.GeometryScriptColorFlags(), True)
    GS.copy_mesh_to_static_mesh(dyn, mesh, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD())
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log(f"{path}: paved the {x1 - x0:.0f} x {y1 - y0:.0f} cm hole (material {material}, colour {color})")


move(f"{ROOT}/B/{CHUNK}/StaticMeshes/{CHUNK}")
move(f"{ROOT}/BCollision/{CHUNK}_Col")
fill(f"{ROOT}/G/G_-6_-2/StaticMeshes/G_-6_-2", HOLE)
