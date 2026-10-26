#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "Animation/AnimMontage.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/VFX/ShinobiCastFxComponent.h"

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
	OrbWeight = 0.f;
	const USkeletalMeshComponent* Mesh = InAnimInstance->GetSkelMeshComponent();
	BoneShapes.Reset();
	if (const UShinobiBodyComponent* Body = Cast<UShinobiBodyComponent>(Mesh))
	{
		BoneShapes = Body->GetBoneShapes().Array();
	}
	const USkeletalMeshComponent* Source = Mesh ? Cast<USkeletalMeshComponent>(Mesh->GetAttachParent()) : nullptr;
	const UAnimInstance* SourceAnim = Source ? Source->GetAnimInstance() : nullptr;
	if (!SourceAnim)
	{
		return;
	}
	float MontageWeight = 0.f;
	float FormingWeight = 0.f; // the Rasengan forming in the hand (not the dash)
	for (const FAnimMontageInstance* Montage : SourceAnim->MontageInstances)
	{
		// Only the jutsu casts; the guard, roll and seat keep the plain retarget.
		MontageWeight += Montage && ShinobiIsJutsuCast(Montage->Montage) ? Montage->GetWeight() : 0.f;
		const FString Name = Montage && Montage->Montage ? Montage->Montage->GetName() : FString();
		FormingWeight += Name == TEXT("AM_Rasengan_Seal") || Name == TEXT("AM_Rasengan_Charge") ? Montage->GetWeight() : 0.f;
	}
	const AShinobiCharacterBase* Owner = Cast<AShinobiCharacterBase>(Mesh->GetOwner());
	const UShinobiCastFxComponent* CastFx = Owner ? Owner->GetCastFx() : nullptr;
	OrbRadius = CastFx ? CastFx->GetRasenganOuterRadius() : 0.f;
	if (OrbRadius > 0.f)
	{
		OrbCentre = CastFx->GetOrbLocation();
		OrbWeight = FMath::Min(FormingWeight, 1.f);
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
	if (BoneShapes.Num())
	{
		ShapeBones(Output);
	}
	if (ContactWeight > UE_KINDA_SMALL_NUMBER)
	{
		KeepHandContact(Output);
	}
	if (OrbWeight > UE_KINDA_SMALL_NUMBER)
	{
		KeepPalmOnOrb(Output);
	}
	return true;
}

void FShinobiRetargetAnimInstanceProxy::ShapeBones(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	for (const TPair<FName, FVector>& Shape : BoneShapes)
	{
		const int32 MeshIndex = Ref.FindBoneIndex(Shape.Key);
		const FCompactPoseBoneIndex Bone = MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
		if (!Bone.IsValid())
		{
			continue;
		}
		Output.Pose[Bone].SetScale3D(Output.Pose[Bone].GetScale3D() * Shape.Value);
		const FVector Inverse = FVector::OneVector / Shape.Value;
		for (int32 Child = MeshIndex + 1; Child < Ref.GetNum(); ++Child)
		{
			if (Ref.GetParentIndex(Child) != MeshIndex)
			{
				continue;
			}
			bool bHasChildren = false;
			for (int32 Grandchild = Child + 1; Grandchild < Ref.GetNum() && !bHasChildren; ++Grandchild)
			{
				bHasChildren = Ref.GetParentIndex(Grandchild) == Child;
			}
			const FCompactPoseBoneIndex Compact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Child));
			if (bHasChildren && Compact.IsValid())
			{
				Output.Pose[Compact].SetScale3D(Output.Pose[Compact].GetScale3D() * Inverse);
			}
		}
	}
}

bool FShinobiRetargetAnimInstanceProxy::GetArms(const FPoseContext& Output, FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex (&Index)[2][3], FTransform (&Arm)[2][3]) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			if (!ArmBones[Side][Joint].IsValidToEvaluate(Bones))
			{
				return false;
			}
			Index[Side][Joint] = ArmBones[Side][Joint].GetCompactPoseIndex(Bones);
		}
	}
	Pose.InitPose(Output.Pose);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			Arm[Side][Joint] = Pose.GetComponentSpaceTransform(Index[Side][Joint]);
		}
	}
	return true;
}

void FShinobiRetargetAnimInstanceProxy::KeepPalmOnOrb(FPoseContext& Output) const
{
	FCSPose<FCompactPose> Pose;
	FCompactPoseBoneIndex Index[2][3] = {{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)},
		{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)}};
	FTransform Arm[2][3];
	if (!GetArms(Output, Pose, Index, Arm))
	{
		return;
	}
	const FVector Centre = ComponentToWorld.InverseTransformPosition(OrbCentre);
	const double Radius = ComponentToWorld.InverseTransformVector(FVector(OrbRadius, 0, 0)).Size();
	const FVector Wrist = Arm[0][2].GetLocation();
	const FVector OnSurface = Centre + (Wrist - Centre).GetSafeNormal() * Radius;
	FTransform Upper = Arm[0][0], Lower = Arm[0][1], Hand = Arm[0][2];
	const FQuat HandRotation = Hand.GetRotation();
	AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Lower.GetLocation(), FMath::Lerp(Wrist, OnSurface, static_cast<double>(OrbWeight)), false, 1.0, 1.0);
	Hand.SetRotation(HandRotation);
	TArray<FBoneTransform, TInlineAllocator<3>> Solved = {FBoneTransform(Index[0][0], Upper), FBoneTransform(Index[0][1], Lower), FBoneTransform(Index[0][2], Hand)};
	Pose.SafeSetCSBoneTransforms(Solved);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose), Output.Pose);
}

void FShinobiRetargetAnimInstanceProxy::KeepHandContact(FPoseContext& Output) const
{
	FCSPose<FCompactPose> Pose;
	FCompactPoseBoneIndex Index[2][3] = {{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)},
		{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)}};
	FTransform Arm[2][3];
	if (!GetArms(Output, Pose, Index, Arm))
	{
		return;
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
