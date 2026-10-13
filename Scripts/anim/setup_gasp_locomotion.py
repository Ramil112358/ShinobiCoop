"""Hooks the Game Animation Sample's motion-matching AnimBP up to our characters. Every humanoid's hidden Manny runs it
(AShinobiCharacterBase::BeginPlay).

Run after Scripts/anim/migrate_gasp.py, with the editor closed:

  UnrealEditor-Cmd.exe E:\\ShinobiCoop\\ShinobiCoop.uproject -run=pythonscript -script=E:\\ShinobiCoop\\Scripts\\anim\\setup_gasp_locomotion.py -unattended -nop4 -nosplash

1. Reparents /Game/Blueprints/SandboxCharacter_CMC_ABP to UShinobiLocomotionAnimInstance, whose UpdateCharacterProperties()
   fills the AnimBP's CharacterProperties from our C++ characters. The AnimBP is edited in place, not duplicated: the
   sample's chooser tables (CHT_CMCCharacterAnimations, CHT_PoseSearchDatabases, ...) take exactly
   SandboxCharacter_CMC_ABP_C as their context object and reject any other class.
2. Marks SK_Mannequin (our hidden Manny, and the skeleton of every /Game/Shinobi/Anims montage) and SK_UEFN_Mannequin
   (the sample's animations) compatible with each other, so the AnimBP runs on Manny and plays both sets of animations.

One step needs the editor's graph tools and is done through Unreal MCP (BlueprintTools.write_graph_dsl), not here: replace
the body of the AnimBP's Update_PropertiesFromCharacter function, which calls the sample's pawn interface, with

  (fn Update_PropertiesFromCharacter ()
    (Shinobi|Animation|UpdateCharacterProperties))

Config/DefaultEngine.ini also carries the sample's [/Script/Engine.DataDrivenConsoleVariableSettings] (DDCvar.*), which the
AnimBP reads.
"""
import unreal

ANIM_BLUEPRINT = "/Game/Blueprints/SandboxCharacter_CMC_ABP"
PARENT = "/Script/ShinobiCoop.ShinobiLocomotionAnimInstance"
MANNY_SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
UEFN_SKELETON = "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin"


def make_compatible(skeleton_path, other_path):
    skeleton = unreal.load_asset(skeleton_path)
    other = unreal.load_asset(other_path)
    compatible = list(skeleton.get_editor_property("compatible_skeletons"))
    if any(s and s.get_path_name() == other.get_path_name() for s in compatible):
        return
    compatible.append(other)
    skeleton.set_editor_property("compatible_skeletons", compatible)
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton, only_if_is_dirty=False)
    unreal.log(f"setup_gasp_locomotion: {skeleton_path} now compatible with {other_path}")


def main():
    make_compatible(MANNY_SKELETON, UEFN_SKELETON)
    make_compatible(UEFN_SKELETON, MANNY_SKELETON)

    blueprint = unreal.load_asset(ANIM_BLUEPRINT)
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, unreal.load_class(None, PARENT))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    unreal.log(f"setup_gasp_locomotion: {ANIM_BLUEPRINT} reparented to {PARENT}")


main()
