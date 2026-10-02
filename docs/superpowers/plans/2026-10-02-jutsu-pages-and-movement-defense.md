# Jutsu Pages and Movement/Defense Jutsu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** add a page system so keys 1-4 reach more than four jutsu, and five new jutsu: Teleport (Flying Raijin), Kamui, Almighty Push, Earth Wall, Water Wall.

**Architecture:**
- A page is a loose gameplay tag `State.JutsuPage.N` on the player's ASC. `UShinobiJutsuAbility::CanActivateAbility` refuses an input-bound jutsu whose `Page` tag is absent (enemy specs have no input, so they ignore pages). Keys stay `Jutsu1..4`.
- New jutsu are `UShinobiJutsuAbility` subclasses (seal, charge, release, cost, cooldown, interrupt come free). Two things that must happen *after* a jutsu ends (teleporting to a kunai, leaving Kamui early) are tiny separate abilities gated by a tag, so the jutsu itself never has to stay active.
- Kamui is a timed effect granting `State.Phased`; the damage exec, collision, AI targeting and projectile overlap all read that tag. Walls are one replicated actor class, `AShinobiBarrier`, with per-kind numbers.

**Tech Stack:** UE 5.8 C++ with GAS and Enhanced Input, UE Python editor scripts, Blender 5.2 Python (`blender -b -P`), UE automation tests (`Scripts/run_tests.ps1`), SpriteCook MCP, Hyper3D Rodin MCP.

**Spec:** `docs/superpowers/specs/2026-10-02-jutsu-pages-and-movement-defense-design.md`

## Global Constraints

- Branch `feature/jutsu-pages`, cut from the current branch (`feature/london`). The working tree already holds the user's unrelated uncommitted work. **Never `git add -A` or `git add .`; add explicit paths.** Several files this plan edits already carry that work (`ShinobiHUDWidget.*`, `ShinobiPlayerController.*`, `ShinobiBurst.*`, `ShinobiVfx.*`, `ShinobiWorldGameMode.*`). Do not commit those files; list them to the user at the end of each task instead. Commit steps below name only files that were clean.
- Build with `powershell -File Scripts/build.ps1` (close the Unreal Editor first). Run tests with `powershell -File Scripts/run_tests.ps1 -Filter "<prefix>"`; it must exit 0. UE Python scripts also need the GUI editor closed.
- Pages: `ShinobiTags::NumJutsuPages = 5`. Player starts on page 1. Page map: P1 existing four; P2 Teleport (slot 1), Kamui (2), Almighty Push (3), Earth Wall (4); P3 Water Wall (slot 1).
- Numbers (chakra / cooldown / seal `WindUp`):

| Jutsu | Chakra | Cooldown | WindUp | Other |
|---|---|---|---|---|
| Teleport | 25 | 10 | 0.4 | 3 kunai, ±12° fan, speed 4000, stick range 2500, window 6 s, pick angle 15°, arrival damage 20 within 200 |
| Kamui | 30 | 12 | 0.3 | phase 3 s |
| Almighty Push | 30 | 8 | 0.5 | `ReleaseDelay` 0.15, radius 600, damage 25, launch 1500 horizontal + 400 up |
| Earth Wall | 35 | 14 | 0.5 | HP 400, life 10 s |
| Water Wall | 25 | 9 | 0.4 | HP 250, life 6 s |

