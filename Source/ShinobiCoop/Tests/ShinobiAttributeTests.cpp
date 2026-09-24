#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAttributeDefaultsTest, "ShinobiCoop.Core.AttributeDefaults", ShinobiTestFlags)
bool FShinobiAttributeDefaultsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	TestTrue(TEXT("BeginPlay routed"), C->HasActorBegunPlay());
	TestEqual(TEXT("Health"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("MaxHealth"), ShinobiTest::Get(C, UShinobiAttributeSet::GetMaxHealthAttribute()), 500.f);
	TestEqual(TEXT("Chakra"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	TestEqual(TEXT("Stamina"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	TestEqual(TEXT("SubstitutionCharges"), ShinobiTest::Get(C, UShinobiAttributeSet::GetSubstitutionChargesAttribute()), 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDamageClampsTest, "ShinobiCoop.Core.DamageReducesHealthAndClampsAtZero", ShinobiTestFlags)
bool FShinobiDamageClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Damage(C, 120.f);
	TestEqual(TEXT("Health after 120 damage"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 380.f);
	TestEqual(TEXT("IncomingDamage reset"), ShinobiTest::Get(C, UShinobiAttributeSet::GetIncomingDamageAttribute()), 0.f);
	ShinobiTest::Damage(C, 1000.f);
	TestEqual(TEXT("Health clamps at 0"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutOfHealthOnceTest, "ShinobiCoop.Core.OutOfHealthFiresOncePerDeath", ShinobiTestFlags)
bool FShinobiOutOfHealthOnceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	int32 Deaths = 0;
	C->GetAttributes()->OnOutOfHealth.AddLambda([&Deaths]() { ++Deaths; });

	ShinobiTest::Damage(C, 600.f);
	ShinobiTest::Damage(C, 10.f);
	TestEqual(TEXT("One death after overkill + extra hit"), Deaths, 1);

	ShinobiTest::Set(C, UShinobiAttributeSet::GetHealthAttribute(), 500.f);
	ShinobiTest::Damage(C, 600.f);
	TestEqual(TEXT("Second death after reset"), Deaths, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraGainClampsTest, "ShinobiCoop.Core.ChakraGainClampsToMax", ShinobiTestFlags)
bool FShinobiChakraGainClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetChakraAttribute(), 98.f);
	ShinobiTest::ApplySetByCaller(C, UGE_ChakraGain::StaticClass(), ShinobiTags::Data_ChakraGain, 5.f);
	TestEqual(TEXT("Chakra clamped"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMaxShrinkClampsTest, "ShinobiCoop.Core.LoweringMaxClampsCurrent", ShinobiTestFlags)
bool FShinobiMaxShrinkClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetMaxHealthAttribute(), 300.f);
	TestEqual(TEXT("Health clamped to lowered max"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 300.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInvulnerableTest, "ShinobiCoop.Core.InvulnerableIgnoresDamage", ShinobiTestFlags)
bool FShinobiInvulnerableTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::ApplyEffect(C, UGE_DodgeIFrames::StaticClass());
	ShinobiTest::Damage(C, 50.f);
	TestEqual(TEXT("No damage during i-frames"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	W.Tick(0.3f);
	ShinobiTest::Damage(C, 50.f);
	TestEqual(TEXT("Damage after i-frames"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraRegenTest, "ShinobiCoop.Core.ChakraRegensTwoPerSecond", ShinobiTestFlags)
bool FShinobiChakraRegenTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetChakraAttribute(), 0.f);
	W.Tick(1.f);
	const float Chakra = ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Chakra ~2 after 1s (got %.2f)"), Chakra), Chakra >= 1.4f && Chakra <= 2.6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStaminaRegenDelayTest, "ShinobiCoop.Core.StaminaRegenWaitsForDelay", ShinobiTestFlags)
bool FShinobiStaminaRegenDelayTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetStaminaAttribute(), 50.f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.9f);
	TestEqual(TEXT("No regen during delay"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 50.f);
	W.Tick(0.6f);
	const float Stamina = ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Regen resumed (got %.2f)"), Stamina), Stamina > 50.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
