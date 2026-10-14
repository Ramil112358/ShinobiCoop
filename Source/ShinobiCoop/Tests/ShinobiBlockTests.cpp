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
