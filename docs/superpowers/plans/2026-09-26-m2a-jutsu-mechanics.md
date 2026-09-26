# ShinobiCoop M2a (Jutsu Mechanics) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** All of M2's gameplay, networked and tested:
- the four jutsu (Fireball, Chidori, Rasengan, Amaterasu), each with chakra cost, cooldown and an interruptible wind-up;
- the Burn, Black Flame and Shock statuses, and the Firestorm reaction;
- perfect-dodge Substitution;
- jutsu input (keys 1–4, gamepad RB + face buttons);
- HUD jutsu slots and Substitution pips.

Visuals use placeholders: the fireball is a grey ball, and statuses show as text on the training dummies.

**Architecture:**
- **Damage.** One `UShinobiDamageExec` (a gameplay-effect execution) becomes the single place damage is computed. It handles invulnerability (and Substitution), the Shock bonus, and stagger events.
- **Jutsu.** A `UShinobiJutsuAbility` base class handles cost, cooldown, wind-up and interrupt. Each of the four jutsu is a small subclass.
- **Statuses** are C++ `UShinobiEffect` subclasses.
- **Fireball** is a server-spawned, replicated `AShinobiProjectile`.
- **Code layout:** everything follows the M1 patterns (same folders, same test world).

**Tech Stack:** UE 5.8.2 C++, GameplayAbilities (executions, gameplay events, SetByCaller, ability triggers), Enhanced Input (a second mapping context), UMG built in C++.

**Sources this plan builds on:**
- Spec: `docs/superpowers/specs/2026-09-23-combat-slice-design.md`: §3 Jutsu, Statuses and reactions, Targeting, Movement/Substitution; §4 Input, HUD.
- Carry-forward notes: the "Carry-forward notes for M2+" section at the end of `docs/superpowers/plans/2026-09-24-m0-m1-setup-and-core.md`. This plan resolves the M2 items there; see Self-review.

**M2 is split in two:**
- **M2a (this plan):** mechanics.
- **M2b (next plan):** Gameplay Cue VFX (fire, lightning, the Rasengan sphere, flame shells, the Substitution log and smoke) and wind-up animations. M2b needs animation downloads from the user, so it runs separately.

---

## Conventions (read first)

Same as the M0/M1 plan's "Conventions", in short:
- **Paths.** The project root is `C:\Users\ahmad\ShinobiCoop`; all paths below are relative to it. The engine is at `C:\Program Files\Epic Games\UE_5.8`. Its source is available locally: confirm any API you are unsure of there, not from memory.
- **Branch.** Work on `feature/m2a-jutsu-mechanics` (created in Task 0).
- **Close the Unreal Editor** before `Scripts/build.ps1` and before any `Scripts/*.py`.
- **Running scripts.** Build, tests and smoke run as `powershell -ExecutionPolicy Bypass -File Scripts\<script>.ps1`, with the shell tool timeout set to 600000 ms.
  - `Scripts/run_tests.ps1 -Filter ShinobiCoop.Jutsu` runs a subset of the tests.
  - Always finish a task with the full suite (no `-Filter`).
- **Expected red state.** A C++ "failing test" step usually fails by **not compiling** (a missing header or type). That is the expected red state.
- **Editing files.** Use the Write and Edit tools for text files. PowerShell 5.1 `Set-Content` corrupts UTF-8.
- **Staging.**
  - `Config/DefaultEngine.ini` has the user's uncommitted startup-map edit: never stage, revert or commit it.
  - `SourceArt/Mixamo/Power_Up_60fps.fbx` is ignored: never add it.
  - Stage files by explicit path, never with `git add -A`.
- **Commit trailer.** Commit messages end with:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
- **UE code rules.**
  - A new header that declares a `UCLASS`, `USTRUCT` or `UENUM` includes its `*.generated.h` last.
  - A local variable that shadows a member or engine name (e.g. a local `Slot` in a widget, or `bVisible`) is a compile error. Use distinct names.
- **Test-world notes.**
  - Test actors have no floor, so they are always falling.
  - Pawns spawned in tests have no controller, so the aim helpers fall back to the actor's eyes, which look along the actor's facing.
  - A character spawned with `FRotator::ZeroRotator` faces +X.
  - Timers started from test code between `W.Tick` calls (e.g. a wind-up started by a `Tap`) start counting one 0.05 s tick late, so a delay of D fires at about D + 0.05–0.1. Put timing checkpoints at least 0.1 s after the nominal time, never exactly one tick after it. (Found in Task 5.)
  - Falling characters drop ~120 units by t = 0.5 s. Anything gravity-free that is launched later (a projectile) misses them, so use `ShinobiTest::Hover` on the characters. (Found in Task 5.)

## Decisions and deviations from the spec (all deliberate)

| # | Spec says | This plan does | Why |
|---|---|---|---|
| 1 | Bonus tag `State.Shocked.Bonus` | `State.ShockBonus` | A child tag matches its parent. `State.Shocked.Bonus` would satisfy every `State.Shocked` check, so the bonus would count as a stun. |
| 2 | Jutsu numbers in `UJutsuData` assets | `EditDefaultsOnly` properties on each jutsu class | Same pattern as the M1 abilities. Each jutsu's numbers still live in one place. There's no asset script, soft path or cook entry for six numbers per jutsu. |
| 3 | Rasengan snaps to a target within 15° | 45° cone at its 200 reach | At 200 units, 15° is ±52 units; you'd whiff while touching the enemy. Chidori keeps 15°. |
| 4 | (silent) | Periodic damage (Burn, Black Flame) ticks through i-frames. It never staggers, never triggers Substitution and never uses up a Shock bonus. | Being on fire isn't dodgeable. A burn tick shouldn't eat the bonus meant for the follow-up hit. |
| 5 | Shock bonus lasts until consumed | 10 s | Avoids a surprise +25% minutes later. Initial value, tune after playtest. |
| 6 | Burn from Fireball | The direct target only; splash just damages | Simplest reading of "60 direct + 40 splash … applies Burn". |
| 7 | Lock-on (Tab/R3) | Moved to M3 | It's worth building once real enemies exist. M2a has crosshair aim and the Chidori cone snap. |
| 8 | Damage-number types, hit-stop, camera shake | Stay in M5 | They are M5 by the milestone table. |
| 9 | VFX, wind-up animations, Substitution log | M2b | Needs Gameplay Cues and user-downloaded animations. |
| 10 | (new) | Dev cvar `Shinobi.DummyAttack` makes training dummies punch nearby players | Without hostile attackers, Substitution, interrupts and the charge cancel can't be checked by hand until M3. |

Numbers not in the spec (initial values, tune after playtest):
- Fireball flies at 2500 u/s for at most 3 s.
- Chidori dashes at 4000 u/s, stops 100 short of its target, and strikes within 200.
- Substitution lands 150 behind the attacker.
- The dummy punch hits for 25 every 2 s, with 250 reach.

## File structure

| File | Responsibility |
|---|---|
| `Source/ShinobiCoop/Shinobi/ShinobiTags.h/.cpp` | + event, status, jutsu, cooldown and failure tags |
| `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h/.cpp` | **New.** The damage execution: invulnerability/Substitution, Shock bonus, stagger |
| `Source/ShinobiCoop/Shinobi/ShinobiCombat.h/.cpp` | **New.** Server helpers shared by abilities and projectiles: `ApplyDamage`, `ApplyStatus`, `FindHostilesInRadius` |
| `Source/ShinobiCoop/Shinobi/ShinobiEffects.h/.cpp` | `GE_Damage` runs the exec; + `GE_Burn`, `GE_BlackFlame`, `GE_Shock`, `GE_ShockBonus`, `GE_Stagger`, `GE_JutsuCooldown`, `GE_SubstitutionCost`, `GE_SubstitutionRegen` |
| `Source/ShinobiCoop/Shinobi/ShinobiProjectile.h/.cpp` | **New.** Replicated projectile: direct hit + status + splash |
| `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h/.cpp` | + `Jutsu1..4` input IDs, shared stun/stagger blockers, `ApplyDamage` via ShinobiCombat, `FindNearestHostileInCone` |
| `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h/.cpp` | **New.** Jutsu base: cost, wind-up, interrupt + refund, cooldown, aim helpers, HUD slot state |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.h/.cpp` | **New.** Slot 1 |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.h/.cpp` | **New.** Slot 2 |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.h/.cpp` | **New.** Slot 3 + Firestorm |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.h/.cpp` | **New.** Slot 4 |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.h/.cpp` | **New.** Perfect-dodge teleport, triggered by `Event.Substitution` |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h/.cpp` | Ends on a staggering hit or an interrupted pose |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp` | Also cancels jutsu |
| `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h/.cpp` | Grants the new abilities and the Substitution regen; the `CanMove` gate (charge, stun, stagger); ends held abilities on unpossess; binds the RB jutsu modifier |
| `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h/.cpp` | Jutsu actions: keys 1–4, plus `IMC_ShinobiJutsu` (A/B/X/Y) added while RB is held |
| `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h/.cpp` | Four jutsu slots (glyph, key, cost, cooldown shade and seconds, greyed, red flash) and three Substitution pips |
| `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h/.cpp` | Status line in the HP text; `Shinobi.DummyAttack` punches |
| `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h/.cpp` | `ShinobiCast <slot>` cheat, `Burn=` in `ShinobiDumpStats`, a smoke-test cast, a HUD screenshot in `-ShinobiShot` |
| `Scripts/smoke_m1.ps1` | Also asserts the client sees a burning, fireballed dummy |
| `Source/ShinobiCoop/Tests/ShinobiTestWorld.h` | + `FEventCounter`, `Tap`, `CountActors` |
| `Source/ShinobiCoop/Tests/ShinobiDamageTests.cpp` | **New.** Damage exec tests |
| `Source/ShinobiCoop/Tests/ShinobiStatusTests.cpp` | **New.** Status effect tests |
| `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp` | **New.** Projectile, jutsu and Substitution tests |
| `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp` | + chakra charge end conditions |
| `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp` | Jutsu actions: 1 key in each context |
| `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp` | + dummy-attack cheat test |

---

### Task 0: Branch

**Files:** none

- [ ] **Step 1: Create the branch from `main`**

```powershell
cd C:\Users\ahmad\ShinobiCoop
git status --short
git switch -c feature/m2a-jutsu-mechanics main
```
Expected: `git status --short` shows only ` M Config/DefaultEngine.ini`. After the switch, `git branch --show-current` prints `feature/m2a-jutsu-mechanics`. The uncommitted ini edit carries over untouched.

---

### Task 1: Damage execution, combat helpers, new tags

Replaces `GE_Damage`'s plain modifier and its "ignored while invulnerable" requirement with `UShinobiDamageExec`. Invulnerable targets still receive the effect, so the exec can negate the hit and request Substitution. `ApplyDamage` decides "landed" by checking invulnerability *before* applying. This keeps the M1 rule: no chakra and no knockback for a blocked hit.

This task resolves carry-forward notes 1, 2 and the "invulnerability only on GE_Damage" note.

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiTags.h`, `ShinobiTags.cpp` (full replacement)
- Create: `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h`, `ShinobiDamageExec.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiCombat.h`, `ShinobiCombat.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `ShinobiEffects.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`
- Create: `Source/ShinobiCoop/Tests/ShinobiDamageTests.cpp`

- [ ] **Step 1: Replace `Source/ShinobiCoop/Shinobi/ShinobiTags.h`**

```cpp
#pragma once

#include "NativeGameplayTags.h"

namespace ShinobiTags
{
	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_ChakraGain);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown);

	// Gameplay events (sent on the server by UShinobiDamageExec and GA_Substitution)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Stagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Substitution);

	// Character states
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_StaminaRegenBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ChakraCharging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning);        // parent: matches either burn
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning_Normal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning_Black);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shocked);        // stunned
	// Not "State.Shocked.Bonus": a child tag matches its parent, so it would count as a stun.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ShockBonus);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);

	// Ability identity (asset tags)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ChakraCharge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Jutsu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Substitution);

	// Jutsu cooldowns (granted by GE_JutsuCooldown)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Fireball);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Chidori);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Rasengan);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Amaterasu);

	// Activation failure reasons
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fail_NoTarget);
}
```

- [ ] **Step 2: Replace `Source/ShinobiCoop/Shinobi/ShinobiTags.cpp`**

```cpp
#include "Shinobi/ShinobiTags.h"

namespace ShinobiTags
{
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_ChakraGain, "Data.ChakraGain");
	UE_DEFINE_GAMEPLAY_TAG(Data_Cooldown, "Data.Cooldown");

	UE_DEFINE_GAMEPLAY_TAG(Event_Hit_Stagger, "Event.Hit.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(Event_Substitution, "Event.Substitution");

	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_StaminaRegenBlocked, "State.StaminaRegenBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_ChakraCharging, "State.ChakraCharging");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning, "State.Burning");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning_Normal, "State.Burning.Normal");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning_Black, "State.Burning.Black");
	UE_DEFINE_GAMEPLAY_TAG(State_Shocked, "State.Shocked");
	UE_DEFINE_GAMEPLAY_TAG(State_ShockBonus, "State.ShockBonus");
	UE_DEFINE_GAMEPLAY_TAG(State_Staggered, "State.Staggered");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ChakraCharge, "Ability.ChakraCharge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Jutsu, "Ability.Jutsu");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Substitution, "Ability.Substitution");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Fireball, "Cooldown.Jutsu.Fireball");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Chidori, "Cooldown.Jutsu.Chidori");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Rasengan, "Cooldown.Jutsu.Rasengan");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Amaterasu, "Cooldown.Jutsu.Amaterasu");

	UE_DEFINE_GAMEPLAY_TAG(Fail_NoTarget, "Fail.NoTarget");
}
```

- [ ] **Step 3: Add the event counter to `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`**

Inside `namespace ShinobiTest`, after `HasTag`, add:

```cpp
	/** Counts gameplay events with Tag delivered to Character's ability system and remembers the last
	 * instigator. Declare it after the FShinobiTestWorld so it is destroyed first. */
	struct FEventCounter
	{
		int32 Count = 0;
		const AActor* LastInstigator = nullptr;

		FEventCounter(AShinobiCharacterBase* Character, const FGameplayTag& Tag)
		{
			Character->GetAbilitySystemComponent()->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda([this](const FGameplayEventData* Payload)
			{
				++Count;
				LastInstigator = Payload->Instigator.Get();
			});
		}

		FEventCounter(const FEventCounter&) = delete;
		FEventCounter& operator=(const FEventCounter&) = delete;
	};
```

- [ ] **Step 4: Write the failing tests `Source/ShinobiCoop/Tests/ShinobiDamageTests.cpp`**

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStaggerThresholdTest, "ShinobiCoop.Damage.StaggerEventAtTwentyOrMore", ShinobiTestFlags)
bool FShinobiStaggerThresholdTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(200.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(Target, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Attacker->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("19 damage lands"), ShinobiCombat::ApplyDamage(Source, Target, 19.f));
	TestEqual(TEXT("19 damage dealt (exec applies it once)"), ShinobiTest::Get(Target, Health), 481.f);
	TestEqual(TEXT("19 damage doesn't stagger"), Staggers.Count, 0);

	TestTrue(TEXT("20 damage lands"), ShinobiCombat::ApplyDamage(Source, Target, 20.f));
	TestEqual(TEXT("20 damage dealt"), ShinobiTest::Get(Target, Health), 461.f);
	TestEqual(TEXT("20 damage staggers"), Staggers.Count, 1);
	TestTrue(TEXT("Stagger names the attacker"), Staggers.LastInstigator == Attacker);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInvulnerableHitTest, "ShinobiCoop.Damage.InvulnerableNegatesAndRequestsSubstitution", ShinobiTestFlags)
bool FShinobiInvulnerableHitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Substitutions(P, ShinobiTags::Event_Substitution);
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* EnemySource = E->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_DodgeIFrames::StaticClass());
	TestFalse(TEXT("A hit on an invulnerable target doesn't land"), ShinobiCombat::ApplyDamage(EnemySource, P, 50.f));
	TestEqual(TEXT("Invulnerable: no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("Substitution requested"), Substitutions.Count, 1);
	TestTrue(TEXT("Substitution names the attacker"), Substitutions.LastInstigator == E);
	TestEqual(TEXT("A negated hit doesn't stagger"), Staggers.Count, 0);

	// Self-inflicted (nobody to teleport behind): negated, no Substitution.
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Self-inflicted, invulnerable: no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("No Substitution without an attacker"), Substitutions.Count, 1);

	// No charges left: the dodge still avoids the hit, but there's no Substitution.
	ShinobiTest::Set(P, UShinobiAttributeSet::GetSubstitutionChargesAttribute(), 0.f);
	ShinobiCombat::ApplyDamage(EnemySource, P, 50.f);
	TestEqual(TEXT("Out of charges: still no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("Out of charges: no Substitution"), Substitutions.Count, 1);

	W.Tick(0.3f); // the 0.2 s i-frames are over
	TestTrue(TEXT("Lands once the i-frames end"), ShinobiCombat::ApplyDamage(EnemySource, P, 50.f));
	TestEqual(TEXT("Damage after the i-frames"), ShinobiTest::Get(P, Health), 450.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 5: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL. `Shinobi/ShinobiCombat.h` is not found.

- [ ] **Step 6: Write `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ShinobiDamageExec.generated.h"

