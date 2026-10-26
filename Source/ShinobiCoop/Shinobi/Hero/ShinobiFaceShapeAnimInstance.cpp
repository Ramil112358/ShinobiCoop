#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "ShinobiCoop.h"

namespace
{
	/**
	 * One face region a slider moves: the facial joints within Radius cm of the Anchor joint (each face's own reference
	 * pose, so the table fits every MetaHuman face) take, at value 1 and fading to nothing at the edge:
	 *  - Move: a translation in face component space (X outward on the left side, mirrored for _R_; +Y forward; +Z up);
	 *  - Spread: their offset from the anchor scaled per axis by (1 + Spread) (width, size);
	 *  - Grow: their own scale by (1 + Grow) (the eyeballs and lids, which sit at the eye's centre).
 * Within Core cm of the anchor the joints take all of it, fading out between Core and Radius.
	 * An _L_ anchor stands for both sides: the _R_ twin is added with Move.X negated.
	 * ponytail: hand-tuned amounts; Creator variants can replace them with measured joint deltas later.
	 */
	struct FFaceRegion
	{
		int32 Slider;
		const TCHAR* Anchor;
		float Radius;
		FVector Move = FVector::ZeroVector;
		FVector Spread = FVector::ZeroVector;
		float Grow = 0.f;
		float Core = 0.f; // within this many cm the full amount (a feature moving as one, the eye with its lids)
	};

	const FName SliderNames[] = {
		TEXT("JawWidth"), TEXT("JawAngleHeight"), TEXT("ChinLength"), TEXT("ChinWidth"), TEXT("ChinForward"),
		TEXT("CheekboneWidth"), TEXT("CheekboneHeight"), TEXT("CheekFullness"),
		TEXT("NoseProjection"), TEXT("NoseLength"), TEXT("NoseWidth"), TEXT("NoseBridgeHeight"), TEXT("NoseBridgeWidth"), TEXT("NoseTipTilt"),
		TEXT("EyeSpacing"), TEXT("EyeHeight"), TEXT("EyeSize"), TEXT("EyeTilt"), TEXT("EyeDepth"),
		TEXT("BrowHeight"), TEXT("BrowProtrusion"), TEXT("BrowTilt"),
		TEXT("MouthWidth"), TEXT("MouthHeight"), TEXT("MouthProtrusion"), TEXT("UpperLipThickness"), TEXT("LowerLipThickness"), TEXT("LipCornerHeight"),
		TEXT("EarSize"), TEXT("EarAngle"),
		TEXT("ForeheadSlope"), TEXT("ForeheadHeight"), TEXT("TempleWidth"),
	};

	enum ESlider : int32
	{
		JawWidth, JawAngleHeight, ChinLength, ChinWidth, ChinForward,
		CheekboneWidth, CheekboneHeight, CheekFullness,
		NoseProjection, NoseLength, NoseWidth, NoseBridgeHeight, NoseBridgeWidth, NoseTipTilt,
		EyeSpacing, EyeHeight, EyeSize, EyeTilt, EyeDepth,
		BrowHeight, BrowProtrusion, BrowTilt,
		MouthWidth, MouthHeight, MouthProtrusion, UpperLipThickness, LowerLipThickness, LipCornerHeight,
		EarSize, EarAngle,
		ForeheadSlope, ForeheadHeight, TempleWidth,
		SliderCount
	};
	static_assert(SliderCount == UE_ARRAY_COUNT(SliderNames), "one name per slider");

	/** Every region's Move and Spread at value 1 are this many times the table's (tuned by eye on crowd faces at 1.2 m). */
	constexpr float Reach = 2.f;

