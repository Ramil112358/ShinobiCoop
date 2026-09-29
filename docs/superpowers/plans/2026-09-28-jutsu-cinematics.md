# Jutsu Cinematics and Charge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** make the four jutsu play as seal → hold-to-charge → release, with Naruto-style animation (Blender-scripted seals and body poses) and effects: mouth fire, lightning arcs, a spinning Rasengan, eye blood and black flames.

**Architecture:**
- `UShinobiJutsuAbility` gets a three-phase timeline. The phase lengths are C++ values; the clips are stretched to fit them. The server measures the charge from GAS's replicated input release and drains chakra while the button is held.
- The charge level travels on spawned actors as an initial-only replicated `Scale`. Persistent visuals follow replicated tags: `State.Casting.*`, the new `State.Casting.Charging`, and `State.Burning.Black`.
- Animations are authored as pose keys in headless Blender on the Mixamo rig, then imported through the existing retarget pipeline, as one montage per phase.

**Tech Stack:** UE 5.8 C++ with GAS, UE Python editor scripts, Blender 5.2 Python (`blender -b -P`), and UE automation tests (`Scripts/run_tests.ps1`).

**Spec:** `docs/superpowers/specs/2026-09-28-jutsu-cinematics-design.md`

## Global Constraints

- Branch: `feature/jutsu-cinematics`, cut from `feature/open-world` at `489118a`. Don't commit the user's unrelated `Config/DefaultEngine.ini` change.
- Charge constants live on `UShinobiJutsuAbility`:
  - `MaxChargeTime = 2.0`
  - `ChargeDrainFraction = 0.5` (of `ChakraCost`, over a full charge)
  - damage scale `1 + 0.75·L`
  - visual scale `1 + 0.8·L`
  - interrupt refund stays `0.5 × ChakraCost`, and drained chakra is not refunded
- Seal lengths (the existing `WindUp` field, which now means the seal phase):
  - Fireball `0.6`, plus `ReleaseDelay 0.1`
  - Chidori `0.5`
  - Rasengan `0.4`
  - Amaterasu `0.4`
  - `UGA_EnemyFireball` keeps `0.8`
- Rasengan dash: `DashDistance 900`, `DashSpeed 4000`, `SnapHalfAngle 15`, `StopShort 100`, `StrikeRange 200`. Keep `Damage 150`, `Knockback 1500`, and the Firestorm values.
- VFX: mesh and material only. No Niagara. All materials are built by `Scripts/setup_vfx.py`. Close the GUI editor before running any UE Python script.
- Montage asset paths: `/Game/Shinobi/Anims/AM_<Jutsu>_Seal`, `AM_<Jutsu>_Charge`, `AM_<Jutsu>_Release`. Missing assets must never break gameplay: the ability simply skips playback.
- Match the surrounding code: comment density, `ponytail:` notes on deliberate shortcuts, and the one-line `UPROPERTY` docs.
- Run the full suite with `powershell -File Scripts/run_tests.ps1` (default filter `ShinobiCoop`). It must end with every test passing.

## Review Focus

1. **Release sent before activation finishes** (a tap, or `ShinobiCast`): the jutsu must still fire uncharged after the seals. It must not hang in the charge phase forever. Task 1: `ChargeTapFiresUnchargedAfterSeals`.
2. **Chakra runs out mid-charge:** auto-release at the level reached, and chakra never goes negative. Task 1: `ChargeAutoReleasesWhenChakraRunsOut`.
3. **Enemy or AI owner with `InputID None`:** there's no release event ever, so it must fire uncharged, not charge for 2 s. Task 1: `EnemyJutsuFiresUncharged`.
4. **Chidori or Rasengan dash with no target:** it must dash the full distance and end cleanly (no dangling tag or orb). Task 3: `RasenganWithoutTargetDashesFullDistance`.
5. **The black-flame target dies or the tag is removed:** the flame cluster must hide, with no components left ticking. Task 6: `BlackFlameClusterFollowsTag`.

---

