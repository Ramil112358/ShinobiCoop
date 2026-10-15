#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Block.generated.h"

/**
 * Hold to guard: a Susanoo ribcage rises round the player (UGE_Blocking, shown by AShinobiPlayerCharacter's guard figure), who
 * stands still behind it. Hits cost stamina instead of most of their damage (UShinobiDamageExec). Ends on release, when the
 * guard breaks or the player goes down (the tag goes), or when a dodge cancels it; then stamina regen waits as after a sprint.
 */
UCLASS()
class SHINOBICOOP_API UGA_Block : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Block();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnGuardRemoved();

	FActiveGameplayEffectHandle GuardHandle;
	bool bGuardRaised = false;
};