	const FFaceRegion Regions[] = {
		{JawWidth, TEXT("FACIAL_L_JawBulge"), 4.5f, FVector(1.2, 0.0, 0.0), FVector::ZeroVector, 0.f, 1.5f},
		{JawWidth, TEXT("FACIAL_L_Jawline"), 3.5f, FVector(0.8, 0.0, 0.0)},
		{JawAngleHeight, TEXT("FACIAL_L_JawBulge"), 3.5f, FVector(0.0, 0.0, 0.6)},
		{ChinLength, TEXT("FACIAL_C_Chin"), 3.5f, FVector(0.0, 0.0, -0.9), FVector::ZeroVector, 0.f, 1.2f},
		{ChinLength, TEXT("FACIAL_C_UnderChin"), 3.f, FVector(0.0, 0.0, -0.4)},
		{ChinWidth, TEXT("FACIAL_C_Chin"), 4.f, FVector::ZeroVector, FVector(0.5, 0.0, 0.0)},
		{ChinForward, TEXT("FACIAL_C_Chin"), 4.f, FVector(0.0, 0.9, 0.0), FVector::ZeroVector, 0.f, 1.2f},
		{CheekboneWidth, TEXT("FACIAL_L_CheekOuter4"), 3.5f, FVector(0.9, 0.0, 0.0)},
		{CheekboneHeight, TEXT("FACIAL_L_CheekOuter4"), 3.f, FVector(0.0, 0.0, 0.5)},
		{CheekFullness, TEXT("FACIAL_L_CheekOuter2"), 3.f, FVector(0.3, 0.4, 0.0)},
		{CheekFullness, TEXT("FACIAL_L_CheekLower"), 2.5f, FVector(0.2, 0.3, 0.0)},
		{NoseProjection, TEXT("FACIAL_C_NoseTip"), 2.2f, FVector(0.0, 0.6, 0.0)},
		{NoseLength, TEXT("FACIAL_C_NoseTip"), 2.5f, FVector(0.0, 0.0, -0.5)},
		{NoseWidth, TEXT("FACIAL_L_Nostril"), 1.5f, FVector(0.35, 0.0, 0.0)},
		{NoseBridgeHeight, TEXT("FACIAL_C_NoseBridge"), 1.8f, FVector(0.0, 0.4, 0.0)},
		{NoseBridgeWidth, TEXT("FACIAL_L_NoseBridge"), 1.2f, FVector(0.25, 0.0, 0.0)},
		{NoseTipTilt, TEXT("FACIAL_C_NoseTip"), 1.5f, FVector(0.0, 0.0, 0.3)},
		{EyeSpacing, TEXT("FACIAL_L_Eye"), 3.4f, FVector(0.3, 0.0, 0.0), FVector::ZeroVector, 0.f, 1.9f},
		{EyeHeight, TEXT("FACIAL_L_Eye"), 3.4f, FVector(0.0, 0.0, 0.3), FVector::ZeroVector, 0.f, 1.9f},
		{EyeSize, TEXT("FACIAL_L_Eye"), 0.3f, FVector::ZeroVector, FVector::ZeroVector, 0.04f},
		{EyeSize, TEXT("FACIAL_L_Eye"), 3.f, FVector::ZeroVector, FVector(0.05, 0.0, 0.05), 0.f, 1.9f},
		{EyeTilt, TEXT("FACIAL_L_EyeCornerOuter"), 1.2f, FVector(0.0, 0.0, 0.35)},
		{EyeTilt, TEXT("FACIAL_L_EyeCornerInner"), 1.0f, FVector(0.0, 0.0, -0.15)},
		{EyeDepth, TEXT("FACIAL_L_Eye"), 3.4f, FVector(0.0, -0.25, 0.0), FVector::ZeroVector, 0.f, 1.9f},
		{BrowHeight, TEXT("FACIAL_L_ForeheadMid"), 2.5f, FVector(0.0, 0.0, 0.5)},
		{BrowProtrusion, TEXT("FACIAL_L_ForeheadMid"), 2.5f, FVector(0.0, 0.4, 0.0)},
		{BrowTilt, TEXT("FACIAL_L_ForeheadOut"), 1.8f, FVector(0.0, 0.0, 0.4)},
		{MouthWidth, TEXT("FACIAL_L_LipCorner"), 1.5f, FVector(0.4, 0.0, 0.0)},
		{MouthHeight, TEXT("FACIAL_C_LipUpper"), 3.f, FVector(0.0, 0.0, 0.4)},
		{MouthHeight, TEXT("FACIAL_C_LipLower"), 3.f, FVector(0.0, 0.0, 0.4)},
		{MouthProtrusion, TEXT("FACIAL_C_LipUpper"), 3.f, FVector(0.0, 0.4, 0.0)},
		{MouthProtrusion, TEXT("FACIAL_C_LipLower"), 3.f, FVector(0.0, 0.4, 0.0)},
		{UpperLipThickness, TEXT("FACIAL_C_LipUpper1"), 2.f, FVector(0.0, 0.15, 0.15)},
		{LowerLipThickness, TEXT("FACIAL_C_LipLower1"), 2.f, FVector(0.0, 0.15, -0.15)},
		{LipCornerHeight, TEXT("FACIAL_L_LipCorner"), 1.2f, FVector(0.0, 0.0, 0.3)},
		{EarSize, TEXT("FACIAL_L_Ear"), 3.f, FVector::ZeroVector, FVector(0.2, 0.2, 0.2)},
		{EarAngle, TEXT("FACIAL_L_Ear3"), 2.f, FVector(0.5, 0.0, 0.0)},
		{ForeheadSlope, TEXT("FACIAL_C_ForeheadSkin"), 3.f, FVector(0.0, -0.5, 0.0)},
		{ForeheadHeight, TEXT("FACIAL_C_Hair1"), 3.f, FVector(0.0, 0.0, 0.6)},
		{TempleWidth, TEXT("FACIAL_L_Temple"), 2.5f, FVector(0.4, 0.0, 0.0)},
	};
}

