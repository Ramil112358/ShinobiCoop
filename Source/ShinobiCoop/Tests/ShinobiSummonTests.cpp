#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"

namespace
{
	/** A summoned pack, hovering (the test world has no floor). */
	TArray<AShinobiSummon*> Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground)
	{
		TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, Kind, Ground);
		for (AShinobiSummon* Each : Pack)
		{
			ShinobiTest::Hover(Each);
		}
		return Pack;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonTeamTest, "ShinobiCoop.Summons.FightForTheCasterNotAgainstThem", ShinobiTestFlags)
bool FShinobiSummonTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);

	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One toad"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("On the players' side"), Pack[0]->GetTeam(), EShinobiTeam::Players);
	TestTrue(TEXT("Hostile to enemies"), Pack[0]->IsHostileTo(Ninja) && Ninja->IsHostileTo(Pack[0]));
	TestFalse(TEXT("Not hostile to the caster"), Pack[0]->IsHostileTo(Caster) || Caster->IsHostileTo(Pack[0]));
	TestTrue(TEXT("Remembers its summoner"), Pack[0]->GetSummoner() == Caster);
	TestEqual(TEXT("Toad health"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetMaxHealthAttribute()), 600.f);

	// A jutsu that hits everything hostile around the caster leaves the summon alone.
	const TArray<AShinobiCharacterBase*> Hit = ShinobiCombat::FindHostilesInRadius(Caster, FVector::ZeroVector, 1000.f);
	TestFalse(TEXT("The caster's area jutsu skips the summon"), Hit.Contains(Pack[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFightTest, "ShinobiCoop.Summons.AttackEnemiesAndEnemiesAttackThem", ShinobiTestFlags)
bool FShinobiSummonFightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(5000.f, 0.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>();
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}

	W.Tick(3.f);
	TestTrue(TEXT("The toad hurt the ninja"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()) < 200.f);
	TestTrue(TEXT("The ninja went for the toad, not the far-off caster"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetHealthAttribute()) < 600.f);
	TestEqual(TEXT("The caster was never touched"), ShinobiTest::Get(Caster, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLifetimeTest, "ShinobiCoop.Summons.LeaveWhenTheirTimeIsUp", ShinobiTestFlags)
bool FShinobiSummonLifetimeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Snake, FVector(300.f, 0.f, 0.f));
	const float Lifetime = AShinobiSummon::GetLifetime(EShinobiSummonKind::Snake);

	W.Tick(Lifetime - 1.f);
	TestEqual(TEXT("Still here before its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
	W.Tick(1.2f);
	TestEqual(TEXT("Gone after its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLimitTest, "ShinobiCoop.Summons.OneOfEachKindPerCaster", ShinobiTestFlags)
bool FShinobiSummonLimitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Other = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Other);

	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	W.Tick(0.1f);
	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 300.f, 0.f));
	TestEqual(TEXT("The second toad replaced the first"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	Summon(Caster, EShinobiSummonKind::Snake, FVector(-300.f, 0.f, 0.f));
	TestEqual(TEXT("A snake joins the toad"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	Summon(Other, EShinobiSummonKind::Toad, FVector(0.f, 3300.f, 0.f));
	TestEqual(TEXT("Another caster's toad is not replaced"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonDogsTest, "ShinobiCoop.Summons.DogsComeAsAPackOfFour", ShinobiTestFlags)
bool FShinobiSummonDogsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("Four ninken"), Pack.Num(), AShinobiSummon::DogPackSize);
	TestEqual(TEXT("Four dogs stand"), ShinobiTest::CountActors<AShinobiSummon>(W), AShinobiSummon::DogPackSize);

	FVector Min(FLT_MAX), Max(-FLT_MAX);
	for (const AShinobiSummon* Dog : Pack)
	{
		Min = Min.ComponentMin(Dog->GetActorLocation());
		Max = Max.ComponentMax(Dog->GetActorLocation());
		TestEqual(TEXT("Dog health"), ShinobiTest::Get(const_cast<AShinobiSummon*>(Dog), UShinobiAttributeSet::GetMaxHealthAttribute()), 120.f);
	}
	TestTrue(TEXT("Spread out, not stacked"), (Max - Min).Size2D() > 100.f);

	Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("Calling the pack again replaces it"), ShinobiTest::CountActors<AShinobiSummon>(W), AShinobiSummon::DogPackSize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonDeathTest, "ShinobiCoop.Summons.AFallenSummonIsNotCounted", ShinobiTestFlags)
bool FShinobiSummonDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	ShinobiTest::Damage(Pack[0], 500.f);
	TestFalse(TEXT("Dog fell"), Pack[0]->IsAlive());

	// Calling the pack again does not wait on the body: three of the old four were alive and go; the new four arrive.
	Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	int32 Alive = 0;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Alive += It->IsAlive() ? 1 : 0;
	}
	TestEqual(TEXT("Four living dogs"), Alive, AShinobiSummon::DogPackSize);
	return true;
}

#endif
