"""Which full MetaHuman bodies share the body template's vertex layout, and how far each differs in shape (spike
question 2 of the Mutable customisation plan). The City Sample crowd "body" meshes are only the visible skin (hands), so
the bases and build morphs come from full MetaHuman bodies.

Bakes trial morphs onto a throwaway copy of the template (UShinobiFaceBlendLibrary reports matched or rejected, and the
largest vertex offset, per body), logs each body's size, then deletes the copy.
  Scripts/run_python.ps1 -Script Scripts/mutable/check_layout.py
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(__file__))
from names import BODY_TEMPLATE, FULL_BODIES  # noqa: E402

TEMP = "/Game/Shinobi/Mutable/_LayoutCheck"


def log(message):
    unreal.log(f"[setup_layout] {message}")


def size(mesh):
    box = mesh.get_bounds().box_extent
    return f"{box.x * 2:.0f}x{box.y * 2:.0f}x{box.z * 2:.0f} cm"


template = unreal.load_asset(BODY_TEMPLATE)
log(f"template {template.get_path_name()} {size(template)}")
bodies = []
for name, path in FULL_BODIES.items():
    body = unreal.load_asset(path)
    if body:
        log(f"{name}: {size(body)}")
        bodies.append((name, body))
    else:
        log(f"{name}: missing {path}")

copy = unreal.EditorAssetLibrary.duplicate_asset(BODY_TEMPLATE, f"{TEMP}/Template")
report = unreal.ShinobiFaceBlendLibrary.bake_face_blend_morphs(copy, [b for _, b in bodies], [n for n, _ in bodies], 1, "root", "Check_", 60.0)
for part in report.split(";"):
    if part.strip():
        log(part.strip())
unreal.EditorAssetLibrary.delete_directory(TEMP)
