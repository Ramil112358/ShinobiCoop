#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "Animation/AnimMontage.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"

bool ShinobiIsJutsuCast(const UAnimMontage* Montage)
{
	const FString Name = Montage ? Montage->GetName() : FString();
	return Name.EndsWith(TEXT("_Seal")) || Name.EndsWith(TEXT("_Charge")) || Name.EndsWith(TEXT("_Release"));
}

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

void FShinobiRetargetAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	ContactWeight = 0.f;
	const USkeletalMeshComponent* Mesh = InAnimInstance->GetSkelMeshComponent();
	const USkeletalMeshComponent* Source = Mesh ? Cast<USkeletalMeshComponent>(Mesh->GetAttachParent()) : nullptr;
	const UAnimInstance* SourceAnim = Source ? Source->GetAnimInstance() : nullptr;
	if (!SourceAnim)
	{
		return;
	}
	float MontageWeight = 0.f;
	for (const FAnimMontageInstance* Montage : SourceAnim->MontageInstances)
	{
		// Only the jutsu casts; the guard, roll and seat keep the plain retarget.
		MontageWeight += Montage && ShinobiIsJutsuCast(Montage->Montage) ? Montage->GetWeight() : 0.f;
	}
	// The source is Manny (the hidden mesh running the gameplay animations).
	const FVector HandL = Source->GetSocketLocation(TEXT("hand_l"));
	const FVector HandR = Source->GetSocketLocation(TEXT("hand_r"));
	SourceHandOffset = HandL - HandR;
	SourceForeArmLength = FVector::Dist(HandR, Source->GetSocketLocation(TEXT("lowerarm_r")));
	ComponentToWorld = Mesh->GetComponentTransform();
	const float Close = 1.f - FMath::SmoothStep(ContactNear, ContactFar, static_cast<float>(SourceHandOffset.Size()));
	ContactWeight = FMath::Min(MontageWeight, 1.f) * Close;
}

void FShinobiRetargetAnimInstanceProxy::CacheBones()
{
	if (bBoneCachesInvalidated)
	{
		FAnimationCacheBonesContext Context(this);
		RetargetNode->CacheBones_AnyThread(Context);
		bBoneCachesInvalidated = false;

		const FBoneContainer& Bones = GetRequiredBones();
		const UIKRetargeter* Retargeter = RetargetNode->IKRetargeterAsset;
		const UIKRigDefinition* Rig = Retargeter ? Retargeter->GetIKRig(ERetargetSourceOrTarget::Target) : nullptr;
		const TCHAR* Chains[2] = {TEXT("LeftArm"), TEXT("RightArm")};
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FBoneChain* Chain = Rig ? Rig->GetRetargetChainByName(Chains[Side]) : nullptr;
			const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
			int32 Index = Chain ? RefSkeleton.FindBoneIndex(Chain->EndBone.BoneName) : INDEX_NONE;
			for (int32 Joint = 2; Joint >= 0; --Joint) // hand, forearm, upper arm
			{
				ArmBones[Side][Joint] = FBoneReference(Index != INDEX_NONE ? RefSkeleton.GetBoneName(Index) : NAME_None);
				ArmBones[Side][Joint].Initialize(Bones);
				Index = Index != INDEX_NONE ? RefSkeleton.GetParentIndex(Index) : INDEX_NONE;
			}
		}
	}
}

bool FShinobiRetargetAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	RetargetNode->Evaluate_AnyThread(Output);
	if (ContactWeight > UE_KINDA_SMALL_NUMBER)
	{
		KeepHandContact(Output);
	}
	return true;
}

void FShinobiRetargetAnimInstanceProxy::KeepHandContact(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	FCompactPoseBoneIndex Index[2][3] = {{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)},
		{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)}};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			if (!ArmBones[Side][Joint].IsValidToEvaluate(Bones))
			{
				return;
			}
			Index[Side][Joint] = ArmBones[Side][Joint].GetCompactPoseIndex(Bones);
		}
	}
	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	FTransform Arm[2][3];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			Arm[Side][Joint] = Pose.GetComponentSpaceTransform(Index[Side][Joint]);
		}
	}
	// The source's offset in this mesh's component space, scaled by forearm length to this body.
	const FVector Offset = ComponentToWorld.InverseTransformVector(SourceHandOffset);
	const double SourceForeArm = ComponentToWorld.InverseTransformVector(FVector(SourceForeArmLength, 0, 0)).Size();
	const double ForeArm = FVector::Dist(Arm[1][1].GetLocation(), Arm[1][2].GetLocation());
	if (SourceForeArm < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Gap = Arm[1][2].GetLocation() + Offset * (ForeArm / SourceForeArm) - Arm[0][2].GetLocation();
	const FVector Target[2] = {Arm[0][2].GetLocation() + Gap * (0.5 * ContactWeight), Arm[1][2].GetLocation() - Gap * (0.5 * ContactWeight)};

	TArray<FBoneTransform, TInlineAllocator<6>> Solved;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FTransform Upper = Arm[Side][0], Lower = Arm[Side][1], Hand = Arm[Side][2];
		const FQuat HandRotation = Hand.GetRotation();
		// The current elbow as the joint target keeps the arm bending the way the animation has it.
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Lower.GetLocation(), Target[Side], false, 1.0, 1.0);
		Hand.SetRotation(HandRotation);
		Solved.Add(FBoneTransform(Index[Side][0], Upper));
		Solved.Add(FBoneTransform(Index[Side][1], Lower));
		Solved.Add(FBoneTransform(Index[Side][2], Hand));
	}
	Solved.Sort(FCompareBoneTransformIndex());
	Pose.SafeSetCSBoneTransforms(Solved);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
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
	// Runs again when the body swaps characters (a new mesh re-initialises the anim instance).
	if (const UShinobiBodyComponent* Body = Cast<UShinobiBodyComponent>(GetSkelMeshComponent()))
	{
		RetargetNode.IKRetargeterAsset = Body->GetRetargeter();
	}
}
