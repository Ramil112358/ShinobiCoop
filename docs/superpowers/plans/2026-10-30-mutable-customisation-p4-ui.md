# Mutable customisation P4: CREATE UI, preview camera, studio set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The CREATE tab edits every Mutable look field (presets, head, wardrobe with thumbnails and colours, build, skin tone, make-up, freckles, tattoos) on a rotatable, zoomable preview in a studio-lit set.

**Architecture:** `UShinobiCreatorPage` (code-built UMG, `ShinobiMenuStyle`) is rebuilt around four categories that all change the look through one `EditLook` path (instant for skin, debounced Mutable rebuild for meshes). `AShinobiMenuStage` gains yaw and continuous zoom, fed by the main menu's mouse and gamepad input, and moves into a new code-built `AShinobiStudioSet` while CREATE is open. Thumbnails are rendered in that studio by a menu shot and imported by `Scripts/setup_mutable.py`.

**Tech Stack:** UE 5.8 C++ (UMG, Slate input), Mutable (`CO_Shinobi`, existing), Unreal Python (`setup_mutable.py`, `setup_menu.py`), PowerShell shot scripts.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md` (sections "CREATE tab UI and preview", "Preview camera", "Studio set"; P4 in "Phases"). Earlier phases: P3b plan `docs/superpowers/plans/2026-10-30-mutable-customisation-p3b-skin.md`.

## Global Constraints

- Unreal Engine 5.8; UI stays code-built with `ShinobiMenuStyle` (accent `#ff7a1a`, `StyleButton`, `TickFocus`), no UMG assets.
- Works with mouse and gamepad (every new control reachable by focus navigation).
- Packaged build and co-op keep working: only `FShinobiAppearance` replicates; nothing new replicates.
- Generated content (thumbnails, studio materials, Mutable data) is local and untracked; only code, scripts and docs are committed.
- Shared tree on local `main`: path-limited commits of own files only; never push without the user's go-ahead.
- Never run Creator or setup jobs in the user's open editor; scripts run headless (`Scripts/run_python.ps1 -Editor`).
- Tests: only what changed per task; the full suite once in the phase close.
- User decisions in force: Aged dropped; Hero, Bika, Cat, Misha, Pabu keep legacy bodies in play but their presets stay available in CREATE.

## Review Focus

1. Gamepad only: every CREATE control (preset strip, thumbnails, swatches, tattoo rows) is reachable, and the right stick turns/zooms the preview without moving focus. Test: Task 4 `CreatorEveryRowFocusable`.
2. Switching sex keeps the look valid: a head, wardrobe row or tattoo the other sex lacks is cleared, not left pointing at a missing mesh. Test: Task 4 `CreatorSexSwitchKeepsValidLook`.
3. Fast picks while a Mutable build is pending: the last pick is what ends up worn. Test: Task 4 `CreatorLastPickWins`.
4. Leaving CREATE mid-fade or mid-build returns the stage to the rooftop with the played character, and the studio's lights never reach the rooftop. Tests: Task 2 `MenuStageLeavesStudioMidFade`, `StudioLightsStayInStudio`.
5. Small windows (1280×720) and ultrawide: the panel never covers the character and the hints stay on screen. Check: Task 8 shots at 1280×720 and 2560×1080.

---

### Task 1: Preview camera (rotate, zoom, reset, hints)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiMenuStage.h/.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiMainMenuWidget.h/.cpp`, `ShinobiCreatorPage.cpp` (callers of `SetCloseUp`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMenuTests.cpp`

**Interfaces:**
- Produces on `AShinobiMenuStage`: `void AddYaw(float Degrees)`, `void AddZoom(float Delta)`, `void SetAutoZoom(float Zoom)`, `void ResetView()`, `float GetZoom() const`, `float GetYaw() const`. `SetCloseUp` is removed (callers use `SetAutoZoom(bHead ? 1.f : 0.f)`).
- Produces on `UShinobiMainMenuWidget`: `void UseGamepadHints(bool bGamepad)`, `FText GetHintText() const` (the two hint labels joined, for tests).

