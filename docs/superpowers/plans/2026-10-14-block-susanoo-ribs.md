# Block with Susanoo Ribcage Armour Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hold F / Left Trigger to guard: the player stands still in a guard pose inside a glowing purple Susanoo ribcage, takes a fifth of each hit for stamina, and is staggered when the stamina can't pay.

**Architecture:** A hold ability (`UGA_Block`, modelled on `UGA_Sprint`) applies an infinite `UGE_Blocking` on the server; its replicated `State.Blocking` tag drives everything else: the damage exec's cut/stamina/guard-break rule, a zero max speed, a second instance of the (generalised) `UShinobiSusanooComponent` showing a new body-hugging mesh `SM_SusanooRibs`, and a looping `AM_Block_Guard` montage played the way the mounted sit loop is.

**Tech Stack:** UE 5.8 C++ (GAS, Enhanced Input), UE automation tests, Unreal Python commandlet imports, Hyper3D Rodin (MCP) + Blender 5.2 headless, Mixamo FBX.

**Spec:** `docs/superpowers/specs/2026-10-14-block-susanoo-ribs-design.md`

## Global Constraints

- Branch `feature/block` (off `d649b09d`, which has GASP). Never switch branches or merge with the Unreal Editor open.
- Close the GUI editor before `Scripts/build.ps1`, `Scripts/run_tests.ps1` or `Scripts/run_python.ps1`.
- Build: `powershell -File Scripts/build.ps1`. Tests: `powershell -File Scripts/run_tests.ps1 -Filter "<prefix>"`. Python imports: `powershell -File Scripts/run_python.ps1 -Script <script.py>`.
- Test only what changed (user rule): run the task's filter, not the whole `ShinobiCoop` suite, until the final task.
- Constants (verbatim from the spec): `DamageTakenScale = 0.2`, `StaminaPerDamage = 0.5`, `MinStaminaCost = 5`; fade-in 0.15 s, fade-out 0.25 s; ribcage about 2.2 m tall, about 20k tris, origin at the feet, +X forward, centimetres, one material slot, no collision.
- Players only: enemies, villagers and PvP bots do not get `UGA_Block` or the guard figure.
- Comments match the repo's style: plain sentences saying why, no banners. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Spending credits (image generation, Rodin) needs the user's go-ahead at the checkpoints in Task 4.

## Review Focus

1. Guarding with Susanoo up: both armours show; a 100 hit lands as 100 x 0.5 x 0.2 = 10 and costs 25 stamina (the cost uses the damage after Susanoo, before the block cut). Test in Task 1.
2. A hit during dodge i-frames or Kamui while guarding: nothing lands and no stamina is spent (those checks return before the block rule). Test in Task 1.
3. A guard break from a small hit (below the 20 stagger threshold) still staggers, and only one stagger event is sent for a big one. Test in Task 1.
4. Going down while guarding drops the guard (no ribcage over the body) and ends the ability. Tests in Tasks 1 and 2.
5. Owning client in a networked game: the guard (tag, root, figure) appears one round trip after the press, since only the server applies `UGE_Blocking`. Expect a brief movement correction, never a stuck root. Playtest check in Task 6 (no automation test can drive two machines here).

---

### Task 1: Guard rule in the damage exec

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiTags.h` (declarations), `Source/ShinobiCoop/Shinobi/ShinobiTags.cpp` (definitions)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp`, `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h` (doc comment)
- Create: `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`

**Interfaces:**
- Produces: `ShinobiTags::State_Blocking`, `ShinobiTags::Ability_Block`; `UGE_Blocking` (infinite; grants `State.Blocking` + `State.StaminaRegenBlocked`; inhibited while Downed/Dead) with `static constexpr float DamageTakenScale = 0.2f, StaminaPerDamage = 0.5f, MinStaminaCost = 5.f`.

- [ ] **Step 1: Write the failing tests**