### Task 1: Charge timeline in the jutsu base

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiTags.h`, `.cpp` (`State_Casting_Charging`)
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.cpp`, `GA_Chidori.cpp`, `GA_Rasengan.cpp`, `GA_Amaterasu.cpp` (new `WindUp` values; the three montage paths replace `CastMontage`)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiProjectile.h`, `.cpp` (`Scale`, needed by this task's tests)
- Test: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

**Interfaces:**
- Produces, on `UShinobiJutsuAbility`:
  - `static constexpr float MaxChargeTime = 2.f;`
  - `static constexpr float ChargeDrainFraction = 0.5f;`
  - `float GetChargeLevel() const` (0–1, frozen at release)
  - `float GetDamageScale() const` (`1 + 0.75f * L`)
  - `float GetVisualScale() const` (`1 + 0.8f * L`)
  - `UPROPERTY float ReleaseDelay = 0.f` (seconds from release to `ExecuteJutsu()`)
  - `TSoftObjectPtr<UAnimMontage> SealMontage, ChargeMontage, ReleaseMontage` (these replace `CastMontage` and `CastReleaseFraction`)
- Produces: `ShinobiTags::State_Casting_Charging` (`"State.Casting.Charging"`).
- Produces, on `AShinobiProjectile`: `UPROPERTY(Replicated) float Scale = 1.f;`, set before `FinishSpawning`, replicated `COND_InitialOnly`. In `BeginPlay` it scales the collision radius, the mesh and the light radius.
- `ExecuteJutsu()` keeps its contract. It now runs `ReleaseDelay` after the release, and subclasses read `GetDamageScale()` and `GetVisualScale()`.

- [ ] **Step 1: Create the branch.** Run `git checkout -b feature/jutsu-cinematics`.

- [ ] **Step 2: Write the failing tests** in `ShinobiJutsuTests.cpp`. Use Fireball on `AShinobiPlayerCharacter` P with a hovering enemy E at `(600,0,0)`, as `FShinobiFireballTest` does. Tolerances: ±1 chakra, ±0.06 scale, ±2 health (the drain is quantised to 0.1 s steps, and timers start one tick late).

```cpp
// ShinobiCoop.Jutsu.ChargeTapFiresUnchargedAfterSeals
Tap(P, Jutsu1); W.Tick(0.5f);
TestEqual("Nothing thrown during the 0.6 s seals", CountActors<AShinobiProjectile>(W), 0);
TestFalse("No charge phase on a tap", HasTag(P, State_Casting_Charging));
W.Tick(0.35f); // t = 0.85 (seal 0.6 + release delay 0.1 + one-tick timer lag)
TestEqual("Thrown after seals", CountActors<AShinobiProjectile>(W), 1);
TestEqual("Uncharged scale", Ball->Scale, 1.f);
TestEqual("No drain on a tap", Get(P, Chakra), 80.f);

// ShinobiCoop.Jutsu.ChargeHoldScalesAndDrains
Press(P, Jutsu1); W.Tick(0.7f);
TestTrue("Charging after seals while held", HasTag(P, State_Casting_Charging));
W.Tick(1.0f); Release(P, Jutsu1);        // ~1.0 s charged -> L ≈ 0.5
TestFalse("Charging tag gone on release", HasTag(P, State_Casting_Charging));
W.Tick(0.2f);
TestNearlyEqual(Ball->Scale, 1.4f);       // 1 + 0.8 * 0.5
TestNearlyEqual(Get(P, Chakra), 75.f);    // 100 - 20 - 0.5*20*0.5
W.Tick(0.4f);
TestNearlyEqual(Get(E, Health), 500.f - 60.f * 1.375f);

// ShinobiCoop.Jutsu.ChargeAutoReleasesAtCap (no enemy in this world)
Press(P, Jutsu1); W.Tick(0.6f + 2.2f);   // never released
TestEqual("Auto-released at the cap", CountActors<AShinobiProjectile>(W), 1);
TestNearlyEqual(Ball->Scale, 1.8f); TestNearlyEqual(Get(P, Chakra), 70.f);

// ShinobiCoop.Jutsu.ChargeAutoReleasesWhenChakraRunsOut
Set(P, Chakra, 22.f); Press(P, Jutsu1); W.Tick(0.6f + 1.0f);  // 2 left after cost; drain 5/s -> empty at ~0.4 s
TestEqual("Released when chakra ran out", CountActors<AShinobiProjectile>(W), 1);
TestTrue("Chakra never negative", Get(P, Chakra) >= 0.f); TestTrue("Partial charge", Ball->Scale > 1.1f && Ball->Scale < 1.3f);

// ShinobiCoop.Jutsu.InterruptDuringChargeRefundsBaseHalf
Press(P, Jutsu1); W.Tick(0.6f + 1.0f); Damage(P, 25.f);
TestFalse("Interrupted", IsActive(P, UGA_Fireball)); TestNearlyEqual(Get(P, Chakra), 85.f); // 75 + 10, drain kept
TestFalse("No cooldown", HasTag(P, Cooldown_Jutsu_Fireball)); W.Tick(0.5f); TestEqual("Nothing thrown", CountActors<AShinobiProjectile>(W), 0);

