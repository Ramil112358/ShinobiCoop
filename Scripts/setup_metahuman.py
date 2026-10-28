"""MetaHuman roster characters from head scans. Idempotent per stage; close the GUI editor first.

  powershell -ExecutionPolicy Bypass -File Scripts\\run_python.ps1 -Script Scripts\\setup_metahuman.py

SETUP_MH_NAME picks the character (default RamilMH, scan from SourceArt/Character/Models/Ramil/MetaHuman, made by
Scripts/character/prep_mh_head.py). SETUP_MH_ONLY=<stage>[,<stage>] runs only those stages:

  scan     (re)import the prepped head as SM_<Name>_HeadScan with its photo texture (unlit).
  conform  the scan head (SM_<Name>_HeadScan) becomes MHC_<Name>'s face: landmarks tracked on the prep's portrait from
           the prep's camera, rigid align, then the Creator's "head_only" auto solve; then the body's measurements.
  footage  the face from a Live Link Face take through a MetaHuman Identity (GUI editor), replacing the character's face.
  template the face from a head already in MetaHuman topology (CFG template, e.g. KeenFace's head.fbx; GUI editor).
  hair     only the Hair slot, to CHARACTERS' pick (then build with SETUP_MH_GROOM_COLOURS=1 to colour it).
  fabhair  a Fab Hair pick put into the built blueprint (the build drops single-LOD grooms); build runs it too.
  bodyhide the built body's skin material takes outfit hide masks (build runs it too).
  bakeinputs the built face mesh + base colour for Scripts/character/bake_face_video.py (run_python.ps1 -Render).
  photoskin the baked video face (face_basecolor) as the face base colour override.
  clearskin the face base colour override off (before a build whose base colour bakeinputs exports for a re-bake).
  body     only the body's measurements (keeps a face made in the Creator, e.g. Import > From Identity).
  look     skin tone, eye colour and grooms (hair, brows, stubble, lashes).
  cloud    face rig + high-res skin textures from Epic's MetaHuman service (the editor must be signed in to Epic).
  build    the grooms' melanin, then the Optimized / High assembly into <Dest>/Build (GUI editor only; last stage).
  colours  just the grooms' melanin on the built material instances.
  retarget IK_<Name> + RTG_MannyTo<Name> on the built body (the roster's retargeter).
  preview  front and three-quarter renders next to the scan (needs run_python.ps1 -Render).
Output: /Game/Shinobi/MetaHumans/<Name>.
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
NAME = os.environ.get("SETUP_MH_NAME", "RamilMH")
ONLY = [s for s in os.environ.get("SETUP_MH_ONLY", "").split(",") if s]
# Per character: the scan folder (prep_mh_head.py output) and the body's measurements in cm (Body Params constraint
# names, lowered with spaces as _; the log lists every one: height, chest, waist, hip, inseam, ... fat, muscularity).
CHARACTERS = {
    "RamilMH": {
        "scan": os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil", "MetaHuman"),
        "scan_name": "Ramil",
        # Live Link Face (MetaHuman Animator mode, iPhone 14 TrueDepth) take: the folder holding the take folder, and its
        # neutral, front-facing, eyes-open frame. The app records H.264, but the scripted ingest reads only MJPEG: this is
        # a copy with the video transcoded (frame for frame, OpenCV) from LiveLinkFace/ (Ramil_LiveLinkFace.zip).
        "footage": os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil", "LiveLinkFaceMJPG"),
        "neutral_frame": 180,
        "side_frames": [270, 900],  # three-quarter right / left: they pin the jawline and cheeks
        # The face's base colour from the same take (projected and baked onto the head, Scripts/character/bake_face_video.py):
        # set as the Creator's face texture override.
        "face_basecolor": os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil", "MetaHuman", "Ramil_face_bc_video.png"),
        # The same take ingested by Live Link Hub's Capture Manager (Take Archive Ingest device on LiveLinkFace/), which
        # decodes the H.264 original: preferred over the scripted MJPEG ingest when it exists.
        "capture_data": "/Game/CaptureManager/Imports/Take_Archive_Ingest/MySlate_4/CD_MySlate_4",
        # 186 cm, 92 kg, chest 110: athletic, muscular. The waist is an estimate for that build; muscularity is the Body
        # Params slider (-1..1, the default body is 0.2).
        "body": {"height": 186.0, "chest": 110.0, "waist": 86.0, "muscularity": 0.6},
        # Skin tone (u, v) near the Lorenzo preset's light olive; the Cameron preset's dark brown iris (the Mateo / Jorge
        # one, v 0.56, rendered hazel).
        "skin": {"u": 0.42, "v": 0.48},
        "iris": {"primary_color_u": 0.86, "primary_color_v": 0.10, "secondary_color_u": 0.96, "secondary_color_v": 0.0},
        # Wardrobe slot -> item under /MetaHumanCharacter/Optional/Grooms/Bindings/<folder>/, and its melanin (black ~1).
        "grooms": {  # the user's Creator picks (Hair & Clothing), and their colour (Hair Color + Lightness)
            # Fab "FREE Sample - MetaHuman Hairstyle Male 11" (.mhpkg unpacked to Content/Grooms/SC_Hairstyle_Male_11):
            # medium, full and wavy, closer to his than any bundled style (the fullest bundled: Hair/WI_Hair_S_SweptUp)
            "Hair": ("/Game/Grooms/SC_Hairstyle_Male_11/WI_SC_Hairstyle_Male_11", 1.0),
            "Eyebrows": ("Eyebrows/WI_Eyebrows_M_Dense", 1.0),
            "Beard": ("Beards/WI_Beard_M_Stubble", 1.0),
            "Mustache": ("Mustaches/WI_Mustache_M_Stubble", 1.0),
            "Eyelashes": ("Eyelashes/WI_Eyelashes_L_SlightCurl", 1.0),
        },
        "groom_redness": 0.1,
        "groom_lightness": 0.3,
    },
    # No head scan: the face comes only from the Live Link Face take (footage stage, once Live Link Hub has ingested it:
    # Take Archive Ingest on SourceArt/.../Bika/LiveLinkFace). The character starts as a copy of a female Creator preset.
    "BikaMH": {
        "base_preset": "Ada",  # Masculine/Feminine 0.6; its face is replaced by the footage's
        "footage": os.path.join(ROOT, "SourceArt", "Character", "Models", "Bika", "LiveLinkFace"),
        "neutral_frame": 480,  # front, eyes most open (MediaPipe blendshapes over the take), relaxed (the take: 634 frames at 60 fps, 1080x1920)
        "side_frames": [100, 444],  # turns of 35-40 degrees to each side, eyes open
        "capture_data": "/Game/CaptureManager/Imports/Take_Archive_Ingest/MySlate_6/CD_MySlate_6",
        # 160 cm, 50 kg, lean and athletic: the waist/hip/chest are estimates for that build (Ada: 173.5 cm, 87/67/99).
        "body": {"height": 160.0, "chest": 84.0, "waist": 64.0, "hip": 91.0, "fat": -0.5, "muscularity": 0.25},
        # Photos (SourceArt/Character/Photos/Bika): medium olive skin, dark brown eyes, long dark-brown wavy hair, full natural brows.
        "skin": {"u": 0.42, "v": 0.48},
        "iris": {"primary_color_u": 0.86, "primary_color_v": 0.10, "secondary_color_u": 0.96, "secondary_color_v": 0.0, "global_tint": 0.15},
        # The face base colour baked from the take (Scripts/character/bake_face_video.py, BAKE_* env: see its docstring).
        "face_basecolor": os.path.join(ROOT, "SourceArt", "Character", "Models", "Bika", "MetaHuman", "Bika_face_bc_video.png"),
        "grooms": {
            "Hair": ("Hair/WI_Hair_L_Straight", 1.0),
            "Eyebrows": ("Eyebrows/WI_Eyebrows_M_Full", 1.0),
            "Eyelashes": ("Eyelashes/WI_Eyelashes_L_Curl", 1.0),
        },
        "hair_strands": True,
        "no_outfit": True,
        "groom_redness": 0.0,
        "groom_lightness": 0.3,
    },
}
# A second face for RamilMH (AShinobiPlayerCharacter::Faces): the later RealityScan (Ramil_head_RealityScan1, prepped into
# MetaHumanScan1). A copy of MHC_RamilMH (its Creator skin, eyes, grooms and colours) whose face conform replaces; the game
# keeps RamilMH's body and wears only this build's face and grooms. Stages: conform, cloud, build (GUI); then the scan's
# photo texture: bakeinputs (-Render), bake_face_video.py with BAKE_SCAN, photoskin, build.
CHARACTERS["RamilMHScan"] = {**{k: v for k, v in CHARACTERS["RamilMH"].items() if k not in ("footage", "capture_data", "face_basecolor")},
                             "scan": os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil", "MetaHumanScan1"),
                             # the scan's photo texture baked onto this face (bake_face_video.py with BAKE_SCAN)
                             "face_basecolor": os.path.join(ROOT, "SourceArt", "Character", "Models", "Ramil", "MetaHumanScan1", "Ramil_face_bc_scan.png"),
                             "copy_of": "RamilMH"}
# A second face for BikaMH (AShinobiPlayerCharacter::Faces): KeenFace's head from photos (Bika_keenface.zip unzipped to
# Bika/KeenFace: head.fbx already in MetaHuman head topology, 4K colour/normal/SRMF maps). A copy of MHC_BikaMH whose face the
# template stage replaces. Stages: template, cloud, photoskin, build (GUI).
KEENFACE_BIKA = os.path.join(ROOT, "SourceArt", "Character", "Models", "Bika", "KeenFace")
CHARACTERS["BikaMHKeenFace"] = {**{k: v for k, v in CHARACTERS["BikaMH"].items() if k not in ("footage", "capture_data", "face_basecolor")},
                                "template": os.path.join(KEENFACE_BIKA, "head.fbx"),
                                # The Creator overrides only base colour, normal and cavity: KeenFace's SRMF has no slot.
                                "face_textures": {"BASECOLOR": os.path.join(KEENFACE_BIKA, "texture", "Head_Color.jpg"),
                                                  "NORMAL": os.path.join(KEENFACE_BIKA, "texture", "Head_Normal.jpg")},
                                "copy_of": "BikaMH"}
CFG = CHARACTERS[NAME]
DEST = f"/Game/Shinobi/MetaHumans/{NAME}"
# SETUP_MH_QUALITY=LOW|MEDIUM|HIGH (Optimized pipeline); a non-default quality builds into Build_<Quality> beside Build.
# HIGH by default: a player is seen up close, and MEDIUM keeps only a 4k-vertex face (HIGH 19k) and bakes the brows and
# stubble into the skin.
QUALITY = os.environ.get("SETUP_MH_QUALITY", "HIGH").upper()
BUILD_DIR = "Build" if QUALITY == "HIGH" else f"Build_{QUALITY.title()}"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MH = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(message):
    unreal.log(f"[setup_metahuman] {message}")


def import_head_scan():
    fbx = os.path.join(CFG["scan"], f"{CFG['scan_name']}_head_mh.fbx")
    if not os.path.exists(fbx):
        raise RuntimeError(f"{fbx} missing: run Scripts/character/prep_mh_head.py")
    mesh = import_static_mesh(fbx, f"SM_{NAME}_HeadScan")
    texture_scan(mesh, fbx.replace(".fbx", "_diffuse.jpg"))
    return mesh


def import_static_mesh(fbx, name):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = fbx, DEST, name
    task.automated = task.save = task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{DEST}/{name}")
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"could not import {fbx}")
    return mesh


def texture_scan(mesh, jpg):
    """The scan's photo texture, unlit, on the head: what the Identity editor's face tracker looks at."""
    if not os.path.exists(jpg):
        return
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = jpg, DEST, f"T_{NAME}_HeadScan"
    task.automated = task.save = task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(f"{DEST}/T_{NAME}_HeadScan")
    path = f"{DEST}/M_{NAME}_HeadScan"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mel = unreal.MaterialEditingLibrary
    material = TOOLS.create_asset(f"M_{NAME}_HeadScan", DEST, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 0)
    sample.texture = texture
    mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    mesh.set_material(0, material)
    EAL.save_loaded_asset(mesh)


def character_asset():
    path = f"{DEST}/MHC_{NAME}"
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    if CFG.get("copy_of"):
        EAL.make_directory(DEST)
        other = CFG["copy_of"]
        copy = EAL.duplicate_asset(f"/Game/Shinobi/MetaHumans/{other}/MHC_{other}", path)
        if not copy:
            raise RuntimeError(f"could not copy MHC_{other}")
        return copy
    if CFG.get("base_preset"):  # a preset carries its sex's body and defaults; a blank character is male
        EAL.make_directory(DEST)
        preset = unreal.load_asset(f"/MetaHumanCharacter/Optional/Presets/{CFG['base_preset']}")
        copy = TOOLS.duplicate_asset(f"MHC_{NAME}", DEST, preset)
        if not copy:
            raise RuntimeError(f"could not copy preset {CFG['base_preset']}")
        return copy
    character = TOOLS.create_asset(asset_name=f"MHC_{NAME}", package_path=DEST, asset_class=unreal.MetaHumanCharacter,
                                   factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
    if character is None:
        raise RuntimeError(f"could not create {path}")
    return character


class Editing:
    """The subsystem's edit lock on a character, released however the block ends."""

    def __init__(self, character):
        self.character = character

    def __enter__(self):
        if not MH.try_add_object_to_edit(self.character):
            raise RuntimeError(f"{self.character.get_name()} is open for edit elsewhere (close the editor)")
        return self.character

    def __exit__(self, *exc):
        if MH.is_object_added_for_editing(self.character):
            MH.remove_object_to_edit(self.character)


