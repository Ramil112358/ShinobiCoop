#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNode_RigLogic.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "ShinobiFaceShapeAnimInstance.generated.h"

/**
 * Face sliders on a MetaHuman face (character creator): RigLogic writes every facial joint as its DNA neutral plus the
 * expression's delta, so a face's shape can only be changed after it. This proxy runs the face's whole graph natively:
 * copy the body's pose (its attached parent), RigLogic, then move, spread or grow the facial joints around each slider's
 * anchor joints (falloff by distance in the face's own reference pose), so expressions keep working on the reshaped face
 * and the same table fits every MetaHuman face. Preset blending works the same way: each facial joint moves toward where
 * another face (same MetaHuman topology and joint names) has it, by that face's weight.
 */
USTRUCT()
struct FShinobiFaceShapeAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShinobiFaceShapeAnimInstanceProxy() = default;
	FShinobiFaceShapeAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_CopyPoseFromMesh* InCopyPose, FAnimNode_RigLogic* InRigLogic);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void CacheBones() override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return RigLogic; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RigLogic); OutNodes.Add(CopyPose); }

private:
	struct FMovedJoint
	{
		FCompactPoseBoneIndex Bone;
		FCompactPoseBoneIndex Parent;
		int32 ParentMoved; // index into Moved of the parent when it moves too, else INDEX_NONE
		TArray<TPair<int32, FVector>> Offsets; // (slider, component-space offset at value 1, weighted)
		TArray<TPair<int32, float>> Grows; // (slider, scale growth at value 1, weighted)
	};

	FAnimNode_CopyPoseFromMesh* CopyPose = nullptr;
	FAnimNode_RigLogic* RigLogic = nullptr;
	TArray<FMovedJoint> Moved;
	TArray<float> Values; // per slider (ShinobiFaceSliders order), then per blend slot, copied in PreUpdate

	// Each blend slot's face: its facial joints' component-space reference positions, rebuilt when the face changes.
	TArray<TMap<FName, FVector>> BlendPositions;
	uint32 BlendSerial = 0;
};

UCLASS(Transient, NotBlueprintable)
class SHINOBICOOP_API UShinobiFaceShapeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UShinobiFaceShapeAnimInstance();

	/** Sets a face slider (-1..1); false for a name that isn't one. */
	bool SetFaceShape(FName Slider, float Value);
	float GetFaceShape(int32 Slider) const { return Values.IsValidIndex(Slider) ? Values[Slider] : 0.f; }

	/** The slider names, in order. */
	static TArray<FName> GetSliderNames();

	/** Preset blending: Slot (0..MaxBlends-1) pulls this face toward Target's shape by Weight (0..1); null or 0 is off. */
	static constexpr int32 MaxBlends = 2;
	void SetFaceBlend(int32 Slot, USkeletalMesh* Target, float Weight);
	USkeletalMesh* GetBlendTarget(int32 Slot) const { return BlendTargets.IsValidIndex(Slot) ? BlendTargets[Slot].Get() : nullptr; }
	float GetBlendWeight(int32 Slot) const { return BlendWeights.IsValidIndex(Slot) ? BlendWeights[Slot] : 0.f; }
	uint32 GetBlendSerial() const { return BlendSerial; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FAnimNode_CopyPoseFromMesh CopyPose;

	UPROPERTY(Transient)
	FAnimNode_RigLogic RigLogic;

	TArray<float> Values;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMesh>> BlendTargets;

	TArray<float> BlendWeights;
	uint32 BlendSerial = 0;
};