FShinobiFaceShapeAnimInstanceProxy::FShinobiFaceShapeAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_CopyPoseFromMesh* InCopyPose, FAnimNode_RigLogic* InRigLogic)
	: FAnimInstanceProxy(InAnimInstance)
	, CopyPose(InCopyPose)
	, RigLogic(InRigLogic)
{
}

void FShinobiFaceShapeAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	// The body's pose (this face's attached parent), then RigLogic on top, as the face's own blueprints do.
	CopyPose->bUseAttachedParent = true;
	CopyPose->bCopyCurves = true;
	RigLogic->AnimSequence.SetLinkNode(CopyPose);
	FAnimationInitializeContext InitContext(this);
	RigLogic->Initialize_AnyThread(InitContext);
}

void FShinobiFaceShapeAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UShinobiFaceShapeAnimInstance* Instance = CastChecked<UShinobiFaceShapeAnimInstance>(InAnimInstance);
	Values.SetNum(SliderCount + UShinobiFaceShapeAnimInstance::MaxBlends);
	for (int32 Slider = 0; Slider < SliderCount; ++Slider)
	{
		Values[Slider] = Instance->GetFaceShape(Slider);
	}
	for (int32 Slot = 0; Slot < UShinobiFaceShapeAnimInstance::MaxBlends; ++Slot)
	{
		Values[SliderCount + Slot] = Instance->GetBlendTarget(Slot) ? Instance->GetBlendWeight(Slot) : 0.f;
	}
	if (BlendSerial != Instance->GetBlendSerial())
	{
		// A blend face changed: read its facial joints' reference positions here (game thread), rebuild the offsets.
		BlendSerial = Instance->GetBlendSerial();
		BlendPositions.Reset();
		for (int32 Slot = 0; Slot < UShinobiFaceShapeAnimInstance::MaxBlends; ++Slot)
		{
			TMap<FName, FVector>& Positions = BlendPositions.AddDefaulted_GetRef();
			if (const USkeletalMesh* Target = Instance->GetBlendTarget(Slot))
			{
				const FReferenceSkeleton& Ref = Target->GetRefSkeleton();
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					if (Ref.GetBoneName(Bone).ToString().StartsWith(TEXT("FACIAL_")))
					{
						Positions.Add(Ref.GetBoneName(Bone), FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation());
					}
				}
			}
		}
		bBoneCachesInvalidated = true;
	}
}

