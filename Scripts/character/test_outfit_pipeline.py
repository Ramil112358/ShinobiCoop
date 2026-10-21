"""Blender (5.2) checks for the outfit pipeline helpers (docs/character/outfit-steps.md). Run from the repo root:

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b --factory-startup -P Scripts/character/test_outfit_pipeline.py [-- <test name filter>]

Prints PASS, or fails with the first broken assertion (exit 1).
"""
import os
import sys
import traceback

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

MODELS = os.path.join("SourceArt", "Character", "Models")


def body(name):
    return os.path.join(MODELS, name, f"{name}_rigged.fbx")


def test_landmarks():
    from body_landmarks import measure
    hero = measure(body("Hero"))
    assert hero["tail"] is None, hero["tail"]
    assert 0.18 < hero["chest"][1] < 0.28, hero["chest"]  # Hero's chest is ~0.23 m deep
    cat = measure(body("Cat"))
    assert cat["tail"] is not None, "the Cat has a tail"
    assert cat["tail"]["behind"] > 0.05 and cat["tail"]["tip"][2] < cat["hips_z"], cat["tail"]
    misha = measure(body("Misha"))
    assert misha["tail"] is not None, "Misha has a tail"
    assert measure(body("Pabu"))["tail"] is None, "Pabu's trouser folds are not a tail"
    bika = measure(body("Bika"))
    assert bika["tail"] is None
    assert bika["chest"][1] > hero["chest"][1] + 0.08, (bika["chest"], hero["chest"])  # ~0.35 vs ~0.23


OUTFITS = os.path.join("SourceArt", "Character", "Outfits")
_fitted = {}


def fit(outfit, name):
    """Runs skin_outfit.py for (outfit, body) once into a temp dir; returns the FBX path."""
    import subprocess
    import tempfile
    key = (outfit, name)
    if key not in _fitted:
        out = tempfile.mkdtemp(prefix=f"fit_{outfit}_{name}_")
        master = os.path.join(OUTFITS, outfit, "Akatsuki_clouds.glb" if outfit == "Akatsuki" else f"{outfit}_tripo.glb")
        run = subprocess.run([bpy.app.binary_path, "-b", "--factory-startup", "--python-exit-code", "1", "-P", os.path.join("Scripts", "character", "skin_outfit.py"),
                              "--", body(name), master, out, outfit, name], capture_output=True, text=True)
        assert run.returncode == 0, run.stdout[-3000:]
        _fitted[key] = os.path.join(out, f"{outfit}_{name}_outfit.fbx")
        assert os.path.exists(_fitted[key]), os.listdir(out)
    return _fitted[key]


def load_fitted(fbx):
    """(armature, mesh, per-vertex (R, G) shares, set of leg group indices) of a fitted copy."""
    import numpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    armature = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    mesh = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    colours = mesh.data.color_attributes.active_color
    assert colours is not None, "the copy has vertex colours"
    shares = numpy.zeros((len(mesh.data.vertices), 2))
    for loop, value in zip(mesh.data.loops, colours.data) if colours.domain == "CORNER" else enumerate(colours.data):
        index = loop.vertex_index if colours.domain == "CORNER" else loop
        shares[index] = value.color_srgb[:2]
    legs = {g.index for g in mesh.vertex_groups if any(k in g.name.split(":")[-1] for k in ("UpLeg", "Leg", "Foot", "Toe"))}
    return armature, mesh, shares, legs


