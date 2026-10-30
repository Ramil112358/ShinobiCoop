"""CO_Shinobi's content: the Mutable character customisation (docs/superpowers/specs/2026-10-29-mutable-customisation-design.md).

  Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py            (every stage, in order)
  $env:SETUP_MUTABLE_ONLY="graph"; Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py

(-Editor: the Customizable Object factory needs Slate.) The content stays local, like the project's other MetaHuman content:
these stages rebuild it.

bases: each sex's base body (/Game/Shinobi/Mutable/Bases/SK_MutableBase_<Sex>) = the average MetaHuman body built by
  Scripts/mutable/base_bodies.py, with morphs Build_Under and Build_Over (the slim and heavy builds) baked in by
  UShinobiFaceBlendLibrary (same vertex layout, aligned at the root); and SK_MutableOwn_<Sex>, RamilMH's or BikaMH's own
  body (the Own build). Both skin the neck and "head" a little (UShinobiMutableMeshLibrary::add_bone_skinning).
faceskin (not in the default run): the neck and head skinning alone, on the existing bases.
heads (needs run_python.ps1 -Render: FBX export): each roster character's head (names.HEADS) grafted onto its sex's base
  by Scripts/character/graft_head.py (donor SourceArt/Character/Models/<Name>/<Name>_rigged.fbx/.png, the base and its
  average build's MetaHuman face for the neck) and imported as /Game/Shinobi/Mutable/Heads/SK_Head_<Name> with the
  donor's material and a neck material; Saved/MutableHeads/<Name>_tint.json is the body tint that matches its skin
  (the neck texture's mean colour over the base body's).
skintone: M_StaticHead (/Game/Shinobi/Mutable/Heads: base colour texture "BaseColor" times vector "Skin Tint", default
  white; M_Character, tracked and shared, is left alone) under every roster head's materials (its own MI_Head_<Name>_Skin with the donor's texture, and its neck
  MI), so a look's skin tone reaches the head (UShinobiBodyComponent::ApplySkinTone). Run after heads.
skinlayer: /Game/Shinobi/Skin/M_ShinobiSkin, the project's MetaHuman skin material (M_skin_unified_baked) with the skin
  layers (UShinobiSkinLibrary::AddSkinLayers: 3 tattoos, eye/lips/blush make-up, freckles; all off), under the shared
  MetaHumans/Common MI_Head_Baked_LOD0_VT, so every MetaHuman body and face in the project can wear them.
tattoos: DT_Tattoos (FShinobiTattooDesign: the Mutable Sample's tattoo designs with alpha) and DT_TattooRegions
  (FShinobiTattooRegion: where each body region sits in the MetaHuman body's UVs, found on the male base by
  UShinobiSkinLibrary::SkinUV; mirrored left/right UVs are logged).
makeup: DT_MakeUp (FShinobiMakeUpRow: row 0 none, then looks from the MetaHuman plugin's ArtistDelights face masks).
catalog: DA_CreatorCatalog's Heads (with their tints), the roster MetaHuman faces and a preset per roster character
  (run after setup_creator.py catalog, which rewrites Faces).
palette: DT_Palette, the wardrobe colours (FShinobiPaletteRow).
fits: morph target Fit_<Source> on each sex's base per names.FIT_SOURCES, moving it onto the body that source's pieces
  were made on (UShinobiMutableMeshLibrary::BakeFitMorph), so a piece is reshaped from the body it fits to whatever body
  is built.
samplecopy: the Mutable Sample's content folders (names.SAMPLE_FOLDERS) copied into Content/ at their own paths, so their
  references hold (local only, git-ignored), when missing.
fabimport: the user's MetaHuman Fab outfit packages (names.FAB_PACKAGES, from VaultCache/FabLibrary) imported into
  /Game/Outfits/<folder> when missing (local only, git-ignored); their cloth assets become rows (names.FAB_PIECES).
coverage: wardrobe, remaking every row's coverage volume first (/Game/Shinobi/Mutable/Wardrobe/Clips/SK_Clip_<Slot>_<Row>_<Sex>:
  the closed shell under the skin each piece covers, made on its fit shape; UShinobiMutableMeshLibrary::MakeClipVolume).
wardrobe: DT_Wardrobe_<Slot> per slot (FShinobiWardrobeRow): data the game passes to CO_Shinobi at runtime, so rows can be
  added without recompiling. Top, Bottom and Shoes hold every City Sample catalog style, index-aligned per sex, then
  names.SAMPLE_PIECES (a "pose:" source's pieces rebound onto each base once: UShinobiMutableMeshLibrary::RebindPiece).
graph: CO_Shinobi, built with UShinobiMutableGraphLibrary. Body parameter -> Build switch over the build morphs (scaled by
  Build Blend) -> pose reshape (the skeleton follows the build) -> each slot's clip (when "<Slot> Hides" is ON) -> each slot's piece merged in (when "<Slot>" is ON), its
  "<Slot> Mesh" reshaped from the base to the built body -> the Body component.
"""
import glob
import json
import os
import sys

import unreal

sys.path += [os.path.dirname(os.path.abspath(__file__)), os.path.join(os.path.dirname(__file__), "mutable")]
from names import (BASE, BUILDS, CATALOG, FAB_MAX_INSIDE, FAB_PACKAGES, FAB_PIECES, SAMPLE_CONTENT, SAMPLE_FOLDERS, SAMPLE_PIECES, CLIP, CLIP_DEPTH, CLIP_ERODE, CLIP_OUT, CLIP_REACH, FIT_KEEP_BONES, FIT_KEEP_FADE, FIT_PREFIX, FIT_MAX_MOVE, FIT_SMOOTH, FIT_SOURCES, FIT_SUFFIX, HEADS, HEADS_DIR, CLIP_SUFFIX, HIDES_SUFFIX,  # noqa: E402
                   MESH_SUFFIX, MUTABLE_ROOT, OBJECT, OFF, ON, OWN, OWN_BODIES, PALETTE, PARAM_BODY, PARAM_BUILD, PARAM_BUILD_BLEND,
                   NO_FIT, PARAM_FIT_BASE, PARAM_FIT_STRENGTH, SLOTS, WARDROBE_TABLE)

