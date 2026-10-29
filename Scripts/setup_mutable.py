"""CO_Shinobi's content: the Mutable character customisation (docs/superpowers/specs/2026-10-29-mutable-customisation-design.md).

  Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py            (every stage, in order)
  $env:SETUP_MUTABLE_ONLY="graph"; Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py

(-Editor: the Customizable Object factory needs Slate.) The content stays local, like the project's other MetaHuman content:
these stages rebuild it.

bases: each sex's base body (/Game/Shinobi/Mutable/Bases/SK_MutableBase_<Sex>) = the average MetaHuman body built by
  Scripts/mutable/base_bodies.py, with morphs Build_Under, Build_Over (the slim and heavy builds) and Build_Own (RamilMH's
  or BikaMH's own body) baked in by UShinobiFaceBlendLibrary (same vertex layout, aligned at the root).
palette: DT_Palette, the wardrobe colours (FShinobiPaletteRow).
wardrobe: DT_Wardrobe_<Slot> per slot (FShinobiWardrobeRow): data the game passes to CO_Shinobi at runtime, so rows can be
  added without recompiling. For now one City Sample piece per Top, Bottom and Shoes (a sex's first style of the slot).
graph: CO_Shinobi, built with UShinobiMutableGraphLibrary. Body parameter -> Build switch over the build morphs (scaled by
  Build Blend) -> each slot's clip (when "<Slot> Hides" is ON) -> each slot's piece merged in (when "<Slot>" is ON), its
  "<Slot> Mesh" reshaped from the base to the built body -> the Body component.
"""
import json
import os
import sys

import unreal

sys.path.append(os.path.join(os.path.dirname(__file__), "mutable"))
from names import (BASE, BUILDS, CATALOG, CLIP_SUFFIX, HIDES_SUFFIX, MESH_SUFFIX, MUTABLE_ROOT, OBJECT, OFF, ON, OWN_BODIES,  # noqa: E402
                   PALETTE, PARAM_BODY, PARAM_BUILD, PARAM_BUILD_BLEND, SLOTS, WARDROBE_TABLE)

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
        report = unreal.ShinobiFaceBlendLibrary.bake_face_blend_morphs(base, others, ["Under", "Over", "Own"], 4, "root", "Build_", 60.0)
        EAL.save_loaded_asset(base, only_if_is_dirty=False)
        log(f"{sex} base {path}: {report}")


def new_table(path, struct, rows):
    """A Data Table of struct at path with rows ({column: value} keyed "Name"), replacing any old one."""
    replace(path)
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct.static_struct())
    folder, name = path.rsplit("/", 1)
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.DataTable, factory)
    if rows and not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError(f"could not fill {path}")
    EAL.save_loaded_asset(table, only_if_is_dirty=False)
    return table


def palette():
    rows = [{"Name": name, "DisplayName": name, "Colour": {"R": r, "G": g, "B": b, "A": 1.0}} for name, (r, g, b) in PALETTE_ROWS]
    new_table(PALETTE, unreal.ShinobiPaletteRow, rows)
    log(f"DT_Palette: {len(rows)} colours")


def wardrobe_rows(slot):
    """The slot's rows: for now one City Sample piece (each sex's first catalog style of the slot, average build)."""
    styles = unreal.load_asset(CATALOG).get_editor_property("styles")
    enum_slot = getattr(unreal.ShinobiWardrobeSlot, slot.upper())

    def first(sex):
        for style in styles:
            if style.get_editor_property("sex") == sex and style.get_editor_property("slot") == enum_slot:
                return style.get_editor_property("by_build")[1].get_path_name()
        return None

    male, female = first(unreal.ShinobiSex.MALE), first(unreal.ShinobiSex.FEMALE)
    return [{"Name": "CitySample1", "MaleMesh": male, "FemaleMesh": female, "bHasWear": False}] if male and female else []


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
    replace(OBJECT)
    params = unreal.NewCustomizableObjectParameters()
    params.set_editor_property("package_path", OBJECT.rsplit("/", 1)[0])
    params.set_editor_property("asset_name", OBJECT.rsplit("/", 1)[1])
    co = unreal.CustomizableObjectEditorFunctionLibrary.new_customizable_object(params)
    if not co:
        raise RuntimeError(f"could not create {OBJECT}")
    root = G.find_node(co, "CustomizableObjectNodeObject", 0)
    base = asset_path(BASE.format(sex="Male"))

    # Body: the sex's base (given at runtime), morphed toward the chosen build.
    body = mesh_parameter(co, PARAM_BODY, base, -3000, 0)
    blend = node(co, "CustomizableObjectNodeFloatParameter", -3000, 300, ParameterName=PARAM_BUILD_BLEND, DefaultValue="1.0")
    build = enum(co, PARAM_BUILD, BUILDS, "Normal", -3000, 450)
    built = switch(co, build, -2400, 0)
    for i, value in enumerate(BUILDS):
        if value == "Normal":
            link(body, "Skeletal Mesh", built, value)
            continue
        morph = node(co, "CONodeSkeletalMeshMorph", -2700, 150 * i, MorphTargetName=f"Build_{value}")
        link(body, "Skeletal Mesh", morph, "Skeletal Mesh")
        link(blend, "Float", morph, "Factor")
        link(morph, "Skeletal Mesh", built, value)

    # Each slot: its clip hides body skin (Hides ON), its piece is reshaped to the build and merged in (slot ON).
    current, current_pin = built, "Skeletal Mesh"
    pieces = []
    for i, slot in enumerate(SLOTS):
        y = 800 + 700 * i
        rows = wardrobe_rows(slot)
        default = rows[0]["MaleMesh"] if rows else None  # a parameter's default: any piece (it is passed at runtime)
        piece = mesh_parameter(co, slot + MESH_SUFFIX, default or base, -3000, y)
        reshape = node(co, "CONodeSkeletalMeshReshape", -2400, y)
        link(piece, "Skeletal Mesh", reshape, "Base Skeletal Mesh")
        link(body, "Skeletal Mesh", reshape, "Base Shape")
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


STAGES = {"bases": bases, "palette": palette, "wardrobe": wardrobe, "graph": graph}
for stage in ONLY or list(STAGES):
    STAGES[stage]()