- Walls: box 450 wide × 60 thick × 300 tall, centred 350 uu in front of the caster at their feet, facing their yaw. One wall per caster; a new one destroys the old.
- VFX: mesh/material and existing SpriteCook sprites only (`ShinobiVfx::ESprite`), no Niagara. Missing assets must never break gameplay: fall back to the engine cube/cone and `MakeGlow` tints.
- Match surrounding code: comment density, `ponytail:` notes on shortcuts, one-line `UPROPERTY` docs. New automation tests live in `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp` (created in Task 1; include `Tests/ShinobiTestWorld.h`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`; use `ShinobiTestFlags`).

## Review Focus

1. **Enemy jutsu must ignore pages.** `UGA_EnemyFireball` derives from `UGA_Fireball` and its spec has no input; a page check that required the tag would silence every enemy fireball. Task 1.
2. **A client sends an out-of-range page** (`ServerSetJutsuPage(9)`): ignored, server page unchanged. Task 1.
3. **Teleport thrown at open sky:** kunai never stick; the marked window must not teleport the caster anywhere or leave an active ability. Task 7.
4. **Kamui ends while the caster overlaps an enemy:** caster is nudged out, not stuck inside it. Task 6.
5. **Second wall while the first lives, and two casters:** the caster's own old wall breaks; another player's wall stays. Task 4.

---

### Task 1: Tags and the page model

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiTags.h`, `ShinobiTags.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h`, `ShinobiJutsuAbility.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `ShinobiPlayerCharacter.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces (`ShinobiTags`): `constexpr int32 NumJutsuPages = 5;` `SHINOBICOOP_API const FGameplayTag& JutsuPage(int32 Page);` (clamps to 1..5); declared tags `State_JutsuPage_1..5`, `State_Phased`, `State_TeleportMarked`, `Cooldown_Jutsu_Teleport`, `Cooldown_Jutsu_Kamui`, `Cooldown_Jutsu_AlmightyPush`, `Cooldown_Jutsu_EarthWall`, `Cooldown_Jutsu_WaterWall` (strings follow the existing pattern, e.g. `"State.JutsuPage.1"`, `"Cooldown.Jutsu.Teleport"`).
- Produces (`UShinobiJutsuAbility`): `UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "1", ClampMax = "5")) int32 Page = 1;` and an override of `CanActivateAbility` that additionally returns false when the spec has `InputID != INDEX_NONE` and the owner lacks `ShinobiTags::JutsuPage(Page)`.
- Produces (`AShinobiPlayerCharacter`): `int32 GetJutsuPage() const;` `void SetJutsuPage(int32 Page);` (owner: apply locally, then `ServerSetJutsuPage` unless authority) `void ApplyJutsuPage(int32 Page);` (local tags only: removes the current page tag, adds `JutsuPage(Page)`, no-op if out of 1..5) `bool IsCastingJutsu() const;` `bool PageHasJutsu(int32 Page) const;` (any `StartupAbilities` jutsu CDO with that `Page`) `bool TryCycleJutsuPage(int32 Direction);` `UFUNCTION(Server, Reliable) void ServerSetJutsuPage(uint8 Page);` `UFUNCTION(Client, Reliable) void ClientSetJutsuPage(uint8 Page);`

- [ ] **Step 1: Write the failing tests** in the new test file:
  - `ShinobiCoop.JutsuPages.OffPageJutsuDoesNotFire`: spawn a player; `TestTrue` it has `JutsuPage(1)`. `ApplyJutsuPage(2)`; `TestFalse` it still has `JutsuPage(1)`; `Tap(P, Jutsu1)`; `TestFalse` `IsActive(P, UGA_Fireball::StaticClass())`; `TestEqual` chakra `100.f`. `ApplyJutsuPage(1)`; `Tap`; `TestTrue` active; chakra `80.f`.
  - `ShinobiCoop.JutsuPages.EnemyJutsuIgnoresPages`: spawn a plain `AShinobiCharacterBase` (enemy team, no page tag); `ASC->GiveAbility(FGameplayAbilitySpec(UGA_EnemyFireball::StaticClass(), 1, INDEX_NONE))`; `TestTrue(ASC->TryActivateAbilityByClass(UGA_EnemyFireball::StaticClass()))`.
  - `ShinobiCoop.JutsuPages.ServerRejectsOutOfRangePage`: player calls `ServerSetJutsuPage_Implementation(9)` directly; `TestEqual(P->GetJutsuPage(), 1)`; then `(3)` → `3`.
- [ ] **Step 2: Build and run** `-Filter "ShinobiCoop.JutsuPages"`. Expected: build error (symbols missing), i.e. the tests cannot pass yet.
- [ ] **Step 3: Declare the tags and `JutsuPage()`** in `ShinobiTags.h/.cpp` (a static array of the five tags behind `JutsuPage`).
- [ ] **Step 4: Add `Page` and the `CanActivateAbility` override** to `UShinobiJutsuAbility`; read the spec with `ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)`.
- [ ] **Step 5: Implement the page API** on `AShinobiPlayerCharacter`. `BeginPlay` (all machines) adds `JutsuPage(1)` as a loose tag. `ServerSetJutsuPage_Implementation`: ignore values outside 1..5; if `IsCastingJutsu()` reply `ClientSetJutsuPage(current)`; else `ApplyJutsuPage`. `IsCastingJutsu` loops `AbilitySystem->GetActivatableAbilities()` for an active `UShinobiJutsuAbility` spec. `TryCycleJutsuPage`: false if casting; steps through pages in `Direction`, skipping pages where `!PageHasJutsu`, wraps; false if no other page is populated; otherwise `SetJutsuPage(next)` and true.
- [ ] **Step 6: Build and run** the filter. Expected: the three tests pass; also run `-Filter "ShinobiCoop.Jutsu"` and expect the existing jutsu tests still pass.
- [ ] **Step 7: Commit** the clean files: `git add Source/ShinobiCoop/Shinobi/ShinobiTags.* Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.* Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.* Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp` then `git commit -m "Jutsu pages: page tags, page-gated activation, player page API"` (with the attribution line from the session).

### Task 2: Page input and HUD

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h`, `ShinobiInputConfig.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp` (bindings)
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`, `ShinobiHUDWidget.cpp` (user WIP present: do not commit)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, `ShinobiPlayerController.cpp` (user WIP present: do not commit)
- Test: `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`

**Interfaces:**
- Consumes: `AShinobiPlayerCharacter::TryCycleJutsuPage`, `SetJutsuPage`, `GetJutsuPage` (Task 1).
- Produces: `UShinobiInputConfig::JutsuPageNext`, `JutsuPagePrev` (`TObjectPtr<UInputAction>`, boolean); `UFUNCTION(Exec) void AShinobiPlayerController::ShinobiPage(int32 Page)` (calls `SetJutsuPage` on the pawn; used by scripts).

- [ ] **Step 1: Failing test** in `FShinobiInputMappingsTest`: `TestNotNull` mappings: `Q` → `JutsuPageNext` and `MouseScrollUp` → `JutsuPageNext` in `Context`; `MouseScrollDown` → `JutsuPagePrev` in `Context`; `Gamepad_DPad_Right` → Next and `Gamepad_DPad_Left` → Prev in `JutsuContext`.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Input"`; expect a compile failure (members missing).
- [ ] **Step 3: Add the two actions** in `UShinobiInputConfig::Build` next to the jutsu actions, with the mappings above. Bind both in `SetupPlayerInputComponent` (`ETriggerEvent::Started`) to lambdas or small methods calling `TryCycleJutsuPage(+1/-1)`.
- [ ] **Step 4: HUD slots follow the page.** In `UpdateJutsuSlots`, skip any jutsu whose `Page` differs from the pawn's `GetJutsuPage()`; slots with no jutsu on the page collapse (`SlotFrames[i]` and `SlotNames[i]` collapsed, restored when filled), and `SlotJutsu[i]` is cleared so the icon refreshes when the page changes back. In `OnAbilityFailed`, ignore failures of jutsu not on the current page, and ignore all failures while the pawn has `State_Phased` or `State_TeleportMarked` (those re-presses are expected to fail the jutsu itself).
- [ ] **Step 5: Page pips.** Add a second `UHorizontalBox` ("PagePips") anchored bottom-centre 12 px below the slot row, one small `UBorder` diamond per page that `PageHasJutsu`; the current page uses `PipOnColor`, others `PipOffColor`. Update each tick in `NativeTick`. Add the `ShinobiPage` exec.
- [ ] **Step 6: Build; run** `-Filter "ShinobiCoop.Input"` and `-Filter "ShinobiCoop.Jutsu"`. Expected: pass. HUD is checked visually in Task 12.
- [ ] **Step 7: Commit** `ShinobiInputConfig.*`, `ShinobiPlayerCharacter.cpp`, `Tests/ShinobiInputTests.cpp` only. Tell the user the HUD and controller edits are left uncommitted because of their WIP.

### Task 3: Burst kinds for the new jutsu

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiBurst.h`, `ShinobiBurst.cpp` (user WIP present: do not commit)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces: `EShinobiBurst::Shockwave, Swirl, TeleportFlash, Crumble, Splash` (append after `BlackFlameIgnite`). Shockwave: push ring and dust. Swirl: Kamui in/out. TeleportFlash: arrival flash. Crumble: earth wall breaking. Splash: water wall breaking.

- [ ] **Step 1: Failing test** `ShinobiCoop.Burst.NewKindsSpawnAndReplicateKind`: for each new kind, `AShinobiBurst::Spawn(W.World, Kind, FVector::ZeroVector, 1.5f)` returns non-null, `GetKind()` equals the kind, `GetScale()` is `1.5f`.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Burst"`; expect compile failure.
- [ ] **Step 3: Add the enum values and a row for each in the properties `switch`** (`Color`, `Duration`, `MaxScale`, `Intensity`, `LightIntensity`) using the glow-sphere look: Shockwave smoke-white 0.5 s; Swirl chakra blue 0.5 s; TeleportFlash lightning yellow-white 0.3 s; Crumble brown 0.5 s; Splash chakra blue 0.5 s. Cards come in Task 9; `BuildCards()` returns false for these until then.
- [ ] **Step 4: Build; run** the filter and `-Filter "ShinobiCoop.Jutsu"`. Expected: pass.
- [ ] **Step 5:** nothing to commit (file carries user WIP); note it for the user.

### Task 4: Barrier actor, damage interface, Earth and Water Wall

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/ShinobiDamageable.h`, `Source/ShinobiCoop/Shinobi/ShinobiBarrier.h`, `ShinobiBarrier.cpp`
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_EarthWall.h/.cpp`, `GA_WaterWall.h/.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiProjectile.cpp` (`OnOverlap`)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp` (`StartupAbilities`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces (`IShinobiDamageable`, a `UINTERFACE`): `virtual void ReceiveShinobiDamage(float Amount, const AActor* Source) = 0;` `virtual EShinobiTeam GetDamageableTeam() const = 0;`
- Produces (`AShinobiBarrier : AActor, IShinobiDamageable`): `enum class EShinobiBarrierKind : uint8 { Earth, Water }`; `static AShinobiBarrier* Spawn(UWorld* World, const FTransform& BaseTransform, AShinobiCharacterBase* Caster, EShinobiBarrierKind Kind);` (server; `BaseTransform` is the wall's ground-level centre; destroys the caster's existing barrier first) `float GetHealth() const;` `EShinobiBarrierKind GetKind() const;` Replicated initial-only `Kind` and `Team`. A `UBoxComponent` (extent 225 × 30 × 150, centre raised 150): object type `ECC_WorldStatic`, blocks all, overlaps `ECC_WorldDynamic` (projectiles). Server-only `Health` from the table (Earth 400, Water 250), lifespan (10 / 6 s) via `SetLifeSpan`. Look: cube mesh scaled to the box, `MakeGlow` tint fallback (earth brown, water blue); rises from 300 below over 0.4 s locally in `Tick` (cosmetic).
- Produces (`UGA_EarthWall`, `UGA_WaterWall` : `UShinobiJutsuAbility`): constructor values from Global Constraints; Earth `InputID` `Jutsu4`, `Page` 2, `Cooldown_Jutsu_EarthWall`; Water `Jutsu1`, `Page` 3, `Cooldown_Jutsu_WaterWall`. `ExecuteJutsu` (authority): `FaceAim(nullptr)`, spawn the barrier 350 uu ahead at the caster's feet, `EndJutsu()`.
- Consumes: Task 1 tags; Task 3 bursts are used in Task 9, not here.

- [ ] **Step 1: Failing tests:**
  - `ShinobiCoop.Barrier.EnemyProjectileDamagesAndStopsAtWall`: player P at origin; `AShinobiBarrier::Spawn` Earth at `(300,0,-100)`; enemy E (plain character) at `(900,0,0)` hovering fires an `AShinobiKunai` (instigator E, `DirectDamage 30`) toward -X from `(860,0,40)`; after `W.Tick(0.6f)`: kunai gone, `GetHealth()` is `370.f`, P health `500.f`.
  - `ShinobiCoop.Barrier.FriendlyProjectileFliesThrough`: same wall; P fires the usual `AShinobiProjectile` (`DirectDamage 60`) along +X at a target at `(600,0,0)`: target health `440.f`, wall health `400.f`.
  - `ShinobiCoop.Barrier.BreaksAtZeroAndExpires`: `ReceiveShinobiDamage(400, nullptr)` then `W.Tick(0.1f)` → wall invalid; a fresh Earth wall is invalid after `Tick(10.3f)`, a Water wall after `Tick(6.3f)`.
  - `ShinobiCoop.Barrier.NewWallReplacesOnlyOwnersOldWall`: P1 spawns two walls in sequence → one barrier owned by P1; P2's wall survives P1's second cast; total count 2.
  - `ShinobiCoop.Wall.EarthWallCast`: `ApplyJutsuPage(2)`, `Tap(Jutsu4)`, `Tick(0.9f)` → one barrier at X ≈ 350 (±30), Earth, health `400.f`, chakra `65.f`, `Cooldown_Jutsu_EarthWall` present.
  - `ShinobiCoop.Wall.WaterWallCastOnPage3`: `ApplyJutsuPage(3)`, `Tap(Jutsu1)`, `Tick(0.8f)` → Water barrier health `250.f`, chakra `75.f`.
  - `ShinobiCoop.JutsuPages.CycleSkipsEmptyAndBlocksWhileCasting`: `TryCycleJutsuPage(+1)` from 1 → page 2; again → 3; again → wraps to 1 (pages 4, 5 empty). With page 2 active, `Tap(Jutsu4)` then `TryCycleJutsuPage(+1)` returns false and page stays 2.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Barrier"`; expect compile failure.
- [ ] **Step 3: Implement the interface, `AShinobiBarrier`, and the two abilities;** add both ability classes to the player's `StartupAbilities`.
- [ ] **Step 4: Projectile hook** in `AShinobiProjectile::OnOverlap`: if `OtherActor` implements `IShinobiDamageable` — authority only, and only when `GetDamageableTeam() != Caster->GetTeam()` — call `ReceiveShinobiDamage(DirectDamage, Caster)` and `Explode(nullptr, <contact point>)`; in every case return before the character path (friendly barriers are flown through).
- [ ] **Step 5: Build; run** `-Filter "ShinobiCoop.Barrier"`, `"ShinobiCoop.Wall"`, `"ShinobiCoop.JutsuPages"`, `"ShinobiCoop.Jutsu"`. Expected: all pass.
- [ ] **Step 6: Commit** `ShinobiDamageable.h`, `ShinobiBarrier.*`, `GA_EarthWall.*`, `GA_WaterWall.*`, `ShinobiProjectile.cpp`, `ShinobiPlayerCharacter.cpp`, the test file. Message: `Jutsu: earth and water walls on a shared barrier actor`.

### Task 5: Almighty Push

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_AlmightyPush.h/.cpp`
- Modify: `ShinobiPlayerCharacter.cpp` (`StartupAbilities`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces: `UGA_AlmightyPush : UShinobiJutsuAbility`, `InputID Jutsu3`, `Page 2`, `Cooldown_Jutsu_AlmightyPush`, `float Radius = 600.f; float Damage = 25.f; float LaunchSpeed = 1500.f; float LaunchUp = 400.f;`. `ExecuteJutsu` (authority): for each `ShinobiCombat::FindHostilesInRadius(Self, Self->GetActorLocation(), Radius * GetVisualScale())` call `ShinobiCombat::ApplyDamage(Source, T, Damage * GetDamageScale())`; if it landed, `ShinobiCombat::ApplyStatus(..., UGE_Stagger)` and `T->LaunchCharacter(Dir2D * LaunchSpeed * GetVisualScale() + FVector(0,0,LaunchUp), true, true)`; spawn `EShinobiBurst::Shockwave` at the caster with `GetVisualScale()`; `EndJutsu()`.

- [ ] **Step 1: Failing test** `ShinobiCoop.Push.LaunchesOnlyHostilesInRadius`: P hovering at origin, enemy E1 at `(300,0,0)`, enemy E2 at `(0,900,0)`, ally player A at `(0,300,0)`. `ApplyJutsuPage(2)`, `Tap(Jutsu3)`, `Tick(0.9f)` then `Tick(0.3f)`. Assert E1 health `475.f` and `E1->GetActorLocation().X > 400.f`; E2 health `500.f` and `Y` still `900.f`; A health `500.f`, A location unchanged; P location unchanged; chakra `70.f`.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Push"`; expect compile failure.
- [ ] **Step 3: Implement** the ability; register it.
- [ ] **Step 4: Build; run** the filter plus `"ShinobiCoop.Jutsu"`. Expected: pass.
- [ ] **Step 5: Commit** `GA_AlmightyPush.*`, `ShinobiPlayerCharacter.cpp`, test file. Message: `Jutsu: Almighty Push`.

### Task 6: Kamui

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h/.cpp` (`UGE_Kamui`)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCombat.cpp` (`IsInvulnerable`)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h/.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Enemies/ShinobiAIController.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiProjectile.cpp` (`OnOverlap`)
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp`, `GA_Melee.cpp` (blocked while phased)
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Kamui.h/.cpp` (holds `UGA_Kamui` and `UGA_KamuiExit`)
- Modify: `ShinobiPlayerCharacter.cpp` (`StartupAbilities`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces: `UGE_Kamui : UShinobiEffect` (`static constexpr float Seconds = 3.f;` `SetDuration`, `GrantTag(State_Phased)`); `bool AShinobiCharacterBase::IsPhased() const;` `bool IsTargetable() const;` (`IsAlive() && !IsPhased()`); `UGA_Kamui : UShinobiJutsuAbility` (`Jutsu2`, page 2, `Cooldown_Jutsu_Kamui`; `ExecuteJutsu` applies `UGE_Kamui` to self on authority, spawns `EShinobiBurst::Swirl`, `EndJutsu()`); `UGA_KamuiExit : UShinobiGameplayAbility` (`InputID Jutsu2`, `NetExecutionPolicy ServerOnly`, `ActivationRequiredTags` = `State_Phased` and `JutsuPage(2)`; activation removes `UGE_Kamui` via `RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(State_Phased))` then ends).
- Rules: `UShinobiDamageExec` returns early (no damage, no event) when the target has `State_Phased`, placed before the periodic/direct split. `ShinobiCombat::IsInvulnerable` also true when phased. Base character `RegisterGameplayTagEvent(State_Phased)` (like the existing dead-tag handler): on gain, remember and set the capsule's `ECC_Pawn` response to `ECR_Ignore`; on loss, restore it, and on authority nudge out of overlap with `World->FindTeleportSpot(this, Location, Rotation)` then `SetActorLocation`. `AShinobiAIController`: both the retarget test and `FindNearestPlayer` use `IsTargetable()`. `AShinobiProjectile::OnOverlap`: ignore a phased character. `UShinobiJutsuAbility` and `UGA_Melee` add `State_Phased` to `ActivationBlockedTags`.

- [ ] **Step 1: Failing tests:**
  - `ShinobiCoop.Kamui.IgnoresAllDamageThenExpires`: `ApplyEffect(P, UGE_Kamui)`. `Damage(P, 50)` → health `500.f`. `ApplySetByCaller(P, UGE_BlackFlame, Data_Damage, 15)` then `Tick(2.5f)` → health `500.f`. With an `FEventCounter` on `Event_Substitution`, `ShinobiCombat::ApplyDamage(E->ASC, P, 50)` returns false and the counter is `0`. After `Tick(1.0f)` (3.5 s total) `HasTag(P, State_Phased)` is false and `Damage(P, 50)` → `450.f`.
  - `ShinobiCoop.Kamui.PassesEnemiesBlocksCastingAndIsNotTargetable`: while phased, `P->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn)` is `ECR_Ignore` and `IsTargetable()` false; `Tap(Jutsu1)` on page 1 and `Tap(Melee)` leave `Fireball` / melee inactive; after expiry the original response is restored.
  - `ShinobiCoop.Kamui.CastThenExitEarly`: `ApplyJutsuPage(2)`, `Tap(Jutsu2)`, `Tick(0.6f)` → phased, chakra `70.f`, cooldown tag present; `Tap(Jutsu2)`, `Tick(0.2f)` → not phased, cooldown tag still present.
  - `ShinobiCoop.Kamui.EndingInsideAnEnemyNudgesOut`: P and enemy E at the same spot; `ApplyEffect(P, UGE_Kamui)`; `Tick(3.3f)`; `FVector::Dist2D(P, E) > 20.f`.
  - `ShinobiCoop.Kamui.ProjectileFliesThroughPhasedPlayer`: enemy kunai along the line to a phased P: P health unchanged and the kunai still flying past.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Kamui"`; expect compile failure.