ONLY = [s for s in os.environ.get("SETUP_MUTABLE_ONLY", "").split(",") if s]
EAL = unreal.EditorAssetLibrary
G = unreal.ShinobiMutableGraphLibrary
BASES = f"{MUTABLE_ROOT}/Bases"
SEXES = ("Male", "Female")
# A clip parameter's default (never worn: "<Slot> Hides" is OFF unless a row gives a clip mesh).
CLIP_DEFAULT = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"
# Row 0 keeps a piece's own colours (FShinobiPaletteRow).
PALETTE_ROWS = [("Original", (1.0, 1.0, 1.0)), ("Black", (0.02, 0.02, 0.02)), ("White", (0.85, 0.85, 0.82)), ("Grey", (0.25, 0.25, 0.26)),
                ("Navy", (0.03, 0.05, 0.16)), ("Red", (0.6, 0.04, 0.03)), ("Crimson", (0.3, 0.01, 0.03)),
                ("Orange", (0.85, 0.3, 0.04)), ("Ochre", (0.55, 0.36, 0.08)), ("Olive", (0.2, 0.22, 0.06)),
                ("Forest", (0.03, 0.15, 0.06)), ("Teal", (0.02, 0.25, 0.25)), ("Purple", (0.18, 0.05, 0.3))]


def log(message):
    unreal.log(f"[setup_mutable] {message}")


def built_body(name):
    return f"{BASES}/Build/MHC_{name}/Body/SKM_MHC_{name}_BodyMesh"


def asset_path(path):
    """The object path of a package path (/Game/A/B -> /Game/A/B.B)."""
    return f"{path}.{path.rsplit('/', 1)[1]}"


def replace(path):
    if EAL.does_asset_exist(path):
        if not EAL.delete_asset(path):
            raise RuntimeError(f"could not delete {path} (open in an editor?)")


def skin_face_bones(mesh):
    return {bone: unreal.ShinobiMutableMeshLibrary.add_bone_skinning(mesh, bone) for bone in ("neck_01", "neck_02", "head")}


def face_skin():
    """skin_face_bones on the existing bases (bases re-creates them, which fails in an editor that has CO_Shinobi loaded)."""
    for sex in SEXES:
        for path in (BASE.format(sex=sex), OWN.format(sex=sex)):
            mesh = unreal.load_asset(path)
            log(f"{path}: {skin_face_bones(mesh)}")
            EAL.save_loaded_asset(mesh, only_if_is_dirty=False)


def bases():
    for sex in SEXES:
        path = BASE.format(sex=sex)
        replace(path)
        base = EAL.duplicate_asset(built_body(f"MB_{sex}_Normal"), path)
        if not base:
            raise RuntimeError(f"no built MB_{sex}_Normal body: run Scripts/mutable/base_bodies.py first")
        others = [unreal.load_asset(built_body(f"MB_{sex}_Under")), unreal.load_asset(built_body(f"MB_{sex}_Over")),
                  unreal.load_asset(OWN_BODIES[sex])]
        report = unreal.ShinobiFaceBlendLibrary.bake_face_blend_morphs(base, others[:2], ["Under", "Over"], 4, "root", "Build_", 60.0)
        # Bodies skin the neck and head a little (the face mesh skins them, not the body), so a merged piece skinned to them
        # can't put its own body's pose of them on the skeleton: the face would ride it, and its grooms be flung off it.
        headed = skin_face_bones(base)
        EAL.save_loaded_asset(base, only_if_is_dirty=False)
        log(f"{sex} base {path}: {report}; head skinned on {headed} vertices")
        # The Own build: RamilMH's or BikaMH's own body (exact joints), worn as the Body with the pieces reshaped onto it.
        own = OWN.format(sex=sex)
        replace(own)
        copy = EAL.duplicate_asset(OWN_BODIES[sex], own)
        headed = skin_face_bones(copy)
        EAL.save_loaded_asset(copy, only_if_is_dirty=False)
        log(f"{sex} own body {own}: head skinned on {headed} vertices")
        # The average build's character blueprint isn't used (the bases take only its body) and references its face and
        # grooms, which aren't cooked (Config/DefaultGame.ini DirectoriesToNeverCook): remove it so the cook doesn't fail.
        # (the file itself: the commandlet's asset registry doesn't list it)
        blueprint = os.path.join(unreal.Paths.project_content_dir(), "Shinobi", "Mutable", "Bases", "Build",
                                 f"MHC_MB_{sex}_Normal", f"BP_MHC_MB_{sex}_Normal.uasset")
        if os.path.exists(blueprint):
            os.remove(blueprint)
            log(f"removed {blueprint}")


def fits():
    dna_dir = os.path.join(unreal.Paths.engine_plugins_dir(), "MetaHuman", "MetaHumanCharacter", "Content", "Optional", "Body", "FixedCompatibility")
    M = unreal.ShinobiMutableMeshLibrary
    for source, bodies in FIT_SOURCES.items():
        for sex, body in bodies.items():
            base = unreal.load_asset(BASE.format(sex=sex))
            # (a bool UFUNCTION with out parameters returns the outs, or None when it returns false)
            if body.startswith("dna:"):
                surface = M.read_dna_body(os.path.join(dna_dir, body[4:] + ".dna"))
            elif body.startswith("pose:"):
                surface = M.read_posed_surface(unreal.load_asset(body[5:]), base)
            else:
                surface = M.read_mesh_surface(unreal.load_asset(body))
            if not surface:
                raise RuntimeError(f"no surface for {source} {sex}: {body}")
            positions, triangles = surface
            report = M.bake_fit_morph(base, FIT_PREFIX + source, positions, triangles, FIT_MAX_MOVE, FIT_SMOOTH, FIT_KEEP_BONES, FIT_KEEP_FADE)
            EAL.save_loaded_asset(base, only_if_is_dirty=False)
            log(f"fit {source} {sex} from {body} ({len(positions)} vertices): {report}")