def track_portrait(meta):
    size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(meta["portrait"])
    if size.x <= 0:
        raise RuntimeError(f"could not load {meta['portrait']}")
    result = MH.track_face_landmarks_from_image(pixels, size.x, size.y)
    if isinstance(result, tuple) and len(result) == 1:
        result = result[0]
    if not result or not hasattr(result, "items"):
        raise RuntimeError("no face landmarks on the portrait")
    log(f"tracked {len(result)} face contours on {os.path.basename(meta['portrait'])}")
    view = unreal.MinimalViewInfo()
    view.location = unreal.Vector(*meta["camera_location"])
    r = meta["camera_rotation"]
    view.rotation = unreal.Rotator(pitch=r["pitch"], yaw=r["yaw"], roll=r["roll"])
    view.fov = meta["fov"]
    view.aspect_ratio = 1.0
    view.projection_mode = unreal.CameraProjectionMode.PERSPECTIVE
    return result, view, size


def set_body(character):
    constraints = MH.get_body_constraints(character)
    by_name = {str(c.name).lower().replace(" ", "_"): c for c in constraints}
    log("body constraints: " + ", ".join(f"{k}={c.target_measurement:.1f}" for k, c in by_name.items()))
    for key, cm in CFG["body"].items():
        if key not in by_name:
            log(f"WARNING: no body constraint '{key}'")
            continue
        by_name[key].is_active = True
        by_name[key].target_measurement = cm
    MH.set_body_constraints(character, list(by_name.values()))
    MH.commit_body_state(character)
    after = {str(c.name).lower().replace(" ", "_"): c for c in MH.get_body_constraints(character)}
    log("body now: " + ", ".join(f"{k}={after[k].target_measurement:.1f}" for k in CFG["body"] if k in after))


