#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ShinobiLocomotionAnimInstance.generated.h"

/**
 * Native parent of the Game Animation Sample's motion-matching AnimBP, /Game/Blueprints/SandboxCharacter_CMC_ABP (migrated
 * by Scripts/anim/migrate_gasp.py, reparented by setup_gasp_locomotion.py). It is edited in place rather than duplicated:
 * the sample's chooser tables take exactly SandboxCharacter_CMC_ABP_C as their context and reject a copy.
 *
 * The sample reads its inputs from the pawn through a Blueprint interface (BPI_SandboxCharacter_Pawn,
 * Get_PropertiesForAnimation) that C++ characters can't implement. Its Update_PropertiesFromCharacter now calls
 * UpdateCharacterProperties() instead, which fills the AnimBP's CharacterProperties variable (the Blueprint struct
 * S_CharacterPropertiesForAnimation) from the character and its movement component. Struct fields are found by their
 * authored names and Blueprint enum values by their display names, so the sample's assets stay untouched.
 */
UCLASS()
class SHINOBICOOP_API UShinobiLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The sample's AnimBP (a subclass of this). Load it at BeginPlay or later, never from a constructor: class default
	 * objects are built while this module loads, before plugin modules the sample's assets import (DrawDebugLibrary),
	 * and loading the AnimBP then breaks its nodes for the whole session. Null when the sample isn't migrated. */
	static UClass* LoadAnimBlueprintClass();

	/** Game thread: called from the AnimBP's EventBlueprintUpdateAnimation, before its locomotion logic runs. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Animation")
	void UpdateCharacterProperties();

	/** Walk gait when the character's max speed is at or below this (villagers); Run above it, Sprint while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float WalkGaitMaxSpeed = 250.f;

	/** How long JustLanded stays set after touching down: the AnimBP picks its land animations from it and LandVelocity. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float JustLandedSeconds = 0.3f;

protected:
	virtual void NativeInitializeAnimation() override;

private:
	/** The AnimBP's CharacterProperties variable, and its fields (including the nested InputState's) by authored name. */
	const FStructProperty* PropertiesProperty = nullptr;
	const FStructProperty* InputStateProperty = nullptr;
	TMap<FName, const FProperty*> Fields;
	TMap<FName, TMap<FString, int64>> EnumValues;

	void SetEnum(void* Container, FName Field, const TCHAR* DisplayName) const;

	bool bWasInAir = false;
	FVector LastAirVelocity = FVector::ZeroVector;
	FVector LandVelocity = FVector::ZeroVector;
	double LandedTime = -1.e9;
};
