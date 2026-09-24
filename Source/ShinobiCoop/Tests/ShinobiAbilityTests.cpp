#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Melee.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintTest, "ShinobiCoop.Abilities.SprintDrainsStaminaThenRegenAfterDelay", ShinobiTestFlags)
bool FShinobiSprintTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	// Max speed is not asserted: test actors have no floor, so they are falling and sprint speed only applies when walking.
	TestTrue(TEXT("Movement sprinting"), Move->IsSprinting());

	W.Tick(1.f);
	const float AfterSprint = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Stamina drained ~10 (got %.2f)"), AfterSprint), AfterSprint >= 86.f && AfterSprint <= 91.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint ended on release"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting"), Move->IsSprinting());

	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after sprint"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), AfterSprint);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > AfterSprint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintExhaustTest, "ShinobiCoop.Abilities.SprintEndsAtZeroStamina", ShinobiTestFlags)
bool FShinobiSprintExhaustTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 5.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(1.f);
	TestFalse(TEXT("Sprint ended when exhausted"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Stamina empty"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	TestFalse(TEXT("Movement not sprinting after exhaustion"), Move->IsSprinting());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Can't start sprint at 0 stamina"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintCancelTest, "ShinobiCoop.Abilities.SprintCancelClearsFlagAndDelaysRegen", ShinobiTestFlags)
bool FShinobiSprintCancelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(0.3f);

	FGameplayTagContainer Tags(ShinobiTags::Ability_Sprint);
	P->GetAbilitySystemComponent()->CancelAbilities(&Tags);

	TestFalse(TEXT("Sprint cancelled"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting after cancel"), Move->IsSprinting());

	const float S = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), S);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > S);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDodgeTest, "ShinobiCoop.Abilities.DodgeCostsStaminaAndGrantsIFrames", ShinobiTestFlags)
bool FShinobiDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FVector Start = P->GetActorLocation();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before dodge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	const float Before = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("Dodge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Dodge cost 25"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), Before - 25.f);
	TestTrue(TEXT("Invulnerable"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("I-frames block damage"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	W.Tick(0.15f);
	TestTrue(TEXT("Still invulnerable mid-dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	W.Tick(0.25f);
	TestFalse(TEXT("Dodge finished"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("I-frames over"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));

	const FVector Displacement = (P->GetActorLocation() - Start) * FVector(1.f, 1.f, 0.f);
	TestTrue(FString::Printf(TEXT("Backstep ~600 on -X (got %s)"), *Displacement.ToString()), Displacement.X < -550.f && Displacement.X > -700.f);

	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Damage after dodge"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 450.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 20.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge blocked below 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("No stamina spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 20.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 25.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active at exactly 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("Stamina exactly spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraChargeTest, "ShinobiCoop.Abilities.ChakraChargeRefillsWhileHeld", ShinobiTestFlags)
bool FShinobiChakraChargeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	// At full chakra (the default), pressing charge should do nothing: no ability, no tag.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Can't charge at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("No charging tag at full chakra"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 0.f);

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging tag"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(1.1f);
	const float Charged = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("~27.5 chakra after 1.1s charge (got %.2f)"), Charged), Charged >= 26.5f && Charged <= 28.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge ended"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(0.5f);
	TestTrue(TEXT("Only passive regen after release"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) <= Charged + 1.5f);

	// Chakra is well below max here, so charging interacts normally with sprint/dodge.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before charge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestTrue(TEXT("Charge active"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint blocked while charging"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge cancels charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed by dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));

	// Let the dodge finish: GA_ChakraCharge blocks activation while State.Dodging is present, and
	// that tag stays granted for Dodge's full 0.25 s duration, not just until it cancels the charge.
	W.Tick(0.4f);
	TestFalse(TEXT("Dodge finished before next charge attempt"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	// Auto-end when charging pushes chakra up to the cap.
	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 95.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charge started from 95"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	W.Tick(0.6f);
	TestFalse(TEXT("Charge auto-ends at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestEqual(TEXT("Chakra capped at max"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeComboTest, "ShinobiCoop.Abilities.MeleeComboHitsHostilesOnly", ShinobiTestFlags)
bool FShinobiMeleeComboTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator); // faces +X
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(120.f, 40.f, 0.f));
	AShinobiCharacterBase* FarEnemy = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Set(Attacker, UShinobiAttributeSet::GetChakraAttribute(), 50.f);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 1: 20 damage"), ShinobiTest::Get(Enemy, Health), 480.f);
	TestEqual(TEXT("Ally untouched"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("Far enemy untouched"), ShinobiTest::Get(FarEnemy, Health), 500.f);
	const float Chakra = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("+5 chakra on hit (got %.2f)"), Chakra), Chakra >= 55.f && Chakra <= 56.5f);

	// Attacker chakra is 55.5, well below the 100 max, so this can only be blocked by GA_Melee's
	// BlockAbilitiesWithTag, not by GA_ChakraCharge's own full-chakra gate.
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge blocked mid-combo"), ShinobiTest::IsActive(Attacker, UGA_ChakraCharge::StaticClass()));
	ShinobiTest::Release(Attacker, EShinobiAbilityInputID::ChakraCharge);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 2: 25 damage"), ShinobiTest::Get(Enemy, Health), 455.f);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 3: 40 damage"), ShinobiTest::Get(Enemy, Health), 415.f);
	// LaunchCharacter only sets PendingLaunchVelocity; CharacterMovementComponent applies it to Velocity
	// on its next tick, which lands after this one (the hit frame's timer fires from FTimerManager::Tick,
	// which runs after this substep's tick groups, i.e. after this substep's movement tick already ran).
	W.Tick(0.1f);
	TestTrue(FString::Printf(TEXT("Finisher knockback (got %.2f)"), Enemy->GetVelocity().X), Enemy->GetVelocity().X > 100.f);

	W.Tick(0.5f);
	TestFalse(TEXT("Combo ended"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeWindowTest, "ShinobiCoop.Abilities.MeleeComboResetsAfterWindow", ShinobiTestFlags)
bool FShinobiMeleeWindowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(1.f); // hit at 0.25, window closes at 0.75
	TestFalse(TEXT("Ended after window"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("New combo restarts at hit 1 (20 + 20)"), ShinobiTest::Get(Enemy, Health), 460.f);

	// Invulnerable target: the hit is blocked, so it doesn't count -- no chakra for the attacker.
	Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(ShinobiTags::State_Invulnerable);
	const float ChakraBeforeBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	W.Tick(0.7f); // let the still-active combo's window close (it's at 0.3 of 0.75s; extra margin for timer jitter)
	TestFalse(TEXT("Second combo ended before third attempt"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Invulnerable target untouched"), ShinobiTest::Get(Enemy, Health), 460.f);
	const float ChakraGainedFromBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute()) - ChakraBeforeBlockedHit;
	TestTrue(FString::Printf(TEXT("No chakra from a blocked hit, only passive regen (got +%.2f)"), ChakraGainedFromBlockedHit), ChakraGainedFromBlockedHit < 5.f);
	Enemy->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShinobiTags::State_Invulnerable);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
