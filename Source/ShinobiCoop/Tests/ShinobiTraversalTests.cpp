#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "AIController.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "UObject/UObjectIterator.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Traversal/ShinobiTraversalComponent.h"

namespace
{
	/** An engine cube (100 cm) scaled to Size cm, its base centred at Base. */
	AStaticMeshActor* TraversalBlock(FShinobiTestWorld& W, const FVector& Base, const FVector& Size)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Actor = W.Spawn<AStaticMeshActor>(Base + FVector(0.f, 0.f, Size.Z * 0.5f));
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); // a Static one refuses a mesh after spawning
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->SetActorScale3D(Size / 100.f);
		return Actor;
	}

	/** A player standing on a floor at the origin, facing +X, possessed so its movement runs. */
	AShinobiPlayerCharacter* TraversalPlayer(FShinobiTestWorld& W)
	{
		TraversalBlock(W, FVector(0.f, 0.f, -10.f), FVector(4000.f, 4000.f, 10.f));
		AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(Player); // a PlayerController without a LocalPlayer never moves here
		W.Tick(0.5f); // lands
		// The chooser pose-matches against the traversal databases, which the editor indexes in the background.
		for (TObjectIterator<UPoseSearchDatabase> It; It; ++It)
		{
			using namespace UE::PoseSearch;
			FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(*It, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
		}
		return Player;
	}

	float TraversalFeetZ(const AShinobiPlayerCharacter* Player)
	{
		return Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTraversalHurdleTest, "ShinobiCoop.Traversal.GoesOverALowWall", ShinobiTestFlags)
bool FShinobiTraversalHurdleTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = TraversalPlayer(W);
	TraversalBlock(W, FVector(90.f, 0.f, 0.f), FVector(30.f, 400.f, 90.f)); // 90 cm high, 30 cm deep, its face 75 cm ahead
	UShinobiTraversalComponent* Traversal = P->FindComponentByClass<UShinobiTraversalComponent>();

	TestTrue(TEXT("Traversal starts at a low wall"), Traversal->TryTraversal());
	TestTrue(TEXT("Traversing"), Traversal->IsTraversing());
	W.Tick(3.f);
	TestFalse(TEXT("Done"), Traversal->IsTraversing());
	TestTrue(FString::Printf(TEXT("Ends beyond the wall (x %.0f)"), P->GetActorLocation().X), P->GetActorLocation().X > 105.f);
	TestTrue(FString::Printf(TEXT("Ends on the floor (feet z %.0f)"), TraversalFeetZ(P)), FMath::Abs(TraversalFeetZ(P)) < 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTraversalMantleTest, "ShinobiCoop.Traversal.ClimbsOntoAHighBlock", ShinobiTestFlags)
bool FShinobiTraversalMantleTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = TraversalPlayer(W);
	TraversalBlock(W, FVector(275.f, 0.f, 0.f), FVector(400.f, 400.f, 150.f)); // 150 cm high and deep, its face 75 cm ahead
	UShinobiTraversalComponent* Traversal = P->FindComponentByClass<UShinobiTraversalComponent>();

	TestTrue(TEXT("Traversal starts at a high block"), Traversal->TryTraversal());
	W.Tick(3.f);
	TestTrue(FString::Printf(TEXT("Ends on top (feet z %.0f)"), TraversalFeetZ(P)), FMath::Abs(TraversalFeetZ(P) - 150.f) < 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTraversalTallWallTest, "ShinobiCoop.Traversal.JumpsAtATallWall", ShinobiTestFlags)
bool FShinobiTraversalTallWallTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = TraversalPlayer(W);
	TraversalBlock(W, FVector(275.f, 0.f, 0.f), FVector(400.f, 400.f, 600.f)); // too high to mantle: wall-run territory
	TestFalse(TEXT("No traversal at a tall wall"), P->FindComponentByClass<UShinobiTraversalComponent>()->TryTraversal());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
