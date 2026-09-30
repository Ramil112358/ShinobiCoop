#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"

UShinobiBodyComponent::UShinobiBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule and the driver do the colliding
}

bool UShinobiBodyComponent::Wear(const TCHAR* MeshPath, const TCHAR* RetargeterPath)
{
	// Quiet: a character whose model isn't made yet is expected.
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UIKRetargeter* Found = Body ? LoadObject<UIKRetargeter>(nullptr, RetargeterPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Found)
	{
		return false;
	}
	// Retargeter first: a new mesh re-initialises the anim instance, which reads it.
	Retargeter = Found;
	SetSkeletalMeshAsset(Body);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	return true;
}

bool UShinobiBodyComponent::WearCharacter(const FString& Name)
{
	const FString Dir = FString::Printf(TEXT("/Game/Shinobi/Characters/%s/"), *Name);
	return Wear(*FString::Printf(TEXT("%sSK_%s.SK_%s"), *Dir, *Name, *Name),
		*FString::Printf(TEXT("%sRTG_MannyTo%s.RTG_MannyTo%s"), *Dir, *Name, *Name));
}

void UShinobiBodyComponent::HideDriver()
{
	if (USkeletalMeshComponent* Driver = Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		Driver->SetVisibility(false);
		Driver->SetCastShadow(false);
		Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}
