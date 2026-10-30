# Mutable customisation P5: cloaks, checks and merge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cloaks judged and settled on Mutable looks (timeboxed), the two long-standing test failures resolved, a made character with every new field checked over the network, the whole feature checked packaged, then merged to `origin/main`.

**Architecture:** Cloaks stay what they are: separate Chaos-cloth components dressed by `AShinobiPlayerCharacter::DressOutfit` from the per-frame fitted copies (`SK_<Outfit>_Crowd<Sex><Build>`), chosen through `UShinobiBodyComponent::GetFittingName()`. Mutable reshapes a garment's render mesh but not its cloth simulation (spec, measured 2026-10-29), so cloaks don't move into `DT_Wardrobe_Cloak`. The only code changes are the Own build's cloak choice, two test fixes, a log line the network smoke waits on, and the smoke's look code.

**Tech Stack:** UE 5.8 C++, PowerShell shot and smoke scripts.

**Spec:** `docs/superpowers/specs/2026-10-29-mutable-customisation-design.md` ("Cloaks (timeboxed)", "Testing", "Phases" item 6). Earlier phase: `docs/superpowers/plans/2026-10-30-mutable-customisation-p4-ui.md`.

## Global Constraints

- Unreal Engine 5.8; nothing new replicates (only `FShinobiAppearance`).
- Shared tree on local `main` (58 commits ahead of `origin/main`, which hasn't moved): path-limited commits of own files only.
- **No push without the user's explicit go-ahead** (they said "don't push yet"); Task 4 ends by asking.
- Generated Mutable content stays local and untracked; never run setup jobs in the user's open editor.
- Tests: only what changed per task; the full suite once in Task 4.
- Cloaks are timeboxed (the user: "cloaks don't matter much, will be remade"): one task, no new cloak meshes.

## Review Focus

1. A made character on a client, with the new fields (Own build, blend, a Fab piece, a colour, a tattoo, make-up), looks the same on the host. Test: Task 3 smoke.
2. The Own build (RamilMH's and BikaMH's own bodies) in a cloak doesn't show the body through it at idle. Check: Task 1 shots; test `OwnBuildCloakCopy`.
3. A roster character whose Mutable preset fails to build falls back to its legacy body, with or without a cloak copy, not to a missing body. Test: Task 2 (the rewritten `EveryCopyHasCloth` and the existing `MenuStageMissingOutfitShowsBody`).
4. The packaged game plays a made character and the roster presets after the merge's last build. Check: Task 4 packaged shots.
5. Pushing changes nothing others rely on: `origin/main` still has no new commits when the push happens. Check: Task 4 fetches and compares before pushing.

---

### Task 1: Cloaks on Mutable looks (timeboxed check)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCreatorSpikeShot.cpp`, `ShinobiPlayerController.h` (flag `-ShinobiCloakShot`), `Scripts/shot_chakra.ps1` (`-Cloaks`)
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiBodyComponent.cpp:107` (the Own build's fitting name)
- Test: `Source/ShinobiCoop/Tests/ShinobiMutableTests.cpp`

**Interfaces:** Produces `static EShinobiBuild UShinobiBodyComponent::CloakBuild(EShinobiSex Sex, EShinobiBuild Build)`: Under, Normal and Over map to themselves; Own maps to the crowd build whose copy fits that sex's own body best (chosen in Step 3 from the shots). `DressBuilt` uses it for `FittingName`.

- [ ] **Step 1: Shot.** `-ShinobiCloakShot`:
  - Every cloak (Hokage, Akatsuki) on each sex × Slim/Average/Heavy/Own, wearing the default Top/Bottom/Shoes, idle, front and back. Named `ShinobiCloakShot_<Cloak>_<Sex><Build>_<Front|Back>`.
  - Then the four preset roster characters (Ramil, Elvin, RamilMH, BikaMH) in each cloak.
  - `shot_chakra.ps1 -Cloaks` runs it on Training Grounds with the plain game mode (`?game=/Script/ShinobiCoop.ShinobiGameMode`; the arena's waves knock the character down).
- [ ] **Step 2: Run it and judge the contact sheet.** Acceptable: at idle no body or Mutable piece shows through the cloak at front or back on Average. Slim and Heavy may show small clipping.
- [ ] **Step 3: Rule.**
  - Own's cloak build: compare Own wearing the Normal copy and the Over copy for each sex in the sheet; pick the better.
  - If Average fails the bar, cloaks drop out of Mutable looks until they're remade. In that case CREATE's Cloak row hides and the presets wear none (the spec's fallback); otherwise everything stays.
  - Ledger the ruling.
- [ ] **Step 4: Failing test** `ShinobiCoop.Mutable.OwnBuildCloakCopy`: for each sex, `CloakBuild(Sex, Own)` is the ruled build; for each outfit with copies, `IsOutfitBuilt`-style lookup of `SK_<Outfit>_` + `FittingName(Sex, CloakBuild(Sex, Own))` loads. Run `-Filter ShinobiCoop.Mutable.OwnBuildCloakCopy`. Expected: FAIL (no `CloakBuild`).
- [ ] **Step 5: Implement**; rerun Step 4 (PASS); rerun the Own shots and confirm the change by eye.
- [ ] **Step 6: Commit.**

### Task 2: The two long-standing failures

**Files:** `Source/ShinobiCoop/Tests/ShinobiHeroTests.cpp` (PickSwapsBody), `Source/ShinobiCoop/Tests/ShinobiOutfitTests.cpp` (EveryCopyHasCloth); production code only if the cause is there.

`Hero.PickSwapsBody` ("RamilMH wears its MetaHuman face and hair") has failed since 2026-10-29. So has `Outfit.EveryCopyHasCloth` ("Hokage/Akatsuki on BikaMH: its copy is imported"); BikaMH never got legacy cloak copies.

- [ ] **Step 1: Find the cause** (superpowers:systematic-debugging): run each alone; read the test and the code path; check whether RamilMH now plays its Mutable preset (`Shinobi.RosterPresets` 2), so its face parts arrive only after the build.
- [ ] **Step 2: Fix by cause:**
  - **PickSwapsBody.** If the test asserts the legacy body while presets are on, it holds the switch at 0 with `FShinobiOwnBodies` (ShinobiTestWorld.h), as P2's legacy-body tests do. If RamilMH's legacy face really is gone, fix the code.
  - **EveryCopyHasCloth.** The roster characters that play Mutable presets (`AShinobiPlayerCharacter::PresetRoster`) wear the crowd copies (Task 1). For them, the test checks those copies instead of `SK_<Outfit>_<Body>`. A failed preset falls back to the legacy body, which without a copy shows no cloak (`MenuStageMissingOutfitShowsBody` covers that).
  - Ledger the ruling.
- [ ] **Step 3: Run** `-Filter "ShinobiCoop.Hero.PickSwapsBody+ShinobiCoop.Outfit+ShinobiCoop.UI.MenuStageMissingOutfit"`. Expected: PASS.
- [ ] **Step 4: Commit.**

### Task 3: A made character over the network

**Files:** `Scripts/smoke_m4.ps1`, `Source/ShinobiCoop/Shinobi/Hero/ShinobiBodyComponent.cpp` (one log line), test `ShinobiMutableTests.cpp`.

**Interfaces:** `DressBuilt` logs `"%s wears its Mutable look (%s)"` with the owner's name and `PendingLook.ToCode()` (Log verbosity).

- [ ] **Step 1: Failing test** `ShinobiCoop.Mutable.SmokeLookCodeIsFull`: decoding the smoke's code (a constant `ShinobiMutable::SmokeLookCode`, used by the test and copied into smoke_m4.ps1) gives a female look with:
  - Build Own and BuildBlend 200;
  - Top = the first female Fab row;
  - SlotColour[Top] 3;
  - one tattoo (design 1, region Back);
  - MakeUp 2.

  It round-trips through `ToCode` unchanged. Run it. Expected: FAIL (no constant).
- [ ] **Step 2: Implement** the constant (built by hand from `ToCode` of that look, written as the literal code string) and the log line. Rerun: PASS.
- [ ] **Step 3: smoke_m4.ps1** uses the constant's code. It also waits on the host log for `wears its Mutable look (<code>)` for the client's pawn ("host built the client's made character") and the same on the client. Run `Scripts/smoke_m4.ps1`. Expected: every step passes.
- [ ] **Step 4: Commit.**

### Task 4: Phase close and merge

- [ ] `Scripts/run_tests.ps1` (full suite, once). Expected: all pass (the two old failures fixed in Task 2).
- [ ] Final review of P5 (`review-package` from the P5 base) on the most capable model; one fix pass with RED→GREEN tests.
- [ ] `Scripts/package.ps1`; packaged `shot_chakra.ps1 -RosterPresets -Map <plain game mode>`, `menu_shots.ps1 -Creator -Packaged`, `shot_chakra.ps1 -Cloaks -Packaged`.
- [ ] Report to the user: rulings, deferred minors, the cloak sheet; update memory; delete the plan workspace.
- [ ] **Ask the user to approve the push.** Only after their yes:
  - `git fetch`;
  - confirm `origin/main` hasn't moved past this tree's base (otherwise stop and ask);
  - `git push origin main`.
