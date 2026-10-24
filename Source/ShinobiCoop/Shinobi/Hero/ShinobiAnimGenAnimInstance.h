#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "ShinobiAnimGenAnimInstance.generated.h"

/**
 * Ambient walking (villagers and Londoners, Shinobi.AnimGenVillagers): native parent of ABP_AnimGenVillager, a copy of the AnimGen Example's NPC AnimBP
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

	/** A trained style that needs no props (the example's others hold phones, bags, ... on its own prop bones), picked by
	 * Seed: a man's or a woman's when known, else only the styles that suit either. */
	static FName PickStyle(int32 Seed, TOptional<bool> bWoman = {});

	/** The controller LOD ambient walkers use: measured cheaper than motion matching (docs/anim/animgen-spike.md). */
	static constexpr int32 AmbientLOD = 2;

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
