"""Gives a World Partition map a NavMeshBoundsVolume over its playable area and its RecastNavMesh actor, which the Python
build commandlets never created, so villagers and enemies can path (game log without it: "Unable to find
RecastNavMesh instance"). UE Python commandlet: SHINOBI_NAV_MAP=/Game/Shinobi/Maps/Lvl_HiddenLeafValley
SHINOBI_NAV_HALF_M=470 Scripts/run_python.ps1 Scripts/world/add_navmesh.py. Idempotent."""
import os

import unreal

MAP = os.environ.get("SHINOBI_NAV_MAP", "/Game/Shinobi/Maps/Lvl_HiddenLeafValley")
HALF_M = float(os.environ.get("SHINOBI_NAV_HALF_M", "470"))
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level(MAP):
    raise RuntimeError(f"could not load {MAP}")
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NavMeshBoundsVolume):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    nav.set_actor_scale3d(unreal.Vector(HALF_M, HALF_M, 150))  # the brush is 200 uu: scale = half-extent in m
    nav.set_actor_label("NavBounds")
    nav.set_editor_property("is_spatially_loaded", False)
if unreal.ShinobiWorldBuilderLibrary.ensure_nav_mesh(world) is None:
    raise RuntimeError("no navigation data")
unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log(f"[setup_nav] {MAP}: nav bounds +/-{HALF_M:.0f} m and navmesh in place")