def heads():
    import subprocess

    os.environ["SETUP_HERO_ONLY"] = "library"
    import setup_creator as creator  # (as a library: it runs nothing without SETUP_CREATOR_*)
    hero = creator.hero
    work = os.path.join(hero.PROJECT, "Saved", "MutableHeads")
    os.makedirs(work, exist_ok=True)

    def export(asset, path, exporter):
        task = unreal.AssetExportTask()
        task.object, task.filename, task.automated, task.prompt, task.replace_identical = asset, path, True, False, True
        task.exporter = exporter
        if isinstance(exporter, unreal.SkeletalMeshExporterFBX):
            task.options = unreal.FbxExportOption()
            task.options.level_of_detail = False
            task.options.export_morph_targets = False
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError(f"could not export {asset.get_path_name()}")
        return path

    parent = unreal.load_asset(f"{hero.CHARACTERS_DIR}/M_Character")
    exported = {}
    for name, sex in HEADS.items():
        if sex not in exported:
            build = f"{BASES}/Build/MHC_MB_{sex}_Normal"
            exported[sex] = (
                export(unreal.load_asset(BASE.format(sex=sex)), os.path.join(work, f"Base_{sex}.fbx"), unreal.SkeletalMeshExporterFBX()),
                export(unreal.load_asset(f"{build}/Face/SKM_MHC_MB_{sex}_Normal_FaceMesh"), os.path.join(work, f"Face_{sex}.fbx"), unreal.SkeletalMeshExporterFBX()),
                export(unreal.load_asset(f"{build}/Body/Baked/T_Body_BC_VT"), os.path.join(work, f"Body_{sex}.png"), unreal.TextureExporterPNG()))
        body_fbx, face_fbx, body_png = exported[sex]
        donor = os.path.join(hero.MODELS_DIR, name, f"{name}_rigged")
        out = os.path.join(work, f"Head_{name}.fbx")
        run = subprocess.run([hero.BLENDER, "-b", "--factory-startup", "--python-exit-code", "1", "-P",
                              os.path.join(hero.PROJECT, "Scripts", "character", "graft_head.py"), "--", body_fbx, face_fbx,
                              donor + ".fbx", out, "--texture", donor + ".png", "--body-texture", body_png,
                              "--render", os.path.join(work, name)], capture_output=True, text=True)
        if run.returncode != 0:
            raise RuntimeError(f"graft_head failed for {name}: {run.stdout[-1500:]} {run.stderr[-800:]}")
        log(f"head {name}: {[l for l in run.stdout.splitlines() if '[graft_head]' in l][-3:]}")

        skeleton = unreal.load_asset(BASE.format(sex=sex)).get_editor_property("skeleton")
        head = creator.import_skeletal(out, HEADS_DIR, f"SK_Head_{name}", skeleton, morphs=False)
        neck_png = os.path.splitext(out)[0] + "_neck.png"
        neck = hero.import_texture_file(neck_png, HEADS_DIR, f"T_Head_{name}_Neck")
        # The donor's material: its MI_<Name>_<Name>_rigged, or (the hero, imported apart) its roster mesh's.
        face = f"{hero.CHARACTERS_DIR}/{name}/MI_{name}_{name}_rigged"
        face = unreal.load_asset(face) if EAL.does_asset_exist(face) else             unreal.load_asset(f"{hero.HERO_DIR}/SK_{name}").get_editor_property("materials")[0].get_editor_property("material_interface")
        slots = []
        for slot in head.get_editor_property("materials"):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            material = creator.material_instance(f"MI_Head_{name}_Neck", HEADS_DIR, neck, parent) if slot_name.endswith("_Neck") else face
            slots.append(unreal.SkeletalMaterial(material_interface=material, material_slot_name=slot_name))
        head.set_editor_property("materials", slots)
        EAL.save_loaded_asset(head)

        # The body tint: the head's neck skin over the base body's (Scripts/mutable/skin_tint.py, system Python + Pillow).
        tint = subprocess.run(["python", os.path.join(hero.PROJECT, "Scripts", "mutable", "skin_tint.py"), neck_png, body_png],
                              capture_output=True, text=True)
        if tint.returncode != 0:
            raise RuntimeError(f"tint for {name}: {tint.stderr[-500:]}")
        with open(os.path.join(work, f"{name}_tint.json"), "w", encoding="utf-8") as f:
            f.write(tint.stdout.strip())
        log(f"head {name}: SK_Head_{name}, slots {[str(s.material_slot_name) for s in slots]}, tint {tint.stdout.strip()}")


# Every roster MetaHuman face (AShinobiPlayerCharacter::Faces): its catalog Faces entry. The first of each is its roster pick.
MH_FACES = {"RamilMH": "Male", "RamilMHScan": "Male", "BikaMH": "Female", "BikaMHKeenFace": "Female"}


STATIC_HEAD = f"{HEADS_DIR}/M_StaticHead"
STATIC_HEAD_TINT = "Skin Tint"  # ShinobiMutable::StaticHeadTintParameter