- [ ] **Step 1: Write the failing tests** in `ShinobiMenuTests.cpp`, each spawning the stage in an `FShinobiTestWorld` and ticking ~2 s:
  - `ShinobiCoop.UI.MenuStageZoomFramesHead`: `SetAutoZoom(1)` → camera FOV within 0.5 of 22 and camera Z within 10 cm of the head bone; `SetAutoZoom(0)` → FOV within 0.5 of 45.
  - `ShinobiCoop.UI.MenuStageYawTurnsCharacter`: `AddYaw(90)` → `GetYaw()` within 1 of 90, the driver's relative yaw within 1 of 0 (−90 + 90), the camera transform unchanged.
  - `ShinobiCoop.UI.MenuStageZoomClamps`: `AddZoom(5)` → `GetZoom() == 1`; `AddZoom(-5)` → 0.
  - `ShinobiCoop.UI.MenuStageResetView`: `SetAutoZoom(1); AddYaw(120); AddZoom(-0.5); ResetView()` → yaw 0, zoom 1 (the last auto zoom).
  - `ShinobiCoop.UI.MainMenuHintsFollowDevice`: `UseGamepadHints(false)` → hint text contains "Wheel" and "Drag"; `true` → contains "RS".
- [ ] **Step 2: Run them** — `Scripts/run_tests.ps1 -Filter "ShinobiCoop.UI.MenuStage+ShinobiCoop.UI.MainMenuHints"`. Expected: compile errors for the missing members, then FAIL.
- [ ] **Step 3: Implement.** Stage keeps `TargetYaw`/`Yaw` and `TargetZoom`/`Zoom` (0..1), eased in `Tick` with `FInterpTo` (speed 8 for yaw, 6 for zoom); driver relative yaw = −90 + Yaw. Camera = lerp by Zoom between today's full view (location (560, Y, 115), pitch −3, FOV 45) and today's close-up (location (330, Y, HeadHeight + 2), pitch 0, FOV 22), so the aim slides from chest to face. `ResetView` sets yaw 0 and zoom back to the last `SetAutoZoom` value. `FramePortrait` sets yaw and zoom to 0 at once. Main menu, on the SHINOBI and CREATE tabs only:
  - The root canvas becomes `Visible` so clicks on empty screen reach it.
  - Left or right drag → `AddYaw(DeltaX * 0.4)`, wheel → `AddZoom(Wheel * 0.1)`, double-click or `Gamepad_RightThumbstick` → `ResetView`.
  - `NativeOnAnalogValueChanged` stores right-stick X/Y (dead zone 0.2), applied in `NativeTick` at 180°/s and 1.0 zoom/s.
  - Any gamepad key or axis → `UseGamepadHints(true)`; mouse move or click → `false`.
  - Hints sit bottom right as two `T_Hud_KeyBadge`-style text badges: mouse "Wheel  Zoom in/out", "Drag  Rotate Camera"; gamepad "RS ↕  Zoom in/out", "RS ↔  Rotate Camera". They are collapsed on the other tabs.
- [ ] **Step 4: Run the Step 2 command plus `ShinobiCoop.UI`.** Expected: all pass.
- [ ] **Step 5: Commit** — `git commit -m "feat: menu stage preview camera: rotate, zoom, reset and input hints" -- <files>`.

### Task 2: Studio set and light presets

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiStudioSet.h/.cpp`
- Modify: `ShinobiMenuStage.h/.cpp`, `ShinobiMainMenuWidget.h/.cpp`, `Scripts/setup_menu.py` (new stage `studio`: materials + placement), `Config/DefaultGame.ini` only if the materials' folder isn't under `/Game/Shinobi` (it is: `/Game/Shinobi/UI/Studio`)
- Test: `ShinobiMenuTests.cpp`

**Interfaces:**
- Produces: `UENUM() enum class EShinobiStudioLight : uint8 { Studio, Day, Dusk };` and `AShinobiStudioSet` with components `Backdrop` (engine sphere, radius 10 m, `M_StudioBackdrop`), `Floor` (`M_StudioFloor`), `Ring` (`M_StudioRing`), `KeyLight`, `FillLight`, `RimLight` (`URectLightComponent`, attenuation radius 1500 cm), `PostProcess` (bounded by a `UBoxComponent` 25 m). Methods: `void ApplyPreset(EShinobiStudioLight Preset)`, `EShinobiStudioLight GetPreset() const`. Material paths are constants in `ShinobiStudioSet.cpp`: `/Game/Shinobi/UI/Studio/M_StudioBackdrop`, `M_StudioFloor`, `M_StudioRing`. A missing material logs once and keeps the engine default.
- Produces on `AShinobiMenuStage`: `void SetStudio(bool bInStudio)` (teleports to the first `AShinobiStudioSet`'s transform or back to its BeginPlay transform; no studio → no-op), `bool IsInStudio() const`.
- Produces on `UShinobiMainMenuWidget`: entering CREATE fades a full-screen black `UBorder` in over 0.2 s, calls `SetStudio(true)`, then fades out over 0.3 s; leaving reverses. A light preset chip row (STUDIO / DAY / DUSK) shows at the stage's top right on CREATE only.

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.UI.StudioSetLightRig`: spawned set → fill intensity = 0.35 × key (±0.01); key is above and front-left of the set's origin as seen from the camera side (+X); rim is behind (−X) with blue > red in its colour.
  - `ShinobiCoop.UI.StudioPresetsRetune`: `ApplyPreset(Day)` and `ApplyPreset(Dusk)` each change the key light's colour and the backdrop MID's "Tint" vector from the Studio values; `ApplyPreset(Studio)` restores them.
  - `ShinobiCoop.UI.MenuStageMovesToStudio`: a stage and a set 200 m apart → `SetStudio(true)` puts the stage at the set's location, `SetStudio(false)` back at its start.
  - `ShinobiCoop.UI.MenuStageLeavesStudioMidFade` (Review Focus 4): the main menu selects CREATE, then SHINOBI after 0.1 s (mid fade-in) → after 1 s of ticks the stage is at the rooftop, not in the studio, and the black overlay is fully transparent.
  - `ShinobiCoop.UI.StudioLightsStayInStudio` (Review Focus 4): with the set at its placed offset (Z + 20000 cm from the stage, as `setup_menu.py` places it), every studio light's attenuation radius is shorter than its distance to the stage's rooftop position.
