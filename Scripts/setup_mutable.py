"""CO_Shinobi's content: the Mutable character customisation (docs/superpowers/specs/2026-10-29-mutable-customisation-design.md).

  Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py            (every stage, in order)
  $env:SETUP_MUTABLE_ONLY="bases"; Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py

bases: each sex's base body (/Game/Shinobi/Mutable/Bases/SK_MutableBase_<Sex>) = the average MetaHuman body built by
  Scripts/mutable/base_bodies.py, with morphs Build_Under, Build_Over (the slim and heavy builds) and Build_Own (RamilMH's
  or BikaMH's own body) baked in by UShinobiFaceBlendLibrary (same vertex layout, aligned at the root).
"""
import os
import sys

import unreal

sys.path.append(os.path.join(os.path.dirname(__file__), "mutable"))
from names import MUTABLE_ROOT, OWN_BODIES  # noqa: E402

ONLY = [s for s in os.environ.get("SETUP_MUTABLE_ONLY", "").split(",") if s]
EAL = unreal.EditorAssetLibrary
BASES = f"{MUTABLE_ROOT}/Bases"
SEXES = ("Male", "Female")


def log(message):
    unreal.log(f"[setup_mutable] {message}")


def built_body(name):
    return f"{BASES}/Build/MHC_{name}/Body/SKM_MHC_{name}_BodyMesh"


def base_path(sex):
    return f"{BASES}/SK_MutableBase_{sex}"


def bases():
    for sex in SEXES:
        path = base_path(sex)
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        base = EAL.duplicate_asset(built_body(f"MB_{sex}_Normal"), path)
        if not base:
            raise RuntimeError(f"no built MB_{sex}_Normal body: run Scripts/mutable/base_bodies.py first")
        others = [unreal.load_asset(built_body(f"MB_{sex}_Under")), unreal.load_asset(built_body(f"MB_{sex}_Over")),
                  unreal.load_asset(OWN_BODIES[sex])]
        report = unreal.ShinobiFaceBlendLibrary.bake_face_blend_morphs(base, others, ["Under", "Over", "Own"], 4, "root", "Build_", 60.0)
        EAL.save_loaded_asset(base, only_if_is_dirty=False)
        log(f"{sex} base {path}: {report}")


STAGES = {"bases": bases}
for stage in ONLY or list(STAGES):
    STAGES[stage]()
