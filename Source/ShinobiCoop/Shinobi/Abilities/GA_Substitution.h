#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Substitution.generated.h"

/**
 * Perfect dodge. Triggered on the server by Event.Substitution, which UShinobiDamageExec sends when a hit lands
 * during dodge i-frames and a charge is left. Spends a charge, cancels the dodge, teleports behind the attacker
 * facing them (clients get the corrected position), and staggers the attacker for 0.5 s. The log and smoke left
 * behind are M2b's cue.
 */
UCLASS()
class SHINOBICOOP_API UGA_Substitution : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Substitution();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float BehindDistance = 150.f;
};
