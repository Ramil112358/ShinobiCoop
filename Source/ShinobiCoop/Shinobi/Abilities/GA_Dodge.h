#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Dodge.generated.h"

class UAnimMontage;

/** Quick dash in the movement direction (backstep with no input). Costs stamina; brief invulnerability. */
UCLASS()
class SHINOBICOOP_API UGA_Dodge : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dodge();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Distance = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.05"))
	float Duration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TSoftObjectPtr<UAnimMontage> Montage;

private:
	UFUNCTION()
	void OnDodgeFinished();
};