// ShinobiCoop.Jutsu.EnemyJutsuFiresUncharged
// An AShinobiCharacterBase given UGA_EnemyFireball, activated with TryActivateAbilityByClass (as AShinobiAIController does).
W.Tick(0.8f + 0.1f + 0.15f); TestEqual("Thrown after 0.8 s seals", CountActors<AShinobiProjectile>(W), 1); TestEqual("Uncharged", Ball->Scale, 1.f);
```

- [ ] **Step 3: Update the existing tests to the new seal lengths.** Change only the tick values and messages; the assertions stay the same.
  - `FireballCostWindUpCooldown`: 0.4 → 0.6 + 0.1.
  - `StaggerInterruptsWindUpWithHalfRefund` and `VfxFollowCastingStatusAndImpact`: shift the ticks by +0.3.
  - The Chidori tests: 0.6 → 0.5.
  - Amaterasu: 0.5 → 0.4.
  - Rasengan tests: shift their ticks to the 0.4 s seal. They are rewritten for the dash in Task 3.

- [ ] **Step 4: Build and confirm the new tests fail.** Run `powershell -File Scripts/build.ps1`, then `powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop.Jutsu`. Expected: the six new tests FAIL (no `Scale`, no charge).

- [ ] **Step 5: Implement the timeline in `ShinobiJutsuAbility.cpp`.**

**Activation** keeps the cost, `FaceAim` and interrupt logic. It starts `UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ true)` and sets `bReleaseRequested = true` when it fires. When `GetCurrentAbilitySpec()->InputID` is `None`, it sets `bReleaseRequested = true` immediately. It plays `SealMontage` at rate `Clamp(Length / WindUp, 0.5, 2.5)`, then waits `WindUp`.

**Seal end:**
- If `bReleaseRequested`, go to Release.
- Otherwise enter Charge:
  - add `State_Casting_Charging` the same way GAS adds ActivationOwnedTags: `AddLooseGameplayTag` on every machine running the ability, plus `AddMinimalReplicationGameplayTag` on authority;
  - play `ChargeMontage` at rate 1;
  - start `UAbilityTask_Repeat` every 0.1 s.
- On authority, each repeat drains `ChakraCost * ChargeDrainFraction * 0.1 / MaxChargeTime` through `UGE_ChakraGain`. It auto-releases when the charge time reaches `MaxChargeTime`, or when chakra is below one step (drain the remainder, never below 0).
- A release request during Charge also goes to Release.

**Release:**
- freeze `ChargeTime` into `ChargeLevel = Clamp(ChargeTime / MaxChargeTime, 0, 1)`;
- remove the Charging tag symmetrically;
- end the interrupt task, `CommitAbilityCooldown`, play `ReleaseMontage`;
- after `ReleaseDelay`, call `ExecuteJutsu()` (directly when the delay is 0).

**Also:**
- Reset `ChargeTime`, `ChargeLevel` and `bReleaseRequested` at activation (the ability is instanced per actor).
- Make sure `EndAbility` and the interrupt remove the Charging tag if it's still present. Override `EndAbility` and check a `bChargingTagAdded` flag.
- Charge time counts server-side from the Charge entry to the release event's arrival. On the owning client it counts locally, which is only used for visuals.

- [ ] **Step 6: Add `Scale` to `AShinobiProjectile`** (Interfaces above). In `GA_Fireball::ExecuteJutsu`:
  - set `Ball->Scale = GetVisualScale()`;
  - set `DirectDamage * GetDamageScale()`, `SplashDamage * GetDamageScale()` and `SplashRadius * GetVisualScale()`;
  - set `StatusDamagePerTick * GetDamageScale()`.

- [ ] **Step 7: Run the tests.** Run `powershell -File Scripts/run_tests.ps1`. Expected: all pass except the Rasengan tests, if Step 3 left them for Task 3.

- [ ] **Step 8: Commit.** Run `git add` on the files above, then `git commit -m "Jutsu: seal, hold-to-charge and release phases"`.

---

### Task 2: Charge scaling for Chidori and Amaterasu, and scaled bursts

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiBurst.h`, `.cpp`
- Modify: `GA_Chidori.cpp`, `GA_Amaterasu.cpp`, `GA_Fireball.cpp`, `ShinobiProjectile.cpp` (it passes `Scale` to its impact burst)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h`, `.cpp` (`GetHeadBone`)
- Test: `ShinobiJutsuTests.cpp`

**Interfaces:**
- Produces:
  - `static AShinobiBurst* AShinobiBurst::Spawn(UWorld*, EShinobiBurst Kind, const FVector& Location, float Scale = 1.f, const FRotator& Rotation = FRotator::ZeroRotator);`
  - `UPROPERTY(Replicated) float Scale` (`COND_InitialOnly`), which multiplies `MaxScale` and the light radius.
- New `EShinobiBurst` kinds:
  - `FireBreath`: a cone mesh (`/Engine/BasicShapes/Cone`) along the rotation's forward, 0.3 s, `M_Flame` once Task 4 exists (use `MakeGlow` fire until then);
  - `BlackFlameIgnite`: a dark sphere burst, `BlackFlame` colour, 0.5 s.
- Produces: `FName AShinobiCharacterBase::GetHeadBone() const`, which returns `head` (Manny) or `Head` (Mixamo). Same pattern as `GetRightHandBone`.

- [ ] **Step 1: Write the failing tests.**

```cpp
// ShinobiCoop.Jutsu.ChargedHitsScaleDamage
// Chidori: Press(Jutsu2); Tick(0.5 + 2.0) auto-release at cap -> dash -> enemy at (600,0,0): health == 500 - 120 * 1.75 (±1)
// Amaterasu: Press(Jutsu4) aimed at enemy; Tick(0.4 + 1.0); Release -> Tick(0.1):
//   one AShinobiBurst of kind BlackFlameIgnite with Scale ≈ 1.4; after 1 more second health ≈ 500 - 15 * 1.375 (±1)
// Fireball tap: one AShinobiBurst of kind FireBreath at release, located within 40 units of P's head bone, Scale 1
```

- [ ] **Step 2: Build and run.** Expected: FAIL (the kinds and the `Spawn` overload don't exist).

- [ ] **Step 3: Implement.**
  - Chidori: `Damage * GetDamageScale()`, and the `Lightning` burst at `GetVisualScale()`.
  - Amaterasu: `UGE_BlackFlame::DamagePerTick * GetDamageScale()`, plus a `BlackFlameIgnite` burst at the target, `GetVisualScale()`.
  - Fireball, in this order:
    1. The muzzle is the head bone's world location plus forward × 25 (fall back to the old chest point if the bone is missing).
    2. Spawn a `FireBreath` burst there, rotated along the aim.
    3. Spawn the projectile.
  - The projectile's `Fire` impact burst takes its `Scale`.

- [ ] **Step 4: Run the full suite.** Expected: PASS.

- [ ] **Step 5: Commit.** Run `git commit -m "Jutsu: charge scales Chidori/Amaterasu; breath and ignite bursts"`.

---

### Task 3: Rasengan becomes a dash (shared dash base)

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiDashJutsu.h`, `.cpp`
- Modify: `GA_Chidori.h`, `.cpp` (derive from the base; only the strike remains)
- Modify: `GA_Rasengan.h`, `.cpp`
- Test: `ShinobiJutsuTests.cpp`

