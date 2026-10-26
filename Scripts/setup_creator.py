"""Character creator assets (docs: C:/Users/ahmad/.claude/plans/i-want-to-use-proud-curry.md). Close the GUI editor.

  SETUP_CREATOR_SPIKE=<dir> Scripts/run_python.ps1 -Script Scripts/setup_creator.py
  SETUP_CREATOR_ONLY=catalog Scripts/run_python.ps1 -Script Scripts/setup_creator.py

catalog: /Game/Shinobi/Characters/Creator/DA_CreatorCatalog (UShinobiCreatorCatalog) from the City Sample crowd
  (Content/CitySampleCrowd/Character, everything on metahuman_base_skel): the 6 frames (<m|f>_tal_<unw|nrw|ovw>_body),
  the faces (<m|f>_00N/Face/<id>_nrw_FaceMesh: one head serves all three builds), the grooms bound to each face
  (<Sex>/GroomBindings/GB_<face>_nrw_FaceMesh_<groom>) and the wardrobe styles fitted to every build (the build's
  Meshes/ folder: each name minus <m|f>_tal_<build>_, slotted shoes / bottom / top by name).

Phase 0 spike (SETUP_CREATOR_SPIKE: a folder holding the Blender outputs):
  MHBase_keys.fbx            -> /Game/Shinobi/Characters/Creator/SK_MHBase: the MetaHuman body with body morph targets,
                                on the MetaHuman skeleton, wearing the RamilMH body's materials and physics asset
  Head_Ramil.fbx (+ _neck.png) -> /Game/Shinobi/Characters/Creator/SK_Head_Ramil: a static head (graft_head.py)
  Akatsuki_RamilMH_Morph_outfit.fbx -> /Game/Shinobi/Outfits/Akatsuki/SK_Akatsuki_RamilMH_Morph (cloth, hide mask; worn
                                with Shinobi.OutfitCopySuffix _Morph) carrying the body's morphs (transfer_morphs.py)
"""
import os
import re
import sys

import unreal

os.environ["SETUP_HERO_ONLY"] = "library"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_hero as hero  # noqa: E402

CREATOR_DIR = "/Game/Shinobi/Characters/Creator"
MH_BODY = "/Game/Shinobi/MetaHumans/RamilMH/Build/MHC_RamilMH/Body/SKM_MHC_RamilMH_BodyMesh"
MH_CLOTH_COLLISION = "/Game/Shinobi/MetaHumans/RamilMH/PA_RamilMH_Cloth"
EAL = unreal.EditorAssetLibrary


def log(message):
    unreal.log(f"[setup_creator] {message}")


def import_skeletal(fbx, dest, name, skeleton=None, morphs=True, vertex_colours=False):
    """An FBX (already unit-scale: clean_rig/export_unit_scale_fbx output) as a skeletal mesh, with its shape keys as
    morph targets; on skeleton when given (no physics asset, no materials)."""
    for stale in (name, f"{name}_Skeleton", f"{name}_PhysicsAsset"):
        if EAL.does_asset_exist(f"{dest}/{stale}"):
            EAL.delete_asset(f"{dest}/{stale}")
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.create_physics_asset = False
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    if skeleton is not None:
        options.skeleton = skeleton
    data = options.skeletal_mesh_import_data
    data.set_editor_property("import_morph_targets", morphs)
    if vertex_colours:
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = fbx, dest, name
    task.automated = task.save = task.replace_existing = True
    task.options = options
    hero.TOOLS.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, unreal.SkeletalMesh)), None)
    if mesh is None:
        raise RuntimeError(f"import of {fbx} produced no skeletal mesh")
    morph_names = [m.get_name() for m in mesh.get_editor_property("morph_targets")]
    log(f"{name}: imported, morphs {morph_names}")
    return mesh


def material_instance(name, dest, texture, parent):
    instance = hero.load_or_create(name, lambda: hero.TOOLS.create_asset(name, dest, unreal.MaterialInstanceConstant,
                                                                         unreal.MaterialInstanceConstantFactoryNew()), dest)
    hero.MEL.set_material_instance_parent(instance, parent)
    hero.MEL.set_material_instance_texture_parameter_value(instance, "BaseColor", texture)
    EAL.save_loaded_asset(instance)
    return instance


