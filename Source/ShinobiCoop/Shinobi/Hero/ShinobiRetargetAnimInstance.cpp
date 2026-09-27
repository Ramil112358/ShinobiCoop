#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"
#include "Retargeter/IKRetargeter.h"

FShinobiRetargetAnimInstanceProxy::FShinobiRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode)
	: FAnimInstanceProxy(InAnimInstance)
	, RetargetNode(InRetargetNode)
{
}

void FShinobiRetargetAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	FAnimationInitializeContext InitContext(this);
	RetargetNode->Initialize_AnyThread(InitContext);
}

void FShinobiRetargetAnimInstanceProxy::CacheBones()
{
	if (bBoneCachesInvalidated)
	{
		FAnimationCacheBonesContext Context(this);
		RetargetNode->CacheBones_AnyThread(Context);
		bBoneCachesInvalidated = false;
	}
}

bool FShinobiRetargetAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	RetargetNode->Evaluate_AnyThread(Output);
	return true;
}

void FShinobiRetargetAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	RetargetNode->Update_AnyThread(InContext);
}

UShinobiRetargetAnimInstance::UShinobiRetargetAnimInstance()
{
	// The source pose is read from another component during the update: keep it on the game thread, like the engine's
	// retarget preview instance does.
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UShinobiRetargetAnimInstance::CreateAnimInstanceProxy()
{
	return new FShinobiRetargetAnimInstanceProxy(this, &RetargetNode);
}

void UShinobiRetargetAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RetargetNode.RetargetFrom = ERetargetSourceMode::ParentSkeletalMeshComponent;
	if (!RetargetNode.IKRetargeterAsset)
	{
		RetargetNode.IKRetargeterAsset = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
	}
}