**Interfaces:**
- Produces `UCLASS(Abstract) UShinobiDashJutsu : public UShinobiJutsuAbility`:
  - It holds `DashDistance`, `DashSpeed`, `SnapHalfAngle`, `StopShort`, `StrikeRange` and `DashYaw`.
  - `ExecuteJutsu()` and `OnDashFinished()` move here from `GA_Chidori`, unchanged.
  - `OnDashFinished` restores `DashYaw`. On authority it finds the victim (`FindNearestHostileInCone(StrikeRange, 60.f)`) and calls `virtual void Strike(AShinobiCharacterBase* Victim)` if one is found, then `EndJutsu()`.
- `UGA_Chidori::Strike` is today's damage, burst, stun and shock.
- `UGA_Rasengan::Strike` is today's `ExecuteJutsu` body minus the targeting: damage, the `Spiral` burst at `GetVisualScale()`, knockback × `GetDamageScale()`, and Firestorm when the target is burning. Remove `Range` and `HalfAngle`.

- [ ] **Step 1: Rewrite the Rasengan tests.**
  - `RasenganFirestormOnBurningTarget` and `RasenganKeepsBlackFlameAndNeedsBurn`: put the target at `(700,0,0)` and tick `0.4 + 0.4` (the dash takes ~0.15 s).
  - Add:

```cpp
// ShinobiCoop.Jutsu.RasenganDashesToTargetAndKnocksBack
// enemy at (700,0,0); Tap(Jutsu3); Tick(0.8): P within 150 of E (2D), E health 350, E velocity along +X > 1000
// ShinobiCoop.Jutsu.RasenganWithoutTargetDashesFullDistance
// no enemy; Tap(Jutsu3); Tick(0.8): P moved 900 (±60) along +X, ability inactive, no State_Casting_Rasengan tag
```

