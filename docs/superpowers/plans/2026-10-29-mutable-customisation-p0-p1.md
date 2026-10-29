# Mutable Customisation P0–P1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Answer the spike questions, then make a made character's body, build, skin and three wardrobe slots come from one Mutable object at runtime.

**Architecture:** An editor-only C++ library builds the `CO_Shinobi` Mutable graph from Python by node class name and pin name. A setup script makes the male and female base bodies (City Sample normal frames with build morphs), the wardrobe Data Tables and the graph. At runtime `UShinobiBodyComponent::WearAppearance` maps a version-2 `FShinobiAppearance` to Mutable parameters, builds asynchronously and swaps the generated mesh in.

**Tech Stack:** UE 5.8 C++, Mutable plugin 1.8.0 (`CustomizableObject`, `CustomizableObjectEditor`), Unreal Python (headless `Scripts/run_python.ps1`), UE automation tests.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md`. P2–P5 get their own plans after this plan's phase review.

## Global Constraints

- Engine: UE 5.8 (`C:\Program Files\Epic Games\UE_5.8`); Mutable 1.8.0; project `E:\ShinobiCoop\ShinobiCoop.uproject`.
- Asset root for all new content: `/Game/Shinobi/Mutable/`.
- Setup scripts run headless through `Scripts/run_python.ps1`, never inside the user's open editor. Close the editor before builds that relink the module.
- Never switch or merge branches with the editor open. The tree holds other sessions' uncommitted work: commit only your own paths with `git commit -- <paths>`.
- Content from `/Game/CitySampleCrowd` (local-only, not in git) may be read by setup scripts but committed assets must not reference it: duplicate what is used under `/Game/Shinobi/Mutable/`.
- Tests: `Scripts/run_tests.ps1 -Filter <prefix>`; per task run only the task's tests. Full suite only in Task 9.
- Mutable parameter names are the strings in `ShinobiMutableNames.h` (Task 4); Python reads the same strings from `Scripts/mutable/names.py`. The two lists must match exactly.

## Review Focus

1. **A version-1 code or save slot from today's players** must still load and wear the same sex, build, face, hair and clothes. Test in Task 4.
2. **A client sends out-of-range rows, palette indices or 4+ tattoos.** The server clamps them; nothing crashes. Test in Task 4.
3. **The look changes again while a build is pending.** The last request wins, and an earlier build finishing late must not overwrite it. Test in Task 7.
4. **`CO_Shinobi` is missing or a build fails.** The previous mesh stays and an error is logged once. Test in Task 7.
5. **Switching from a Mutable look back to a legacy roster body** (`Wear()`) drops the Mutable mesh and restores the roster retargeter. Test in Task 7.

---

## P0 Spike

### Task 1: Mutable graph helper (editor library)

Kept as production code: P1's graph script depends on it. Answers spike question 1.

**Files:**
- Modify: `Source/ShinobiCoop/ShinobiCoop.Build.cs` (runtime `CustomizableObject`; editor-only `CustomizableObjectEditor`)
- Modify: `ShinobiCoop.uproject` (no change expected: `Mutable` is already enabled through `MutableClothing`; verify `Mutable` itself is listed, add `{"Name": "Mutable", "Enabled": true}` if not)
- Create: `Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableGraphLibrary.h/.cpp`
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:**
- Produces (all `UFUNCTION(BlueprintCallable)` static, `WITH_EDITOR` bodies, return empty/false outside the editor):
  - `UEdGraph* GetGraph(UCustomizableObject* Object)`: the object's source graph (through `Object->GetPrivate()->GetSource()`; if that isn't reachable from outside the plugin, find the `UEdGraph` subobject whose outer is the object).
  - `UEdGraphNode* AddNode(UCustomizableObject* Object, const FString& NodeClass, int32 X, int32 Y)`: `NodeClass` is the short class name as reported by Python (`"CustomizableObjectNodeSkeletalMesh"`, `"CONodeComponentSkeletalMesh"`, …); found with `FindFirstObject<UClass>`; `NewObject` in the graph, `CreateNewGuid`, `PostPlacedNewNode`, `AllocateDefaultPins`, added to the graph.
  - `bool SetNodeProperty(UEdGraphNode* Node, const FString& Property, const FString& Value)`: `FProperty::ImportText_Direct`; then `PostEditChangeProperty` and `ReconstructNode` so pins follow (for example a Skeletal Mesh node growing section pins).
  - `TArray<FString> ListPins(UEdGraphNode* Node)`: `"<In|Out>|<PinName>|<PinCategory>"` per pin.
  - `bool LinkPins(UEdGraphNode* From, const FString& FromPin, UEdGraphNode* To, const FString& ToPin)`: through the graph schema's `TryCreateConnection` so Mutable's own pin rules apply.
  - `UEdGraphNode* FindNode(UCustomizableObject* Object, const FString& NodeClass, int32 Index)`.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.GraphHelper` in `ShinobiMutableTests.cpp` (`WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`, flags `ShinobiTestFlags`): create a transient `UCustomizableObject` with `UCustomizableObjectEditorFunctionLibrary::NewCustomizableObject` into `/Temp`; find the default `CustomizableObjectNodeObject`; `AddNode` a `CONodeComponentSkeletalMesh` and a `CustomizableObjectNodeSkeletalMesh`; `SetNodeProperty(Mesh, "SkeletalMesh", <CrowdMNormal body path from DA_CreatorCatalog>)`; link object → component → mesh using names read from `ListPins`; assert `CompileCustomizableObjectSynchronously(...) == ECustomizableObjectCompilationState::Completed` and that a created instance's `UpdateSkeletalMeshAsyncResult` reports `EUpdateResult::Success` (latent command polling up to 30 s).
