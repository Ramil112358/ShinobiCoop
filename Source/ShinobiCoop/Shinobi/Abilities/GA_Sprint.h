#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Sprint.generated.h"

/** Hold to sprint. Drains stamina; ends on release or at 0 stamina, then delays stamina regen. */
UCLASS()
class SHINOBICOOP_API UGA_Sprint : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Sprint();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnStaminaThreshold(bool bMatchesComparison, float CurrentValue);

	void SetSprinting(bool bSprinting) const;

	FActiveGameplayEffectHandle DrainHandle;
	bool bSprintStarted = false;
};
