"""Ice Meteor hail sources -> SourceArt/Fab/IceHail/ (then Scripts/setup_ice_hail.py imports them). Plain Python + Pillow.

    python Scripts/prep_ice_hail.py <folder of the pack's textures exported as PNG>

The textures come from the Fab pack "Celebrations All In One Bundle" (Content/Celebrations_All_In_One, untracked), exported
with unreal.TextureExporterPNG from /Game/Celebrations_All_In_One/Christmas_All_In_One/Textures/Mask/: T_Ice, T_Ice3, T_Ice5
(ice shattering into chips, on black), T_Ice6 (frost crackle, on black), T_Ice9 (frosted ice surface).

Writes:
  ice_shatter_sheet.png  2048 x 1024, a 4 x 2 atlas of 512 cells on black (ESprite::IceShatter = cells 0-5, FrostCrack = 6-7)
  ice_surface.png        512, the frosted ice the shards are made of
  ice_shard_0..2.obj     faceted ice shards, 100 cm long along +X (tip at +60, broken back end at -40), ~22 cm across
  ice_trail.obj          an 8-sided tube 100 cm long from the origin along -X, radius 1 -> 0.3, U 0 -> 1 along it
"""
import math
import os
import random
import sys

from PIL import Image, ImageOps

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "SourceArt", "Fab", "IceHail")
CELL = 512


def on_black(path):
    image = Image.open(path).convert("RGBA")
    black = Image.new("RGBA", image.size, (0, 0, 0, 255))
    black.alpha_composite(image)
    return black.convert("RGB")


def fit(image):
    """Fit into a cell with a 4% black margin (the atlas is clamped, not padded, so a sprite must not touch its cell edge)."""
    image = image.copy()
    image.thumbnail((int(CELL * 0.92), int(CELL * 0.92)), Image.LANCZOS)
    cell = Image.new("RGB", (CELL, CELL))
    cell.paste(image, ((CELL - image.width) // 2, (CELL - image.height) // 2))
    return cell


def sheet(src):
    ice, ice3, ice5, crack = (on_black(os.path.join(src, f"{n}.png")) for n in ("T_Ice", "T_Ice3", "T_Ice5", "T_Ice6"))
    cells = [ice, ice3, ice5, ImageOps.mirror(ice).rotate(90), ImageOps.mirror(ice3), ice5.rotate(180), crack, crack.rotate(90)]
    atlas = Image.new("RGB", (CELL * 4, CELL * 2))
    for index, cell in enumerate(cells):
        atlas.paste(fit(cell), ((index % 4) * CELL, (index // 4) * CELL))
    atlas.save(os.path.join(OUT, "ice_shatter_sheet.png"))
    Image.open(os.path.join(src, "T_Ice9.png")).convert("RGB").resize((CELL, CELL), Image.LANCZOS).save(os.path.join(OUT, "ice_surface.png"))


def write_obj(name, rings, faces_uv):
    """rings: list of vertex positions; faces_uv: (vertex indices, uvs) triangles. Every face gets its own flat normal."""
    lines, normals, uvs, faces = [f"# {name}"], [], [], []
    for (a, b, c), tri_uv in faces_uv:
        pa, pb, pc = rings[a], rings[b], rings[c]
        u = [pb[i] - pa[i] for i in range(3)]
        v = [pc[i] - pa[i] for i in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(sum(x * x for x in n)) or 1.0
        normals.append([x / length for x in n])
        corners = []
        for vertex, uv in zip((a, b, c), tri_uv):
            uvs.append(uv)
            corners.append(f"{vertex + 1}/{len(uvs)}/{len(normals)}")
        faces.append("f " + " ".join(corners))
    lines += [f"v {x:.4f} {y:.4f} {z:.4f}" for x, y, z in rings]
    lines += [f"vt {u:.4f} {v:.4f}" for u, v in uvs]
    lines += [f"vn {x:.5f} {y:.5f} {z:.5f}" for x, y, z in normals]
    lines += ["s off"] + faces
    with open(os.path.join(OUT, name), "w") as f:
        f.write("\n".join(lines) + "\n")


def shard(seed):
    """A hexagonal ice crystal: a long sharp point, a jittered waist, a shorter broken back. Counter-clockwise, facing out."""
    rng = random.Random(seed)
    sides = 6
    twist = rng.uniform(0, math.tau)
    def ring(x, radius, jitter):
        return [(x + rng.uniform(-3, 3), math.cos(twist + k * math.tau / sides + rng.uniform(-0.25, 0.25)) * radius * rng.uniform(1 - jitter, 1 + jitter),
                 math.sin(twist + k * math.tau / sides + rng.uniform(-0.25, 0.25)) * radius * rng.uniform(1 - jitter, 1 + jitter)) for k in range(sides)]
    tip = (60.0, rng.uniform(-2, 2), rng.uniform(-2, 2))
    waist = ring(0.0, 11.0, 0.25)
    back_ring = ring(-25.0, 8.5, 0.3)
    back = (-40.0 + rng.uniform(-4, 4), rng.uniform(-3, 3), rng.uniform(-3, 3))
    verts = [tip] + waist + back_ring + [back]
    W, B, END = 1, 1 + sides, 1 + 2 * sides
    faces = []
    for k in range(sides):
        n = (k + 1) % sides
        u0, u1 = k / sides, (k + 1) / sides
        faces.append(((0, W + k, W + n), ((u0, 0.0), (u0, 0.6), (u1, 0.6))))
        faces.append(((W + k, B + n, W + n), ((u0, 0.6), (u1, 0.85), (u1, 0.6))))
        faces.append(((W + k, B + k, B + n), ((u0, 0.6), (u0, 0.85), (u1, 0.85))))
        faces.append(((B + k, END, B + n), ((u0, 0.85), (u0, 1.0), (u1, 0.85))))
    return verts, faces


def trail():
    sides = 8
    near = [(0.0, math.cos(k * math.tau / sides), math.sin(k * math.tau / sides)) for k in range(sides)]
    far = [(-100.0, 0.3 * math.cos(k * math.tau / sides), 0.3 * math.sin(k * math.tau / sides)) for k in range(sides)]
    faces = []
    for k in range(sides):
        n = (k + 1) % sides
        v0, v1 = k / sides, (k + 1) / sides
        faces.append(((k, sides + n, n), ((0.0, v0), (1.0, v1), (0.0, v1))))
        faces.append(((k, sides + k, sides + n), ((0.0, v0), (1.0, v0), (1.0, v1))))
    return near + far, faces


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(OUT, exist_ok=True)
    sheet(sys.argv[1])
    for index in range(3):
        write_obj(f"ice_shard_{index}.obj", *shard(17 + index * 31))
    write_obj("ice_trail.obj", *trail())
    print("wrote", os.path.abspath(OUT))
