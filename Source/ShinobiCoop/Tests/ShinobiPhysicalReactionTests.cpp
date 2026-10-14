#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Shinobi/Hero/ShinobiPhysicalReactionComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

namespace
{
	/** A player standing on a floor, possessed so its movement runs. */
	AShinobiPlayerCharacter* ReactionTestPlayer(FShinobiTestWorld& W)
	{
		AStaticMeshActor* Floor = W.Spawn<AStaticMeshActor>(FVector(0.f, 0.f, -10.f));
		Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); // a Static one refuses a mesh after spawning
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorScale3D(FVector(40.f, 40.f, 0.2f));
		AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(Player);
		W.Tick(0.5f);
		return Player;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsHitTest, "ShinobiCoop.Reactions.HitShovesTheUpperBody", ShinobiTestFlags)
bool FShinobiReactionsHitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	UShinobiPhysicalReactionComponent* Reactions = P->GetPhysicalReactions();
	if (!TestTrue(TEXT("Players react physically"), Reactions && Reactions->IsPhysical()))
	{
		return false;
	}
	auto Head = [P]() { return P->GetMesh()->GetBodyInstance(TEXT("head"))->GetUnrealWorldTransform().GetLocation(); };
	const FVector Before = Head();
	Reactions->Hit(FVector(1.f, 0.f, 0.f), 30.f);
	W.Tick(0.1f);
	const float Shoved = Head().X - Before.X;
	TestTrue(FString::Printf(TEXT("The upper body gives along the hit (head %.1f cm)"), Shoved), Shoved > 3.f);
	W.Tick(1.f);
	const FVector Offset = P->GetMesh()->GetRelativeLocation();
	TestTrue(FString::Printf(TEXT("The body settles back on the capsule (mesh offset %s)"), *Offset.ToCompactString()),
		FVector::Dist(Offset, FVector(0.f, 0.f, -96.f)) < 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsKnockdownTest, "ShinobiCoop.Reactions.HardLaunchKnocksDownThenRecovers", ShinobiTestFlags)
bool FShinobiReactionsKnockdownTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	UShinobiPhysicalReactionComponent* Reactions = P->GetPhysicalReactions();

	P->LaunchCharacter(FVector(300.f, 0.f, 200.f), true, true); // a shove, not a throw
	TestFalse(TEXT("A soft launch doesn't knock down"), Reactions->IsKnockedDown());

	P->LaunchCharacter(FVector(1200.f, 0.f, 300.f), true, true);
	TestTrue(TEXT("A hard launch knocks down"), Reactions->IsKnockedDown());
	TestTrue(TEXT("Held while down"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	W.Tick(0.05f); // the physics control applies the ragdoll profile on its next tick
	TestTrue(TEXT("The driver tumbles as a ragdoll"), P->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));

	W.Tick(Reactions->KnockdownHoldSeconds + 0.15f);
	TestFalse(TEXT("Back up"), Reactions->IsKnockedDown());
	TestFalse(TEXT("Free to act again"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsDeathTest, "ShinobiCoop.Reactions.DeathRagdollsTheDriver", ShinobiTestFlags)
bool FShinobiReactionsDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	ShinobiTest::ApplyEffect(P, UGE_Dead::StaticClass());
	TestTrue(TEXT("The driver falls as a ragdoll"), P->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