def spike(folder):
    mh_body = unreal.load_asset(MH_BODY)
    if mh_body is None:
        raise RuntimeError(f"missing {MH_BODY} (Scripts/setup_metahuman.py)")
    skeleton = mh_body.get_editor_property("skeleton")
    parent = unreal.load_asset(f"{hero.CHARACTERS_DIR}/M_Character")

    body = import_skeletal(os.path.join(folder, "MHBase_keys.fbx"), CREATOR_DIR, "SK_MHBase", skeleton)
    body.set_editor_property("materials", mh_body.get_editor_property("materials"))
    body.set_editor_property("physics_asset", mh_body.get_editor_property("physics_asset"))
    EAL.save_loaded_asset(body)

    head = import_skeletal(os.path.join(folder, "Head_Ramil.fbx"), CREATOR_DIR, "SK_Head_Ramil", skeleton, morphs=False)
    neck_texture = hero.import_texture_file(os.path.join(folder, "Head_Ramil_neck.png"), CREATOR_DIR, "T_Head_Ramil_Neck")
    face = unreal.load_asset(f"{hero.CHARACTERS_DIR}/Ramil/MI_Ramil_Ramil_rigged")
    slots = []
    for slot in head.get_editor_property("materials"):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = material_instance("MI_Head_Ramil_Neck", CREATOR_DIR, neck_texture, parent) if slot_name.endswith("_Neck") else face
        slots.append(unreal.SkeletalMaterial(material_interface=material, material_slot_name=slot_name))
    head.set_editor_property("materials", slots)
    EAL.save_loaded_asset(head)
    log(f"SK_Head_Ramil: slots {[str(s.get_editor_property('material_slot_name')) for s in slots]}")

    dest = f"{hero.OUTFITS_GAME_DIR}/Akatsuki"
    outfit = import_skeletal(os.path.join(folder, "Akatsuki_RamilMH_Morph_outfit.fbx"), dest, "SK_Akatsuki_RamilMH_Morph",
                             vertex_colours=True)
    png = os.path.join(hero.OUTFITS_DIR, "Akatsuki", "Akatsuki_outfit.png")
    hero.use_materials(outfit, "Akatsuki", dest, lambda slot: png, parent)
    collision = unreal.load_asset(MH_CLOTH_COLLISION)
    if not unreal.ShinobiOutfitBuilderLibrary.build_cloth(outfit, collision, hero.HEM_MAX_DISTANCE_CM, hero.SLEEVE_MAX_DISTANCE_CM):
        raise RuntimeError("SK_Akatsuki_RamilMH_Morph: could not build its cloth")
    tuning = f"{dest}/Tuning/SK_Akatsuki_Tuning"
    if EAL.does_asset_exist(tuning):
        unreal.ShinobiOutfitBuilderLibrary.copy_cloth_tuning(unreal.load_asset(tuning), [outfit])
    EAL.save_loaded_asset(outfit)
    hero.import_hide_mask(os.path.join(hero.OUTFITS_DIR, "Akatsuki", "Akatsuki_RamilMH_hide.png"), dest,
                          "T_Akatsuki_RamilMH_Morph_Hide")
    log("spike assets done")


CROWD = "/Game/CitySampleCrowd/Character"
CREATOR_FACES = "/Game/Shinobi/MetaHumans/Creator/Build"
BUILDS = (("UNDER", "UnderWeight", "unw"), ("NORMAL", "NormalWeight", "nrw"), ("OVER", "OverWeight", "ovw"))
SHOES, BOTTOMS = ("loafers", "oxfords", "dressFlats"), ("jeans", "slacks", "skirt")
# Grooms that draw nothing on our faces: the buzz cut's cards have no mesh (City Sample paints it into the head's follicle mask).
SKIP_GROOMS = ("Hair_S_BuzzCut",)


