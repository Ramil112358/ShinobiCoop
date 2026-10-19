# Jutsu Loadout, 8-Slot Bar, Skills Screen and Shortcuts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 5 fixed jutsu pages with a player-chosen 8-slot loadout (server-authoritative), an 8-slot HUD bar, a Skills screen (K) to equip jutsus, and a Shortcuts screen to rebind keyboard/mouse keys.

**Architecture:** A pure `ShinobiLoadout` module owns slot math, validation and save/load. The player character holds a replicated `Loadout` (8 jutsu classes) and stamps each jutsu spec's ASC `InputID` from it (Jutsu1..Jutsu8 = slots 1..8; unslotted = `INDEX_NONE`), so the existing `AbilityLocalInputPressed` path fires slots with no page gate. A pure `ShinobiKeyBindings` module owns rebindable keys and rewrites the code-built mapping context. HUD, Skills screen and Shortcuts screen are C++-built UMG widgets like the existing HUD/menus.

**Tech Stack:** UE 5.8 C++, GAS, Enhanced Input (code-built `UInputAction`/`UInputMappingContext`, `UInputTriggerChordAction`/`UInputTriggerChordBlocker`), UMG built in C++, `GConfig` + `GGameUserSettingsIni`, UE automation tests (`Scripts/run_tests.ps1`), build via `Scripts/build.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-19-jutsu-loadout-hud-design.md`

## Global Constraints

- 8 slots. Default loadout: 1 Fireball, 2 Chidori, 3 Rasengan, 4 Amaterasu, 5 Teleport, 6 Kamui, 7 Almighty Push, 8 Susanoo.
- Default keys: slots 1, 2, 3, 4, R, T, G, Caps Lock; Skills screen K; Melee Left Mouse; Dodge Left Ctrl; Sprint Left Shift; Block F; Chakra charge C; Revive E; Lock-on Tab; Jump Space; Dismount dragon X; Map M.
- Alt is the only modifier. Ctrl/Shift are never modifiers.
- Unbindable: Esc, Left/Right Windows, Alt+F4, Alt+Tab, W, A, S, D, Right Mouse, any gamepad key.
- Gamepad fixed: RB + A/B/X/Y (Bottom/Right/Left/Top) = current pad page's slots 1-4 or 5-8; RB + D-pad right/left flips page 1 ⇄ 2.
- In combat = within 5 s after the player last dealt damage to, or took damage from, a hostile character (includes DoT, own summons, dragon breath, fire patches; excludes self and teammates).
- Whole loadout locked while: casting a jutsu, `State_Mounted`, `State_Downed`, in combat, PvP phase `Fighting`.
- One slot locked while: its jutsu is active or on cooldown; Kamui + `State_Phased`; Teleport + `State_TeleportMarked`; Susanoo + `State_Susanoo`; a summon jutsu while its summon is alive.
- Loadout and bindings save per PC in `GGameUserSettingsIni` (sections `/Script/ShinobiCoop.ShinobiLoadout`, `/Script/ShinobiCoop.ShinobiKeyBindings`).
- Server is the authority for which jutsu each slot fires; the client never fires an unslotted jutsu.
- Per the user's CLAUDE.md: run only the tests touched by each task; run the full suite once at the end.
- **Staging rule:** the working tree also holds the user's uncommitted fix/movement-tilt edits: `ShinobiLocomotionAnimInstance.cpp`, `ShinobiPhysicalReactionComponent.cpp`, `ShinobiMovementComponent.{h,cpp}`, `Tests/ShinobiCastTests.cpp`, `Tests/ShinobiPhysicalReactionTests.cpp`, `Tests/ShinobiSurfaceTraversalTests.cpp`. Stage files by explicit path only, never `git add -A`/`.`. If a task needs to change one of those 7 files, stop and ask the user first.
- Every commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Never switch/merge branches with the Unreal Editor open (memory: git-merge-editor-lock). Close the editor before building from the command line.

## Review Focus

1. **Spec `InputID` reaching the owning client.** The client predicts with its local specs; if a loadout change doesn't reach them, keys fire the old jutsu or nothing. Expect: after a swap, the owner's next key press fires the new jutsu. Test: Task 2 `ShinobiCoop.Loadout.OnRepRestampsInputIDs` calls `OnRep_Loadout` on a spawned player and checks the spec InputIDs.
2. **Follow-ups after reorder.** Moving Kamui from slot 6 to slot 2 must move Kamui-exit with it. Test: Task 2 `ShinobiCoop.Loadout.FollowUpsTrackTheirParentSlot`.
3. **A held key across a pad page flip.** Pressing RB+A on page 1, flipping the page, then releasing must release slot 1, not slot 5 (charged jutsus). Test: Task 4 `ShinobiCoop.Input.PadReleaseUsesPressedSlot`.
4. **Plain key firing under Alt.** Alt+1 must not also fire the slot bound to 1. Test: Task 5 `ShinobiCoop.Input.AltBindingsChordAndBlock`.
5. **A corrupt or stale saved loadout** (renamed class, duplicates, wrong count). Expect: falls back to defaults, no crash. Test: Task 1 `ShinobiCoop.Loadout.SaveLoadRoundTrip` writes garbage and expects the default.

