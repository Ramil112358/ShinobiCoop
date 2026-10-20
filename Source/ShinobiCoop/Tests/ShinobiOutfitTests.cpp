#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ClothingSystemRuntimeTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

// Outfits worn over the roster bodies (docs/character/outfit-steps.md): fitted copies whose hem and sleeves are Chaos
// cloth. These run against what Scripts/setup_hero.py has imported.

namespace
{
	/** Every simulated particle's position in component space (the body's frame). */
	TArray<FVector3f> ClothPositions(const USkeletalMeshComponent* Mesh)
	{
		TArray<FVector3f> Out;
		for (const TPair<int32, FClothSimulData>& Cloth : Mesh->GetCurrentClothingData_GameThread())
		{
			for (const FVector3f& Position : Cloth.Value.Positions)
			{
				Out.Add(FVector3f(Cloth.Value.ComponentRelativeTransform.TransformPosition(FVector(Position))));
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitClothSimulatesTest, "ShinobiCoop.Outfit.ClothSimulatesOnFollower", ShinobiTestFlags)
bool FShinobiOutfitClothSimulatesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	W.World->bShouldSimulatePhysics = true; // a game world sets it; cloth actors are only created when it's on
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	if (!TestNotNull(TEXT("HeroMesh in use"), P->GetHeroMesh()))
	{
		return false;
	}
	P->SetCharacterIndex(0);
	P->SetOutfitIndex(uint8(AShinobiPlayerCharacter::FindOutfitIndex(TEXT("Hokage"))));
	USkeletalMeshComponent* Outfit = P->GetOutfitMesh();
	if (Outfit)
	{
		Outfit->bWaitForParallelClothTask = true; // GetCurrentClothingData_GameThread() needs it (the game never reads the cloth)
	}
	W.Tick(0.5f);
	if (!TestTrue(TEXT("Hokage is worn"), Outfit && Outfit->GetSkeletalMeshAsset() != nullptr))
	{
		return false;
	}
	TestEqual(TEXT("The outfit has one clothing asset"), Outfit->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num(), 1);
	TestTrue(TEXT("It still follows the body through leader pose"), Outfit->LeaderPoseComponent.Get() == P->GetHeroMesh());
	const TArray<FVector3f> Before = ClothPositions(Outfit);
	if (!TestTrue(TEXT("The follower simulates cloth"), Before.Num() > 0))
	{
		return false;
	}
	// Run forward: the hem swings back relative to the body, so the cloth moves differently from its rest frame.
	for (int32 Step = 0; Step < 30; ++Step)
	{
		P->AddMovementInput(P->GetActorForwardVector(), 1.f);
		W.Tick(1.f / 30.f);
	}
	const TArray<FVector3f> After = ClothPositions(Outfit);
	float MostMoved = 0.f;
	for (int32 Index = 0; Index < FMath::Min(Before.Num(), After.Num()); ++Index)
	{
		MostMoved = FMath::Max(MostMoved, FVector3f::Dist(Before[Index], After[Index]));
	}
	TestTrue(FString::Printf(TEXT("Some cloth swings more than 1 cm against the body (most %.1f cm)"), MostMoved), MostMoved > 1.f);
	return true;
}

#endif
