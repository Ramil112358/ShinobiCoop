#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemyMeleeTest, "ShinobiCoop.Enemies.RogueNinjaChasesAndHitsTwice", ShinobiTestFlags)
bool FShinobiEnemyMeleeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>();
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::Hover(P);
	TestNotNull(TEXT("AI controller spawned"), Ninja->GetController());

	W.Tick(1.f);
	const float Distance = FVector::Dist2D(Ninja->GetActorLocation(), P->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Closed in (%.0f)"), Distance), Distance < 200.f);
	W.Tick(1.5f);
	TestEqual(TEXT("Two-hit combo landed: 2 x 25"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKunaiTest, "ShinobiCoop.Enemies.KunaiThrowerHitsFromRange", ShinobiTestFlags)
bool FShinobiKunaiTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiKunaiThrower* Thrower = W.Spawn<AShinobiKunaiThrower>();
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(1000.f, 0.f, 0.f));
	ShinobiTest::Hover(Thrower);
	ShinobiTest::Hover(P);

	W.Tick(1.5f); // 0.6 s wind-up + ~0.35 s flight; next throw no earlier than 2.2 s
	TestEqual(TEXT("One kunai for 30"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 470.f);
	TestTrue(TEXT("Kept its distance"), FVector::Dist2D(Thrower->GetActorLocation(), P->GetActorLocation()) > 700.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemyDeathTest, "ShinobiCoop.Enemies.DiesOnceThenDisappears", ShinobiTestFlags)
bool FShinobiEnemyDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiRogueNinja* Ninja = W.World->SpawnActorDeferred<AShinobiRogueNinja>(AShinobiRogueNinja::StaticClass(), FTransform::Identity);
	Ninja->HealthScale = 1.5f;
	Ninja->FinishSpawning(FTransform::Identity);
	ShinobiTest::Hover(Ninja);
	TestEqual(TEXT("HP scaled: 200 x 1.5"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetMaxHealthAttribute()), 300.f);
	TestEqual(TEXT("Starts full"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()), 300.f);

	int32 Deaths = 0;
	Ninja->OnDied.AddLambda([&Deaths](AShinobiEnemyCharacter*) { ++Deaths; });
	ShinobiTest::Damage(Ninja, 400.f);
	ShinobiTest::Damage(Ninja, 50.f);
	TestEqual(TEXT("OnDied fired once"), Deaths, 1);
	TestTrue(TEXT("State.Dead"), ShinobiTest::HasTag(Ninja, ShinobiTags::State_Dead));
	TestFalse(TEXT("Not alive"), Ninja->IsAlive());

	W.Tick(AShinobiEnemyCharacter::DeathLifeSpan + 0.2f);
	TestEqual(TEXT("Body removed"), ShinobiTest::CountActors<AShinobiRogueNinja>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBossStunTest, "ShinobiCoop.Enemies.BossShrugsOffStuns", ShinobiTestFlags)
bool FShinobiBossStunTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiRogueJonin* Boss = W.Spawn<AShinobiRogueJonin>();
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Boss);
	ShinobiTest::Hover(P);
	ShinobiCombat::ApplyStun(P->GetAbilitySystemComponent(), Boss);
	TestTrue(TEXT("Stunned"), ShinobiTest::HasTag(Boss, ShinobiTags::State_Shocked));
	W.Tick(UGE_Shock::Seconds * 0.33f + 0.1f);
	TestFalse(TEXT("Recovered after a third of the stun"), ShinobiTest::HasTag(Boss, ShinobiTags::State_Shocked));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
