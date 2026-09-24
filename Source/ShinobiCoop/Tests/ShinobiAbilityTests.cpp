#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"

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
	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 0.f);

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging tag"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(1.1f);
	const float Charged = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("~27.5 chakra after 1.1s charge (got %.2f)"), Charged), Charged >= 24.f && Charged <= 35.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge ended"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(0.5f);
	TestTrue(TEXT("Only passive regen after release"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) <= Charged + 1.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
