#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiCastFxComponent.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Shinobi/VFX/ShinobiVfx.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Substitution.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

namespace
{
	template <class T>
	T* FirstActor(const FShinobiTestWorld& W)
	{
		TActorIterator<T> It(W.World);
		return It ? *It : nullptr;
	}

	const AShinobiBurst* FindBurst(const FShinobiTestWorld& W, EShinobiBurst Kind)
	{
		for (TActorIterator<AShinobiBurst> It(W.World); It; ++It)
		{
			if (It->GetKind() == Kind)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

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
	W.Tick(0.5f);
	TestEqual(TEXT("Nothing thrown during the 0.6 s seals"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);

	// Timers started from test code between ticks start counting a tick late, so the 0.6 s seals end at ~0.65-0.7
	// and the ball leaves 0.1 s later.
	W.Tick(0.35f); // t = 0.85
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

	W.Tick(3.8f); // t = 5.05; the cooldown started at the release (~0.65-0.7) and lasts 4 s
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
	W.Tick(0.6f); // timers set before a tick start counting one tick late: the 0.4 s seals end at ~0.45-0.5
	TestTrue(TEXT("Black flame after the 0.4 s seals"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Black));
	TestFalse(TEXT("Not a normal burn"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestTrue(TEXT("Cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Amaterasu));
	W.Tick(1.2f); // t = 1.8: exactly one tick (~1.5); the next is ~2.5
	TestEqual(TEXT("15 damage per second"), ShinobiTest::Get(E, Health), 485.f);
	TestEqual(TEXT("Enemy behind untouched"), ShinobiTest::Get(Behind, Health), 500.f);
	TestEqual(TEXT("Enemy beyond range untouched"), ShinobiTest::Get(TooFar, Health), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAimPastAllyTest, "ShinobiCoop.Jutsu.AimSkipsAlliesInTheWay", ShinobiTestFlags)
bool FShinobiAimPastAllyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f)); // a teammate standing on the aim line
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1500.f, 0.f, 0.f));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestTrue(TEXT("Amaterasu finds the enemy behind the teammate"), ShinobiTest::IsActive(P, UGA_Amaterasu::StaticClass()));
	W.Tick(0.6f);
	TestTrue(TEXT("Enemy behind the teammate gets the black flame"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Black));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChidoriTest, "ShinobiCoop.Jutsu.ChidoriDashesToTargetAndShocks", ShinobiTestFlags)
bool FShinobiChidoriTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(); // aims along +X
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 100.f, 0.f)); // ~7 degrees off the aim
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 30"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 70.f);
	W.Tick(1.0f); // 0.5 s seals + ~0.18 s dash (706 units at 4000 u/s)
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
	W.Tick(0.65f); // mid-dash: the seals end at ~0.55-0.6 and the ~0.18 s dash runs past 0.7
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
	W.Tick(1.2f); // 0.5 s seals + 0.3 s dash
	const float X = P->GetActorLocation().X;
	TestTrue(FString::Printf(TEXT("Dashed ~1200 straight ahead (X = %.0f)"), X), X > 1100.f && X < 1300.f);
	TestEqual(TEXT("Enemy off to the side untouched"), ShinobiTest::Get(Side, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiRasenganTest, "ShinobiCoop.Jutsu.RasenganFirestormOnBurningTarget", ShinobiTestFlags)
bool FShinobiRasenganTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 0.f, 0.f));
	AShinobiCharacterBase* Near = W.Spawn<AShinobiCharacterBase>(FVector(950.f, 250.f, 0.f)); // 354 from E: in the Firestorm, out of reach
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 700.f, 0.f));  // 700 from E
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	ShinobiCombat::ApplyStatus(P->GetAbilitySystemComponent(), E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 35"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 65.f);
	W.Tick(0.9f); // the 0.4 s seals end at ~0.45-0.5 (pre-tick timers start one tick late), then a ~0.15 s dash; the burn's first tick is ~1.05
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
	AShinobiCharacterBase* BlackFlamed = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 0.f, 0.f));
	AShinobiPlayerCharacter* P2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	AShinobiCharacterBase* NotBurning = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 3000.f, 0.f));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiRasenganDashTest, "ShinobiCoop.Jutsu.RasenganDashesToTargetAndKnocksBack", ShinobiTestFlags)