---

## File Structure

| File | Responsibility |
|---|---|
| Create `Source/ShinobiCoop/Shinobi/Abilities/ShinobiLoadout.{h,cpp}` | Slot math, default loadout, validation, edit operations, save/load. No world access. |
| Create `Source/ShinobiCoop/Shinobi/UI/ShinobiKeyBindings.{h,cpp}` | Rebindable bindings: defaults, labels, blocked keys, swap-on-conflict, save/load. |
| Create `Source/ShinobiCoop/Shinobi/UI/ShinobiSkillsWidget.{h,cpp}` | K screen. |
| Create `Source/ShinobiCoop/Shinobi/UI/ShinobiShortcutsWidget.{h,cpp}` | Shortcuts screen. |
| Create `Source/ShinobiCoop/Tests/ShinobiLoadoutTests.cpp` | Loadout module + character loadout + combat tests. |
| Create `Source/ShinobiCoop/Tests/ShinobiKeyBindingTests.cpp` | Key binding tests. |
| Modify `Shinobi/Abilities/ShinobiGameplayAbility.h` | Append `Jutsu5..Jutsu8` to `EShinobiAbilityInputID`; add `FollowsSlotOf`. |
| Modify `Shinobi/Abilities/ShinobiJutsuAbility.{h,cpp}` | Drop `Page` and its gate; add `IsSlotLocked`, `Description`; gate unslotted player specs. |
| Modify `Shinobi/Abilities/GA_*.cpp` | Drop `Page =`, jutsu `InputID =` and `JutsuPage(n)` tags; set `FollowsSlotOf`; lock overrides; descriptions. |
| Modify `Shinobi/ShinobiPlayerCharacter.{h,cpp}` | Remove page system; add loadout, RPC, lock reasons, combat time, pad page routing. |
| Modify `Shinobi/ShinobiTags.{h,cpp}` | Remove `State_JutsuPage_*`, `NumJutsuPages`, `JutsuPage()`. |
| Modify `Shinobi/ShinobiAttributeSet.cpp`, `Shinobi/ShinobiCombat.{h,cpp}` | Record combat on damage. |
| Modify `Shinobi/ShinobiInputConfig.{h,cpp}` | Slot actions 1-8, pad face actions, Alt modifier, Skills action, `ApplyKeyBindings`. |
| Modify `Shinobi/ShinobiPlayerController.{h,cpp}` | K toggle, last-input-device tracking, `ShinobiPage` → `ShinobiEquipPage`. |
| Modify `Shinobi/UI/ShinobiHUDWidget.{h,cpp}` | 8-slot single-row bar. |
| Modify `Shinobi/UI/ShinobiMenuWidgets.{h,cpp}` | Pause menu "Jutsu" entry; Settings "Shortcuts" entry. |
| Modify `Tests/ShinobiTestWorld.h` + page-using tests | `ShinobiTest::Equip` helper; migrate off pages. |

---

