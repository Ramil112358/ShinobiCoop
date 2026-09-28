"""Realistic look for the arenas (sub-project #3b): builds M_OutlinePost (Scripts/world/look.py) and swaps each arena's
screen-wide cartoon post (M_ToonPost volume from setup_m5.py) for the character outline and realistic post settings.
UE Python commandlet: Scripts/run_python.ps1 -Script Scripts/setup_look.py. The valley gets the same through
Scripts/world/build_world.py (stage "mood")."""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "world"))
import look  # noqa: E402

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ARENAS = ["/Game/Shinobi/Maps/Lvl_TrainingGrounds", "/Game/Variant_Combat/Lvl_Combat", "/Game/ThirdPerson/Lvl_ThirdPerson"]
OLD_LABEL = "ShinobiToonPost"

look.outline_material()
for path in ARENAS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    if world is None:
        raise RuntimeError(f"failed to load {path}")
    volumes = [a for a in EAS.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume)
               and a.get_actor_label() in (OLD_LABEL, look.VOLUME_LABEL)]
    volume = volumes[0] if volumes else EAS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    volume.set_actor_label(look.VOLUME_LABEL)
    look.apply_post(volume)
    for extra in volumes[1:]:
        EAS.destroy_actor(extra)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
        raise RuntimeError(f"failed to save {path}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"[setup_look] {path}: outline post")