def drop_copied_face(character):
    """The copied face's rig and its photo texture (baked for that face's shape) don't fit the new one: cloud re-rigs."""
    MH.remove_face_rig(character)
    skin = character.get_editor_property("skin_settings")
    over = skin.get_editor_property("texture_material_overrides")
    over.set_editor_property("enable_texture_overrides", False)
    skin.set_editor_property("texture_material_overrides", over)
    MH.commit_skin_settings(character, skin)


def template():
    """CFG["template"] (an FBX head in MetaHuman head topology, e.g. KeenFace's) as the character's face: Import > From Template
    (vertices matched by UV, scaling alignment, so the head keeps the character's size: a photo-made head has no real scale).
    GUI editor: the head goes to Saved/MetaHumanFit for the distance check. KeenFace Bika: 0.39 mm median from the FBX; a
    second fit keeping the high-frequency delta (as from_identity_detailed does) came out identical, so there is none."""
    head = import_static_mesh(CFG["template"], f"SM_{NAME}_Template")
    character = character_asset()
    with Editing(character):
        if CFG.get("copy_of"):
            drop_copied_face(character)
        else:
            MH.remove_face_rig(character)
        result = MH.import_from_template(character, head, None, None, None, unreal.ImportFromTemplateParams())
        log(f"import_from_template: {result}")
        if result != unreal.ImportErrorCode.SUCCESS:
            raise RuntimeError(f"import_from_template failed: {result}")
        export_head_fbx(character, "template")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"saved {character.get_path_name()}")


def conform():
    head = import_head_scan()
    with open(os.path.join(CFG["scan"], f"{CFG['scan_name']}_head_mh.json")) as f:
        meta = json.load(f)
    character = character_asset()
    with Editing(character):
        if CFG.get("copy_of"):
            drop_copied_face(character)
        curves, view, size = track_portrait(meta)
        verts, indices, *_ = MH.get_mesh_data_for_conforming(head)
        log(f"head scan: {len(verts)} verts, {len(indices) // 3} tris")
        params = unreal.ConformTargetParams()
        params.conform_target_mesh.target_parts_type = unreal.TargetPartsType.HEAD_ONLY
        params.conform_target_mesh.head_vertices = verts
        params.conform_target_mesh.head_vertex_indices = indices
        params.curve_tracking_points = curves
        params.camera_view_info = view
        params.image_size = size
        key = unreal.MetaHumanCharacterTargetMeshKey()
        key.head_mesh = head
        if not MH.align_to_target_meshes(character, key, params):
            raise RuntimeError("align_to_target_meshes failed")
        log("aligned")
        params.auto_solve = True
        params.body_conform_solve_settings.pipeline_name = "head_only"
        if not MH.conform_to_target_meshes(character, key, params):
            raise RuntimeError("conform_to_target_meshes failed")
        MH.commit_posed_state_as_a_pose(character, key)
        log("head conformed")
        set_body(character)
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"saved {character.get_path_name()}")


def preview():
    """Front and three-quarter renders of the character as it stands (run_python.ps1 -Render), next to the scan's portrait:
    SourceArt/.../MetaHuman/<Name>_preview_<view>.png."""
    character = character_asset()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    with Editing(character):
        mh = MH.spawn_meta_human_actor(character, True)
        # Shadowless lights (the lash cards' shadows hide the eyes): a key above the camera, shining toward -Y (the face
        # looks down +Y), and a dim one from the left for some modelling.
        light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-20, -90, 0))
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-10, -40, 0))
        fill.light_component.set_intensity(1.2)
        for l in (light, fill):
            l.light_component.set_cast_shadows(False)
        sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
        sky.light_component.set_intensity(1.0)
        capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 60, 170))
        comp = capture.capture_component2d
        rt = unreal.RenderingLibrary.create_render_target2d(world, 1024, 1024, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp.texture_target = rt
        comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        comp.fov_angle = 24.0
        meshes = mh.get_components_by_class(unreal.SkeletalMeshComponent)
        log("components: " + ", ".join(c.get_name() for c in meshes))
        body = next(c for c in meshes if c.does_socket_exist("head"))
        eye = body.get_socket_location("head") + unreal.Vector(0, 2, 9)  # the head bone is at the skull's base
        for view, (dx, dy) in {"front": (0, 80), "threequarter": (-55, 58)}.items():
            loc = unreal.Vector(eye.x + dx, eye.y + dy, eye.z)
            capture.set_actor_location_and_rotation(loc, unreal.MathLibrary.find_look_at_rotation(loc, eye), False, False)
            comp.capture_scene()  # materials still compiling render as the default grey: fine for judging the shape
            out = os.path.join(CFG["scan"], f"{NAME}_preview_{view}.png")
            unreal.RenderingLibrary.export_render_target(world, rt, os.path.dirname(out), os.path.basename(out))
            log(f"preview {out}")
        for a in (light, fill, sky, capture):
            actors.destroy_actor(a)


def body():
    """Just the body's measurements (conform does them too): for a face made in the Creator (e.g. Import > From Identity)."""
    character = character_asset()
    with Editing(character):
        set_body(character)
    EAL.save_loaded_asset(character, only_if_is_dirty=False)


def look():
    """Skin tone, eye colour and grooms (hair, brows, stubble, lashes) from CHARACTERS."""
    character = character_asset()
    with Editing(character):
        skin = character.get_editor_property("skin_settings")
        for key, value in CFG["skin"].items():
            setattr(skin.skin, key, value)
        MH.commit_skin_settings(character, skin)
        eyes = character.get_editor_property("eyes_settings")
        for eye in (eyes.eye_left, eyes.eye_right):
            for key, value in CFG["iris"].items():
                # global_tint is the iris brightness (a preset's dark brown eye has ~0.1-0.2, a hazel one 1.0)
                setattr(eye.iris, key, unreal.LinearColor(value, value, value, 1.0) if key == "global_tint" else value)
        MH.commit_eyes_settings(character, eyes)
        collection = character.internal_collection
        instance = collection.default_instance
        keys = {}
        for slot, (item, _) in CFG["grooms"].items():
            path = f"/MetaHumanCharacter/Optional/Grooms/Bindings/{item}"
            wardrobe = unreal.load_asset(f"{path}.{path.rsplit('/', 1)[1]}")
            if not wardrobe:
                raise RuntimeError(f"no wardrobe item {path}")
            existing = collection.get_item_keys_for_wardrobe_item(wardrobe)
            keys[slot] = existing[0] if existing else collection.try_add_item_from_wardrobe_item(slot, wardrobe)
            instance.set_single_slot_selection(slot, keys[slot])  # replaces an earlier pick
            log(f"{slot}: {item.rsplit('/', 1)[1]}")
        if CFG.get("no_outfit"):
            # A preset comes dressed (default T-shirt and shorts, sized to its own body: they don't fit a re-measured one).
            # Without them the body's own underwear shows, as on a blank character (RamilMH).
            instance.set_single_slot_selection("Outfits", unreal.MetaHumanPaletteItemKey())
            log("Outfits: " + ", ".join(str(d.selection.slot_name) for d in instance.get_slot_selection_data() if str(d.selection.slot_name) == "Outfits" and d.selection.selected_item.references_external_wardrobe_item) + " (empty = cleared)")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"look saved on {character.get_name()}")