/**
 * The single place damage is computed. Reads SetByCaller Data.Damage and outputs IncomingDamage.
 * For a direct hit (not a periodic tick):
 * - target has State.Invulnerable: the hit is negated; if another character dealt it and the target has a
 *   Substitution charge, Event.Substitution is sent to the target (GA_Substitution does the rest);
 * - final damage >= StaggerThreshold: Event.Hit.Stagger is sent to the target (interrupts jutsu wind-ups and
 *   chakra charge).
 * Periodic damage (Burn, Black Flame) skips all of that: it ticks through i-frames and never staggers.
 */
UCLASS()
class SHINOBICOOP_API UShinobiDamageExec : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	static constexpr float StaggerThreshold = 20.f;

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
```

- [ ] **Step 7: Write `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp`**

```cpp
#include "Shinobi/ShinobiDamageExec.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	void SendEvent(UAbilitySystemComponent* Target, const FGameplayTag& Tag, const AActor* Instigator, float Magnitude)
	{
		FGameplayEventData Payload;
		Payload.EventTag = Tag;
		Payload.Instigator = Instigator;
		Payload.Target = Target->GetAvatarActor();
		Payload.EventMagnitude = Magnitude;
		Target->HandleGameplayEvent(Tag, &Payload);
	}
}

void UShinobiDamageExec::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* Target = ExecutionParams.GetTargetAbilitySystemComponent();
	const UAbilitySystemComponent* Source = ExecutionParams.GetSourceAbilitySystemComponent();
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float Damage = Spec.GetSetByCallerMagnitude(ShinobiTags::Data_Damage, false, 0.f);
	if (!Target || Damage <= 0.f)
	{
		return;
	}

	// Only direct hits send events. They come from instant effects, whose spec is the applier's own copy, so a
	// listener that removes effects from the target (e.g. chakra charge ending) can't invalidate it mid-execution.
	const bool bPeriodic = Spec.GetPeriod() > 0.f;
	if (!bPeriodic)
	{
		const AActor* Attacker = Source ? Source->GetAvatarActor() : nullptr;
		if (Target->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable))
		{
			const bool bHitByOther = Attacker && Attacker != Target->GetAvatarActor();
			if (bHitByOther && Target->GetNumericAttribute(UShinobiAttributeSet::GetSubstitutionChargesAttribute()) >= 1.f)
			{
				SendEvent(Target, ShinobiTags::Event_Substitution, Attacker, Damage);
			}
			return;
		}
		if (Damage >= StaggerThreshold)
		{
			SendEvent(Target, ShinobiTags::Event_Hit_Stagger, Attacker, Damage);
		}
	}
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UShinobiAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage));
}
```

- [ ] **Step 8: Write `Source/ShinobiCoop/Shinobi/ShinobiCombat.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AShinobiCharacterBase;
class UAbilitySystemComponent;
class UGameplayEffect;

/** Server-side combat helpers shared by abilities and projectiles. Call them on the server only. */
namespace ShinobiCombat
{
	/**
	 * Applies GE_Damage (Data.Damage = Amount) from Source to Target and returns whether the hit landed: false
	 * with no source/target, or when the target is invulnerable. An invulnerable target still receives the
	 * effect so the damage exec can negate it and request Substitution; it just doesn't count as a hit (no
	 * chakra, knockback or status for the attacker).
	 */
	SHINOBICOOP_API bool ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount);

	/**
	 * Applies a status effect (Burn, Shock, ...) from Source to Target, with Data.Damage = DamagePerTick for
	 * effects that deal damage. Skipped (returns false) for an invulnerable target, like a hit that didn't land.
	 */
	SHINOBICOOP_API bool ApplyStatus(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float DamagePerTick = 0.f);

	/** Characters hostile to Caster whose location is within Radius (3D) of Center. */
	SHINOBICOOP_API TArray<AShinobiCharacterBase*> FindHostilesInRadius(const AShinobiCharacterBase* Caster, const FVector& Center, float Radius);
}
```

- [ ] **Step 9: Write `Source/ShinobiCoop/Shinobi/ShinobiCombat.cpp`**

```cpp
#include "Shinobi/ShinobiCombat.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

namespace ShinobiCombat
{
	namespace
	{
		bool IsInvulnerable(const AShinobiCharacterBase* Target)
		{
			return Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable);
		}

		void Apply(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float Damage)
		{
			const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, 1.f, Source->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Damage);
			Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
		}
	}

	bool ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount)
	{
		if (!Source || !Target)
		{
			return false;
		}
		// Decided before applying: the exec negates hits on invulnerable targets, so this is what "landed" means.
		const bool bLanded = !IsInvulnerable(Target);
		Apply(Source, Target, UGE_Damage::StaticClass(), Amount);
		return bLanded;
	}

	bool ApplyStatus(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float DamagePerTick)
	{
		if (!Source || !Target || !Effect || IsInvulnerable(Target))
		{
			return false;
		}
		Apply(Source, Target, Effect, DamagePerTick);
		return true;
	}

	TArray<AShinobiCharacterBase*> FindHostilesInRadius(const AShinobiCharacterBase* Caster, const FVector& Center, float Radius)
	{
		TArray<AShinobiCharacterBase*> Result;
		if (!Caster)
		{
			return Result;
		}
		// ponytail: linear scan over all characters like FindHostilesInCone; switch to an overlap query if counts grow
		for (TActorIterator<AShinobiCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			if (Caster->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Center) <= Radius)
			{
				Result.Add(*It);
			}
		}
		return Result;
	}
}
```

- [ ] **Step 10: Route `GE_Damage` through the exec**

In `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, add to `UShinobiEffect`'s protected helpers (after `InhibitWhileTargetHas`):

```cpp
	/** Deal damage through UShinobiDamageExec, which reads SetByCaller Data.Damage. */
	void AddDamageExecution();
```

In the same file, replace the `UGE_Damage` comment:

```cpp
/** Instant damage through UShinobiDamageExec (SetByCaller Data.Damage). Invulnerable targets take none; see the exec. */
```

In `Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp`, add `#include "Shinobi/ShinobiDamageExec.h"` after the `ShinobiAttributeSet.h` include. Then add this after `InhibitWhileTargetHas`:

```cpp
void UShinobiEffect::AddDamageExecution()
{
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UShinobiDamageExec::StaticClass();
}
```

Replace the `UGE_Damage` constructor. This removes both the `IncomingDamage` modifier (it would double-apply alongside the exec) and the invulnerability application requirement:

```cpp
UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddDamageExecution();
}
```

- [ ] **Step 11: Point the ability helper at ShinobiCombat**

In `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, replace the `ApplyDamage` doc comment with:

```cpp
	/** Server only. Deals Amount through the damage exec (ShinobiCombat::ApplyDamage). Returns whether the hit
	 * landed: false if not authority, no target, or the target was invulnerable. */
```

In `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp`:
- add `#include "Shinobi/ShinobiCombat.h"` and keep the existing includes;
- replace the `ApplyDamage` body with:

```cpp
bool UShinobiGameplayAbility::ApplyDamage(AShinobiCharacterBase* Target, float Amount) const
{
	// Predicted (LocalPredicted) abilities run this on the owning client too; without this guard
	// the client would apply damage to Target locally in addition to the server's authoritative copy.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}
	return ShinobiCombat::ApplyDamage(GetAbilitySystemComponentFromActorInfo(), Target, Amount);
}
```

- [ ] **Step 12: Build and run all tests**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `BUILD OK`, then `ALL 20 TESTS PASSED`.
- The M1 tests `DamageReducesHealthAndClampsAtZero` (380), `InvulnerableIgnoresDamage`, `DodgeCostsStaminaAndGrantsIFrames` and `MeleeComboResetsAfterWindow` must still pass unchanged. Do NOT edit their assertions.
- Check the "GAS warnings / ensures" section for new warnings.

- [ ] **Step 13: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiTags.h Source/ShinobiCoop/Shinobi/ShinobiTags.cpp Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp Source/ShinobiCoop/Shinobi/ShinobiCombat.h Source/ShinobiCoop/Shinobi/ShinobiCombat.cpp Source/ShinobiCoop/Shinobi/ShinobiEffects.h Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp Source/ShinobiCoop/Tests/ShinobiTestWorld.h Source/ShinobiCoop/Tests/ShinobiDamageTests.cpp
git commit -m "feat: damage execution with stagger and substitution events

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Status effects, Shock bonus, shared stun/stagger blockers

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiStatusTests.cpp`

- [ ] **Step 1: Write the failing tests `Source/ShinobiCoop/Tests/ShinobiStatusTests.cpp`**

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Melee.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBurnTest, "ShinobiCoop.Status.BurnTicksAndRefreshes", ShinobiTestFlags)
bool FShinobiBurnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("Burn applied"), ShinobiCombat::ApplyStatus(Source, E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick));
	TestTrue(TEXT("Burning.Normal"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestTrue(TEXT("Parent State.Burning matches"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning));
	W.Tick(0.9f);
	TestEqual(TEXT("No tick on application"), ShinobiTest::Get(E, Health), 500.f);
	W.Tick(0.2f);
	TestEqual(TEXT("First tick at 1 s: 10"), ShinobiTest::Get(E, Health), 490.f);

	// Re-applying at ~2.1 s refreshes the 4 s duration; there's still only one burn (10 per tick, not 20).
	W.Tick(1.0f);
	TestEqual(TEXT("Second tick: 480"), ShinobiTest::Get(E, Health), 480.f);
	ShinobiCombat::ApplyStatus(Source, E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick);
	W.Tick(1.0f);
	TestEqual(TEXT("One stack: third tick is 10"), ShinobiTest::Get(E, Health), 470.f);
	W.Tick(2.0f); // t = 5.1: an unrefreshed burn would have ended at 4.0
	TestTrue(TEXT("Refreshed burn still going at 5.1 s"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	W.Tick(1.5f); // t = 6.6: the refreshed burn ended at ~6.1
	TestFalse(TEXT("Burn over"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning));
	// Timer boundaries can make the tick exactly at expiry land either side, so allow one tick of slack.
	const float After = ShinobiTest::Get(E, Health);
	TestTrue(FString::Printf(TEXT("Ticks until expiry (got %.0f)"), After), After >= 440.f && After <= 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPeriodicRulesTest, "ShinobiCoop.Status.PeriodicDamageIgnoresIFramesAndBonus", ShinobiTestFlags)
bool FShinobiPeriodicRulesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Target = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Substitutions(Target, ShinobiTags::Event_Substitution);
	ShinobiTest::FEventCounter Staggers(Target, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("Black flame applied"), ShinobiCombat::ApplyStatus(Source, Target, UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick));
	TestTrue(TEXT("Burning.Black"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Black));
	TestTrue(TEXT("Shock bonus applied"), ShinobiCombat::ApplyStatus(Source, Target, UGE_ShockBonus::StaticClass()));

	Target->GetAbilitySystemComponent()->AddLooseGameplayTag(ShinobiTags::State_Invulnerable);
	TestFalse(TEXT("No new statuses on an invulnerable target"), ShinobiCombat::ApplyStatus(Source, Target, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick));
	TestFalse(TEXT("Burn not applied"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Normal));

	W.Tick(1.1f);
	TestEqual(TEXT("Black flame ticks through invulnerability"), ShinobiTest::Get(Target, Health), 485.f);
	TestTrue(TEXT("Periodic ticks don't use up the Shock bonus"), ShinobiTest::HasTag(Target, ShinobiTags::State_ShockBonus));
	TestEqual(TEXT("Periodic ticks never request Substitution"), Substitutions.Count, 0);
	TestEqual(TEXT("Periodic ticks never stagger"), Staggers.Count, 0);
	Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShinobiTags::State_Invulnerable);

	W.Tick(9.5f); // t = 10.6
	TestFalse(TEXT("Black flame over after 10 s"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Black));
	const float After = ShinobiTest::Get(Target, Health);
	TestTrue(FString::Printf(TEXT("15 per second for 10 s (got %.0f)"), After), After >= 350.f && After <= 365.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiShockBonusTest, "ShinobiCoop.Status.ShockBonusAddsQuarterOnce", ShinobiTestFlags)
bool FShinobiShockBonusTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiCombat::ApplyStatus(Source, E, UGE_ShockBonus::StaticClass());
	ShinobiCombat::ApplyDamage(Source, E, 100.f);
	TestEqual(TEXT("Shocked target takes +25%"), ShinobiTest::Get(E, Health), 375.f);
	TestFalse(TEXT("Bonus consumed"), ShinobiTest::HasTag(E, ShinobiTags::State_ShockBonus));
	ShinobiCombat::ApplyDamage(Source, E, 100.f);
	TestEqual(TEXT("Next hit is normal"), ShinobiTest::Get(E, Health), 275.f);

	ShinobiCombat::ApplyStatus(Source, E, UGE_ShockBonus::StaticClass());
	W.Tick(10.1f);
	TestFalse(TEXT("Unused bonus expires after 10 s"), ShinobiTest::HasTag(E, ShinobiTags::State_ShockBonus));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStunTest, "ShinobiCoop.Status.StunAndStaggerBlockActions", ShinobiTestFlags)
bool FShinobiStunTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	ShinobiTest::ApplyEffect(P, UGE_Shock::StaticClass());
	TestTrue(TEXT("Shocked"), ShinobiTest::HasTag(P, ShinobiTags::State_Shocked));
	TestFalse(TEXT("Shocked: can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("Shocked: can't attack"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Shocked: can't dodge"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	W.Tick(1.6f);
	TestFalse(TEXT("Shock over after 1.5 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Shocked));
	TestTrue(TEXT("Can move again"), P->CanMove());

	ShinobiTest::ApplyEffect(P, UGE_Stagger::StaticClass());
	TestFalse(TEXT("Staggered: can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Staggered: can't dodge"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	W.Tick(0.6f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Stagger over after 0.5 s"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL. `UGE_Burn` is undeclared.

- [ ] **Step 3: Add the effect helpers and status effects to `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`**

Add to `UShinobiEffect`'s protected helpers (after `AddDamageExecution`):

```cpp
	/** For timed effects (SetDuration): execute every PeriodSeconds, the first time one period after application. */
	void SetPeriodic(float PeriodSeconds);
	/** One copy per target from any source: re-applying refreshes the duration instead of stacking a second copy. */
	void UseSingleRefreshingStack();
```

Append at the end of the file:

```cpp
/** Burn: 10 damage per second for 4 s, through the damage exec. Re-applying refreshes the duration. */
UCLASS()
class SHINOBICOOP_API UGE_Burn : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 10.f;
	UGE_Burn();
};

/** Amaterasu's black flame: 15 damage per second for 10 s. Firestorm never removes it. */
UCLASS()
class SHINOBICOOP_API UGE_BlackFlame : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 15.f;
	UGE_BlackFlame();
};

/** Shock stun: 1.5 s of State.Shocked (no abilities, no movement). */
UCLASS()
class SHINOBICOOP_API UGE_Shock : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Shock();
};

/** Shock bonus: the next direct hit within 10 s deals +25%; the damage exec consumes it. */
UCLASS()
class SHINOBICOOP_API UGE_ShockBonus : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ShockBonus();
};