def make(struct, **values):
    """A catalog struct (its properties are EditAnywhere, not BlueprintReadWrite: no keyword constructor)."""
    item = struct()
    for name, value in values.items():
        item.set_editor_property(name, value)
    return item


def asset_names(folder):
    return sorted({a.rsplit(".", 1)[-1] for a in EAL.list_assets(folder, recursive=False)})


def catalog():
    path = f"{CREATOR_DIR}/DA_CreatorCatalog"
    if EAL.does_asset_exist(path):
        data = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ShinobiCreatorCatalog)
        data = hero.TOOLS.create_asset("DA_CreatorCatalog", CREATOR_DIR, unreal.ShinobiCreatorCatalog, factory)
    frames, faces, grooms, bindings, styles = [], [], [], [], []
    for sex, folder, p in (("MALE", "Male", "m"), ("FEMALE", "Female", "f")):
        enum_sex = getattr(unreal.ShinobiSex, sex)
        for build, sub, tag in BUILDS:
            body = f"{CROWD}/{folder}/{sub}/Meshes/{p}_tal_{tag}_body"
            if EAL.does_asset_exist(body):
                frames.append(make(unreal.ShinobiCreatorFrame, sex=enum_sex, build=getattr(unreal.ShinobiBuild, build),
                                                         body=unreal.load_asset(body)))
        face_ids = sorted(d.rstrip("/").rsplit("/", 1)[-1] for d in EAL.list_assets(f"{CROWD}/{folder}", recursive=False, include_folder=True)
                          if re.fullmatch(rf"{p}_\d\d\d", d.rstrip("/").rsplit("/", 1)[-1]))
        for face_id in face_ids:
            mesh = f"{CROWD}/{folder}/{face_id}/Face/{face_id}_nrw_FaceMesh"
            if EAL.does_asset_exist(mesh):
                faces.append(make(unreal.ShinobiCreatorFace, id=face_id, sex=enum_sex, mesh=unreal.load_asset(mesh)))
        seen = {}
        for name in asset_names(f"{CROWD}/{folder}/GroomBindings"):
            m = re.fullmatch(rf"GB_({p}_\d\d\d)_nrw_FaceMesh_(.+)", name)
            if not m:
                continue
            face_id, groom = m.groups()
            if groom in SKIP_GROOMS:
                continue
            binding = unreal.load_asset(f"{CROWD}/{folder}/GroomBindings/{name}")
            bindings.append(make(unreal.ShinobiCreatorBinding, face=face_id, groom=groom, binding=binding))
            if groom not in seen:
                slot = "HAIR" if groom.startswith("Hair_") else "MUSTACHE" if groom.startswith("Mustache_") else "BEARD"
                seen[groom] = make(unreal.ShinobiCreatorGroom, id=groom, sex=enum_sex, slot=getattr(unreal.ShinobiGroomSlot, slot),
                                                         groom=binding.get_editor_property("groom"))
        grooms += [seen[g] for g in sorted(seen, key=lambda g: (not g.startswith("Hair_"), g))]
        # Creator faces (Scripts/character/creator_faces.py builds): every groom of the sex rebound to each one, from the
        # crowd face it was bound to (all MetaHuman faces share topology).
        for face_id in sorted(d.rstrip("/").rsplit("/", 1)[-1] for d in EAL.list_assets(CREATOR_FACES, recursive=False, include_folder=True)
                              if re.fullmatch(rf"MHC_CF_{p.upper()}\d+", d.rstrip("/").rsplit("/", 1)[-1])):
            face_id = face_id[len("MHC_"):]
            mesh = unreal.load_asset(f"{CREATOR_FACES}/MHC_{face_id}/Face/SKM_MHC_{face_id}_FaceMesh")
            if mesh is None:
                continue
            own = [unreal.load_asset(f"{CREATOR_FACES}/MHC_{face_id}/Grooms/{n}") for n in asset_names(f"{CREATOR_FACES}/MHC_{face_id}/Grooms")
                   if re.fullmatch(r"(Eyebrows|Eyelashes)_.+_Binding", n)]
            faces.append(make(unreal.ShinobiCreatorFace, id=face_id, sex=enum_sex, mesh=mesh, own_grooms=own))
            slots = [m.material_interface.get_name() if m.material_interface else "" for m in mesh.get_editor_property("materials")]
            skin = next((i for i, mi in enumerate(slots) if "Skin" in mi and "LOD1" in mi), -1)  # the head's skin section
            for groom in seen:
                source = next(b.get_editor_property("binding") for b in bindings if b.get_editor_property("groom") == groom)
                path = f"{CREATOR_DIR}/Bindings/GB_{face_id}_{groom}"
                if not EAL.does_asset_exist(path):
                    unreal.GroomLibrary.create_new_groom_binding_asset_with_path(
                        path, seen[groom].get_editor_property("groom"), mesh, 100, source.get_editor_property("target_skeletal_mesh"), skin)
                binding = unreal.load_asset(path)
                EAL.save_loaded_asset(binding)
                bindings.append(make(unreal.ShinobiCreatorBinding, face=face_id, groom=groom, binding=binding))
            log(f"Creator face {face_id}: {len(seen)} grooms bound (skin section {skin})")
        for name in asset_names(f"{CROWD}/{folder}/NormalWeight/Meshes"):
            style = name[len(f"{p}_tal_nrw_"):]
            if not name.startswith(f"{p}_tal_nrw_") or style in ("body", "base"):
                continue
            slot = "SHOES" if style in SHOES else "BOTTOM" if style.startswith(BOTTOMS) else "TOP"
            by_build = []
            for _, sub, tag in BUILDS:
                mesh = f"{CROWD}/{folder}/{sub}/Meshes/{p}_tal_{tag}_{style}"
                by_build.append(unreal.load_asset(mesh) if EAL.does_asset_exist(mesh) else None)
            styles.append(make(unreal.ShinobiCreatorStyle, id=style, sex=enum_sex, slot=getattr(unreal.ShinobiWardrobeSlot, slot), by_build=by_build))
    data.set_editor_property("frames", frames)
    data.set_editor_property("faces", faces)
    data.set_editor_property("grooms", grooms)
    data.set_editor_property("bindings", bindings)
    data.set_editor_property("styles", styles)
    EAL.save_loaded_asset(data)
    log(f"catalog: {len(frames)} frames, {len(faces)} faces, {len(grooms)} grooms, {len(bindings)} bindings, {len(styles)} styles")
    for st in styles:
        log(f"  style {st.get_editor_property('sex')} {st.get_editor_property('slot')} {st.get_editor_property('id')} "
            f"builds {[b is not None for b in st.get_editor_property('by_build')]}")
    for g in grooms:
        log(f"  groom {g.get_editor_property('sex')} {g.get_editor_property('slot')} {g.get_editor_property('id')}")