- [ ] **Step 2: Run them.** Expected: FAIL (Rasengan doesn't move).

- [ ] **Step 3: Implement** the base class and the two subclasses (Interfaces above). The existing Chidori tests must pass unchanged.

- [ ] **Step 4: Run the full suite.** Expected: PASS.

- [ ] **Step 5: Commit.** Run `git commit -m "Rasengan: dash-and-strike on a shared dash-jutsu base"`.

---

### Task 4: Flame and streak materials

**Files:**
- Modify: `Scripts/setup_vfx.py`
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiVfx.h`, `.cpp`

**Interfaces:**
- Produces these assets:
  - `/Game/Shinobi/VFX/M_Flame` (unlit, additive);
  - `/Game/Shinobi/VFX/M_FlameDark` (unlit, translucent);
  - `/Game/Shinobi/VFX/M_Streak` (unlit, opaque).
  - Use the same folder as `M_ChakraBodyAura` if it differs; check the script's constant.
- Flame parameters: `Color` (vector), `Intensity`, `Erode` (0–1, higher = thinner tongues) and `Speed` (upward pan in world units/s). `M_Streak` takes `Color` only.
- Bake `MATUSAGE_SkeletalMesh` and `MATUSAGE_StaticMesh`, as the aura does.
- Produces:
  - `UMaterialInstanceDynamic* ShinobiVfx::MakeFlame(UObject* Outer, const FLinearColor& Color, float Intensity, bool bDark);`
  - `UMaterialInstanceDynamic* ShinobiVfx::MakeStreak(UObject* Outer, const FLinearColor& Color);`
  - both return null with an `ensure` when the asset is missing, like `MakeGlow`.
- New colours: `Colors::BlackFlameCore(0.01, 0.0, 0.015)` and `Colors::Blood(0.35, 0.0, 0.0)`.

- [ ] **Step 1: Build the materials in `setup_vfx.py`.**
  - `M_Flame`: noise sampled at `WorldPosition - (0,0,Time·Speed)`, times a vertical fresnel falloff. Opacity or emissive is masked by `step(Erode, noise)`, softened with a smoothstep.
  - `M_FlameDark`: the same, with emissive = `lerp(Color·Intensity rim, BlackFlameCore, fresnel-inverted)`, and opacity from the erosion.
  - Follow the pin names already documented in the script's docstring.

- [ ] **Step 2: Run the script and check it.** Run `powershell -File Scripts/run_python.ps1 Scripts/setup_vfx.py` with the editor closed. Expected: the log shows the three materials saved and the usage asserts pass.

- [ ] **Step 3: Add `MakeFlame` and `MakeStreak`.** Switch `FireBreath` (Task 2) to `MakeFlame(Fire)`, and `BlackFlameIgnite` to `MakeFlame(BlackFlame, bDark = true)`.

- [ ] **Step 4: Build, then run the full suite.** Expected: PASS.

- [ ] **Step 5: Commit.** Run `git commit -m "VFX: flame, dark flame and streak materials"`.

---

### Task 5: Phase-aware cast effects

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiCastFxComponent.h`, `.cpp`. If the file passes ~450 lines, split the lightning arcs into `ShinobiLightningArcs.h/.cpp` as a plain helper struct owned by the component.
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.cpp` (the fire projectile gets an `M_Flame` shell sphere 1.3× the core)
- Test: `ShinobiJutsuTests.cpp`

**Interfaces:**
- Consumes: `State_Casting_Charging`, `GetHeadBone()`, `GetRightHandBone()`, `MakeFlame`, `MakeStreak` and `MakeGlow`.
- Produces, on `UShinobiCastFxComponent`:
  - `FGameplayTag GetShownTag() const` (kept);
  - `float GetChargeVisualScale() const`, which is `1 + 0.8·min(t/MaxChargeTime, 1)`, where t is the local time since `State_Casting_Charging` appeared (1 when not charging). It freezes when the tag goes away and resets when the casting tag ends.
  - for tests: `bool IsPartVisible(EShinobiCastPart Part) const`, where `enum class EShinobiCastPart : uint8 { HandOrb, RasenganShells, LightningArcs, MouthFlame, EyeGlow, BloodStreaks }`.

**Parts per jutsu** (all hidden when no casting tag is present):

| Jutsu | Parts | Placement and look |
|---|---|---|
| Fireball | MouthFlame only (no HandOrb) | `M_Flame` sphere of 0.12 scale plus an orange light at the head bone plus forward 18 / up 5 (`ponytail:` tune by screenshot). Visible only while Charging, sized × `GetChargeVisualScale()`. |
| Chidori | HandOrb (`Lightning`) plus LightningArcs | 6 arcs. Each arc is 3 thin cylinders (scale 0.02, 0.02, length) chained with random bends, re-randomised every 0.05 s. The arc length is 25–45 units × charge scale. Light intensity: random 0.6–1.5× per frame. |
| Rasengan | HandOrb (`Chakra`, core intensity ×1.5) plus RasenganShells | 2 spheres at 1.25× and 1.45× the core, `MakeGlow` Core 0 (rim), rotating ±720°/s on different axes. Core diameter 25 → 45 units via the charge scale. |
| Amaterasu | EyeGlow plus BloodStreaks (no HandOrb) | Eye dots are 0.02-scale spheres with `MakeGlow(red 1,0.02,0.02; 10)`. Streaks are 0.008 × 0.008 × 0.06 cubes with `MakeStreak(Blood)`, pivoted at the top. Their Z scale grows 0 → 1 over the first 0.4 s of the cast. Positions are the head bone plus per-body offsets: `EyeOffsetManny` / `EyeOffsetHero` (FVector, left eye; mirror Y for the right), tuned in Task 9. |

- [ ] **Step 1: Rewrite `VfxFollowCastingStatusAndImpact` and add a test.**

```cpp
// VfxFollowCastingStatusAndImpact (rewritten)
Press(P, Jutsu1); TestTrue(GetShownTag() == State_Casting_Fireball); TestFalse(IsPartVisible(HandOrb)); TestFalse(IsPartVisible(MouthFlame));
W.Tick(0.7f); TestTrue("Mouth flame while charging", IsPartVisible(MouthFlame));
W.Tick(1.0f); TestTrue("Grows", GetChargeVisualScale() > 1.3f); Release(P, Jutsu1);
W.Tick(0.9f); TestFalse("Gone after the cast", GetShownTag().IsValid()); /* burst + burn aura asserts kept */

// ShinobiCoop.Jutsu.CastFxPartsPerJutsu
Tap(Jutsu2); TestTrue(HandOrb && LightningArcs); TestFalse(RasenganShells);  // then tick until the ability ends
Tap(Jutsu3); TestTrue(HandOrb && RasenganShells); TestFalse(LightningArcs);
Tap(Jutsu4) at enemy; TestTrue(EyeGlow && BloodStreaks); TestFalse(HandOrb);
```

- [ ] **Step 2: Run them.** Expected: FAIL.

- [ ] **Step 3: Implement the component.** Create all part components once in `BeginPlay` (as today), and toggle their visibility in `Refresh`. The tick only runs while any part is visible. Skip everything on a dedicated server, as today.

- [ ] **Step 4: Run the full suite.** Expected: PASS.

- [ ] **Step 5: Commit.** Run `git commit -m "Cast FX: mouth flame, lightning arcs, Rasengan shells, eye blood"`.

---

### Task 6: Black-flame cluster on the target

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/VFX/ShinobiBlackFlameComponent.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h`, `.cpp` (create it next to the aura, with a `GetBlackFlame()` accessor)
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiChakraAuraComponent.cpp` (the black-flame aura row's intensity goes 4 → 1.5, a faint rim)
- Test: `ShinobiJutsuTests.cpp`

**Interfaces:**
- Produces `UShinobiBlackFlameComponent : USceneComponent`, with `bool IsBurning() const` and `int32 NumVisibleTongues() const`.
- It follows `State_Burning_Black` through `RegisterGameplayTagEvent`, following the pattern in `UShinobiCastFxComponent`.
- It creates 10 tongues: `/Engine/BasicShapes/Sphere` scaled (0.12, 0.12, 0.45), each with its own `MakeFlame(BlackFlame, Intensity 3, bDark = true)`.
  - The tongues are attached to the body mesh at spine and limb bones. Pick existing bone names, with the Manny/Mixamo fallback as in `GetRightHandBone`.
  - Each frame each tongue flickers: Z scale × random 0.8–1.25, and a small yaw wobble.
  - One dark point light (`BlackFlame` colour, intensity 400).
- Hidden, with tick off, when the tag is absent or on a dedicated server.

- [ ] **Step 1: Write the failing test.**

```cpp
// ShinobiCoop.Jutsu.BlackFlameClusterFollowsTag
ApplySetByCaller(E, UGE_BlackFlame::StaticClass(), Data_Damage, 15.f); W.Tick(0.1f);
TestTrue(E->GetBlackFlame()->IsBurning()); TestEqual(NumVisibleTongues(), 10);
E->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(State_Burning_Black)); W.Tick(0.1f);
TestFalse(IsBurning()); TestEqual(NumVisibleTongues(), 0); TestFalse(E->GetBlackFlame()->IsComponentTickEnabled());
```

- [ ] **Step 2: Run it.** Expected: FAIL.

- [ ] **Step 3: Implement the component and lower the aura row.** The existing aura test must still pass; update the black-flame intensity assert if it pins 4.

- [ ] **Step 4: Run the full suite.** Expected: PASS.

- [ ] **Step 5: Commit.** Run `git commit -m "VFX: black flame tongues on Amaterasu targets"`.

---

### Task 7: Blender jutsu animation builder, and the user's preview review

**Files:**
- Create: `Scripts/character/build_jutsu_anims.py`
- Output: `SourceArt/Mixamo/Jutsu/<Jutsu>_<Seal|Charge|Release>.fbx` (committed) and `Saved/JutsuPreviews/<Jutsu>_<Phase>.png` (not committed)

**Interfaces:**
- Run with `"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Scripts/character/build_jutsu_anims.py -- [--only <Jutsu>] [--preview-only]`.
- Produces 12 FBX files: the 4 jutsu × 3 phases, 30 fps, armature only, bone names identical to `Power_Up_30fps.fbx` (`mixamorig:*`).
- Phase lengths (the clips are rate-fitted in-engine, so these are natural lengths):
  - Seal: ~0.8–1.0 s
  - Charge: exactly 2.2 s
  - Release: ~0.6–0.8 s