/** Stagger: 0.5 s of State.Staggered (no abilities, no movement). Substitution staggers the attacker. */
UCLASS()
class SHINOBICOOP_API UGE_Stagger : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Stagger();
};
```

- [ ] **Step 4: Implement them in `Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp`**

After `AddDamageExecution`:

```cpp
void UShinobiEffect::SetPeriodic(float PeriodSeconds)
{
	Period = FScalableFloat(PeriodSeconds);
	bExecutePeriodicEffectOnApplication = false;
}

void UShinobiEffect::UseSingleRefreshingStack()
{
	// Direct access to StackingType is deprecated since 5.7 and its setter is editor-only, so constructors still
	// assign it directly, the same way UGameplayEffect's own constructor does.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
}
```

At the end of the file:

```cpp
UGE_Burn::UGE_Burn()
{
	SetDuration(4.f);
	SetPeriodic(1.f);
	AddDamageExecution();
	GrantTag(ShinobiTags::State_Burning_Normal);
	UseSingleRefreshingStack();
}

UGE_BlackFlame::UGE_BlackFlame()
{
	SetDuration(10.f);
	SetPeriodic(1.f);
	AddDamageExecution();
	GrantTag(ShinobiTags::State_Burning_Black);
	UseSingleRefreshingStack();
}

UGE_Shock::UGE_Shock()
{
	SetDuration(1.5f);
	GrantTag(ShinobiTags::State_Shocked);
	UseSingleRefreshingStack();
}

UGE_ShockBonus::UGE_ShockBonus()
{
	SetDuration(10.f);
	GrantTag(ShinobiTags::State_ShockBonus);
	UseSingleRefreshingStack();
}

UGE_Stagger::UGE_Stagger()
{
	SetDuration(0.5f);
	GrantTag(ShinobiTags::State_Staggered);
	UseSingleRefreshingStack();
}
```

- [ ] **Step 5: Consume the Shock bonus in the damage exec**

In `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h`:
- add `static constexpr float ShockBonusMultiplier = 1.25f;` below `StaggerThreshold`;
- add this bullet to the class comment, before the stagger bullet:
  ` * - target has State.ShockBonus: damage x ShockBonusMultiplier and the bonus is consumed;`
- change the last comment line to: ` * Periodic damage (Burn, Black Flame) skips all of that: it ticks through i-frames, never staggers and never uses up the bonus.`

In `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp`:
- change `const float Damage =` to `float Damage =`;
- insert this between the invulnerability block and the stagger check:

```cpp
		if (Target->HasMatchingGameplayTag(ShinobiTags::State_ShockBonus))
		{
			Damage *= ShockBonusMultiplier;
			Target->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_ShockBonus));
		}
```

- [ ] **Step 6: Shared blockers in `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp`**

Make sure `#include "Shinobi/ShinobiTags.h"` is present, then replace the constructor:

```cpp
UShinobiGameplayAbility::UShinobiGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Shared blockers: a stunned or staggered character can't act. Subclasses add their own.
	ActivationBlockedTags.AddTag(ShinobiTags::State_Shocked);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Staggered);
}
```

- [ ] **Step 7: The movement gate in `AShinobiPlayerCharacter`**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, add to the public section after `GetChakraAura`:

```cpp
	/** False while charging chakra, shocked or staggered: move and jump input are ignored. */
	bool CanMove() const;
```

Change the `JumpPressed` comment to: `/** Ignores the jump input while CanMove() is false. */`

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add after the constructor:

```cpp
bool AShinobiPlayerCharacter::CanMove() const
{
	static const FGameplayTagContainer Blockers = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ShinobiTags::State_ChakraCharging);
		Tags.AddTag(ShinobiTags::State_Shocked);
		Tags.AddTag(ShinobiTags::State_Staggered);
		return Tags;
	}();
	return !AbilitySystem->HasAnyMatchingGameplayTags(Blockers);
}
```

- In `Move`, replace `if (!Controller || AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))` with `if (!Controller || !CanMove())`.
- In `JumpPressed`, replace `if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))` with `if (!CanMove())`.

- [ ] **Step 8: Build and run all tests**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 24 TESTS PASSED`. The build must not show a C4996 deprecation warning from `ShinobiEffects.cpp`, because the pragma covers it.

- [ ] **Step 9: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiEffects.h Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiStatusTests.cpp
git commit -m "feat: burn, black flame, shock and stagger statuses

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Chakra charge end conditions

The charge is the one ability that never ends on its own, so it needs every exit:
- **Stagger:** a staggering hit ends it (spec).
- **Interrupted pose:** if something else takes over its montage slot, it ends (carry-forward note). Otherwise the character is left movement-locked in the idle pose.
- **Lost controller:** losing the controller ends the held abilities (carry-forward priority note).

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`

- [ ] **Step 1: Append the failing tests to `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`**

Add `#include "AIController.h"` next to the other includes. Then, before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeStaggerTest, "ShinobiCoop.Abilities.ChakraChargeEndsOnStaggerOrBrokenPose", ShinobiTestFlags)
bool FShinobiChargeStaggerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	ShinobiTest::Damage(P, 10.f);
	TestTrue(TEXT("A 10-damage hit doesn't end the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	ShinobiTest::Damage(P, 25.f);
	TestFalse(TEXT("A 25-damage hit ends the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag gone"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);

	// Another montage taking over the slot (a future hit reaction) must end the charge too.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging again"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	UAnimMontage* Other = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Dodge.AM_Dodge"));
	UAnimInstance* Anim = P->GetMesh()->GetAnimInstance();
	if (TestNotNull(TEXT("AM_Dodge loads"), Other) && TestNotNull(TEXT("Player has an anim instance"), Anim))
	{
		Anim->Montage_Play(Other);
		W.Tick(0.1f); // blending-out events are queued and dispatched during the anim tick
		TestFalse(TEXT("Charge ends when its pose is interrupted"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiUnpossessTest, "ShinobiCoop.Abilities.UnpossessEndsHeldAbilities", ShinobiTestFlags)
bool FShinobiUnpossessTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AAIController* Controller = W.Spawn<AAIController>();

	Controller->Possess(P);
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging while possessed"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess ends the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	Controller->Possess(P);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprinting while possessed"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess ends the sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	return true;
}
```

- [ ] **Step 2: Build and run to see them fail**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1 -Filter ShinobiCoop.Abilities
```
Expected: the build succeeds, and the two new tests FAIL on "A 25-damage hit ends the charge", "Charge ends when its pose is interrupted" and "Unpossess ends the charge".

- [ ] **Step 3: Update `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h`**

Replace the class comment's "ends on release, dodge, or (M2) a stagger hit" with "ends on release, dodge, a staggering hit, or its pose being interrupted".

Add to the private section, after `OnInputReleased`:

```cpp
	/** A staggering hit, or another montage taking over the pose: end the charge. */
	UFUNCTION()
	void OnChargeBroken();

	UFUNCTION()
	void OnStaggered(FGameplayEventData Payload);
```

- [ ] **Step 4: Update `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.cpp`**

Add `#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"` after the WaitInputRelease include.

In `ActivateAbility`, bind the interrupt before `Play->ReadyForActivation();`:

```cpp
		// Anything else taking over the slot (e.g. a hit reaction) would leave the character charging in the idle
		// pose, movement-locked. OnCancelled is deliberately not bound: it also fires when the montage can't play
		// at all (no anim instance), which must not stop the charge itself.
		Play->OnInterrupted.AddDynamic(this, &UGA_ChakraCharge::OnChargeBroken);
```

Right after the `Release->ReadyForActivation();` line, add:

```cpp
	// Sent only on the server by the damage exec (>= 20 damage); the end replicates to the owner.
	UAbilityTask_WaitGameplayEvent* Hit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	Hit->EventReceived.AddDynamic(this, &UGA_ChakraCharge::OnStaggered);
	Hit->ReadyForActivation();
```

After `OnInputReleased`, add:

```cpp
void UGA_ChakraCharge::OnChargeBroken()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ChakraCharge::OnStaggered(FGameplayEventData Payload)
{
	OnChargeBroken();
}
```

- [ ] **Step 5: End held abilities on unpossess**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, add to the protected section after `SetupPlayerInputComponent`:

```cpp
	virtual void UnPossessed() override;
```

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add after `CanMove`:

```cpp
void AShinobiPlayerCharacter::UnPossessed()
{
	// A lost controller (unpossess, disconnect) never sends the release for held inputs, and chakra charge never
	// ends on its own, so end the hold-to-use abilities here. Runs on the server; the end replicates to the owner.
	FGameplayTagContainer HeldAbilities;
	HeldAbilities.AddTag(ShinobiTags::Ability_Sprint);
	HeldAbilities.AddTag(ShinobiTags::Ability_ChakraCharge);
	AbilitySystem->CancelAbilities(&HeldAbilities);
	Super::UnPossessed();
}
```

- [ ] **Step 6: Build and run all tests**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 26 TESTS PASSED`. The M1 `ChakraChargeRefillsWhileHeld` must still pass: its release path ends the ability normally, which must not trip `OnInterrupted`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp
git commit -m "fix: chakra charge ends on stagger, broken pose, or lost controller

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Projectile

A replicated actor that the server spawns. It is tested on its own here, before any jutsu uses it.

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/ShinobiProjectile.h`, `ShinobiProjectile.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

- [ ] **Step 1: Write the failing test `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiProjectile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiProjectileTest, "ShinobiCoop.Jutsu.ProjectileHitsFirstHostileAndSplashes", ShinobiTestFlags)
bool FShinobiProjectileTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f)); // in the flight path
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	AShinobiCharacterBase* Nearby = W.Spawn<AShinobiCharacterBase>(FVector(650.f, 150.f, 0.f));
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 500.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	const FTransform Launch(FRotator::ZeroRotator, FVector(60.f, 0.f, 40.f)); // flying along +X
	AShinobiProjectile* Ball = W.World->SpawnActorDeferred<AShinobiProjectile>(AShinobiProjectile::StaticClass(), Launch, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Ball->DirectDamage = 60.f;
	Ball->SplashDamage = 40.f;
	Ball->SplashRadius = 250.f;
	Ball->StatusEffect = UGE_Burn::StaticClass();
	Ball->StatusDamagePerTick = UGE_Burn::DamagePerTick;
	Ball->FinishSpawning(Launch);
	const TWeakObjectPtr<AShinobiProjectile> WeakBall(Ball);

	W.Tick(0.5f); // 2500 u/s: reaches the target ~500 units away in ~0.2 s
	TestFalse(TEXT("Projectile destroyed on impact"), WeakBall.IsValid());
	TestEqual(TEXT("Ally in the path is flown through, unharmed"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("Direct hit: 60"), ShinobiTest::Get(Target, Health), 440.f);
	TestTrue(TEXT("Direct target burning"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Splash: 40"), ShinobiTest::Get(Nearby, Health), 460.f);
	TestFalse(TEXT("Splash doesn't burn"), ShinobiTest::HasTag(Nearby, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Outside the splash radius"), ShinobiTest::Get(Far, Health), 500.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL. `Shinobi/ShinobiProjectile.h` is not found.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/ShinobiProjectile.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiProjectile.generated.h"

class AShinobiCharacterBase;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Server-spawned, replicated projectile (Fireball; M3's kunai). Flies straight; explodes on the first character
 * hostile to its instigator (direct damage, plus StatusEffect if the hit lands) or on world geometry, and deals
 * SplashDamage to the other hostiles within SplashRadius. Damage is applied on the server only, from the
 * instigator's ability system. Spawn with SpawnActorDeferred (Owner and Instigator = the caster), set the damage
 * fields, then FinishSpawning.
 */
UCLASS()
class SHINOBICOOP_API AShinobiProjectile : public AActor
{
	GENERATED_BODY()

public:
	AShinobiProjectile();

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float DirectDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SplashDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SplashRadius = 0.f;

	/** Applied to the direct target when its hit lands (e.g. GE_Burn), with Data.Damage = StatusDamagePerTick. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TSubclassOf<UGameplayEffect> StatusEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float StatusDamagePerTick = 0.f;

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Server only. DirectTarget may be null (hit a wall or the floor). */
	void Explode(AShinobiCharacterBase* DirectTarget, const FVector& Location);

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UProjectileMovementComponent> Movement;

	bool bExploded = false;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/ShinobiProjectile.cpp`**

```cpp
#include "Shinobi/ShinobiProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"

AShinobiProjectile::AShinobiProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 3.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Characters overlap instead of block, so hostility (not collision) decides what the projectile explodes on;
	// walls and the floor still stop it.
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AShinobiProjectile::OnOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AShinobiProjectile::OnHit);
	RootComponent = Collision;

	// ponytail: a grey ball until M2b's fire VFX
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.4f)); // the engine sphere is 100 units across
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 2500.f;
	Movement->MaxSpeed = 2500.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void AShinobiProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AShinobiCharacterBase* Caster = Cast<AShinobiCharacterBase>(GetInstigator());
	AShinobiCharacterBase* Target = Cast<AShinobiCharacterBase>(OtherActor);
	if (HasAuthority() && Caster && Caster->IsHostileTo(Target))
	{
		Explode(Target, GetActorLocation());
	}
}

void AShinobiProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority())
	{
		Explode(nullptr, Hit.ImpactPoint);
	}
}

void AShinobiProjectile::Explode(AShinobiCharacterBase* DirectTarget, const FVector& Location)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;

	const AShinobiCharacterBase* Caster = Cast<AShinobiCharacterBase>(GetInstigator());
	if (UAbilitySystemComponent* Source = Caster ? Caster->GetAbilitySystemComponent() : nullptr)
	{
		if (DirectTarget && ShinobiCombat::ApplyDamage(Source, DirectTarget, DirectDamage) && StatusEffect)
		{
			ShinobiCombat::ApplyStatus(Source, DirectTarget, StatusEffect, StatusDamagePerTick);
		}
		for (const AShinobiCharacterBase* Other : ShinobiCombat::FindHostilesInRadius(Caster, Location, SplashRadius))
		{
			if (Other != DirectTarget)
			{
				ShinobiCombat::ApplyDamage(Source, Other, SplashDamage);
			}
		}
	}
	Destroy();
}
```

- [ ] **Step 5: Build and run all tests**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 27 TESTS PASSED`.

If the projectile test fails with all healths at 500 and the projectile still valid, overlaps aren't firing in the test world. Tell the controller; don't work around it with a direct `Explode` call. Check these before reporting:
- the physics scene exists: `W.World->GetPhysicsScene()` is non-null;
- the ball actually moved.

- [ ] **Step 6: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiProjectile.h Source/ShinobiCoop/Shinobi/ShinobiProjectile.cpp Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: replicated projectile with direct hit, status and splash

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Jutsu base class and Fireball

**Base class (`UShinobiJutsuAbility`):**
- **Activation:** `PrepareJutsu()` runs first and may fail for free. Then the chakra cost is paid (cost only, via `CommitAbilityCost`) and the caster faces the aim.
- **Wind-up:** a hand-seal delay; M2b adds the animation.
- **Interrupt:** a staggering hit during the wind-up cancels the jutsu and refunds 50%, with no cooldown.
- **Otherwise:** the cooldown starts (`CommitAbilityCooldown`) and `ExecuteJutsu()` runs.
- **Cost and cooldown overrides:** cost is paid from Chakra through `GE_ChakraGain` with a negative SetByCaller. Cooldown is one `GE_JutsuCooldown` with a SetByCaller duration, plus each jutsu's `Cooldown.Jutsu.*` tag as a dynamic granted tag. `CheckCooldown` uses the overridden `GetCooldownTags()`.

Fireball is the first subclass and the vehicle for the base-class tests.

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h` (input IDs)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `.cpp` (`UGE_JutsuCooldown`)
- Create: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h`, `.cpp`
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`, `ShinobiJutsuTests.cpp`

- [ ] **Step 1: Add test helpers to `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`**

Add `#include "EngineUtils.h"` and `#include "GameFramework/CharacterMovementComponent.h"` to the includes. Inside `namespace ShinobiTest`, after `Release`:

```cpp
	/** Press and release (jutsu fire on press). Not named Cast: that would hide UE's Cast<T> inside this namespace. */
	inline void Tap(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Press(Character, InputID);
		Release(Character, InputID);
	}

	/** The test world has no floor, so characters fall. Flying mode with no input keeps one hovering in place, e.g. so a
	 * gravity-free projectile thrown after a wind-up still meets it where it was aimed. */
	inline void Hover(AShinobiCharacterBase* Character)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}

	template <class T>
	int32 CountActors(const FShinobiTestWorld& W)
	{
		int32 Count = 0;
		for (TActorIterator<T> It(W.World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
```

- [ ] **Step 2: Append the failing tests to `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

Add these includes after the existing ones:

```cpp
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Melee.h"
```

Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFireballTest, "ShinobiCoop.Jutsu.FireballCostWindUpCooldown", ShinobiTestFlags)
bool FShinobiFireballTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	// No floor: both would fall while the gravity-free ball flies level and passes over the enemy.
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	UAbilitySystemComponent* ASC = P->GetAbilitySystemComponent();
	const UGA_Fireball* Fireball = GetDefault<UGA_Fireball>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Fireball active"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Cost 20 paid up front"), ShinobiTest::Get(P, Chakra), 80.f);
	TestFalse(TEXT("No cooldown during the wind-up"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	W.Tick(0.3f);
	TestEqual(TEXT("Nothing thrown during the 0.4 s wind-up"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);

	// Timers started from test code between ticks start counting a tick late, so a 0.4 s wind-up fires at ~0.45-0.5.
	W.Tick(0.2f); // t = 0.5
	TestEqual(TEXT("Thrown after the wind-up"), ShinobiTest::CountActors<AShinobiProjectile>(W), 1);
	TestTrue(TEXT("Cooldown starts after the wind-up"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	TestFalse(TEXT("Ability ends once thrown"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	const FShinobiJutsuSlotState Slot = Fireball->GetSlotState(*ASC);
	TestTrue(FString::Printf(TEXT("HUD sees the cooldown (%.2f s of %.2f s left)"), Slot.CooldownRemaining, Slot.CooldownDuration),
		Slot.CooldownRemaining > 3.5f && Slot.CooldownRemaining <= 4.f && FMath::IsNearlyEqual(Slot.CooldownDuration, 4.f));

	const float ChakraAfterCast = ShinobiTest::Get(P, Chakra);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Blocked by the cooldown"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Nothing spent on a blocked cast"), ShinobiTest::Get(P, Chakra), ChakraAfterCast);

	W.Tick(0.4f); // t = 0.9: the ball has reached the enemy
	TestEqual(TEXT("Enemy hit for 60"), ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute()), 440.f);
	TestTrue(TEXT("Enemy burning"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));

	W.Tick(3.8f); // t = 4.7; the cooldown started at ~0.45-0.5 and lasts 4 s
	TestFalse(TEXT("Cooldown over"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	ShinobiTest::Set(P, Chakra, 19.f);
	TestFalse(TEXT("HUD: unaffordable below the cost"), Fireball->GetSlotState(*ASC).bAffordable);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Blocked below 20 chakra"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Nothing spent when unaffordable"), ShinobiTest::Get(P, Chakra), 19.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInterruptTest, "ShinobiCoop.Jutsu.StaggerInterruptsWindUpWithHalfRefund", ShinobiTestFlags)
bool FShinobiInterruptTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Cost paid"), ShinobiTest::Get(P, Chakra), 80.f);
	W.Tick(0.1f);
	ShinobiTest::Damage(P, 10.f);
	TestTrue(TEXT("A 10-damage hit doesn't interrupt"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	ShinobiTest::Damage(P, 25.f);
	TestFalse(TEXT("A 25-damage hit interrupts the wind-up"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	const float Refunded = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("Half the cost refunded (got %.2f)"), Refunded), Refunded >= 90.f && Refunded <= 90.6f);
	TestFalse(TEXT("No cooldown after an interrupt"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	W.Tick(0.5f);
	TestEqual(TEXT("Nothing thrown"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Can recast right away"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiJutsuRulesTest, "ShinobiCoop.Jutsu.BlockedWhileChargingCancelledByDodge", ShinobiTestFlags)
bool FShinobiJutsuRulesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("No jutsu while charging"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Nothing spent"), ShinobiTest::Get(P, Chakra), 100.f);
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);

	// A jutsu can finish a melee combo.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestTrue(TEXT("Melee swinging"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Jutsu starts mid-combo"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestFalse(TEXT("Jutsu cancels the combo"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));

	// Dodging out of a wind-up cancels it: no refund, no cooldown, nothing thrown.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge starts during the wind-up"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("Dodge cancels the jutsu"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	W.Tick(0.6f);
	TestEqual(TEXT("Nothing thrown"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);
	TestFalse(TEXT("No cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	return true;
}
```

- [ ] **Step 3: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL. `Shinobi/Abilities/GA_Fireball.h` is not found.

- [ ] **Step 4: Add the jutsu input IDs**

In `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, extend the enum. Append the new values at the end so the existing values don't change:

```cpp
enum class EShinobiAbilityInputID : uint8
{
	None,
	Melee,
	Dodge,
	Sprint,
	ChakraCharge,
	Jutsu1,
	Jutsu2,
	Jutsu3,
	Jutsu4
};
```

- [ ] **Step 5: Add `UGE_JutsuCooldown` to `Source/ShinobiCoop/Shinobi/ShinobiEffects.h` / `.cpp`**

Header, appended:

```cpp
/** Jutsu cooldown. Duration: SetByCaller Data.Cooldown. The jutsu's Cooldown.Jutsu.* tag is added to each spec as a
 * dynamic granted tag (UShinobiJutsuAbility::ApplyCooldown), so one class serves all four jutsu. */
UCLASS()
class SHINOBICOOP_API UGE_JutsuCooldown : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_JutsuCooldown();
};
```

Source, appended:

```cpp
UGE_JutsuCooldown::UGE_JutsuCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ShinobiTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
```

- [ ] **Step 6: Write `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiJutsuAbility.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitGameplayEvent;

/** What the HUD shows for one jutsu slot. */
struct FShinobiJutsuSlotState
{
	float CooldownRemaining = 0.f;
	float CooldownDuration = 0.f;
	bool bAffordable = true;
};

/**
 * Base for the four jutsu. On activation: PrepareJutsu() (may fail for free), pay the chakra cost, face the
 * aim, then a hand-seal wind-up. A staggering hit (Event.Hit.Stagger, >= 20 damage) during the wind-up
 * cancels the jutsu and refunds half the cost without starting the cooldown. Otherwise the cooldown starts
 * and ExecuteJutsu() runs. Each subclass sets its numbers in its constructor (the spec's UJutsuData, kept in
 * C++ like the M1 abilities).
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiJutsuAbility : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UShinobiJutsuAbility();

	static constexpr float InterruptRefundFraction = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	FText DisplayName;

	/** Kanji shown on the HUD slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	FText Glyph;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ChakraCost = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Cooldown = 0.f;

	/** Hand-seal wind-up before the jutsu fires; a staggering hit interrupts it. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "0.01"))
	float WindUp = 0.5f;

	/** For the HUD; safe to call on the class default object. */
	FShinobiJutsuSlotState GetSlotState(const UAbilitySystemComponent& ASC) const;

	virtual const FGameplayTagContainer* GetCooldownTags() const override { return &CooldownTags; }
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Runs before any cost is paid. Return false to fail the activation for free (the HUD flashes the slot). */
	virtual bool PrepareJutsu() { return true; }

	/** Runs after the wind-up, once the cooldown has started. Must end the ability (EndJutsu), directly or from a task. */
	virtual void ExecuteJutsu() PURE_VIRTUAL(UShinobiJutsuAbility::ExecuteJutsu, );

	void EndJutsu();

	/** Players: the camera (the server has the client's from ServerUpdateCamera). AI and tests: the eyes. */
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Sphere-sweeps along the aim on ECC_Pawn (hits characters and world geometry), ignoring the caster.
	 * Range counts from the caster, not from the camera behind it. */
	bool TraceAim(float Range, float Radius, FHitResult& OutHit) const;

	/** Turns the caster toward Target, or to the aim's yaw when Target is null. */
	void FaceAim(const AActor* Target) const;

	/** This jutsu's Cooldown.Jutsu.* tag; set in each subclass constructor. */
	FGameplayTagContainer CooldownTags;

private:
	UFUNCTION()
	void OnWindUpFinished();

	UFUNCTION()
	void OnInterruptedByHit(FGameplayEventData Payload);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InterruptTask;
};
```

- [ ] **Step 7: Write `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp`**

```cpp
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UShinobiJutsuAbility::UShinobiJutsuAbility()
{
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Jutsu));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee); // a jutsu can finish a combo
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
}

void UShinobiJutsuAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!PrepareJutsu())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->NotifyAbilityFailed(Handle, this, FGameplayTagContainer(ShinobiTags::Fail_NoTarget));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Cost now, cooldown only when the wind-up completes: an interrupted jutsu refunds half and can be recast at once.
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FaceAim(nullptr);

	// Event.Hit.Stagger is only sent on the server, so only the server's copy gets interrupted; its cancel
	// replicates to the owning client.
	InterruptTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	InterruptTask->EventReceived.AddDynamic(this, &UShinobiJutsuAbility::OnInterruptedByHit);
	InterruptTask->ReadyForActivation();

	UAbilityTask_WaitDelay* Seals = UAbilityTask_WaitDelay::WaitDelay(this, WindUp);
	Seals->OnFinish.AddDynamic(this, &UShinobiJutsuAbility::OnWindUpFinished);
	Seals->ReadyForActivation();
}

void UShinobiJutsuAbility::OnWindUpFinished()
{
	if (InterruptTask)
	{
		InterruptTask->EndTask();
		InterruptTask = nullptr;
	}
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	ExecuteJutsu();
}

void UShinobiJutsuAbility::OnInterruptedByHit(FGameplayEventData Payload)
{
	FGameplayEffectSpecHandle Refund = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
	Refund.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraCost * InterruptRefundFraction);
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Refund);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UShinobiJutsuAbility::EndJutsu()
{
	// Only the server replicates a normal end. The owning client's wind-up starts ~one-way latency earlier, so a
	// client-sent ServerEndAbility can arrive before the server's own wind-up timer fires and kill its copy before it
	// executes (chakra spent, nothing thrown). Cancels (dodge, interrupt) still replicate from either side.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
}

bool UShinobiJutsuAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) >= ChakraCost)
	{
		return true;
	}
	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(CostTag);
	}
	return false;
}

void UShinobiJutsuAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UGE_ChakraGain::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, -ChakraCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UShinobiJutsuAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UGE_JutsuCooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Cooldown, Cooldown);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

FShinobiJutsuSlotState UShinobiJutsuAbility::GetSlotState(const UAbilitySystemComponent& ASC) const
{
	FShinobiJutsuSlotState State;
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	for (const TPair<float, float>& RemainingAndDuration : ASC.GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (RemainingAndDuration.Key > State.CooldownRemaining)
		{
			State.CooldownRemaining = RemainingAndDuration.Key;
			State.CooldownDuration = RemainingAndDuration.Value;
		}
	}
	State.bAffordable = ASC.GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) >= ChakraCost;
	return State;
}

void UShinobiJutsuAbility::GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!Pawn)
	{
		return;
	}
	if (const AController* Controller = Pawn->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	}
	else
	{
		Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
	}
}

bool UShinobiJutsuAbility::TraceAim(float Range, float Radius, FHitResult& OutHit) const
{
	const AActor* Self = GetAvatarActorFromActorInfo();
	if (!Self)
	{
		return false;
	}
	FVector Start;
	FRotator Rotation;
	GetAimViewPoint(Start, Rotation);
	const FVector Direction = Rotation.Vector();
	// Start level with the caster, not at the camera 450 units behind: anyone standing between them (a partner, or an
	// enemy behind you) would otherwise become the aim point or target.
	Start += Direction * FMath::Max(0.f, FVector::DotProduct(Self->GetActorLocation() - Start, Direction));
	const FVector End = Start + Direction * Range;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiAim), false, Self);
	return Self->GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);
}

void UShinobiJutsuAbility::FaceAim(const AActor* Target) const
{
	AActor* Self = GetAvatarActorFromActorInfo();
	if (!Self)
	{
		return;
	}
	float Yaw = 0.f;
	if (Target)
	{
		Yaw = (Target->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw;
	}
	else
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		Yaw = ViewRotation.Yaw;
	}
	Self->SetActorRotation(FRotator(0.f, Yaw, 0.f));
}
```

- [ ] **Step 8: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Fireball.generated.h"

/** Slot 1: a fireball thrown toward the crosshair. 60 damage to the first enemy it touches (which starts
 * burning), 40 to other enemies within 250 of the impact. */
UCLASS()
class SHINOBICOOP_API UGA_Fireball : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Fireball();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float DirectDamage = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SplashDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SplashRadius = 250.f;

	/** How far the crosshair trace looks for an aim point. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRange = 5000.f;

protected:
	virtual void ExecuteJutsu() override;
};
```

- [ ] **Step 9: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/ShinobiTags.h"

UGA_Fireball::UGA_Fireball()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	DisplayName = INVTEXT("Fireball");
	Glyph = INVTEXT("火"); // fire
	ChakraCost = 20.f;
	Cooldown = 4.f;
	WindUp = 0.4f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Fireball);
}

void UGA_Fireball::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the wind-up
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		const FVector AimPoint = TraceAim(AimRange, 10.f, Hit) ? Hit.ImpactPoint : ViewLocation + ViewRotation.Vector() * AimRange;

		// ponytail: fixed chest-height launch point in front of the caster; a hand socket arrives with M2b's animations
		const FVector Muzzle = Self->GetActorLocation() + Self->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
		const FTransform Launch((AimPoint - Muzzle).Rotation(), Muzzle);
		if (AShinobiProjectile* Ball = Self->GetWorld()->SpawnActorDeferred<AShinobiProjectile>(AShinobiProjectile::StaticClass(), Launch, Self, Self, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Ball->DirectDamage = DirectDamage;
			Ball->SplashDamage = SplashDamage;
			Ball->SplashRadius = SplashRadius;
			Ball->StatusEffect = UGE_Burn::StaticClass();
			Ball->StatusDamagePerTick = UGE_Burn::DamagePerTick;
			Ball->FinishSpawning(Launch);
		}
	}
	EndJutsu();
}
```

- [ ] **Step 10: Dodge cancels jutsu, and the player gets Fireball**

In `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp`, add after `CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);`:

```cpp
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
```

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`:
- add `#include "Shinobi/Abilities/GA_Fireball.h"`;
- change the startup list to:

```cpp
	StartupAbilities = {
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(),
		UGA_Fireball::StaticClass(),
	};
```

- [ ] **Step 11: Build and run all tests**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 30 TESTS PASSED`.

- [ ] **Step 12: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h Source/ShinobiCoop/Shinobi/ShinobiEffects.h Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.h Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.h Source/ShinobiCoop/Shinobi/Abilities/GA_Fireball.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiTestWorld.h Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: jutsu base (cost, wind-up, interrupt refund, cooldown) and Fireball

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Amaterasu

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

- [ ] **Step 1: Append the failing test to `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

