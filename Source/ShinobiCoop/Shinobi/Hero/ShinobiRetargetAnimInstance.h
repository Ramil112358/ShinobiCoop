#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "ShinobiRetargetAnimInstance.generated.h"

class UIKRetargeter;

/** Runs the anim instance's one node: retarget the attached parent mesh's pose onto this mesh. */
USTRUCT()
struct FShinobiRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShinobiRetargetAnimInstanceProxy() = default;
	FShinobiRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void CacheBones() override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }

private:
	FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;
};

/**
 * Copies the pose of the skeletal mesh this component is attached to (the player's hidden Manny, which runs every
 * gameplay animation) onto the hero mesh through RTG_MannyToHero, every frame, on every machine. The native C++
 * equivalent of an Animation Blueprint holding one "Retarget Pose From Mesh" node, modelled on the engine's own
 * IKRetargetAnimInstance, so no Animation Blueprint asset is needed.
 */
UCLASS(Transient, NotBlueprintable)
class SHINOBICOOP_API UShinobiRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UShinobiRetargetAnimInstance();

	static constexpr const TCHAR* RetargeterPath = TEXT("/Game/Shinobi/Hero/RTG_MannyToHero.RTG_MannyToHero");

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeInitializeAnimation() override;

	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