def skintone():
    MEL = unreal.MaterialEditingLibrary
    # M_Character's look (setup_hero.character_material: masked by the texture's alpha, two-sided, rough 0.8, specular
    # 0.3) times Skin Tint; heads need none of its outfit hide mask. Rebuilt in place, so the head instances keep it.
    folder, name = STATIC_HEAD.rsplit("/", 1)
    material = unreal.load_asset(STATIC_HEAD) if EAL.does_asset_exist(STATIC_HEAD) else         unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    texture = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    texture.set_editor_property("parameter_name", "BaseColor")
    texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 300)
    tint.set_editor_property("parameter_name", STATIC_HEAD_TINT)
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    rgb = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -450, 300)
    for channel in ("r", "g", "b"):
        rgb.set_editor_property(channel, True)
    multiply = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 0)
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -450, 450)
    rough.set_editor_property("r", 0.8)
    spec = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -450, 550)
    spec.set_editor_property("r", 0.3)
    shading = MEL.create_material_expression(material, unreal.MaterialExpressionSubstrateShadingModels, 0, 0)
    wired = [MEL.connect_material_expressions(texture, "RGB", multiply, "A"), MEL.connect_material_expressions(tint, "", rgb, ""),
             MEL.connect_material_expressions(rgb, "", multiply, "B"),
             MEL.connect_material_expressions(multiply, "", shading, "BaseColor"),
             MEL.connect_material_expressions(rough, "", shading, "Roughness"),
             MEL.connect_material_expressions(spec, "", shading, "Specular"),
             MEL.connect_material_property(shading, "", unreal.MaterialProperty.MP_FRONT_MATERIAL),
             MEL.connect_material_property(texture, "A", unreal.MaterialProperty.MP_OPACITY_MASK)]
    if not all(wired):
        raise RuntimeError(f"could not wire M_StaticHead: {wired}")
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, only_if_is_dirty=False)
    log(f"made {STATIC_HEAD}")
    parent = unreal.load_asset(STATIC_HEAD)
    for name in HEADS:
        head = unreal.load_asset(f"{HEADS_DIR}/SK_Head_{name}")
        if not head:
            continue
        slots = []
        for slot in head.get_editor_property("materials"):
            material = slot.get_editor_property("material_interface")
            if material and material.get_name().endswith("_Neck"):
                MEL.set_material_instance_parent(material, parent)
                EAL.save_loaded_asset(material, only_if_is_dirty=False)
            elif material:
                texture = MEL.get_material_instance_texture_parameter_value(material, "BaseColor") if isinstance(material, unreal.MaterialInstance) else None
                if texture:
                    import setup_creator as creator  # (as a library)
                    material = creator.material_instance(f"MI_Head_{name}_Skin", HEADS_DIR, texture, parent)
                else:
                    log(f"head {name}: {material.get_name()} has no BaseColor texture: left untinted")
            slots.append(unreal.SkeletalMaterial(material_interface=material, material_slot_name=slot.get_editor_property("material_slot_name")))
        head.set_editor_property("materials", slots)
        EAL.save_loaded_asset(head, only_if_is_dirty=False)
        log(f"head {name}: {[s.material_interface.get_name() for s in slots]}")


SKIN = "/Game/Shinobi/Skin/M_ShinobiSkin"
_SKIN_DIR = "/Game/Shinobi/MetaHumans/Common/Lookdev_UHM/Skin/Materials"
SKIN_SOURCE, SKIN_ROOT = f"{_SKIN_DIR}/M_skin_unified_baked", f"{_SKIN_DIR}/Baked/MI_Head_Baked_LOD0_VT"


def skinlayer():
    if not EAL.does_asset_exist(SKIN):  # (delete it to remake: a deleted asset can't be duplicated onto in the same session)
        material = EAL.duplicate_asset(SKIN_SOURCE, SKIN)
        if not unreal.ShinobiSkinLibrary.add_skin_layers(material):
            raise RuntimeError(f"no attributes output on {SKIN_SOURCE}")
        EAL.save_loaded_asset(material, only_if_is_dirty=False)
        log(f"made {SKIN}")
    root = unreal.load_asset(SKIN_ROOT)
    unreal.MaterialEditingLibrary.set_material_instance_parent(root, unreal.load_asset(SKIN))
    EAL.save_loaded_asset(root, only_if_is_dirty=False)
    log(f"{SKIN_ROOT} now under {SKIN}")


_TATTOO = "/Game/Character/Skin_Variations"
TATTOO_DESIGNS = [f"{_TATTOO}/Projected_Tattoos/T_Tattoo{n}_alpha" for n in ("Owl", "Geometric", "Native", "Precolumbian")] +                  [f"{_TATTOO}/Cylindrical_Tattos_Projected/T_Tattoo{n}_alpha" for n in ("Frieze", "Stripes", "Triangles")]
# Region: (bone, end bone, direction out of the body (MetaHuman bodies face +Y; left is +X), along bone to end, reach cm,
# UV scale).
TATTOO_REGIONS = {
    "Chest": ("spine_05", "", (0, 1, 0), 0.0, 30, 0.13), "Back": ("spine_04", "", (0, -1, 0), 0.0, 30, 0.15),
    "Belly": ("spine_02", "", (0, 1, 0), 0.0, 30, 0.1), "Neck": ("neck_01", "neck_02", (0, 1, 0), 0.5, 20, 0.05),
    "UpperArmL": ("upperarm_l", "lowerarm_l", (0, 1, 0), 0.5, 20, 0.08), "UpperArmR": ("upperarm_r", "lowerarm_r", (0, 1, 0), 0.5, 20, 0.08),
    "ForearmL": ("lowerarm_l", "hand_l", (0, 1, 0), 0.5, 20, 0.06), "ForearmR": ("lowerarm_r", "hand_r", (0, 1, 0), 0.5, 20, 0.06),
    "ThighL": ("thigh_l", "calf_l", (0, 1, 0), 0.5, 25, 0.09), "ThighR": ("thigh_r", "calf_r", (0, 1, 0), 0.5, 25, 0.09),
    "CalfL": ("calf_l", "foot_l", (0, -1, 0), 0.4, 20, 0.07), "CalfR": ("calf_r", "foot_r", (0, -1, 0), 0.4, 20, 0.07),
}
TATTOOS, TATTOO_REGIONS_TABLE = f"{MUTABLE_ROOT}/DT_Tattoos", f"{MUTABLE_ROOT}/DT_TattooRegions"


def tattoos():
    # A design's thumbnail is the design itself (a white mask, shown on the CREATE tab's dark buttons).
    designs = [{"Name": path.rsplit("/", 1)[1][len("T_Tattoo"):-len("_alpha")], "Texture": asset_path(path), "Thumbnail": asset_path(path)}
               for path in TATTOO_DESIGNS if EAL.does_asset_exist(path)]
    new_table(TATTOOS, unreal.ShinobiTattooDesign, designs)
    base = unreal.load_asset(BASE.format(sex="Male"))
    regions = []
    for name, (bone, end, direction, along, reach, scale) in TATTOO_REGIONS.items():
        uv = unreal.ShinobiSkinLibrary.skin_uv(base, bone, end or "None", unreal.Vector(*direction), along, reach)
        regions.append({"Name": name, "Centre": {"X": uv.x, "Y": uv.y}, "Scale": scale, "Rotation": 0.0})
        log(f"tattoo region {name}: UV ({uv.x:.3f}, {uv.y:.3f})")
    for left in [r for r in regions if r["Name"].endswith("L")]:
        right = next(r for r in regions if r["Name"] == left["Name"][:-1] + "R")
        if abs(left["Centre"]["X"] - right["Centre"]["X"]) + abs(left["Centre"]["Y"] - right["Centre"]["Y"]) < 0.01:
            log(f"tattoo regions {left['Name']} and {right['Name']} share UVs (mirrored): a tattoo shows on both")
    new_table(TATTOO_REGIONS_TABLE, unreal.ShinobiTattooRegion, regions)
    log(f"DT_Tattoos: {[d['Name'] for d in designs]}; DT_TattooRegions: {list(TATTOO_REGIONS)}")


