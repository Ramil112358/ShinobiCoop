"""Switches the already-built scatter chunks (the valley's nature, London's and the arenas' street furniture) from
hierarchical to plain instanced static mesh components for their Nanite meshes, in place: no rebuild of the world.
Nanite culls and picks detail per instance on the GPU, so the hierarchical cluster tree only costs build time and memory
(docs/perf/2026-11-03-performance-audit.md, item 9). Meshes without Nanite keep the hierarchical component.
AShinobiScatterActor::ConvertNaniteToInstanced does the work; new chunks are built this way already. Idempotent.
UE Python commandlet (close the editor first):
  Scripts/run_python.ps1 -TimeoutSec 3600 -Script Scripts/world/convert_scatter_ism.py
"""
import unreal

MAPS = [
    "/Game/Shinobi/Maps/Lvl_HiddenLeafValley",
    "/Game/Shinobi/Maps/Lvl_London",
    "/Game/Shinobi/Maps/Lvl_Arena_WestEnd",
    "/Game/Shinobi/Maps/Lvl_Arena_Camden",
    "/Game/Shinobi/Maps/Lvl_Arena_Westminster",
    "/Game/Shinobi/Maps/Lvl_Arena_Tower",
]


def log(message):
    unreal.log(f"[convert_scatter_ism] {message}")


def convert(map_path):
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        log(f"{map_path}: not built, skipped")
        return
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(map_path):
        raise RuntimeError(f"could not load {map_path}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        # World Partition: every actor, as the build scripts load them (the chunks live in external actor packages).
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
    except Exception as error:  # (a map without World Partition has its actors loaded already)
        log(f"{map_path}: no World Partition actors to load ({error})")
    chunks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ShinobiScatterActor)]
    switched = 0
    for chunk in chunks:
        switched += chunk.convert_nanite_to_instanced()
    left = sum(chunk.count_hierarchical_nanite() for chunk in chunks)
    log(f"{map_path}: {len(chunks)} chunks, {switched} components switched to instanced, {left} Nanite components still hierarchical")
    if switched:
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
            raise RuntimeError(f"could not save {map_path}")
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


for path in MAPS:
    convert(path)
log("done")
