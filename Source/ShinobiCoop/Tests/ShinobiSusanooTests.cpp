#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooDamageTest, "ShinobiCoop.Susanoo.HalvesDamageTakenAndBoostsDamageDealt", ShinobiTestFlags)
bool FShinobiSusanooDamageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	AShinobiCharacterBase* Enemy2 = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	TestTrue(TEXT("Susanoo is on"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestTrue(TEXT("An enemy's 100 lands"), ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f));
	TestEqual(TEXT("but only half of it does"), ShinobiTest::Get(P, Health), 450.f);

	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Enemy, 100.f);
	TestEqual(TEXT("The caster's 100 hits for 130"), ShinobiTest::Get(Enemy, Health), 370.f);

	// Susanoo on both sides: 100 x 1.3 x 0.5.
	ShinobiTest::ApplyEffect(Enemy2, UGE_Susanoo::StaticClass());
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Enemy2, 100.f);
	TestEqual(TEXT("Armour against armour: x1.3 x0.5"), ShinobiTest::Get(Enemy2, Health), 435.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooPeriodicTest, "ShinobiCoop.Susanoo.HalvesBurningAndEndsAfterTwentySeconds", ShinobiTestFlags)
bool FShinobiSusanooPeriodicTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::ApplySetByCaller(P, UGE_BlackFlame::StaticClass(), ShinobiTags::Data_Damage, 20.f); // 20 a second, self-inflicted
	W.Tick(2.5f);
	// Two ticks of 20: the caster is both source and target here, so x1.3 x0.5 = 13 each.
	TestEqual(TEXT("Burning damage is scaled too"), ShinobiTest::Get(P, Health), 474.f);

	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning_Black));
	W.Tick(17.0f); // 19.5 s since the cast
	TestTrue(TEXT("Still on just before 20 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	W.Tick(1.0f);
	TestFalse(TEXT("Over after 20 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestEqual(TEXT("and damage is full again"), ShinobiTest::Get(P, Health), 374.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooChakraTest, "ShinobiCoop.Susanoo.RegeneratesChakraFaster", ShinobiTestFlags)
bool FShinobiSusanooChakraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Set(P, Chakra, 0.f);
	W.Tick(2.0f);
	const float Normal = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("Normally about 4 in two seconds (%.1f)"), Normal), Normal > 2.f && Normal < 6.f);

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::Set(P, Chakra, 0.f);
	W.Tick(2.0f);
	const float Boosted = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("With Susanoo about 12 (%.1f)"), Boosted), Boosted > 9.f && Boosted < 15.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