Create `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`:

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDamageTest, "ShinobiCoop.Block.Damage.AFifthLandsForStamina", ShinobiTestFlags)
bool FShinobiBlockDamageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Enemy->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Stamina = UShinobiAttributeSet::GetStaminaAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	TestTrue(TEXT("Guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("A blocked hit still lands"), ShinobiCombat::ApplyDamage(Source, P, 100.f));
	TestEqual(TEXT("but only a fifth of it"), ShinobiTest::Get(P, Health), 480.f);
	TestEqual(TEXT("for half its damage in stamina"), ShinobiTest::Get(P, Stamina), 50.f);
	TestEqual(TEXT("and it does not stagger"), Staggers.Count, 0);

	ShinobiCombat::ApplyDamage(Source, P, 10.f);
	TestEqual(TEXT("A small hit: a fifth"), ShinobiTest::Get(P, Health), 478.f);
	TestEqual(TEXT("for the minimum 5 stamina"), ShinobiTest::Get(P, Stamina), 45.f);
	TestTrue(TEXT("Still guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBreakTest, "ShinobiCoop.Block.Damage.AHitTheStaminaCannotPayBreaksTheGuard", ShinobiTestFlags)
bool FShinobiBlockBreakTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Enemy->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Stamina = UShinobiAttributeSet::GetStaminaAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::Set(P, Stamina, 30.f);
	ShinobiCombat::ApplyDamage(Source, P, 100.f); // costs 50
	TestEqual(TEXT("The breaking hit lands in full"), ShinobiTest::Get(P, Health), 400.f);
	TestEqual(TEXT("and empties the stamina"), ShinobiTest::Get(P, Stamina), 0.f);
	TestFalse(TEXT("The guard is broken"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("and the player staggers"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	TestEqual(TEXT("One stagger event, not two"), Staggers.Count, 1);

	ShinobiCombat::ApplyDamage(Source, P, 10.f);
	TestEqual(TEXT("The next hit is a normal one"), ShinobiTest::Get(P, Health), 390.f);
	TestEqual(TEXT("and below 20 it does not stagger"), Staggers.Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockSmallBreakTest, "ShinobiCoop.Block.Damage.ASmallBreakingHitStillStaggers", ShinobiTestFlags)
bool FShinobiBlockSmallBreakTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 3.f);
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 10.f); // costs 5
	TestEqual(TEXT("Lands in full"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 490.f);
	TestFalse(TEXT("The guard is broken"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestEqual(TEXT("A guard break staggers whatever the damage"), Staggers.Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBurningTest, "ShinobiCoop.Block.Damage.BurningIsCutButCostsNothing", ShinobiTestFlags)
bool FShinobiBlockBurningTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplySetByCaller(P, UGE_BlackFlame::StaticClass(), ShinobiTags::Data_Damage, 20.f); // 20 a second
	W.Tick(2.5f); // two ticks
	TestEqual(TEXT("Two ticks of 20, a fifth each"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 492.f);
	TestEqual(TEXT("No stamina spent on burning"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	TestTrue(TEXT("Still guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockSusanooTest, "ShinobiCoop.Block.Damage.StacksWithSusanoo", ShinobiTestFlags)
bool FShinobiBlockSusanooTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestEqual(TEXT("100 x 0.5 x 0.2"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 490.f);
	TestEqual(TEXT("The cost is on what Susanoo let through"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDodgeTest, "ShinobiCoop.Block.Damage.ADodgedHitCostsNoStamina", ShinobiTestFlags)
bool FShinobiBlockDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_DodgeIFrames::StaticClass());
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 50.f);
	TestEqual(TEXT("Nothing lands"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("and nothing is spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDownedTest, "ShinobiCoop.Block.Damage.ADownedPlayerIsNotGuarding", ShinobiTestFlags)
bool FShinobiBlockDownedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("No guard on a downed player"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Build to confirm it fails**

Run: `powershell -File Scripts/build.ps1`
Expected: compile errors `'UGE_Blocking': undeclared identifier` and `'State_Blocking': is not a member of 'ShinobiTags'`.

- [ ] **Step 3: Add the tags**

`ShinobiTags.h`, after the `State_Shield` declaration:

```cpp
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking); // holding a guard (UGA_Block): hits cost stamina instead of most of their damage
```

and after `UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Revive);`:

```cpp
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block);
```

`ShinobiTags.cpp`, next to the `State_Shield` definition and after `UE_DEFINE_GAMEPLAY_TAG(Ability_Revive, "Ability.Revive");` respectively:

```cpp
	UE_DEFINE_GAMEPLAY_TAG(State_Blocking, "State.Blocking");
```

```cpp
	UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");
```

- [ ] **Step 4: Add `UGE_Blocking`**

`ShinobiEffects.h`, after `UGE_Susanoo`:

```cpp
/** Holding a guard (UGA_Block): State.Blocking and paused stamina regen until the ability removes it. UShinobiDamageExec cuts a
 * blocked hit to DamageTakenScale and charges stamina for it; a hit the stamina can't pay for breaks the guard (removes this). */
UCLASS()
class SHINOBICOOP_API UGE_Blocking : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamageTakenScale = 0.2f; // read by UShinobiDamageExec
	static constexpr float StaminaPerDamage = 0.5f; // of the hit before the guard's cut
	static constexpr float MinStaminaCost = 5.f;
	UGE_Blocking();
};
```

`ShinobiEffects.cpp`, after `UGE_Susanoo::UGE_Susanoo()`:

```cpp
UGE_Blocking::UGE_Blocking()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite; // UGA_Block removes it on release, the damage exec on a guard break
	GrantTag(ShinobiTags::State_Blocking);
	GrantTag(ShinobiTags::State_StaminaRegenBlocked);
	// A downed or dead player is not guarding (no ribcage over the body); losing the tag also ends UGA_Block.
	InhibitWhileTargetHas(ShinobiTags::State_Downed);
	InhibitWhileTargetHas(ShinobiTags::State_Dead);
}
```

- [ ] **Step 5: Add the guard rule to the exec**

`ShinobiDamageExec.cpp`: add includes

```cpp
#include "Shinobi/VFX/ShinobiBurst.h"
```

Insert right after the closing brace of the `if (!bPeriodic) { ... invulnerable / shock bonus ... }` block, before the shield block:

```cpp
	// The guard (UGA_Block): a blocked hit costs stamina and only a fifth of it lands, with no stagger or shove. One the stamina
	// can't pay for breaks the guard: it lands in full and staggers. Burning is cut too but costs nothing.
	bool bBlocked = false;
	bool bGuardBroken = false;
	if (Target->HasMatchingGameplayTag(ShinobiTags::State_Blocking))
	{
		if (bPeriodic)
		{
			Damage *= UGE_Blocking::DamageTakenScale;
		}
		else
		{
			const FGameplayAttribute Stamina = UShinobiAttributeSet::GetStaminaAttribute();
			const float Cost = FMath::Max(UGE_Blocking::MinStaminaCost, UGE_Blocking::StaminaPerDamage * Damage);
			const float Left = Target->GetNumericAttribute(Stamina);
			bBlocked = Left >= Cost;
			bGuardBroken = !bBlocked;
			Target->SetNumericAttributeBase(Stamina, bBlocked ? Left - Cost : 0.f);
			if (bBlocked)
			{
				Damage *= UGE_Blocking::DamageTakenScale;
				if (const AActor* Guard = Target->GetAvatarActor())
				{
					AShinobiBurst::Spawn(Guard->GetWorld(), EShinobiBurst::SusanooRise, Guard->GetActorLocation(), 0.15f);
				}
			}
			else
			{
				// Safe mid-execution for the same reason as the shield below: the effects container defers changes under its scope lock.
				Target->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Blocking));
				Target->ApplyGameplayEffectToSelf(GetDefault<UGE_Stagger>(), 1.f, Target->MakeEffectContext());
			}
		}
	}
```

Then change the stagger and shove conditions further down:

```cpp
	if (!bPeriodic && !bBlocked && (bGuardBroken || Damage >= StaggerThreshold))
	{
		SendEvent(Target, ShinobiTags::Event_Hit_Stagger, Attacker, Damage);
	}
	// The body takes the hit physically, away from the attacker (burning ticks don't shove it around, nor does a guarded hit).
	const AShinobiCharacterBase* Victim = Cast<AShinobiCharacterBase>(Target->GetAvatarActor());
	if (!bPeriodic && !bBlocked && Attacker && Victim && Victim->GetPhysicalReactions())
```

Note: the shield block sits between the guard and these lines and can `return` early when it soaks everything; that is fine (a fully soaked hit never staggered).

In `ShinobiDamageExec.h`, add one bullet to the class doc list, next to the stagger bullet:

```cpp
 * - target guarding (State.Blocking, UGE_Blocking): a fifth lands for stamina; a hit the stamina can't pay breaks the guard
```

- [ ] **Step 6: Build and run the tests**

Run: `powershell -File Scripts/build.ps1` then `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop.Block.Damage"`
Expected: BUILD OK; 7 tests pass.

Then the neighbours this touches: `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop.Damage"` and `-Filter "ShinobiCoop.Susanoo"`. Expected: all pass (unchanged behaviour without the tag).

- [ ] **Step 7: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/ShinobiTags.h Source/ShinobiCoop/Shinobi/ShinobiTags.cpp Source/ShinobiCoop/Shinobi/ShinobiEffects.h Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp Source/ShinobiCoop/Shinobi/ShinobiDamageExec.h Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp
git commit -m "feat: guarded hits cost stamina and a guard breaks when it can't pay"
```

---

### Task 2: `UGA_Block` on F / Left Trigger

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Block.h`, `Source/ShinobiCoop/Shinobi/Abilities/GA_Block.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h:12-25` (input id)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.cpp:83-88` (mapping)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp:24-45` (include), `:71-78` (grant list)
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp:18-21` (cancels the block)
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp:30-33` (casting blocks the block)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp:139-156` (root)
- Modify: `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`, `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`

**Interfaces:**
- Consumes: `UGE_Blocking`, `ShinobiTags::State_Blocking`, `ShinobiTags::Ability_Block` (Task 1); `UShinobiGameplayAbility::ApplyEffectToSelf(TSubclassOf<UGameplayEffect>) const -> FActiveGameplayEffectHandle`; `AShinobiBurst::Spawn(UWorld*, EShinobiBurst, const FVector&, float Scale)`.
- Produces: `EShinobiAbilityInputID::Block`; `UGA_Block`.

- [ ] **Step 1: Write the failing tests**

Append to `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`, before `#endif`, and add `#include "Shinobi/Abilities/GA_Block.h"`, `#include "Shinobi/Abilities/GA_Dodge.h"`, `#include "Shinobi/Abilities/GA_Melee.h"`, `#include "Shinobi/Abilities/GA_Sprint.h"`, `#include "Shinobi/Abilities/GA_Susanoo.h"`, `#include "Shinobi/ShinobiMovementComponent.h"` at the top with the other includes:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockHoldTest, "ShinobiCoop.Block.Ability.HoldToGuardReleaseToLower", ShinobiTestFlags)
bool FShinobiBlockHoldTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("Guard up while held"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestTrue(TEXT("Guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestEqual(TEXT("Rooted while guarding"), Move->GetMaxSpeed(), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Holding the guard costs nothing by itself"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Guard down on release"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestFalse(TEXT("Not guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("Free to move again"), Move->GetMaxSpeed() > 0.f);
	TestTrue(TEXT("Stamina regen waits a moment, as after a sprint"), ShinobiTest::HasTag(P, ShinobiTags::State_StaminaRegenBlocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockExhaustedTest, "ShinobiCoop.Block.Ability.NoGuardWithoutStamina", ShinobiTestFlags)
bool FShinobiBlockExhaustedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 0.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Can't raise a guard at 0 stamina"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBreakEndsTest, "ShinobiCoop.Block.Ability.AGuardBreakEndsTheAbility", ShinobiTestFlags)
bool FShinobiBlockBreakEndsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 10.f);
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestFalse(TEXT("The broken guard ends the ability"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestTrue(TEXT("Staggered"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	TestEqual(TEXT("The hit landed in full"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 400.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	W.Tick(0.6f); // the stagger is over
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 100.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("The guard can go up again"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockNoAttackTest, "ShinobiCoop.Block.Ability.NoAttackingOrSprintingBehindTheGuard", ShinobiTestFlags)
bool FShinobiBlockNoAttackTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("No melee"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("No sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("No jutsu"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	TestTrue(TEXT("Still guarding"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDodgeOutTest, "ShinobiCoop.Block.Ability.ADodgeDropsTheGuard", ShinobiTestFlags)
bool FShinobiBlockDodgeOutTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("The dodge goes"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("and the guard drops"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestFalse(TEXT("Not guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockCastingTest, "ShinobiCoop.Block.Ability.NoGuardMidCast", ShinobiTestFlags)
bool FShinobiBlockCastingTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1); // Susanoo's seals take 0.9 s
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("A jutsu being cast keeps the guard down"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDownedEndsTest, "ShinobiCoop.Block.Ability.GoingDownEndsTheGuard", ShinobiTestFlags)
bool FShinobiBlockDownedEndsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("Downed: the guard ends"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}
```

`Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`, after the line `TestNotNull(TEXT("E revives"), ...)`:

```cpp
	TestNotNull(TEXT("F guards"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Block], EKeys::F));
	TestNotNull(TEXT("LT guards"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Block], EKeys::Gamepad_LeftTrigger));
```

- [ ] **Step 2: Build to confirm it fails**

Run: `powershell -File Scripts/build.ps1`
Expected: compile errors: `Block: is not a member of 'EShinobiAbilityInputID'`, `GA_Block.h: No such file`.

- [ ] **Step 3: Input id and mapping**

`ShinobiGameplayAbility.h`, the enum:

```cpp
	Revive,
	Dismount,
	Block // appended last: the ASC input ids are the enum values
};
```

(remove the `// appended last...` comment from `Dismount`).

`ShinobiInputConfig.cpp`, after the `IA_Revive` line:

```cpp
	AddAbility(EShinobiAbilityInputID::Block, TEXT("IA_Block"), EKeys::F, EKeys::Gamepad_LeftTrigger); // hold
```

- [ ] **Step 4: Write `UGA_Block`**

`Source/ShinobiCoop/Shinobi/Abilities/GA_Block.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Block.generated.h"

/**
 * Hold to guard: a Susanoo ribcage rises round the player (UGE_Blocking, shown by AShinobiPlayerCharacter's guard figure), who
 * stands still behind it. Hits cost stamina instead of most of their damage (UShinobiDamageExec). Ends on release, when the
 * guard breaks or the player goes down (the tag goes), or when a dodge cancels it; then stamina regen waits as after a sprint.
 */
UCLASS()
class SHINOBICOOP_API UGA_Block : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Block();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnGuardRemoved();

	FActiveGameplayEffectHandle GuardHandle;
	bool bGuardRaised = false;
};
```

`Source/ShinobiCoop/Shinobi/Abilities/GA_Block.cpp`:

```cpp
#include "Shinobi/Abilities/GA_Block.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Block::UGA_Block()
{
	InputID = EShinobiAbilityInputID::Block;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Block));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Phased);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Mounted);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	// Behind the guard: no attacking, casting, sprinting or charging. A dodge is still allowed and drops the guard (UGA_Dodge).
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
}

bool UGA_Block::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// As UGA_Sprint: gating here means a client at 0 stamina never sends the activation.
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetStaminaAttribute()) > 0.f;
}

void UGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bGuardRaised = true;

	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_Block::OnInputReleased);
	Release->ReadyForActivation();

	if (!HasAuthority(&ActivationInfo))
	{
		return; // the guard itself is the server's (it replicates); the owning client only predicts the ability
	}
	GuardHandle = ApplyEffectToSelf(UGE_Blocking::StaticClass());
	if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
	{
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::SusanooRise, Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), 0.35f);
	}
	// A guard break (UShinobiDamageExec) or going down takes the tag away; the ability follows. Server only: on the client
	// the tag arrives later by replication, and this task fires at once when the tag is missing.
	UAbilityTask_WaitGameplayTagRemoved* Lost = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, ShinobiTags::State_Blocking);
	Lost->Removed.AddDynamic(this, &UGA_Block::OnGuardRemoved);
	Lost->ReadyForActivation();
}

void UGA_Block::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Block::OnGuardRemoved()
{
	if (bGuardRaised) // not when EndAbility itself removed the guard
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bGuardRaised)
	{
		bGuardRaised = false;
		if (HasAuthority(&ActivationInfo))
		{
			// The regen delay first, as in UGA_Sprint::EndAbility: both grant State.StaminaRegenBlocked, so the count never hits 0.
			ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveActiveGameplayEffect(GuardHandle); // already gone after a guard break; harmless
			}
			GuardHandle.Invalidate();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

- [ ] **Step 5: Grant it, let dodge cancel it, let casting block it, root the player**

`ShinobiPlayerCharacter.cpp`: add `#include "Shinobi/Abilities/GA_Block.h"` (alphabetical, after `GA_Amaterasu.h`), and in the grant list change line 71 to:

```cpp
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(), UGA_Block::StaticClass(),
```

`GA_Dodge.cpp`, after `CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);`:

```cpp
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Block); // dodging out of a guard
```

`ShinobiJutsuAbility.cpp`, after `BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);`:

```cpp
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Block);
```

`ShinobiMovementComponent.cpp`, in `GetMaxSpeed()` right after the `const AShinobiCharacterBase* Character = ...` line:

```cpp
	if (Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Blocking))
	{
		return 0.f; // guarding roots the player (the guard pose is full-body); they still turn to face their input
	}
```

- [ ] **Step 6: Build and run the tests**

Run: `powershell -File Scripts/build.ps1`, then `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop.Block"` and `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop.Input"`.
Expected: all `ShinobiCoop.Block.*` pass (7 damage + 7 ability); input tests pass (the loop over ability actions counts F + LT as 2 keys in `IMC_Shinobi`).

Then the touched neighbours: `-Filter "ShinobiCoop.Abilities"` (dodge, sprint, melee). Expected: pass.

- [ ] **Step 7: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/Abilities/GA_Block.h Source/ShinobiCoop/Shinobi/Abilities/GA_Block.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h Source/ShinobiCoop/Shinobi/ShinobiInputConfig.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp Source/ShinobiCoop/Tests/ShinobiInputTests.cpp
git commit -m "feat: hold F / LT to guard"
```

---

### Task 3: The guard figure (generalised Susanoo component)

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/VFX/ShinobiSusanooComponent.h`, `Source/ShinobiCoop/Shinobi/VFX/ShinobiSusanooComponent.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp:116-123`
- Modify: `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`

**Interfaces:**
- Consumes: `ShinobiTags::State_Blocking`.
- Produces: `UShinobiSusanooComponent` public config `FGameplayTag StateTag`, `FSoftObjectPath Model`, `float FadeInSeconds`, `float FadeOutSeconds`; `AShinobiPlayerCharacter::GetGuard() const -> UShinobiSusanooComponent*`.

- [ ] **Step 1: Write the failing test**

Append to `ShinobiBlockTests.cpp` (add `#include "Shinobi/VFX/ShinobiSusanooComponent.h"`):

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockFigureTest, "ShinobiCoop.Block.Figure.StandsWhileGuarding", ShinobiTestFlags)
bool FShinobiBlockFigureTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiSusanooComponent* Guard = P->GetGuard();
	if (!TestNotNull(TEXT("Players have a guard figure"), Guard))
	{
		return false;
	}
	TestTrue(TEXT("It is a second figure, not the Susanoo jutsu's"), Guard != P->GetSusanoo());
	TestFalse(TEXT("Down before guarding"), Guard->IsShown());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("Up while guarding"), Guard->IsShown());
	TestFalse(TEXT("and the 6 m Susanoo stays down"), P->GetSusanoo()->IsShown());

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Down on release"), Guard->IsShown());
	return true;
}
```

- [ ] **Step 2: Build to confirm it fails**

Run: `powershell -File Scripts/build.ps1`
Expected: `'GetGuard': is not a member of 'AShinobiPlayerCharacter'`.

- [ ] **Step 3: Make the component configurable**

`ShinobiSusanooComponent.h`: replace the class doc comment and add the config block after `IsShown()`/`GetFigure()`:

```cpp
/**
 * A Susanoo figure: while its owner carries StateTag, Model (origin at the feet, in the glowing M_Susanoo) stands around them.
 * It fades in, breathes slowly, and fades out when the tag goes. The tag replicates, so every machine shows it. Cosmetic only.
 * Defaults: the Susanoo jutsu's 6 m ribcage (State.Susanoo, SM_Susanoo). Players have a second one for their guard (UGA_Block).
 */
```

```cpp
	/** Set by the owner's constructor; the defaults are the Susanoo jutsu's. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FGameplayTag StateTag;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FSoftObjectPath Model;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FadeInSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FadeOutSeconds = 0.5f;
```

Rename `void OnSusanooChanged(const FGameplayTag Tag, int32 Count);` to `void OnStateChanged(const FGameplayTag Tag, int32 Count);`.

`ShinobiSusanooComponent.cpp`:
- Delete the anonymous namespace with `SusanooFadeInSeconds` / `SusanooFadeOutSeconds`.
- Constructor, add:

```cpp
	StateTag = ShinobiTags::State_Susanoo;
	Model = FSoftObjectPath(TEXT("/Game/Shinobi/Jutsu/SM_Susanoo.SM_Susanoo"));
```

- In `BeginPlay`, `EndPlay`: replace every `ShinobiTags::State_Susanoo` with `StateTag`, and `&UShinobiSusanooComponent::OnSusanooChanged` with `&UShinobiSusanooComponent::OnStateChanged`. Rename the definition `OnSusanooChanged` to `OnStateChanged`.
- In `EnsureFigure`, replace the load line and the comment above it:

```cpp
	// Built the first time the figure is wanted, not at spawn: most characters never wear it. Origin at the feet.
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Model.ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh)
	{
		return;
	}
	Figure = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("SusanooFigure")));
	Figure->SetStaticMesh(Mesh);
```

- In `TickComponent`, replace the constants: `DeltaTime / FadeInSeconds` and `DeltaTime / FadeOutSeconds`.

- [ ] **Step 4: Give players the guard figure**

`ShinobiPlayerCharacter.h`: forward-declare `class UShinobiSusanooComponent;` with the others, add next to `GetNameTag()`:

```cpp
	/** The ribcage that stands round the player while they guard (UGA_Block). */
	UShinobiSusanooComponent* GetGuard() const { return Guard; }
```

and next to the `NameTag` member:

```cpp
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiSusanooComponent> Guard;
```

`ShinobiPlayerCharacter.cpp`: add `#include "Shinobi/VFX/ShinobiSusanooComponent.h"` (after `ShinobiSurfaceFxComponent.h`), and at the end of the constructor (after `MotionWarping = ...`):

```cpp
	// The guard's ribcage: a body-hugging Susanoo (Scripts/setup_susanoo_ribs.py) that snaps up and down quickly.
	Guard = CreateDefaultSubobject<UShinobiSusanooComponent>(TEXT("Guard"));
	Guard->StateTag = ShinobiTags::State_Blocking;
	Guard->Model = FSoftObjectPath(TEXT("/Game/Shinobi/Jutsu/SM_SusanooRibs.SM_SusanooRibs"));
	Guard->FadeInSeconds = 0.15f;
	Guard->FadeOutSeconds = 0.25f;
```

(`IsShown()` follows the tag even while `SM_SusanooRibs` does not exist yet, so the test passes before Task 4.)

- [ ] **Step 5: Build and run the tests**

Run: `powershell -File Scripts/build.ps1`, then `run_tests.ps1 -Filter "ShinobiCoop.Block.Figure"` and `-Filter "ShinobiCoop.Susanoo"`.
Expected: the figure test passes; all Susanoo tests still pass (same defaults).

- [ ] **Step 6: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/VFX/ShinobiSusanooComponent.h Source/ShinobiCoop/Shinobi/VFX/ShinobiSusanooComponent.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp
git commit -m "feat: a Susanoo ribcage stands round a guarding player"
```

---

### Task 4: `SM_SusanooRibs` from Rodin

**Files:**
- Create: `SourceArt/Rodin/SusanooRibs/concept.png`, `SourceArt/Rodin/SusanooRibs/susanoo_ribs_raw.glb` (downloads)
- Create: `Scripts/character/prep_susanoo_ribs.py` (Blender headless cleanup)
- Create: `Scripts/setup_susanoo_ribs.py` (Unreal import)
- Create: `SourceArt/Props/susanoo_ribs.fbx` (output)
- Modify: `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`

**Interfaces:**
- Produces: `/Game/Shinobi/Jutsu/SM_SusanooRibs` (static mesh, origin at the feet, +X forward, about 220 cm tall, no collision, one material slot). Task 3 loads it by that path.

- [ ] **Step 1: Write the failing test**

Append to `ShinobiBlockTests.cpp` (add `#include "Engine/StaticMesh.h"` and `#include "Components/StaticMeshComponent.h"`):

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockModelTest, "ShinobiCoop.Block.Figure.IsTheBodyHuggingRibcage", ShinobiTestFlags)
bool FShinobiBlockModelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass()); // the figure is built when it is first wanted
	const UStaticMeshComponent* Figure = P->GetGuard() ? P->GetGuard()->GetFigure() : nullptr;
	const UStaticMesh* Mesh = Figure ? Figure->GetStaticMesh() : nullptr;
	if (TestNotNull(TEXT("The guard figure has a mesh (run Scripts/setup_susanoo_ribs.py if not)"), Mesh))
	{
		TestEqual(TEXT("the guard ribcage"), Mesh->GetName(), FString(TEXT("SM_SusanooRibs")));
		const FBox Box = Mesh->GetBoundingBox();
		TestTrue(FString::Printf(TEXT("standing on the ground, about 2.2 m tall (Z %.0f to %.0f)"), Box.Min.Z, Box.Max.Z), Box.Min.Z > -20.0 && Box.Min.Z < 20.0 && Box.Max.Z > 190.0 && Box.Max.Z < 250.0);
		TestTrue(FString::Printf(TEXT("wide enough to go round the body (Y %.0f to %.0f)"), Box.Min.Y, Box.Max.Y), Box.Max.Y - Box.Min.Y > 90.0);
		TestTrue(TEXT("centred on the player"), FMath::Abs(Box.GetCenter().X) < 25.0 && FMath::Abs(Box.GetCenter().Y) < 25.0);
	}
	return true;
}
```

Run: `powershell -File Scripts/build.ps1` then `run_tests.ps1 -Filter "ShinobiCoop.Block.Figure.IsTheBodyHuggingRibcage"`.
Expected: FAIL "The guard figure has a mesh (run Scripts/setup_susanoo_ribs.py if not)".

- [ ] **Step 2: Concept image (checkpoint: user approves before spending)**

Tell the user the credit cost, then generate one image (Higgsfield `generate_image`; call `models_explore` with `action: "recommend"` first). Prompt:

> Spectral Susanoo ribcage armour from Naruto, empty with no person inside, hollow, a human-sized cage of 6 curved glowing purple chakra ribs spiralling around an invisible standing person, open gaps between the ribs, a spine at the back, flame-like wisps on the ribs, white rims, isolated on a plain light grey background, three-quarter front view, full height visible, game asset concept, no character, no text

Save it to `SourceArt/Rodin/SusanooRibs/concept.png`. Show it to the user and wait for approval; regenerate on a no.

- [ ] **Step 3: Rodin mesh (checkpoint: user approves the cost first)**

Use the hyper3d MCP: upload `concept.png` (`rodin_create_uploads` / `rodin_import_images`), `rodin_generate` with that image (Gen-2, GLB output, PBR materials off: Unreal uses `M_Susanoo`), `rodin_wait`, `rodin_get_result`. Download to `SourceArt/Rodin/SusanooRibs/susanoo_ribs_raw.glb`.

- [ ] **Step 4: Write the Blender cleanup script**

`Scripts/character/prep_susanoo_ribs.py`:

```python
"""Blender (5.2) headless prep of the Rodin guard ribcage GLB for Scripts/setup_susanoo_ribs.py.

  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P Scripts/character/prep_susanoo_ribs.py -- <in.glb> [height_cm] [clear_radius_cm]

Steps: import -> join -> apply transforms -> front (-Y in Blender, as Rodin exports it) turned to +X -> scale to height_cm
(default 220) -> feet at z = 0, centred -> delete faces inside a vertical cylinder of clear_radius_cm (default 36) round
the axis between the shins and the head, so the player fits inside and shows through -> drop specks -> decimate to
20k triangles -> one material slot -> SourceArt/Props/susanoo_ribs.fbx (centimetres; the exporter scales by 0.01) and
Saved/SusanooRibsPreview_front.png / _side.png.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DST = os.path.join(ROOT, "SourceArt", "Props", "susanoo_ribs.fbx")
PREVIEW = os.path.join(ROOT, "Saved", "SusanooRibsPreview")
MAX_TRIS = 20_000
SPECK_FACES = 30


def fail(message):
    print(f"[prep_susanoo_ribs] ERROR: {message}")
    sys.exit(1)


args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not args:
    fail("usage: -- <in.glb> [height_cm] [clear_radius_cm]")
src = args[0]
height = float(args[1]) if len(args) > 1 else 220.0
clear = float(args[2]) if len(args) > 2 else 36.0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    fail(f"no mesh in {src}")
for obj in bpy.context.scene.objects:
    obj.select_set(obj in meshes)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
rig = bpy.context.view_layer.objects.active
rig.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Work in centimetres: Blender metres x 100, the exporter's 0.01 brings it back.
mesh = rig.data
mesh.transform(Matrix.Rotation(math.radians(90.0), 4, "Z"))  # front -Y -> +X
zs = [v.co.z for v in mesh.vertices]
mesh.transform(Matrix.Scale(height / (max(zs) - min(zs)), 4))
xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
mesh.transform(Matrix.Translation(Vector((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs)))))

bm = bmesh.new()
bm.from_mesh(mesh)
# Hollow it: anything inside the player's own space goes (Rodin tends to fill the cage with a body or a solid core).
inside = [f for f in bm.faces if math.hypot(f.calc_center_median().x, f.calc_center_median().y) < clear and 25.0 < f.calc_center_median().z < height - 15.0]
bmesh.ops.delete(bm, geom=inside, context="FACES")
# Specks: tiny islands left over from the cut.
bm.faces.ensure_lookup_table()
seen = set()
for face in list(bm.faces):
    if face in seen or not face.is_valid:
        continue
    island, stack = [], [face]
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        island.append(f)
        stack.extend(n for e in f.edges for n in e.link_faces if n not in seen)
    if len(island) < SPECK_FACES:
        bmesh.ops.delete(bm, geom=island, context="FACES")
bm.to_mesh(mesh)
bm.free()
mesh.update()

tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
if tris > MAX_TRIS:
    mod = rig.modifiers.new("decimate", "DECIMATE")
    mod.ratio = MAX_TRIS / tris
    bpy.ops.object.modifier_apply(modifier=mod.name)
mesh.materials.clear()
mesh.materials.append(bpy.data.materials.new("M_SusanooRibs_Slot"))
rig.name = "SM_SusanooRibs"
bpy.ops.object.shade_smooth()

xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
print(f"[prep_susanoo_ribs] {sum(len(p.vertices) - 2 for p in mesh.polygons)} tris, X {min(xs):.0f}..{max(xs):.0f}, Y {min(ys):.0f}..{max(ys):.0f}, Z {min(zs):.0f}..{max(zs):.0f}")

os.makedirs(os.path.dirname(DST), exist_ok=True)
for obj in bpy.context.scene.objects:
    obj.select_set(obj == rig)
# The same settings as make_susanoo.py, whose SM_Susanoo faces +X in Unreal.
bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, axis_forward="-Y", axis_up="Z", global_scale=0.01, apply_unit_scale=True, path_mode="COPY")

os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x, scene.render.resolution_y = 600, 800
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.1, 0.1, 0.13)
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = height * 1.3
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
centre = Vector((0, 0, height / 2))
for name, offset, rot in (("front", Vector((900, 0, 0)), (math.radians(90), 0, math.radians(90))), ("side", Vector((0, -900, 0)), (math.radians(90), 0, 0))):
    cam.location = centre + offset
    cam.rotation_euler = rot
    scene.render.filepath = f"{PREVIEW}_{name}.png"
    bpy.ops.render.render(write_still=True)
```


- [ ] **Step 5: Run it and check the previews**

Run: `& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Scripts/character/prep_susanoo_ribs.py -- SourceArt/Rodin/SusanooRibs/susanoo_ribs_raw.glb`
Expected: a `[prep_susanoo_ribs] N tris, X ..., Z 0..220` line, `SourceArt/Props/susanoo_ribs.fbx`, and the two previews. Read both previews: the ribs must curve round an empty middle with gaps, facing +X. Adjust `clear_radius_cm` (third arg) if the cut eats ribs or leaves a core. **Fallback**: if the mesh is a closed shell with no rib gaps, stop and tell the user (the spec's fallback is a procedural rib mesh extending `make_susanoo.py`).

- [ ] **Step 6: Unreal import script**

`Scripts/setup_susanoo_ribs.py`:

```python
"""Imports the guard's ribcage: SourceArt/Props/susanoo_ribs.fbx (Scripts/character/prep_susanoo_ribs.py) ->
/Game/Shinobi/Jutsu/SM_SusanooRibs. Idempotent. Close the GUI editor first. No collision: AShinobiPlayerCharacter's guard figure
(UShinobiSusanooComponent) only shows it.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FBX = os.path.join(ROOT, "SourceArt", "Props", "susanoo_ribs.fbx")
DEST = "/Game/Shinobi/Jutsu"
NAME = "SM_SusanooRibs"

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

mesh = unreal.load_asset(f"{DEST}/{NAME}")
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError(f"{NAME} was not imported from {FBX}")
unreal.StaticMeshEditorSubsystem().remove_collisions(mesh)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
box = mesh.get_bounding_box()
unreal.log(f"[setup_susanoo_ribs] imported {mesh.get_path_name()}, bounds {box.min} .. {box.max}")
```

Run: `powershell -File Scripts/run_python.ps1 -Script Scripts/setup_susanoo_ribs.py`
Expected: `[setup_susanoo_ribs] imported /Game/Shinobi/Jutsu/SM_SusanooRibs.SM_SusanooRibs, bounds ...` with Z about 0..220.

- [ ] **Step 7: Run the model test**

Run: `run_tests.ps1 -Filter "ShinobiCoop.Block.Figure"`
Expected: both figure tests pass.

- [ ] **Step 8: Commit**

```bash
git add Scripts/character/prep_susanoo_ribs.py Scripts/setup_susanoo_ribs.py SourceArt/Props/susanoo_ribs.fbx SourceArt/Rodin/SusanooRibs Content/Shinobi/Jutsu/SM_SusanooRibs.uasset Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp
git commit -m "feat: the guard's Susanoo ribcage model (Rodin)"
```

---

### Task 5: Guard pose

**Files:**
- Create (user): `SourceArt/Mixamo/Standing Block Idle.fbx`
- Modify: `Scripts/setup_jutsu_anims.py:20-36` (`JUTSU`, `PHASES`), `:44` and `:152-155` (Mixamo copy)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h:170-182`, `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp:540` and `:574-606`
- Modify: `Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp`

**Interfaces:**
- Consumes: `ShinobiTags::State_Blocking`; `UGA_Block` via `EShinobiAbilityInputID::Block`.
- Produces: `/Game/Shinobi/Anims/AM_Block_Guard` (looping in `DefaultSlot`, tag-driven).

- [ ] **Step 1: Ask the user for the clip**

The user downloads Mixamo "Standing Block Idle": FBX, Without Skin, 30 fps, on the same character as `Sitting Idle.fbx` (the "Power Up" Mixamo character, so the bone names match `Power_Up_30fps_Skeleton`). Saved as `SourceArt/Mixamo/Standing Block Idle.fbx`. Wait for it.

- [ ] **Step 2: Write the failing test**

Append to `ShinobiBlockTests.cpp` (add `#include "Animation/AnimInstance.h"`, `#include "Animation/AnimMontage.h"`, `#include "Components/SkeletalMeshComponent.h"`):

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockPoseTest, "ShinobiCoop.Block.Pose.GuardsWhileHeld", ShinobiTestFlags)
bool FShinobiBlockPoseTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py with JUTSU_ONLY=Block if the montage is missing)
	const UAnimMontage* Pose = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Block_Guard.AM_Block_Guard"));
	if (!TestNotNull(TEXT("guard montage"), Pose))
	{
		return false;
	}
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UAnimInstance* Anim = P->GetMesh()->GetAnimInstance();
	if (!TestNotNull(TEXT("the character's animation instance"), Anim))
	{
		return false;
	}
	W.Tick(0.3f);
	TestFalse(TEXT("No guard pose before guarding"), Anim->Montage_IsPlaying(Pose));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	W.Tick(0.3f);
	TestTrue(TEXT("Guard pose while guarding"), Anim->Montage_IsPlaying(Pose));
	W.Tick(Pose->GetPlayLength() * 2.5f);
	TestTrue(TEXT("and still after the clip has looped"), Anim->Montage_IsPlaying(Pose));

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	W.Tick(1.f);
	TestFalse(TEXT("Gone on release"), Anim->Montage_IsPlaying(Pose));
	return true;
}
```

- [ ] **Step 3: Import the clip**

`Scripts/setup_jutsu_anims.py`:
- In `JUTSU`, after `"Land": ("Roll",),`: `"Block": ("Guard",),  # the guard pose (AShinobiPlayerCharacter; Mixamo Standing Block Idle)`
- In `PHASES`, add `"Guard": (0.15, 0.2)`.
- After the `MIXAMO_SIT` constant:

```python
# The guard pose: a Mixamo "Standing Block Idle" (FBX, without skin, on the Power Up character) as SourceArt/Mixamo/Standing Block Idle.fbx.
MIXAMO_BLOCK = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + "SourceArt/Mixamo/Standing Block Idle.fbx")
```

- After the `if os.path.exists(MIXAMO_SIT): ...` block:

```python
if os.path.exists(MIXAMO_BLOCK):
    shutil.copyfile(MIXAMO_BLOCK, os.path.join(SOURCE_DIR, "Block_Guard.fbx"))
```

and move `import shutil` to the top-level imports (it is currently inside the `MIXAMO_SIT` block).

Run (PowerShell): `$env:JUTSU_ONLY = "Block"; powershell -File Scripts/run_python.ps1 -Script Scripts/setup_jutsu_anims.py; Remove-Item Env:JUTSU_ONLY`
Expected: `[setup_jutsu_anims] /Game/Shinobi/Anims/AM_Block_Guard: N.NNs`.

- [ ] **Step 4: Play it tag-driven, like the seated loop**

`ShinobiPlayerCharacter.h`: replace the `UpdateSeatedPose` declaration, `SeatedMontage` and `bSeatedLoadTried` with:

```cpp
	/** Keeps Montage looping while the player has Tag, whenever no other montage (a jutsu, a dodge) is playing, and stops it when
	 * the tag goes. Tag-driven, so every machine shows it. Loads the montage once; a missing asset just means no pose. */
	void UpdateLoopPose(TObjectPtr<UAnimMontage>& Montage, bool& bLoadTried, const TCHAR* Path, const FGameplayTag& Tag, float BlendOutSeconds);

	/** While mounted: the seated loop (AM_Mount_Sit). */
	UPROPERTY()
	TObjectPtr<UAnimMontage> SeatedMontage;

	/** While guarding: the guard pose (AM_Block_Guard). */
	UPROPERTY()
	TObjectPtr<UAnimMontage> GuardMontage;
```

and next to the other `bool` flags:

```cpp
	bool bSeatedLoadTried = false;
	bool bGuardLoadTried = false;
```

`ShinobiPlayerCharacter.cpp`: at line 540 replace `UpdateSeatedPose();` with:

```cpp
	UpdateLoopPose(SeatedMontage, bSeatedLoadTried, TEXT("/Game/Shinobi/Anims/AM_Mount_Sit.AM_Mount_Sit"), ShinobiTags::State_Mounted, 0.3f);
	UpdateLoopPose(GuardMontage, bGuardLoadTried, TEXT("/Game/Shinobi/Anims/AM_Block_Guard.AM_Block_Guard"), ShinobiTags::State_Blocking, 0.2f);
```

Replace the whole `UpdateSeatedPose` definition with:

```cpp
void AShinobiPlayerCharacter::UpdateLoopPose(TObjectPtr<UAnimMontage>& Montage, bool& bLoadTried, const TCHAR* Path, const FGameplayTag& Tag, float BlendOutSeconds)
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (!Montage)
	{
		if (bLoadTried)
		{
			return;
		}
		bLoadTried = true; // once: a missing asset must not cost a failed load every frame
		Montage = LoadObject<UAnimMontage>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Montage)
		{
			return;
		}
	}
	if (AbilitySystem->HasMatchingGameplayTag(Tag))
	{
		if (!Anim->IsAnyMontagePlaying()) // another montage takes the pose for its moment, then the loop comes back
		{
			Anim->Montage_Play(Montage);
			Anim->Montage_SetNextSection(TEXT("Default"), TEXT("Default"), Montage); // a loop
		}
	}
	else if (Anim->Montage_IsPlaying(Montage))
	{
		Anim->Montage_Stop(BlendOutSeconds, Montage);
	}
}
```

- [ ] **Step 5: Build and run the tests**

Run: `powershell -File Scripts/build.ps1`, then `run_tests.ps1 -Filter "ShinobiCoop.Block.Pose"` and `-Filter "ShinobiCoop.Mount.TheRiderSits"` (the refactored sit loop).
Expected: both pass.

- [ ] **Step 6: Commit**

```bash
git add Scripts/setup_jutsu_anims.py "SourceArt/Mixamo/Standing Block Idle.fbx" Content/Shinobi/Mixamo/Jutsu Content/Shinobi/Anims Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiBlockTests.cpp
git commit -m "feat: guard pose while blocking"
```

---

### Task 6: Full suite, playtest, notes

**Files:**
- Modify: `C:\Users\ahmad\.claude\projects\e--ShinobiCoop\memory\` (new `block-guard.md` + `MEMORY.md` line)

- [ ] **Step 1: Full suite (the one full run)**

Run: `powershell -File Scripts/run_tests.ps1 -Filter "ShinobiCoop"`
Expected: no new failures against `feature/summon-misha`'s baseline. Any failure: list it with its output; fix only if this branch caused it.

- [ ] **Step 2: Hand the user the playtest checklist**

The user runs it (two-player PIE, listen server + client):
1. Hold F: the ribcage snaps up around the character with a violet flash, the guard pose plays, and the character stands still but turns with WASD.
2. A partner or an enemy hits you: small violet spark, about a fifth of the damage, and the stamina bar drops.
3. Keep taking hits until the stamina runs out: the ribcage vanishes, the full hit lands, and you stagger.
4. Dodge while guarding: the dodge happens and the guard drops.
5. On the client, from the client's own view: the guard appears about one ping after the press, with no stuck root after release (Review Focus 5).
6. Susanoo jutsu then F: the big and small ribcages both show, and they don't flicker.
7. The size looks right (the ribs hug the body without clipping into the arms in the guard pose). If not, rerun `prep_susanoo_ribs.py` with another height or radius, then `setup_susanoo_ribs.py`.

- [ ] **Step 3: Memory note**

Write `block-guard.md` (type project): branch `feature/block` off the GASP WIP commit `d649b09d`; guard = `UGA_Block` + `UGE_Blocking` + exec rule; generalised `UShinobiSusanooComponent` (second instance `Guard` on players); asset pipeline concept -> Rodin -> `prep_susanoo_ribs.py` -> `setup_susanoo_ribs.py`; pose via `UpdateLoopPose` because the GASP ABP only has `DefaultSlot` (an upper-body slot would allow walking guard later). Add one line to `MEMORY.md`.

- [ ] **Step 4: Commit any test fixes; report to the user**
