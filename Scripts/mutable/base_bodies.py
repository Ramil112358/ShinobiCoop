"""The Mutable base bodies made in MetaHuman Creator (GUI editor only; close your own editor first). Run through
run_metahuman_gui.ps1:

  Scripts/character/run_metahuman_gui.ps1 -Script Scripts/mutable/base_bodies.py -Stages make
  Scripts/character/run_metahuman_gui.ps1 -Script Scripts/mutable/base_bodies.py -Stages build -Name MB_Male_Normal
                                                                          (one body per editor run: a build takes ~20 min,
                                                                          and several in one editor ran out of memory)

make: each body (BODIES) = a Creator preset copied to /Game/Shinobi/Mutable/Bases/<Name>/MHC_<Name>, its outfit cleared
(the Optimized build removes body skin hidden under an outfit, which changes the body's vertex layout) and its body
measurements set (BUILD_SCALES; slim, average and heavy of each sex share everything else), then auto-rigged and given skin texture
sources by Epic's cloud services (the editor must be signed in to Epic).
build: each body built (Optimized, High) into /Game/Shinobi/Mutable/Bases/Build. Its body mesh keeps the MetaHuman
template's vertex layout (32334 render vertices at LOD 0), so Scripts/setup_mutable.py (stage bases) can bake the slim and
heavy bodies into build morphs on the average one.
"""
import os

import unreal

DEST = "/Game/Shinobi/Mutable/Bases"
PRESETS = "/MetaHumanCharacter/Optional/Presets"
ONLY = [s for s in os.environ.get("SETUP_MH_ONLY", "").split(",") if s]
NAME = os.environ.get("SETUP_MH_NAME", "")
EAL = unreal.EditorAssetLibrary
MH = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

# name (MB_<Sex>_<Build>): Creator preset. Sexes per the presets (Mateo male, Ada female).
BODIES = {f"MB_{sex}_{build}": preset for sex, preset in (("Male", "Mateo"), ("Female", "Ada")) for build in ("Normal", "Under", "Over")}


def log(message):
    unreal.log(f"[setup_bases] {message}")


def character_path(name):
    return f"{DEST}/{name}/MHC_{name}"


class Editing:
    """The subsystem's edit lock on a character, released however the block ends."""

    def __init__(self, character):
        self.character = character

    def __enter__(self):
        if not MH.try_add_object_to_edit(self.character):
            raise RuntimeError(f"{self.character.get_name()} is open for edit elsewhere (close the editor)")
        return self.character

    def __exit__(self, *_):
        if MH.is_object_added_for_editing(self.character):
            MH.remove_object_to_edit(self.character)


# Measurement scales per build (of the preset's own measurements): Creator's "Fat" alone left the body unchanged.
BUILD_SCALES = {
    "Under": {"Waist": 0.85, "Chest": 0.92, "Hip": 0.92, "Thigh": 0.85, "Bicep": 0.85},
    "Normal": {},
    "Over": {"Waist": 1.35, "Chest": 1.15, "Underbust": 1.15, "Hip": 1.15, "High Hip": 1.15, "Thigh": 1.2, "Bicep": 1.2},
}


def set_shape(character, build):
    """Activates the build's body measurements at the preset's own values times BUILD_SCALES (elements are written back:
    Python iterates struct copies)."""
    scales = BUILD_SCALES[build]
    constraints = MH.get_body_constraints(character)
    for index in range(len(constraints)):
        constraint = constraints[index]
        name = str(constraint.get_editor_property("name"))
        if name in scales:
            constraint.set_editor_property("is_active", True)
            constraint.set_editor_property("target_measurement", constraint.get_editor_property("target_measurement") * scales[name])
            constraints[index] = constraint
    MH.set_body_constraints(character, constraints)
    MH.commit_body_state(character)
    active = {str(c.get_editor_property("name")): round(c.get_editor_property("target_measurement"), 1)
              for c in MH.get_body_constraints(character) if c.get_editor_property("is_active")}
    log(f"{character.get_name()}: {build} measurements {active}")


def shape():
    """Applies each body's build measurements to the made (already rigged) characters (all, or -Name's)."""
    for name in [NAME] if NAME else BODIES:
        character = unreal.load_asset(character_path(name))
        with Editing(character):
            set_shape(character, name.rsplit("_", 1)[1])
        EAL.save_loaded_asset(character, only_if_is_dirty=False)


def make():
    for name, preset in ((n, BODIES[n]) for n in ([NAME] if NAME else BODIES)):
        path = character_path(name)
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        if not EAL.duplicate_asset(f"{PRESETS}/{preset}", path):
            raise RuntimeError(f"could not copy preset {preset}")
        character = unreal.load_asset(path)
        with Editing(character):
            set_shape(character, name.rsplit("_", 1)[1])
            # As setup_metahuman.py's no_outfit: an empty Outfits selection (no hidden-face removal at build).
            character.internal_collection.default_instance.set_single_slot_selection("Outfits", unreal.MetaHumanPaletteItemKey())
            # A build needs a rig and skin texture sources: Epic's cloud services, as creator_faces.py (editor signed in).
            rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            rig.blocking, rig.report_progress = True, False
            rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
            MH.request_auto_rigging(character, rig)
            tex = unreal.MetaHumanCharacterTextureRequestParams()
            tex.blocking, tex.report_progress = True, False
            MH.request_texture_sources(character, tex)
        EAL.save_loaded_asset(character, only_if_is_dirty=False)
        log(f"{name}: {preset} -> {path}")


def build():
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    params.absolute_build_path = f"{DEST}/Build"
    params.common_folder_path = "/Game/Shinobi/MetaHumans/Common"
    params.enable_wardrobe_item_validation = False
    sks = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    if NAME not in BODIES:
        raise RuntimeError(f"build needs -Name, one of {list(BODIES)}")
    for name in [NAME]:
        character = unreal.load_asset(character_path(name))
        with Editing(character):
            try:
                MH.build_meta_human(character=character, params=params)
            except RuntimeError as error:
                if "Cannot break link" not in str(error):
                    raise
        body = unreal.load_asset(f"{DEST}/Build/MHC_{name}/Body/SKM_MHC_{name}_BodyMesh")
        if not body:
            raise RuntimeError(f"build made no body for {name}")
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=False, save_content_packages=True)
        log(f"{name}: built and saved, body LOD 0 {sks.get_num_verts(body, 0)} vertices")


STAGES = {"make": make, "shape": shape, "build": build}
try:
    for stage in ONLY:
        STAGES[stage]()
except Exception as error:
    log(f"ERROR: {error}")
    raise
finally:
    if os.environ.get("SETUP_MH_QUIT"):
        unreal.SystemLibrary.quit_editor()