void FShinobiFaceShapeAnimInstanceProxy::CacheBones()
{
	if (!bBoneCachesInvalidated)
	{
		return;
	}
	FAnimationCacheBonesContext Context(this);
	RigLogic->CacheBones_AnyThread(Context);
	bBoneCachesInvalidated = false;

	// Which facial joints each region moves, from the reference pose (parents come before children in compact order).
	Moved.Reset();
	const FBoneContainer& Bones = GetRequiredBones();
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	struct FPlaced { int32 Slider; FVector Anchor; float Radius; FVector Move, Spread; float Grow, Core; };
	TArray<FPlaced> Placed;
	for (const FFaceRegion& Region : Regions)
	{
		const FString Left = Region.Anchor;
		const bool bSided = Left.Contains(TEXT("_L_"));
		for (int32 Side = 0; Side < (bSided ? 2 : 1); ++Side)
		{
			const int32 Anchor = Ref.FindBoneIndex(FName(Side ? *Left.Replace(TEXT("_L_"), TEXT("_R_")) : *Left));
			if (Anchor == INDEX_NONE)
			{
				continue;
			}
			const FVector Move = (Side ? FVector(-Region.Move.X, Region.Move.Y, Region.Move.Z) : Region.Move) * Reach;
			Placed.Add({Region.Slider, FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Anchor).GetLocation(), Region.Radius, Move, Region.Spread * Reach, Region.Grow * Reach, Region.Core});
		}
	}
	static const FName FacialRoot(TEXT("FACIAL_C_FacialRoot"));
	const int32 OwnRoot = Ref.FindBoneIndex(FacialRoot);
	const FVector OwnRootPosition = OwnRoot != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, OwnRoot).GetLocation() : FVector::ZeroVector;
	TArray<TPair<float, FName>> MaxDelta;
	MaxDelta.SetNum(BlendPositions.Num());
	TMap<int32, int32> MovedByMeshIndex;
	for (const FBoneIndexType MeshIndex : Bones.GetBoneIndicesArray())
	{
		if (!Ref.GetBoneName(MeshIndex).ToString().StartsWith(TEXT("FACIAL_")))
		{
			continue;
		}
		const FVector Position = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, MeshIndex).GetLocation();
		FMovedJoint Joint;
		for (const FPlaced& Region : Placed)
		{
			const float Distance = FVector::Dist(Position, Region.Anchor);
			if (Distance >= Region.Radius)
			{
				continue;
			}
			const float Weight = FMath::SmoothStep(0.f, 1.f, 1.f - FMath::Max(Distance - Region.Core, 0.f) / (Region.Radius - Region.Core));
			const FVector Offset = (Region.Move + (Position - Region.Anchor) * Region.Spread) * Weight;
			if (!Offset.IsNearlyZero())
			{
				Joint.Offsets.Emplace(Region.Slider, Offset);
			}
			if (Region.Grow != 0.f)
			{
				Joint.Grows.Emplace(Region.Slider, Region.Grow * Weight);
			}
		}
		// Each face's joints relative to its own facial root: faces sit at different heights in their reference poses.
		for (int32 Slot = 0; Slot < BlendPositions.Num(); ++Slot)
		{
			const FVector* There = BlendPositions[Slot].Find(Ref.GetBoneName(MeshIndex));
			const FVector* ThereRoot = BlendPositions[Slot].Find(FacialRoot);
			if (There && ThereRoot && OwnRoot != INDEX_NONE)
			{
				const FVector Delta = (*There - *ThereRoot) - (Position - OwnRootPosition);
				if (Delta.Size() > MaxDelta[Slot].Key)
				{
					MaxDelta[Slot] = {static_cast<float>(Delta.Size()), Ref.GetBoneName(MeshIndex)};
				}
				if (!Delta.IsNearlyZero(0.01))
				{
					Joint.Offsets.Emplace(SliderCount + Slot, Delta);
				}
			}
		}
		if (Joint.Offsets.IsEmpty() && Joint.Grows.IsEmpty())
		{
			continue;
		}
		const int32 ParentMesh = Ref.GetParentIndex(MeshIndex);
		Joint.Bone = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		Joint.Parent = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ParentMesh));
		const int32* ParentMoved = MovedByMeshIndex.Find(ParentMesh);
		Joint.ParentMoved = ParentMoved ? *ParentMoved : INDEX_NONE;
		MovedByMeshIndex.Add(MeshIndex, Moved.Add(MoveTemp(Joint)));
	}
	for (int32 Slot = 0; Slot < MaxDelta.Num(); ++Slot)
	{
		UE_CLOG(MaxDelta[Slot].Key > 0.f, LogShinobiCoop, Log, TEXT("Face blend %d: largest joint move %.2f cm (%s)"), Slot, MaxDelta[Slot].Key, *MaxDelta[Slot].Value.ToString());
	}
}

