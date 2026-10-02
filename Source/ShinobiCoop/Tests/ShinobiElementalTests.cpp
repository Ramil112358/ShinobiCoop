#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiTelegraph.h"
#include "Shinobi/Abilities/GA_FireMeteor.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"

namespace
{
	/** A 100-unit cube scaled into a wall for the aim to hit (the test world is empty). */
	AStaticMeshActor* SpawnMeteorWall(FShinobiTestWorld& W, const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Block = W.World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Block->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Block->SetActorScale3D(Scale);
		return Block;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillSlowsTest, "ShinobiCoop.Chill.SlowsMovementRefreshesAndExpires", ShinobiTestFlags)
bool FShinobiChillSlowsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>();
	const UCharacterMovementComponent* Movement = E->GetCharacterMovement();
	const float Normal = Movement->GetMaxSpeed();
	TestTrue(TEXT("A base speed to compare with"), Normal > 100.f);

	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass());
	TestTrue(TEXT("Chilled"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("Moves at 60%"), Movement->GetMaxSpeed(), Normal * 0.6f);

	W.Tick(2.5f);
	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass()); // another hit refreshes it instead of stacking
	W.Tick(2.5f);
	TestTrue(TEXT("Still chilled 5 s after the first hit"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	W.Tick(0.8f);
	TestFalse(TEXT("Three seconds after the last hit it wears off"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("and the speed is back"), Movement->GetMaxSpeed(), Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillAuraTest, "ShinobiCoop.Chill.TintsTheBody", ShinobiTestFlags)
bool FShinobiChillAuraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Chill::StaticClass());
	TestTrue(TEXT("The aura shows the chill"), P->GetChakraAura()->GetShownTag() == ShinobiTags::State_Chilled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTelegraphTest, "ShinobiCoop.Telegraph.ShowsThenExpires", ShinobiTestFlags)
bool FShinobiTelegraphTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	const AShinobiTelegraph* Ring = AShinobiTelegraph::Spawn(W.World, FVector::ZeroVector, 300.f, 1.f, FLinearColor::White);
	if (TestNotNull(TEXT("A telegraph"), Ring))
	{
		TestEqual(TEXT("with the radius it was given"), Ring->GetRadius(), 300.f);
	}
	W.Tick(0.5f);
	TestEqual(TEXT("still showing half way"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	W.Tick(0.8f);
	TestEqual(TEXT("gone once its time is up"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFireMeteorTest, "ShinobiCoop.Meteor.FireMeteorWarnsFallsAndBurns", ShinobiTestFlags)
bool FShinobiFireMeteorTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f)); // the aim lands on its face at X = 1450
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(1400.f, 150.f, 0.f));
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1400.f, 0.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), static_cast<AShinobiCharacterBase*>(Ally), E })
	{
		ShinobiTest::Hover(Each);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	P->ApplyJutsuPage(3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 50 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 50.f);
	W.Tick(0.9f);
	TestEqual(TEXT("A ring marks the spot after the seals"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	TestEqual(TEXT("and the meteor is falling"), ShinobiTest::CountActors<AShinobiMeteor>(W), 1);
	TestEqual(TEXT("Nothing is hurt yet: they can run"), ShinobiTest::Get(E, Health), 500.f);

	W.Tick(1.8f);
	TestEqual(TEXT("The meteor has landed"), ShinobiTest::CountActors<AShinobiMeteor>(W), 0);
	TestEqual(TEXT("and the ring is gone"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	TestEqual(TEXT("The enemy under it took 90"), ShinobiTest::Get(E, Health), 410.f);
	TestTrue(TEXT("and burns"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Fire is left on the ground"), ShinobiTest::CountActors<AShinobiFirePatch>(W), 1);
	TestEqual(TEXT("The ally beside it is unharmed"), ShinobiTest::Get(Ally, Health), 500.f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_FireMeteor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFireMeteorSkyTest, "ShinobiCoop.Meteor.OpenSkyFailsFree", ShinobiTestFlags)
bool FShinobiFireMeteorSkyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W; // nothing to aim at: no wall, no ground
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestFalse(TEXT("The cast fails"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestEqual(TEXT("for free"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
