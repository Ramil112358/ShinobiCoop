#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOffPageJutsuTest, "ShinobiCoop.JutsuPages.OffPageJutsuDoesNotFire", ShinobiTestFlags)
bool FShinobiOffPageJutsuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	TestTrue(TEXT("Starts on page 1"), ShinobiTest::HasTag(P, ShinobiTags::JutsuPage(1)));
	P->ApplyJutsuPage(2);
	TestFalse(TEXT("Page 1 tag removed"), ShinobiTest::HasTag(P, ShinobiTags::JutsuPage(1)));
	TestEqual(TEXT("On page 2"), P->GetJutsuPage(), 2);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Fireball is on page 1: key 1 does nothing on page 2"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Nothing spent"), ShinobiTest::Get(P, Chakra), 100.f);

	P->ApplyJutsuPage(1);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Back on page 1 the Fireball fires"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Cost paid"), ShinobiTest::Get(P, Chakra), 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemyJutsuIgnoresPagesTest, "ShinobiCoop.JutsuPages.EnemyJutsuIgnoresPages", ShinobiTestFlags)
bool FShinobiEnemyJutsuIgnoresPagesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>();
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ASC->GiveAbility(FGameplayAbilitySpec(UGA_EnemyFireball::StaticClass(), 1, INDEX_NONE));
	TestTrue(TEXT("An enemy has no page tag and still casts"), ASC->TryActivateAbilityByClass(UGA_EnemyFireball::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiServerRejectsPageTest, "ShinobiCoop.JutsuPages.ServerRejectsOutOfRangePage", ShinobiTestFlags)
bool FShinobiServerRejectsPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	P->ServerSetJutsuPage_Implementation(9);
	TestEqual(TEXT("A client's out-of-range page is ignored"), P->GetJutsuPage(), 1);
	P->ServerSetJutsuPage_Implementation(0);
	TestEqual(TEXT("Page 0 is ignored too"), P->GetJutsuPage(), 1);
	P->ServerSetJutsuPage_Implementation(3);
	TestEqual(TEXT("A valid page is taken"), P->GetJutsuPage(), 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