- [ ] **Step 3: Implement** everything in the Interfaces/Rules block; register `UGA_Kamui` and `UGA_KamuiExit` in `StartupAbilities`.
- [ ] **Step 4: Build; run** `"ShinobiCoop.Kamui"`, `"ShinobiCoop"` (full suite: damage, status, enemy and ability tests exercise `IsInvulnerable`, the exec and the AI). Expected: all pass.
- [ ] **Step 5: Commit** the clean files (all listed above). Message: `Jutsu: Kamui phasing`.

### Task 7: Teleport (Flying Raijin)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h/.cpp` and `ShinobiJutsuAbility.h/.cpp` (move `GetAimViewPoint` to the base as a protected const method; same signature)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h/.cpp` (`UGE_TeleportMarked`)
- Create: `Source/ShinobiCoop/Shinobi/ShinobiMarkerKunai.h/.cpp`
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Teleport.h/.cpp` (holds `UGA_Teleport` and `UGA_TeleportJump`)
- Modify: `ShinobiPlayerCharacter.cpp` (`StartupAbilities`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces: `UGE_TeleportMarked : UShinobiEffect` (`static constexpr float Seconds = 6.f;` duration, `GrantTag(State_TeleportMarked)`).
- Produces (`AShinobiMarkerKunai : AActor`): `static AShinobiMarkerKunai* Spawn(UWorld* World, AShinobiCharacterBase* Owner, const FVector& Start, const FRotator& Direction);` (server; Owner and Instigator = caster) `bool IsStuck() const;` `float GetThrownAt() const;` `FVector GetArrivalPoint(float CapsuleRadius) const;` (stuck location + surface normal × radius). Replicated movement; sphere collision radius 8 that blocks `WorldStatic`/`WorldDynamic` and ignores `Pawn`; `UProjectileMovementComponent` speed 4000, no gravity; `OnProjectileStop` marks stuck (server); an unstuck kunai destroys itself after 0.65 s (range 2500); `InitialLifeSpan` 6; engine cone mesh as the placeholder look.
- Produces (`UGA_Teleport : UShinobiJutsuAbility`): `Jutsu1`, page 2, `Cooldown_Jutsu_Teleport`, `ActivationBlockedTags` adds `State_TeleportMarked`. `ExecuteJutsu` (authority): `FaceAim`, aim point as in `UGA_Fireball` (`TraceAim(2500.f, 10.f, Hit)`), spawn 3 kunai from the chest toward it at yaw offsets -12°, 0°, +12°, `ApplyEffectToSelf(UGE_TeleportMarked)`, `EndJutsu()`.
- Produces (`UGA_TeleportJump : UShinobiGameplayAbility`): `InputID Jutsu1`, `ServerOnly`, `ActivationRequiredTags` = `State_TeleportMarked` and `JutsuPage(2)`. Picks, among stuck kunai owned by the caster, the one whose direction from the view point has the smallest angle to the aim ray if within 15°, else the oldest by `GetThrownAt()`. If none is stuck, ends with no effect. Otherwise: `TeleportTo(Kunai->GetArrivalPoint(radius), FRotator(0, aimYaw, 0))`, `ForceRotationCorrectionFor(1.f)`, `ShinobiCombat::ApplyDamage` 20 plus `ApplyStatus(UGE_Stagger)` to hostiles within 200, `EShinobiBurst::TeleportFlash` at the arrival, destroy the kunai, and when no stuck owned kunai remain remove the marked effect. No cost, no cooldown.

- [ ] **Step 1: Failing tests** (use a `StaticMeshActor` cube wall at `(1500,0,0)`, scaled `(1,20,20)`, blocking all, as the existing jutsu tests do for static geometry):
  - `ShinobiCoop.Teleport.ThrowsThreeKunaiAndMarks`: P hovering at origin, `ApplyJutsuPage(2)`, `Tap(Jutsu1)`, `Tick(0.9f)` → 3 `AShinobiMarkerKunai`, `HasTag(State_TeleportMarked)`, chakra `75.f`, cooldown tag, `GA_Teleport` not active.
  - `ShinobiCoop.Teleport.RepressJumpsToEachKunaiThenEnds`: after `Tick(1.5f)` all three stuck. `Tap(Jutsu1)`, `Tick(0.2f)` → `P.X > 1000`, 2 kunai left, still marked. `Tap`/`Tick` again → 1 kunai. Third → 0 kunai and no `State_TeleportMarked`. Chakra stays `75.f` throughout.
  - `ShinobiCoop.Teleport.ArrivalStrikesNearbyHostiles`: an enemy placed 100 uu from where a kunai sticks loses 20 health at the jump and is staggered.
  - `ShinobiCoop.Teleport.WindowExpiresAndRemovesKunai`: `Tick(6.6f)` after the throw → 0 kunai, no marked tag; `Tap(Jutsu1)` activates neither ability.
  - `ShinobiCoop.Teleport.OpenSkyThrowLeavesNothingToJumpTo` (Review Focus 3): no wall; after `Tick(1.5f)` 0 kunai; `Tap(Jutsu1)` leaves the caster's location unchanged and no ability active.
  - `ShinobiCoop.Teleport.ForeignKunaiAreNotUsed`: P2 throws its fan; P1 gets `UGE_TeleportMarked` via `ApplyEffect` and taps `Jutsu1` on page 2; P1 location unchanged.
- [ ] **Step 2: Run** `-Filter "ShinobiCoop.Teleport"`; expect compile failure.
- [ ] **Step 3: Move `GetAimViewPoint`** into `UShinobiGameplayAbility` (protected, const, unchanged body) and delete it from the jutsu class; the existing jutsu keep calling it through inheritance.
- [ ] **Step 4: Implement** the effect, kunai actor and both abilities; register `UGA_Teleport` and `UGA_TeleportJump` in `StartupAbilities`.
- [ ] **Step 5: Build; run** `"ShinobiCoop.Teleport"` then the full `"ShinobiCoop"` suite. Expected: all pass.
- [ ] **Step 6: Commit** the clean files. Message: `Jutsu: Teleport with marker kunai`.

### Task 8: Kamui ghost look

**Files:**
- Modify: `Scripts/setup_vfx.py` (new `M_Phased`)
- Create: `Source/ShinobiCoop/Shinobi/VFX/ShinobiPhaseFxComponent.h/.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h/.cpp` (component, like `CastFx`)
- Test: `Source/ShinobiCoop/Tests/ShinobiMovementJutsuTests.cpp`

**Interfaces:**
- Produces: `UShinobiPhaseFxComponent : UActorComponent`, created in the base character constructor. Every machine: on `State_Phased` gain, replace all materials of every visible `USkeletalMeshComponent` on the owner with one `M_Phased` dynamic instance (parameter `Opacity`, 0.3; `M_Phased` is translucent, a fresnel rim in `ShinobiVfx::Colors::Chakra`, usage flags for skeletal meshes as `build_body_aura` does); on loss, restore the originals. Outline stencil off while phased (`ShinobiLook`). Spec deviation, say so in the final report: the spec's 5%/25% split assumed human enemies; enemies are AI, so one 0.3 opacity serves the owner and teammates.

- [ ] **Step 1: Add `M_Phased`** to `Scripts/setup_vfx.py` following `build_flame`/`build_body_aura` (same helper functions); run it: `powershell -File Scripts/run_python.ps1 Scripts/setup_vfx.py` (check the script's usage line); expect the log to list `M_Phased` created.
- [ ] **Step 2: Failing test** `ShinobiCoop.Kamui.GhostMaterialSwapsAndRestores`: record `P->GetBodyMesh()->GetMaterial(0)`; `ApplyEffect(P, UGE_Kamui)`, `Tick(0.1f)`; `TestTrue(Cast<UMaterialInstanceDynamic>(...GetMaterial(0)))` and its `Opacity` parameter reads `0.3f`; `Tick(3.2f)`; material 0 equals the recorded original.
- [ ] **Step 3: Run;** expect failure. **Implement** the component; run again; expect pass.
- [ ] **Step 4: Cards for the `Swirl` burst** in `ShinobiBurst.cpp::BuildCards`: a flat spinning `ESprite::Vortex` card (chakra tint, ~500 uu, `Spin` 360) plus a `Burst` card. (User WIP file: do not commit.)
- [ ] **Step 5: Build; full `"ShinobiCoop"` suite.** Expected: pass.
- [ ] **Step 6: Commit** `Scripts/setup_vfx.py`, `ShinobiPhaseFxComponent.*`, `ShinobiCharacterBase.*`, test file. Message: `Kamui: ghost material while phased`.

### Task 9: Push, wall and teleport visuals

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiBurst.cpp` (cards; user WIP, do not commit)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiBarrier.cpp` (real look, break bursts)
- Create: `Scripts/setup_barrier_assets.py`

- [ ] **Step 1: Bursts.** In `BuildCards`: Shockwave = flat `Ring` (white, ~1300 uu × scale) + two `Smoke` cards rising; TeleportFlash = `Burst` card + small yellow-white `Ring`; Crumble = three `Smoke` plus an `Ember` card (brown tint); Splash = blue `Ring` + `Burst`. Same helper calls as the existing cases (`AddCard`).
- [ ] **Step 2: Wall assets.** `Scripts/setup_barrier_assets.py` builds `/Game/Shinobi/Jutsu/MI_Barrier_Earth` (a PBR rock material instance from one Poly Haven rock already in `Scripts/world/library_catalog.json`; pick the rock whose albedo is closest to grey-brown) and `MI_Barrier_Water` (translucent instance of the existing river surface material, found under `Content/Shinobi/World`). `AShinobiBarrier` loads them by soft path, falling back to the Task 4 tint. On destruction by damage, spawn `Crumble` (Earth) or `Splash` (Water); on lifetime expiry spawn the same burst at half scale.
- [ ] **Step 3: Run the script,** then `-Filter "ShinobiCoop.Barrier"`; expect pass (look code must not change behaviour).
- [ ] **Step 4: Visual check:** `powershell -File Scripts/shot_chakra.ps1 -Jutsu` is extended in Task 12; for now open the editor on `Lvl_TrainingGrounds`, `ShinobiPage 2`, `ShinobiCast` 3 and 4, and view the push ring and the earth wall; save a screenshot to `Saved/Screenshots/`.
- [ ] **Step 5: Commit** `ShinobiBarrier.cpp`, `Scripts/setup_barrier_assets.py`. Message: `Walls: rock and water looks, break bursts`.

### Task 10: Marker kunai model

**Files:**
- Create: `Scripts/setup_marker_kunai.py`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMarkerKunai.cpp`, `docs/world/asset-credits.md`