Add `#include "Shinobi/Abilities/GA_Amaterasu.h"` to the includes. Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAmaterasuTest, "ShinobiCoop.Jutsu.AmaterasuBurnsCrosshairTargetOrFailsFree", ShinobiTestFlags)
bool FShinobiAmaterasuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(); // aims along +X
	AShinobiCharacterBase* Behind = W.Spawn<AShinobiCharacterBase>(FVector(-1000.f, 0.f, 0.f));
	AShinobiCharacterBase* TooFar = W.Spawn<AShinobiCharacterBase>(FVector(3500.f, 0.f, 0.f));
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	int32 Failures = 0;
	P->GetAbilitySystemComponent()->AbilityFailedCallbacks.AddLambda([&Failures](const UGameplayAbility*, const FGameplayTagContainer&) { ++Failures; });

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestFalse(TEXT("No enemy under the crosshair within 3000: fails"), ShinobiTest::IsActive(P, UGA_Amaterasu::StaticClass()));
	TestEqual(TEXT("Failed cast costs nothing"), ShinobiTest::Get(P, Chakra), 100.f);
	TestEqual(TEXT("Failure reported (HUD flash)"), Failures, 1);
	TestFalse(TEXT("No cooldown after a failed cast"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Amaterasu));

	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1500.f, 0.f, 0.f));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestTrue(TEXT("Casts on the enemy under the crosshair"), ShinobiTest::IsActive(P, UGA_Amaterasu::StaticClass()));
	TestEqual(TEXT("Cost 50"), ShinobiTest::Get(P, Chakra), 50.f);
	W.Tick(0.6f); // timers set before a tick start counting one tick late: the 0.5 s wind-up fires at ~0.55-0.6
	TestTrue(TEXT("Black flame after the 0.5 s wind-up"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Black));
	TestFalse(TEXT("Not a normal burn"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestTrue(TEXT("Cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Amaterasu));
	W.Tick(1.2f); // t = 1.8: exactly one tick (~1.6); the next is ~2.6
	TestEqual(TEXT("15 damage per second"), ShinobiTest::Get(E, Health), 485.f);
	TestEqual(TEXT("Enemy behind untouched"), ShinobiTest::Get(Behind, Health), 500.f);
	TestEqual(TEXT("Enemy beyond range untouched"), ShinobiTest::Get(TooFar, Health), 500.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Expected: FAIL. `Shinobi/Abilities/GA_Amaterasu.h` is not found.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Amaterasu.generated.h"

class AShinobiCharacterBase;

/** Slot 4: black flame on the enemy under the crosshair (within 3000): 15 damage per second for 10 s. With no
 * enemy there the cast fails before any cost is paid (the HUD flashes the slot). */
UCLASS()
class SHINOBICOOP_API UGA_Amaterasu : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Amaterasu();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 3000.f;

	/** Crosshair forgiveness: radius of the aim sweep. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRadius = 30.f;

protected:
	virtual bool PrepareJutsu() override;
	virtual void ExecuteJutsu() override;

private:
	TWeakObjectPtr<AShinobiCharacterBase> Target;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Amaterasu::UGA_Amaterasu()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	DisplayName = INVTEXT("Amaterasu");
	Glyph = INVTEXT("天"); // heaven
	ChakraCost = 50.f;
	Cooldown = 20.f;
	WindUp = 0.5f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Amaterasu);
}

bool UGA_Amaterasu::PrepareJutsu()
{
	Target = nullptr;
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	FHitResult Hit;
	if (Self && TraceAim(Range, AimRadius, Hit))
	{
		AShinobiCharacterBase* Candidate = Cast<AShinobiCharacterBase>(Hit.GetActor());
		if (Self->IsHostileTo(Candidate) && FVector::Dist(Self->GetActorLocation(), Candidate->GetActorLocation()) <= Range)
		{
			Target = Candidate;
		}
	}
	return Target.IsValid();
}

void UGA_Amaterasu::ExecuteJutsu()
{
	if (Target.IsValid() && HasAuthority(&CurrentActivationInfo))
	{
		ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Target.Get(), UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick);
	}
	EndJutsu();
}
```

- [ ] **Step 5: Grant it**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add `#include "Shinobi/Abilities/GA_Amaterasu.h"`, and add `UGA_Amaterasu::StaticClass(),` to the startup list after `UGA_Fireball::StaticClass(),`.

- [ ] **Step 6: Build and run all tests**

Expected: `ALL 31 TESTS PASSED`.

If the "Casts on the enemy" assertion fails but the rest of the suite passes, scene queries aren't working in the test world. Task 4's projectile test would then fail too. Report it rather than replacing the sweep.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.h Source/ShinobiCoop/Shinobi/Abilities/GA_Amaterasu.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: Amaterasu black flame on the crosshair target

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Chidori

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, `.cpp` (`FindNearestHostileInCone`)
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

- [ ] **Step 1: Append the failing tests to `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

Add `#include "Shinobi/Abilities/GA_Chidori.h"` to the includes. Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChidoriTest, "ShinobiCoop.Jutsu.ChidoriDashesToTargetAndShocks", ShinobiTestFlags)
bool FShinobiChidoriTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(); // aims along +X
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 100.f, 0.f)); // ~7 degrees off the aim
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 30"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 70.f);
	W.Tick(1.0f); // 0.6 s wind-up + ~0.18 s dash (706 units at 4000 u/s)
	TestFalse(TEXT("Chidori finished"), ShinobiTest::IsActive(P, UGA_Chidori::StaticClass()));
	const float Gap = FVector::Dist2D(P->GetActorLocation(), E->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Dashed up to the target (gap %.0f)"), Gap), Gap < 200.f);
	TestEqual(TEXT("120 damage"), ShinobiTest::Get(E, Health), 380.f);
	TestTrue(TEXT("Target stunned"), ShinobiTest::HasTag(E, ShinobiTags::State_Shocked));
	TestTrue(TEXT("Target carries the Shock bonus"), ShinobiTest::HasTag(E, ShinobiTags::State_ShockBonus));
	TestTrue(TEXT("Cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Chidori));

	W.Tick(1.6f);
	TestFalse(TEXT("Stun over after 1.5 s"), ShinobiTest::HasTag(E, ShinobiTags::State_Shocked));
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), E, 100.f);
	TestEqual(TEXT("Follow-up hit gets the +25% bonus"), ShinobiTest::Get(E, Health), 255.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChidoriTurnTest, "ShinobiCoop.Jutsu.ChidoriStrikesAlongDashWhenTurned", ShinobiTestFlags)
bool FShinobiChidoriTurnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 0.f, 0.f));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	W.Tick(0.75f); // mid-dash: the wind-up fires at ~0.65-0.7 and the ~0.18 s dash runs past 0.8
	TestTrue(TEXT("Still dashing"), ShinobiTest::IsActive(P, UGA_Chidori::StaticClass()));
	// Stand-in for held strafe input turning the character (orient-to-movement) during the dash.
	P->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	W.Tick(0.3f);
	TestEqual(TEXT("Strike still lands along the dash"), ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute()), 380.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChidoriNoTargetTest, "ShinobiCoop.Jutsu.ChidoriWithoutTargetDashesFullDistance", ShinobiTestFlags)
bool FShinobiChidoriNoTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Side = W.Spawn<AShinobiCharacterBase>(FVector(0.f, 800.f, 0.f)); // 90 degrees off: no snap

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	W.Tick(1.2f); // 0.6 s wind-up + 0.3 s dash
	const float X = P->GetActorLocation().X;
	TestTrue(FString::Printf(TEXT("Dashed ~1200 straight ahead (X = %.0f)"), X), X > 1100.f && X < 1300.f);
	TestEqual(TEXT("Enemy off to the side untouched"), ShinobiTest::Get(Side, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Expected: FAIL. `Shinobi/Abilities/GA_Chidori.h` is not found.

- [ ] **Step 3: Add `FindNearestHostileInCone` to the ability base**

In `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, after `FindHostilesInCone`:

```cpp
	/** The closest (2D) of FindHostilesInCone, or null. */
	AShinobiCharacterBase* FindNearestHostileInCone(float Range, float HalfAngleDegrees) const;
```

In `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp`, at the end:

```cpp
AShinobiCharacterBase* UShinobiGameplayAbility::FindNearestHostileInCone(float Range, float HalfAngleDegrees) const
{
	const TArray<AShinobiCharacterBase*> Candidates = FindHostilesInCone(Range, HalfAngleDegrees);
	AShinobiCharacterBase* Nearest = nullptr;
	if (Candidates.Num() == 0)
	{
		return Nearest;
	}
	// FindHostilesInCone only returns candidates when the owner exists.
	const FVector Origin = GetShinobiCharacter()->GetActorLocation();
	float NearestDistance = TNumericLimits<float>::Max();
	for (AShinobiCharacterBase* Candidate : Candidates)
	{
		const float Distance = FVector::Dist2D(Origin, Candidate->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Candidate;
		}
	}
	return Nearest;
}
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Chidori.generated.h"

/**
 * Slot 2: a lightning dash of up to 1200 that steers to the nearest enemy within 15 degrees of the aim (straight
 * ahead otherwise), then strikes: 120 damage, a 1.5 s stun and the Shock bonus. The dash is a root-motion force
 * like the dodge's, but started from the wind-up timer on each side independently (see the plan's dash-sync note).
 */
UCLASS()
class SHINOBICOOP_API UGA_Chidori : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Chidori();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float DashDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "1"))
	float DashSpeed = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SnapHalfAngle = 15.f;

	/** How far short of the target the dash stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float StopShort = 100.f;

	/** Reach of the strike at the end of the dash. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float StrikeRange = 200.f;

protected:
	virtual void ExecuteJutsu() override;

private:
	UFUNCTION()
	void OnDashFinished();

	/** Facing when the dash started; the strike uses it, since held move input turns the character mid-dash. */
	float DashYaw = 0.f;
};
```

- [ ] **Step 5: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Chidori::UGA_Chidori()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	DisplayName = INVTEXT("Chidori");
	Glyph = INVTEXT("雷"); // lightning
	ChakraCost = 30.f;
	Cooldown = 8.f;
	WindUp = 0.6f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Chidori);
}

void UGA_Chidori::ExecuteJutsu()
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		EndJutsu();
		return;
	}
	FaceAim(nullptr); // the camera may have turned during the wind-up
	float Distance = DashDistance;
	if (const AShinobiCharacterBase* Target = FindNearestHostileInCone(DashDistance, SnapHalfAngle))
	{
		FaceAim(Target);
		Distance = FMath::Max(0.f, FVector::Dist2D(Self->GetActorLocation(), Target->GetActorLocation()) - StopShort);
	}
	DashYaw = Self->GetActorRotation().Yaw;
	if (Distance < 1.f)
	{
		OnDashFinished();
		return;
	}
	UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Chidori"), Self->GetActorForwardVector().GetSafeNormal2D(), DashSpeed, Distance / DashSpeed, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	Dash->OnFinish.AddDynamic(this, &UGA_Chidori::OnDashFinished);
	Dash->ReadyForActivation();
}

void UGA_Chidori::OnDashFinished()
{
	// Strike along the dash, not the current facing: movement input held during the dash turns the character
	// (bOrientRotationToMovement), which would swing the strike cone off the target.
	if (AActor* Self = GetAvatarActorFromActorInfo())
	{
		Self->SetActorRotation(FRotator(0.f, DashYaw, 0.f));
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiCharacterBase* Victim = FindNearestHostileInCone(StrikeRange, 60.f);
		if (Victim && ApplyDamage(Victim, Damage))
		{
			// Bonus after the hit, so it boosts the follow-up rather than this strike.
			UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_Shock::StaticClass());
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_ShockBonus::StaticClass());
		}
	}
	EndJutsu();
}
```

- [ ] **Step 6: Grant it**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add `#include "Shinobi/Abilities/GA_Chidori.h"`. Change the jutsu line of the startup list to:

```cpp
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Amaterasu::StaticClass(),
```

- [ ] **Step 7: Build and run all tests**

Expected: `ALL 34 TESTS PASSED`.

- [ ] **Step 8: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.h Source/ShinobiCoop/Shinobi/Abilities/GA_Chidori.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: Chidori dash strike with shock

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Rasengan and Firestorm

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

- [ ] **Step 1: Append the failing tests to `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

Add `#include "Shinobi/Abilities/GA_Rasengan.h"` to the includes. Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiRasenganTest, "ShinobiCoop.Jutsu.RasenganFirestormOnBurningTarget", ShinobiTestFlags)
bool FShinobiRasenganTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	AShinobiCharacterBase* Near = W.Spawn<AShinobiCharacterBase>(FVector(400.f, 250.f, 0.f)); // 354 from E: in the Firestorm, out of reach
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 700.f, 0.f));  // 700 from E
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	ShinobiCombat::ApplyStatus(P->GetAbilitySystemComponent(), E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 35"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 65.f);
	W.Tick(0.9f); // the 0.8 s wind-up fires at ~0.85-0.9 (pre-tick timers start one tick late); the burn's first tick is ~1.05
	TestEqual(TEXT("Burning target: 150 + Firestorm 200"), ShinobiTest::Get(E, Health), 150.f);
	TestEqual(TEXT("Firestorm reaches enemies within 500 of the target"), ShinobiTest::Get(Near, Health), 300.f);
	TestEqual(TEXT("Firestorm stops at 500"), ShinobiTest::Get(Far, Health), 500.f);
	TestFalse(TEXT("Firestorm consumes the normal burn"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestTrue(TEXT("Cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Rasengan));
	W.Tick(0.1f); // LaunchCharacter's velocity applies on the next movement tick
	TestTrue(FString::Printf(TEXT("Knocked back (vx %.0f)"), E->GetVelocity().X), E->GetVelocity().X > 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiRasenganBlackFlameTest, "ShinobiCoop.Jutsu.RasenganKeepsBlackFlameAndNeedsBurn", ShinobiTestFlags)
bool FShinobiRasenganBlackFlameTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	// Two casters far apart, so each fights alone and both cast at once (separate cooldowns).
	AShinobiPlayerCharacter* P1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* BlackFlamed = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	AShinobiPlayerCharacter* P2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	AShinobiCharacterBase* NotBurning = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 3000.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	ShinobiCombat::ApplyStatus(P1->GetAbilitySystemComponent(), BlackFlamed, UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick);

	ShinobiTest::Tap(P1, EShinobiAbilityInputID::Jutsu3);
	ShinobiTest::Tap(P2, EShinobiAbilityInputID::Jutsu3);
	W.Tick(0.9f);
	TestEqual(TEXT("Black flame also sets off Firestorm"), ShinobiTest::Get(BlackFlamed, Health), 150.f);
	TestTrue(TEXT("Black flame survives Firestorm"), ShinobiTest::HasTag(BlackFlamed, ShinobiTags::State_Burning_Black));
	TestEqual(TEXT("Not burning: just the 150 hit"), ShinobiTest::Get(NotBurning, Health), 350.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Expected: FAIL. `Shinobi/Abilities/GA_Rasengan.h` is not found.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Rasengan.generated.h"

class AShinobiCharacterBase;

/**
 * Slot 3: a point-blank strike (reach 200): 150 damage and a heavy knockback. On a burning target (normal or
 * black flame) it sets off Firestorm: 200 damage to every enemy within 500 of the target, and the target's
 * normal burn is consumed (black flame stays).
 */
UCLASS()
class SHINOBICOOP_API UGA_Rasengan : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Rasengan();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 200.f;

	/** Target cone at point-blank range. Wider than Chidori's 15 degrees: at 200 units that would be only +-52. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float HalfAngle = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Knockback = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormDamage = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormRadius = 500.f;

protected:
	virtual void ExecuteJutsu() override;

private:
	void Firestorm(AShinobiCharacterBase* Center) const;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"

UGA_Rasengan::UGA_Rasengan()
{
	InputID = EShinobiAbilityInputID::Jutsu3;
	DisplayName = INVTEXT("Rasengan");
	Glyph = INVTEXT("螺"); // spiral
	ChakraCost = 35.f;
	Cooldown = 10.f;
	WindUp = 0.8f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Rasengan);
}

void UGA_Rasengan::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the wind-up
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	AShinobiCharacterBase* Victim = FindNearestHostileInCone(Range, HalfAngle);
	if (Self && Victim)
	{
		FaceAim(Victim);
		if (HasAuthority(&CurrentActivationInfo))
		{
			const bool bBurning = Victim->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning);
			if (ApplyDamage(Victim, Damage))
			{
				const FVector Push = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, 300.f);
				Victim->LaunchCharacter(Push, true, true);
				if (bBurning)
				{
					Firestorm(Victim);
				}
			}
		}
	}
	EndJutsu();
}

void UGA_Rasengan::Firestorm(AShinobiCharacterBase* Center) const
{
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	for (const AShinobiCharacterBase* Caught : ShinobiCombat::FindHostilesInRadius(GetShinobiCharacter(), Center->GetActorLocation(), FirestormRadius))
	{
		ShinobiCombat::ApplyDamage(Source, Caught, FirestormDamage);
	}
	Center->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning_Normal));
}
```

- [ ] **Step 5: Grant it**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add `#include "Shinobi/Abilities/GA_Rasengan.h"`. Change the jutsu line to:

```cpp
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
```

- [ ] **Step 6: Build and run all tests**

Expected: `ALL 36 TESTS PASSED`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.h Source/ShinobiCoop/Shinobi/Abilities/GA_Rasengan.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: Rasengan with Firestorm reaction on burning targets

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Substitution

A perfect dodge: a hit lands during the dodge's i-frames while the player has a charge. The damage exec (Task 1) already negates the hit and sends `Event.Substitution` to the target, naming the attacker. `GA_Substitution` is triggered by that event on the server, and then:
- spends a charge;
- cancels the dodge (carry-forward note: otherwise the dodge's root-motion force keeps pushing);
- teleports 150 behind the attacker, facing them;
- staggers the attacker: 0.5 s `GE_Stagger`, plus a stagger event so their wind-up or charge is interrupted too.

Charges come back at +1 every 15 s.

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `.cpp`
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`

- [ ] **Step 1: Append the failing test to `Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp`**

Add `#include "Shinobi/Abilities/GA_Substitution.h"` to the includes. Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSubstitutionTest, "ShinobiCoop.Jutsu.SubstitutionTeleportsBehindAttacker", ShinobiTestFlags)
bool FShinobiSubstitutionTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f)); // faces P
	UAbilitySystemComponent* EnemySource = E->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Charges = UShinobiAttributeSet::GetSubstitutionChargesAttribute();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Perfect dodge: the hit doesn't land"), ShinobiCombat::ApplyDamage(EnemySource, P, 50.f));
	TestEqual(TEXT("No damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("One charge spent"), ShinobiTest::Get(P, Charges), 2.f);
	TestFalse(TEXT("Dodge cancelled"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	const FVector Behind(450.f, 0.f, 0.f); // 150 behind E, which faces -X
	TestTrue(FString::Printf(TEXT("Teleported behind the attacker (at %s)"), *P->GetActorLocation().ToString()), FVector::Dist2D(P->GetActorLocation(), Behind) < 50.f);
	TestTrue(FString::Printf(TEXT("Facing the attacker (yaw %.0f)"), P->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(P->GetActorRotation().Yaw - 180.f)) < 5.f);
	TestTrue(TEXT("Attacker staggered"), ShinobiTest::HasTag(E, ShinobiTags::State_Staggered));
	W.Tick(0.6f);
	TestFalse(TEXT("Stagger over after 0.5 s"), ShinobiTest::HasTag(E, ShinobiTags::State_Staggered));

	// Out of charges: the dodge still avoids the hit, but nobody teleports.
	ShinobiTest::Set(P, Charges, 0.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	const FVector Before = P->GetActorLocation();
	ShinobiCombat::ApplyDamage(EnemySource, P, 50.f);
	TestEqual(TEXT("Out of charges: still no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestTrue(TEXT("Out of charges: no teleport"), P->GetActorLocation().Equals(Before));
	TestEqual(TEXT("Charges stay at 0"), ShinobiTest::Get(P, Charges), 0.f);

	W.Tick(15.1f); // the regen started at spawn: +1 at 15 s
	TestEqual(TEXT("+1 charge every 15 s"), ShinobiTest::Get(P, Charges), 1.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Expected: FAIL. `Shinobi/Abilities/GA_Substitution.h` is not found.

- [ ] **Step 3: Add the Substitution effects to `Source/ShinobiCoop/Shinobi/ShinobiEffects.h` / `.cpp`**

Header, appended:

```cpp
/** Substitution cost: -1 charge (used as the ability's cost, so it can't fire at 0). */
UCLASS()
class SHINOBICOOP_API UGE_SubstitutionCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SubstitutionCost();
};

/** +1 Substitution charge every 15 s. Applied at spawn to players. */
UCLASS()
class SHINOBICOOP_API UGE_SubstitutionRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SubstitutionRegen();
};
```

Source, appended:

```cpp
UGE_SubstitutionCost::UGE_SubstitutionCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddModifier(UShinobiAttributeSet::GetSubstitutionChargesAttribute(), -1.f);
}

UGE_SubstitutionRegen::UGE_SubstitutionRegen()
{
	SetInfinitePeriodic(15.f);
	AddModifier(UShinobiAttributeSet::GetSubstitutionChargesAttribute(), 1.f);
	bExecutePeriodicEffectOnApplication = false; // a fresh spawn already has full charges
}
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Substitution.generated.h"

/**
 * Perfect dodge. Triggered on the server by Event.Substitution, which UShinobiDamageExec sends when a hit lands
 * during dodge i-frames and a charge is left. Spends a charge, cancels the dodge, teleports behind the attacker
 * facing them (clients get the corrected position), and staggers the attacker for 0.5 s. The log and smoke left
 * behind are M2b's cue.
 */
UCLASS()
class SHINOBICOOP_API UGA_Substitution : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Substitution();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float BehindDistance = 150.f;
};
```

- [ ] **Step 5: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Substitution.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Substitution::UGA_Substitution()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Substitution));
	// Otherwise the dodge's root-motion force keeps pushing the teleported character.
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Dodge);
	CostGameplayEffectClass = UGE_SubstitutionCost::StaticClass();

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ShinobiTags::Event_Substitution;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_Substitution::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	const AShinobiCharacterBase* Attacker = TriggerEventData ? Cast<AShinobiCharacterBase>(TriggerEventData->Instigator.Get()) : nullptr;
	if (!Self || !Attacker || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Behind = Attacker->GetActorLocation() - Attacker->GetActorForwardVector().GetSafeNormal2D() * BehindDistance;
	const FRotator FaceAttacker(0.f, (Attacker->GetActorLocation() - Behind).Rotation().Yaw, 0.f);
	Self->TeleportTo(Behind, FaceAttacker); // stays put if something blocks the spot; the hit is negated either way

	ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Attacker, UGE_Stagger::StaticClass());
	FGameplayEventData Stagger;
	Stagger.EventTag = ShinobiTags::Event_Hit_Stagger;
	Stagger.Instigator = Self;
	Stagger.Target = Attacker;
	Attacker->GetAbilitySystemComponent()->HandleGameplayEvent(Stagger.EventTag, &Stagger); // interrupts their wind-up or charge too

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

- [ ] **Step 6: Grant it and the regen**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add `#include "Shinobi/Abilities/GA_Substitution.h"` and `#include "Shinobi/ShinobiEffects.h"`. Replace the startup list:

```cpp
	StartupAbilities = {
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(),
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
		UGA_Substitution::StaticClass(),
	};
	StartupEffects.Add(UGE_SubstitutionRegen::StaticClass());
```

- [ ] **Step 7: Build and run all tests**

Expected: `ALL 37 TESTS PASSED`.

Task 1's `InvulnerableNegatesAndRequestsSubstitution` now runs a real Substitution on its first hit (P teleports behind E and E is staggered). It must still pass unchanged:
- every assertion there is about damage and event counts;
- charges are set to 0 explicitly before the "out of charges" part.

- [ ] **Step 8: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiEffects.h Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.h Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiJutsuTests.cpp
git commit -m "feat: perfect-dodge Substitution with charges and regen

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Jutsu input (keys 1–4, gamepad RB + face buttons)

The keyboard number keys go into `IMC_Shinobi`. The gamepad face buttons go into a second context, `IMC_ShinobiJutsu`, which the player adds only while RB is held, at a priority above `IMC_Shinobi`. `UInputAction::bConsumeInput` defaults to true, so while RB is held, A/B/X/Y cast jutsu instead of jumping, dodging and punching (spec §4).

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`

- [ ] **Step 1: Update the test `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`**

Replace the `for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : Config->AbilityActions)` loop (the "exactly two keys" check) with:

```cpp
	const TArray<FEnhancedActionKeyMapping>& JutsuMappings = Config->JutsuContext->GetMappings();
	const auto CountFor = [](const TArray<FEnhancedActionKeyMapping>& List, const UInputAction* Action)
	{
		int32 Count = 0;
		for (const FEnhancedActionKeyMapping& Mapping : List)
		{
			Count += Mapping.Action == Action ? 1 : 0;
		}
		return Count;
	};
	int32 JutsuActions = 0;
	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : Config->AbilityActions)
	{
		// Jutsu: a number key in IMC_Shinobi, a face button in IMC_ShinobiJutsu (active only while RB is held).
		// Everything else: keyboard + gamepad in IMC_Shinobi.
		const bool bJutsu = Pair.Key >= EShinobiAbilityInputID::Jutsu1;
		JutsuActions += bJutsu ? 1 : 0;
		const int32 ID = static_cast<int32>(Pair.Key);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_Shinobi"), ID), CountFor(Mappings, Pair.Value), bJutsu ? 1 : 2);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_ShinobiJutsu"), ID), CountFor(JutsuMappings, Pair.Value), bJutsu ? 1 : 0);
	}
	TestEqual(TEXT("Four jutsu actions"), JutsuActions, 4);
	TestNotNull(TEXT("RB holds the jutsu modifier"), FindMapping(Mappings, Config->JutsuModifier, EKeys::Gamepad_RightShoulder));
	TestNotNull(TEXT("Key 1 casts jutsu 1"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu1], EKeys::One));
	TestNotNull(TEXT("Key 4 casts jutsu 4"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu4], EKeys::Four));
	TestNotNull(TEXT("RB + A casts jutsu 1"), FindMapping(JutsuMappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu1], EKeys::Gamepad_FaceButton_Bottom));
	TestNotNull(TEXT("RB + Y casts jutsu 4"), FindMapping(JutsuMappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu4], EKeys::Gamepad_FaceButton_Top));
