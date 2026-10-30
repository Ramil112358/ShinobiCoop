# Mutable Customisation P3 (Wardrobe) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every wardrobe piece sits on every body and build without clipping or spiking: City Sample, Mutable Sample (Cyborg and its MetaHuman clothing) and optionally MetaHuman Fab pieces, each reshaped from the body it was made for, hiding the skin it covers, in the palette's colours.

**Architecture:** Mutable's Skeletal Mesh Reshape needs its Base and Target shapes to share topology. So each wardrobe source gets a *fit shape* per sex: a copy of `SK_MutableBase_<Sex>` (template topology) whose vertices are moved onto the body the source's pieces were authored on (a MetaHuman compatibility body read from its DNA, or the Mutable Sample's body). A slot's piece is then reshaped from its row's fit shape to the built body at runtime (dataless: new rows need no recompile). Coverage volumes are closed shells cut from the fit shape under each piece, reshaped the same way and clipping the body. Colours go onto tint-enabled copies of each piece's materials.

**Tech Stack:** UE 5.8 Mutable (dataless `CO_Shinobi`), RigLogic `IDNAReader`, editor C++ helpers called from `Scripts/setup_mutable.py`, Blender (`Scripts/character/skin_outfit.py`) for re-skinning Cyborg pieces, UE automation tests, `shot_chakra.ps1` contact sheets.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md` (P3 Wardrobe; Content pipeline: refit, coverage, sources). P1/P2 ledgers: `.superpowers/sdd/2026-10-29-mutable-customisation-p0-p1/progress.md` (P2's was cleaned up; its rulings are in git log and memory).

**Scope split (ruling, approved with this plan):** the spec's P3 also lists skin tone, aged, make-up, freckles and tattoos (moved to P3 by a P1 ruling), and thumbnails. Skin and tattoos are a separate subsystem on the body and head materials: **P3b**, its own plan after this one, with P2's skin-tint calibration. Thumbnails exist only for the CREATE UI: **P4**.

## Global Constraints

- All Mutable and wardrobe content (`/Game/Shinobi/Mutable/**`, copied Mutable Sample content, Fab content) stays local and untracked; scripts regenerate it. Only code, scripts and docs are committed, path-limited (`git commit -- <paths>`): other sessions share the tree, which stays on local `main`. Nothing is pushed.
- Editor jobs run in separate processes (`Scripts/run_python.ps1 -Editor`, `Scripts/character/run_metahuman_gui.ps1`), never in the user's open editor.
- New wardrobe rows are appended after the existing `CitySampleNN` rows, so version-1 looks keep their pieces.
- Reshape inputs share topology: every fit shape is a vertex-moved copy of its sex's base (same LOD0 vertex and triangle order).
- Test only what changed per task; the full suite once at the end (user instruction).
- Research UE 5.8 specifics before implementing anything not settled here (project CLAUDE.md).

## Review Focus

1. **A row with no fit shape for the wearer's sex** (e.g. a female-only source on a male body) is reshaped from the sex's base, as in P1, never from an empty parameter. Test in Task 2.
2. **The Own build (RamilMH, BikaMH) wearing pieces again**: no spikes or slabs. Checked by eye in Task 6's sheet; the catalog test in Task 2 pins that their presets wear pieces.
3. **Slim and heavy builds with coverage on**: no skin holes outside the garment and no skin through it. Checked by eye in Task 6 (every slot on Under/Normal/Over/Own).
4. **Version-1 looks** keep their Top/Bottom/Shoes pieces after new sources are appended. Test in Task 5.
5. **A piece whose material has no tint** takes no colour, silently. Test in Task 4.

---

### Task 1: Fit shapes from the authoring bodies

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableMeshLibrary.h/.cpp` (editor-only functions, like `AddBoneSkinning`)
- Modify: `Scripts/setup_mutable.py` (stage `fits`), `Scripts/mutable/names.py` (`FIT_SOURCES`, `FIT` path)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Produces:
  - `static bool UShinobiMutableMeshLibrary::ReadDnaBody(const FString& DnaPath, TArray<FVector>& Positions, TArray<int32>& Triangles)`: mesh 0, LOD 0 of a body DNA in UE space (cm, Z up, as `IDNAReader` returns it; faces fanned into triangles).
  - `static bool UShinobiMutableMeshLibrary::ReadMeshSurface(USkeletalMesh* Mesh, TArray<FVector>& Positions, TArray<int32>& Triangles)`: LOD 0 reference pose of a skeletal mesh.
  - `static FString UShinobiMutableMeshLibrary::BakeFitShape(USkeletalMesh* Fit, const TArray<FVector>& Positions, const TArray<int32>& Triangles, FVector Offset, float MaxMove = 8.f, int32 Smooth = 10)`: moves every LOD's mesh-description vertices of `Fit` onto the closest point of the surface (shifted by `Offset`), the displacement smoothed `Smooth` times over vertex neighbours and capped at `MaxMove` cm; positions only, so vertex and triangle order stay the base's. Returns "moved N of M vertices, mean D cm, max X cm".
  - `static float UShinobiMutableMeshLibrary::FractionInside(USkeletalMesh* Piece, USkeletalMesh* Body, float Tolerance = 0.5f)`: the share of `Piece`'s LOD 0 vertices more than `Tolerance` cm inside `Body` (closest-point normal test). Used by tests and by `setup_mutable.py` reports.
  - Assets `/Game/Shinobi/Mutable/Bases/Fit/SK_MutableFit_<Source>_<Sex>` for `FIT_SOURCES`:
    - `CitySample`: Male from `m_tal_nrw.dna`, Female from `f_tal_nrw.dna` (`.../Plugins/MetaHuman/MetaHumanCharacter/Content/Optional/Body/FixedCompatibility/`).
    - `MHSample`: both sexes from `f_med_nrw.dna` (the Mutable Sample's MetaHuman clothing is female, legacy body).
    - `Cyborg`: both sexes from the Mutable Sample's `SK_BaseBody` (copied in Task 5; this source is added in Task 5).
  - `Offset` = the sex base's `pelvis` minus the source's pelvis (DNA joint `pelvis` / the mesh's root-most hip bone), so the bodies stand on one spot.

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.FitShapeKeepsTopology`: each existing `SK_MutableFit_*` has its sex base's LOD 0 vertex count and index buffer (equal triangles), and at least 1000 vertices differ from the base by more than 0.5 cm.
  - `ShinobiCoop.Mutable.FitShapeHugsSource`: `FractionInside(<every City Sample Normal-build Top and Bottom of the sex>, SK_MutableFit_CitySample_<Sex>)` < 0.03 for both sexes, and the same pieces against the plain `SK_MutableBase_<Sex>` give a larger fraction (the clipping these fit shapes remove; log both).
- [ ] **Step 2:** Run `Scripts/run_tests.ps1 -Filter ShinobiCoop.Mutable.FitShape` → FAIL (no fit shapes).
- [ ] **Step 3:** Implement the four functions and stage `fits` (duplicate the base with `EditorAssetLibrary.duplicate_asset`, `BakeFitShape`, save; log the report). Run `SETUP_MUTABLE_ONLY=fits Scripts/run_python.ps1 -Editor -Script Scripts/setup_mutable.py`. If `FitShapeHugsSource` fails on the shoulders or crotch, raise `Smooth` or `MaxMove` before anything else, and ledger the values that pass.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable.FitShape` → PASS.
- [ ] **Step 5: Commit** `feat: fit shapes, the Mutable bases moved onto each wardrobe source's body`.

### Task 2: Pieces reshape from their row's fit shape

**Files:**
- Modify: `ShinobiMutableNames.h` + `Scripts/mutable/names.py` (`FitSuffix = " Fit"`; `FitBase` removed), `Scripts/setup_mutable.py` (`graph`: per-slot fit parameter; `wardrobe`: fit columns; `catalog`: MetaHuman presets wear pieces)
- Modify: `ShinobiCreatorCatalog.h` (`FShinobiWardrobeRow`), `ShinobiMutableLook.cpp` (`SetParameters`)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 1's `SK_MutableFit_<Source>_<Sex>`.
- Produces:
  - `FShinobiWardrobeRow` gains `TSoftObjectPtr<USkeletalMesh> MaleFit, FemaleFit` (empty = the sex's base).
  - `CO_Shinobi`: Skeletal Mesh Parameter `"<Slot> Fit"` per slot (default: male base) is the Base Shape of that slot's piece reshape; the shared `"Fit Base"` parameter is gone.
  - `UShinobiMutableLook::SetParameters` sets `"<Slot> Fit"` to the worn row's fit for the look's sex, else `ShinobiMutable::BasePath(Sex)`.
  - City Sample rows carry `SK_MutableFit_CitySample_<Sex>`.
  - `SetPreset` for RamilMH/BikaMH gets `Pieces = 0` again (reverses P2's no-pieces ruling).

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.SlotFitParameter` (latent): a male look wearing City Sample Top 0 builds with instance parameter `"Top Fit"` = `SK_MutableFit_CitySample_Male`, and `"Shoes Fit"` = the male base when that row has no fit for the sex.
  - `ShinobiCoop.Mutable.CatalogPresets`: change its MetaHuman-preset expectation to Top/Bottom/Shoes = 0.
- [ ] **Step 2:** Run `-Filter ShinobiCoop.Mutable` → the two FAIL.
- [ ] **Step 3:** Implement; rerun stages `wardrobe`, `catalog`, `graph`.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS (`OwnHeadUndistorted` and `OwnFaceBones` now run with pieces worn: they must still pass).
- [ ] **Step 5: Commit** `feat: each wardrobe piece reshapes from the body it was made for`.

### Task 3: Coverage volumes

**Files:**
- Modify: `ShinobiMutableMeshLibrary.h/.cpp`, `Scripts/setup_mutable.py` (stage `coverage`; `graph`: clip reshape), `ShinobiMutableLook.cpp` (Hides ON when the row has a clip for the sex)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 2's fit columns.
- Produces:
  - `static USkeletalMesh* UShinobiMutableMeshLibrary::MakeClipVolume(USkeletalMesh* Fit, USkeletalMesh* Piece, const FString& AssetPath, float Reach = 4.f, int32 Erode = 1, float Out = 0.3f, float Depth = 3.f)`. It is built from the fit shape's LOD 0, in these steps:
    1. A triangle is covered when each of its vertices' outward normal ray hits `Piece` within `Reach` cm.
    2. The covered region is eroded by `Erode` vertex rings, so skin stays at hems and the piece hides the cut.
    3. The region is closed into a shell: the covered surface pushed `Out` cm outward, a copy pushed `Depth` cm inward, and side walls along the border.
    4. It is skinned with the fit shape's weights per source vertex and saved at `AssetPath`.
    5. It returns null when nothing is covered.
  - Rows get `MaleClip`/`FemaleClip` = `/Game/Shinobi/Mutable/Wardrobe/Clips/SK_Clip_<Slot>_<Row>_<Sex>`.
  - `CO_Shinobi`: each slot's clip is reshaped from `"<Slot> Fit"` to the built body before `Clip With Mesh`.

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.ClipVolumesClosed`: every row clip is watertight (each LOD 0 edge is shared by exactly 2 triangles) and has the clip's fit shape's skeleton.
  - `ShinobiCoop.Mutable.RowsHaveClips`: every Top/Bottom/Shoes row with a mesh for a sex has a clip for that sex.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement; run stages `coverage`, `wardrobe`, `graph`; take `shot_chakra.ps1 -CreatorMutable` once. If Mutable 5.8 shows spikes when clipping (reported on the forums for reshaped meshes), first try clipping the body *before* the build morph, then ledger. Fallback: Hides OFF for the affected rows (ledger the rows).
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS.
- [ ] **Step 5: Commit** `feat: wardrobe pieces hide the skin they cover (coverage volumes)`.

### Task 4: Palette colours on the pieces

**Files:**
- Modify: `ShinobiCreatorCatalog.h/.cpp` (`FShinobiWardrobeRow`, `Sanitize`), `ShinobiMutableLook.cpp` or `ShinobiBodyComponent.cpp` (where slot colour is applied today), `Scripts/setup_mutable.py` (stage `tints`)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Produces:
  - `FShinobiWardrobeRow` gains `TMap<TSoftObjectPtr<UMaterialInterface>, TSoftObjectPtr<UMaterialInterface>> Tinted` (a piece's material → its tint-enabled copy) and `TArray<FName> ColourParameters` (vector parameters that take the palette colour).
  - Stage `tints`: for each City Sample piece material, a copy under `/Game/Shinobi/Mutable/Wardrobe/CitySample/Materials/` with static switches `A_Color_DoTint` and `A_ColorCrowd_DoTint` on and its tint parameters white. `ColourParameters` lists the copy's tint vectors (`A_Color_Tint`, `A_ColorCrowd_Tint`, the B/C/D ones that exist).
  - The wearer swaps each built section whose material is a key of a worn row's `Tinted` for a MID of the copy, then sets `ColourParameters` to `DT_Palette[SlotColour[slot]]`. Sections without a mapping are left alone.
  - `Sanitize` clamps `SlotColour` to the palette's row count (a P1 deferred minor).

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.SlotColourApplied` (latent): a look with Top 0 and `SlotColour[Top]` = the "Red" palette row gives a top section MID with `A_Color_Tint` = that red.
  - `ShinobiCoop.Mutable.UntintedPieceUnchanged`: a row without `Tinted` keeps its section materials, with no error logged.
  - `ShinobiCoop.Creator.SanitizeClampsColour`: `SlotColour` 200 sanitises to 0 with a palette of 12.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement; run stages `tints`, `wardrobe`.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable+ShinobiCoop.Creator` → PASS.
- [ ] **Step 5: Commit** `feat: wardrobe pieces take the palette colour`.

### Task 5: Mutable Sample pieces (Cyborg and its MetaHuman clothing)

**Files:**
- Modify: `Scripts/setup_mutable.py` (stages `samplecopy`, `samplepieces`; `wardrobe` appends rows), `Scripts/mutable/names.py` (`SAMPLE_PIECES`)
- Reuse: `Scripts/character/skin_outfit.py` (weights from a body onto a garment)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Tasks 1-4's fit, clip and tint machinery.
- Produces:
  - `samplecopy`: the listed folders of `E:\MutableSample\Content` copied into this project's `Content/` at the same paths (`Character/<Piece>`, `Character/Materials`, `Character/Textures`, `Character/Body`, `MetaHumans/Clothing`, `MetaHumans/Body`), so references hold. Local only; a `.gitignore` entry for `Content/Character/` and `Content/MetaHumans/`.
  - `SAMPLE_PIECES` (slot, source, sex, mesh):
    - Cyborg: Jacket_A, Jacket_B, TShirt, TankTop → Top; Pants, ScifiPants → Bottom; Boots, Shoes, Sneakers → Shoes. Both sexes.
    - MHSample (female): the shirts, jeans, cargo pants and boots under `MetaHumans/Clothing`.
  - `samplepieces`:
    1. Cyborg pieces are exported, re-skinned onto `metahuman_base_skel` with `skin_outfit.py` (weights from `SK_MutableFit_Cyborg_<Sex>`, already in the Cyborg body's shape) and imported to `/Game/Shinobi/Mutable/Wardrobe/Cyborg/SK_<Piece>_<Sex>`.
    2. MHSample pieces are used as they are (already on the MetaHuman skeleton).
    3. Rows `Cyborg<Piece>` / `MHSample<Piece>` are appended with their fits, clips and tints.
  - Task 1's `FIT_SOURCES` gains `Cyborg` (from `SK_BaseBody`).
  - **Gate:** after alignment, the Cyborg body's wrists must lie within 3 cm of the base's. If they don't (a T-pose source), keep only the sleeveless pieces (TankTop, Pants, ScifiPants and the shoes) and ledger it.

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.VersionOneRowsKept`: the first N rows of each slot table are still `CitySample01..N` with their P1 meshes (N recorded from today's tables in the test).
  - `ShinobiCoop.Mutable.SamplePiecesOnMetaHumanSkeleton`: every `Cyborg*`/`MHSample*` row mesh uses the base's skeleton, has a fit for each sex it has a mesh for, and `FractionInside(mesh, its fit)` < 0.03.
- [ ] **Step 2:** Run → FAIL (`VersionOneRowsKept` passes now: it's a guard; note that in the ledger).
- [ ] **Step 3:** Implement; run stages `samplecopy`, `fits`, `samplepieces`, `coverage`, `tints`, `wardrobe`, `graph`.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS.
- [ ] **Step 5: Commit** `feat: Mutable Sample Cyborg and MetaHuman clothing join the wardrobe`.

### Task 6: Contact sheets and the user's review

**Files:**
- Modify: `ShinobiCreatorSpikeShot.cpp` (`RunCreatorMutableShot`: every row of each slot on Male/Female × Under/Normal/Over, plus Own for both sexes), `Scripts/shot_chakra.ps1` if new switches are needed

- [ ] **Step 1:** Extend the shot, then run `shot_chakra.ps1 -CreatorMutable` and `-RosterPresets`, both on `Lvl_TrainingGrounds?game=/Script/ShinobiCoop.ShinobiGameMode`. Build one sheet per slot (rows: pieces; columns: builds), plus the roster sheet.
- [ ] **Step 2: Commit** `test: wardrobe contact sheets`.
- [ ] **Step 3: Stop.** Send the sheets: what fits, what still clips, and the rows proposed for removal. Wait for the user's verdict; rows they reject are dropped from the tables (appended rows only).

### Task 7: MetaHuman Fab wardrobe (only if the user has downloaded items)

**Files:**
- Modify: `Scripts/mutable/base_bodies.py` (stage `outfits`), `Scripts/setup_mutable.py` (`wardrobe` appends `Fab<Item>` rows)

- [ ] **Step 1:** Ask the user which downloaded Fab items to add. If none, ledger it and skip to Task 8.
- [ ] **Step 2: Write the failing test.** Extend `SamplePiecesOnMetaHumanSkeleton` to `Fab*` rows.
- [ ] **Step 3: Implement.**
  - Outfit clothing (Chaos outfit assets that resize to a body): a GUI job puts each item on a copy of `MB_<Sex>_Normal` and builds it, keeping the fitted clothing mesh. Its fit is the sex's base (empty column).
  - Skeletal clothing made for a body type: its fit comes from that type's DNA, through Task 1.
  - Then run `coverage`, `tints` and `wardrobe`.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS; add the rows to Task 6's sheets for the user.
- [ ] **Step 5: Commit** `feat: MetaHuman Fab clothing joins the wardrobe`.

### Task 8: Phase close

- [ ] **Step 1:** Run the full suite `Scripts/run_tests.ps1` once → PASS, except failures that predate P3 (named).
- [ ] **Step 2:** Final whole-branch review (fresh reviewer, most capable model), then one fix pass.
- [ ] **Step 3:** Packaged smoke: `Scripts/package.ps1`, then `shot_chakra.ps1 -CreatorMutable -Packaged`.
- [ ] **Step 4:** Report to the user: suite, rulings, deferred minors. Next is P3b (skin and tattoos).
