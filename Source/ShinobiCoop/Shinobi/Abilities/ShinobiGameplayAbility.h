#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ShinobiGameplayAbility.generated.h"

class AShinobiCharacterBase;
class UShinobiMovementComponent;

/** Input slots abilities bind to. Values are passed to the ASC as InputID. */
UENUM(BlueprintType)
enum class EShinobiAbilityInputID : uint8
{
	None,
	Melee,
	Dodge,
	Sprint,
	ChakraCharge,
	Jutsu1,
	Jutsu2,
	Jutsu3,
	Jutsu4,
	Revive
};

/** Base for all Shinobi abilities: instanced per actor, predicted on the owning client. */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UShinobiGameplayAbility();

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	EShinobiAbilityInputID InputID = EShinobiAbilityInputID::None;

protected:
	AShinobiCharacterBase* GetShinobiCharacter() const;
	UShinobiMovementComponent* GetShinobiMovement() const;

	/** Players: the camera (the server has the client's from ServerUpdateCamera). AI and tests: the eyes. */
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Applies an effect class (no SetByCaller) to the owner. */
	FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass) const;

	/** Server only. Deals Amount through the damage exec (ShinobiCombat::ApplyDamage). Returns whether the hit
	 * landed: false if not authority, no target, or the target was invulnerable. */
	bool ApplyDamage(AShinobiCharacterBase* Target, float Amount) const;

	/** Hostile characters within Range (2D) whose direction is within HalfAngleDegrees of the owner's forward. */
	TArray<AShinobiCharacterBase*> FindHostilesInCone(float Range, float HalfAngleDegrees) const;

	/** The closest (2D) of FindHostilesInCone, or null. */
	AShinobiCharacterBase* FindNearestHostileInCone(float Range, float HalfAngleDegrees) const;
};
