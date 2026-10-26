"""Character creator face presets made in MetaHuman Creator (GUI editor only, one step per editor run; close your own
editor first: Creator scripting inside an open, busy editor crashed it twice). Run through run_metahuman_gui.ps1:

  Scripts/character/run_metahuman_gui.ps1 -Script Scripts/character/creator_faces.py -Stages read
  ... -Stages make  -Name CF_M1      (one candidate per run: duplicate its base preset, blend in other presets' faces,
                                     then Epic's cloud auto-rig and skin textures; needs the editor signed in to Epic)
  ... -Stages build -Name CF_M1      (Optimized, High quality, into /Game/Shinobi/MetaHumans/Creator/Build)

read: every Creator preset's face model coefficients and settings summary -> Saved/CreatorFaces/presets.json.
A candidate (CANDIDATES) = a base preset (its skin, eyes, body) whose face is mixed with other presets' faces by weight
(get/set_face_model_coefficients: the Creator's own face model, so any mix is a valid MetaHuman face).
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Saved", "CreatorFaces")
PRESETS = "/MetaHumanCharacter/Optional/Presets"
DEST = "/Game/Shinobi/MetaHumans/Creator"
ONLY = [s for s in os.environ.get("SETUP_MH_ONLY", "").split(",") if s]
NAME = os.environ.get("SETUP_MH_NAME", "")
EAL = unreal.EditorAssetLibrary
MH = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

# name: (base preset, {other preset: weight}); the base keeps the remaining weight. Sexes per the Creator presets.
CANDIDATES = {
    "CF_M1": ("Mateo", {"Kelvin": 0.4}),
    "CF_M2": ("Lorenzo", {"Bo": 0.35, "Victor": 0.15}),
    "CF_M3": ("Omari", {"Isaiah": 0.4}),
    "CF_M4": ("Jorge", {"Mikel": 0.35, "Cameron": 0.2}),
    "CF_F1": ("Ada", {"Lani": 0.4}),  # (Aoi came out male, beard and all: the presets carry no sex)
    "CF_F2": ("Celeste", {"Jelena": 0.35, "Vivian": 0.15}),
    "CF_F3": ("Zuri", {"Asha": 0.4}),
    "CF_F4": ("Sunita", {"Grace": 0.35, "Tuya": 0.2}),
}


def log(message):
    unreal.log(f"[creator_faces] {message}")


class Editing:
    def __init__(self, character):
        self.character = character

    def __enter__(self):
        if not MH.try_add_object_to_edit(self.character):
            raise RuntimeError(f"{self.character.get_name()} is open for edit elsewhere")
        return self.character

    def __exit__(self, *_):
        MH.remove_object_to_edit(self.character)


def preset_names():
    return sorted(a.rsplit(".", 1)[-1] for a in EAL.list_assets(PRESETS, recursive=False))


def read():
    os.makedirs(OUT, exist_ok=True)
    data = {}
    for name in preset_names():
        character = unreal.load_asset(f"{PRESETS}/{name}")
        with Editing(character):
            coefficients = list(MH.get_face_model_coefficients(character))
        body = character.get_editor_property("body_type") if hasattr(character, "body_type") else None
        data[name] = {"coefficients": coefficients, "body_type": str(body)}
        log(f"{name}: {len(coefficients)} coefficients, body {body}")
    with open(os.path.join(OUT, "presets.json"), "w", encoding="utf-8") as f:
        json.dump(data, f)
    props = [p for p in dir(unreal.load_asset(f"{PRESETS}/{preset_names()[0]}")) if not p.startswith("_")]
    log(f"character properties: {[p for p in props if any(k in p for k in ('body', 'sex', 'gender', 'skin', 'eye', 'head'))]}")


def types():
    for name in preset_names():
        character = unreal.load_asset(f"{PRESETS}/{name}")
        log(f"{name}: {character.get_editor_property('fixed_body_type')}")


def make():
    base, mix = CANDIDATES[NAME]
    with open(os.path.join(OUT, "presets.json"), encoding="utf-8") as f:
        presets = json.load(f)
    path = f"{DEST}/{NAME}/MHC_{NAME}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    if not EAL.duplicate_asset(f"{PRESETS}/{base}", path):
        raise RuntimeError(f"could not copy preset {base}")
    character = unreal.load_asset(path)
    weights = {base: 1.0 - sum(mix.values()), **mix}
    blended = [sum(w * presets[p]["coefficients"][i] for p, w in weights.items())
               for i in range(len(presets[base]["coefficients"]))]
    with Editing(character):
        MH.set_face_model_coefficients(character, blended)
        MH.commit_face_state(character)
        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking, rig.report_progress = True, False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        MH.request_auto_rigging(character, rig)
        log(f"{NAME}: auto-rig requested")
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking, tex.report_progress = True, False
        MH.request_texture_sources(character, tex)
        log(f"{NAME}: texture sources requested")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"{NAME}: {base} + {mix} -> {path}")


def build():
    character = unreal.load_asset(f"{DEST}/{NAME}/MHC_{NAME}")
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    params.absolute_build_path = f"{DEST}/Build"
    params.common_folder_path = "/Game/Shinobi/MetaHumans/Common"
    params.enable_wardrobe_item_validation = False
    with Editing(character):
        log(f"{NAME}: can build {MH.can_build_meta_human(character)}")
        try:
            MH.build_meta_human(character=character, params=params)
        except RuntimeError as error:
            if "Cannot break link" not in str(error):
                raise
    blueprint = f"{DEST}/Build/MHC_{NAME}/BP_MHC_{NAME}"
    if not EAL.does_asset_exist(blueprint):
        raise RuntimeError(f"build made no {blueprint}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=False, save_content_packages=True)
    log(f"{NAME}: built {blueprint}")


STAGES = {"types": types, "read": read, "make": make, "build": build}
try:
    for stage in ONLY:
        STAGES[stage]()
except Exception as error:
    log(f"ERROR: {error}")
    raise
finally:
    if os.environ.get("SETUP_MH_QUIT"):
        unreal.SystemLibrary.quit_editor()