def retarget():
    """IK_<Name> on the built body and RTG_MannyTo<Name>, the retargeter UShinobiBodyComponent copies the hidden Manny's
    pose through (as setup_hero.py's build_retargeter does for the Mixamo bodies). The MetaHuman body has Manny's bone
    names and hands, so the chains map exactly and there is no arm spread or hand roll to fix."""
    body = unreal.load_asset(f"{DEST}/Build/MHC_{NAME}/Body/SKM_MHC_{NAME}_BodyMesh")
    ik_manny = unreal.load_asset("/Game/Shinobi/Retarget/IK_Manny")
    if not body or not ik_manny:
        raise RuntimeError("build the MetaHuman first (and Scripts/setup_chakra_pose.py for IK_Manny)")
    rig_name, rtg_name = f"IK_{NAME}", f"RTG_MannyTo{NAME}"
    rig = unreal.load_asset(f"{DEST}/{rig_name}") or unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(DEST, rig_name)
    rig_ctl = unreal.IKRigController.get_controller(rig)
    if not rig_ctl.set_skeletal_mesh(body) or not rig_ctl.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"{rig_name}: auto-characterisation failed")
    EAL.save_loaded_asset(rig)
    rtg = unreal.load_asset(f"{DEST}/{rtg_name}") or TOOLS.create_asset(rtg_name, DEST, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctl = unreal.IKRetargeterController.get_controller(rtg)
    source, target = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    ctl.set_ik_rig(source, ik_manny)
    ctl.set_ik_rig(target, rig)
    ctl.remove_all_ops()
    ctl.add_default_ops()
    ctl.assign_ik_rig_to_all_ops(source, ik_manny)
    ctl.assign_ik_rig_to_all_ops(target, rig)
    ctl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    ctl.reset_retarget_pose(ctl.get_current_retarget_pose_name(target), [], target)
    ctl.auto_align_all_bones(target, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    for index in range(ctl.get_num_retarget_ops()):
        if isinstance(ctl.get_op_controller(index), (unreal.IKRetargetRunIKRigController, unreal.IKRetargetRootMotionController)):
            ctl.set_retarget_op_enabled(index, False)
    missing = [c for c in ("Spine", "Neck", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg") if not str(ctl.get_source_chain(c))]
    if missing:
        raise RuntimeError(f"{rtg_name}: no Manny chain for {missing}")
    EAL.save_loaded_asset(rtg)
    log(f"built {rtg.get_path_name()}")


def groom_colours():
    """hairMelanin on the built grooms' material instances (Build/MHC_<Name>/Grooms/MI_WI_<item>_*). The Creator keeps it as a
    wardrobe instance parameter, but those only exist on a collection assembled by editor ticks, which scripts don't get."""
    folder = f"{DEST}/{BUILD_DIR}/MHC_{NAME}/Grooms"
    mel = unreal.MaterialEditingLibrary
    for slot, (item, melanin) in CFG["grooms"].items():
        short = item.rsplit("/", 1)[1]  # WI_Hair_S_SideSweptFringe
        for path in EAL.list_assets(folder, recursive=False):
            if not path.rsplit("/", 1)[1].startswith(f"MI_{short}"):
                continue
            mi = unreal.load_asset(path)
            names = [str(n) for n in mel.get_scalar_parameter_names(mi)] if isinstance(mi, unreal.MaterialInstanceConstant) else []
            if "hairMelanin" in names:
                mel.set_material_instance_scalar_parameter_value(mi, "hairMelanin", melanin)
                EAL.save_loaded_asset(mi)
                log(f"{slot}: {mi.get_name()} melanin {melanin}")


def cloud():
    """Epic's MetaHuman cloud service (needs the editor signed in to an Epic account): the face rig (joints
    only) and the high-resolution skin textures."""
    character = character_asset()
    with Editing(character):
        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking, rig.report_progress = True, False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY  # no facial animation in the game yet; blendshapes cost memory
        MH.request_auto_rigging(character, rig)
        log("auto-rig requested")
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking, tex.report_progress = True, False
        MH.request_texture_sources(character, tex)
        log("texture sources requested")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)


def run_build(character, params):
    """build_meta_human, tolerating the errors it logs while copying the Common face Control Rigs ("Cannot break link
    ... RerouteNode"), which Python raises once the call returns; the build itself is checked by its blueprint."""
    try:
        MH.build_meta_human(character=character, params=params)
    except RuntimeError as error:
        if "Cannot break link" not in str(error):
            raise
        log("build logged Control Rig relink errors (benign)")
    blueprint = f"{params.absolute_build_path}/{character.get_name()}/BP_{character.get_name()}"
    if not EAL.does_asset_exist(blueprint):
        raise RuntimeError(f"build made no {blueprint}")
    log(f"built {blueprint}")


def export_head_fbx(character, tag):
    """The character's current head (export_geometry, GUI only) as an FBX in Saved/MetaHumanFit/<Name>_<tag>.fbx."""
    folder = f"{DEST}/FitCheck/{tag}"
    geo = unreal.MetaHumanGeometryExportParams()
    geo.project_path = folder
    geo.head_skeletal_mesh = True
    geo.body_skeletal_mesh = geo.full_body_skeletal_mesh = False
    geo.overwrite_existing_assets = True
    unreal.MetaHumanCharacterExportBlueprintLibrary.export_geometry(character, geo)
    mesh = next(unreal.load_asset(a) for a in EAL.list_assets(folder, recursive=True)
                if isinstance(unreal.load_asset(a), unreal.SkeletalMesh))
    out = os.path.join(ROOT, "Saved", "MetaHumanFit", f"{NAME}_{tag}.fbx")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    task = unreal.AssetExportTask()
    task.object, task.filename, task.automated, task.prompt, task.replace_identical = mesh, out, True, False, True
    task.exporter = unreal.SkeletalMeshExporterFBX()
    task.options = unreal.FbxExportOption()
    task.options.level_of_detail = False
    unreal.Exporter.run_asset_export_task(task)
    log(f"fitcheck {tag} -> {out}")


def fitcheck():
    """Diagnostics (GUI editor): fresh characters conformed to the scan in several ways, each head exported for a distance
    check against the scan (Saved/MetaHumanFit). SETUP_MH_FIT picks variants (comma list); default all."""
    head = import_head_scan()  # whatever prep_mh_head.py wrote last
    with open(os.path.join(CFG["scan"], f"{CFG['scan_name']}_head_mh.json")) as f:
        meta = json.load(f)
    verts, indices, *_ = MH.get_mesh_data_for_conforming(head)
    curves, view, size = track_portrait(meta)
    key = unreal.MetaHumanCharacterTargetMeshKey()
    key.head_mesh = head

    def schedule(value):
        w = unreal.WeightSchedule()
        w.start = w.end = value
        return w

    def params(with_curves, solve, tune=None):
        p = unreal.ConformTargetParams()
        p.conform_target_mesh.target_parts_type = unreal.TargetPartsType.HEAD_ONLY
        p.conform_target_mesh.head_vertices = verts
        p.conform_target_mesh.head_vertex_indices = indices
        if with_curves:
            p.curve_tracking_points, p.camera_view_info, p.image_size = curves, view, size
        if solve:
            manual = (tune or {}).pop("manual", False) if tune else False
            p.auto_solve = not manual  # the auto pipeline uses its own preset weights; the manual face step reads these
            p.body_conform_solve_settings.pipeline_name = "head_only"
            if manual:
                p.body_conform_solve_settings.iterations = 0  # the Creator's "face step": no body solve
            for key, value in (tune or {}).items():
                st = p.body_conform_solve_settings
                setattr(st, key, value if isinstance(value, int) else schedule(value))
        return p

    # (align with curves?, [conform passes: with curves?], solver overrides)
    variants = {"curves": (True, [True], {}), "meshonly": (False, [False], {}), "curves_then_mesh": (True, [True, False], {}),
                "tA": (True, [True], {"face_icp_weight": 100.0, "model_regularization": 3.0}),
                "tB": (True, [True], {"face_icp_weight": 150.0, "model_regularization": 1.0, "face_iterations": 14}),
                "tC": (True, [True], {"face_icp_weight": 300.0, "model_regularization": 0.3, "face_iterations": 20}),
                "mD": (True, [True], {"manual": True}),
                "mE": (True, [True], {"manual": True, "face_icp_weight": 100.0, "model_regularization": 3.0, "face_iterations": 12}),
                "mF": (True, [True], {"manual": True, "face_icp_weight": 300.0, "model_regularization": 0.5, "face_iterations": 20})}
    wanted = [v for v in os.environ.get("SETUP_MH_FIT", "").split(",") if v] or list(variants)
    for tag in wanted:
        align_curves, passes, tune = variants[tag]
        tune = dict(tune)
        path = f"{DEST}/FitCheck/MHC_{tag}"
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        character = TOOLS.create_asset(asset_name=f"MHC_{tag}", package_path=f"{DEST}/FitCheck", asset_class=unreal.MetaHumanCharacter,
                                       factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
        with Editing(character):
            MH.align_to_target_meshes(character, key, params(align_curves, False))
            for with_curves in passes:
                log(f"fitcheck {tag}: pass curves={with_curves} -> {MH.conform_to_target_meshes(character, key, params(with_curves, True, dict(tune)))}")
            MH.commit_posed_state_as_a_pose(character, key)
            export_head_fbx(character, os.environ.get("SETUP_MH_FIT_TAG", "") + tag)


def identity():
    """Mesh to MetaHuman through a MetaHuman Identity (GUI editor): the Identity's face conform wraps its template onto the
    scan, then import_from_identity gives a fresh FitCheck character that face, exported for the distance check."""
    head = import_head_scan()
    with open(os.path.join(CFG["scan"], f"{CFG['scan_name']}_head_mh.json")) as f:
        meta = json.load(f)
    folder = f"{DEST}/FitCheck"
    for name in ("CD_HeadScan", "MHI_HeadScan", "MHC_identity"):
        if EAL.does_asset_exist(f"{folder}/{name}"):
            EAL.delete_asset(f"{folder}/{name}")
    capture = TOOLS.create_asset("CD_HeadScan", folder, unreal.MeshCaptureData, unreal.MeshCaptureDataFactory())
    capture.set_editor_property("target_mesh", head)
    ident = TOOLS.create_asset("MHI_HeadScan", folder, unreal.MetaHumanIdentity, unreal.MetaHumanIdentityFactoryNew())
    face = ident.get_or_create_part_of_class(unreal.MetaHumanIdentityFace)
    pose = unreal.new_object(type=unreal.MetaHumanIdentityPose, outer=face)
    face.add_pose_of_type(unreal.IdentityPoseType.NEUTRAL, pose)
    pose.set_capture_data(capture)
    pose.load_default_tracker()
    # The Identity editor initialises a new frame's contour markers (InitializeMarkersFromParsedConfig, not scriptable)
    # when it sees the frame added: have it open.
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([ident])
    frame, _ = pose.add_new_promoted_frame()
    log(f"identity frame {frame.get_class().get_name()}")
    loc, r = meta["camera_location"], meta["camera_rotation"]
    frame.set_editor_property("view_location", unreal.Vector(*loc))
    frame.set_editor_property("view_rotation", unreal.Rotator(pitch=r["pitch"], yaw=r["yaw"], roll=r["roll"]))
    verts, *_ = MH.get_mesh_data_for_conforming(head)
    centre = unreal.Vector(sum(v.x for v in verts) / len(verts), sum(v.y for v in verts) / len(verts), sum(v.z for v in verts) / len(verts))
    frame.set_editor_property("look_at_location", centre)
    frame.set_editor_property("camera_view_fov", meta["fov"])
    frame.set_editor_property("is_front_view", True)
    frame.set_navigation_locked(True)
    size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(meta["portrait"])
    ident.set_blocking_processing(True)
    ident.start_frame_tracking_pipeline(pixels, size.x, size.y, "", pose, frame, False)
    log(f"identity tracked: contours active {frame.frame_contours_contain_active_data()}")
    result = face.conform()
    log(f"identity conform: {result}")
    character = TOOLS.create_asset(asset_name="MHC_identity", package_path=folder, asset_class=unreal.MetaHumanCharacter,
                                   factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
    with Editing(character):
        params = unreal.ImportFromIdentityParams()
        params.use_eye_meshes = True
        params.use_teeth_mesh = True
        params.use_metric_scale = True
        log(f"import_from_identity: {MH.import_from_identity(character, ident, params)}")
        export_head_fbx(character, "identity")
    EAL.save_loaded_asset(ident)


def export_mesh_fbx(mesh, tag):
    out = os.path.join(ROOT, "Saved", "MetaHumanFit", f"{NAME}_{tag}.fbx")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    task = unreal.AssetExportTask()
    task.object, task.filename, task.automated, task.prompt, task.replace_identical = mesh, out, True, False, True
    task.exporter = unreal.SkeletalMeshExporterFBX()
    task.options = unreal.FbxExportOption()
    task.options.level_of_detail = False
    unreal.Exporter.run_asset_export_task(task)
    log(f"measure {tag} -> {out}")


def measure():
    """The character's current head (and any MetaHuman Identity head beside it) as FBX in Saved/MetaHumanFit, for the
    distance check against the scan (GUI editor: export_geometry needs it)."""
    character = character_asset()
    with Editing(character):
        export_head_fbx(character, "current")
    identity_head = unreal.load_asset(f"{DEST}/SK_MHI_{CFG['scan_name']}")
    if identity_head:
        export_mesh_fbx(identity_head, "identity_head")


def footage():
    """The face from a Live Link Face take (GUI editor): Epic's MetaHuman Animator scripts ingest it as capture data and make
    a MetaHuman Identity from the neutral frame (tracked contours + TrueDepth depth, then the Identity's face conform);
    the character's face rig comes off and the Identity's face goes on at its real (metric) size."""
    import sys
    sys.path.append("C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/MetaHuman/MetaHumanAnimator/Content/Python")
    import create_capture_data
    if EAL.does_asset_exist(CFG.get("capture_data", "")):
        captures = [CFG["capture_data"]]
    else:
        captures = create_capture_data.import_take_data_for_specified_device(
            footage_path=CFG["footage"], using_LLF_data=True, storage_path=f"{DEST}/Footage/")
    if not captures:
        raise RuntimeError(f"no take ingested from {CFG['footage']}")
    log(f"capture data {captures}")
    ident = identity_from_frame(unreal.load_asset(captures[0]), CFG["neutral_frame"], f"MHI_{NAME}_Footage",
                                CFG.get("side_frames", []))
    # The conformed face isn't kept when the Identity is saved and reloaded: use it in this session.
    from_identity_detailed(ident)


def identity_from_frame(capture, frame_number, name, side_frames=()):
    """Epic's create_identity_for_performance.create_identity_from_frame, but without the eye fit (on this take it threw a
    spike out of the left eye) and returning the Identity once its face is conformed."""
    path = f"{DEST}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    ident = TOOLS.create_asset(name, DEST, unreal.MetaHumanIdentity, unreal.MetaHumanIdentityFactoryNew())
    face = ident.get_or_create_part_of_class(unreal.MetaHumanIdentityFace)
    pose = unreal.new_object(type=unreal.MetaHumanIdentityPose, outer=face)
    face.add_pose_of_type(unreal.IdentityPoseType.NEUTRAL, pose)
    pose.set_capture_data(capture)
    pose.fit_eyes = False
    pose.load_default_tracker()
    ident.set_blocking_processing(True)
    camera = pose.get_editor_property("camera")
    for number in [frame_number, *side_frames]:  # the front view first
        frame, _ = pose.add_new_promoted_frame()
        frame.is_front_view = number == frame_number
        frame.set_navigation_locked(True)
        frame.frame_number = number
        if not unreal.PromotedFrameUtils.initialize_contour_data_for_footage_frame(pose, frame):
            raise RuntimeError(f"no contour data for frame {number}")
        image = unreal.PromotedFrameUtils.get_image_path_for_frame(capture, camera, number, True, pose.timecode_alignment)
        depth = unreal.PromotedFrameUtils.get_image_path_for_frame(capture, camera, number, False, pose.timecode_alignment)
        size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(image)
        ident.start_frame_tracking_pipeline(pixels, size.x, size.y, depth, pose, frame, False)
    result = face.conform()
    log(f"identity conform (frames {[frame_number, *side_frames]}, no eye fit): {result}")
    if not unreal.MetaHumanIdentity.handle_error(result, True):
        raise RuntimeError(f"identity conform failed: {result}")
    EAL.save_loaded_asset(ident)
    return ident


def from_identity_detailed(ident):
    """The Identity's conformed head onto the character, keeping its detail: Import > From Identity fits the Creator's face
    model with the high-frequency delta disabled (only the broad shape survives); this exports the conformed template head
    (MetaHuman topology) and fits to its vertices with that delta kept, at the Identity's real scale."""
    face = ident.find_part_of_class(unreal.MetaHumanIdentityFace)
    tpl = f"{DEST}/SM_{NAME}_IdentityHead"
    if EAL.does_asset_exist(tpl):
        EAL.delete_asset(tpl)
    face.export_template_mesh(tpl, f"SM_{NAME}_IdentityHead")
    head = unreal.load_asset(tpl)
    verts, *_ = MH.get_mesh_data_for_conforming(head)
    log(f"identity head: {len(verts)} vertices")
    character = character_asset()
    with Editing(character):
        MH.remove_face_rig(character)
        fit = unreal.MetaHumanCharacterFitToVerticesParams()
        opts = unreal.FitToTargetOptions()
        opts.alignment_options = unreal.MetaHumanAlignmentOptions.ROTATION_TRANSLATION  # keep the metric size
        opts.disable_high_frequency_delta = False
        opts.adapt_neck = True
        fit.options = opts
        fit.head_vertices = [unreal.Vector(v.x, v.y, v.z) for v in verts]
        ok = MH.fit_state_to_target_vertices(character=character, params=fit)
        log(f"detailed fit: {ok}")
        if not ok:
            raise RuntimeError("fit_state_to_target_vertices failed (vertex order/count not the MetaHuman head's?)")
        MH.commit_face_state(character)
        export_head_fbx(character, "footage_detailed")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)


def from_identity(ident=None):
    """Import > From Identity onto the character (face rig removed first), then its head exported for checking."""
    ident = ident or unreal.load_asset(f"{DEST}/MHI_{NAME}_Footage")
    character = character_asset()
    with Editing(character):
        MH.remove_face_rig(character)
        params = unreal.ImportFromIdentityParams()
        params.use_eye_meshes = False  # the footage eye fit came out wrong (a spike from the left eye): MetaHuman's eyes
        params.use_teeth_mesh = False  # no teeth pose in the Identity
        params.use_metric_scale = True  # TrueDepth depth is metric: keep the real head size
        result = MH.import_from_identity(character, ident, params)
        log(f"import_from_identity: {result}")
        if result != unreal.ImportErrorCode.SUCCESS:
            raise RuntimeError(f"import_from_identity failed: {result}")
        export_head_fbx(character, "footage")
    EAL.save_loaded_asset(character, only_if_is_dirty=False)


def hair():
    """Only the Hair slot swapped to CHARACTERS' pick, leaving every other Creator choice as it is (the look stage would
    reset skin, eyes and all grooms). Run build with SETUP_MH_GROOM_COLOURS=1 after, to colour the new groom."""
    character = character_asset()
    item = CFG["grooms"]["Hair"][0]
    path = item if item.startswith("/") else f"/MetaHumanCharacter/Optional/Grooms/Bindings/{item}"
    wardrobe = unreal.load_asset(f"{path}.{path.rsplit('/', 1)[1]}")
    if not wardrobe:
        raise RuntimeError(f"no wardrobe item {path}")
    collection = character.internal_collection
    existing = collection.get_item_keys_for_wardrobe_item(wardrobe)
    key = existing[0] if existing else collection.try_add_item_from_wardrobe_item("Hair", wardrobe)
    collection.default_instance.set_single_slot_selection("Hair", key)
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log(f"Hair: {item}")


def fab_hair():
    """A Fab groom (CHARACTERS' Hair is a path, not a bundled item) put in the built blueprint's Hair by hand: the Optimized
    build drops it. Its OptimizeGroom keeps the cards of LODs 1 and 3 only, and a Fab sample has a single LOD (strands +
    cards at LOD0), so the blueprint's Hair comes out empty. Bound to the built face, coloured by a copy of its strands
    material, and left out of the LODSync (whose face-to-hair mapping names LODs this groom doesn't have)."""
    item = CFG["grooms"]["Hair"][0]
    if not item.startswith("/"):
        return
    folder = item.rsplit("/", 1)[0]
    short = item.rsplit("/", 1)[1][len("WI_"):]  # SC_Hairstyle_Male_11
    groom = unreal.load_asset(f"{folder}/{short}/{short}")
    source = unreal.load_asset(f"{folder}/{short}/GroomMesh/SKM_MH_Groom_Head")  # the head the sample's strands were made on
    build = f"{DEST}/{BUILD_DIR}/MHC_{NAME}"
    face = unreal.load_asset(f"{build}/Face/SKM_MHC_{NAME}_FaceMesh")
    slots = [(str(m.material_slot_name), m.material_interface.get_name() if m.material_interface else "") for m in face.get_editor_property("materials")]
    skin = next(i for i, (_, mi) in enumerate(slots) if "Skin" in mi and "LOD1" in mi)  # the head's skin section (the bake's head_LOD1 slot)
    binding_path = f"{DEST}/GB_{NAME}_FabHair"
    if EAL.does_asset_exist(binding_path):
        EAL.delete_asset(binding_path)  # rebound to the face each build: the face mesh changes
    binding = unreal.GroomLibrary.create_new_groom_binding_asset_with_path(binding_path, groom, face, 100, source, skin)
    EAL.save_loaded_asset(binding)
    # The sample's cards are untextured placeholders (its one texture is the Fab thumbnail): it is drawn as strands, with
    # the Creator's colour on a copy of its strands material (the sample's MI_Hair is MetaHuman's default brown).
    mats = groom.get_editor_property("hair_groups_materials")
    strands_slot = next(i for i, m in enumerate(mats) if m.get_editor_property("material").get_name() == "MI_Hair")
    mi_path = f"{DEST}/MI_{NAME}_FabHair"
    mi = unreal.load_asset(mi_path) or TOOLS.create_asset(mi_path.rsplit("/", 1)[1], DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, mats[strands_slot].get_editor_property("material"))
    for name, value in (("hairMelanin", CFG["grooms"]["Hair"][1]), ("hairRedness", CFG.get("groom_redness"))):
        if value is not None:
            mel.set_material_instance_scalar_parameter_value(mi, name, value)
    EAL.save_loaded_asset(mi)
    bp = unreal.load_asset(f"{build}/BP_MHC_{NAME}")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    templates = {str(lib.get_variable_name(lib.get_data(h))): lib.get_associated_object(lib.get_data(h)) for h in sds.k2_gather_subobject_data_for_blueprint(bp)}
    hair_component = templates["Hair"]
    hair_component.set_editor_property("groom_asset", groom)
    hair_component.set_editor_property("binding_asset", binding)
    overrides = [None] * len(mats)
    overrides[strands_slot] = mi
    hair_component.set_editor_property("override_materials", overrides)
    hair_component.set_editor_property("component_tags", ["ShinobiStrands"])  # UShinobiBodyComponent: don't switch it to cards
    sync = next(t for t in templates.values() if isinstance(t, unreal.LODSyncComponent))
    entries = list(sync.get_editor_property("components_to_sync"))
    for entry in entries:
        if str(entry.get_editor_property("name")) == "Hair":
            entry.set_editor_property("sync_option", unreal.SyncOption.DISABLED)
    sync.set_editor_property("components_to_sync", entries)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, only_if_is_dirty=False)
    log(f"Hair: {groom.get_name()} bound to {face.get_name()} (section {skin}), out of the LODSync: {[str(e.get_editor_property('name')) for e in entries]}")


def strand_hair():
    """CFG["hair_strands"]: a bundled Hair and Eyebrows drawn as strands. The Optimized build keeps their cards for LODs 1 and 3
    only, so UShinobiBodyComponent's cards-everywhere (bUseCards) leaves the face LOD0 close-ups bald and browless; the
    ShinobiStrands tag keeps strands (and pins the face to LOD0, as for a Fab hair)."""
    if not CFG.get("hair_strands"):
        return
    bp = unreal.load_asset(f"{DEST}/{BUILD_DIR}/MHC_{NAME}/BP_MHC_{NAME}")
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    templates = {str(lib.get_variable_name(lib.get_data(h))): lib.get_associated_object(lib.get_data(h)) for h in sds.k2_gather_subobject_data_for_blueprint(bp)}
    names = [n for n in ("Hair", "Eyebrows") if n in templates]
    for name in names:
        templates[name].set_editor_property("component_tags", ["ShinobiStrands"])
    sync = next(t for t in templates.values() if isinstance(t, unreal.LODSyncComponent))  # their card LODs follow the body's
    entries = list(sync.get_editor_property("components_to_sync"))
    for entry in entries:
        if str(entry.get_editor_property("name")) in names:
            entry.set_editor_property("sync_option", unreal.SyncOption.DISABLED)
    sync.set_editor_property("components_to_sync", entries)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, only_if_is_dirty=False)
    log(f"{', '.join(names)} tagged ShinobiStrands")


def body_hide():
    """The built body's skin material takes a HideMask (white by default): wearing an outfit, the game sets it to the
    outfit's hide mask for this body (AShinobiPlayerCharacter::DressOutfit), so the skin under the cloak isn't drawn.
    MetaHuman's skin material has no opacity, so the body's instance is moved onto a masked copy of it (with a copy of the
    Common instance between them, which the face keeps sharing). Run after every build: it remakes the body's instance."""
    mel = unreal.MaterialEditingLibrary
    body = unreal.load_asset(f"{DEST}/{BUILD_DIR}/MHC_{NAME}/Body/SKM_MHC_{NAME}_BodyMesh")
    mi = body.get_editor_property("materials")[0].material_interface
    common = mi.get_editor_property("parent")
    if common.get_path_name().startswith(DEST):
        log(f"body hide mask: {mi.get_name()} is already on {common.get_name()}")
        return
    base = common.get_editor_property("parent") if isinstance(common, unreal.MaterialInstance) else common
    material_path, parent_path = f"{DEST}/M_{NAME}_BodySkin", f"{DEST}/MI_{NAME}_BodySkin"
    for path in (parent_path, material_path):
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
    material = EAL.duplicate_asset(base.get_path_name(), material_path)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mask = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 900)
    mask.set_editor_property("parameter_name", "HideMask")
    mask.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)  # as the hide masks import
    if mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK) is not None:
        raise RuntimeError(f"{base.get_name()} already has an opacity mask: multiply HideMask into it")
    if not mel.connect_material_property(mask, "R", unreal.MaterialProperty.MP_OPACITY_MASK):
        raise RuntimeError("could not wire HideMask into the body skin's opacity mask")
    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    parent = EAL.duplicate_asset(common.get_path_name(), parent_path)
    mel.set_material_instance_parent(parent, material)
    EAL.save_loaded_asset(parent)
    mel.set_material_instance_parent(mi, parent)
    EAL.save_loaded_asset(mi)
    log(f"body hide mask: {mi.get_name()} -> {parent.get_name()} -> {material.get_name()} (masked copy of {base.get_name()})")


