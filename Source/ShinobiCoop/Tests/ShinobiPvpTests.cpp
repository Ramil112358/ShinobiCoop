#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTeamsHostileTest, "ShinobiCoop.Pvp.TeamsAreHostileAcrossAAndB", ShinobiTestFlags)
bool FShinobiPvpTeamsHostileTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, -500.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(5000.f, 0.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);

	TestTrue(TEXT("A is hostile to B"), A1->IsHostileTo(B1));
	TestTrue(TEXT("B is hostile to A"), B1->IsHostileTo(A1));
	TestFalse(TEXT("Teammates are not hostile"), A1->IsHostileTo(A2) || A2->IsHostileTo(A1));
	TestTrue(TEXT("Team A is hostile to enemies"), A1->IsHostileTo(Ninja) && Ninja->IsHostileTo(A1));
	TestTrue(TEXT("Only the arena teams are pvp teams"), IsPvpTeam(EShinobiTeam::TeamA) && IsPvpTeam(EShinobiTeam::TeamB)
		&& !IsPvpTeam(EShinobiTeam::Players) && !IsPvpTeam(EShinobiTeam::Enemies));
	TestEqual(TEXT("A player starts on the players' side"), W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 1000.f, 0.f))->GetTeam(), EShinobiTeam::Players);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpAreaTest, "ShinobiCoop.Pvp.AreaJutsuHitsOpposingTeamOnly", ShinobiTestFlags)
bool FShinobiPvpAreaTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(200.f, 0.f, 0.f));
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(-200.f, 0.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(A1);
	ShinobiTest::Hover(A2);
	ShinobiTest::Hover(B1);

	const TArray<AShinobiCharacterBase*> Hit = ShinobiCombat::FindHostilesInRadius(A1, FVector::ZeroVector, 1000.f);
	TestTrue(TEXT("The opposing player is hit"), Hit.Contains(B1));
	TestFalse(TEXT("The ally is spared"), Hit.Contains(A2));
	TestFalse(TEXT("The caster is spared"), Hit.Contains(A1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpSummonTeamTest, "ShinobiCoop.Pvp.SummonInheritsCasterTeam", ShinobiTestFlags)
bool FShinobiPvpSummonTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Foe = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	Caster->SetTeam(EShinobiTeam::TeamB);
	Foe->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Foe);

	const TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One toad"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	ShinobiTest::Hover(Pack[0]);
	TestEqual(TEXT("The toad fights for team B"), Pack[0]->GetTeam(), EShinobiTeam::TeamB);
	TestTrue(TEXT("A team A player is hostile to it"), Foe->IsHostileTo(Pack[0]) && Pack[0]->IsHostileTo(Foe));
	TestFalse(TEXT("Its own caster is not"), Pack[0]->IsHostileTo(Caster));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpOutlineTest, "ShinobiCoop.Pvp.OutlineFollowsTheViewersSide", ShinobiTestFlags)
bool FShinobiPvpOutlineTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);

	// No local player controller in the test world: the old static rule (only Enemies is red) holds.
	TestEqual(TEXT("Without a viewer a team B player is friendly"), B1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));
	TestEqual(TEXT("Without a viewer a team A player is friendly"), A1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));

	// With a viewer, the rule is hostility.
	A1->ApplyOutlineFor(A1);
	B1->ApplyOutlineFor(A1);
	TestEqual(TEXT("The viewer's own side is friendly"), A1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));
	TestEqual(TEXT("The opposing side is outlined as an enemy"), B1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilEnemy));
	return true;
}

#endif
