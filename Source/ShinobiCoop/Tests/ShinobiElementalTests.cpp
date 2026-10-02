#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillSlowsTest, "ShinobiCoop.Chill.SlowsMovementRefreshesAndExpires", ShinobiTestFlags)
bool FShinobiChillSlowsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>();
	const UCharacterMovementComponent* Movement = E->GetCharacterMovement();
	const float Normal = Movement->GetMaxSpeed();
	TestTrue(TEXT("A base speed to compare with"), Normal > 100.f);

	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass());
	TestTrue(TEXT("Chilled"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("Moves at 60%"), Movement->GetMaxSpeed(), Normal * 0.6f);

	W.Tick(2.5f);
	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass()); // another hit refreshes it instead of stacking
	W.Tick(2.5f);
	TestTrue(TEXT("Still chilled 5 s after the first hit"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	W.Tick(0.8f);
	TestFalse(TEXT("Three seconds after the last hit it wears off"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("and the speed is back"), Movement->GetMaxSpeed(), Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillAuraTest, "ShinobiCoop.Chill.TintsTheBody", ShinobiTestFlags)
bool FShinobiChillAuraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Chill::StaticClass());
	TestTrue(TEXT("The aura shows the chill"), P->GetChakraAura()->GetShownTag() == ShinobiTags::State_Chilled);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