- [ ] **Step 1: Round-trip spike (throwaway check).**
  1. Import `SourceArt/Mixamo/Power_Up_30fps.fbx` with the options matching how `setup_chakra_pose.py`'s source was made.
  2. Key the rest pose for 10 frames and export it with `add_leaf_bones=False`, `bake_anim=True`, `armature_nodetype='NULL'`, and the same axis/scale settings as the import.
  3. Import that onto the Power_Up skeleton with a temporary one-clip entry in `setup_jutsu_anims.py`.
  4. Confirm the retargeted Manny stands upright with no 100× scale error. Check with an editor thumbnail, or with `shot_chakra.ps1` using any montage.
  5. Record the working export settings in the script's docstring. If it fails, fix the settings before any pose work.

- [ ] **Step 2: Build the pose library.** Add `POSES: dict[str, dict[str, tuple[float, float, float]]]` (bone → local XYZ Euler, in degrees, applied on top of the rest pose). It covers:
  - the seven seals: Tiger, Ram, Snake, Horse, Ox, Monkey, Rabbit. Hands meet at the chest, and the finger bones `mixamorig:{Left,Right}Hand{Thumb,Index,Middle,Ring,Pinky}{1,2,3}` shape each seal (for example, Tiger has index and middle up and together, the other fingers interlaced);
  - the body poses `lean_back_inhale`, `blow_forward`, `chidori_hold`, `rasengan_hold`, `eye_focus` and `dash_strike`.
  - Seal poses carry only arms and fingers, so a body pose and a seal can be combined per key.

