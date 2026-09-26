#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Melee.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBurnTest, "ShinobiCoop.Status.BurnTicksAndRefreshes", ShinobiTestFlags)
bool FShinobiBurnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("Burn applied"), ShinobiCombat::ApplyStatus(Source, E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick));
	TestTrue(TEXT("Burning.Normal"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestTrue(TEXT("Parent State.Burning matches"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning));
	W.Tick(0.9f);
	TestEqual(TEXT("No tick on application"), ShinobiTest::Get(E, Health), 500.f);
	W.Tick(0.2f);
	TestEqual(TEXT("First tick at 1 s: 10"), ShinobiTest::Get(E, Health), 490.f);

	// Re-applying at ~2.1 s refreshes the 4 s duration; there's still only one burn (10 per tick, not 20).
	W.Tick(1.0f);
	TestEqual(TEXT("Second tick: 480"), ShinobiTest::Get(E, Health), 480.f);
	ShinobiCombat::ApplyStatus(Source, E, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick);
	W.Tick(1.0f);
	TestEqual(TEXT("One stack: third tick is 10"), ShinobiTest::Get(E, Health), 470.f);
	W.Tick(2.0f); // t = 5.1: an unrefreshed burn would have ended at 4.0
	TestTrue(TEXT("Refreshed burn still going at 5.1 s"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	W.Tick(1.5f); // t = 6.6: the refreshed burn ended at ~6.1
	TestFalse(TEXT("Burn over"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning));
	// Timer boundaries can make the tick exactly at expiry land either side, so allow one tick of slack.
	const float After = ShinobiTest::Get(E, Health);
	TestTrue(FString::Printf(TEXT("Ticks until expiry (got %.0f)"), After), After >= 440.f && After <= 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPeriodicRulesTest, "ShinobiCoop.Status.PeriodicDamageIgnoresIFramesAndBonus", ShinobiTestFlags)
bool FShinobiPeriodicRulesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Target = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Substitutions(Target, ShinobiTags::Event_Substitution);
	ShinobiTest::FEventCounter Staggers(Target, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("Black flame applied"), ShinobiCombat::ApplyStatus(Source, Target, UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick));
	TestTrue(TEXT("Burning.Black"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Black));
	TestTrue(TEXT("Shock bonus applied"), ShinobiCombat::ApplyStatus(Source, Target, UGE_ShockBonus::StaticClass()));

	Target->GetAbilitySystemComponent()->AddLooseGameplayTag(ShinobiTags::State_Invulnerable);
	TestFalse(TEXT("No new statuses on an invulnerable target"), ShinobiCombat::ApplyStatus(Source, Target, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick));
	TestFalse(TEXT("Burn not applied"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Normal));

	W.Tick(1.1f);
	TestEqual(TEXT("Black flame ticks through invulnerability"), ShinobiTest::Get(Target, Health), 485.f);
	TestTrue(TEXT("Periodic ticks don't use up the Shock bonus"), ShinobiTest::HasTag(Target, ShinobiTags::State_ShockBonus));
	TestEqual(TEXT("Periodic ticks never request Substitution"), Substitutions.Count, 0);
	TestEqual(TEXT("Periodic ticks never stagger"), Staggers.Count, 0);
	Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShinobiTags::State_Invulnerable);

	W.Tick(9.5f); // t = 10.6
	TestFalse(TEXT("Black flame over after 10 s"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Black));
	const float After = ShinobiTest::Get(Target, Health);
	TestTrue(FString::Printf(TEXT("15 per second for 10 s (got %.0f)"), After), After >= 350.f && After <= 365.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiShockBonusTest, "ShinobiCoop.Status.ShockBonusAddsQuarterOnce", ShinobiTestFlags)
bool FShinobiShockBonusTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	UAbilitySystemComponent* Source = Caster->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiCombat::ApplyStatus(Source, E, UGE_ShockBonus::StaticClass());
	ShinobiCombat::ApplyDamage(Source, E, 100.f);
	TestEqual(TEXT("Shocked target takes +25%"), ShinobiTest::Get(E, Health), 375.f);
	TestFalse(TEXT("Bonus consumed"), ShinobiTest::HasTag(E, ShinobiTags::State_ShockBonus));
	ShinobiCombat::ApplyDamage(Source, E, 100.f);
	TestEqual(TEXT("Next hit is normal"), ShinobiTest::Get(E, Health), 275.f);

	ShinobiCombat::ApplyStatus(Source, E, UGE_ShockBonus::StaticClass());
	W.Tick(10.1f);
	TestFalse(TEXT("Unused bonus expires after 10 s"), ShinobiTest::HasTag(E, ShinobiTags::State_ShockBonus));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStunTest, "ShinobiCoop.Status.StunAndStaggerBlockActions", ShinobiTestFlags)
bool FShinobiStunTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	ShinobiTest::ApplyEffect(P, UGE_Shock::StaticClass());
	TestTrue(TEXT("Shocked"), ShinobiTest::HasTag(P, ShinobiTags::State_Shocked));
	TestFalse(TEXT("Shocked: can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("Shocked: can't attack"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Shocked: can't dodge"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	W.Tick(1.6f);
	TestFalse(TEXT("Shock over after 1.5 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Shocked));
	TestTrue(TEXT("Can move again"), P->CanMove());

	ShinobiTest::ApplyEffect(P, UGE_Stagger::StaticClass());
	TestFalse(TEXT("Staggered: can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Staggered: can't dodge"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	W.Tick(0.6f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Stagger over after 0.5 s"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
