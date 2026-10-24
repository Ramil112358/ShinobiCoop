"""Dresses Lvl_London's named villagers as City Sample Londoners too (adds the marker tag Look:Crowd), so the city's people
are all crowd characters; they keep their walks and London barks. One-off patch of the built map; close the GUI editor.

  powershell -File Scripts/run_python.ps1 -Script Scripts/london/crowd_only.py
"""
import unreal

MAP = "/Game/Shinobi/Maps/Lvl_London"


def log(message):
    unreal.log(f"[crowd_only] {message}")


def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    named = [a for a in actors if "ShinobiVillager" in [str(t) for t in a.tags] and not any(str(t).startswith("Look:") for t in a.tags)]
    for actor in named:
        actor.modify()
        actor.tags = list(actor.tags) + [unreal.Name("Look:Crowd")]
        log(f"{actor.get_actor_label()} -> crowd")
    if named:
        unreal.EditorLoadingAndSavingUtils.save_packages([a.get_outermost() for a in named], True)
    log(f"{len(named)} villagers now crowd Londoners")


main()