- [ ] **Step 3: Build the timelines.** `TIMELINES[jutsu][phase] = [(time_s, [pose names], easing)]`:

| Jutsu | Seal | Charge (2.2 s) | Release |
|---|---|---|---|
| Fireball | Snake → Ram → Tiger over ~0.8 s, blending into `lean_back_inhale` | `lean_back_inhale` + Tiger, slow chest rise | `blow_forward` in ~0.15 s, hold, then recover to neutral |
| Chidori | Ox → Rabbit → Monkey | `chidori_hold` with a slow crouch deepening and a hand tremble | `dash_strike` (right arm thrust) |
| Rasengan | quick Ram → `rasengan_hold` | `rasengan_hold`, the left hand circling above the palm | `dash_strike` with the palm forward |
| Amaterasu | Tiger → `eye_focus` | `eye_focus` with the head tilting slightly down and a still stare | a sharp head snap toward the target, then relax |

- [ ] **Step 4: Render the previews.** Use Workbench (as `prep_mesh.py` does), with the Power_Up mesh visible. For each phase, render 4 evenly spaced frames from the front and 4 from the side, composited into one PNG strip per phase.

- [ ] **Step 5: Run it for all four jutsu.** Expected: 12 FBX files and 12 PNG files, with no Python errors. Open the PNGs yourself. Fix any obvious breaks (a hand through the body, a mangled finger, an inverted bend) and re-run until they look clean.

- [ ] **Step 6: USER CHECKPOINT.** Show the user the 12 preview strips and ask for approval per jutsu. Apply the requested pose changes and re-render. Don't start Task 8 before the user approves.

- [ ] **Step 7: Commit.** Run `git add Scripts/character/build_jutsu_anims.py SourceArt/Mixamo/Jutsu/*.fbx`, then `git commit -m "Anims: Blender-scripted jutsu seals and phases"`.

---

### Task 8: Import the phase clips as montages

**Files:**
- Modify: `Scripts/setup_jutsu_anims.py`

**Interfaces:**
- Consumes the Task 7 FBX files. Produces `/Game/Shinobi/Anims/AM_<Jutsu>_{Seal,Charge,Release}`, which the Task 1 soft pointers load.

- [ ] **Step 1: Update `JUTSU_CLIPS`.**
  - The key becomes `(jutsu, phase)`, and the source is `SourceArt/Mixamo/Jutsu/<Jutsu>_<Phase>.fbx`.
  - `import_clip`, `retarget` and `build_montage` get parameterised on the `<Jutsu>_<Phase>` name.
  - Blends: Seal blend-in 0.1 / blend-out 0.1; Charge 0.1 / 0.1; Release 0.05 / 0.25.
  - Delete the old single `AM_<Jutsu>` montages and their `MX_` / `AS_` sources if present.
  - Update the docstring.