```

- [ ] **Step 2: Build to see it fail**

Expected: FAIL. `JutsuContext` / `JutsuModifier` are not members of `UShinobiInputConfig`.

- [ ] **Step 3: Update `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h`**

Replace the priority comment and constant block with:

```cpp
/** Priority IMC_Shinobi is added at. */
constexpr int32 ShinobiInputPriority = 1;

/** IMC_ShinobiJutsu goes above IMC_Shinobi so, while RB is held, its face buttons win over jump/dodge/melee. */
constexpr int32 ShinobiJutsuInputPriority = ShinobiInputPriority + 1;
```

Add these properties after `Jump`:

```cpp
	/** Gamepad RB: while held, the player adds JutsuContext. */
	UPROPERTY()
	TObjectPtr<UInputAction> JutsuModifier;

	/** Gamepad A/B/X/Y -> Jutsu 1-4. Only active while RB is held. */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> JutsuContext;
```

- [ ] **Step 4: Build the jutsu mappings in `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.cpp`**

Before `return Config;`, add:

```cpp
	// Jutsu 1-4: number keys here; the gamepad face buttons live in JutsuContext, which the player adds while RB is held.
	Config->JutsuModifier = MakeAction(Config, TEXT("IA_JutsuModifier"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->JutsuModifier, EKeys::Gamepad_RightShoulder);
	UInputMappingContext* JutsuCtx = NewObject<UInputMappingContext>(Config, TEXT("IMC_ShinobiJutsu"));
	Config->JutsuContext = JutsuCtx;
	const auto AddJutsu = [Config, Ctx, JutsuCtx](EShinobiAbilityInputID ID, FName Name, const FKey& Key, const FKey& PadKey)
	{
		UInputAction* Action = MakeAction(Config, Name, EInputActionValueType::Boolean);
		Ctx->MapKey(Action, Key);
		JutsuCtx->MapKey(Action, PadKey);
		Config->AbilityActions.Add(ID, Action);
	};
	AddJutsu(EShinobiAbilityInputID::Jutsu1, TEXT("IA_Jutsu1"), EKeys::One, EKeys::Gamepad_FaceButton_Bottom);
	AddJutsu(EShinobiAbilityInputID::Jutsu2, TEXT("IA_Jutsu2"), EKeys::Two, EKeys::Gamepad_FaceButton_Right);
	AddJutsu(EShinobiAbilityInputID::Jutsu3, TEXT("IA_Jutsu3"), EKeys::Three, EKeys::Gamepad_FaceButton_Left);
	AddJutsu(EShinobiAbilityInputID::Jutsu4, TEXT("IA_Jutsu4"), EKeys::Four, EKeys::Gamepad_FaceButton_Top);
```

- [ ] **Step 5: Bind RB in the player**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, add to the private section after `AbilityInputReleased`:

```cpp
	/** RB held: adds IMC_ShinobiJutsu (A/B/X/Y cast jutsu); released: removes it. */
	void SetJutsuModifier(bool bHeld);
```

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`, add two bindings at the end of `SetupPlayerInputComponent`, after the ability loop. The loop already binds the jutsu actions, because they're in `AbilityActions`.

```cpp
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::SetJutsuModifier, true);
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::SetJutsuModifier, false);
```

Then add after `AbilityInputReleased`:

```cpp
void AShinobiPlayerCharacter::SetJutsuModifier(bool bHeld)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem)
	{
		return;
	}
	const UInputMappingContext* JutsuContext = UShinobiInputConfig::Get().JutsuContext;
	if (bHeld)
	{
		Subsystem->AddMappingContext(JutsuContext, ShinobiJutsuInputPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(JutsuContext);
	}
}
```

Add `#include "InputMappingContext.h"` if the compiler asks for the full type.

- [ ] **Step 6: Build and run all tests**

Expected: `ALL 37 TESTS PASSED`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h Source/ShinobiCoop/Shinobi/ShinobiInputConfig.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiInputTests.cpp
git commit -m "feat: jutsu input on keys 1-4 and gamepad RB + face buttons

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: HUD jutsu slots and Substitution pips

**What the HUD adds (spec §4):**
- **Four slots, bottom-center.** Each shows the jutsu's kanji, its key label and its chakra cost.
- **Cooldown:** a dark shade whose height is the remaining share of the cooldown, plus whole seconds left. This stands in for the spec's radial sweep; M5 can swap in a radial material.
- **Unaffordable:** the glyph is dimmed.
- **Failed cast:** a red flash for 0.3 s. It covers cooldown, cost, blocked activation and Amaterasu with no target, through the ability system's `AbilityFailedCallbacks`.
- **Substitution pips:** three pips under the stamina bar.

Widgets need a local player, so there's no automation test here. The slot data comes from `GetSlotState`, which Task 5 tests. The look is checked with the `-ShinobiShot` screenshot harness, which gets a HUD shot in this task.

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`, `.cpp` (full replacement)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, `.cpp` (`RunShotTest`)
- Modify: `Scripts/shot_chakra.ps1` (comment only)

- [ ] **Step 1: Replace `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UCanvasPanel;
class UGameplayAbility;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FGameplayTagContainer;

/** Health / chakra / stamina bars, Substitution pips and the four jutsu slots. Built in code; polled each tick. */
UCLASS()
class SHINOBICOOP_API UShinobiHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 NumJutsuSlots = 4;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UProgressBar* AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height);
	void BuildSubstitutionPips(UVerticalBox* Bars);
	void BuildJutsuSlots(UCanvasPanel* Root);
	UTextBlock* AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint);
	void UpdateJutsuSlots(const UAbilitySystemComponent& ASC);

	/** Binds the red "can't cast" flash to the current pawn's ability system; rebinds when the pawn changes. */
	void BindFailureFlash(UAbilitySystemComponent* ASC);
	void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons);

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SubstitutionPips;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotFrames;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotGlyphs;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCosts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCooldownTexts;

	/** Dark overlay whose height is the remaining share of the cooldown. */
	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> SlotShades;

	/** World time until which each slot shows the red flash. */
	TArray<double> SlotFlashUntil;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle FailedHandle;
};
```

- [ ] **Step 2: Replace `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.cpp`**

```cpp
#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"

namespace
{
	const FLinearColor HealthColor(0.2f, 0.85f, 0.3f);
	const FLinearColor LowHealthColor(0.9f, 0.15f, 0.1f);
	const FLinearColor ChakraColor(0.2f, 0.5f, 1.f);
	const FLinearColor StaminaColor(1.f, 0.8f, 0.2f);
	const FLinearColor PipOnColor(0.85f, 0.6f, 0.3f); // a wooden log
	const FLinearColor PipOffColor(0.15f, 0.15f, 0.15f, 0.6f);
	const FLinearColor SlotColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor SlotFlashColor(0.8f, 0.05f, 0.05f, 0.85f);
	const FLinearColor ShadeColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor KeyColor(0.8f, 0.8f, 0.8f);
	const FLinearColor CostColor(0.45f, 0.7f, 1.f);
	constexpr float SlotSize = 76.f;
	constexpr double FlashSeconds = 0.3;
	constexpr int32 NumPips = 3; // MaxSubstitutionCharges

	int32 JutsuSlotIndex(EShinobiAbilityInputID InputID)
	{
		return static_cast<int32>(InputID) - static_cast<int32>(EShinobiAbilityInputID::Jutsu1);
	}
}