- [ ] **Step 2: Run** `Scripts/run_tests.ps1 -Filter "ShinobiCoop.UI.Studio+ShinobiCoop.UI.MenuStageMovesToStudio+ShinobiCoop.UI.MenuStageLeavesStudio"`. Expected: FAIL (missing class).
- [ ] **Step 3: Implement** the actor, the presets and the stage/menu calls. Presets live in a static table in the .cpp: Studio = neutral white key, fill 35 %, rim (0.8, 0.9, 1.0), backdrop tint charcoal; Day = warmer key (1.0, 0.95, 0.85), brighter backdrop; Dusk = orange key (1.0, 0.6, 0.35), purple-blue rim, darker backdrop. The post process uses manual exposure with a fixed bias, default tone mapping and no grading. The sphere backdrop encloses the set, so the rooftop's sun doesn't light it (no lighting channels; see Rulings). In `setup_menu.py` stage `studio`:
  - Build three materials with `MaterialEditingLibrary`.
  - Backdrop: unlit, two-sided, a screen-centred radial gradient from charcoal (0.02, 0.022, 0.026) at the edges to slate (0.09, 0.1, 0.11) behind the character, times a vector parameter "Tint".
  - Floor: lit, dark, roughness 0.9.
  - Ring: unlit, translucent, a thin radial band in the accent orange at 0.25 opacity.
  - Place one `ShinobiStudioSet` at the stage's location + (0, 0, 20000) and save `Lvl_MainMenu`.
- [ ] **Step 4: Rerun tests; build the content** with `SETUP_MENU_ONLY=studio Scripts/run_python.ps1 -Script Scripts/setup_menu.py -Editor`; take `Scripts/menu_shots.ps1` shots of CREATE in each preset (Task 8 adds the flag; here a manual `-ShinobiMenuTour` run with CREATE is enough). Expected: tests pass; the character is lit by the studio, with no sky or rooftop visible.
- [ ] **Step 5: Commit** the code and script.

### Task 3: Instant skin edits without a Mutable rebuild

**Files:** Modify `Source/ShinobiCoop/Shinobi/Hero/ShinobiBodyComponent.h/.cpp`; Test `ShinobiMutableTests.cpp`.

