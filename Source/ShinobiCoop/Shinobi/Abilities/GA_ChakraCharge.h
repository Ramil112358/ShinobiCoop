#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_ChakraCharge.generated.h"

/** Hold to refill chakra quickly. Movement input is ignored while State.ChakraCharging is present. */
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

	FActiveGameplayEffectHandle ChargeHandle;
};