TSharedRef<SWidget> UShinobiHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		// Read-only HUD; never eat cursor/click input meant for the game or other widgets.
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Bars"));
		UCanvasPanelSlot* BarsSlot = Root->AddChildToCanvas(Bars);
		BarsSlot->SetAnchors(FAnchors(0.f, 1.f));
		BarsSlot->SetAlignment(FVector2D(0.f, 1.f));
		BarsSlot->SetPosition(FVector2D(40.f, -40.f));
		BarsSlot->SetAutoSize(true);

		HealthBar = AddBar(Bars, TEXT("HealthBar"), HealthColor, 22.f);
		ChakraBar = AddBar(Bars, TEXT("ChakraBar"), ChakraColor, 16.f);
		StaminaBar = AddBar(Bars, TEXT("StaminaBar"), StaminaColor, 10.f);
		BuildSubstitutionPips(Bars);
		BuildJutsuSlots(Root);
	}
	return Super::RebuildWidget();
}

UProgressBar* UShinobiHUDWidget::AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	// Without an explicit width, SProgressBar::ComputeDesiredSize falls back to the 20x12 marquee image
	// (the runtime UMG style's Common/ProgressBar_Marquee brush, set up in UMGCoreStyle.cpp), so the
	// auto-sized canvas slot above would otherwise collapse to ~20 px wide.
	SizeBox->SetWidthOverride(380.f);
	SizeBox->SetHeightOverride(Height);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	Bar->SetFillColorAndOpacity(Color);
	SizeBox->AddChild(Bar);
	UVerticalBoxSlot* BarSlot = Box->AddChildToVerticalBox(SizeBox);
	BarSlot->SetPadding(FMargin(0.f, 2.f));
	return Bar;
}

void UShinobiHUDWidget::BuildSubstitutionPips(UVerticalBox* Bars)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SubstitutionPips"));
	Bars->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	for (int32 Index = 0; Index < NumPips; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(16.f);
		Box->SetHeightOverride(16.f);
		UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pip->SetBrushColor(PipOffColor);
		Box->AddChild(Pip);
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		SubstitutionPips.Add(Pip);
	}
}

void UShinobiHUDWidget::BuildJutsuSlots(UCanvasPanel* Root)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JutsuSlots"));
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RowSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RowSlot->SetPosition(FVector2D(0.f, -40.f));
	RowSlot->SetAutoSize(true);

	static const TCHAR* KeyLabels[NumJutsuSlots] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };
	for (int32 Index = 0; Index < NumJutsuSlots; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(SlotSize);
		Box->SetHeightOverride(SlotSize);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(SlotColor);
		Frame->SetPadding(FMargin(0.f));
		Box->AddChild(Frame);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Frame->SetContent(Layers);

		// Back to front: glyph, cooldown shade, cooldown seconds, key, cost.
		UTextBlock* Glyph = AddLabel(Layers, 34, HAlign_Center, VAlign_Center, FLinearColor::White);
		USizeBox* Shade = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Shade->SetHeightOverride(0.f);
		UBorder* ShadeFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ShadeFill->SetBrushColor(ShadeColor);
		Shade->AddChild(ShadeFill);
		UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Bottom);
		UTextBlock* CooldownText = AddLabel(Layers, 22, HAlign_Center, VAlign_Center, FLinearColor::White);
		AddLabel(Layers, 12, HAlign_Left, VAlign_Top, KeyColor)->SetText(FText::FromString(KeyLabels[Index]));
		UTextBlock* CostText = AddLabel(Layers, 12, HAlign_Right, VAlign_Bottom, CostColor);

		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(4.f, 0.f));
		SlotFrames.Add(Frame);
		SlotGlyphs.Add(Glyph);
		SlotShades.Add(Shade);
		SlotCooldownTexts.Add(CooldownText);
		SlotCosts.Add(CostText);
	}
	SlotFlashUntil.Init(0.0, NumJutsuSlots);
}

UTextBlock* UShinobiHUDWidget::AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo LabelFont = Label->GetFont();
	LabelFont.Size = FontSize;
	Label->SetFont(LabelFont);
	Label->SetColorAndOpacity(FSlateColor(Tint));
	UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HorizontalAlign);
	LabelSlot->SetVerticalAlignment(VerticalAlign);
	LabelSlot->SetPadding(FMargin(4.f, 2.f));
	return Label;
}

void UShinobiHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const AShinobiCharacterBase* Pawn = Cast<AShinobiCharacterBase>(GetOwningPlayerPawn());
	const UShinobiAttributeSet* Stats = Pawn ? Pawn->GetAttributes() : nullptr;
	UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	BindFailureFlash(ASC);
	if (!Stats || !ASC || !HealthBar)
	{
		return;
	}
	const float HealthRatio = Stats->GetHealth() / FMath::Max(Stats->GetMaxHealth(), 1.f);
	HealthBar->SetPercent(HealthRatio);
	HealthBar->SetFillColorAndOpacity(HealthRatio < 0.3f ? LowHealthColor : HealthColor);
	ChakraBar->SetPercent(Stats->GetChakra() / FMath::Max(Stats->GetMaxChakra(), 1.f));
	StaminaBar->SetPercent(Stats->GetStamina() / FMath::Max(Stats->GetMaxStamina(), 1.f));

	const int32 Charges = FMath::RoundToInt(Stats->GetSubstitutionCharges());
	for (int32 Index = 0; Index < SubstitutionPips.Num(); ++Index)
	{
		SubstitutionPips[Index]->SetBrushColor(Index < Charges ? PipOnColor : PipOffColor);
	}
	UpdateJutsuSlots(*ASC);
}

void UShinobiHUDWidget::UpdateJutsuSlots(const UAbilitySystemComponent& ASC)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// ponytail: per-frame poll of the granted abilities, like the bars; fine for a handful of specs
	for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
	{
		const UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Spec.Ability);
		const int32 Index = Jutsu ? JutsuSlotIndex(Jutsu->InputID) : INDEX_NONE;
		if (!SlotFrames.IsValidIndex(Index))
		{
			continue;
		}
		const FShinobiJutsuSlotState State = Jutsu->GetSlotState(ASC);
		const bool bCooling = State.CooldownRemaining > 0.f && State.CooldownDuration > 0.f;
		SlotGlyphs[Index]->SetText(Jutsu->Glyph);
		SlotGlyphs[Index]->SetRenderOpacity(State.bAffordable ? 1.f : 0.35f);
		SlotCosts[Index]->SetText(FText::AsNumber(FMath::RoundToInt(Jutsu->ChakraCost)));
		SlotCooldownTexts[Index]->SetText(bCooling ? FText::AsNumber(FMath::CeilToInt(State.CooldownRemaining)) : FText::GetEmpty());
		SlotShades[Index]->SetHeightOverride(bCooling ? SlotSize * State.CooldownRemaining / State.CooldownDuration : 0.f);
		SlotFrames[Index]->SetBrushColor(Now < SlotFlashUntil[Index] ? SlotFlashColor : SlotColor);
	}
}

void UShinobiHUDWidget::BindFailureFlash(UAbilitySystemComponent* ASC)
{
	if (BoundASC == ASC)
	{
		return;
	}
	if (UAbilitySystemComponent* Old = BoundASC.Get())
	{
		Old->AbilityFailedCallbacks.Remove(FailedHandle);
	}
	BoundASC = ASC;
	FailedHandle = ASC ? ASC->AbilityFailedCallbacks.AddUObject(this, &UShinobiHUDWidget::OnAbilityFailed) : FDelegateHandle();
}

void UShinobiHUDWidget::OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons)
{
	const UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Ability);
	const int32 Index = Jutsu ? JutsuSlotIndex(Jutsu->InputID) : INDEX_NONE;
	if (SlotFlashUntil.IsValidIndex(Index) && GetWorld())
	{
		SlotFlashUntil[Index] = GetWorld()->GetTimeSeconds() + FlashSeconds;
	}
}

void UShinobiHUDWidget::NativeDestruct()
{
	BindFailureFlash(nullptr);
	Super::NativeDestruct();
}
```

- [ ] **Step 3: Add a HUD shot to `-ShinobiShot`**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, replace the `RunShotTest` comment with:

```cpp
	/** With -ShinobiShot: turn the camera to face the pawn, hold chakra charge (screenshots), release, then throw a
	 * Fireball and retry it on cooldown for a HUD screenshot (cooldown + red flash), quit. Used by Scripts/shot_chakra.ps1. */
```

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp`, replace the whole `RunShotTest` function with:

```cpp
void AShinobiPlayerController::RunShotTest()
{
	// ponytail: fixed delays; raise them if shaders are still compiling when the first shot lands
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Input = [this](EShinobiAbilityInputID InputID, bool bPressed)
	{
		if (AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>())
		{
			const int32 ID = static_cast<int32>(InputID);
			bPressed ? Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(ID)
			         : Shinobi->GetAbilitySystemComponent()->AbilityLocalInputReleased(ID);
		}
	};
	auto Shot = [](const TCHAR* Name, bool bShowUI)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), bShowUI, false);
	};
	At(4.f, [this]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			SetControlRotation(FRotator(-10.f, Shinobi->GetActorRotation().Yaw + 180.f, 0.f)); // camera in front, looking back
		}
	});
	At(5.f, [Shot]() { Shot(TEXT("ShinobiShot_0_Idle"), false); });
	At(5.5f, [Input]() { Input(EShinobiAbilityInputID::ChakraCharge, true); });
	At(6.f, [Shot]() { Shot(TEXT("ShinobiShot_1_Charge"), false); });
	At(7.f, [Shot]() { Shot(TEXT("ShinobiShot_2_Charge"), false); });
	At(8.f, [Shot]() { Shot(TEXT("ShinobiShot_3_Charge"), false); });
	At(8.2f, [Input]() { Input(EShinobiAbilityInputID::ChakraCharge, false); });
	At(8.8f, [Shot]() { Shot(TEXT("ShinobiShot_4_Released"), false); });
	// HUD: throw a Fireball (slot 1 goes on cooldown), then retry at once so slot 1 flashes red.
	At(9.f, [Input]() { Input(EShinobiAbilityInputID::Jutsu1, true); Input(EShinobiAbilityInputID::Jutsu1, false); });
	At(9.6f, [Input]() { Input(EShinobiAbilityInputID::Jutsu1, true); Input(EShinobiAbilityInputID::Jutsu1, false); });
	At(9.7f, [Shot]() { Shot(TEXT("ShinobiShot_5_Hud"), true); });
	At(10.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}
```

In `Scripts/shot_chakra.ps1`, change the second comment line to:
`# chakra charge, takes screenshots (idle, 3x charging, released), throws a Fireball for a HUD shot, and quits.`

- [ ] **Step 4: Build, run all tests, take the HUD screenshot**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
powershell -ExecutionPolicy Bypass -File Scripts\shot_chakra.ps1
```
Expected: `ALL 37 TESTS PASSED`, and six screenshot paths including `ShinobiShot_5_Hud.png`.

Open `Saved\Screenshots\WindowsEditor\ShinobiShot_5_Hud.png` with the Read tool and check:
- the bars and three log pips are at the bottom left;
- the four slots are at the bottom center, showing the glyphs 火 雷 螺 天, the key numbers and the costs 20/30/35/50;
- slot 1 is shaded with `4` and has a red frame (the flash).

If the kanji render as boxes, the default font has no CJK fallback in this build. Report it rather than changing fonts; the M2b plan will pick a font.

- [ ] **Step 5: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp Scripts/shot_chakra.ps1
git commit -m "feat: HUD jutsu slots with cooldown, cost and failure flash; substitution pips

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: Dummy status text, dummy attacks, cast cheat, smoke test

These dev aids make M2a checkable by hand before M2b's VFX and M3's enemies:
- dummies list their statuses under their HP;
- `Shinobi.DummyAttack 1` makes them punch the nearest player (telegraphed);
- `ShinobiCast <1-4>` taps a jutsu from the console.

The two-process smoke test now also casts a Fireball from the client, and checks that the client sees the dummy damaged and burning.

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h`, `.cpp` (full replacement)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, `.cpp`
- Modify: `Scripts/smoke_m1.ps1`
- Modify: `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp`

- [ ] **Step 1: Append the failing test to `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp`**

Add these includes after the existing ones:

```cpp
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
```

Before the final `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDummyAttackTest, "ShinobiCoop.Core.DummyAttackCheatPunchesNearestPlayer", ShinobiTestFlags)
bool FShinobiDummyAttackTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* DummyAttack = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.DummyAttack"));
	if (!TestNotNull(TEXT("Shinobi.DummyAttack exists"), DummyAttack))
	{
		return false;
	}
	DummyAttack->Set(true, ECVF_SetByCode);
	ON_SCOPE_EXIT { DummyAttack->Set(false, ECVF_SetByCode); };

	FShinobiTestWorld W;
	W.Spawn<AShinobiTrainingDummy>();
	AShinobiPlayerCharacter* Near = W.Spawn<AShinobiPlayerCharacter>(FVector(200.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Far = W.Spawn<AShinobiPlayerCharacter>(FVector(600.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	W.Tick(1.9f);
	TestEqual(TEXT("No punch before 2 s"), ShinobiTest::Get(Near, Health), 500.f);
	W.Tick(0.2f);
	TestEqual(TEXT("Nearest player within reach punched for 25"), ShinobiTest::Get(Near, Health), 475.f);
	TestEqual(TEXT("Player out of reach untouched"), ShinobiTest::Get(Far, Health), 500.f);
	return true;
}
```

- [ ] **Step 2: Build and run to see it fail**

Expected: the build succeeds and the new test FAILS on `Shinobi.DummyAttack exists`.

- [ ] **Step 3: Replace `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiTrainingDummy.generated.h"

class UTextRenderComponent;

/**
 * Enemy-team punching bag. Shows its health (and, until M2b's cue VFX, its statuses) above its head and resets
 * 1.5 s after being defeated. With the dev cvar Shinobi.DummyAttack on, it punches the nearest player within reach
 * every 2 s, so Substitution, jutsu interrupts and the chakra-charge cancel can be tried before M3's enemies.
 */
UCLASS()
class SHINOBICOOP_API AShinobiTrainingDummy : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float PunchInterval = 2.f;
	static constexpr float PunchTelegraph = 0.4f;
	static constexpr float PunchReach = 250.f;
	static constexpr float PunchDamage = 25.f;

protected:
	virtual void HandleOutOfHealth() override;

private:
	void ResetHealth();

	/** Rewrites HealthText only when the rounded HP, max HP or status list changed. */
	void RefreshText();

	/** Server only: PunchDamage to the nearest hostile character within PunchReach. */
	void Punch();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UTextRenderComponent> HealthText;

	FTimerHandle ResetTimer;

	/** Last values written to HealthText, so SetText (which always marks render state dirty) only runs on a change. */
	int32 DisplayedHealth = -1;
	int32 DisplayedMaxHealth = -1;
	FString DisplayedStatuses;
	bool bDisplayedTelegraph = false;

	/** Time since the last punch while Shinobi.DummyAttack is on. */
	float PunchClock = 0.f;
};
```

- [ ] **Step 4: Replace `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.cpp`**