bool FShinobiRasenganDashTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 0.f, 0.f));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	W.Tick(0.8f); // seals end ~0.45-0.5, then a 600-unit dash at 4000 u/s
	const float X = P->GetActorLocation().X;
	TestTrue(FString::Printf(TEXT("Dashed up to the target, stopping 100 short (X = %.0f)"), X), FMath::IsNearlyEqual(X, 600.f, 60.f));
	TestEqual(TEXT("150 damage"), ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute()), 350.f);
	TestTrue(FString::Printf(TEXT("Knocked back (vx %.0f)"), E->GetVelocity().X), E->GetVelocity().X > 1000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiRasenganNoTargetTest, "ShinobiCoop.Jutsu.RasenganWithoutTargetDashesFullDistance", ShinobiTestFlags)
bool FShinobiRasenganNoTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	W.Tick(0.8f); // seals end ~0.45-0.5, then a 900-unit dash (~0.23 s)
	const float X = P->GetActorLocation().X;
	// The force runs in 50 ms test steps and hands over 200 u/s at the end, so it overshoots a little (as Chidori's does).
	TestTrue(FString::Printf(TEXT("Dashed ~900 straight ahead (X = %.0f)"), X), X > 850.f && X < 1100.f);
	TestFalse(TEXT("Rasengan finished"), ShinobiTest::IsActive(P, UGA_Rasengan::StaticClass()));
	TestFalse(TEXT("Nothing left in the hand"), ShinobiTest::HasTag(P, ShinobiTags::State_Casting_Rasengan));
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSubstitutionOnceTest, "ShinobiCoop.Jutsu.SubstitutionOncePerDodge", ShinobiTestFlags)
bool FShinobiSubstitutionOnceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E1 = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));
	AShinobiCharacterBase* E2 = W.Spawn<AShinobiCharacterBase>(FVector(-300.f, 0.f, 0.f));
	const FGameplayAttribute Charges = UShinobiAttributeSet::GetSubstitutionChargesAttribute();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	ShinobiCombat::ApplyDamage(E1->GetAbilitySystemComponent(), P, 50.f);
	const FVector AfterFirst = P->GetActorLocation();
	TestFalse(TEXT("Second hit in the same i-frames doesn't land"), ShinobiCombat::ApplyDamage(E2->GetAbilitySystemComponent(), P, 50.f));
	TestEqual(TEXT("Still no damage"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("Only one charge spent per dodge"), ShinobiTest::Get(P, Charges), 2.f);
	TestTrue(TEXT("No second teleport"), P->GetActorLocation().Equals(AfterFirst));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiJutsuVfxTest, "ShinobiCoop.Jutsu.VfxFollowCastingStatusAndImpact", ShinobiTestFlags)
bool FShinobiJutsuVfxTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);

	const UShinobiCastFxComponent* Fx = P->GetCastFx();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Fireball cast shown"), Fx->GetShownTag() == ShinobiTags::State_Casting_Fireball);
	TestFalse(TEXT("Fireball: nothing in the hand"), Fx->IsPartVisible(EShinobiCastPart::HandOrb));
	TestFalse(TEXT("No mouth flame during the seals"), Fx->IsPartVisible(EShinobiCastPart::MouthFlame));
	W.Tick(0.7f);
	TestTrue(TEXT("Mouth flame while charging"), Fx->IsPartVisible(EShinobiCastPart::MouthFlame));
	W.Tick(1.f);
	TestTrue(FString::Printf(TEXT("The flame grows with the charge (%.2f)"), Fx->GetChargeVisualScale()), Fx->GetChargeVisualScale() > 1.3f);
	ShinobiTest::Release(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.2f); // thrown at ~1.8, hits at ~2.0
	TestFalse(TEXT("Effects gone once the jutsu ends"), Fx->GetShownTag().IsValid());
	TestFalse(TEXT("Mouth flame gone"), Fx->IsPartVisible(EShinobiCastPart::MouthFlame));
	TestNotNull(TEXT("Fire burst at the impact"), FindBurst(W, EShinobiBurst::Fire));
	TestTrue(TEXT("Burning target glows with the burn aura"), E->GetChakraAura()->GetShownTag() == ShinobiTags::State_Burning_Normal);
	TestEqual(TEXT("Burn aura faded in"), E->GetChakraAura()->GetFade(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCastFxPartsTest, "ShinobiCoop.Jutsu.CastFxPartsPerJutsu", ShinobiTestFlags)
bool FShinobiCastFxPartsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiCastFxComponent* Fx = P->GetCastFx();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();
	auto Shows = [Fx](EShinobiCastPart Part) { return Fx->IsPartVisible(Part); };

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestTrue(TEXT("Chidori: orb and lightning arcs"), Shows(EShinobiCastPart::HandOrb) && Shows(EShinobiCastPart::LightningArcs));
	TestFalse(TEXT("Chidori: no Rasengan shells"), Shows(EShinobiCastPart::RasenganShells));
	W.Tick(1.5f); // seals, full dash, end
	TestFalse(TEXT("Chidori over: arcs hidden"), Shows(EShinobiCastPart::LightningArcs));

	ShinobiTest::Set(P, Chakra, 100.f);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestTrue(TEXT("Rasengan: orb and shells"), Shows(EShinobiCastPart::HandOrb) && Shows(EShinobiCastPart::RasenganShells));
	TestFalse(TEXT("Rasengan: no lightning"), Shows(EShinobiCastPart::LightningArcs));
	W.Tick(1.5f);

	ShinobiTest::Set(P, Chakra, 100.f);
	W.Spawn<AShinobiCharacterBase>(P->GetActorLocation() + FVector(1000.f, 0.f, 0.f));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestTrue(TEXT("Amaterasu cast"), ShinobiTest::IsActive(P, UGA_Amaterasu::StaticClass()));
	TestTrue(TEXT("Amaterasu: glowing, bleeding eyes"), Shows(EShinobiCastPart::EyeGlow) && Shows(EShinobiCastPart::BloodStreaks));
	TestFalse(TEXT("Amaterasu: nothing in the hand"), Shows(EShinobiCastPart::HandOrb));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeTapTest, "ShinobiCoop.Jutsu.ChargeTapFiresUnchargedAfterSeals", ShinobiTestFlags)
bool FShinobiChargeTapTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.5f);
	TestEqual(TEXT("Nothing thrown during the 0.6 s seals"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);
	TestFalse(TEXT("No charge phase on a tap"), ShinobiTest::HasTag(P, ShinobiTags::State_Casting_Charging));
	W.Tick(0.35f); // t = 0.85: the seals end at ~0.65-0.7, the ball leaves 0.1 s later
	const AShinobiProjectile* Ball = FirstActor<AShinobiProjectile>(W);
	TestNotNull(TEXT("Thrown after the seals"), Ball);
	TestEqual(TEXT("Uncharged size"), Ball ? Ball->Scale : -1.f, 1.f);
	const float Left = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("No drain on a tap: cost 20, +2/s regen (got %.2f)"), Left), FMath::IsNearlyEqual(Left, 80.f + 2.f * 0.85f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeHoldTest, "ShinobiCoop.Jutsu.ChargeHoldScalesAndDrains", ShinobiTestFlags)