- [ ] **Step 1: Find a free three-prong kunai** (Fab, Sketchfab; CC0/CC-BY or Fab free licence). Download a candidate, render a preview, and **show it to the user for approval before importing**. If none is usable, generate one with Hyper3D Rodin (`mcp__hyper3d__rodin_generate`, prompt "Minato three-pronged Hiraishin kunai, metal blade, wrapped handle with ring pommel, game prop") and ask the user to approve the preview.
- [ ] **Step 2:** `setup_marker_kunai.py` imports it to `/Game/Shinobi/Jutsu/SM_MarkerKunai` (no collision, pivot at the tip, forward = +X) following `Scripts/world/import_library.py`'s import options. `AShinobiMarkerKunai` loads it by soft path and keeps the cone when it is missing.
- [ ] **Step 3: Record the licence** in `docs/world/asset-credits.md` (source URL, author, licence).
- [ ] **Step 4: Run** `"ShinobiCoop.Teleport"`; expect pass. Take an in-game screenshot of a stuck kunai.
- [ ] **Step 5: Commit** the script, `ShinobiMarkerKunai.cpp`, the credits file. Message: `Teleport: Hiraishin kunai model`.

### Task 11: Animations and icons

**Files:**
- Modify: `Scripts/character/build_jutsu_anims.py`, `Scripts/setup_jutsu_anims.py`
- Modify: the new ability constructors (montage paths)
- Create: SpriteCook icon sources and imports as the existing jutsu icons are (see `Scripts/setup_hud.py`)

