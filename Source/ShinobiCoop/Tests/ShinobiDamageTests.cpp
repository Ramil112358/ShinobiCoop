#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStaggerThresholdTest, "ShinobiCoop.Damage.StaggerEventAtTwentyOrMore", ShinobiTestFlags)
bool FShinobiStaggerThresholdTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(200.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(Target, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Attacker->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("19 damage lands"), ShinobiCombat::ApplyDamage(Source, Target, 19.f));
	TestEqual(TEXT("19 damage dealt (exec applies it once)"), ShinobiTest::Get(Target, Health), 481.f);
	TestEqual(TEXT("19 damage doesn't stagger"), Staggers.Count, 0);

	TestTrue(TEXT("20 damage lands"), ShinobiCombat::ApplyDamage(Source, Target, 20.f));
	TestEqual(TEXT("20 damage dealt"), ShinobiTest::Get(Target, Health), 461.f);
	TestEqual(TEXT("20 damage staggers"), Staggers.Count, 1);
	TestTrue(TEXT("Stagger names the attacker"), Staggers.LastInstigator == Attacker);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInvulnerableHitTest, "ShinobiCoop.Damage.InvulnerableNegatesAndRequestsSubstitution", ShinobiTestFlags)
bool FShinobiInvulnerableHitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Substitutions(P, ShinobiTags::Event_Substitution);
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* EnemySource = E->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_DodgeIFrames::StaticClass());
	TestFalse(TEXT("A hit on an invulnerable target doesn't land"), ShinobiCombat::ApplyDamage(EnemySource, P, 50.f));
	TestEqual(TEXT("Invulnerable: no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("Substitution requested"), Substitutions.Count, 1);
	TestTrue(TEXT("Substitution names the attacker"), Substitutions.LastInstigator == E);
	TestEqual(TEXT("A negated hit doesn't stagger"), Staggers.Count, 0);

	// Self-inflicted (nobody to teleport behind): negated, no Substitution.
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Self-inflicted, invulnerable: no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("No Substitution without an attacker"), Substitutions.Count, 1);

	// No charges left: the dodge still avoids the hit, but there's no Substitution.
	ShinobiTest::Set(P, UShinobiAttributeSet::GetSubstitutionChargesAttribute(), 0.f);
	ShinobiCombat::ApplyDamage(EnemySource, P, 50.f);
	TestEqual(TEXT("Out of charges: still no damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("Out of charges: no Substitution"), Substitutions.Count, 1);

	W.Tick(0.3f); // the 0.2 s i-frames are over
	TestTrue(TEXT("Lands once the i-frames end"), ShinobiCombat::ApplyDamage(EnemySource, P, 50.f));
	TestEqual(TEXT("Damage after the i-frames"), ShinobiTest::Get(P, Health), 450.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