- [ ] **Step 2: Run it to verify it fails** — `Scripts/run_tests.ps1 -Filter ShinobiCoop.Mutable.GraphHelper` → fails to compile (library missing).
- [ ] **Step 3: Implement the library** with the interfaces above. Record the real pin names the test used in a comment at the top of `ShinobiMutableGraphLibrary.h` (they are needed by Task 6).
- [ ] **Step 4: Run the test** → PASS. If `GetGraph` or `TryCreateConnection` cannot work from outside the plugin, stop: P1 switches to the hand-wiring fallback (spec, "Editor helper"). Write the user step list for the graph in Task 6 instead of the script stage.
- [ ] **Step 5: Commit** `Build.cs`, the library and the test: `feat: editor helper that builds Mutable graphs from Python`.

### Task 2: Shared vertex layout check (spike question 2)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiFaceBlendLibrary.h/.cpp`: add `FName AlignBone = TEXT("FACIAL_C_FacialRoot")` and `FString Prefix = TEXT("Blend_")` parameters to `BakeFaceBlendMorphs` (defaults keep today's behaviour; `setup_creator.py` callers unchanged).
- Create: `Scripts/mutable/check_layout.py`
- Create: `Scripts/mutable/names.py` (paths and names shared by every Mutable script; Task 4 adds parameter names)

**Interfaces:**
- Produces: `names.py` constants `MUTABLE_ROOT = "/Game/Shinobi/Mutable"`, `CATALOG = "/Game/Shinobi/Characters/Creator/DA_CreatorCatalog"`, `OWN_BODIES = {"Male": RamilMH body path, "Female": BikaMH body path}` (paths from `AShinobiPlayerCharacter::Roster`).

- [ ] **Step 1:** Write `check_layout.py`: duplicate each sex's Normal frame body to `/Temp/LayoutCheck/`, call `BakeFaceBlendMorphs(copy, [same-sex Under, Over, own body, other-sex Normal], names, MaxLods=1, AlignBone="root", Prefix="Check_")` and log the returned report. Delete `/Temp/LayoutCheck` afterwards.
- [ ] **Step 2:** Run `Scripts/run_python.ps1 -Script Scripts/mutable/check_layout.py`. Expected record: for each pair, matched or "vertex count differs". The report decides P1: same-sex frames and own bodies must match for build morphs (Task 5). A male/female mismatch means reshape stays within each sex (spec risk 1).
- [ ] **Step 3: Commit** the library change and both scripts: `feat: body layout check for Mutable build morphs`.

