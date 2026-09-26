#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_ChakraCharge.generated.h"

class UAnimMontage;

/**
 * Hold to charge chakra (Naruto Storm style). Works at any chakra level, including full, so the pose and
 * aura always play; ends on release, dodge, a staggering hit, or its pose being interrupted. Movement and
 * jump are ignored while State.ChakraCharging is present.
 */
UCLASS()
class SHINOBICOOP_API UGA_ChakraCharge : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ChakraCharge();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** A staggering hit, or another montage taking over the pose: end the charge. */
	UFUNCTION()
	void OnChargeBroken();

	UFUNCTION()
	void OnStaggered(FGameplayEventData Payload);

	FActiveGameplayEffectHandle ChargeHandle;

	/** The Mixamo "Power Up" pose, retargeted to Manny; looped while C is held (see ActivateAbility). */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TSoftObjectPtr<UAnimMontage> Montage;
};