bool FShinobiFaceShapeAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	RigLogic->Evaluate_AnyThread(Output);
	bool bAny = false;
	for (const float Value : Values)
	{
		bAny |= !FMath::IsNearlyZero(Value);
	}
	if (!bAny || Moved.IsEmpty())
	{
		return true;
	}
	// Each joint's component-space offset, less its moved parent's (the parent already carries it), into the parent's space.
	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	TArray<FVector> Offset;
	Offset.SetNumZeroed(Moved.Num());
	for (int32 Index = 0; Index < Moved.Num(); ++Index)
	{
		const FMovedJoint& Joint = Moved[Index];
		for (const TPair<int32, FVector>& Part : Joint.Offsets)
		{
			Offset[Index] += Part.Value * Values[Part.Key];
		}
		float Grow = 0.f;
		for (const TPair<int32, float>& Part : Joint.Grows)
		{
			Grow += Part.Value * Values[Part.Key];
		}
		if (Grow != 0.f)
		{
			Output.Pose[Joint.Bone].SetScale3D(Output.Pose[Joint.Bone].GetScale3D() * (1.f + Grow));
		}
		const FVector Own = Offset[Index] - (Joint.ParentMoved != INDEX_NONE ? Offset[Joint.ParentMoved] : FVector::ZeroVector);
		if (!Joint.Parent.IsValid() || Own.IsNearlyZero())
		{
			continue;
		}
		const FTransform& Parent = Pose.GetComponentSpaceTransform(Joint.Parent);
		Output.Pose[Joint.Bone].AddToTranslation(Parent.GetRotation().UnrotateVector(Own) / Parent.GetScale3D());
	}
	return true;
}

void FShinobiFaceShapeAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	RigLogic->Update_AnyThread(InContext);
}

UShinobiFaceShapeAnimInstance::UShinobiFaceShapeAnimInstance()
{
	// The body's pose is read from another component during the update (as UShinobiRetargetAnimInstance does).
	bUseMultiThreadedAnimationUpdate = false;
	Values.SetNumZeroed(SliderCount);
}

bool UShinobiFaceShapeAnimInstance::SetFaceShape(FName Slider, float Value)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SliderNames); ++Index)
	{
		if (SliderNames[Index] == Slider)
		{
			Values[Index] = FMath::Clamp(Value, -1.f, 1.f);
			return true;
		}
	}
	return false;
}

void UShinobiFaceShapeAnimInstance::SetFaceBlend(int32 Slot, USkeletalMesh* Target, float Weight)
{
	if (Slot < 0 || Slot >= MaxBlends)
	{
		return;
	}
	BlendTargets.SetNum(MaxBlends);
	BlendWeights.SetNum(MaxBlends);
	if (BlendTargets[Slot] != Target)
	{
		BlendTargets[Slot] = Target;
		++BlendSerial;
	}
	BlendWeights[Slot] = FMath::Clamp(Weight, 0.f, 1.f);
}

TArray<FName> UShinobiFaceShapeAnimInstance::GetSliderNames()
{
	return TArray<FName>(SliderNames, UE_ARRAY_COUNT(SliderNames));
}

FAnimInstanceProxy* UShinobiFaceShapeAnimInstance::CreateAnimInstanceProxy()
{
	return new FShinobiFaceShapeAnimInstanceProxy(this, &CopyPose, &RigLogic);
}