bool FShinobiChargeHoldTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.7f);
	TestTrue(TEXT("Charging after the seals while held"), ShinobiTest::HasTag(P, ShinobiTags::State_Casting_Charging));
	W.Tick(1.f); // charged ~1.0-1.05 s: L ~ 0.5
	ShinobiTest::Release(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Charging tag gone on release"), ShinobiTest::HasTag(P, ShinobiTags::State_Casting_Charging));
	W.Tick(0.2f);
	const AShinobiProjectile* Ball = FirstActor<AShinobiProjectile>(W);
	const float Scale = Ball ? Ball->Scale : -1.f;
	TestTrue(FString::Printf(TEXT("Half-charged ball is ~1.4x (got %.3f)"), Scale), FMath::IsNearlyEqual(Scale, 1.4f, 0.06f));
	const float Left = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("Cost 20, ~5 drained, +2/s regen (got %.2f)"), Left), FMath::IsNearlyEqual(Left, 75.f + 2.f * 1.9f, 1.f));
	W.Tick(0.4f);
	const float Health = ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute());
	TestTrue(FString::Printf(TEXT("Hit for ~60 x 1.375 (health %.1f)"), Health), FMath::IsNearlyEqual(Health, 500.f - 60.f * 1.375f, 2.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeCapTest, "ShinobiCoop.Jutsu.ChargeAutoReleasesAtCap", ShinobiTestFlags)
bool FShinobiChargeCapTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W; // no enemy: the ball must still be in flight when checked
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);

	ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu1); // never released
	W.Tick(3.f); // seals end at ~0.7, the cap at ~2.7-2.75 (0.1 s charge ticks), the ball leaves 0.1 s later
	const AShinobiProjectile* Ball = FirstActor<AShinobiProjectile>(W);
	TestNotNull(TEXT("Auto-released at the 2 s cap"), Ball);
	const float Scale = Ball ? Ball->Scale : -1.f;
	TestTrue(FString::Printf(TEXT("Full charge is 1.8x (got %.3f)"), Scale), FMath::IsNearlyEqual(Scale, 1.8f, 0.06f));
	const float Left = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Cost 20, 10 drained, +2/s regen (got %.2f)"), Left), FMath::IsNearlyEqual(Left, 70.f + 2.f * 3.f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeEmptyTest, "ShinobiCoop.Jutsu.ChargeAutoReleasesWhenChakraRunsOut", ShinobiTestFlags)
bool FShinobiChargeEmptyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();
	ShinobiTest::Set(P, Chakra, 22.f);

	// 2 left after the cost, ~3 with the +2/s regen by the end of the seals; drain 5/s minus regen 2/s runs dry after
	// ~1.1 s of charging, well short of the 2 s cap.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.6f + 1.6f);
	const AShinobiProjectile* Ball = FirstActor<AShinobiProjectile>(W);
	TestNotNull(TEXT("Released when chakra ran out"), Ball);
	TestTrue(TEXT("Chakra never negative"), ShinobiTest::Get(P, Chakra) >= 0.f);
	const float Scale = Ball ? Ball->Scale : -1.f;
	TestTrue(FString::Printf(TEXT("Partial charge (got %.3f)"), Scale), Scale > 1.1f && Scale < 1.7f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeInterruptTest, "ShinobiCoop.Jutsu.InterruptDuringChargeRefundsBaseHalf", ShinobiTestFlags)
bool FShinobiChargeInterruptTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);

	ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.6f + 1.f);
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();
	const float Before = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("Charging drained chakra (%.2f left)"), Before), Before < 80.f);
	ShinobiTest::Damage(P, 25.f);
	TestFalse(TEXT("A staggering hit interrupts the charge"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestFalse(TEXT("Charging tag removed"), ShinobiTest::HasTag(P, ShinobiTags::State_Casting_Charging));
	const float Refund = ShinobiTest::Get(P, Chakra) - Before;
	TestTrue(FString::Printf(TEXT("Refunds half the base cost, not the drain (got %.2f)"), Refund), FMath::IsNearlyEqual(Refund, 10.f, 0.6f));
	TestFalse(TEXT("No cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	W.Tick(0.5f);
	TestEqual(TEXT("Nothing thrown"), ShinobiTest::CountActors<AShinobiProjectile>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargedHitsTest, "ShinobiCoop.Jutsu.ChargedHitsScaleDamage", ShinobiTestFlags)
bool FShinobiChargedHitsTest::RunTest(const FString& Parameters)
{
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	{
		// Chidori held to the cap: 120 x 1.75, and a full-size lightning burst.
		FShinobiTestWorld W;
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
		AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
		ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu2);
		W.Tick(3.f); // seals end ~0.55-0.6, the cap ~2.6-2.7, then a ~0.13 s dash
		TestTrue(FString::Printf(TEXT("Full-charge Chidori hits for 210 (health %.1f)"), ShinobiTest::Get(E, Health)), FMath::IsNearlyEqual(ShinobiTest::Get(E, Health), 290.f, 1.f));
		const AShinobiBurst* Bolt = FindBurst(W, EShinobiBurst::Lightning);
		TestTrue(TEXT("Lightning burst at full size"), Bolt && FMath::IsNearlyEqual(Bolt->GetScale(), 1.8f, 0.01f));
	}
	{
		// Amaterasu half-charged: a bigger ignition and a stronger black flame.
		FShinobiTestWorld W;
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
		AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1500.f, 0.f, 0.f));
		ShinobiTest::Press(P, EShinobiAbilityInputID::Jutsu4);
		W.Tick(0.5f); // seals end ~0.45-0.5
		W.Tick(1.f);
		ShinobiTest::Release(P, EShinobiAbilityInputID::Jutsu4); // charged ~1.0-1.05 s
		W.Tick(0.1f);
		const AShinobiBurst* Ignite = FindBurst(W, EShinobiBurst::BlackFlameIgnite);
		const float Scale = Ignite ? Ignite->GetScale() : -1.f;
		TestTrue(FString::Printf(TEXT("Ignition burst ~1.4x (got %.3f)"), Scale), FMath::IsNearlyEqual(Scale, 1.4f, 0.06f));
		W.Tick(1.1f); // the first black-flame tick is 1 s after the release
		const float Left = ShinobiTest::Get(E, Health);
		TestTrue(FString::Printf(TEXT("One tick of 15 x ~1.38 (health %.1f)"), Left), FMath::IsNearlyEqual(Left, 500.f - 15.f * 1.385f, 1.f));
	}
	{
		// Fireball tap: fire breath at the mouth, uncharged.
		FShinobiTestWorld W;
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
		ShinobiTest::Hover(P);
		ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
		W.Tick(0.85f);
		const AShinobiBurst* Breath = FindBurst(W, EShinobiBurst::FireBreath);
		TestNotNull(TEXT("Fire breath on the release"), Breath);
		if (Breath)
		{
			const FVector Head = P->GetBodyMesh()->GetSocketLocation(P->GetHeadBone());
			const float Gap = FVector::Dist(Breath->GetActorLocation(), Head);
			TestTrue(FString::Printf(TEXT("Breath starts at the mouth (%.0f from the head bone)"), Gap), Gap < 40.f);
			TestEqual(TEXT("Uncharged breath"), Breath->GetScale(), 1.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFlameMaterialsTest, "ShinobiCoop.Jutsu.FlameMaterialsLoadWithParameters", ShinobiTestFlags)
bool FShinobiFlameMaterialsTest::RunTest(const FString& Parameters)
{
	UObject* Outer = GetTransientPackage();
	for (const bool bDark : { false, true })
	{
		const UMaterialInstanceDynamic* Flame = ShinobiVfx::MakeFlame(Outer, ShinobiVfx::Colors::Fire, 3.f, bDark);
		TestNotNull(bDark ? TEXT("Dark flame material") : TEXT("Flame material"), Flame);
		float Erode = -1.f;
		float Fade = -1.f;
		TestTrue(TEXT("Has Erode"), Flame && Flame->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Erode")), Erode));
		TestTrue(TEXT("Has Fade (bursts fade it out)"), Flame && Flame->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Fade")), Fade));
	}
	const UMaterialInstanceDynamic* Streak = ShinobiVfx::MakeStreak(Outer, ShinobiVfx::Colors::Blood);
	FLinearColor Color;
	TestTrue(TEXT("Streak material takes a Color"), Streak && Streak->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Color));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemyJutsuTest, "ShinobiCoop.Jutsu.EnemyJutsuFiresUncharged", ShinobiTestFlags)
bool FShinobiEnemyJutsuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiRogueJonin* Boss = W.Spawn<AShinobiRogueJonin>();
	ShinobiTest::Hover(Boss);

	TestTrue(TEXT("Boss fireball activates"), Boss->GetAbilitySystemComponent()->TryActivateAbilityByClass(UGA_EnemyFireball::StaticClass()));
	W.Tick(0.8f + 0.1f + 0.15f); // 0.8 s seals + 0.1 s release delay + timer lag; there's no input to hold
	const AShinobiProjectile* Ball = FirstActor<AShinobiProjectile>(W);
	TestNotNull(TEXT("Thrown after the seals"), Ball);
	TestEqual(TEXT("Uncharged"), Ball ? Ball->Scale : -1.f, 1.f);
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
