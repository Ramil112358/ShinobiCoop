#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Susanoo.generated.h"

/** Page 5, slot 3: a spectral armour rises around the caster for UGE_Susanoo::Seconds. Damage taken is halved, damage dealt
 * is +30%, chakra comes back faster. A power-up rather than an attack, so it needs no target and charging does nothing. */
UCLASS()
class SHINOBICOOP_API UGA_Susanoo : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Susanoo();

	virtual bool IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const override;

protected:
	virtual void ExecuteJutsu() override;
};