- [ ] **Step 1: Poses.** Add to the Blender pose library: Teleport (seal into a throwing arm, `Seal` + `Release`), Kamui (hands in the ram seal, a short release), Push (palm thrust, `Release` with the blow at 0.15 s), Walls (palms to the ground slam; one shared set for both walls). Same structure as the existing Fireball poses. Render the preview strips to `Saved/JutsuPreviews/` and **show them to the user for review before importing**.
- [ ] **Step 2: Montages.** Extend `setup_jutsu_anims.py` to import and retarget the new FBX files and build `AM_<Jutsu>_Seal` / `_Release` (names `Teleport`, `Kamui`, `AlmightyPush`, `EarthWall`, `WaterWall`); set the `SealMontage` and `ReleaseMontage` soft paths in each ability constructor, as `UGA_Fireball` does.
- [ ] **Step 3: Icons.** Generate five painted icons with SpriteCook (`generate_game_art`, matching the existing `T_Jutsu_*` style: read two existing icons first for the style and size; prompts in the same wording as the existing entries of `SourceArt/SpriteCook/spritecook-assets.json`; record asset ids and cost there). Import as `/Game/Shinobi/UI/T_Jutsu_<Name>` and set each ability's `Icon`.
- [ ] **Step 4: Run** the full suite; expect pass. Screenshot the HUD on pages 2 and 3.
- [ ] **Step 5: Commit** the scripts, constructors and `spritecook-assets.json`. Message: `Jutsu: poses, montages and icons for the five new jutsu`.