_ART = "/MetaHumanCharacter/Textures/ArtistDelights"
MAKEUP = f"{MUTABLE_ROOT}/DT_MakeUp"
# Look: (eye mask, eye colour, lips mask, lips colour, blush mask, blush colour); a mask None leaves that part bare.
MAKEUP_LOOKS = {
    "None": (None, None, None, None, None, None),
    "Natural": (None, None, "Natural", (0.45, 0.16, 0.14), "Apple", (0.55, 0.22, 0.2)),
    "Smoky": ("SoftSmokey", (0.04, 0.035, 0.035), "Natural", (0.38, 0.2, 0.17), None, None),
    "CatEye": ("CatEye", (0.01, 0.01, 0.01), "Hollywood", (0.45, 0.02, 0.03), None, None),
    "Dramatic": ("DramaticSmudge", (0.05, 0.02, 0.06), "Cupid", (0.3, 0.02, 0.06), "Angled", (0.45, 0.12, 0.15)),
}


def makeup():
    rows = []
    for name, (eye, eye_colour, lips, lips_colour, blush, blush_colour) in MAKEUP_LOOKS.items():
        row = {"Name": name}
        for part, folder, mask, colour in (("Eye", "EyeMakeup", eye, eye_colour), ("Lips", "LipsMakeup", lips, lips_colour),
                                           ("Blush", "BlushMakeup", blush, blush_colour)):
            if mask:
                row[part] = asset_path(f"{_ART}/{folder}/T_{folder}_{mask}")
                row[f"{part}Colour"] = {"R": colour[0], "G": colour[1], "B": colour[2], "A": 1.0}
        rows.append(row)
    new_table(MAKEUP, unreal.ShinobiMakeUpRow, rows)
    log(f"DT_MakeUp: {list(MAKEUP_LOOKS)}")


def catalog():
    """After setup_creator.py catalog (which rewrites Faces) and heads: the static heads, the roster MetaHuman faces and a
    preset per roster character in DA_CreatorCatalog."""
    data = unreal.load_asset(CATALOG)
    heads_list = []
    for name, sex in HEADS.items():
        with open(os.path.join(unreal.Paths.project_saved_dir(), "MutableHeads", f"{name}_tint.json"), encoding="utf-8") as f:
            tint = json.load(f)
        heads_list.append(make(unreal.ShinobiCreatorHead, id=name, sex=getattr(unreal.ShinobiSex, sex.upper()),
                               mesh=unreal.load_asset(f"{HEADS_DIR}/SK_Head_{name}"), skin_tint=unreal.LinearColor(*tint, 1.0)))
    data.set_editor_property("heads", heads_list)

    faces = [f for f in data.get_editor_property("faces") if str(f.get_editor_property("id")) not in MH_FACES]
    for face_id, sex in MH_FACES.items():
        roster = "RamilMH" if face_id.startswith("RamilMH") else "BikaMH"  # the body its Own build is
        build = f"/Game/Shinobi/MetaHumans/{face_id}/Build/MHC_{face_id}"
        # Worn through its blueprint (its Fab hair, groom materials and LODSync are set there), as on its roster body.
        faces.append(make(unreal.ShinobiCreatorFace, id=face_id, sex=getattr(unreal.ShinobiSex, sex.upper()),
                          mesh=unreal.load_asset(f"{build}/Face/SKM_MHC_{face_id}_FaceMesh"),
                          meta_human=EAL.load_blueprint_class(f"{build}/BP_MHC_{face_id}"),
                          retargeter=unreal.load_asset(f"/Game/Shinobi/MetaHumans/{roster}/RTG_MannyTo{roster}")))
    data.set_editor_property("faces", faces)

    def face_index(face_id):
        sex = getattr(unreal.ShinobiSex, MH_FACES[face_id].upper())
        return [f for f in faces if f.get_editor_property("sex") == sex].index(next(f for f in faces if str(f.get_editor_property("id")) == face_id))

    data.set_editor_property("presets", [])
    for i, (name, sex) in enumerate(HEADS.items()):
        data.set_preset(name, getattr(unreal.ShinobiSex, sex.upper()), i, 0, unreal.ShinobiBuild.NORMAL, 0)
    # ponytail: no Mutable pieces on the own bodies (their blueprint's underwear comes with the face): unfitted City Sample
    # pieces spike when reshaped onto them. P3's wardrobe refit dresses them.
    for name in ("RamilMH", "BikaMH"):
        data.set_preset(name, getattr(unreal.ShinobiSex, MH_FACES[name].upper()), 255, face_index(name), unreal.ShinobiBuild.OWN, 0)
    EAL.save_loaded_asset(data)
    log(f"catalog: {len(heads_list)} heads, faces {list(MH_FACES)}, {len(data.get_editor_property('presets'))} presets")


def make(struct, **values):
    item = struct()
    for name, value in values.items():
        item.set_editor_property(name, value)
    return item


def new_table(path, struct, rows):
    """The Data Table of struct at path, made if missing, holding exactly rows ({column: value} keyed "Name"). An existing
    table is refilled rather than replaced (deleting and re-creating it in one editor session leaves it unusable)."""
    table = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if table and table.get_editor_property("row_struct") != struct.static_struct():
        replace(path)
        table = None
    if not table:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct.static_struct())
        folder, name = path.rsplit("/", 1)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.DataTable, factory)
    if not table:
        raise RuntimeError(f"could not make {path}")
    if not rows:
        if unreal.DataTableFunctionLibrary.get_data_table_row_names(table):  # JSON can't fill a table with no rows
            replace(path)
            return new_table(path, struct, rows)
    elif not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError(f"could not fill {path}")
    EAL.save_loaded_asset(table, only_if_is_dirty=False)
    return table


