#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Substitution.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
