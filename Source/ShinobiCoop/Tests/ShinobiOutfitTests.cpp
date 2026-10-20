#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ClothingAsset.h"
#include "ClothingSystemRuntimeTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	/** A player wearing Outfit as roster body Body, cloth readable (see ClothSimulatesOnFollower), settled 0.5 s. */
	AShinobiPlayerCharacter* Wearer(FShinobiTestWorld& W, const TCHAR* Outfit, const TCHAR* Body)
	{
		W.World->bShouldSimulatePhysics = true; // a game world sets it; cloth actors are only created when it's on
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
		if (!P->GetHeroMesh())
		{
			return nullptr;
		}
		P->SetCharacterIndex(uint8(AShinobiPlayerCharacter::FindRosterIndex(Body)));
		P->SetOutfitIndex(uint8(AShinobiPlayerCharacter::FindOutfitIndex(Outfit)));
		P->GetOutfitMesh()->bWaitForParallelClothTask = true; // GetCurrentClothingData_GameThread() needs it
		W.Tick(0.5f);
		return P;
	}

	/** The texture every body material's HideMask parameter shows, or null if the slots disagree or aren't dynamic. */
	UTexture* BodyHideMask(const AShinobiPlayerCharacter* P)
	{
		UTexture* Shared = nullptr;
		const USkeletalMeshComponent* Body = P->GetHeroMesh();
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			const UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(Slot));
			UTexture* Texture = nullptr;
			if (!Material || !Material->GetTextureParameterValue(FName(TEXT("HideMask")), Texture) || (Slot > 0 && Texture != Shared))
			{
				return nullptr;
			}
			Shared = Texture;
		}
		return Shared;
	}

	FVector3f ClothMean(const USkeletalMeshComponent* Mesh)
	{
		const TArray<FVector3f> Points = ClothPositions(Mesh);
		FVector3f Sum = FVector3f::ZeroVector;
		for (const FVector3f& Point : Points)
		{
			Sum += Point;
		}
		return Points.Num() ? Sum / float(Points.Num()) : Sum;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitSwapBodyTest, "ShinobiCoop.Outfit.SwapBodyKeepsFit", ShinobiTestFlags)
bool FShinobiOutfitSwapBodyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Wearer(W, TEXT("Akatsuki"), TEXT("Hero"));
	if (!TestNotNull(TEXT("HeroMesh in use"), P))
	{
		return false;
	}
	P->SetCharacterIndex(uint8(AShinobiPlayerCharacter::FindRosterIndex(TEXT("Bika"))));
	W.Tick(0.2f);
	const USkeletalMesh* Worn = P->GetOutfitMesh()->GetSkeletalMeshAsset();
	TestEqual(TEXT("Bika wears the copy fitted to her"), Worn ? Worn->GetName() : FString(), FString(TEXT("SK_Akatsuki_Bika")));
	const UTexture* Mask = BodyHideMask(P);
	TestEqual(TEXT("Her body hides what that copy covers"), Mask ? Mask->GetName() : FString(), FString(TEXT("T_Akatsuki_Bika_Hide")));
	TestTrue(TEXT("The copy follows her pose"), P->GetOutfitMesh()->LeaderPoseComponent.Get() == P->GetHeroMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitTeleportTest, "ShinobiCoop.Outfit.TeleportResetsCloth", ShinobiTestFlags)
bool FShinobiOutfitTeleportTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Wearer(W, TEXT("Hokage"), TEXT("Hero"));
	if (!TestNotNull(TEXT("HeroMesh in use"), P))
	{
		return false;
	}
	const FVector3f Before = ClothMean(P->GetOutfitMesh());
	P->TeleportTo(P->GetActorLocation() + FVector(5000.f, 0.f, 0.f), P->GetActorRotation(), false, true);
	W.Tick(1.f / 30.f);
	const FVector3f After = ClothMean(P->GetOutfitMesh());
	TestTrue(FString::Printf(TEXT("The cloth comes along at once (%.0f cm off the body's frame)"), FVector3f::Dist(Before, After)),
		FVector3f::Dist(Before, After) < 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitGravityTest, "ShinobiCoop.Outfit.ClothFollowsCharacterGravity", ShinobiTestFlags)
bool FShinobiOutfitGravityTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Wearer(W, TEXT("Hokage"), TEXT("Hero"));
	if (!TestNotNull(TEXT("HeroMesh in use"), P))
	{
		return false;
	}
	P->GetCharacterMovement()->SetMovementMode(MOVE_None); // held still: in a fall cloth and body accelerate alike
	W.Tick(0.1f); // (not long: cloth that has settled sleeps, and a gravity change alone doesn't wake it)
	const FVector Before = P->GetOutfitMesh()->GetComponentTransform().TransformVector(FVector(ClothMean(P->GetOutfitMesh())));
	P->GetCharacterMovement()->SetGravityDirection(FVector(1.f, 0.f, 0.f)); // a wall-run: "down" is +X
	W.Tick(1.f);
	const FVector After = P->GetOutfitMesh()->GetComponentTransform().TransformVector(FVector(ClothMean(P->GetOutfitMesh())));
	TestTrue(FString::Printf(TEXT("The hem swings towards the character's down (%.1f cm along +X)"), After.X - Before.X), After.X - Before.X > 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitMissingCopyTest, "ShinobiCoop.Outfit.MissingCopyWearsNothing", ShinobiTestFlags)
bool FShinobiOutfitMissingCopyTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Suffix = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.OutfitCopySuffix"));
	if (!TestNotNull(TEXT("Shinobi.OutfitCopySuffix exists"), Suffix))
	{
		return false;
	}
	const FString Old = Suffix->GetString();
	Suffix->Set(TEXT("_NotImported"), ECVF_SetByConsole); // no copy of that name exists
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Wearer(W, TEXT("Hokage"), TEXT("Cat"));
	Suffix->Set(*Old, ECVF_SetByConsole);
	if (!TestNotNull(TEXT("HeroMesh in use"), P))
	{
		return false;
	}
	TestFalse(TEXT("No copy for this body: nothing is worn (not another body's copy)"), P->GetOutfitMesh()->IsVisible());
	const UTexture* Mask = BodyHideMask(P);
	TestEqual(TEXT("And nothing of the body is hidden"), Mask ? Mask->GetName() : FString(), FString(TEXT("WhiteSquareTexture")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitNoneTest, "ShinobiCoop.Outfit.NoneRestoresBody", ShinobiTestFlags)
bool FShinobiOutfitNoneTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Wearer(W, TEXT("Akatsuki"), TEXT("Hero"));
	if (!TestNotNull(TEXT("HeroMesh in use"), P))
	{
		return false;
	}
	P->SetOutfitIndex(0);
	const UTexture* Mask = BodyHideMask(P);
	TestEqual(TEXT("Taking the outfit off shows the whole body again"), Mask ? Mask->GetName() : FString(), FString(TEXT("WhiteSquareTexture")));
	return true;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitEveryCopyTest, "ShinobiCoop.Outfit.EveryCopyHasCloth", ShinobiTestFlags)
bool FShinobiOutfitEveryCopyTest::RunTest(const FString& Parameters)
{
	// Every outfit fitted to every roster body (Scripts/character/build_outfits.ps1, Scripts/setup_hero.py).
	for (int32 Outfit = 1; Outfit < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Outfit)
	{
		for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
		{
			const FString Who = FString::Printf(TEXT("%s on %s"), AShinobiPlayerCharacter::Outfits[Outfit].Name, AShinobiPlayerCharacter::Roster[Body].Name);
			const USkeletalMesh* Copy = LoadObject<USkeletalMesh>(nullptr, *AShinobiPlayerCharacter::OutfitMeshPath(Outfit, Body), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!TestNotNull(Who + TEXT(": its copy is imported"), Copy))
			{
				continue;
			}
			TestNotNull(Who + TEXT(": its hide mask is imported"), LoadObject<UTexture>(nullptr, *AShinobiPlayerCharacter::OutfitHidePath(Outfit, Body), nullptr, LOAD_NoWarn | LOAD_Quiet));
			if (!TestEqual(Who + TEXT(": one clothing asset"), Copy->GetMeshClothingAssets().Num(), 1))
			{
				continue;
			}
#if WITH_EDITORONLY_DATA
			const UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Copy->GetMeshClothingAssets()[0]);
			const FPointWeightMap* MaxDistance = Cloth && Cloth->LodData.Num() ? Cloth->LodData[0].PointWeightMaps.FindByPredicate(
				[](const FPointWeightMap& Map) { return Map.CurrentTarget == uint8(EWeightMapTargetCommon::MaxDistance); }) : nullptr;
			if (TestNotNull(Who + TEXT(": a Max Distance map"), MaxDistance))
			{
				TestTrue(Who + TEXT(": the hem swings (Max Distance over 20 cm)"), MaxDistance->Values.ContainsByPredicate([](float V) { return V > 20.f; }));
				TestTrue(Who + TEXT(": the shoulders stay pinned (Max Distance 0)"), MaxDistance->Values.ContainsByPredicate([](float V) { return V == 0.f; }));
			}
#endif
		}
	}
	return true;
}

#endif