def palette():
    rows = [{"Name": name, "DisplayName": name, "Colour": {"R": r, "G": g, "B": b, "A": 1.0}} for name, (r, g, b) in PALETTE_ROWS]
    new_table(PALETTE, unreal.ShinobiPaletteRow, rows)
    log(f"DT_Palette: {len(rows)} colours")


def wardrobe_rows(slot):
    """The slot's rows: every City Sample catalog style, index-aligned per sex (row N holds each sex's Nth style of the slot,
    average build), so a version-1 look's Top/Bottom/Shoes index (a style within its sex) wears the same piece. A sex with
    fewer styles has no piece in the last rows (as in version 1)."""
    styles = unreal.load_asset(CATALOG).get_editor_property("styles")
    enum_slot = getattr(unreal.ShinobiWardrobeSlot, slot.upper())

    def of(sex):
        return [s.get_editor_property("by_build")[1].get_path_name() for s in styles
                if s.get_editor_property("sex") == sex and s.get_editor_property("slot") == enum_slot]

    male, female = of(unreal.ShinobiSex.MALE), of(unreal.ShinobiSex.FEMALE)
    rows = []
    for index in range(max(len(male), len(female))):
        row = {"Name": f"CitySample{index + 1:02d}", "bHasWear": False}
        if index < len(male):
            row["MaleMesh"], row["MaleFit"] = male[index], "CitySample"
        if index < len(female):
            row["FemaleMesh"], row["FemaleFit"] = female[index], "CitySample"
        rows.append(row)
    return rows


def clip(slot, row, sex, mesh, fit, remake):
    """The coverage volume of row's piece for sex (made when remake or missing), or None when it covers nothing."""
    path = CLIP.format(slot=slot, row=row, sex=sex)
    if EAL.does_asset_exist(path) and not remake:
        return asset_path(path)
    base = BASE.format(sex=sex)
    # (kept between runs and remade in place: a deleted asset can't be duplicated onto in the same session)
    volume = unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(base, path)
    covered = unreal.ShinobiMutableMeshLibrary.make_clip_volume(volume, unreal.load_asset(base), FIT_PREFIX + fit if fit else "None",
                                                                  unreal.load_asset(mesh), CLIP_REACH, CLIP_ERODE, CLIP_OUT, CLIP_DEPTH)
    EAL.save_loaded_asset(volume, only_if_is_dirty=False)
    log(f"clip {slot} {row} {sex}: {covered} triangles")
    return asset_path(path) if covered else None


CITY_SAMPLE_CLOTH = "M_Clothing"  # the City Sample crowd clothing master material
CITY_SAMPLE_TINT_SWITCHES = ("A_ColorCrowd_DoTint", "A_ColorCrowd_Tint_UseMain")  # its layer A takes A_CrowdColor_main
CITY_SAMPLE_COLOUR = "A_CrowdColor_main"
TINTED = f"{MUTABLE_ROOT}/Wardrobe/CitySample/Materials"


def tinted(mesh):
    """Per material slot of a piece, a copy of a City Sample clothing material with its colour tint switched on (made
    once), else None."""
    MEL = unreal.MaterialEditingLibrary
    copies = []
    for slot in unreal.load_asset(mesh).get_editor_property("materials"):
        material, parent = slot.material_interface, slot.material_interface
        while isinstance(parent, unreal.MaterialInstance):
            parent = parent.get_editor_property("parent")
        if not material or not parent or parent.get_name() != CITY_SAMPLE_CLOTH:
            copies.append(None)
            continue
        path = f"{TINTED}/{material.get_name()}_Tint"
        if not EAL.does_asset_exist(path):
            copy = EAL.duplicate_asset(material.get_path_name(), path)
            for switch in CITY_SAMPLE_TINT_SWITCHES:
                MEL.set_material_instance_static_switch_parameter_value(copy, switch, True)
            MEL.update_material_instance(copy)
            EAL.save_loaded_asset(copy, only_if_is_dirty=False)
        copies.append(asset_path(path))
    return copies


def sample_copy():
    import shutil
    content = unreal.Paths.project_content_dir()
    for folder in SAMPLE_FOLDERS:
        target = os.path.join(content, folder)
        if not os.path.isdir(target):
            shutil.copytree(os.path.join(SAMPLE_CONTENT, folder), target)
            log(f"copied {SAMPLE_CONTENT}/{folder}")
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([f"/Game/{f}" for f in SAMPLE_FOLDERS], True)


def fab_import():
    import glob
    vault = os.path.join(unreal.Paths.project_dir(), "VaultCache", "FabLibrary")
    for folder, package in FAB_PACKAGES.items():
        if EAL.does_directory_have_assets(f"/Game/Outfits/{folder}"):
            continue
        found = glob.glob(os.path.join(vault, "*", "metahuman", "UE_5.*", f"{package}.mhpkg"))
        if not found:
            log(f"no {package}.mhpkg under {vault}: skipped")
            continue
        task = unreal.AssetImportTask()
        task.filename, task.destination_path, task.automated, task.save, task.replace_existing = found[0], f"/Game/Outfits/{folder}", True, True, True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        log(f"imported {found[0]}: {list(task.imported_object_paths)}")


