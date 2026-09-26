#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Revive.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDownedTest, "ShinobiCoop.Downed.ZeroHealthDownsThenBleedsOut", ShinobiTestFlags)
bool FShinobiDownedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::Damage(P, 600.f);
	TestTrue(TEXT("Downed, not dead"), ShinobiTest::HasTag(P, ShinobiTags::State_Downed) && !ShinobiTest::HasTag(P, ShinobiTags::State_Dead));
	TestFalse(TEXT("Can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("Can't attack"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Damage(P, 50.f);
	TestTrue(TEXT("No damage while downed"), ShinobiTest::HasTag(P, ShinobiTags::State_Downed));

	W.Tick(UGE_Downed::BleedOutSeconds - 1.f);
	TestTrue(TEXT("Still downed before the bleed-out"), ShinobiTest::HasTag(P, ShinobiTags::State_Downed));
	TestTrue(TEXT("Bleed-out countdown"), FMath::IsNearlyEqual(P->GetBleedOutRemaining(), 1.f, 0.1f));
	W.Tick(1.2f);
	TestTrue(TEXT("Bled out: dead"), ShinobiTest::HasTag(P, ShinobiTags::State_Dead) && !ShinobiTest::HasTag(P, ShinobiTags::State_Downed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReviveTest, "ShinobiCoop.Downed.HoldToReviveTeammate", ShinobiTestFlags)
bool FShinobiReviveTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Down = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Helper = W.Spawn<AShinobiPlayerCharacter>(FVector(150.f, 0.f, 0.f));
	ShinobiTest::Hover(Down);
	ShinobiTest::Hover(Helper);
	ShinobiTest::Damage(Down, 600.f);

	ShinobiTest::Press(Helper, EShinobiAbilityInputID::Revive);
	W.Tick(1.f);
	ShinobiTest::Release(Helper, EShinobiAbilityInputID::Revive);
	W.Tick(2.5f);
	TestTrue(TEXT("Released early: still downed"), ShinobiTest::HasTag(Down, ShinobiTags::State_Downed));

	ShinobiTest::Press(Helper, EShinobiAbilityInputID::Revive);
	TestTrue(TEXT("Reviver held still"), ShinobiTest::HasTag(Helper, ShinobiTags::State_Reviving));
	W.Tick(UGA_Revive::Seconds + 0.1f);
	TestFalse(TEXT("Revived"), ShinobiTest::HasTag(Down, ShinobiTags::State_Downed));
	TestFalse(TEXT("Not dead"), ShinobiTest::HasTag(Down, ShinobiTags::State_Dead));
	TestEqual(TEXT("30% HP"), ShinobiTest::Get(Down, UShinobiAttributeSet::GetHealthAttribute()), 150.f);
	TestTrue(TEXT("Back in the fight"), Down->IsAlive() && Down->CanMove());

	ShinobiTest::Press(Down, EShinobiAbilityInputID::Revive);
	TestFalse(TEXT("Nobody to revive: no-op"), ShinobiTest::HasTag(Down, ShinobiTags::State_Reviving));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
