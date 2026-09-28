"""Blender headless: builds the open world's Japanese architecture and camp pieces procedurally (sub-project #3, W2).

  blender -b -P Scripts/world/blender/make_gap_pieces.py -- <out_dir>

One .glb per piece, in metres, with its front facing +X (the layout's yaw 0), feet at z = 0. Every material is a flat
palette colour named after its palette key (Plaster, WoodDark, RoofTile, ...), which Scripts/world/import_library.py
maps to the shared toon-ready material instances, so all pieces match each other and the Quaternius kits.
No CC0 kit covered these (the Quaternius village kit is European); these are original designs, not Konoha's.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

PALETTE = {
    "Plaster": (0.86, 0.80, 0.67), "WoodDark": (0.17, 0.10, 0.06), "WoodLight": (0.46, 0.31, 0.17),
    "RoofTile": (0.17, 0.23, 0.31), "RoofTileRed": (0.46, 0.13, 0.08), "Stone": (0.46, 0.45, 0.42),
    "Red": (0.62, 0.08, 0.05), "Paper": (0.96, 0.92, 0.82), "Cloth": (0.72, 0.63, 0.47),
    "ClothRed": (0.62, 0.11, 0.08), "Black": (0.05, 0.05, 0.05), "Rope": (0.66, 0.56, 0.36),
}
OUT = os.path.abspath(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else os.path.abspath("Generated")


# ---------------------------------------------------------------------------------------------------------------- basics
def material(key):
    mat = bpy.data.materials.get(key)
    if mat is None:
        mat = bpy.data.materials.new(key)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        bsdf.inputs["Base Color"].default_value = (*PALETTE[key], 1)
        bsdf.inputs["Roughness"].default_value = 0.85
    return mat


def finish(obj, key):
    obj.data.materials.append(material(key))
    for poly in obj.data.polygons:
        poly.use_smooth = False
    return obj


def box(size, loc, key, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    return finish(obj, key)


def cyl(radius, depth, loc, key, verts=12, rot=(0, 0, 0), radius2=None):
    if radius2 is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc, rotation=rot)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=radius, radius2=radius2, depth=depth, location=loc, rotation=rot)
    return finish(bpy.context.active_object, key)


def mesh_from(verts, faces, key, name="piece", thickness=0.0):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    if thickness:
        mod = obj.modifiers.new("Solidify", "SOLIDIFY")
        mod.thickness = thickness
        mod.offset = 1.0
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return finish(obj, key)


def hip_roof(w, d, h, z, key="RoofTile", overhang=0.9, flare=0.35, rows=7, cx=0.0, cy=0.0):
    """Japanese hip roof: eaves rectangle (w+2o by d+2o) rising along a t^1.6 profile (steep top, flat flared eaves) to
    a ridge line; the eave corners curl up by `flare`."""
    ew, ed = w / 2 + overhang, d / 2 + overhang
    ridge = max(0.0, (w - d) / 2)
    verts, faces = [], []
    for i in range(rows + 1):
        t = i / rows
        hx = ew + (ridge - ew) * t
        hy = ed * (1 - t) + 0.001
        zz = z + h * t ** 1.6
        curl = flare * (1 - t) ** 2
        for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
            verts.append((cx + sx * hx, cy + sy * hy, zz + curl))
    for i in range(rows):
        a, b = i * 4, (i + 1) * 4
        for k in range(4):
            faces.append((a + k, a + (k + 1) % 4, b + (k + 1) % 4, b + k))
    obj = mesh_from(verts, faces, key, "roof", thickness=0.18)
    # ridge cap
    if ridge > 0.01:
        box((2 * ridge + 0.6, 0.35, 0.3), (cx, cy, z + h + 0.1), "Black")
    else:
        cyl(0.22, 0.5, (cx, cy, z + h + 0.15), "Black", verts=8)
    return obj


def gable_cap(length, width, z, key="RoofTile"):
    """Small two-sided tile cap along X (wall tops)."""
    verts = [(-length / 2, -width / 2, z), (length / 2, -width / 2, z), (length / 2, 0, z + width * 0.45),
             (-length / 2, 0, z + width * 0.45), (-length / 2, width / 2, z), (length / 2, width / 2, z)]
    faces = [(0, 1, 2, 3), (3, 2, 5, 4)]
    return mesh_from(verts, faces, key, "cap", thickness=0.1)


def export(name):
    os.makedirs(OUT, exist_ok=True)
    objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    joined = bpy.context.active_object
    joined.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    path = os.path.join(OUT, f"{name}.glb")
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True)
    print(f"[gap_pieces] {name}: {len(joined.data.polygons)} faces -> {path}")
    bpy.ops.wm.read_factory_settings(use_empty=True)


# ------------------------------------------------------------------------------------------------------------ buildings
def house(w, d, storeys, roof_key="RoofTile", name="house"):
    """Plaster walls framed by dark posts and beams on a stone plinth, sliding door and paper windows on the +X front,
    hip roof; a second storey sits set back behind a skirt roof."""
    wall_h = 3.0
    box((w + 0.6, d + 0.6, 0.45), (0, 0, 0.225), "Stone")
    z0 = 0.45
    for storey in range(storeys):
        sw, sd = (w, d) if storey == 0 else (w * 0.72, d * 0.72)
        box((sw, sd, wall_h), (0, 0, z0 + wall_h / 2), "Plaster")
        for sx in (-1, 1):
            for sy in (-1, 1):
                box((0.28, 0.28, wall_h), (sx * sw / 2, sy * sd / 2, z0 + wall_h / 2), "WoodDark")
        for zz in (z0 + 0.12, z0 + wall_h - 0.15):
            box((sw + 0.1, sd + 0.1, 0.22), (0, 0, zz), "WoodDark")
        if storey == 0:
            box((0.12, 1.6, 2.2), (sw / 2 + 0.04, 0, z0 + 1.1), "WoodDark")          # door frame
            box((0.1, 1.4, 2.0), (sw / 2 + 0.06, 0.0, z0 + 1.05), "Paper")          # sliding door
            for y in (-0.35, 0.35):
                box((0.12, 0.06, 2.0), (sw / 2 + 0.1, y, z0 + 1.05), "WoodDark")
        for side in (-1, 1):                                                      # side windows
            box((min(1.6, sw * 0.4), 0.1, 0.9), (0, side * (sd / 2 + 0.05), z0 + 1.8), "Paper")
            box((0.08, 0.14, 0.9), (0, side * (sd / 2 + 0.07), z0 + 1.8), "WoodDark")
        top = z0 + wall_h
        if storey < storeys - 1:
            hip_roof(sw, sd, 0.9, top, roof_key, overhang=0.8, flare=0.25, rows=4)   # skirt roof
            z0 = top + 0.4
        else:
            hip_roof(sw, sd, min(sw, sd) * 0.42 + 0.8, top, roof_key)
    export(name)


def house_round(name="house_round"):
    """Round plaster house with a conical tiled roof, a nod to the genre without copying any village."""
    r, wall_h = 4.2, 3.2
    cyl(r + 0.4, 0.45, (0, 0, 0.225), "Stone", verts=24)
    cyl(r, wall_h, (0, 0, 0.45 + wall_h / 2), "Plaster", verts=24)
    for zz in (0.6, 0.45 + wall_h - 0.2):
        cyl(r + 0.08, 0.2, (0, 0, zz), "WoodDark", verts=24)
    box((0.14, 1.5, 2.1), (r - 0.02, 0, 1.5), "WoodDark")
    box((0.1, 1.3, 1.9), (r + 0.03, 0, 1.45), "Paper")
    for a in range(0, 360, 60):
        if a != 0:
            x, y = r * math.cos(math.radians(a)), r * math.sin(math.radians(a))
            box((0.1, 1.1, 0.8), (x, y, 2.2), "Paper", rot=(0, 0, math.radians(a)))
    cyl(r + 1.1, 2.6, (0, 0, 0.45 + wall_h + 1.3), "RoofTileRed", verts=24, radius2=0.35)
    cyl(0.3, 0.8, (0, 0, 0.45 + wall_h + 2.9), "Black", verts=8)
    export(name)


def shrine(name="shrine"):
    box((7, 6, 0.8), (0, 0, 0.4), "Stone")
    for i in range(3):
        box((0.5, 2.4, 0.27), (3.75 + i * 0.5, 0, 0.13 + i * 0.27 * 0 + 0.135 * (2 - i)), "Stone")
    box((5, 4, 3.2), (0, 0, 0.8 + 1.6), "Plaster")
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(0.22, 3.6, (sx * 2.7, sy * 2.2, 0.8 + 1.8), "Red", verts=10)
    box((5.6, 4.6, 0.25), (0, 0, 0.8 + 3.3), "Red")
    box((0.1, 2.4, 2.3), (2.53, 0, 0.8 + 1.15), "WoodDark")
    box((0.12, 0.9, 0.5), (2.6, 0, 0.8 + 2.8), "Black")                           # plaque
    hip_roof(5.4, 4.4, 2.6, 0.8 + 3.45, "RoofTile", overhang=1.2, flare=0.5)
    export(name)


def torii(name="torii"):
    for y in (-2.3, 2.3):
        cyl(0.3, 5.2, (0, y, 2.6), "Red", verts=14)
        cyl(0.42, 0.5, (0, y, 0.25), "Black", verts=14)
    box((0.45, 5.8, 0.35), (0, 0, 3.9), "Red")                                     # nuki
    box((0.25, 0.45, 0.8), (0, 0, 4.45), "Black")                                  # plaque
    # kasagi: a gently curved top beam, ends lifting
    verts, faces, n = [], [], 12
    for i in range(n + 1):
        y = -3.8 + 7.6 * i / n
        lift = 0.35 * (abs(y) / 3.8) ** 2
        for x, z in ((-0.35, 0), (0.35, 0), (0.35, 0.45), (-0.35, 0.45)):
            verts.append((x, y, 5.15 + z + lift))
    for i in range(n):
        a, b = i * 4, (i + 1) * 4
        for k in range(4):
            faces.append((a + k, a + (k + 1) % 4, b + (k + 1) % 4, b + k))
    faces += [(0, 3, 2, 1), (n * 4, n * 4 + 1, n * 4 + 2, n * 4 + 3)]
    mesh_from(verts, faces, "Black", "kasagi")
    box((0.5, 6.6, 0.25), (0, 0, 5.05), "Red")                                     # shimaki
    export(name)


def gate(name="gate"):
    """Main village gate: two tall posts, a roofed lintel, heavy doors swung open inward (-X)."""
    span = 10.0
    for y in (-span / 2, span / 2):
        box((1.0, 1.0, 9.0), (0, y, 4.5), "WoodDark")
        box((1.3, 1.3, 0.6), (0, y, 0.3), "Stone")
    box((1.2, span + 1.6, 1.0), (0, 0, 8.3), "WoodDark")
    box((1.25, span + 1.2, 0.35), (0, 0, 7.0), "Red")
    hip_roof(1.4, span + 2.0, 1.4, 8.8, "RoofTile", overhang=0.8, flare=0.35, rows=5)
    for side in (-1, 1):
        box((span / 2 - 0.2, 0.3, 6.8), (-(span / 4), side * (span / 2 - 0.4), 3.5), "WoodLight",
            rot=(0, 0, math.radians(side * 80) * 0))
    export(name)


def wall_segment(name="wall_segment"):
    box((10.2, 0.9, 0.5), (0, 0, 0.25), "Stone")
    box((10.0, 0.7, 3.0), (0, 0, 2.0), "Plaster")
    for x in (-5, 0, 5):
        box((0.3, 0.8, 3.0), (x, 0, 2.0), "WoodDark")
    gable_cap(10.6, 1.5, 3.5)
    export(name)


def ruin_wall(name="ruin_wall"):
    """A broken stone wall, 8 m: a plinth, block courses whose tops step down unevenly, two fallen blocks."""
    box((8.4, 1.3, 0.5), (0, 0, 0.25), "Stone")
    heights = (3.6, 3.1, 2.4, 2.8, 1.6, 1.1, 1.9, 0.8)
    for i, h in enumerate(heights):
        box((0.98, 1.0, h), (-3.5 + i, 0, 0.5 + h / 2), "Stone")
    box((1.0, 0.9, 0.7), (1.2, 1.5, 0.35), "Stone", rot=(0, 0, math.radians(20)))
    box((0.9, 0.8, 0.6), (3.4, -1.3, 0.3), "Stone", rot=(math.radians(8), 0, math.radians(-35)))
    export(name)


def ruin_pillar(name="ruin_pillar"):
    """A weathered stone pillar, 6 m, on a stepped base, its top block knocked askew."""
    box((2.0, 2.0, 0.4), (0, 0, 0.2), "Stone")
    box((1.6, 1.6, 0.4), (0, 0, 0.6), "Stone")
    box((1.1, 1.1, 4.6), (0, 0, 3.1), "Stone")
    box((1.5, 1.5, 0.6), (0.1, 0.05, 5.75), "Stone", rot=(math.radians(6), math.radians(-4), math.radians(12)))
    export(name)


def stall(name="stall"):
    box((3.0, 1.6, 1.0), (0, 0, 0.5), "WoodLight")
    box((3.1, 1.7, 0.1), (0, 0, 1.05), "WoodDark")
    for x in (-1.4, 1.4):
        for y in (-0.7, 0.7):
            box((0.12, 0.12, 2.6), (x, y, 1.3), "WoodDark")
    verts = [(-1.8, 1.2, 2.2), (1.8, 1.2, 2.2), (1.8, -1.1, 2.9), (-1.8, -1.1, 2.9)]
    mesh_from(verts, [(0, 1, 2, 3)], "ClothRed", "awning", thickness=0.06)
    for x in (-1.0, 0.0, 1.0):
        box((0.5, 0.35, 0.3), (x, 0, 1.25), "Cloth")
    export(name)


def stone_lantern(name="stone_lantern"):
    box((0.8, 0.8, 0.25), (0, 0, 0.125), "Stone")
    cyl(0.16, 0.9, (0, 0, 0.7), "Stone", verts=8)
    box((0.6, 0.6, 0.12), (0, 0, 1.2), "Stone")
    box((0.45, 0.45, 0.45), (0, 0, 1.48), "Stone")
    box((0.32, 0.47, 0.25), (0, 0, 1.5), "Paper")
    cyl(0.62, 0.45, (0, 0, 1.93), "Stone", verts=4, radius2=0.05, rot=(0, 0, math.radians(45)))
    cyl(0.07, 0.2, (0, 0, 2.2), "Stone", verts=6)
    export(name)


def paper_lantern(name="lantern"):
    """Street lantern: a wooden post with an arm and a hanging red paper lantern."""
    box((0.18, 0.18, 3.2), (0, 0, 1.6), "WoodDark")
    box((0.9, 0.12, 0.12), (0.4, 0, 3.05), "WoodDark")
    cyl(0.28, 0.55, (0.75, 0, 2.55), "ClothRed", verts=12)
    for z in (2.27, 2.83):
        cyl(0.22, 0.06, (0.75, 0, z), "Black", verts=12)
    cyl(0.02, 0.25, (0.75, 0, 2.95), "Black", verts=4)
    export(name)


def well(name="well"):
    cyl(1.1, 0.9, (0, 0, 0.45), "Stone", verts=16)
    cyl(0.85, 0.95, (0, 0, 0.5), "Black", verts=16)
    for y in (-1.0, 1.0):
        box((0.18, 0.18, 2.6), (0, y, 1.3), "WoodDark")
    cyl(0.08, 2.1, (0, 0, 2.1), "WoodLight", verts=8, rot=(math.radians(90), 0, 0))
    verts = [(-1.0, -1.4, 2.5), (1.0, -1.4, 2.5), (1.0, 0, 3.1), (-1.0, 0, 3.1), (-1.0, 1.4, 2.5), (1.0, 1.4, 2.5)]
    mesh_from([(y, x, z) for x, y, z in verts], [(0, 1, 2, 3), (3, 2, 5, 4)], "RoofTile", "wellroof", thickness=0.08)
    export(name)


def training_post(name="training_post"):
    cyl(0.18, 1.8, (0, 0, 0.9), "WoodLight", verts=10)
    for z in (0.6, 1.1, 1.5):
        cyl(0.2, 0.12, (0, 0, z), "Rope", verts=10)
    box((0.12, 0.9, 0.12), (0, 0, 1.3), "WoodLight")
    export(name)


def tent(name="tent"):
    verts = [(-1.6, -1.7, 0), (1.6, -1.7, 0), (1.6, 0, 2.2), (-1.6, 0, 2.2), (-1.6, 1.7, 0), (1.6, 1.7, 0)]
    mesh_from(verts, [(0, 1, 2, 3), (3, 2, 5, 4)], "Cloth", "tent", thickness=0.05)
    mesh_from([(-1.6, -1.7, 0), (-1.6, 1.7, 0), (-1.6, 0, 2.2)], [(0, 1, 2)], "Cloth", "back", thickness=0.05)
    for x in (-1.7, 1.7):
        box((0.1, 0.1, 2.4), (x, 0, 1.2), "WoodDark")
    box((3.6, 0.08, 0.08), (0, 0, 2.3), "WoodDark")
    export(name)


def palisade(name="palisade"):
    for i in range(7):
        y = -1.5 + i * 0.5
        h = 3.0 + 0.25 * math.sin(i * 1.7)
        cyl(0.24, h, (0, y, h / 2), "WoodLight", verts=8)
        cyl(0.24, 0.5, (0, y, h + 0.25), "WoodLight", verts=8, radius2=0.02)
    for z in (0.9, 2.1):
        box((0.12, 3.6, 0.18), (0.26, 0, z), "Rope")
    export(name)


def campfire(name="campfire"):
    for i in range(10):
        a = 2 * math.pi * i / 10
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.22, location=(0.75 * math.cos(a), 0.75 * math.sin(a), 0.12))
        finish(bpy.context.active_object, "Stone")
    for a in (0, 60, 120):
        cyl(0.09, 1.2, (0, 0, 0.35), "WoodDark", verts=6, rot=(math.radians(62), 0, math.radians(a)))
    export(name)


def bridge(name="bridge"):
    """Arched taiko bridge along X, 36 m long, deck 3.5 m wide, rising 2 m at the middle, red rails."""
    length, width, rise, n = 36.0, 3.5, 2.0, 24
    verts, faces = [], []
    for i in range(n + 1):
        x = -length / 2 + length * i / n
        z = rise * (1 - (2 * x / length) ** 2)
        for y, dz in ((-width / 2, 0), (width / 2, 0), (width / 2, -0.35), (-width / 2, -0.35)):
            verts.append((x, y, z + dz))
    for i in range(n):
        a, b = i * 4, (i + 1) * 4
        for k in range(4):
            faces.append((a + k, a + (k + 1) % 4, b + (k + 1) % 4, b + k))
    mesh_from(verts, faces, "WoodLight", "deck")
    for i in range(0, n + 1, 3):
        x = -length / 2 + length * i / n
        z = rise * (1 - (2 * x / length) ** 2)
        for y in (-width / 2 + 0.1, width / 2 - 0.1):
            box((0.16, 0.16, 1.0), (x, y, z + 0.5), "Red")
        if 3 <= i <= n - 3:
            for y in (-1.2, 1.2):
                cyl(0.25, z + 4.0, (x, y, z - (z + 4.0) / 2), "WoodDark", verts=8)  # piers down into the river bed
    for y in (-width / 2 + 0.1, width / 2 - 0.1):
        verts, faces = [], []
        for i in range(n + 1):
            x = -length / 2 + length * i / n
            z = rise * (1 - (2 * x / length) ** 2) + 1.0
            verts += [(x, y - 0.08, z), (x, y + 0.08, z), (x, y + 0.08, z + 0.14), (x, y - 0.08, z + 0.14)]
        for i in range(n):
            a, b = i * 4, (i + 1) * 4
            for k in range(4):
                faces.append((a + k, a + (k + 1) % 4, b + (k + 1) % 4, b + k))
        mesh_from(verts, faces, "Red", "rail")
    export(name)


if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    house(6, 5, 1, name="house_small")
    house(8, 6, 1, "RoofTileRed", name="house_medium")
    house(10, 8, 2, name="house_large")
    house_round()
    shrine()
    torii()
    gate()
    wall_segment()
    ruin_wall()
    ruin_pillar()
    stall()
    stone_lantern()
    paper_lantern()
    well()
    training_post()
    tent()
    palisade()
    campfire()
    bridge()