def blend_morphs():
    """Preset blending: every catalog face gets a Blend_<id> morph toward each other face of its sex
    (UShinobiFaceBlendLibrary). Each face's .uasset is copied to Saved/CreatorFaces/Backup first (City Sample and Creator
    builds are not in git); SETUP_CREATOR_FACES=<id>,<id> limits which faces are baked."""
    import shutil
    data = unreal.load_asset(f"{CREATOR_DIR}/DA_CreatorCatalog")
    faces = data.get_editor_property("faces")
    only = [f for f in os.environ.get("SETUP_CREATOR_FACES", "").split(",") if f]
    backup = os.path.join(hero.PROJECT, "Saved", "CreatorFaces", "Backup")
    os.makedirs(backup, exist_ok=True)
    for face in faces:
        face_id = str(face.get_editor_property("id"))
        if only and face_id not in only:
            continue
        mesh = face.get_editor_property("mesh").load_synchronous() if hasattr(face.get_editor_property("mesh"), "load_synchronous") else face.get_editor_property("mesh")
        others = [f for f in faces if f.get_editor_property("sex") == face.get_editor_property("sex") and f is not face
                  and str(f.get_editor_property("id")) != face_id]
        package = mesh.get_outermost().get_name()
        source = os.path.join(hero.PROJECT, "Content", package[len("/Game/"):] + ".uasset")
        target = os.path.join(backup, f"{face_id}.uasset")
        if not os.path.exists(target):
            shutil.copy2(source, target)
        report = unreal.ShinobiFaceBlendLibrary.bake_face_blend_morphs(
            mesh, [o.get_editor_property("mesh") for o in others], [str(o.get_editor_property("id")) for o in others], 4)
        EAL.save_loaded_asset(mesh)
        log(report)