def bake_inputs():
    """The built face mesh (FBX, LOD0) and its base colour into BAKE_DIR, for Scripts/character/bake_face_video.py. The
    base colour is the fill around the photo patch: export it before photoskin (or with the override off)."""
    out = os.environ.get("BAKE_DIR", os.path.join(os.path.dirname(CFG["face_basecolor"]), "bake"))
    os.makedirs(out, exist_ok=True)
    face = f"{DEST}/{BUILD_DIR}/MHC_{NAME}/Face"
    for asset, name, exporter in ((f"{face}/SKM_MHC_{NAME}_FaceMesh", "face.fbx", unreal.SkeletalMeshExporterFBX()),
                                  (f"{face}/Baked/T_Head_LOD1_BC_VT", "base_bc.png", unreal.TextureExporterPNG())):
        task = unreal.AssetExportTask()
        task.object, task.filename, task.exporter = unreal.load_asset(asset), os.path.join(out, name), exporter
        task.automated = task.replace_identical = True
        task.prompt = False
        if name.endswith(".fbx"):
            task.options = unreal.FbxExportOption()
            task.options.level_of_detail = False  # LOD0 only: the LODs overlap in one FBX
        log(f"{name}: {unreal.Exporter.run_asset_export_task(task)}")


def photo_skin():
    """The face's texture overrides (skin settings): CFG face_textures (FaceTextureType name -> image), else the baked video
    face (face_basecolor) as the base colour."""
    sources = CFG.get("face_textures") or {"BASECOLOR": CFG["face_basecolor"]}
    imported = {}
    for kind, image in sources.items():
        name = f"T_{NAME}_Face_{'BC' if kind == 'BASECOLOR' else kind.title()}"
        task = unreal.AssetImportTask()
        task.filename, task.destination_path, task.destination_name = image, DEST, name
        task.automated = task.save = task.replace_existing = True
        TOOLS.import_asset_tasks([task])
        tex = unreal.load_asset(f"{DEST}/{name}")
        tex.set_editor_property("srgb", kind == "BASECOLOR")
        if kind == "NORMAL":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        EAL.save_loaded_asset(tex)
        imported[getattr(unreal.FaceTextureType, kind)] = tex
    character = character_asset()
    with Editing(character):
        skin = character.get_editor_property("skin_settings")
        over = skin.get_editor_property("texture_material_overrides")
        over.set_editor_property("enable_texture_overrides", True)
        textures = over.get_editor_property("texture_overrides")
        face = dict(textures.get_editor_property("face"))
        face.update(imported)
        textures.set_editor_property("face", face)
        over.set_editor_property("texture_overrides", textures)
        skin.set_editor_property("texture_material_overrides", over)
        MH.commit_skin_settings(character, skin)
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log("face texture overrides: " + ", ".join(t.get_path_name() for t in imported.values()))


