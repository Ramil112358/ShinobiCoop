#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// The motion-matching AnimBP and physical reactions against the template AnimBP (Shinobi.MotionMatching 0): montages,
// wall runs and knockdowns must look as they did, or as intended.

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/IConsoleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "UObject/UObjectIterator.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Hero/ShinobiPhysicalReactionComponent.h"

namespace
{
	struct FDiagCVars
	{
		FDiagCVars(bool bMotionMatching, bool bPhysics)
		{
			Set(TEXT("Shinobi.MotionMatching"), bMotionMatching);
			Set(TEXT("Shinobi.PhysicalReactions"), bPhysics);
		}
		~FDiagCVars()
		{
			for (const TPair<IConsoleVariable*, bool>& Saved : Previous)
			{
				Saved.Key->Set(Saved.Value, ECVF_SetByCode);
			}
		}
		void Set(const TCHAR* Name, bool bValue)
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
			Previous.Emplace(CVar, CVar->GetBool());
			CVar->Set(bValue, ECVF_SetByCode);
		}
		TArray<TPair<IConsoleVariable*, bool>> Previous;
	};

	struct FDiagSample
	{
		FVector Hand;
		FVector Foot;
		FString Montage;
		float SlotWeight = 0.f;
	};

	TArray<FDiagSample> SampleCombo(bool bMotionMatching, bool bPhysics)
	{
		FDiagCVars Mode(bMotionMatching, bPhysics);
		FShinobiTestWorld W;
		AStaticMeshActor* Floor = W.Spawn<AStaticMeshActor>(FVector(0.f, 0.f, -10.f));
		Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorScale3D(FVector(40.f, 40.f, 0.2f));
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(P);
		W.Tick(1.f);
		USkeletalMeshComponent* Mesh = P->GetMesh();
		const int32 Hand = Mesh->GetBoneIndex(TEXT("hand_r"));
		const int32 Foot = Mesh->GetBoneIndex(TEXT("foot_l"));
		TArray<FDiagSample> Samples;
		for (int32 Step = 0; Step < 24; ++Step)
		{
			if (Step % 6 == 0 && Step < 18)
			{
				ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
			}
			W.Tick(0.05f);
			UAnimInstance* Anim = Mesh->GetAnimInstance();
			FDiagSample& Sample = Samples.AddDefaulted_GetRef();
			Sample.Hand = Mesh->GetComponentSpaceTransforms()[Hand].GetLocation();
			Sample.Foot = Mesh->GetComponentSpaceTransforms()[Foot].GetLocation();
			Sample.Montage = GetNameSafe(Anim ? Anim->GetCurrentActiveMontage() : nullptr);
			Sample.SlotWeight = Anim ? Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")) : -1.f;
		}
		return Samples;
	}
}

namespace
{
	TArray<FDiagSample> SampleWallRun(bool bMotionMatching, bool bPhysics, FString& OutState)
	{
		FDiagCVars Mode(bMotionMatching, bPhysics);
		FShinobiTestWorld W;
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		auto Block = [&](const FVector& Center, const FVector& Scale)
		{
			AStaticMeshActor* Actor = W.Spawn<AStaticMeshActor>(Center);
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Actor->SetActorScale3D(Scale);
		};
		Block(FVector(0.f, 0.f, -10.f), FVector(40.f, 40.f, 0.2f));
		Block(FVector(300.f, 0.f, 1000.f), FVector(1.f, 20.f, 20.f)); // a tall wall, its face at X = 250
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(P);
		W.Tick(0.5f);
		USkeletalMeshComponent* Mesh = P->GetMesh();
		const int32 Hand = Mesh->GetBoneIndex(TEXT("hand_r"));
		const int32 Foot = Mesh->GetBoneIndex(TEXT("foot_l"));
		const UShinobiMovementComponent* Movement = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
		TArray<FDiagSample> Samples;
		for (int32 Step = 0; Step < 40; ++Step)
		{
			P->AddMovementInput(FVector(1.f, 0.f, 0.f));
			W.Tick(0.05f);
			FDiagSample& Sample = Samples.AddDefaulted_GetRef();
			Sample.Hand = Mesh->GetComponentSpaceTransforms()[Hand].GetLocation();
			Sample.Foot = Mesh->GetComponentSpaceTransforms()[Foot].GetLocation();
			Sample.Montage = Movement->IsWallTraversing() ? TEXT("wall") : TEXT("ground");
		}
		OutState = FString::Printf(TEXT("loc %s"), *P->GetActorLocation().ToCompactString());
		return Samples;
	}
}