FRAME_BODIES = {(sex, build): f"Crowd{sex[0]}{build.title()}" for sex in ("MALE", "FEMALE") for build, _, _ in BUILDS}


FRAME_OUTFIT = ("turtleneck", "jeans", "loafers")  # the base outfit a cloak is fitted over (compose_frame.py)


def frames():
    """Each City Sample frame as one fitting body, SourceArt/Character/Models/<Body>/<Body>_rigged.fbx (CrowdMNormal,
    CrowdFOver, ...): its hands (the frame's *_body), a crowd face for the head and neck, and FRAME_OUTFIT, joined by
    Scripts/character/compose_frame.py; build_outfits.ps1 then fits the outfits to them like any roster body. The parts
    go through Saved/CreatorFrames. Needs run_python.ps1 -Render (FBX export) and absolute paths."""
    import subprocess

    def export(mesh, path):
        task = unreal.AssetExportTask()
        task.object, task.filename, task.automated, task.prompt, task.replace_identical = mesh, path, True, False, True
        task.exporter = unreal.SkeletalMeshExporterFBX()
        task.options = unreal.FbxExportOption()
        task.options.level_of_detail = False
        task.options.export_morph_targets = False  # (the faces' blend morphs crash the exporter, and fitting needs none)
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError(f"could not export {mesh.get_path_name()}")
        return path

    parts_dir = os.path.join(hero.PROJECT, "Saved", "CreatorFrames")
    os.makedirs(parts_dir, exist_ok=True)
    for sex, folder, p in (("MALE", "Male", "m"), ("FEMALE", "Female", "f")):
        # The head and neck: an unbaked MetaHuman face (the crowd faces' blend morphs crash the FBX exporter);
        # compose_frame.py moves it onto the frame's head joint.
        face = export(unreal.load_asset("/Game/Shinobi/MetaHumans/RamilMH/Build/MHC_RamilMH/Face/SKM_MHC_RamilMH_FaceMesh"),
                      os.path.join(parts_dir, "head_face.fbx"))
        for build, sub, tag in BUILDS:
            body = FRAME_BODIES[(sex, build)]
            meshes = f"{CROWD}/{folder}/{sub}/Meshes/{p}_tal_{tag}"
            hands = export(unreal.load_asset(f"{meshes}_body"), os.path.join(parts_dir, f"{body}_hands.fbx"))
            pieces = [export(unreal.load_asset(f"{meshes}_{piece}"), os.path.join(parts_dir, f"{body}_{piece}.fbx")) for piece in FRAME_OUTFIT]
            rigged = os.path.join(hero.MODELS_DIR, body, f"{body}_rigged.fbx")
            run = subprocess.run([hero.BLENDER, "-b", "--factory-startup", "--python-exit-code", "1", "-P",
                                  os.path.join(hero.PROJECT, "Scripts", "character", "compose_frame.py"), "--", rigged, hands, face, *pieces],
                                 capture_output=True, text=True)
            if run.returncode != 0:
                raise RuntimeError(f"compose_frame failed for {body}: {run.stdout[-1500:]} {run.stderr[-800:]}")
            log(f"frame {body}: {[l for l in run.stdout.splitlines() if '[compose_frame]' in l]}")


if os.environ.get("SETUP_CREATOR_ONLY") == "frames":
    frames()
elif os.environ.get("SETUP_CREATOR_ONLY") == "blendmorphs":
    blend_morphs()
elif os.environ.get("SETUP_CREATOR_ONLY") == "catalog":
    catalog()
elif os.environ.get("SETUP_CREATOR_SPIKE"):
    spike(os.environ["SETUP_CREATOR_SPIKE"])