def back_coverage(name, fbx):
    """Share of the body's back torso vertices (hips to shoulders, hair over the coat left out) whose outward normal ray
    meets the copy within 25 cm."""
    import numpy
    from body_landmarks import load_body, measure
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    m = measure(body(name))
    _, cloak, _, _ = load_fitted(fbx)
    cloak_tree = BVHTree.FromPolygons([cloak.matrix_world @ v.co for v in cloak.data.vertices], [p.vertices[:] for p in cloak.data.polygons])
    armature, mesh = load_body(body(name))
    from body_landmarks import outer_layer
    outside = set(outer_layer(armature, mesh))  # long hair lies over the coat by design
    hits = total = 0
    rng = numpy.random.default_rng(1)
    for index in rng.permutation(len(mesh.data.vertices)):
        vertex = mesh.data.vertices[index]
        co = mesh.matrix_world @ vertex.co
        normal = (mesh.matrix_world.to_3x3() @ vertex.normal).normalized()
        back = normal.y * -m["facing"] > 0.5 and co.y * -m["facing"] > 0.05  # (behind the spine: not folds under a bust)
        if back and index not in outside and m["hips_z"] < co.z < m["shoulder_z"] - 0.05 and abs(co.x) < m["shoulder_span"] / 2:
            total += 1
            hits += cloak_tree.ray_cast(co + normal * 0.002, normal, 0.25)[0] is not None
            if total >= 200:
                break
    return hits / max(total, 1)


def test_fitter_regions():
    import numpy
    from body_landmarks import measure
    for name in ("Hero", "Cat"):
        m = measure(body(name))
        _, mesh, shares, legs = load_fitted(fit("Hokage", name))
        assert all(v.groups for v in mesh.data.vertices), "every vertex is skinned"
        for v in mesh.data.vertices:
            if shares[v.index, 0] > 0.01:
                assert not any(g.group in legs and g.weight > 1e-3 for g in v.groups), f"{name}: hem vertex {v.index} on a leg"
        assert shares[:, 0].max() > 0.99 and shares[:, 1].max() > 0.99, shares.max(0)
        co = numpy.array([mesh.matrix_world @ v.co for v in mesh.data.vertices])
        top = (co[:, 2] > m["shoulder_z"] - 0.05) & (numpy.abs(co[:, 0]) < m["shoulder_span"] / 2)
        assert shares[top].max() < 0.01, "shoulders, collar and chest stay pinned"
        assert len(mesh.data.uv_layers) >= 3, [uv.name for uv in mesh.data.uv_layers]


def test_fitter_tail_opening():
    import numpy
    from body_landmarks import measure
    for name, tailed in (("Cat", True), ("Hero", False)):
        m = measure(body(name))
        _, mesh, _, _ = load_fitted(fit("Hokage", name))
        import bmesh as _bmesh
        bm = _bmesh.new()
        bm.from_mesh(mesh.data)
        edge = numpy.array([(mesh.matrix_world @ v.co)[:] for v in bm.verts if v.is_boundary])
        bm.free()
        behind = (edge[:, 1] * -m["facing"] > 0.05) & (numpy.abs(edge[:, 2] - m["hips_z"]) < 0.3) & (numpy.abs(edge[:, 0]) < 0.25)
        if tailed:
            assert behind.any(), f"{name}: an opening in the back for the tail"
        else:
            assert not behind.any(), f"{name}: no opening in the back ({behind.sum()} edge vertices)"


def test_skin_depth_ignores_hair():
    from body_landmarks import load_body, bone_head, skin_depth, outer_layer
    armature, mesh = load_body(body("Bika"))
    chest = bone_head(armature, "LeftArm").z - 0.12
    depth = skin_depth(armature, mesh, chest)
    assert 0.2 < depth < 0.31, f"Bika's chest skin is {depth:.3f} m deep (0.35 with her hair)"
    assert len(outer_layer(armature, mesh)) > 300, "Bika's hair down her back is an outer layer"
    armature, mesh = load_body(body("Hero"))
    assert len(outer_layer(armature, mesh)) < 100, "the Hero has no outer layer on his back"


def coat_depth(fbx, z):
    import numpy
    _, cloak, _, _ = load_fitted(fbx)
    co = numpy.array([cloak.matrix_world @ v.co for v in cloak.data.vertices])
    band = co[(numpy.abs(co[:, 0]) < 0.12) & (numpy.abs(co[:, 2] - z) < 0.03)]
    return float(band[:, 1].max() - band[:, 1].min())