def sample_piece(row, sex, mesh, source, remake):
    """The piece's mesh for the sex and the fit it reshapes from: as it is with its source's fit (MetaHuman skeleton), or
    rebound onto the sex's base for a "pose:" source or a Fab cloth asset (fitted to the base already: no fit). None for a
    Fab piece that doesn't sit on the sex's fit shape."""
    M = unreal.ShinobiMutableMeshLibrary
    cloth = mesh.rsplit("/", 1)[-1].startswith("CA_")
    if not cloth and not FIT_SOURCES[source][sex].startswith("pose:"):
        return asset_path(mesh), source
    path = f"{MUTABLE_ROOT}/Wardrobe/{source}/SK_{row}_{sex}"
    if not EAL.does_asset_exist(path) or remake:
        base = BASE.format(sex=sex)
        if cloth:  # the cloth asset's render mesh, then judged against the sex's fit shape
            piece = M.export_cloth_asset(unreal.load_asset(mesh), f"{path}_Source")
            if not piece:
                raise RuntimeError(f"could not export {mesh}")
            EAL.save_loaded_asset(piece, only_if_is_dirty=False)
            inside = M.fraction_inside(piece, unreal.load_asset(base), 0.5, FIT_PREFIX + source)
            log(f"{row} {sex}: {inside * 100:.1f}% inside its fit shape")
            if inside > FAB_MAX_INSIDE:
                return None, None
            body = None
        else:
            piece = unreal.load_asset(mesh)
            body = unreal.load_asset(FIT_SOURCES[source][sex][len("pose:"):])  # (posed as its body is, so the two stay together)
        out = unreal.load_asset(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(base, path)
        count = M.rebind_piece(out, unreal.load_asset(base), FIT_PREFIX + source, piece, body)
        if not count:
            raise RuntimeError(f"could not rebind {mesh} for {sex}")
        EAL.save_loaded_asset(out, only_if_is_dirty=False)
        log(f"rebound {row} {sex}: {count} vertices")
    return asset_path(path), None


def sample_rows(slot, remake):
    rows = []
    for row, piece_slot, source, meshes, *replaces in SAMPLE_PIECES + FAB_PIECES:
        if piece_slot != slot:
            continue
        entry = {"Name": row, "bHasWear": False}
        if replaces:
            entry["Replaces"] = list(replaces[0])  # (EShinobiWardrobeSlot names: the table is written from JSON)
        for sex, mesh in meshes.items():
            if EAL.does_asset_exist(mesh):
                piece, fit = sample_piece(row, sex, mesh, source, remake)
                if piece:
                    entry[f"{sex}Mesh"] = piece
                if fit:
                    entry[f"{sex}Fit"] = fit
        if any(f"{sex}Mesh" in entry for sex in SEXES):
            rows.append(entry)
    return rows


def coverage():
    wardrobe(remake=True)


THUMBNAILS = f"{MUTABLE_ROOT}/Thumbnails"


def thumbnails():
    """Imports Saved/MenuArt/Thumbnails/T_Thumb_<Slot>_<Row>.png (Scripts/menu_shots.ps1 -Thumbnails), then refills the
    wardrobe tables so each row points at its picture."""
    source = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "MenuArt", "Thumbnails")
    pngs = sorted(glob.glob(os.path.join(source, "T_Thumb_*.png")))
    for png in pngs:
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = THUMBNAILS
        task.destination_name = os.path.splitext(os.path.basename(png))[0]
        task.automated = True
        task.replace_existing = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = next((o for o in task.get_objects() if isinstance(o, unreal.Texture2D)), None)
        if texture is None:
            raise RuntimeError(f"import of {png} produced no texture")
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("max_texture_size", 256)
        EAL.save_loaded_asset(texture, only_if_is_dirty=False)
    log(f"{len(pngs)} thumbnails from {source}")
    wardrobe()


def wardrobe(remake=False):
    for slot in SLOTS:
        rows = wardrobe_rows(slot) + sample_rows(slot, remake)
        for row in rows:
            for sex in SEXES:
                if row.get(f"{sex}Mesh"):
                    row[f"{sex}Clip"] = clip(slot, row["Name"], sex, row[f"{sex}Mesh"], row.get(f"{sex}Fit"), remake)
                    row[f"{sex}Tinted"] = tinted(row[f"{sex}Mesh"])
            row["ColourParameters"] = [CITY_SAMPLE_COLOUR]
            thumb = f"{THUMBNAILS}/T_Thumb_{slot}_{row['Name']}"
            if EAL.does_asset_exist(thumb):
                row["Thumbnail"] = asset_path(thumb)
        new_table(WARDROBE_TABLE.format(slot=slot), unreal.ShinobiWardrobeRow, rows)
        log(f"DT_Wardrobe_{slot}: {[r['Name'] for r in rows]}")


def link(a, pin_a, b, pin_b):
    if not G.link_pins(a, pin_a, b, pin_b):
        raise RuntimeError(f"link {a.get_class().get_name()}.{pin_a} -> {b.get_class().get_name()}.{pin_b} failed;\n"
                           f"  from pins {list(G.list_pins(a))}\n  to pins {list(G.list_pins(b))}")


def node(co, cls, x, y, **props):
    n = G.add_node(co, cls, x, y)
    if not n:
        raise RuntimeError(f"no node {cls}")
    for key, value in props.items():
        if not G.set_node_property(n, key, value):
            raise RuntimeError(f"{cls}.{key} = {value} failed")
    return n


def enum(co, name, values, default, x, y):
    listed = ",".join(f'(Name="{v}")' for v in values)
    return node(co, "CustomizableObjectNodeEnumParameter", x, y, ParameterName=name, Values=f"({listed})", DefaultIndex=str(values.index(default)))


def switch(co, parameter, x, y):
    """A skeletal-mesh switch over an enum parameter's values (one input pin per value)."""
    s = node(co, "CONodeSwitch", x, y, PinType="skeletalMesh")
    link(parameter, "Enum", s, "Switch Parameter")
    return s


def mesh_parameter(co, name, default, x, y):
    return node(co, "CustomizableObjectNodeSkeletalMeshParameter", x, y, ParameterName=name, DefaultValue=default, ReferenceValue=default)


