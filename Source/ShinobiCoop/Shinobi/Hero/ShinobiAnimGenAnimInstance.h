#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "ShinobiAnimGenAnimInstance.generated.h"

/**
 * TRIAL (feature/animgen): native parent of ABP_AnimGenVillager, a copy of the AnimGen Example's NPC AnimBP
 * (/AnimGenExample/Blueprints/ABP_AnimGenNPC), whose AnimGen node generates walking from a trained ML controller
 * (AGC_AnimGenNPC) in one of ~40 trained styles.
 *
 * The example drives a Mover pawn; our villagers walk on CharacterMovement. The copy's event graph calls
 * UpdateAnimGenInputs() instead of reading the example pawn's interface and Mover predictor: it fills the AnimBP's
 * Properties (S_NPCPropertiesForAnimation: style, LOD) and builds its Trajectory from the character movement, as the
 * Game Animation Sample's CMC AnimBP does.
 */
UCLASS()
class SHINOBICOOP_API UShinobiAnimGenAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The AnimBP (a subclass of this), or null when the AnimGen plugins aren't enabled. BeginPlay or later only. */
	static UClass* LoadAnimBlueprintClass();

	/** Trained styles that need no props (the example's other styles hold phones, bags, ... on its own prop bones). */
	static const TArray<FName>& PropFreeStyles();

	/** Game thread: called from the AnimBP's EventBlueprintUpdateAnimation. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Animation")
	void UpdateAnimGenInputs(float DeltaSeconds);

	/** The trained style to walk in (StyleName in the example). */
	UPROPERTY(BlueprintReadWrite, Category = "Shinobi")
	FName Style = TEXT("NeutralMale");

	/** The controller's LOD (0-2): higher is cheaper and rougher. */
	UPROPERTY(BlueprintReadWrite, Category = "Shinobi")
	int32 LODLevel = 0;

private:
	FPoseSearchTrajectoryData TrajectoryData;
};