def clear_skin():
    """The face base-colour override off, so the next build's base colour (bakeinputs) is the Creator's own, not the last bake."""
    character = character_asset()
    with Editing(character):
        skin = character.get_editor_property("skin_settings")
        over = skin.get_editor_property("texture_material_overrides")
        over.set_editor_property("enable_texture_overrides", False)
        skin.set_editor_property("texture_material_overrides", over)
        MH.commit_skin_settings(character, skin)
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    log("face texture overrides off")


PENDING = []  # a GUI build finishing over editor ticks: the editor quits when it is done


def build_params():
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = getattr(unreal.MetaHumanQualityLevel, QUALITY)
    params.absolute_build_path = f"{DEST}/{BUILD_DIR}"
    params.common_folder_path = "/Game/Shinobi/MetaHumans/Common"
    params.enable_wardrobe_item_validation = False
    return params


def set_groom_melanin(character):
    """Melanin as the Creator keeps it, on each groom's wardrobe instance parameters: the Medium build bakes the brows
    and stubble into the face's textures with it. False until the assembled collection has the parameters."""
    preview = MH.get_preview_collection(character)  # what assemble_for_preview builds, synced back by on_edit_preview_collection
    instance = preview.default_instance if preview else None
    if not instance:
        return False
    found = False
    for data in instance.get_slot_selection_data():
        slot = str(data.selection.slot_name)
        if slot not in CFG["grooms"]:
            continue
        params = instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=data.selection.selected_item))
        by_name = {str(p.name): p for p in params}
        if "Melanin" in by_name:
            by_name["Melanin"].set_float(value=CFG["grooms"][slot][1])
            found = True
            for name, key in (("Redness", "groom_redness"), ("Lightness", "groom_lightness")):
                if name in by_name and key in CFG:
                    by_name[name].set_float(value=CFG[key])
    if found:
        MH.on_edit_preview_collection(character)
    return found