### Task 12: Shot run, docs, regression and co-op check

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp` (`RunJutsuShotTest`, `ShinobiDumpStats`; user WIP, do not commit)
- Modify: `docs/vfx/sprite-vfx-guide.md`

- [ ] **Step 1: Shot test.** After the existing jutsu shots, `RunJutsuShotTest` calls `SetJutsuPage(2)` and casts slots 1-4 in turn (Teleport, then the jump press; Kamui; Push at the dummies; Earth Wall) with a screenshot at each effect, then page 3 for Water Wall and a final shot of the HUD. `ShinobiDumpStats` also prints `Phased=%d` and the page.
- [ ] **Step 2: Run** `powershell -File Scripts/shot_chakra.ps1 -Jutsu`; view the new screenshots; fix anything wrong with the effects (positions, scale).
- [ ] **Step 3: Co-op replication.** Run `powershell -File Scripts/smoke_m4.ps1` (or the two-client launch it wraps) casting Kamui on one client; the second client's `ShinobiDumpStats` shows `Phased=1` for that character, and kunai/wall counts match on both.
- [ ] **Step 4: Docs.** Add a short section to `docs/vfx/sprite-vfx-guide.md`: the new burst kinds and where the barrier look is configured.
- [ ] **Step 5: Final full run:** `powershell -File Scripts/run_tests.ps1`. Expected: exit 0, every test passing.
- [ ] **Step 6: Commit** `docs/vfx/sprite-vfx-guide.md` only. Report to the user which files carry their WIP and were left uncommitted.

---

## Self-review notes

- **Spec coverage:** pages (T1, T2); Teleport (T7, assets T10); Kamui (T6, look T8); Push (T5, look T9); walls (T4, look T9); HUD (T2); assets/icons/anims (T8-T11); tests (every task); risks: replicated page desync handled by `ClientSetJutsuPage` (T1), Kamui stuck (T6), the open-after-release contract change avoided by the separate `UGA_TeleportJump`/`UGA_KamuiExit` abilities.
- **Deviations from the spec, stated:** (1) no base-class "hold open" hook: the follow-up press is a separate gated ability, which leaves the jutsu base untouched; (2) Kamui ghost is one opacity (0.3), not 5%/25%; (3) barriers take damage from projectiles only, not enemy melee (enemy melee is cone-based on characters and cannot reach through a 350 uu wall; add when an enemy needs it); (4) the wall rise animation uses each machine's own clock instead of a replicated start time.
