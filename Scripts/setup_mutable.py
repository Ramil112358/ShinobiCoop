"""CO_Shinobi's content: the Mutable character customisation (docs/superpowers/specs/2026-10-29-mutable-customisation-design.md).

  Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py            (every stage, in order)
  $env:SETUP_MUTABLE_ONLY="graph"; Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py

(-Editor: the Customizable Object factory needs Slate.) The content stays local, like the project's other MetaHuman content:
these stages rebuild it.

bases: each sex's base body (/Game/Shinobi/Mutable/Bases/SK_MutableBase_<Sex>) = the average MetaHuman body built by
  Scripts/mutable/base_bodies.py, with morphs Build_Under and Build_Over (the slim and heavy builds) baked in by
  UShinobiFaceBlendLibrary (same vertex layout, aligned at the root); and SK_MutableOwn_<Sex>, RamilMH's or BikaMH's own
  body (the Own build). Both skin "head" a little (UShinobiMutableMeshLibrary::add_bone_skinning).
heads (needs run_python.ps1 -Render: FBX export): each roster character's head (names.HEADS) grafted onto its sex's base
  by Scripts/character/graft_head.py (donor SourceArt/Character/Models/<Name>/<Name>_rigged.fbx/.png, the base and its
  average build's MetaHuman face for the neck) and imported as /Game/Shinobi/Mutable/Heads/SK_Head_<Name> with the
  donor's material and a neck material; Saved/MutableHeads/<Name>_tint.json is the body tint that matches its skin
  (the neck texture's mean colour over the base body's).
catalog: DA_CreatorCatalog's Heads (with their tints), the roster MetaHuman faces and a preset per roster character
  (run after setup_creator.py catalog, which rewrites Faces).
palette: DT_Palette, the wardrobe colours (FShinobiPaletteRow).
wardrobe: DT_Wardrobe_<Slot> per slot (FShinobiWardrobeRow): data the game passes to CO_Shinobi at runtime, so rows can be
  added without recompiling. Top, Bottom and Shoes hold every City Sample catalog style, index-aligned per sex.
graph: CO_Shinobi, built with UShinobiMutableGraphLibrary. Body parameter -> Build switch over the build morphs (scaled by
  Build Blend) -> pose reshape (the skeleton follows the build) -> each slot's clip (when "<Slot> Hides" is ON) -> each slot's piece merged in (when "<Slot>" is ON), its
  "<Slot> Mesh" reshaped from the base to the built body -> the Body component.
"""
import json
import os
import sys

import unreal

sys.path += [os.path.dirname(os.path.abspath(__file__)), os.path.join(os.path.dirname(__file__), "mutable")]
from names import (BASE, BUILDS, CATALOG, HEADS, HEADS_DIR, CLIP_SUFFIX, HIDES_SUFFIX, MESH_SUFFIX, MUTABLE_ROOT, OBJECT, OFF, ON, OWN, OWN_BODIES,  # noqa: E402
                   PALETTE, PARAM_BODY, PARAM_BUILD, PARAM_BUILD_BLEND, PARAM_FIT_BASE, SLOTS, WARDROBE_TABLE)

ONLY = [s for s in os.environ.get("SETUP_MUTABLE_ONLY", "").split(",") if s]
EAL = unreal.EditorAssetLibrary
G = unreal.ShinobiMutableGraphLibrary
BASES = f"{MUTABLE_ROOT}/Bases"
SEXES = ("Male", "Female")
# A clip parameter's default (never worn: "<Slot> Hides" is OFF unless a row gives a clip mesh).
CLIP_DEFAULT = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"
PALETTE_ROWS = [("Black", (0.02, 0.02, 0.02)), ("White", (0.85, 0.85, 0.82)), ("Grey", (0.25, 0.25, 0.26)),
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
    if EAL.does_asset_exist(path) and not EAL.delete_asset(path):
        raise RuntimeError(f"could not delete {path} (open in an editor?)")


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
        # Bodies skin the head a little, so a merged piece skinned to it can't put its own body's head pose on the skeleton.
        headed = unreal.ShinobiMutableMeshLibrary.add_bone_skinning(base, "head")
        EAL.save_loaded_asset(base, only_if_is_dirty=False)
        log(f"{sex} base {path}: {report}; head skinned on {headed} vertices")
        # The Own build: RamilMH's or BikaMH's own body (exact joints), worn as the Body with the pieces reshaped onto it.
        own = OWN.format(sex=sex)
        replace(own)
        copy = EAL.duplicate_asset(OWN_BODIES[sex], own)
        headed = unreal.ShinobiMutableMeshLibrary.add_bone_skinning(copy, "head")
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
        build = f"/Game/Shinobi/MetaHumans/{face_id}/Build/MHC_{face_id}"
        # Worn through its blueprint (its Fab hair, groom materials and LODSync are set there), as on its roster body.
        faces.append(make(unreal.ShinobiCreatorFace, id=face_id, sex=getattr(unreal.ShinobiSex, sex.upper()),
                          mesh=unreal.load_asset(f"{build}/Face/SKM_MHC_{face_id}_FaceMesh"),
                          meta_human=EAL.load_blueprint_class(f"{build}/BP_MHC_{face_id}")))
    data.set_editor_property("faces", faces)

    def face_index(face_id):
        sex = getattr(unreal.ShinobiSex, MH_FACES[face_id].upper())
        return [f for f in faces if f.get_editor_property("sex") == sex].index(next(f for f in faces if str(f.get_editor_property("id")) == face_id))

    data.set_editor_property("presets", [])
    for i, (name, sex) in enumerate(HEADS.items()):
        data.set_preset(name, getattr(unreal.ShinobiSex, sex.upper()), i, 0, unreal.ShinobiBuild.NORMAL)
    for name in ("RamilMH", "BikaMH"):
        data.set_preset(name, getattr(unreal.ShinobiSex, MH_FACES[name].upper()), 255, face_index(name), unreal.ShinobiBuild.OWN)
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
            row["MaleMesh"] = male[index]
        if index < len(female):
            row["FemaleMesh"] = female[index]
        rows.append(row)
    return rows


def wardrobe():
    for slot in SLOTS:
        rows = wardrobe_rows(slot)
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
        reshape = node(co, "CONodeSkeletalMeshReshape", -2400, y)
        link(piece, "Skeletal Mesh", reshape, "Base Skeletal Mesh")
        link(fit_base, "Skeletal Mesh", reshape, "Base Shape")
        link(built, "Skeletal Mesh", reshape, "Target Shape")
        worn = switch(co, enum(co, slot, [ON, OFF], OFF, -2400, y + 200), -2100, y)
        link(reshape, "Skeletal Mesh", worn, ON)
        pieces.append(worn)

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


STAGES = {"bases": bases, "heads": heads, "catalog": catalog, "palette": palette, "wardrobe": wardrobe, "graph": graph}
for stage in ONLY or list(STAGES):
    STAGES[stage]()