**Interfaces:** Produces `void UShinobiBodyComponent::UpdateSkin(const FShinobiAppearance& Look)`. It calls `ApplySkinTone(Look, LastHeadTint)` and `ApplySkinLayers(Look)`, with `LastHeadTint` remembered by `DressBuilt` (the static head's `SkinTint`, else white).

- [ ] **Step 1: Failing tests:**
  - `ShinobiCoop.Mutable.UpdateSkinWithoutRebuild`: wear Ramil's preset (`WearAtTone` helper, tone 128), then `UpdateSkin` with tone 0 → `IsMutableBuildPending()` is false, the body tint = neck tint × `SkinToneMultiplier(0)`, and the head's "Skin Tint" = `SkinToneMultiplier(0)`.
  - `ShinobiCoop.Mutable.UpdateSkinMovesTattoo`: a look with one tattoo, then `UpdateSkin` with OffsetU 64 → the body MID's "Tattoo 1 Offset" X changes, still with no pending build.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Mutable.UpdateSkin"`. Expected: FAIL (no member).
- [ ] **Step 3: Implement** (two lines plus the member).
- [ ] **Step 4: Rerun** with `-Filter "ShinobiCoop.Mutable.UpdateSkin+ShinobiCoop.Mutable.SkinTone+ShinobiCoop.Mutable.TattooOnBody"`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 4: CREATE structure: Character and Face tabs, one edit path, debounce, spinner

**Files:** Modify `Source/ShinobiCoop/Shinobi/UI/ShinobiCreatorPage.h/.cpp`; Test: new `Source/ShinobiCoop/Tests/ShinobiCreatorPageTests.cpp`.

**Interfaces:**
- `enum class ECategory : uint8 { Character, Clothes, BodySkin, Face, Num }`, labels CHARACTER, CLOTHES, BODY & SKIN, FACE.
- `enum class EShinobiLookEdit : uint8 { Rebuild, Debounced, Skin, Shape }`.
- `void EditLook(TFunctionRef<void(FShinobiAppearance&)> Change, EShinobiLookEdit Edit)` is the only way rows change the look:
  - Rebuild: re-dress next tick (today's `bRedressPending`).
  - Debounced: re-dress 0.15 s after the last such edit.
  - Skin: `Body->UpdateSkin(Look)`.
  - Shape: `UpdateBody`/`UpdateFace`.
  - Every kind refreshes the code box.
- `void ApplyPreset(int32 PresetIndex)` copies `Catalog->Presets[PresetIndex].Look` and the edited name (the roster name).
- `static void ValidateForSex(FShinobiAppearance& Look, const UShinobiCreatorCatalog& Catalog)`. It clears:
  - a head of the other sex (to None);
  - wardrobe rows with no mesh for the sex (to None);
  - face, blend and groom indices past the sex's counts (to defaults).

  It replaces today's `ResetForSex` and is used on every sex change and on `LoadCode`.
- `static TArray<int32> HeadsOfSex(const UShinobiCreatorCatalog&, EShinobiSex)` (indices into `Catalog.Heads`).

- [ ] **Step 1: Failing tests** (the page built in a test world with a stage, as `MenuRosterPickSaves` does):
  - `ShinobiCoop.UI.CreatorTabs`: four category buttons labelled as above.
  - `ShinobiCoop.UI.CreatorPresetPick`: `ApplyPreset(i)` for each catalog preset → `GetLook() == Catalog->Presets[i].Look`.
  - `ShinobiCoop.UI.CreatorHeadsBySex`: `HeadsOfSex(Female)` holds exactly Bika's and Cat's head indices.
  - `ShinobiCoop.UI.CreatorSexSwitchKeepsValidLook` (Review Focus 2): a female look with Bika's head, a female-only MHSample Top and face 5 → set Sex Male via `EditLook` → Head None, Top None (or a row with a male mesh), face index < male face count.
  - `ShinobiCoop.UI.CreatorDebouncedBlend`: two `Debounced` BuildBlend edits 0.05 s apart → no build pending 0.1 s after the second; a build is pending (or done with the second value) 0.2 s after it.
  - `ShinobiCoop.UI.CreatorLastPickWins` (Review Focus 3): two `Rebuild` Top edits on consecutive ticks → once nothing is pending, the stage's built look has the second Top (the `UShinobiMutableLook` last-request-wins path).
  - `ShinobiCoop.UI.CreatorEveryRowFocusable` (Review Focus 1): on each category, every button and slider in the rows is focusable (`IsFocusable` / `bIsFocusable`) and the rows' first widget is reachable by `Navigation` down from the category bar.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.UI.Creator"`. Expected: FAIL.
- [ ] **Step 3: Implement.**
  - CHARACTER tab:
    - Preset strip: a horizontal scroll of buttons, each `T_Portrait_<Roster>` (`ShinobiMenuArt`) over the roster name, in catalog order (all presets, including legacy characters).
    - The four save-slot buttons (moved from the top).
    - Sex picker.
    - Head picker: "MetaHuman face" (None) plus `HeadsOfSex` by name.
  - FACE tab:
    - With a static head: one line, "Roster head: its face and hair are fixed".
    - Otherwise: today's Face, Blend and face sliders, plus Hair, Beard and Mustache (the old HAIR tab).
  - Auto-framing: FACE calls `Stage->SetAutoZoom(1)`, the others 0.
  - Spinner: a `UCircularThrobber` on the page's canvas, right side, above the hints, visible while `Body->IsMutableBuildPending()`.
- [ ] **Step 4: Rerun** the Step 2 filter plus `ShinobiCoop.UI.Menu`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 5: CLOTHES tab: thumbnail strips and palette swatches

**Files:** `ShinobiCreatorPage.h/.cpp`, `ShinobiCreatorPageTests.cpp`.

**Interfaces:**
- Consumes `UShinobiMutableLook::FindRow(Slot, Index)` (index = table row order, both sexes) and `DT_Palette` (`ShinobiMutable::PalettePath`).
- Produces `static TArray<uint8> RowsForSex(EShinobiWardrobeSlot, EShinobiSex)` (table indices whose mesh for that sex is set) and `static bool IsTinted(const FShinobiWardrobeRow&, EShinobiSex)` (any non-null tinted material for the sex).

- [ ] **Step 1: Failing tests:**
  - `ShinobiCoop.UI.CreatorClothesStrips`: for each slot Top..Accessory with rows for the sex, the strip holds `RowsForSex(...).Num() + 1` buttons (None first).
  - `ShinobiCoop.UI.CreatorPickPiece`: clicking the strip's 2nd button sets `Look.Slot(Top) == RowsForSex(Top, Sex)[0]`.
  - `ShinobiCoop.UI.CreatorSwatchesOnlyWhenTinted`: a worn City Sample row (tinted) shows one swatch per palette row; an untinted row (an MHSample piece) shows none.
  - `ShinobiCoop.UI.CreatorSwatchSetsColour`: clicking swatch 3 sets `SlotColour[Top] == 3`.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.UI.Creator"`. Expected: the new ones FAIL.
- [ ] **Step 3: Implement.** Each slot gets:
  - a caption;
  - a `UScrollBox` (horizontal) of 72×72 buttons: the row's `Thumbnail`, else its row name in 11 pt;
  - the worn one styled active;
  - the palette swatches (22×22 coloured `UBorder` buttons) under the worn row when it is tinted.

  A slot that a worn row Replaces shows "(worn under <row>)" and no strip. The Cloak picker (legacy `Outfits`) stays until P5. No Wear slider: no row has `bHasWear` (see Rulings). Picks use `EShinobiLookEdit::Rebuild`.
- [ ] **Step 4: Rerun.** Expected: pass.
- [ ] **Step 5: Commit.**

### Task 6: BODY & SKIN tab: build, skin tone bar, make-up, freckles, tattoos

**Files:** `ShinobiCreatorPage.h/.cpp`, `ShinobiCreatorPageTests.cpp`.

**Interfaces:** Consumes `UpdateSkin` (Task 3), `DT_MakeUp`, `DT_Tattoos`, `DT_TattooRegions`, `ShinobiMutable::MaxTattoos`, `UShinobiBodyComponent::SkinToneMultiplier`.

- [ ] **Step 1: Failing tests:**
  - `ShinobiCoop.UI.CreatorBuildPickers`: the Build picker offers Slim, Average, Heavy, Own; picking Own sets `Look.Build == EShinobiBuild::Own`; the Blend slider maps 0..1 to `BuildBlend` 0..255.
  - `ShinobiCoop.UI.CreatorSkinToneInstant`: moving the tone slider to 0 → `Look.SkinTone == 0`, no build pending, the body tint changed.
  - `ShinobiCoop.UI.CreatorTattooAddEditDelete`: ADD → one tattoo (design 0, region 0, scale 128); its region picker → region 3; delete → none.
  - `ShinobiCoop.UI.CreatorTattooCap`: after three ADDs the ADD button is gone (collapsed) and `Tattoos.Num() == 3`.
  - `ShinobiCoop.UI.CreatorMakeUpAndFreckles`: the pickers step through `DT_MakeUp` rows by row name and Freckles None, 1, 2, 3, as Skin edits (no build pending).
- [ ] **Step 2: Run.** Expected: FAIL.
- [ ] **Step 3: Implement.**
  - Build picker and a Blend slider (Debounced), then today's body sliders (Shape).
  - Skin Tone: a slider over a strip of 16 `UBorder` steps coloured (0.8, 0.6, 0.5) × `SkinToneMultiplier(i × 17)`.
  - Make Up and Freckles pickers.
  - Tattoos, per entry:
    - a design strip (design textures, as Task 5's strip);
    - a Region picker (row names);
    - sliders Offset U and Offset V (−1..1 → int8 ×127), Rotation (0..1 → uint8) and Size (0..1 → `Scale` uint8);
    - a DELETE button.
  - "+ ADD TATTOO" while fewer than `MaxTattoos`.
  - All skin and tattoo edits are `Skin`. Picking the Neck region calls `Stage->SetAutoZoom(1)`; other regions 0.
- [ ] **Step 4: Rerun** `-Filter "ShinobiCoop.UI.Creator"`. Expected: pass.
- [ ] **Step 5: Commit.**

### Task 7: Thumbnails

**Files:** Modify `Source/ShinobiCoop/Shinobi/UI/ShinobiMenuShots.h/.cpp` (flag `-ShinobiThumbnailShot`), `Scripts/menu_shots.ps1` (`-Thumbnails`), `Scripts/setup_mutable.py` (stage `thumbnails`; `wardrobe` and `tattoos` fill `Thumbnail`); Test `ShinobiMutableTests.cpp`.

**Interfaces:** Shot output `Saved/MenuArt/Thumbnails/<Slot>_<RowName>.png` at 512×512. Textures go to `/Game/Shinobi/Mutable/Thumbnails/T_Thumb_<Slot>_<RowName>` (UI group, max size 256, sRGB).

- [ ] **Step 1: Failing test** `ShinobiCoop.Mutable.EveryRowHasThumbnail`: every row of every `DT_Wardrobe_<Slot>` and of `DT_Tattoos` has a `Thumbnail` that loads.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Mutable.EveryRowHasThumbnail"`. Expected: FAIL (thumbnails unset).
- [ ] **Step 3: Implement.**
  - The shot moves the stage into the studio (Studio preset).
  - For each slot and row it wears a default look of the row's first sex with a mesh (male first) at Average build, with only that piece. It waits for the build.
  - It frames the camera on the row mesh's bounds, seen 20° off front with a 10 % margin, and screenshots. Hints and spinner are hidden: the shot runs without the menu widget.
  - Stage `thumbnails` imports the PNGs and then calls `wardrobe()`, which sets `Thumbnail` where `T_Thumb_<Slot>_<Row>` exists.
  - `tattoos()` sets each design's `Thumbnail` to its own texture (see Rulings).
- [ ] **Step 4: Run** `Scripts/menu_shots.ps1 -Thumbnails`, then `SETUP_MUTABLE_ONLY=thumbnails,tattoos Scripts/run_python.ps1 -Script Scripts/setup_mutable.py -Editor`, then the test. Expected: PASS. Look at a contact sheet of the thumbnails: each piece is centred and recognisable.
- [ ] **Step 5: Commit** code and scripts (thumbnails stay local).

### Task 8: Phase close

- [ ] Shots:
  - `Scripts/menu_shots.ps1 -Creator`: a new `-ShinobiCreatorTour` flag in `UShinobiMenuShotSubsystem` steps through each CREATE category, each light preset, zoom 0/0.5/1 and yaw 0/90/180.
  - Repeat at `-ResX=1280 -ResY=720` and `-ResX=2560 -ResY=1080` (Review Focus 5).
  - Contact sheet for the user.
- [ ] `Scripts/run_tests.ps1` (full suite, once). Expected: only the two known failures (`Hero.PickSwapsBody`, `Outfit.EveryCopyHasCloth`).
- [ ] Final whole-branch review from the P4 base on the most capable model; one fix pass with RED→GREEN tests.
- [ ] `Scripts/package.ps1`, then the packaged Creator tour (`-Packaged`).
- [ ] Report to the user with rulings and deferred minors; update memory; delete the plan workspace. Don't push.

## Rulings made while planning

- No Wear slider: no wardrobe row has `bHasWear` and no material takes a Wear parameter; the spec says "where supported".
- Hair moves into FACE (the spec lists four tabs).
- Static heads: FACE shows only a note; "colour options only for static heads" has nothing to colour today (their hair is part of the head).
- Studio enclosed by a sphere backdrop instead of lighting channels: Lumen's indirect lighting ignores lighting channels, and an enclosed set keeps the rooftop sun out without per-component channel setup.
- Tattoo thumbnails are the design textures themselves (a render of a flat mask adds nothing).
- Text key badges stand in for mouse and gamepad icons (the project has no input glyph art).
- Auto-framing on tab change replaces any manual zoom; manual zoom holds until the next tab change, region pick or reset.
- The legacy Cloak picker stays until P5 (cloaks are P5).