namespace
{
	struct FWallTurn
	{
		float MaxRoot = 0.f; // the root bone's turn off the capsule, degrees
		float MaxTilt = 0.f; // the spine's lean off the wall's normal, degrees
	};

	/** Runs at a wall, up it, then back down it, measuring every step on the wall. */
	FWallTurn SampleWallTurn()
	{
		FDiagCVars Mode(true, true);
		FShinobiTestWorld W;
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		auto Block = [&](const FVector& Center, const FVector& Scale)
		{
			AStaticMeshActor* Actor = W.Spawn<AStaticMeshActor>(Center);
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Actor->SetActorScale3D(Scale);
		};
		Block(FVector(0.f, 0.f, -10.f), FVector(40.f, 40.f, 0.2f));
		Block(FVector(300.f, 0.f, 1000.f), FVector(1.f, 20.f, 20.f));
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(P);
		W.Tick(0.5f);
		for (TObjectIterator<UPoseSearchDatabase> It; It; ++It)
		{
			using namespace UE::PoseSearch;
			FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(*It, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
		}
		USkeletalMeshComponent* Mesh = P->GetMesh();
		const UShinobiMovementComponent* Movement = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
		FWallTurn Result;
		for (int32 Step = 0; Step < 70; ++Step)
		{
			P->AddMovementInput(!Movement->IsWallTraversing() ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 0.f, Step < 40 ? 1.f : -1.f));
			W.Tick(0.05f);
			if (Movement->IsWallTraversing())
			{
				const FVector Spine = (Mesh->GetBoneLocation(TEXT("head")) - Mesh->GetBoneLocation(TEXT("pelvis"))).GetSafeNormal();
				const float Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Spine | -Movement->GetGravityDirection(), -1.f, 1.f)));
				Result.MaxTilt = FMath::Max(Result.MaxTilt, Tilt);
				Result.MaxRoot = FMath::Max(Result.MaxRoot, FMath::RadiansToDegrees(Mesh->GetComponentSpaceTransforms()[0].GetRotation().GetAngle()));
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimWallTurnTest, "ShinobiCoop.AnimFidelity.WallTurnStaysOnTheWall", ShinobiTestFlags)
bool FShinobiAnimWallTurnTest::RunTest(const FString& Parameters)
{
	// OffsetRootBone counters the capsule's turn as if it were about world Z: stepping onto a wall or turning round on it
	// swung the root 80-85 degrees off the capsule and tilted the body out of the wall. It resets every frame on a wall now.
	const FWallTurn Turn = SampleWallTurn();
	TestTrue(FString::Printf(TEXT("The root stays with the capsule on the wall (%.0f degrees at most)"), Turn.MaxRoot), Turn.MaxRoot < 5.f);
	TestTrue(FString::Printf(TEXT("The body leans no more than running on the ground (%.0f degrees at most)"), Turn.MaxTilt), Turn.MaxTilt < 45.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimWallTest, "ShinobiCoop.AnimFidelity.WallRunKeepsTheBodyOnTheWall", ShinobiTestFlags)
bool FShinobiAnimWallTest::RunTest(const FString& Parameters)
{
	// The motion-matching AnimBP's root offset kept the body upright in the world (hand 1.5 m off) and its foot placement
	// searched for ground below; both release on a wall now.
	FString OldState, GaspState;
	const TArray<FDiagSample> Old = SampleWallRun(false, false, OldState);
	const TArray<FDiagSample> Gasp = SampleWallRun(true, true, GaspState);
	TestEqual(TEXT("Reached the wall"), Gasp.Last().Montage, FString(TEXT("wall")));
	for (int32 Index = Old.Num() - 10; Index < Old.Num(); ++Index)
	{
		const float Hand = FVector::Dist(Gasp[Index].Hand, Old[Index].Hand);
		const float Foot = FVector::Dist(Gasp[Index].Foot, Old[Index].Foot);
		TestTrue(FString::Printf(TEXT("On the wall, posed like the template AnimBP (sample %d: hand %.0f cm, foot %.0f cm off)"), Index, Hand, Foot),
			Hand < 25.f && Foot < 15.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimReactionTest, "ShinobiCoop.AnimFidelity.ReactionsStayOnTheCapsule", ShinobiTestFlags)
bool FShinobiAnimReactionTest::RunTest(const FString& Parameters)
{
	FDiagCVars Mode(true, true);
	FShinobiTestWorld W;
	AStaticMeshActor* Floor = W.Spawn<AStaticMeshActor>(FVector(0.f, 0.f, -10.f));
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(100.f, 100.f, 0.2f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	W.Spawn<AAIController>()->Possess(P);
	W.Tick(1.f);
	for (TObjectIterator<UPoseSearchDatabase> It; It; ++It)
	{
		using namespace UE::PoseSearch;
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(*It, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
	}
	USkeletalMeshComponent* Mesh = P->GetMesh();
	const int32 Head = Mesh->GetBoneIndex(TEXT("head"));
	const FVector Rest = Mesh->GetComponentSpaceTransforms()[Head].GetLocation();
	P->GetPhysicalReactions()->Hit(FVector(1.f, 0.f, 0.f), 20.f);
	float Peak = 0.f;
	for (int32 Step = 0; Step < 16; ++Step)
	{
		W.Tick(0.05f);
		Peak = FMath::Max(Peak, FVector::Dist(Mesh->GetComponentSpaceTransforms()[Head].GetLocation(), Rest));
	}
	TestTrue(FString::Printf(TEXT("A hit sways the head visibly but not floppily (%.0f cm)"), Peak), Peak > 3.f && Peak < 30.f);

	// The mesh followed the ragdoll's pelvis off the capsule (4.5 m) and never came back.
	P->LaunchCharacter(FVector(1500.f, 0.f, 300.f), true, true);
	float MeshOff = 0.f, PelvisOff = 0.f;
	bool bGotUp = false;
	for (int32 Step = 0; Step < 45; ++Step) // the body lands lying before the getup (~2.3 s) starts
	{
		W.Tick(0.1f);
		MeshOff = FMath::Max(MeshOff, FVector::Dist(Mesh->GetComponentLocation(), P->GetActorLocation() + FVector(0.f, 0.f, -96.f)));
		PelvisOff = FMath::Max(PelvisOff, FVector::Dist(Mesh->GetBoneLocation(TEXT("pelvis")), P->GetActorLocation()));
		bGotUp |= Mesh->GetAnimInstance() && Mesh->GetAnimInstance()->GetCurrentActiveMontage() != nullptr;
	}
	TestTrue(FString::Printf(TEXT("The mesh stays on the capsule (%.0f cm at most)"), MeshOff), MeshOff < 2.f);
	TestTrue(FString::Printf(TEXT("The tumble stays near the capsule (%.0f cm at most)"), PelvisOff), PelvisOff < 120.f);
	TestTrue(TEXT("A getup plays"), bGotUp);
	const float End = FVector::Dist(Mesh->GetBoneLocation(TEXT("pelvis")), P->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Back on its feet over the capsule (%.0f cm)"), End), End < 15.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimMeleeTest, "ShinobiCoop.AnimFidelity.MontagesKeepTheirLegs", ShinobiTestFlags)
bool FShinobiAnimMeleeTest::RunTest(const FString& Parameters)
{
	// The motion-matching AnimBP's foot placement and leg IK replanted the legs under the melee combo (feet 20-50 cm off).
	const TArray<FDiagSample> Old = SampleCombo(false, false);
	const TArray<FDiagSample> Gasp = SampleCombo(true, true);
	for (int32 Index = 2; Index < Old.Num(); ++Index) // once the first attack has blended in
	{
		const float Hand = FVector::Dist(Gasp[Index].Hand, Old[Index].Hand);
		const float Foot = FVector::Dist(Gasp[Index].Foot, Old[Index].Foot);
		TestTrue(FString::Printf(TEXT("%s plays as authored (sample %d: hand %.0f cm, foot %.0f cm off)"), *Gasp[Index].Montage, Index, Hand, Foot),
			Hand < 3.f && Foot < 3.f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
