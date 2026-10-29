# Mutable Customisation P2 (Roster) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every roster character becomes a Mutable preset (its own head on the MetaHuman base body of its sex), shown side by side with its legacy body for the user's approval, then the legacy bodies are retired.

**Architecture:** Seven static heads are grafted onto the Mutable bases with `graft_head.py` and imported beside them. The creator catalog gains those heads, the RamilMH and BikaMH MetaHuman faces, and one preset look per roster character. `UShinobiBodyComponent::DressBuilt` wears a look's static head (and tints the body skin to it) instead of a MetaHuman face. Roster picks wear their preset behind the `Shinobi.RosterPresets` console variable until the user approves the side-by-side shots.

**Tech Stack:** UE 5.8 C++, Mutable 1.8.0, Unreal Python (`Scripts/run_python.ps1`, `-Editor` / `-Render`), Blender 5.2 headless (`graft_head.py`), UE automation tests.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md` (phase P2). P1 record: `.superpowers/sdd/2026-10-29-mutable-customisation-p0-p1/progress.md`.

## Global Constraints

- Everything from the P0–P1 plan's Global Constraints still holds (UE 5.8, `/Game/Shinobi/Mutable/`, setup jobs never in the user's open editor, commits limited to own paths, no branch switch with the editor open).
- Mutable content stays local and untracked; the scripts regenerate it (P1 Task 5 ruling).
- Base sex per grafted head (spec): Female: Bika, Cat. Male: Hero, Misha, Pabu, Ramil, Elvin. RamilMH and BikaMH keep their MetaHuman faces and wear the `Own` build.
- Donor models: `SourceArt/Character/Models/<Name>/<Name>_rigged.fbx` and `.png` (all seven present).
- Head material: the donor's existing `/Game/Shinobi/Characters/<Name>/MI_<Name>_<Name>_rigged`; neck slot: a material instance of `/Game/Shinobi/Characters/M_Character` with `BaseColor` = the graft's `_neck.png` (the P0 Ramil spike pattern in `Scripts/setup_creator.py` `spike()`).
- Body skin tint parameter: `Basecolor Global Multiply Post-Bake` (vector) on the bases' body material (`MI_Body_Baked_VT`, parent `M_skin_unified_baked`).
- Skeletal-mesh FBX export from a script needs `Scripts/run_python.ps1 -Render`; creating or editing `CO_Shinobi` needs `-Editor`.

## Review Focus

1. **A roster pick on a machine without the Mutable content** (or with `CO_Shinobi` missing) still shows that character's legacy body. Test in Task 5.
2. **Switching from a static-head look to a MetaHuman-face look and back** leaves exactly one head: no static head under a MetaHuman face, no face over a static head. Test in Task 4.
3. **The `Own` build** puts the face or head on the neck (the skeleton follows the morph), for both sexes. Test in Task 1.
4. **Two machines resolving the same roster pick** wear the same look: a preset is fixed catalog data, never local edits. Test in Task 5.
5. **A roster character's skin under clothes and at the neck** matches its head closely enough that the join doesn't read as a collar. Checked by eye in Task 6's side-by-side shots.

---

### Task 1: The Own build moves the skeleton

**Files:**
- Modify: `Scripts/setup_mutable.py` (`graph()`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Produces: in `CO_Shinobi`, each build morph's output goes through a `CONodeSkeletalMeshReshape` (Base Skeletal Mesh = the morphed body, Base Shape = the `Body` parameter, Target Shape = the morphed body) with `bReshapeVertices = false`, `bReshapePose = true` and `SelectionMethod` covering every bone (the enum value that deforms all bones; check `EBoneDeformSelectionMethod` in the Mutable headers and record which one in the graph-library header's pin table). The switch's `Under`, `Over` and `Own` inputs take the reshaped meshes.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.OwnMovesHead` (latent, `FMutableWearer`): for each sex, wear `Build = Own` with `BuildBlend = 255`; after the build, the generated mesh's ref-pose `head` bone height (`FMutableWearer::Height()`) is within 2 cm of the own body's (`names.py` `OWN_BODIES`: RamilMH or BikaMH body asset) ref-pose `head` height, and differs from the base's by more than 3 cm.
- [ ] **Step 2:** Run `Scripts/run_tests.ps1 -Filter ShinobiCoop.Mutable.OwnMovesHead` → FAIL (the head stays at the base's height).
- [ ] **Step 3:** Add the pose-reshape nodes in `graph()`; regenerate with `SETUP_MUTABLE_ONLY=graph`.
- [ ] **Step 4:** Run the test → PASS; run `-Filter ShinobiCoop.Mutable` → PASS.
- [ ] **Step 5: Commit** `feat: Mutable builds move the skeleton with the body`.

### Task 2: Static heads grafted onto the Mutable bases

**Files:**
- Modify: `Scripts/setup_mutable.py` (stage `heads`; runs with `-Render`)
- Modify: `Scripts/mutable/names.py` (`HEADS`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Produces: `names.py` `HEADS = {"Hero": "Male", "Bika": "Female", "Cat": "Female", "Misha": "Male", "Pabu": "Male", "Ramil": "Male", "Elvin": "Male"}` (catalog order = this order); assets `/Game/Shinobi/Mutable/Heads/SK_Head_<Name>` on the base's skeleton with two material slots (head, `*_Neck`), plus `T_Head_<Name>_Neck` and `MI_Head_<Name>_Neck`; `Saved/MutableHeads/<Name>_tint.json` holding the neck texture's mean linear colour divided by the base body base colour's mean (the body tint).
- Stage `heads`, per name: export `SK_MutableBase_<Sex>` and `/Game/Shinobi/Mutable/Bases/Build/MHC_MB_<Sex>_Normal/Face/SKM_MHC_MB_<Sex>_Normal_FaceMesh` to `Saved/MutableHeads/` (FBX via `AssetExportTask`, as `setup_creator.py` frames do; morph export off); export the base body base-colour texture to PNG (`--body-texture`); run `graft_head.py -- <body.fbx> <face.fbx> <donor_rigged.fbx> Saved/MutableHeads/Head_<Name>.fbx --texture <donor_rigged.png> --body-texture <body.png> --render Saved/MutableHeads/<Name>` with `hero.BLENDER`; import as in `setup_creator.spike()`.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.HeadsImported`: for each of the seven names, `SK_Head_<Name>` loads, its skeleton is the base's of its sex, it has a `head` bone, and exactly one material slot whose name ends in `_Neck`.
- [ ] **Step 2:** Run → FAIL (assets missing).
- [ ] **Step 3:** Implement the stage; run `SETUP_MUTABLE_ONLY=heads` with `-Render`. Look at each `--render` output for the neck join before importing; a bad join is fixed with `--join`/`--scale` per name in `HEADS` (make each value a tuple `(sex, extra_args)` only if needed).
- [ ] **Step 4:** Run the test → PASS.
- [ ] **Step 5: Commit** script, names and test: `feat: roster heads grafted onto the Mutable bases`.

### Task 3: Catalog heads, MetaHuman faces and roster presets

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiCreatorCatalog.h/.cpp`
- Modify: `Scripts/setup_mutable.py` (stage `catalog`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Produces:
  - `USTRUCT() FShinobiCreatorHead { FName Id; EShinobiSex Sex; TSoftObjectPtr<USkeletalMesh> Mesh; FLinearColor SkinTint = FLinearColor::White; }` and `UPROPERTY(EditAnywhere) TArray<FShinobiCreatorHead> Heads;` on `UShinobiCreatorCatalog`; `const FShinobiCreatorHead* FindHead(uint8 Index) const` (null for None or out of range).
  - `USTRUCT() FShinobiRosterPreset { FName Roster; FShinobiAppearance Look; }`, `UPROPERTY(EditAnywhere) TArray<FShinobiRosterPreset> Presets;`, `const FShinobiAppearance* FindPreset(FName Roster) const`.
  - `Faces` gains `RamilMH`, `RamilMHScan` (male) and `BikaMH`, `BikaMHKeenFace` (female) with `OwnGrooms` = every groom binding under their build's `Grooms` folder.
  - Presets (stage `catalog`, after `heads`): one per `AShinobiPlayerCharacter::Roster` name. Static-head characters: `Head` = their `Heads` index, `Sex` per `HEADS`, `Build = Normal`, `Hair = Beard = Mustache = None`, `Top = Bottom = Shoes = 0`. RamilMH / BikaMH: `Head = None`, `Face` = their `Faces` index, `Build = Own`, `BuildBlend = 255`, grooms None (their `OwnGrooms` carry hair).
  - `SkinTint` from `Saved/MutableHeads/<Name>_tint.json`.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.CatalogPresets`: the catalog has a preset for every `Roster[i].Name`; each static-head preset's `FindHead(Look.Head)` is non-null with `Sex == Look.Sex`; RamilMH's and BikaMH's presets have `Head == None`, `Build == Own` and a `FindFace(Look.Sex, Look.Face)` whose `Id` is `RamilMH` / `BikaMH`; every `SkinTint` channel is in 0.2..3.
- [ ] **Step 2:** Run → FAIL (no such members: compile error).
- [ ] **Step 3:** Implement the structs, finders and the `catalog` stage (load `DA_CreatorCatalog`, set the arrays, save).
- [ ] **Step 4:** Run the test and `-Filter ShinobiCoop.Creator` → PASS.
- [ ] **Step 5: Commit** `feat: roster presets, heads and MetaHuman faces in the creator catalog`.

### Task 4: Wearing a static head with a tinted body

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiBodyComponent.h/.cpp` (`DressBuilt`, `GetStaticHead()`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 3 `FindHead`, `SkinTint`.
- Produces: `USkeletalMeshComponent* UShinobiBodyComponent::GetStaticHead() const`. In `DressBuilt`, when `FindHead(Look.Head)` exists: `WearStaticHead(head mesh)`, no MetaHuman face or grooms, and every body material slot whose material has `Basecolor Global Multiply Post-Bake` gets a dynamic instance with that parameter = `SkinTint`. Otherwise the P1 MetaHuman-face path, with the static head removed and the tint reset to white.

- [ ] **Step 1: Write the failing tests** (latent):
  - `ShinobiCoop.Mutable.StaticHeadWorn`: wear the Cat preset look; after the build, `GetStaticHead()` wears `SK_Head_Cat`, `GetMetaHumanParts()` is empty, and the body's skin parameter equals the Cat head's `SkinTint`.
  - `ShinobiCoop.Mutable.OneHeadAtATime`: wear the Cat look, then the RamilMH look, then the Cat look (each after the previous build); after each: exactly one of {static head mesh set, a `Face*` MetaHuman part} holds.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS.
- [ ] **Step 5: Commit** `feat: made looks wear a roster head with a matching body tint`.

### Task 5: Roster picks wear their preset (behind a switch)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h/.cpp` (`ApplyCharacter`, `RosterPreset`)
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiMenuStage.cpp` (`Show`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 3 `FindPreset`.
- Produces:
  - Console variable `Shinobi.RosterPresets` (int, default 0 until Task 7): 1 = roster picks wear their preset.
  - `static const FShinobiAppearance* AShinobiPlayerCharacter::RosterPreset(int32 RosterIndex, int32 FaceIndex = 0)`: the catalog preset of `Roster[RosterIndex].Name`; for a MetaHuman roster character with `FaceIndex > 0`, the preset with `Face` = that face's catalog index (`Faces` entry named by `FShinobiFaceEntry::Name`'s build folder: `RamilMHScan`, `BikaMHKeenFace`); null when the switch is off or there is no preset. P4's CREATE tab copies it into the editor.
  - `ApplyCharacter`: for a roster pick with a preset and the switch on, `WearAppearance(*Preset)`; if that returns false, the legacy `Wear` loop as today. The menu stage's `Show` does the same.

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.RosterPresetWorn` (latent): with the switch set to 1, a player character picking Cat wears a generated (transient) body with `SK_Head_Cat`; two characters picking Cat end with equal `GetAppearance().ToCode()`.
  - `ShinobiCoop.Mutable.RosterPresetFallsBack`: with the switch 1 and `UShinobiMutableLook::ObjectPathOverride` missing (expected error once), picking Cat wears the legacy `Roster[Cat].Mesh`.
  - Both reset the switch to its previous value at the end.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable`, `-Filter ShinobiCoop.UI`, `-Filter ShinobiCoop.Hero` → PASS (Hero's `PickSwapsBody` failure from before P2 is reported separately if still failing).
- [ ] **Step 5: Commit** `feat: roster picks can wear their Mutable preset (Shinobi.RosterPresets)`.

### Task 6: Side-by-side shots and the user's review

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCreatorSpikeShot.cpp` (`RunRosterPresetShot`), `ShinobiPlayerController.h/.cpp`, `Scripts/shot_chakra.ps1` (`-RosterPresets`)

- [ ] **Step 1:** The shot: for each roster character, the legacy body (switch 0) then the preset (switch 1), front at full body and a 110 cm head close-up, named `ShinobiRosterPresetShot_<Name>_<Legacy|Preset>_<Body|Head>`. Wait for each Mutable build as `RunCreatorMutableShot` does.
- [ ] **Step 2:** Run `Scripts/shot_chakra.ps1 -RosterPresets -Map /Game/ThirdPerson/Lvl_ThirdPerson`; build one contact sheet per character (legacy left, preset right).
- [ ] **Step 3: Commit** `test: roster preset side-by-side shots`.
- [ ] **Step 4: Stop.** Send the user the contact sheets with what differs and why (body proportions now the MetaHuman base's, clothes are City Sample pieces until P3, the cloaks don't fit the new bodies). Wait for approval per character before Task 7; characters the user rejects keep their legacy body (their preset stays in the catalog for the CREATE tab).

### Task 7: Retire the legacy bodies (after approval)

**Files:**
- Modify: `ShinobiPlayerCharacter.cpp` (switch default 1, or per-character approval list if the user approved only some)
- Modify: tests that assume a roster pick wears `Roster[i].Mesh` (`ShinobiHeroTests.cpp` `PickSwapsBody`, `ShinobiMenuTests.cpp` stage tests, `ShinobiOutfitTests.cpp` cloak-copy tests): they assert the preset (generated body + head) for approved characters.

- [ ] **Step 1:** Change the failing tests' expectations to the approved behaviour first; run them → FAIL against switch default 0.
- [ ] **Step 2:** Set the switch default (or approval list); run the changed tests → PASS.
- [ ] **Step 3:** Full suite `Scripts/run_tests.ps1` once → PASS except failures that predate P2 (named in the report).
- [ ] **Step 4: Commit** `feat: roster characters wear their Mutable presets`.
- [ ] **Step 5:** Phase review: send the user the suite result, the rulings and deferred minors; packaged smoke (`Scripts/package.ps1 -SkipBuild` if only content changed, then `shot_chakra.ps1 -RosterPresets -Packaged`).