- [ ] **Step 2: Run it.** Run `powershell -File Scripts/run_python.ps1 Scripts/setup_jutsu_anims.py` with the editor closed. Expected: 12 `[setup_jutsu_anims] ... AM_<Jutsu>_<Phase>: <len>s` log lines, and no errors.

- [ ] **Step 3: Run the full suite.** Expected: PASS. The tests run with the real montages loaded now; their timings must not change, because the timing comes from C++.

- [ ] **Step 4: Commit.** `git add Scripts/setup_jutsu_anims.py Content/Shinobi/Anims Content/Shinobi/Mixamo/Jutsu` (including the deletions), then `git commit -m "Anims: per-phase jutsu montages"`.

---

### Task 9: Screenshot run, tuning and final checks

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, `.cpp`: a new exec `ShinobiCastHold(int32 JutsuSlot, float Seconds)` (press now, release after `Seconds` via a timer), and a new `RunJutsuShotTest` timeline.
- Modify: the tuning constants in `ShinobiCastFxComponent.cpp` (mouth and eye offsets, sizes) and `ShinobiBlackFlameComponent.cpp`.
- Modify: `docs/superpowers/plans/2026-09-28-jutsu-cinematics.md` (progress notes).

- [ ] **Step 1: Rebuild the shot timeline.** Refill chakra before each cast (as today). Shots:
  - `01_FireballSeal` (+0.3 s)
  - `02_FireballCharge` (hold 1.5 s, shot at +1.5)
  - `03_FireballBreath` (at release + 0.05)
  - `04_FireballImpact`
  - `05_AmaterasuEyes` (+0.35)
  - `06_BlackFlame` (+1.5 after release)
  - `07_ChidoriCharge` (hold 1.5)
  - `08_ChidoriHit`
  - `09_RasenganCharge` (hold 1.5)
  - `10_RasenganHit` / Firestorm
  - `11_Substitution` (kept)
  - Retime `At(...)` to fit, and quit after the last shot.

- [ ] **Step 2: Run and look.** Run `powershell -File Scripts/shot_chakra.ps1 -Jutsu`. Open every PNG in `Saved/Screenshots/`.
  - Tune the eye and mouth offsets (hero and Manny: run once with the `Shinobi.Hero 0` cvar via `-ExecCmds`, if the script supports it; otherwise note that Manny is untested), and the flame, arc and shell sizes.
  - Re-run until each shot reads as the spec describes. Cap this at 4 rounds, then report what's left to the user.

- [ ] **Step 3: Full suite and smoke test.**
  - Run `powershell -File Scripts/run_tests.ps1`. Expected: all pass, the earlier tests and the new ones.
  - Run `powershell -File Scripts/smoke_m1.ps1`. Expected: PASS, as today.

- [ ] **Step 4: Update the docs.**
  - Add a "Done" notes block to this plan: the working Blender export settings and the tuned offsets.
  - Update `docs/superpowers/plans/2026-09-26-m2b-jutsu-visuals.md` Part 2 with one line pointing here (the old single-clip casts are superseded).

- [ ] **Step 5: Commit.** Run `git commit -m "Jutsu cinematics: shot run, tuning, notes"`.

- [ ] **Step 6: USER CHECKPOINT.** Show the user the final screenshots and ask them to play it: tap and hold each jutsu, and in co-op if possible (partners should see the charge and phases).

## Done notes (2026-09-29)

- **Blender export:** armature only, `add_leaf_bones=False`, `armature_nodetype="NULL"`, default axes and scale; every bone lands within 5 mm of the source after a round trip. Blender's importer shifts animation by one frame, so actions start at frame 0.
- **Animation, second pass (user feedback: "more like Naruto / AAA games"):** leg IK with planted feet, a lowered and twisting pelvis, per-phase stances (ready, braced, crouch, run-in and lunge), per-key easing (seals snap and hold, strikes overshoot), and a wind-up before every release. Stances are kept moderate because the hero's proportions exaggerate the pelvis drop.
- **Tuned in game** (`-ShinobiJutsuShot`, 1280x720):
  - hero face offsets from the head bone: mouth (12, 0, 4), eyes (12, ±3, 10.5), in the actor frame, turned with the head bone;
  - eye glow 3 units at intensity 8; blood streaks 1.2 × 9 units;
  - Chidori: 8 arcs, 35–60 units, 2.5 thick;
  - black-flame tongues 18 × 65, Erode 0.2;
  - flame noise 0.06 per unit;
  - lightning burst base size 1.8.
  - The Manny offsets are untested: the player is always the hero.
- **Shot harness:** face shots come from a spawned camera actor, and the aim is restored before each cast. Turning the player's own camera also turns the aim, and the server sees it late.
- **Checks:** 78/78 tests pass; the smoke test passes.