def test_fitter_contains_back():
    from body_landmarks import load_body, bone_head, skin_depth
    for outfit, name in (("Hokage", "Hero"), ("Akatsuki", "Bika")):
        share = back_coverage(name, fit(outfit, name))
        assert share > 0.98, f"{outfit} on {name}: {share:.0%} of the back is under the cloak"
        armature, mesh = load_body(body(name))
        chest = bone_head(armature, "LeftArm").z - 0.12
        skin = skin_depth(armature, mesh, chest)
        coat = coat_depth(fit(outfit, name), chest)
        assert coat < skin + 0.12, f"{outfit} on {name}: the coat is {coat:.2f} m deep over {skin:.2f} m of chest (a tent)"


_masks = {}


def hide_mask(outfit, name):
    """Bakes <Outfit>_<Body>_hide.png for the fitted copy once; returns its path."""
    import subprocess
    key = (outfit, name)
    if key not in _masks:
        fbx = fit(outfit, name)
        png = os.path.join(os.path.dirname(fbx), f"{outfit}_{name}_hide.png")
        run = subprocess.run([bpy.app.binary_path, "-b", "--factory-startup", "--python-exit-code", "1", "-P", os.path.join("Scripts", "character", "bake_hide_mask.py"),
                              "--", body(name), fbx, png], capture_output=True, text=True)
        assert run.returncode == 0, run.stdout[-3000:]
        _masks[key] = png
    return _masks[key]


def shown_by_mask(name, png):
    """(per-vertex shown flags read from the mask at each vertex's UV, body points, measure()) for a body."""
    import numpy
    from body_landmarks import load_body, measure, _points
    m = measure(body(name))
    _, mesh = load_body(body(name))
    image = bpy.data.images.load(png)
    w, h = image.size
    pixels = numpy.array(image.pixels[:], dtype=numpy.float32).reshape(h, w, image.channels)[..., 0]
    uv = mesh.data.uv_layers[0].data
    shown = numpy.ones(len(mesh.data.vertices), dtype=bool)
    for loop in mesh.data.loops:
        u, v = uv[loop.index].uv
        shown[loop.vertex_index] = pixels[min(max(int(v * h), 0), h - 1), min(max(int(u * w), 0), w - 1)] > 0.5
    return shown, _points(mesh), m


def test_hide_mask():
    import numpy
    for outfit in ("Hokage", "Akatsuki"):
        png = hide_mask(outfit, "Hero")
        shown, points, m = shown_by_mask("Hero", png)
        back = (points[:, 1] * -m["facing"] > 0.08) & (points[:, 2] > m["hips_z"] + 0.1) & (points[:, 2] < m["shoulder_z"] - 0.1) & (numpy.abs(points[:, 0]) < 0.1)
        assert back.any() and (~shown[back]).mean() > 0.9, f"{outfit}: the back under the cloak is hidden ({(~shown[back]).mean():.0%})"
        hands = numpy.abs(points[:, 0]) > 0.42  # the A-pose hands and wrists (the hands sit ~0.47 m out)
        assert shown[hands].all(), f"{outfit}: the hands stay visible"
        neck = points[:, 2] > m["head_z"] - 0.02
        assert shown[neck].all(), f"{outfit}: the head and neck above the collar stay visible"
        share = shown.mean()  # (the thighs and hips under the open skirt stay shown: the cloth collides with them)
        assert share < 0.95, f"{outfit}: {share:.0%} of the body shown"
        if outfit == "Hokage":
            front = (points[:, 1] * m["facing"] > 0.08) & (numpy.abs(points[:, 2] - (m["shoulder_z"] - 0.25)) < 0.03) & (numpy.abs(points[:, 0]) < 0.03)
            assert front.any() and shown[front].all(), "Hokage's open front shows the shirt"


only = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv[:-1] else ""
try:
    for name, test in sorted(globals().items()):
        if name.startswith("test_") and only in name and callable(test):
            test()
            print(f"[test_outfit_pipeline] {name} ok")
    print("PASS")
except Exception:  # noqa: BLE001 - any failure fails the run
    traceback.print_exc()
    print("FAIL")
    sys.exit(1)
