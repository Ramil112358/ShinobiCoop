#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