### Task 3: Table node, reshape and hiding on a MetaHuman body (spike question 3)

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableTypes.h` (`FShinobiWardrobeRow : FTableRowBase`)
- Create: `Scripts/mutable/spike_table.py` (throwaway; deleted at the end of the task)

**Interfaces:**
- Produces: `USTRUCT() FShinobiWardrobeRow` with `UPROPERTY(EditAnywhere)` columns `TSoftObjectPtr<USkeletalMesh> MaleMesh, FemaleMesh; TSoftObjectPtr<UTexture2D> Thumbnail; bool bHasWear = false;` plus the hiding column this task settles on (one of: `TSoftObjectPtr<USkeletalMesh> MaleClip, FemaleClip` for Clip With Mesh, or a per-row block layout).

- [ ] **Step 1:** `spike_table.py` builds `/Temp/Spike/CO_SpikeTable` with the Task 1 helper: male Normal base body; a Skeletal Mesh Morph on it driven by a float parameter `Blend` (morph from a Task 2 style bake of the Over frame); a Table node over `/Temp/Spike/DT_SpikeTop` (`FShinobiWardrobeRow`, two City Sample tops); its mesh through a Mesh Reshape (base shape = unmorphed body, target = morphed body); merged into the body component.
- [ ] **Step 2:** Try per-row hiding in this order and keep the first that compiles and removes skin under the top: Remove Mesh Blocks with a table-driven layout; Clip With Mesh with a table mesh column. Record which one in a comment on `FShinobiWardrobeRow`.
- [ ] **Step 3:** Compile and bake two instances (row 0 at `Blend` 0, row 1 at `Blend` 1) with `Scripts/run_python.ps1`; render each with the existing portrait shot (`-ShinobiPortraitBP` style) or a viewport capture. Expected: top follows the heavier chest with no skin poking through.
- [ ] **Step 4:** Delete `spike_table.py` and `/Temp/Spike`. Commit `ShinobiMutableTypes.h`: `feat: wardrobe row type for Mutable tables`.

### Task 3b: Spike report and phase review

- [ ] **Step 1:** Amend the spec's "Risks and fallbacks" and `Hides` column text with the answers from Tasks 1–3. Spike question 4 (static head beside a Mutable body) is answered by Task 8's shot.
- [ ] **Step 2:** Ask the user to review the spike answers before P1. Commit the spec amendment.

---

## P1 Foundation

### Task 4: Appearance version 2 and Mutable names

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiCreatorCatalog.h/.cpp` (`FShinobiAppearance`, `EShinobiBuild`, `EShinobiWardrobeSlot`)
- Create: `Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableNames.h`
- Modify: `Scripts/mutable/names.py`
- Test: `Source/ShinobiCoop/Tests/ShinobiCreatorTests.cpp` (extend `AppearanceCode`), `ShinobiMutableTests.cpp`

