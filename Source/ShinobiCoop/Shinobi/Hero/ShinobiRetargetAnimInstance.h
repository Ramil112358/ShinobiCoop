#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "ShinobiRetargetAnimInstance.generated.h"

class UIKRetargeter;
class UAnimMontage;

/** A jutsu cast phase, AM_<Jutsu>_<Seal|Charge|Release> (Scripts/setup_jutsu_anims.py): hands that must land exactly. */
SHINOBICOOP_API bool ShinobiIsJutsuCast(const UAnimMontage* Montage);

/** Runs the anim instance's one node: retarget the attached parent mesh's pose onto this mesh. */
USTRUCT()
struct FShinobiRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShinobiRetargetAnimInstanceProxy() = default;
	FShinobiRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void CacheBones() override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }

	/** Full hand contact while the source's wrists are this close (cm), none from ContactFar (the Horse seal's are ~24 apart). */
	static constexpr float ContactNear = 26.f;
	static constexpr float ContactFar = 36.f;

private:
	/**
	 * The FK retarget copies rotations, so on a hero with other proportions (and the arm spread of setup_hero.py) hands
	 * that touch on the source drift apart. While a jutsu montage plays and the source's hands are close (a hand seal), this
	 * moves the hero's left hand back to where the source's is relative to the right one, half by each arm (two-bone IK,
	 * hands keep their rotation; setup_hero.py's align_hand_roll makes that match the source's). Scripts/setup_jutsu_anims.py
	 * does the same offline for the Mixamo clips onto Manny.
	 */
	void KeepHandContact(FPoseContext& Output) const;

	FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;

	// Read on the game thread in PreUpdate, used on the anim thread in Evaluate (same frame).
	float ContactWeight = 0.f;
	FVector SourceHandOffset = FVector::ZeroVector; // the source's left wrist from its right one, world
	float SourceForeArmLength = 0.f; // world
	FTransform ComponentToWorld;

	/** The hero's arms, [left, right][upper arm, forearm, hand], from the retargeter's LeftArm and RightArm chains. */
	FBoneReference ArmBones[2][3];
};

/**
 * Copies the pose of the skeletal mesh this component is attached to (the player's hidden Manny, which runs every
 * gameplay animation) onto the hero mesh through the character's retargeter (RTG_MannyTo<Character>), every frame, on every machine. The native C++
 * equivalent of an Animation Blueprint holding one "Retarget Pose From Mesh" node, modelled on the engine's own
 * IKRetargetAnimInstance, so no Animation Blueprint asset is needed.
 */
UCLASS(Transient, NotBlueprintable)
class SHINOBICOOP_API UShinobiRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UShinobiRetargetAnimInstance();

	/** The retargeter in use: its UShinobiBodyComponent's, as of the last (re)initialise. */
	const UIKRetargeter* GetRetargeter() const { return RetargetNode.IKRetargeterAsset; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeInitializeAnimation() override;

	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
