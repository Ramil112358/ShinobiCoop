# Mutable Customisation P3b (Skin and Tattoos) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A look's skin tone changes the body and head together, roster bodies match their heads' necks, and players can place tattoos and choose make-up and freckles. Each part is built only if the spike proves it can be done.

**Architecture:**
- **Fixed by the probe (2026-10-30):** the body and the MetaHuman faces use the engine's `M_skin_unified_baked`. Their skin is baked into virtual textures, and the only live colour controls are `Basecolor Global Multiply Post-Bake` and the concavity/convexity multipliers. There are no tone, age, make-up or freckle parameters. Static heads use our `M_Character`, which has no tint.
- **Skin tone:** a multiplier on the body, the MetaHuman face and (through a new `Skin Tint` parameter on `M_Character`) the static head.
- **Layers:** tattoos, make-up and freckles need texture layers. Task 2 is a spike that tries the two candidate routes and stops for the user's choice.

**Tech Stack:** UE 5.8 MetaHuman materials, Mutable 5.8 (texture projection), `MaterialEditingLibrary` (scripted material edits), editor C++ helpers, automation tests, `shot_chakra.ps1` contact sheets.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md`: the Skin section (Skin Tone, Aged, Make Up, Freckles, up to three tattoo projectors from `DT_Tattoos`), and the risks table ("MetaHuman skin material doesn't accept Mutable texture layers": Skin Tone and Aged become material parameters, and tattoos stay as texture layers on the body texture). P3 split skin and tattoos off into this plan.

## Global Constraints

- `FShinobiAppearance` already carries `SkinTone` (128 = the head's natural tone), `Aged`, `MakeUp`, `Freckles` and up to three `Tattoos` {Design, Region, OffsetU, OffsetV, Rotation, Scale}. The fields and the code format don't change.
- Generated content stays local (`/Game/Shinobi/Mutable/**`, MetaHuman builds); only code, scripts and docs are committed, path-limited. Nothing is pushed.
- Editor jobs run in separate processes, never in the user's open editor.
- Engine content is never edited. Copies go under `/Game/Shinobi/`.
- Test only what changed per task; the full suite once at the end.
- Research UE 5.8 specifics before implementing anything not settled here.

## Review Focus

1. **The default look (SkinTone 128, no tattoos)** looks exactly as it does today, on every roster preset. Test in Task 1.
2. **A static-head preset at tone 0 and tone 255:** the head and body change together, with no visible neck seam. Checked by eye in Task 1's sheet.
3. **Outfit changes and body swaps** keep the skin tone (the P2/P3 MID rules: `Wear` and `DressBuilt` empty the materials; `DressOutfit` keeps them). Test in Task 1.
4. **Legacy roster bodies on `M_Character`** (Hero, Bika, Cat, Misha, Pabu) aren't changed by the new `Skin Tint` parameter while it is white. Test in Task 1.
5. **A tattoo placed on a slim or heavy build** stays on the skin (it follows the UVs, not world space). Checked in the spike.

---

### Task 1: Skin tone on body and head, and tint calibration

**Files:**
- Modify: `Content/Shinobi/Characters/M_Character` (via script: a vector parameter `Skin Tint`, default white, multiplying base colour), `Scripts/setup_mutable.py` (stage `skintone`: the `M_Character` edit and `DT_SkinTones`), `Scripts/mutable/skin_tint.py` (calibration factor)
- Modify: `ShinobiMutableNames.h` (`StaticHeadTintParameter = "Skin Tint"`, `SkinTonesPath`), `ShinobiBodyComponent.cpp` (`ApplySkinTone`)
- Test: `ShinobiMutableTests.cpp`

**Interfaces:**
- Produces:
  - `DT_SkinTones` (`FShinobiPaletteRow` rows, darkest to lightest). Tone `t` in 0..255 picks the multiplier by linear interpolation along the rows, and 128 gives white exactly.
  - `UShinobiBodyComponent::ApplySkinTone(const FShinobiAppearance& Look)`. It is called from `DressBuilt` after the pieces are coloured, and sets:
    - the body slots that have `SkinTintParameter`: `HeadTint × Tone`, where `HeadTint` is the static head's `SkinTint`, or white;
    - the static head's `M_Character` slots: `Skin Tint = Tone`;
    - the MetaHuman face's skin slots (`SkinTintParameter`): `Tone`.
  - Calibration: `skin_tint.py` multiplies its texture ratio by a per-channel factor `SSS_FACTOR`, so the rendered neck seam matches. The factor is fitted from Task 1's close-up shots, and the heads' tints are re-baked (stage `heads` → `catalog`).

- [ ] **Step 1: Write the failing tests:**
  - `ShinobiCoop.Mutable.SkinToneNeutral`: a Ramil-preset look at tone 128 has the body tinted exactly `Head->SkinTint`, and the static head's `Skin Tint` is white.
  - `ShinobiCoop.Mutable.SkinToneDarkens`: tone 0 gives the body `Head->SkinTint × DT_SkinTones[0]` and the head `DT_SkinTones[0]`, with every channel below the neutral one.
  - `ShinobiCoop.Mutable.SkinToneOnMetaHumanFace`: a RamilMH preset at tone 0 puts the tone on its Face part's skin MIDs.
  - `ShinobiCoop.Mutable.SkinToneSurvivesOutfit`: tone 0 on a player whose outfit then changes keeps the body's value.
  - `ShinobiCoop.Hero.LegacyMaterialUnchanged`: the Hero's legacy body material reads `Skin Tint` = white.
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement. Run stage `skintone`, then take a close-up sheet of the seven static-head presets at tones 0/128/255 (extend `RunRosterPresetShot` with `-ShinobiSkinTones`). Fit `SSS_FACTOR` by eye from the neutral row, re-run `heads` and `catalog`, and ledger the factor.
- [ ] **Step 4:** Run `-Filter ShinobiCoop.Mutable+ShinobiCoop.Hero` → PASS (except the two failures that predate P2).
- [ ] **Step 5: Commit** `feat: skin tone on body and head together; tint calibrated against subsurface scatter`.

### Task 2: Spike: a skin layer for tattoos, make-up and freckles (throwaway, then stop)

Answers one question: which route puts a UV-placed image on a MetaHuman body and face at runtime, on every build, without breaking the baked skin?

- [ ] **Route A (Mutable projection):** in a throwaway copy of `CO_Shinobi` (`CO_SkinSpike`), give the body a Mesh Section whose base-colour texture is in Mutable mode, with a Texture Project node from a Projector Parameter. Then:
  - Build the male base with one tattoo on the upper arm, at Normal and at Over.
  - Record: does it compile with the dataless body? Do the virtual textures survive? Build time, memory, and a shot.
- [ ] **Route B (skin material layer):** a copy of `M_skin_unified_baked` as `/Game/Shinobi/Skin/M_ShinobiSkin`, with a scripted layer before its base-colour output:
  - Tattoo atlas texture with 3 UV transforms (offset, rotation, scale) and an alpha.
  - Make-up masks (lips, eyes, cheeks) with colours.
  - A freckle mask with a strength.
  - Re-parent one base's MI chain to it (local content), set one tattoo, and take a shot at Normal and Over. Record: shader compile, any visible change to the baked skin with the layer off, and draw cost.
- [ ] **Aged:** no parameter exists. Record what the concavity/convexity multipliers and a roughness offset can do (one shot), against dropping Aged.
- [ ] **Stop.** Send the user both routes' shots and numbers, with a recommendation. Ask them to choose:
  - the route for tattoos, make-up and freckles;
  - whether Aged is built as the cheap approximation or dropped.

  Write Tasks 3-5 from that choice (tattoo designs and regions; make-up and freckles on MetaHuman faces, plus static heads if the route allows; Aged). Then continue.

### Task 6: Phase close

- [ ] **Step 1:** Run the full suite once → PASS, except the failures that predate P2 (named).
- [ ] **Step 2:** Final whole-branch review (fresh reviewer, most capable model), then one fix pass.
- [ ] **Step 3:** Packaged smoke: `Scripts/package.ps1`, then `shot_chakra.ps1 -RosterPresets -Packaged`.
- [ ] **Step 4:** Report the suite, rulings and deferred minors. P4 (CREATE UI, thumbnails) is next.
