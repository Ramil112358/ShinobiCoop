#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Shinobi/ShinobiTrainingDummy.h"
#include "HAL/IConsoleManager.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

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

	// A negative gain drains chakra (used by the ShinobiDrainChakra dev command) and clamps at 0.
	ShinobiTest::ApplySetByCaller(C, UGE_ChakraGain::StaticClass(), ShinobiTags::Data_ChakraGain, -60.f);
	TestEqual(TEXT("Chakra drained"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 40.f);
	ShinobiTest::ApplySetByCaller(C, UGE_ChakraGain::StaticClass(), ShinobiTags::Data_ChakraGain, -1000.f);
	TestEqual(TEXT("Chakra drain clamps at 0"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 0.f);
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
	W.Tick(0.4f);
	TestFalse(TEXT("I-frames expired"), ShinobiTest::HasTag(C, ShinobiTags::State_Invulnerable));
	ShinobiTest::Damage(C, 50.f);
	TestEqual(TEXT("Damage after i-frames"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraRegenTest, "ShinobiCoop.Core.ChakraRegensTwoPerSecond", ShinobiTestFlags)
bool FShinobiChakraRegenTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	W.Tick(0.1f); // Let the deferred on-application tick from the startup UGE_ChakraRegen happen first.
	ShinobiTest::Set(C, UShinobiAttributeSet::GetChakraAttribute(), 0.f);
	W.Tick(1.f);
	const float Chakra = ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Chakra ~2 after 1s (got %.2f)"), Chakra), FMath::IsNearlyEqual(Chakra, 2.f, 0.01f));
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

	// The block should last 1 s from the LATEST application, not the first: each application is its own
	// instance, so re-applying mid-block keeps the tag granted past when the first instance would have expired.
	ShinobiTest::Set(C, UShinobiAttributeSet::GetStaminaAttribute(), 50.f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.5f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.9f);
	TestEqual(TEXT("Still blocked past the first instance's expiry"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 50.f);
	W.Tick(0.6f);
	const float StaminaAfterReapply = ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Regen resumed after latest delay (got %.2f)"), StaminaAfterReapply), StaminaAfterReapply > 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDummyResetTest, "ShinobiCoop.Core.TrainingDummyResetsAfterDefeat", ShinobiTestFlags)
bool FShinobiDummyResetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiTrainingDummy* Dummy = W.Spawn<AShinobiTrainingDummy>();
	ShinobiTest::Damage(Dummy, 1000.f);
	TestEqual(TEXT("Dummy defeated"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Still defeated before the reset delay"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Dummy reset to full"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	// The reset timer must re-arm: a second defeat should reset again, not just fire once ever.
	ShinobiTest::Damage(Dummy, 1000.f);
	TestEqual(TEXT("Dummy defeated again"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(2.f);
	TestEqual(TEXT("Dummy reset to full again"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDummyAttackTest, "ShinobiCoop.Core.DummyAttackCheatPunchesNearestPlayer", ShinobiTestFlags)
bool FShinobiDummyAttackTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* DummyAttack = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.DummyAttack"));
	if (!TestNotNull(TEXT("Shinobi.DummyAttack exists"), DummyAttack))
	{
		return false;
	}
	// ECVF_SetByConsole so this wins over (and cleanly restores) a value someone typed into the console this session --
	// ECVF_SetByCode would lose that fight and leave the cvar on Shinobi.DummyAttack 1 dummies attacking after the test.
	TGuardConsoleVariable<bool> DummyAttackGuard(DummyAttack, true, ECVF_SetByConsole);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDummyPerfectDodgeTest, "ShinobiCoop.Core.DummyPunchPerfectDodgeSubstitutes", ShinobiTestFlags)
bool FShinobiDummyPerfectDodgeTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* DummyAttack = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.DummyAttack"));
	if (!TestNotNull(TEXT("Shinobi.DummyAttack exists"), DummyAttack))
	{
		return false;
	}
	TGuardConsoleVariable<bool> DummyAttackGuard(DummyAttack, true, ECVF_SetByConsole);

	FShinobiTestWorld W;
	AShinobiTrainingDummy* Dummy = W.Spawn<AShinobiTrainingDummy>();
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(200.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f)); // faces the dummy

	W.Tick(1.9f); // the telegraph started at ~1.6 and locked P; the punch lands at ~2.0
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge); // backstep away from the dummy, i-frames until ~2.1
	W.Tick(0.3f);
	TestEqual(TEXT("Perfect dodge: no damage"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("Substitution spent a charge"), ShinobiTest::Get(P, UShinobiAttributeSet::GetSubstitutionChargesAttribute()), 2.f);
	TestTrue(FString::Printf(TEXT("Teleported behind the dummy (X = %.0f)"), P->GetActorLocation().X), P->GetActorLocation().X < -100.f);
	TestTrue(TEXT("The dummy faced its target"), FMath::Abs(FRotator::NormalizeAxis(Dummy->GetActorRotation().Yaw)) < 5.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