### Task 1: Loadout module (pure)

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiLoadout.h`, `ShinobiLoadout.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h:12-26` (append `Jutsu5, Jutsu6, Jutsu7, Jutsu8` after `Block`, keeping the "appended last" rule)
- Test: `Source/ShinobiCoop/Tests/ShinobiLoadoutTests.cpp`

**Interfaces:**
- Produces (namespace `ShinobiLoadout`, all `SHINOBICOOP_API`):
  - `constexpr int32 NumSlots = 8;`
  - `using FSlots = TArray<TSubclassOf<UShinobiJutsuAbility>>;` (always `NumSlots` long; null = empty)
  - `EShinobiAbilityInputID SlotInputID(int32 SlotIndex)` (0→Jutsu1 … 7→Jutsu8; out of range → None)
  - `int32 SlotIndexOf(EShinobiAbilityInputID ID)` (inverse; else `INDEX_NONE`)
  - `FSlots Default()`
  - `bool IsValid(const FSlots& Slots, const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known)`: exactly `NumSlots`; each entry null or in `Known`; no duplicates.
  - `bool CanChange(const FSlots& Current, const FSlots& Requested, TFunctionRef<bool(int32 SlotIndex)> IsSlotLocked)`: true when identical; false when any index `i` with `Current[i] != Requested[i]` has `IsSlotLocked(i)` true. (Whole-loadout locks are the caller's job.)
  - `FSlots Place(const FSlots& Current, TSubclassOf<UShinobiJutsuAbility> Jutsu, int32 SlotIndex)`: puts Jutsu at SlotIndex; if Jutsu was already at another index, the two indices swap contents; otherwise the replaced jutsu leaves the loadout.
  - `FSlots Swap(const FSlots& Current, int32 A, int32 B)`
  - `FSlots Clear(const FSlots& Current, int32 SlotIndex)`
  - `FSlots LoadSaved(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known)`: reads `Slots` (string array of class path names, `""` for empty) from `/Script/ShinobiCoop.ShinobiLoadout` in `GGameUserSettingsIni`; invalid → `Default()`.
  - `void Save(const FSlots& Slots)`: writes and `GConfig->Flush`.

- [ ] **Step 1: Write the failing tests** in `ShinobiLoadoutTests.cpp`, same style as `ShinobiSettingsTests.cpp` (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `ShinobiTestFlags`):
  - `ShinobiCoop.Loadout.SlotIds`: `SlotInputID(0)==Jutsu1`, `SlotInputID(7)==Jutsu8`, `SlotInputID(8)==None`, `SlotIndexOf(Jutsu5)==4`, `SlotIndexOf(Melee)==INDEX_NONE`.
  - `ShinobiCoop.Loadout.DefaultIsTheSpecOrder`: `Default()` equals `{UGA_Fireball, UGA_Chidori, UGA_Rasengan, UGA_Amaterasu, UGA_Teleport, UGA_Kamui, UGA_AlmightyPush, UGA_Susanoo}`.
  - `ShinobiCoop.Loadout.Validation`: 7 entries → false; duplicate Fireball → false; a class not in Known → false; one null entry → true.
  - `ShinobiCoop.Loadout.EditOps`: `Place(Default(), UGA_WaterDragon, 2)` → slot 2 Water Dragon, Rasengan absent; `Place(Default(), UGA_Kamui, 0)` → slot 0 Kamui, slot 5 Fireball; `Swap(Default(),0,7)` → slot 0 Susanoo, slot 7 Fireball; `Clear(Default(),3)[3]` null.
  - `ShinobiCoop.Loadout.CanChange`: identical + all locked → true; changing slot 2 while slot 2 is locked → false; changing slot 2 while only slot 5 is locked → true; swapping slots 2 and 5 while slot 5 is locked → false.
  - `ShinobiCoop.Loadout.SaveLoadRoundTrip`: save a modified loadout, `LoadSaved` returns it; write `Slots` = `{"/Script/ShinobiCoop.GA_NoSuchJutsu", …}` → `LoadSaved` returns `Default()`. Restore the old ini value at the end, as `ShinobiSettingsTests` does.

- [ ] **Step 2: Build and run to verify they fail**
  Run: `powershell -File Scripts/build.ps1` then `powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop.Loadout`
  Expected: build fails (`ShinobiLoadout.h` missing).

- [ ] **Step 3: Implement `ShinobiLoadout.{h,cpp}` and append `Jutsu5..Jutsu8`**

- [ ] **Step 4: Run the tests to verify they pass**
  Run: `powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop.Loadout`
  Expected: 6 tests PASS.

- [ ] **Step 5: Commit**
  `git add Source/ShinobiCoop/Shinobi/Abilities/ShinobiLoadout.* Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h Source/ShinobiCoop/Tests/ShinobiLoadoutTests.cpp && git commit -m "feat: jutsu loadout module (8 slots, validation, save)"`

---

### Task 2: Slot-driven activation replaces pages

**Files:**
- Modify: `Shinobi/Abilities/ShinobiGameplayAbility.h` — add `UPROPERTY() TSubclassOf<UGameplayAbility> FollowsSlotOf;` ("this spec fires from whichever slot holds that jutsu").
- Modify: `Shinobi/Abilities/ShinobiJutsuAbility.{h,cpp}` — delete `Page` (h:55) and the page clause in `CanActivateAbility` (cpp:42-46). New rule: when the avatar is an `AShinobiPlayerCharacter`, the spec's `InputID` must not be `INDEX_NONE`. Add `UPROPERTY(EditDefaultsOnly) FText Description;` and `virtual bool IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const;` (base: `Spec.IsActive() || ASC.HasAnyMatchingGameplayTags(CooldownTags)`).
- Modify: every `Shinobi/Abilities/GA_*.cpp` jutsu — delete `Page = n;` and `InputID = EShinobiAbilityInputID::JutsuN;` on jutsu classes. `UGA_KamuiExit`: `FollowsSlotOf = UGA_Kamui::StaticClass()`, drop `JutsuPage(2)`. `UGA_TeleportJump`: `FollowsSlotOf = UGA_Teleport::StaticClass()`, drop `JutsuPage(2)`, drop `InputID`. `UGA_DragonDismount`: drop `InputID` and `JutsuPage(5)`. `UGA_DragonDismountKey`: keep `InputID = Dismount`, delete the `RemoveTag(JutsuPage(5))` line. Lock overrides: `UGA_Kamui` adds `State_Phased`, `UGA_Teleport` adds `State_TeleportMarked`, `UGA_Susanoo` adds `State_Susanoo`, `UGA_SummonBase` adds "a live `AShinobiSummon` whose `GetSummoner()` is this avatar and whose kind is `SummonKind`".
- Modify: `UGA_SummonDragonBase::ExecuteJutsu` (`GA_Dragon.cpp:~44`) — on authority, set the `UGA_DragonDismount` spec's (exact class) `InputID` to this spec's `InputID` and mark it dirty, so the summoning dragon's slot dismounts.
- Modify: `Shinobi/ShinobiTags.{h,cpp}` — remove `NumJutsuPages`, `State_JutsuPage_1..5`, `JutsuPage()`.
- Modify: `Shinobi/ShinobiPlayerCharacter.{h,cpp}` — remove `GetJutsuPage`, `SetJutsuPage`, `TryCycleJutsuPage`, `ApplyJutsuPage`, `PageHasJutsu`, `ServerSetJutsuPage`, `ClientSetJutsuPage`, `CycleJutsuPage` and the `JutsuPage(1)` tag in `BeginPlay`. Add the members below.
- Modify: `Shinobi/ShinobiPlayerController.{h,cpp}` — rename `ShinobiPage(int32)` to `ShinobiEquipPage(int32 Page)`: equips the old page's jutsus into slots 1-4 (table: 1 Fireball/Chidori/Rasengan/Amaterasu; 2 Teleport/Kamui/AlmightyPush/EarthWall; 3 WaterWall/WaterDragon/FireMeteor/IceMeteor; 4 Pusho/Owl/Misha/Pabu; 5 Tairn/Crimson/Susanoo/empty) via `ApplyLoadout`, so the showcase timelines (`ShinobiPlayerController.cpp:340-990`) keep working. Replace every `ShinobiPage(` call.
- Modify: `Tests/ShinobiTestWorld.h` — add `EShinobiAbilityInputID ShinobiTest::Equip(AShinobiPlayerCharacter* P, TSubclassOf<UShinobiJutsuAbility> Jutsu, int32 SlotIndex = 0)`: `ApplyLoadout(ShinobiLoadout::Place(P->GetLoadout(), Jutsu, SlotIndex))`, returns `SlotInputID(SlotIndex)`.
- Modify: the 16 page-using test call sites (`ShinobiMountTests.cpp`, `ShinobiMovementJutsuTests.cpp`, others found by `grep -rn "JutsuPage\|ApplyJutsuPage" Source/ShinobiCoop/Tests`) — replace `P->ApplyJutsuPage(n); Tap(P, JutsuK)` with `Tap(P, ShinobiTest::Equip(P, UGA_X::StaticClass()))`. Rename `ShinobiCoop.Mount.TheDismountKeyWorksOnAnyPage` to `ShinobiCoop.Mount.TheDismountKeyWorksWhateverTheSlots` (unequip both dragons, X still dismounts). Remove the "Q pages forward" / wheel paging asserts from `ShinobiInputTests.cpp:93-97` (Task 4 rewrites that test).
- Test: `Tests/ShinobiLoadoutTests.cpp`

**Interfaces:**
- Consumes: Task 1 `ShinobiLoadout::*`.
- Produces on `AShinobiPlayerCharacter`:
  - `const ShinobiLoadout::FSlots& GetLoadout() const;`
  - `void ApplyLoadout(const ShinobiLoadout::FSlots& Slots);` — authority, no rules: sets `Loadout`, stamps spec InputIDs.
  - `void RequestLoadout(const ShinobiLoadout::FSlots& Slots);` — owner: calls `ServerRequestLoadout` (or `TryChangeLoadout` directly on authority) and `ShinobiLoadout::Save`.
  - `UFUNCTION(Server, Reliable) void ServerRequestLoadout(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Slots);` — `IsValid` against granted jutsus, then `TryChangeLoadout`; on rejection nothing changes (the replicated `Loadout` keeps the client right).
  - `bool TryChangeLoadout(const ShinobiLoadout::FSlots& Slots);` — authority: `GetLoadoutLockReason().IsEmpty()` (or Slots identical) and `ShinobiLoadout::CanChange(Loadout, Slots, IsSlotLocked)`, then `ApplyLoadout`.
  - `FText GetLoadoutLockReason() const;` — empty when swaps are allowed; otherwise "Casting", "Mounted", "Downed", "In combat" (Task 3), "Round in progress" (`AShinobiGameState::PvpPhase == EShinobiPvpPhase::Fighting`).
  - `bool IsSlotLocked(int32 SlotIndex) const;` — calls the slotted jutsu's `IsSlotLocked` with its spec; empty slot → false.
  - `UPROPERTY(ReplicatedUsing = OnRep_Loadout) TArray<TSubclassOf<UShinobiJutsuAbility>> Loadout;` (`COND_OwnerOnly`) + `UFUNCTION() void OnRep_Loadout();` that re-stamps local spec InputIDs.
  - Stamping rule: each spec whose ability is a `UShinobiJutsuAbility` → `SlotInputID(index)` or `INDEX_NONE`; each spec whose ability has `FollowsSlotOf` → that parent's slot ID or `INDEX_NONE`; `MarkAbilitySpecDirty`. Leave other specs alone.
  - Startup: on authority after abilities are granted, `ApplyLoadout(Default())`; the locally controlled owner then sends `RequestLoadout(ShinobiLoadout::LoadSaved(Known))`. Known = granted jutsu classes.

- [ ] **Step 1: Write failing tests** in `ShinobiLoadoutTests.cpp` (spawn via `FShinobiTestWorld`, as other tests do):
  - `ShinobiCoop.Loadout.DefaultSlotsFire`: a fresh player's `GetLoadout()` equals `Default()`; Fireball's spec InputID is `Jutsu1`, Susanoo's is `Jutsu8`, Water Dragon's is `INDEX_NONE`.
  - `ShinobiCoop.Loadout.UnslottedJutsuCannotActivate`: `TryActivateAbility` on the Water Dragon spec handle returns false; after `Equip(P, UGA_WaterDragon, 2)` a `Tap(P, Jutsu3)` activates Water Dragon.
  - `ShinobiCoop.Loadout.FollowUpsTrackTheirParentSlot`: `ApplyLoadout(Swap(Default(), 5, 1))` → Kamui-exit spec InputID `Jutsu2`; Kamui unslotted → Kamui-exit `INDEX_NONE`.
  - `ShinobiCoop.Loadout.OnRepRestampsInputIDs`: set the spec InputIDs to garbage, call `OnRep_Loadout()`, they match the loadout again.
  - `ShinobiCoop.Loadout.CooldownLocksSlot`: cast Fireball, wait for its cooldown to start, `TryChangeLoadout(Place(Loadout, UGA_WaterWall, 0))` → false; the same change on slot 1 → true.
  - `ShinobiCoop.Loadout.MountedLocksAll`: add `State_Mounted` → `GetLoadoutLockReason()` not empty and `TryChangeLoadout` of any change → false.
  - `ShinobiCoop.Loadout.DragonSlotDismounts`: `Equip(P, UGA_SummonCrimsonDragon, 6)`, tap `Jutsu7` → mounted; tap `Jutsu7` again → dismounted.

- [ ] **Step 2: Build; expect compile failures** at every removed page symbol. Fix all call sites listed above.

- [ ] **Step 3: Implement** the members and ability edits above.

- [ ] **Step 4: Run the changed suites**
  Run: `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop.Loadout+ShinobiCoop.Mount+ShinobiCoop.MovementJutsu+ShinobiCoop.Summon+ShinobiCoop.Jutsu"`
  Expected: all PASS.

- [ ] **Step 5: Commit** — `git commit -m "feat: jutsus fire from loadout slots; remove jutsu pages"` (see the staging rule in Global Constraints).

---

### Task 3: In-combat timer

**Files:**
- Modify: `Shinobi/ShinobiPlayerCharacter.{h,cpp}`, `Shinobi/ShinobiAttributeSet.cpp:100-112`, `Shinobi/ShinobiCombat.{h,cpp}`
- Test: `Tests/ShinobiLoadoutTests.cpp`

**Interfaces:**
- Produces:
  - `AShinobiPlayerCharacter`: `static constexpr float CombatSeconds = 5.f;` `UPROPERTY(Replicated) float LastCombatTime = -1000.f;` (`COND_OwnerOnly`, server world time) · `void NoteCombat();` (authority: `LastCombatTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds()`) · `bool IsInCombat() const;` · `float GetCombatSecondsLeft() const;` (for the Skills banner).
  - `ShinobiCombat::NoteCombat(const AActor* Instigator, AShinobiCharacterBase* Victim)`: resolves Instigator to a character (an `AShinobiSummon` or `AShinobiOwl` → `GetSummoner()`; an ability-system owner → its avatar); if both resolve and `Victim->IsHostileTo(Source)`, calls `NoteCombat()` on whichever is an `AShinobiPlayerCharacter`.
  - Hook: in `UShinobiAttributeSet::PostGameplayEffectExecute`, when `Damage > 0.f`, call it with `Data.EffectSpec.GetEffectContext().GetOriginalInstigator()` (fallback `GetInstigator()`) and the owning character.
  - `GetLoadoutLockReason()` returns "In combat" when `IsInCombat()`.

- [ ] **Step 1: Write failing tests**
  - `ShinobiCoop.Loadout.DamageStartsCombat`: player + enemy; `ShinobiCombat::ApplyDamage(player ASC, enemy, 10)` → `IsInCombat()` true on the player; after advancing world time 5.1 s (as other tests tick the world) → false.
  - `ShinobiCoop.Loadout.TakingDamageStartsCombat`: the enemy damages the player → true.
  - `ShinobiCoop.Loadout.SummonHitsCount`: damage applied with a summon as instigator → the summoner is in combat.
  - `ShinobiCoop.Loadout.TeammateDamageDoesNot`: player A damages player B (same team) → neither is in combat.
  - `ShinobiCoop.Loadout.CombatLocksAll`: in combat → `TryChangeLoadout` of any change returns false.
- [ ] **Step 2: Run** `run_tests.ps1 -Filter ShinobiCoop.Loadout`; expect the 5 new tests FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `run_tests.ps1 -Filter "ShinobiCoop.Loadout+ShinobiCoop.Damage"`; expect PASS.
- [ ] **Step 5: Commit** — `git commit -m "feat: 5 s in-combat timer locks jutsu swaps"`

---

### Task 4: Slot inputs and gamepad pages

**Files:**
- Modify: `Shinobi/ShinobiInputConfig.{h,cpp}`, `Shinobi/ShinobiPlayerCharacter.{h,cpp}` (input setup at ~636-670, `AbilityInputPressed` ~914-939)
- Test: `Tests/ShinobiInputTests.cpp`

**Interfaces:**
- Produces on `UShinobiInputConfig`:
  - `AbilityActions` gains `IA_Jutsu5..IA_Jutsu8` (Jutsu5..8). `IA_Jutsu1..8` map to **keyboard keys only** in `Context` (pad face keys move off them).
  - `UPROPERTY() TArray<TObjectPtr<UInputAction>> PadJutsuActions;` — 4 actions `IA_PadJutsu1..4` mapped in `JutsuContext` to `Gamepad_FaceButton_Bottom/Right/Left/Top`.
  - `JutsuPageNext`/`JutsuPagePrev`: gamepad only (`JutsuContext`, D-pad right/left). Q mapping removed.
  - `UPROPERTY() TObjectPtr<UInputAction> SkillsScreen;` (K, in `Context`) and `UPROPERTY() TObjectPtr<UInputAction> AltModifier;` (Left Alt, Right Alt, in `Context`).
- Produces on `AShinobiPlayerCharacter`:
  - `int32 GetPadJutsuPage() const;` (1 or 2, local only, starts 1) · `void FlipPadJutsuPage(int32 Direction);` (wraps 1⇄2).
  - `void PadJutsuPressed(int32 Face);` / `void PadJutsuReleased(int32 Face);` — Face 0..3; press records `PadPressedSlot[Face] = (Page-1)*4 + Face` and calls `AbilityInputPressed(SlotInputID(slot))`; release uses the recorded slot.
  - `AbilityInputPressed`'s `Jutsu1..Jutsu4` range checks (cpp:921) widen to `Jutsu1..Jutsu8` via `ShinobiLoadout::SlotIndexOf(InputID) != INDEX_NONE`.
- `AShinobiPlayerController` binds `SkillsScreen` → `ToggleSkillsScreen()` (body arrives in Task 7; add an empty stub now).

- [ ] **Step 1: Update/add tests in `ShinobiInputTests.cpp`**
  - Rewrite `ShinobiCoop.Input.MappingsAreCorrect` jutsu section: `IA_Jutsu1..8` on One, Two, Three, Four, R, T, G, CapsLock in `Context`; none of them in `JutsuContext`; `PadJutsuActions[0..3]` on face Bottom/Right/Left/Top in `JutsuContext`; `JutsuPageNext` not on Q; `SkillsScreen` on K; `AltModifier` on LeftAlt and RightAlt.
  - `ShinobiCoop.Input.PadPagesReachAllSlots`: page 1, `PadJutsuPressed(0)` activates slot 1's jutsu; `FlipPadJutsuPage(1)`; `PadJutsuPressed(3)` activates slot 8 (Susanoo).
  - `ShinobiCoop.Input.PadReleaseUsesPressedSlot`: `PadJutsuPressed(0)` on page 1, then `FlipPadJutsuPage(1)` → `GetPadPressedSlot(0) == 0`; `PadJutsuReleased(0)` → `GetPadPressedSlot(0) == INDEX_NONE`. (Add `int32 GetPadPressedSlot(int32 Face) const` to the character.)
- [ ] **Step 2: Run** `run_tests.ps1 -Filter ShinobiCoop.Input`; expect FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `run_tests.ps1 -Filter ShinobiCoop.Input`; expect PASS.
- [ ] **Step 5: Commit** — `git commit -m "feat: keys for 8 jutsu slots, gamepad 2 pages of 4"`

---

### Task 5: Key bindings module + live rebinding

**Files:**
- Create: `Shinobi/UI/ShinobiKeyBindings.{h,cpp}`
- Modify: `Shinobi/ShinobiInputConfig.{h,cpp}` — keyboard keys of bindable actions come from `ShinobiKeyBindings::Get`; add `void ApplyKeyBindings() const;` (re-maps bindable actions in `Context`, keeps their fixed gamepad keys, then calls `RequestRebuildControlMappings()` on every local player's `UEnhancedInputLocalPlayerSubsystem`). Called at the end of `Build()` and by `ShinobiKeyBindings::Rebind/ResetToDefaults`.
- Test: `Tests/ShinobiKeyBindingTests.cpp`

**Interfaces:**
- Produces (namespace `ShinobiKeyBindings`):
  - `enum class EBindable : uint8 { Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8, SkillsScreen, Melee, Dodge, Sprint, Block, ChakraCharge, Revive, LockOn, Jump, Dismount, Map, Num };`
  - `struct FBinding { FKey Key; bool bAlt = false; bool operator==(const FBinding&) const = default; };`
  - `FBinding Default(EBindable)`: the Global Constraints defaults.
  - `FBinding Get(EBindable)`: saved value, or the default if missing or blocked.
  - `bool IsBlocked(const FBinding&)`: the Global Constraints unbindable list; any `Key.IsGamepadKey()` is also blocked.
  - `enum class ERebind : uint8 { Done, Swapped, Blocked };` · `ERebind Rebind(EBindable Which, const FBinding& New, EBindable* OutSwappedWith = nullptr)`: if the binding is blocked, return Blocked. If another bindable holds New, that one takes Which's old binding (Swapped). Then save and `ApplyKeyBindings()`.
  - `void ResetToDefaults()`.
  - `FText Label(EBindable)`: "Slot 1".."Slot 8", "Skills screen", "Melee", "Dodge", "Sprint", "Block", "Chakra charge", "Revive", "Lock-on", "Jump", "Dismount dragon", "Map".
  - `FText KeyLabel(const FBinding&)`: e.g. "1", "Alt+1", "Caps", "Mouse 4", "Middle Mouse", "Left Mouse" (short forms for the HUD: CapsLock→"Caps", ThumbMouseButton→"M4", ThumbMouseButton2→"M5", MiddleMouseButton→"MMB" when `bShort`): signature `FText KeyLabel(const FBinding& B, bool bShort = false)`.
  - Storage: one string per bindable under `/Script/ShinobiCoop.ShinobiKeyBindings` in `GGameUserSettingsIni`, key = the enum name ("Slot1"…), value = `"Alt+" + Key.GetFName()` or `Key.GetFName()`.
- Alt handling in `ApplyKeyBindings`: an Alt binding gets a `UInputTriggerChordAction` with `ChordAction = AltModifier`. Every plain keyboard binding gets a `UInputTriggerChordBlocker` with `ChordAction = AltModifier`, so Alt+1 never fires plain 1.

- [ ] **Step 1: Write failing tests** (save and restore the ini section around each test):
  - `ShinobiCoop.KeyBindings.Defaults`: `Get(Slot5).Key == EKeys::R`, `Get(Slot8).Key == EKeys::CapsLock`, `Get(SkillsScreen).Key == EKeys::K`, `Get(Melee).Key == EKeys::LeftMouseButton`, `Get(Map).Key == EKeys::M`.
  - `ShinobiCoop.KeyBindings.SwapOnConflict`: `Rebind(Slot8, {G})` → Swapped, `OutSwappedWith == Slot7`, `Get(Slot7).Key == CapsLock`, `Get(Slot8).Key == G`.
  - `ShinobiCoop.KeyBindings.Blocked`: `Rebind(Slot1, {Escape})`, `{F4, Alt}`, `{Tab, Alt}`, `{W}`, `{RightMouseButton}`, `{LeftCommand}`, `{Gamepad_FaceButton_Bottom}` → each Blocked, and `Get(Slot1)` unchanged.
  - `ShinobiCoop.KeyBindings.AltAndReset`: `Rebind(Slot1, {One, true})` → `KeyLabel(Get(Slot1)) == "Alt+1"`; `ResetToDefaults()` → `Get(Slot1) == Default(Slot1)`.
  - `ShinobiCoop.Input.AltBindingsChordAndBlock`: after `Rebind(Slot2, {One, true})`, the `Context` mapping for `IA_Jutsu2` on `One` has a `UInputTriggerChordAction` on `AltModifier`, and the `IA_Jutsu1` mapping on `One` has a `UInputTriggerChordBlocker` on `AltModifier`.
  - `ShinobiCoop.KeyBindings.LiveRemap`: `Rebind(Slot1, {ThumbMouseButton})` → `Context` maps `IA_Jutsu1` to `ThumbMouseButton` and no longer to `One`.
- [ ] **Step 2: Run** `run_tests.ps1 -Filter "ShinobiCoop.KeyBindings+ShinobiCoop.Input"`; expect FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the same filter**; expect PASS.
- [ ] **Step 5: Commit** — `git commit -m "feat: rebindable keys with Alt chords and swap-on-conflict"`

---

### Task 6: 8-slot HUD bar

**Files:**
- Modify: `Shinobi/UI/ShinobiHUDWidget.{h,cpp}` — `NumJutsuSlots = 8`. Delete `PagePips` and `BuildPagePips`. `UpdateJutsuSlots(const UAbilitySystemComponent& ASC, const AShinobiPlayerCharacter& Player)` fills slot i from `Player.GetLoadout()[i]`.
- Modify: `Shinobi/ShinobiPlayerController.{h,cpp}` — `bool IsUsingGamepad() const;`, updated from the last key (`InputKey` override or an `InputKey` delegate): gamepad key → true, keyboard/mouse key → false.
- No automated test: there are no HUD tests in the repo. Verify by screenshot.

**Behaviour:**
- One row of 8, bottom centre, with a gap after slot 4. Icon size about 60% of today's.
- Per slot: icon; chakra cost in the corner; key label under it (`ShinobiKeyBindings::KeyLabel(Get(SlotN), true)` on keyboard, face glyph text "A/B/X/Y" on gamepad); a dark overlay with a countdown while on cooldown (keep today's logic); desaturated when chakra is below cost.
- Remove the name text under icons. Show the name above the bar for the slot being cast.
- Empty slot: frame and key label only.
- Gamepad: the current page's 4 slots get a bright frame and face labels; the other 4 dim to 50% and show no label.
- Locked-by-rules has no HUD state; that lives on the Skills screen.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build** with `Scripts/build.ps1`; expect success.
- [ ] **Step 3: Screenshot check.** Run the existing HUD screenshot path: the jutsu showcase `ShotUI` calls in `ShinobiPlayerController.cpp:849`/`862`/`883`, now after `ShinobiEquipPage`. Open the PNGs in `Saved/Screenshots/WindowsEditor/` and confirm 8 icons, a gap after 4, keys 1 2 3 4 R T G Caps, and no overlap with the unit frame at 1280×720.
- [ ] **Step 4: Commit** — `git commit -m "feat: HUD shows the 8-slot jutsu bar"`

---

### Task 7: Skills screen (K)

**Files:**
- Create: `Shinobi/UI/ShinobiSkillsWidget.{h,cpp}` — `UCLASS() class UShinobiSkillsWidget : public UUserWidget`, built in `RebuildWidget` like `UShinobiHUDWidget`.
- Modify: `Shinobi/ShinobiPlayerController.{h,cpp}` — `void ToggleSkillsScreen();` (open: `SetInputMode(FInputModeGameAndUI)`, show the cursor, add `IMC` block of gameplay — reuse how `TogglePauseMenu` does it at cpp:133-157; close on K or Esc). Esc closes the Skills screen before it opens the pause menu.
- Modify: `Shinobi/UI/ShinobiMenuWidgets.{h,cpp}` — pause entries become `Resume, Jutsu, Invite Friend, Settings, Leave`; "Jutsu" closes the pause menu and calls `ToggleSkillsScreen()`.
- Modify: each jutsu `GA_*.cpp` — fill in `Description` (one line each, 19 total, in the game's tone).

**Behaviour** (spec "Skills screen"):
- Title "JUTSU". A pool grid of all granted jutsus, 7 per row; slotted ones get a purple frame. Below it, the 8 slots in bar order with key labels and a 4|4 gap.
- Click a jutsu, then click a slot → `RequestLoadout(Place(...))`. Drag a jutsu onto a slot does the same. Drag slot → slot → `Swap`. Right-click a slot, or drag a slot onto the pool → `Clear`.
- A slot where `IsSlotLocked(i)` is true shows a lock and its cooldown seconds, and ignores input.
- `GetLoadoutLockReason()` not empty → a banner: "In combat: swaps unlock in Ns" (`GetCombatSecondsLeft`), or the reason text. All edits are ignored while the banner shows.
- Hover tooltip: name, "Chakra N · Cooldown Ns", `Description`.
- Gamepad: focusable buttons. D-pad/stick moves focus, A picks a jutsu then a slot, B cancels the pick or closes.
- Refresh on tick (cheap: 27 widgets) so the replicated `Loadout` and cooldowns show live.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build**; expect success.
- [ ] **Step 3: Run** `run_tests.ps1 -Filter ShinobiCoop.Loadout` (the request path) and expect PASS. Then play (`Scripts/play.ps1`): press K, put Water Dragon into slot 3, close, press 3 → Water Dragon casts. Hit an enemy and press K → the banner shows and the drop is ignored.
- [ ] **Step 4: Commit** — `git commit -m "feat: K opens the Skills screen to equip jutsus"`

---

### Task 8: Shortcuts screen

**Files:**
- Create: `Shinobi/UI/ShinobiShortcutsWidget.{h,cpp}` — `UShinobiShortcutsWidget : UUserWidget` with `TWeakObjectPtr<UUserWidget> Opener` (same pattern as `UShinobiSettingsWidget`).
- Modify: `Shinobi/UI/ShinobiMenuWidgets.{h,cpp}` — Settings gets a "Shortcuts" entry before "Back" that opens the widget.

**Behaviour** (spec "Shortcuts screen"):
- Title "SHORTCUTS".
- Left column "JUTSU SLOTS": Slot 1-8, each with the slotted jutsu name in grey (main menu: from `ShinobiLoadout::LoadSaved`), then Skills screen.
- Right column "ACTIONS": Melee … Map.
- Clicking a key chip shows "Press a key…" on it. The widget overrides `NativeOnKeyDown` and `NativeOnMouseButtonDown`, reads Alt from `InKeyEvent.IsAltDown()`, and calls `Rebind`.
  - Esc cancels.
  - Gamepad keys are ignored.
  - Blocked → a red line "That key can't be used".
  - Swapped → a gold line "<Key> was on <Label>: <Label> moved to <old key>".
- "Reset to defaults" calls `ResetToDefaults`. "Back" returns to the opener.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build**; expect success.
- [ ] **Step 3: Run** `run_tests.ps1 -Filter ShinobiCoop.KeyBindings`; expect PASS. Then play:
  - Settings → Shortcuts. Rebind Slot 8 to G → the swap line shows.
  - In game, G fires slot 8 and Caps fires slot 7.
  - Bind Slot 1 to Alt+1 → Alt+1 fires slot 1, and plain 1 does nothing.
  - Reset restores the defaults.
- [ ] **Step 4: Commit** — `git commit -m "feat: Shortcuts screen rebinds slot and action keys"`

---

### Task 9: Full verification

- [ ] **Step 1:** `grep -rn "JutsuPage\|NumJutsuPages\|PageHasJutsu\|ShinobiPage(" Source/` → no matches (except `ShinobiEquipPage`).
- [ ] **Step 2:** Full suite: `powershell -File Scripts/run_tests.ps1` → all PASS. Report any failure verbatim.
- [ ] **Step 3:** Two-player check via `Scripts/smoke_m4.ps1` or a listen-server PIE with 2 clients:
  - The client swaps a jutsu out of combat → its key fires the new jutsu on the host's view.
  - The client swaps while in combat → nothing changes.
  - In PvP (`Scripts/smoke_pvp.ps1`), a swap during `Fighting` is refused and a swap during the countdown works.
- [ ] **Step 4:** Update memory with a pointer for this feature (branch, key facts), following the existing memory files' style.