def graph():
    # An existing object is cleared and rebuilt (deleting and re-creating one in the same editor session fails).
    co = unreal.load_asset(OBJECT) if EAL.does_asset_exist(OBJECT) else None
    if co:
        G.clear_graph(co)
    else:
        params = unreal.NewCustomizableObjectParameters()
        params.set_editor_property("package_path", OBJECT.rsplit("/", 1)[0])
        params.set_editor_property("asset_name", OBJECT.rsplit("/", 1)[1])
        co = unreal.CustomizableObjectEditorFunctionLibrary.new_customizable_object(params)
    if not co:
        raise RuntimeError(f"could not create {OBJECT}")
    root = G.find_node(co, "CustomizableObjectNodeObject", 0)
    base = asset_path(BASE.format(sex="Male"))

    # Body: the sex's base (or the Own build's own body), given at runtime, morphed toward the chosen build. Fit Base: the
    # sex's base, which the pieces were authored on.
    body = mesh_parameter(co, PARAM_BODY, base, -3000, 0)
    fit_base = mesh_parameter(co, PARAM_FIT_BASE, base, -3000, -300)
    blend = node(co, "CustomizableObjectNodeFloatParameter", -3000, 300, ParameterName=PARAM_BUILD_BLEND, DefaultValue="1.0")
    fit_strength = node(co, "CustomizableObjectNodeFloatParameter", -3000, -450, ParameterName=PARAM_FIT_STRENGTH, DefaultValue="1.0")
    build = enum(co, PARAM_BUILD, BUILDS, "Normal", -3000, 450)
    built = switch(co, build, -2400, 0)
    for i, value in enumerate(BUILDS):
        if value in ("Normal", "Own"):  # (Own: the Body is the own body itself)
            link(body, "Skeletal Mesh", built, value)
            continue
        morph = node(co, "CONodeSkeletalMeshMorph", -2700, 150 * i, MorphTargetName=f"Build_{value}")
        link(body, "Skeletal Mesh", morph, "Skeletal Mesh")
        link(blend, "Float", morph, "Factor")
        link(morph, "Skeletal Mesh", built, value)

    # The skeleton follows the slim and heavy morphs. (A merged piece can't move a bone the body skins: Mutable keeps the
    # pose of the mesh that skins or reshapes a bone, OpMeshMerge.cpp ComputeBoneMergePriority, ties to the body; the
    # bodies skin "head" for that, see bases.)
    posed = node(co, "CONodeSkeletalMeshReshape", -2100, 0, bReshapeVertices="False", bReshapePose="True",
                 SelectionMethod="ALL_BUT_SELECTED")
    link(built, "Skeletal Mesh", posed, "Base Skeletal Mesh")
    link(body, "Skeletal Mesh", posed, "Base Shape")
    link(built, "Skeletal Mesh", posed, "Target Shape")

    # Each slot: its clip hides body skin (Hides ON), its piece is reshaped to the build and merged in (slot ON).
    current, current_pin = posed, "Skeletal Mesh"
    pieces = []
    for i, slot in enumerate(SLOTS):
        y = 800 + 700 * i
        rows = wardrobe_rows(slot)
        default = next((r.get("MaleMesh") or r.get("FemaleMesh") for r in rows), None)  # any piece: it is passed at runtime
        piece = mesh_parameter(co, slot + MESH_SUFFIX, default or base, -3000, y)
        # The shape the piece was made on: Fit Base, morphed onto the piece's source body (its row's fit).
        fit = switch(co, enum(co, slot + FIT_SUFFIX, [NO_FIT, *FIT_SOURCES], NO_FIT, -2850, y + 100), -2550, y + 100)
        link(fit_base, "Skeletal Mesh", fit, NO_FIT)
        for j, source in enumerate(FIT_SOURCES):
            morph = node(co, "CONodeSkeletalMeshMorph", -2700, y + 150 + 100 * j, MorphTargetName=FIT_PREFIX + source)
            link(fit_base, "Skeletal Mesh", morph, "Skeletal Mesh")
            link(fit_strength, "Float", morph, "Factor")
            link(morph, "Skeletal Mesh", fit, source)
        reshape = node(co, "CONodeSkeletalMeshReshape", -2400, y)
        link(piece, "Skeletal Mesh", reshape, "Base Skeletal Mesh")
        link(fit, "Skeletal Mesh", reshape, "Base Shape")
        link(built, "Skeletal Mesh", reshape, "Target Shape")
        worn = switch(co, enum(co, slot, [ON, OFF], OFF, -2400, y + 200), -2100, y)
        link(reshape, "Skeletal Mesh", worn, ON)
        pieces.append(worn)

        # Its coverage volume, as made on the piece's fit shape (close to the average build): reshaping it to the built
        # body tore holes and spikes through the merged pieces (Mutable 5.8 clipping with a reshaped mesh, also reported
        # on the forums), so slim, heavy and own builds hide roughly what the average one would.
        clip_mesh = mesh_parameter(co, slot + CLIP_SUFFIX, CLIP_DEFAULT, -3000, y + 350)
        hides = switch(co, enum(co, slot + HIDES_SUFFIX, [ON, OFF], OFF, -2400, y + 450), -2100, y + 350)
        link(clip_mesh, "Skeletal Mesh", hides, ON)
        clip = node(co, "CONodeClipSkeletalMeshWithSkeletalMesh", -1800, y + 350)
        link(current, current_pin, clip, "Base Skeletal Mesh")
        link(hides, "Skeletal Mesh", clip, "Clip Skeletal Mesh")
        current, current_pin = clip, "Skeletal Mesh"
    for i, worn in enumerate(pieces):
        merge = node(co, "CONodeSkeletalMeshMerge", -1200, 800 + 700 * i)
        link(current, current_pin, merge, "Base")
        link(worn, "Skeletal Mesh", merge, "Added")
        current, current_pin = merge, "Skeletal Mesh"

    obj_make = node(co, "CONodeSkeletalMeshObjectMake", -600, 0, SkeletalMeshName="Body")
    component = node(co, "CONodeComponentSkeletalMesh", -300, 0, ReferenceSkeletalMesh=base)
    link(current, current_pin, obj_make, "Skeletal Mesh")
    link(obj_make, "Passthrough Skeletal Mesh", component, "Passthrough Skeletal Mesh")
    link(component, "Component", root, "Components")

    state = unreal.CustomizableObjectEditorFunctionLibrary.compile_customizable_object_synchronously(co)
    EAL.save_loaded_asset(co, only_if_is_dirty=False)
    names = [co.get_parameter_name(i) for i in range(co.get_parameter_count())]
    log(f"CO_Shinobi: compile {state}; parameters {names}")
    if state != unreal.CustomizableObjectCompilationState.COMPLETED:
        raise RuntimeError("CO_Shinobi did not compile")


STAGES = {"bases": bases, "faceskin": face_skin, "samplecopy": sample_copy, "fabimport": fab_import, "fits": fits, "heads": heads, "catalog": catalog, "skintone": skintone, "skinlayer": skinlayer, "tattoos": tattoos, "makeup": makeup, "palette": palette, "thumbnails": thumbnails, "coverage": coverage, "wardrobe": wardrobe, "graph": graph}
for stage in ONLY or [s for s in STAGES if s != "faceskin"]:
    STAGES[stage]()