def finish_build(character):
    run_build(character, build_params())
    EAL.save_loaded_asset(character, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=False, save_content_packages=True)
    if SCRIPT_GROOM_COLOURS:
        groom_colours()  # the hair's own material instances
    fab_hair()
    strand_hair()
    body_hide()


# The grooms' colours belong to the Creator (Hair & Clothing > Details > Hair Color / Lightness) once someone has set them
# there; SETUP_MH_GROOM_COLOURS=1 makes the build apply CHARACTERS' melanin instead (and overwrite theirs).
SCRIPT_GROOM_COLOURS = os.environ.get("SETUP_MH_GROOM_COLOURS") == "1"


def build():
    """The Optimized, High-quality assembly into DEST/Build (blueprint, body, face, grooms, materials). GUI editor only
    (run_metahuman_gui.ps1): its texture bakes run on TextureGraph, and the grooms' melanin (baked into the face) can only
    be set once the preview collection has assembled, which takes editor ticks, so the build finishes from a tick callback."""
    character = character_asset()
    if not MH.try_add_object_to_edit(character):
        raise RuntimeError(f"{character.get_name()} is open for edit elsewhere")
    log(f"can build: {MH.can_build_meta_human(character)}")
    if not SCRIPT_GROOM_COLOURS:
        try:
            finish_build(character)
        finally:
            MH.remove_object_to_edit(character)
        return
    MH.assemble_for_preview(character=character)
    ticks = [0]

    def tick(_delta):
        ticks[0] += 1
        ready = ticks[0] > 900 or (ticks[0] % 30 == 0 and set_groom_melanin(character))
        if not ready:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        try:
            log("groom melanin set" if ticks[0] <= 900 else "WARNING: grooms never assembled: built without their melanin")
            finish_build(character)
        finally:
            MH.remove_object_to_edit(character)
            PENDING.clear()
            unreal.SystemLibrary.quit_editor()

    handle = unreal.register_slate_post_tick_callback(tick)
    PENDING.append(handle)


STAGES = {"scan": import_head_scan, "conform": conform, "body": body, "look": look, "cloud": cloud, "build": build, "colours": groom_colours, "retarget": retarget, "preview": preview, "fitcheck": fitcheck, "identity": identity, "measure": measure, "footage": footage, "template": template, "fromidentity": from_identity, "photoskin": photo_skin, "clearskin": clear_skin, "hair": hair, "fabhair": fab_hair, "strandhair": strand_hair, "bakeinputs": bake_inputs, "bodyhide": body_hide}
try:
    for stage in ONLY or list(STAGES):
        STAGES[stage]()
finally:
    if os.environ.get("SETUP_MH_QUIT") and not PENDING:  # run_metahuman_gui.ps1: the GUI editor closes itself afterwards
        unreal.SystemLibrary.quit_editor()