```cpp
#include "Shinobi/ShinobiTrainingDummy.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDummyAttack(
		TEXT("Shinobi.DummyAttack"),
		false,
		TEXT("Dev aid until M3's enemies: training dummies punch the nearest player within reach every 2 s for 25 damage; ")
		TEXT("their text turns yellow just before. For trying Substitution, jutsu interrupts and the chakra-charge cancel."));
}

AShinobiTrainingDummy::AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EShinobiTeam::Enemies;
	AutoPossessAI = EAutoPossessAI::Disabled;

	HealthText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);
	HealthText->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthText->SetHorizontalAlignment(EHTA_Center);
	HealthText->SetWorldSize(28.f);
	HealthText->SetTextRenderColor(FColor::Red);

	UseMannequinMesh();

	// Attributes is constructed by AShinobiCharacterBase's constructor, which has already run at this
	// point, so its defaults are readable here. Avoids showing the widget's default "Text" for a frame.
	DisplayedHealth = FMath::RoundToInt(Attributes->GetHealth());
	DisplayedMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d"), DisplayedHealth, DisplayedMaxHealth)));
}

void AShinobiTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock runs on every machine so clients see the telegraph too (PIE windows share the cvar); only the server punches.
	PunchClock = CVarDummyAttack.GetValueOnGameThread() ? PunchClock + DeltaSeconds : 0.f;
	if (PunchClock >= PunchInterval)
	{
		PunchClock = 0.f;
		if (HasAuthority())
		{
			Punch();
		}
	}
	const bool bTelegraph = PunchClock >= PunchInterval - PunchTelegraph;
	if (bTelegraph != bDisplayedTelegraph)
	{
		bDisplayedTelegraph = bTelegraph;
		HealthText->SetTextRenderColor(bTelegraph ? FColor::Yellow : FColor::Red);
	}

	RefreshText();

	// TextRender geometry lies flat facing its local +X axis, so it's mirrored/culled when viewed from
	// behind. Face the local camera (yaw only) on machines that have one. GetFirstPlayerController()
	// would return any PC in the world -- on a listen server that includes remote clients' PCs, which
	// all have a camera manager but aren't local -- so use the game instance's local-player lookup
	// instead, which is null on a dedicated server and in the test world.
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector CameraLocation = CameraManager->GetCameraLocation();
		const FVector TextLocation = HealthText->GetComponentLocation();
		HealthText->SetWorldRotation(FRotator(0.f, (CameraLocation - TextLocation).Rotation().Yaw, 0.f));
	}
}

void AShinobiTrainingDummy::RefreshText()
{
	const int32 CurrentHealth = FMath::RoundToInt(Attributes->GetHealth());
	const int32 CurrentMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	// ponytail: status text until M2b's cues show them on the body
	FString Statuses;
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Burning_Normal))
	{
		Statuses += TEXT(" [Burn]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Burning_Black))
	{
		Statuses += TEXT(" [Black Flame]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Shocked))
	{
		Statuses += TEXT(" [Shock]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ShockBonus))
	{
		Statuses += TEXT(" [+25%]");
	}
	if (CurrentHealth != DisplayedHealth || CurrentMaxHealth != DisplayedMaxHealth || Statuses != DisplayedStatuses)
	{
		DisplayedHealth = CurrentHealth;
		DisplayedMaxHealth = CurrentMaxHealth;
		DisplayedStatuses = Statuses;
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d%s"), DisplayedHealth, DisplayedMaxHealth, *DisplayedStatuses)));
	}
}

void AShinobiTrainingDummy::Punch()
{
	AShinobiCharacterBase* Nearest = nullptr;
	float NearestDistance = PunchReach;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (IsHostileTo(*It) && Distance <= NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	ShinobiCombat::ApplyDamage(AbilitySystem, Nearest, PunchDamage); // no-op when nobody is in reach
}

void AShinobiTrainingDummy::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AShinobiTrainingDummy::ResetHealth, 1.5f, false);
}

void AShinobiTrainingDummy::ResetHealth()
{
	Attributes->SetHealth(Attributes->GetMaxHealth());
}
```

- [ ] **Step 5: Add the cast cheat and a burn column to the stats dump**

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, add after `ShinobiDrainChakra`:

```cpp
	/** Console: ShinobiCast 1..4 -- taps that jutsu's input, same as pressing its key. */
	UFUNCTION(Exec)
	void ShinobiCast(int32 JutsuSlot);
```

Also change the `ShinobiDumpStats` comment to: `/** Console: ShinobiDumpStats -- logs every character's attributes (and whether it's burning) as this machine sees them. */`

In `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp`, add after `ServerDrainChakra_Implementation`:

```cpp
void AShinobiPlayerController::ShinobiCast(int32 JutsuSlot)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi || JutsuSlot < 1 || JutsuSlot > 4)
	{
		return;
	}
	const int32 Input = static_cast<int32>(EShinobiAbilityInputID::Jutsu1) + JutsuSlot - 1;
	Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(Input);
	Shinobi->GetAbilitySystemComponent()->AbilityLocalInputReleased(Input);
}
```

Replace the `UE_LOG` in `ShinobiDumpStats` with:

```cpp
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiStats %s local=%d H=%.1f C=%.1f S=%.1f Burn=%d"),
			*It->GetName(), It->IsLocallyControlled() ? 1 : 0, Stats->GetHealth(), Stats->GetChakra(), Stats->GetStamina(),
			It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning) ? 1 : 0);
```

In `RunSmokeTest`, add after the `DamageTimer` line. At 5.5 s the client throws a Fireball at the middle dummy, which stands 600 units straight ahead of the PlayerStart:

```cpp
	FTimerHandle CastTimer;
	GetWorldTimerManager().SetTimer(CastTimer, FTimerDelegate::CreateUObject(this, &AShinobiPlayerController::ShinobiCast, 1), 5.5f, false);
```

Change the `RunSmokeTest` comment in the header to: `/** With -ShinobiSmoke on a client: damage self, throw a Fireball at the dummy ahead, dump stats, quit. Used by Scripts/smoke_m1.ps1. */`

- [ ] **Step 6: Assert the Fireball in `Scripts/smoke_m1.ps1`**

Change the first two comment lines to:

```powershell
# Starts a listen server and a client as separate -game processes (no rendering). The client damages itself through a
# server RPC and throws a Fireball at the dummy ahead; asserts the client sees both through replication.
```

Replace everything from `$hit = Select-String` to the end of the file with:

```powershell
$hit = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats .* local=1 H=450\.0"
# The Fireball (cast at 5.5 s) hit the middle dummy: 60 direct plus burn ticks, and it's still burning at 9 s.
$burned = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats ShinobiTrainingDummy\S* local=0 H=(\d+)\.\d .*Burn=1" |
    Where-Object { [double]$_.Matches[0].Groups[1].Value -le 440 }
if ($hit -and $burned) {
    Write-Host "SMOKE PASS: $($hit.Line)"
    Write-Host "SMOKE PASS: $($burned[0].Line)"
} else {
    Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats|Error" | Select-Object -Last 20 | ForEach-Object { Write-Host $_.Line }
    if (-not $hit) { throw "SMOKE FAIL: client never saw its replicated Health drop to 450" }
    throw "SMOKE FAIL: client never saw a fireballed (H <= 440), burning dummy"
}
```

- [ ] **Step 7: Build, run all tests and the smoke test**

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
powershell -ExecutionPolicy Bypass -File Scripts\smoke_m1.ps1
```
Expected:
- `ALL 38 TESTS PASSED`;
- two `SMOKE PASS` lines, the second for a `ShinobiTrainingDummy_*` with `H=` at or below 440 and `Burn=1`.

- [ ] **Step 8: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp Scripts/smoke_m1.ps1 Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp
git commit -m "feat: dummy status text and attack cvar, ShinobiCast cheat, fireball smoke check

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Manual 2-player check (user + agent)

**Files:** none. Fixes go back into the relevant task's files.

- [ ] **Step 1: The agent's pre-check.** Run `Scripts/shot_chakra.ps1` and look at the HUD shot (Task 11, Step 4).
- [ ] **Step 2: The user runs PIE.** Open `Lvl_ThirdPerson` → Play dropdown → 2 players, Play As Listen Server → Play. Walk this checklist in both windows:

| # | Check | Expected |
|---|---|---|
| 1 | Aim at a dummy, press **1** | ~0.4 s pause, then a grey ball flies to the crosshair. The dummy loses 60 and shows `[Burn]`, then loses 10 per second for 4 s. Slot 1 shades over and counts down 4 → 1. |
| 2 | Press **1** again straight away | Slot 1 flashes red; no chakra spent |
| 3 | Console `ShinobiDrainChakra 90`, press **1**–**4** | Slots are dimmed. Each press flashes red and spends nothing. |
| 4 | Face a dummy ~800 away (within ~15° of the crosshair), press **2** | ~0.6 s pause, then a dash up to it. It loses 120 and shows `[Shock]` for 1.5 s and `[+25%]`. Your next punch there deals 25 instead of 20, and `[+25%]` disappears. |
| 5 | Burn a dummy with **1**, walk up to it, press **3** | It loses 150 + 200 and the `[Burn]` goes away. It's knocked back. The dummies either side (300 away) each lose 200. |
| 6 | Crosshair on a dummy (any distance up to ~30 m), press **4**; then aim at the sky and press **4** | `[Black Flame]`, losing 15 per second for 10 s. At the sky: red flash, no chakra spent. |
| 7 | Console `Shinobi.DummyAttack 1`, stand next to a dummy | Its text turns yellow, then you lose 25, every 2 s |
| 8 | Still being punched: hold **C** | The charge ends on the punch |
| 9 | Still being punched: press **1** just as the text turns yellow | The cast is interrupted, you get 10 chakra back, and slot 1 has no cooldown |
| 10 | Still being punched: **Ctrl** just as the text turns yellow | No damage. You appear behind the dummy facing it, and one log pip empties (3 → 2). Pips refill at 1 per 15 s. With no pips left, the dodge still avoids damage but you don't teleport. |
| 11 | Gamepad | RB + A/B/X/Y casts 1–4. Without RB, A jumps, B dodges and X punches. |
| 12 | Second window | Sees the fireball, the dash, dummy HP and status text, and your teleport. Its own HUD shows its own cooldowns and pips. |
| 13 | Client window `NetEmulation.PktLag 150`, then cast each jutsu | Works; no large snaps. A small correction at the start of the Chidori dash is a known risk; note it if seen. |
| 14 | Console `Shinobi.DummyAttack 0` | Punching stops |
| 15 | Client window: cast each jutsu ~10 times (wait out cooldowns) | Every cast fires. None of them spends chakra while nothing happens. |

- [ ] **Step 3: Record the result.** Append a short "M2a manual check" note (date, pass/fail per row, fixes) to the bottom of this plan and commit it:
```powershell
git add docs/superpowers/plans/2026-09-26-m2a-jutsu-mechanics.md
git commit -m "docs: record M2a manual play check

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 4: Tag the milestone.** Run `git tag m2a-jutsu-mechanics`.

---

## Carry-forward notes for M2b+

- **Chidori dash sync.** The dash starts after a wind-up timer that runs separately on client and server. With lag, the server's dash begins about RTT/2 later in move time, so corrections are possible at its start. If PktLag tests show them, sync the start with `UAbilityTask_NetworkSyncPoint` (`OnlyServerWait`) before the dash. Also, each side picks its own direction and length: with no target from its own camera (the server's is up to ~80 ms stale), and with a target from its own positions. At PktLag 150, a camera turning fast just before the wind-up ends can make the two dashes diverge by hundreds of units, and the server then snaps the client. The proper fix is the same client-sent target data as for `PrepareJutsu`, with the direction and distance validated by the server. Snap and strike are also 2D only, so a hostile on a ledge high above within 15° pulls a horizontal dash.
- **Fireball launch point.** It is a fixed chest-height point 60 in front of the caster. Use the hand socket once M2b's cast animations exist.
- **Amaterasu target under lag.** The target is picked separately by the client (predicted) and the server (authoritative, using the client's camera from `ServerUpdateCamera`). Under lag they can disagree; the server wins. If that feels bad, send the client's target as target data.
- **Status text is temporary.** The dummies' status text and the grey Fireball ball are placeholders. M2b replaces them with Gameplay Cues (Burn/Black Flame shells, Shock crackle, the Firestorm burst, Substitution log + smoke, Rasengan sphere, Chidori hand and trail).
- **Cheats in Shipping.** `Shinobi.DummyAttack` joins the cheats to gate out of Shipping before M4: skip the punch under `UE_BUILD_SHIPPING`.
- **M3 boss stun.** Boss stun (spec: 0.5 s, multiplier 0.33) needs `GE_Shock`'s duration to become SetByCaller, scaled by the target's stun multiplier.
- **M3 knockback on players.** Rasengan's knockback uses `LaunchCharacter` on the server. That's fine for AI targets. If enemies ever use it on players, see the M1 note on root-motion hit reactions.
- **HUD cooldown shade.** It approximates the spec's radial sweep; M5 can swap in a radial material.
- **Jutsu cooldown is not predicted.** The cooldown is committed from the wind-up timer, outside any prediction window, so the owning client only sees it once the server's copy replicates (about one-way latency late). A recast in that window is predicted, then rejected: a brief chakra dip that rolls back. Acceptable for now.
- **Client and server can disagree on `PrepareJutsu`.** This matters for Amaterasu, where the server's camera from `ServerUpdateCamera` can be ~80 ms stale. If the client fails but the server succeeds, the server pays and then the client's cancel ends it with no refund. The fix is a target-data exchange (client sends its target) or not replicating the client's failure end.
- **Projectiles block each other.** Both use the WorldDynamic object type and block everything, so a fireball and M3's kunai meet head-on and both explode mid-air. Decide in M3: keep it as "fireball destroys kunai", or give projectiles their own channel and ignore it.
- **M2b explosion VFX needs a server cue.** A ball that explodes at spawn is destroyed before it ever replicates, so an explosion effect must be a Gameplay Cue sent from the server, not something the actor does on clients.
- **Allies block the aim.** `TraceAim` ignores only the caster. An ally within ~72 units of the aim line, in front of the enemy, is the first hit, so Amaterasu fails (for free) and Fireball's aim point lands on the ally. If playtests complain, ignore non-hostile characters in the sweep.
- **Wasted jutsu on a lost target.** Amaterasu locks its target at cast time. If that target becomes invulnerable, dies or is destroyed during the 0.5 s wind-up, the full cost and cooldown are spent for nothing. For M3: add an "alive" check to `ApplyStatus`/`IsHostileTo`, and consider a refund (the interrupt path) when the target is gone.
- **Damage context carries no ability.** `ShinobiCombat` builds the damage spec from the ASC's own effect context, with no ability and at level 1. If M2b hit cues need `GetAbility()` to pick a per-jutsu effect, add a source object.

## Self-review

**Spec coverage (M2 row of §8 and §3 rules):**

| Requirement | Task |
|---|---|
| 4 jutsu: cost, cooldown, wind-up | Tasks 5–8 |
| Fireball: 60 + 40 splash r250, Burn | Tasks 4, 5 |
| Chidori: 1200 dash, 15° snap, 120, Shock | Task 7 |
| Rasengan: 200 reach, 150, 1500 knockback, Firestorm 200 r500 (consumes normal burn, black stays) | Task 8 |
| Amaterasu: range 3000, crosshair, 15/s × 10 s, no target → fail without cost | Task 6 |
| Burn, Black Flame, Shock (stun + next hit +25% once) | Task 2 |
| Interrupt: ≥ 20 during wind-up → cancel, 50% refund, no cooldown | Tasks 1, 5 |
| Substitution: i-frames + charge → negate, teleport behind, attacker staggered 0.5 s, 3 charges, +1/15 s | Tasks 1, 9 |
| Chakra charge ends on stagger | Task 3 |
| Input: 1–4, RB + A/B/X/Y | Task 10 |
| HUD: slots (glyph, key, cost, cooldown, greyed, red flash), log pips | Task 11 |
| Smoke: cast and replicated damage/status | Task 12 |

Moved on purpose: VFX and wind-up animations → M2b; lock-on → M3; damage-number types, hit-stop and camera shake → M5 (deviations table).

**Carry-forward notes from M0/M1 resolved here:**

| Note | Task |
|---|---|
| Exec + "landed" honesty | Task 1 |
| Double-apply modifier | Task 1 |
| Charge montage interrupt | Task 3 |
| Shared blocked tags | Task 2 |
| Input test two-keys rule | Task 10 |
| Invulnerability single place | Task 1 |
| Substitution cancels dodge | Task 9 |
| Held-input release + stagger cancel on charge | Task 3 |
| RB context as part of the singleton config | Task 10 |

Still open and unchanged: dodge direction replication, i-frame RTT, melee yaw staleness, input buffer, montage preload, cook paths, cheat shipping gates, the M3/M5 items.

**Type consistency:**
- `ShinobiCombat::ApplyDamage` and `ApplyStatus` take `const AShinobiCharacterBase*` everywhere.
- `UShinobiGameplayAbility::ApplyDamage` keeps its non-const signature and forwards.
- `FShinobiJutsuSlotState` fields are `CooldownRemaining`, `CooldownDuration` and `bAffordable` in both Task 5 and Task 11.
- `EShinobiAbilityInputID::Jutsu1..4` is used by the jutsu constructors, the HUD (`JutsuSlotIndex`), the input config, `ShinobiCast` and the tests.
- `UGE_Burn::DamagePerTick` and `UGE_BlackFlame::DamagePerTick` are used by the projectile, Amaterasu and the tests.
- Tags are defined once in Task 1 and only used afterwards.

**Test count by task:** 18 → T1 20 → T2 24 → T3 26 → T4 27 → T5 30 → T6 31 → T7 34 → T8 36 → T9 37 → T10 37 → T11 37 → T12 38.