**Interfaces:**
- Produces:
  - `EShinobiBuild` gains `Own` (after `Over`). The field keeps its name `Build` (it is the spec's Build Type).
  - `EShinobiWardrobeSlot` gains `Outer, Accessory, Cloak` (after `Shoes`); `constexpr int32 ShinobiWardrobeSlotCount = 6`.
  - New `FShinobiAppearance` fields: `uint8 Head = None; uint8 BuildBlend = 255; uint8 Outer = None, Accessory = None, Cloak = None; uint8 SlotColour[6] = {}; uint8 SlotWear[6] = {}; uint8 SkinTone = 128; uint8 Aged = 0; uint8 MakeUp = 0; uint8 Freckles = 0; TArray<FShinobiTattoo> Tattoos;`
  - `USTRUCT() FShinobiTattoo { uint8 Design = 0; uint8 Region = 0; int8 OffsetU = 0; int8 OffsetV = 0; uint8 Rotation = 0; uint8 Scale = 128; }` with `operator==`.
  - `uint8& FShinobiAppearance::Slot(EShinobiWardrobeSlot S)` (and const): the row field for a slot.
  - `ShinobiMutableNames.h`, `namespace ShinobiMutable`: `Sex = TEXT("Sex")`, `Build = TEXT("Build")`, `BuildBlend = TEXT("Build Blend")`, `SkinTone = TEXT("Skin Tone")`, `Aged = TEXT("Aged")`, `MakeUp = TEXT("Make Up")`, `Freckles = TEXT("Freckles")`, `SlotNames[6] = {Top, Bottom, Shoes, Outer, Accessory, Cloak}`, `ColourSuffix = TEXT(" Colour")`, `WearSuffix = TEXT(" Wear")`, `ObjectPath = TEXT("/Game/Shinobi/Mutable/CO_Shinobi.CO_Shinobi")`, `PalettePath = TEXT("/Game/Shinobi/Mutable/DT_Palette.DT_Palette")`, `MaxTattoos = 3`. `names.py` mirrors them as `PARAM_*` constants.
- Code layout, version 2: `[2, Sex, Build, BuildBlend, Head, Face, Hair, Beard, Mustache, Top, Bottom, Shoes, Outer, Accessory, Cloak, SlotColour×6, SlotWear×6, SkinTone, Aged, MakeUp, Freckles, BlendFace×2, BlendWeight×2, nFace, FaceShape…, nBody, BodyShape…, nTattoo, Tattoo×6 bytes each]`.

- [ ] **Step 1: Write the failing tests.** In `AppearanceCode`: capture today's code for a fixed look before editing (run the current `ToCode` once and paste the literal into the test) and assert `FromCode(<that v1 literal>)` gives the same `Sex/Build/Face/Hair/Beard/Mustache/Top/Bottom/Shoes/FaceShape/BodyShape/BlendFace/BlendWeight`, with v2 defaults for new fields (`Head == None`, `BuildBlend == 255`, `SkinTone == 128`, `Outer == None`). New `ShinobiCoop.Mutable.AppearanceV2`: a look with every new field non-default (2 tattoos, `Build = Own`) round-trips through `ToCode/FromCode`; `Sanitize` on a look with 5 tattoos keeps 3, clamps `Build` > `Own` to `Normal`, `SlotWear` untouched, unknown `Region` kept (wearing skips it).
- [ ] **Step 2:** Run `Scripts/run_tests.ps1 -Filter ShinobiCoop.Mutable.AppearanceV2` and `-Filter ShinobiCoop.Creator.AppearanceCode` → FAIL (fields missing).
- [ ] **Step 3:** Implement the fields, `Slot()`, the v2 layout in `ToCode`, `FromCode` accepting versions 1 and 2 (v1 reads its old layout, the rest stays default), and the `Sanitize` rules. Add the names header and mirror in `names.py`.
- [ ] **Step 4:** Run both filters → PASS. Run `-Filter ShinobiCoop.Creator` (save slots and codes use the struct) → PASS.
- [ ] **Step 5: Commit** `feat: appearance version 2 for Mutable looks`.

### Task 5: Base bodies with build morphs

**Files:**
- Create: `Scripts/setup_mutable.py` (stages via `SETUP_MUTABLE_ONLY`: `bases`, `palette`, `wardrobe`, `graph`; no value = all, in that order)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Produces: `/Game/Shinobi/Mutable/Bases/SK_MutableBase_Male` and `_Female` (duplicates of the Normal frame bodies) with morphs `Build_Under`, `Build_Over`, `Build_Own` (own = RamilMH body on male, BikaMH body on female; omitted where Task 2 reported a mismatch); `names.py` `BASES = {"Male": ..., "Female": ...}` and `MORPHS = {EShinobiBuild name: morph name}`.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.Bases`: both base assets load, their skeleton is `metahuman_base_skel`, and each has morphs `Build_Under` and `Build_Over` (and `Build_Own` unless Task 2 recorded a mismatch).
- [ ] **Step 2:** Run → FAIL (assets missing).
- [ ] **Step 3:** Stage `bases`: duplicate and bake with `BakeFaceBlendMorphs(base, [Under, Over, Own], [...], MaxLods=4, AlignBone="root", Prefix="Build_")` (Prefix makes names `Build_Under` from names `Under`). Save.
- [ ] **Step 4:** `SETUP_MUTABLE_ONLY=bases` run, then the test → PASS.
- [ ] **Step 5: Commit** script, test and the two base assets: `feat: Mutable base bodies with build morphs`.

### Task 6: Palette, wardrobe tables and `CO_Shinobi`

**Files:**
- Modify: `Scripts/setup_mutable.py` (stages `palette`, `wardrobe`, `graph`)
- Modify: `ShinobiMutableTypes.h` (`FShinobiPaletteRow : FTableRowBase { FText Name; FLinearColor Colour; }`)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 1 helper, Task 3 `FShinobiWardrobeRow`, Task 4 names, Task 5 bases.
- Produces: `/Game/Shinobi/Mutable/DT_Palette` (12 rows: black, white, grey, navy, red, crimson, orange, ochre, olive, forest, teal, purple); `DT_Wardrobe_<Slot>` for all six slots (Top, Bottom, Shoes get one City Sample piece each, duplicated into `/Game/Shinobi/Mutable/Wardrobe/CitySample/`; the rest empty); `/Game/Shinobi/Mutable/CO_Shinobi` with parameters named exactly as in `ShinobiMutableNames.h`. Table option names are row names.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Mutable.ObjectCompiles`: `CO_Shinobi` loads; synchronous compile completes; its parameter names include `Sex, Build, Build Blend, Skin Tone, Aged, Top, Bottom, Shoes, Top Colour, Top Wear`; each wardrobe table's every row has `MaleMesh` and `FemaleMesh` set; `DT_Palette` has 12 rows.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Stages `palette` and `wardrobe` (data). Stage `graph` with the helper (or, after a Task 1 stop, the written hand-wiring steps, carried out by the user in the editor): Sex switch over the two bases; build morphs (Build switch picks the morph, `Build Blend` / 255 the factor); `Skin Tone` and `Aged` into the body material; per slot a Table node, a Sex switch over its two mesh columns, Mesh Reshape to the built body, the Task 3 hiding, colour into the garment material's `Colour` parameter and wear into `Wear` when present.
- [ ] **Step 4:** Run all stages, then the test → PASS.
- [ ] **Step 5: Commit** script, test and the new `/Game/Shinobi/Mutable` assets: `feat: CO_Shinobi with body, skin and three wardrobe slots`.

### Task 7: Runtime Mutable path in the body component

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Hero/ShinobiMutableLook.h/.cpp` (`UShinobiMutableLook`, owned by the body component)
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiBodyComponent.h/.cpp` (`WearAppearance`, `Wear`)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Consumes: Task 4 names and fields, Task 6 assets.
- Produces:
  - `UShinobiMutableLook::Request(const FShinobiAppearance& Look)`: sets parameters (sex, build, blend, skin, rows by option name from the slot's table, palette colours, wear), starts `UpdateSkeletalMeshAsyncResult`. A request while one is pending replaces the pending parameters; only the newest request's result is applied (sequence number).
  - `DECLARE_DELEGATE_OneParam(FOnShinobiMutableBuilt, USkeletalMesh*)` `UShinobiMutableLook::OnBuilt`, null on failure.
  - `UShinobiBodyComponent::WearAppearance` routes made characters through it: keeps the current mesh until `OnBuilt`, then sets the generated mesh, the `RTG_MannyToCrowd` retargeter, the face (MetaHuman) or `WearStaticHead` (when `Head != None`, Task P2 supplies heads; until then `Head` is ignored), grooms, face and body sliders. `Wear()` (roster) cancels a pending request and drops the Mutable mesh.
  - `static FString UShinobiMutableLook::ObjectPathOverride` (tests only; empty = `ShinobiMutable::ObjectPath`).
  - Missing `CO_Shinobi` or a failed build: `UE_LOG(LogShinobiCoop, Error, ...)` once per component, previous mesh kept.

- [ ] **Step 1: Write the failing tests** (latent, test world from `ShinobiTestWorld.h`):
  - `ShinobiCoop.Mutable.WearBuilds`: a pawn wears a female `Over` look with Top row 0; after the build the body mesh's skeleton is `metahuman_base_skel` and has the bones `pelvis`, `spine_05`, `head`, `hand_r`, `foot_l`.
  - `ShinobiCoop.Mutable.LastRequestWins`: two `WearAppearance` calls one frame apart (male, then female); the final mesh is the female build.
  - `ShinobiCoop.Mutable.FailureKeepsMesh`: with the object path overridden to a missing asset (test-only `UShinobiMutableLook::ObjectPathOverride`), the previous mesh stays and one error is logged (`AddExpectedError`).
  - `ShinobiCoop.Mutable.RosterAfterMutable`: wear a Mutable look, then `Wear()` the Hero roster entry; the mesh is `SK_Hero` and no Mutable build applies afterwards.
- [ ] **Step 2:** Run `-Filter ShinobiCoop.Mutable` → the four new tests FAIL.
- [ ] **Step 3:** Implement `UShinobiMutableLook` and the body component changes.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable` → PASS; `-Filter ShinobiCoop.Creator` → PASS.
- [ ] **Step 5: Commit** `feat: made characters wear CO_Shinobi at runtime`.

### Task 8: Shots and spike question 4

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCreatorSpikeShot.cpp` (new phase `-CreatorMutable`)
- Modify: `Scripts/shot_chakra.ps1` (`-CreatorMutable` switch passing the flag)

- [ ] **Step 1:** Add the shot phase: for each sex × build (`Under`, `Normal`, `Over`, `Own`) wear a look with the three slot pieces, frame full body, capture; then one male `Normal` look beside `SK_Head_Ramil` via `WearStaticHead` (the existing spike head) at head close-up for the skin and neck check.
- [ ] **Step 2:** Run `Scripts/shot_chakra.ps1 -CreatorMutable`. Expected: 8 body shots, garments follow every build, no skin through clothes; the head shot shows the neck join and skin-tone difference to be fixed by P2's tint.
- [ ] **Step 3: Commit** `test: Mutable creator shots`.

### Task 9: Phase review

- [ ] **Step 1:** Run the full suite once: `Scripts/run_tests.ps1` → PASS (report any pre-existing failures separately, unchanged).
- [ ] **Step 2:** Send the user the Task 8 contact sheet and the spike answers; wait for review before writing the P2 plan.
