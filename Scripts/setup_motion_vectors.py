"""Upscaler- and frame-generation-friendly content (docs/perf/2026-11-03-graphics-tech-feasibility.md, section 5). Idempotent;
patches the existing assets in place (no rebuild of the VFX, the hail or the maps):

1. Jutsu mesh effects write motion vectors. TSR, DLSS and DLSS Frame Generation rebuild each frame from the last one
   along the motion vectors; a translucent effect writes none by default, so a fireball, a chakra shell or the Susanoo
   smears ("ghosts") behind itself when it or the camera moves. "Output Depth and Velocity" (bOutputTranslucentVelocity)
   on their materials fixes that. Only for the mesh effects (shells, walls, domes, pillars, trails); the soft sprite
   cards (M_Sprite*) are left without: their velocity would overwrite the background's behind their faint edges.
2. The Ice Meteor shower is drawn as instanced static meshes (UShinobiInstancedVfxSubsystem): M_IceShard and
   M_IceTrail get the Instanced Static Meshes usage, which a cooked build needs (without it they render as the default
   checker material).
3. M_OutlinePost moves after the upscaler (Scripts/world/look.py: drawn at output resolution, its stencil lookups mapped
   to the render resolution), rebuilt in place so the valley's, London's and the arenas' post volumes keep it.

Wind-blown foliage (World Position Offset) writes its motion through r.Velocity.EnableVertexDeformation=1 in
Config/DefaultEngine.ini; nothing to do per material.

The builders (setup_vfx.py, setup_walls.py, setup_ice_hail.py) set the same flags, so a rebuild keeps them.
UE Python commandlet: Scripts/run_python.ps1 -Script Scripts/setup_motion_vectors.py (close the editor first)."""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "world"))
import look  # noqa: E402

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

# Translucent / additive jutsu materials drawn on meshes (setup_vfx.py, setup_walls.py, setup_ice_hail.py).
VELOCITY = [
    "/Game/Shinobi/VFX/M_ChakraBodyAura",  # every MakeGlow: orbs, bursts, rasengan, body auras
    "/Game/Shinobi/VFX/M_Flame",
    "/Game/Shinobi/VFX/M_FlameDark",
    "/Game/Shinobi/VFX/M_Phased",
    "/Game/Shinobi/VFX/M_Susanoo",
    "/Game/Shinobi/VFX/M_SusanooBlade",
    "/Game/Shinobi/VFX/M_WaterWall",
    "/Game/Shinobi/VFX/M_PushDome",
    "/Game/Shinobi/VFX/M_ChakraPillar",
    "/Game/Shinobi/VFX/M_IceTrail",
    "/Game/Shinobi/VFX/Walls/M_WaterCurtain",
]
# Drawn as instanced static meshes.
INSTANCED = ["/Game/Shinobi/VFX/M_IceShard", "/Game/Shinobi/VFX/M_IceTrail"]


def log(message):
    unreal.log(f"[setup_motion_vectors] {message}")


def find(path):
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    # (a builder may keep it in another folder: search by name)
    name = path.rsplit("/", 1)[1]
    for found in EAL.list_assets("/Game/Shinobi", recursive=True):
        if found.rsplit("/", 1)[1].split(".")[0] == name:
            return unreal.load_asset(found)
    return None


def main():
    changed = set()
    for path in VELOCITY:
        m = find(path)
        if m is None:
            log(f"MISSING {path} (skipped)")
            continue
        if not m.get_editor_property("output_translucent_velocity"):
            m.set_editor_property("output_translucent_velocity", True)
            changed.add(m)
            log(f"{m.get_name()}: Output Depth and Velocity on")
    for path in INSTANCED:
        m = find(path)
        if m is None:
            log(f"MISSING {path} (skipped)")
            continue
        if not MEL.has_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES):
            MEL.set_base_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
            changed.add(m)
            log(f"{m.get_name()}: Instanced Static Meshes usage on")
    for m in changed:
        errors = MEL.recompile_material(m)
        if errors:
            raise RuntimeError(f"{m.get_name()} failed to compile: {errors}")
        EAL.save_loaded_asset(m)
    look.outline_material()
    log(f"M_OutlinePost rebuilt after tonemapping; {len(changed)} other material(s) changed")


main()
