#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Shinobi/Hero/ShinobiAnimGenAnimInstance.h"
#include "Shinobi/World/ShinobiVillager.h"

// TRIAL (feature/animgen): AnimGen's machine-learned walking on villagers, against the motion-matching AnimBP.
namespace
{
	struct FAnimGenVillagers
	{
		explicit FAnimGenVillagers(bool bOn)
			: CVar(IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.AnimGenVillagers")))
		{
			Previous = CVar->GetBool();
			CVar->Set(bOn, ECVF_SetByCode);
		}
		~FAnimGenVillagers() { CVar->Set(Previous, ECVF_SetByCode); }
		IConsoleVariable* CVar;
		bool Previous = false;
	};

	AShinobiVillager* SpawnVillager(FShinobiTestWorld& W, int32 Variant, const FVector& Where)
	{
		const FTransform Transform(Where);
		AShinobiVillager* Villager = W.World->SpawnActorDeferred<AShinobiVillager>(AShinobiVillager::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Villager->Variant = Variant;
		Villager->FinishSpawning(Transform);
		return Villager;
	}

	/** Seconds per world tick with Count villagers in the given mode. */
	double TickCost(bool bAnimGen, int32 Count, int32 LODLevel = 0)
	{
		FAnimGenVillagers Mode(bAnimGen);
		FShinobiTestWorld W;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			AShinobiVillager* Villager = SpawnVillager(W, Index, FVector((Index % 5) * 300.f, (Index / 5) * 300.f, 0.f));
			if (UShinobiAnimGenAnimInstance* Generated = Cast<UShinobiAnimGenAnimInstance>(Villager->GetMesh()->GetAnimInstance()))
			{
				Generated->LODLevel = LODLevel;
			}
		}
		W.Tick(0.5f); // warm up
		const double Start = FPlatformTime::Seconds();
		W.Tick(2.f);
		return (FPlatformTime::Seconds() - Start) / 40.0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimGenVillagerTest, "ShinobiCoop.AnimGen.VillagerWalksWithTheController", ShinobiTestFlags)
bool FShinobiAnimGenVillagerTest::RunTest(const FString& Parameters)
{
	FAnimGenVillagers Mode(true);
	FShinobiTestWorld W;
	AShinobiVillager* Villager = SpawnVillager(W, 2, FVector::ZeroVector);
	const UShinobiAnimGenAnimInstance* Generated = Cast<UShinobiAnimGenAnimInstance>(Villager->GetMesh()->GetAnimInstance());
	if (!TestNotNull(TEXT("The villager runs the AnimGen AnimBP"), Generated))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Style %s"), *Generated->Style.ToString()));
	W.Tick(1.f);
	// The controller's pose, not the reference pose: the arms hang instead of spreading.
	USkeletalMeshComponent* Mesh = Villager->GetMesh();
	const int32 Hand = Mesh->GetBoneIndex(TEXT("hand_r"));
	const FVector Posed = Mesh->GetComponentSpaceTransforms()[Hand].GetLocation();
	const FVector Reference = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetSkeletalMeshAsset()->GetRefSkeleton(), Hand).GetLocation();
	TestTrue(FString::Printf(TEXT("The hand moved off the reference pose (%.0f cm)"), FVector::Dist(Posed, Reference)), FVector::Dist(Posed, Reference) > 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAnimGenCostTest, "ShinobiCoop.AnimGen.CostAgainstMotionMatching", ShinobiTestFlags)
bool FShinobiAnimGenCostTest::RunTest(const FString& Parameters)
{
	constexpr int32 Count = 20;
	const double MotionMatching = TickCost(false, Count);
	const double AnimGen = TickCost(true, Count);
	const double AnimGenLow = TickCost(true, Count, 2);
	AddInfo(FString::Printf(TEXT("%d villagers, ms per frame (whole frame): motion matching %.2f, AnimGen LOD0 %.2f, AnimGen LOD2 %.2f"), Count,
		MotionMatching * 1000.0, AnimGen * 1000.0, AnimGenLow * 1000.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
