#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/TargetPoint.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/World/ShinobiCamp.h"
#include "Shinobi/World/ShinobiEncounter.h"
#include "Shinobi/World/ShinobiWorldGameMode.h"
#include "Shinobi/World/ShinobiVillager.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/Abilities/GA_Revive.h"

namespace
{
	AShinobiCamp* SpawnCamp(FShinobiTestWorld& W, const FVector& Where, TArray<FShinobiCampWave> Waves)
	{
		const FTransform Transform(Where);
		AShinobiCamp* Camp = W.World->SpawnActorDeferred<AShinobiCamp>(AShinobiCamp::StaticClass(), Transform);
		Camp->CampId = TEXT("test_camp");
		Camp->Radius = 2000.f;
		Camp->Waves = MoveTemp(Waves);
		Camp->FinishSpawning(Transform);
		return Camp;
	}

	AShinobiPlayerCharacter* SpawnPlayer(FShinobiTestWorld& W, const FVector& Where)
	{
		AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(Where);
		ShinobiTest::Hover(Player);
		return Player;
	}

	void KillEnemies(FShinobiTestWorld& W)
	{
		for (TActorIterator<AShinobiEnemyCharacter> It(W.World); It; ++It)
		{
			if (It->IsAlive())
			{
				ShinobiTest::Damage(*It, 100000.f);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCampActivatesTest, "ShinobiCoop.World.CampActivatesForAnyPlayer", ShinobiTestFlags)
bool FShinobiCampActivatesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCamp* Camp = SpawnCamp(W, FVector(70000.f, 0.f, 0.f), { { 2, 1 } });
	SpawnPlayer(W, FVector::ZeroVector);                     // the host, 700 m away
	W.Tick(0.5f);
	TestEqual(TEXT("Nobody near: idle"), Camp->GetState(), EShinobiCampState::Idle);
	SpawnPlayer(W, FVector(70500.f, 0.f, 0.f));              // a client walks in
	W.Tick(0.5f);
	TestEqual(TEXT("Active for the client far from the host"), Camp->GetState(), EShinobiCampState::Active);
	TestEqual(TEXT("First wave spawned"), Camp->GetRemaining(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCampClearsTest, "ShinobiCoop.World.CampClearsWhenAllWavesDie", ShinobiTestFlags)
bool FShinobiCampClearsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCamp* Camp = SpawnCamp(W, FVector::ZeroVector, { { 1, 0 }, { 0, 2 } });
	int32 Cleared = 0;
	Camp->OnCleared.AddLambda([&Cleared]() { ++Cleared; });
	SpawnPlayer(W, FVector(300.f, 0.f, 0.f));
	W.Tick(0.5f);
	TestEqual(TEXT("Wave 1: one ninja"), Camp->GetRemaining(), 1);
	KillEnemies(W);
	W.Tick(0.5f);
	TestEqual(TEXT("Wave 2 follows: two throwers"), Camp->GetRemaining(), 2);
	TestEqual(TEXT("Still active"), Camp->GetState(), EShinobiCampState::Active);
	KillEnemies(W);
	W.Tick(0.5f);
	TestEqual(TEXT("Cleared"), Camp->GetState(), EShinobiCampState::Cleared);
	W.Tick(1.f);
	TestEqual(TEXT("OnCleared fired once"), Cleared, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCampResetsTest, "ShinobiCoop.World.CampResetsWhenEmpty", ShinobiTestFlags)
bool FShinobiCampResetsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCamp* Camp = SpawnCamp(W, FVector::ZeroVector, { { 2, 0 } });
	AShinobiPlayerCharacter* Player = SpawnPlayer(W, FVector(300.f, 0.f, 0.f));
	W.Tick(0.5f);
	TestEqual(TEXT("Active"), Camp->GetState(), EShinobiCampState::Active);

	ShinobiTest::Damage(Player, 10000.f); // downed inside the camp: doesn't count as present
	W.Tick(AShinobiCamp::ResetSeconds - 2.f);
	TestEqual(TEXT("Not yet reset"), Camp->GetState(), EShinobiCampState::Active);
	W.Tick(2.5f);
	TestEqual(TEXT("Reset with only a downed player inside"), Camp->GetState(), EShinobiCampState::Idle);
	TestEqual(TEXT("Enemies despawned"), Camp->GetRemaining(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCampHealthTest, "ShinobiCoop.World.CampHealthScales", ShinobiTestFlags)
bool FShinobiCampHealthTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnCamp(W, FVector::ZeroVector, { { 1, 0 } });
	SpawnPlayer(W, FVector(300.f, 0.f, 0.f));
	SpawnPlayer(W, FVector(-300.f, 0.f, 0.f));
	W.Tick(0.5f);
	TActorIterator<AShinobiRogueNinja> Ninja(W.World);
	if (TestTrue(TEXT("A ninja spawned"), static_cast<bool>(Ninja)))
	{
		TestEqual(TEXT("Two players: 200 x 1.5 HP"), ShinobiTest::Get(*Ninja, UShinobiAttributeSet::GetMaxHealthAttribute()), 300.f);
	}
	TestEqual(TEXT("Encounter scaling matches the arena's"), FShinobiEncounter::GetHealthScale(4), 2.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCampMarkerTest, "ShinobiCoop.World.CampMarkerTags", ShinobiTestFlags)
bool FShinobiCampMarkerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	ATargetPoint* Marker = W.Spawn<ATargetPoint>();
	Marker->Tags = { TEXT("ShinobiDressing"), TEXT("ShinobiCampSite"), TEXT("Camp:camp_4"), TEXT("Difficulty:4"), TEXT("Radius:4000"), TEXT("Waves:4,2;3,3") };
	AShinobiCamp* Camp = W.World->SpawnActorDeferred<AShinobiCamp>(AShinobiCamp::StaticClass(), FTransform::Identity);
	TestTrue(TEXT("Read"), AShinobiWorldGameMode::ReadMarker(*Marker, *Camp));
	TestEqual(TEXT("Id"), Camp->CampId, FName(TEXT("camp_4")));
	TestEqual(TEXT("Difficulty"), Camp->Difficulty, 4);
	TestEqual(TEXT("Radius"), Camp->Radius, 4000.f);
	TestTrue(TEXT("Two waves"), Camp->Waves.Num() == 2 && Camp->Waves[0].Ninjas == 4 && Camp->Waves[1].Throwers == 3);
	ATargetPoint* Boss = W.Spawn<ATargetPoint>();
	Boss->Tags = { TEXT("ShinobiCampSite"), TEXT("Boss") };
	AShinobiCamp* BossCamp = W.World->SpawnActorDeferred<AShinobiCamp>(AShinobiCamp::StaticClass(), FTransform::Identity);
	TestTrue(TEXT("Boss marker"), AShinobiWorldGameMode::ReadMarker(*Boss, *BossCamp) && BossCamp->bBoss);
	ATargetPoint* Other = W.Spawn<ATargetPoint>();
	AShinobiCamp* NotACamp = W.World->SpawnActorDeferred<AShinobiCamp>(AShinobiCamp::StaticClass(), FTransform::Identity);
	TestFalse(TEXT("Untagged point ignored"), AShinobiWorldGameMode::ReadMarker(*Other, *NotACamp));
	Camp->FinishSpawning(FTransform::Identity);
	BossCamp->FinishSpawning(FTransform::Identity);
	NotACamp->FinishSpawning(FTransform::Identity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInteractTest, "ShinobiCoop.World.InteractPrefersRevive", ShinobiTestFlags)
bool FShinobiInteractTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Helper = SpawnPlayer(W, FVector::ZeroVector);
	AShinobiPlayerCharacter* Downed = SpawnPlayer(W, FVector(150.f, 0.f, 0.f));
	AShinobiVillager* Villager = W.Spawn<AShinobiVillager>(FVector(0.f, 150.f, 0.f));
	TestTrue(TEXT("Villager in talk range"), UGA_Revive::FindVillagerNear(*Helper) == Villager);
	ShinobiTest::Damage(Downed, 10000.f);
	ShinobiTest::Press(Helper, EShinobiAbilityInputID::Revive);
	TestTrue(TEXT("A downed ally beats a villager: reviving"), ShinobiTest::HasTag(Helper, ShinobiTags::State_Reviving));
	ShinobiTest::Release(Helper, EShinobiAbilityInputID::Revive);
	Villager->SetActorLocation(FVector(0.f, 1000.f, 0.f));
	TestNull(TEXT("Villager out of range"), UGA_Revive::FindVillagerNear(*Helper));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarkTest, "ShinobiCoop.World.VillagerBarkDeterministic", ShinobiTestFlags)
bool FShinobiBarkTest::RunTest(const FString& Parameters)
{
	TSet<FString> Seen;
	for (int32 Seed = 0; Seed < AShinobiVillager::NumBarks() * 2; ++Seed)
	{
		TestTrue(TEXT("Same seed, same line"), AShinobiVillager::PickBark(Seed).EqualTo(AShinobiVillager::PickBark(Seed)));
		Seen.Add(AShinobiVillager::PickBark(Seed).ToString());
	}
	TestEqual(TEXT("Every line reachable"), Seen.Num(), AShinobiVillager::NumBarks());
	FShinobiTestWorld W;
	AShinobiVillager* Villager = W.Spawn<AShinobiVillager>();
	TestFalse(TEXT("Talking returns a line"), Villager->Talk().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiVillagerTargetTest, "ShinobiCoop.World.VillagerNotTargetable", ShinobiTestFlags)
bool FShinobiVillagerTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>();
	AShinobiPlayerCharacter* Player = SpawnPlayer(W, FVector(200.f, 0.f, 0.f));
	W.Spawn<AShinobiVillager>(FVector(0.f, 200.f, 0.f));
	const TArray<AShinobiCharacterBase*> Targets = ShinobiCombat::FindHostilesInRadius(Ninja, Ninja->GetActorLocation(), 1000.f);
	TestTrue(TEXT("Only the player is a target"), Targets.Num() == 1 && Targets[0] == Player);
	ATargetPoint* Marker = W.Spawn<ATargetPoint>();
	Marker->Tags = { TEXT("ShinobiVillager"), TEXT("Route:100,200,300|400,500,600") };
	TArray<FVector> Route;
	TestTrue(TEXT("Villager marker read"), AShinobiWorldGameMode::ReadVillagerMarker(*Marker, Route) && Route.Num() == 2 && Route[1] == FVector(400, 500, 600));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
