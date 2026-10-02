#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Kamui.generated.h"

/** Page 2, slot 2: phase out for UGE_Kamui::Seconds. Attacks and burning pass through, the caster walks through enemies
 * and the AI drops them, but they cannot attack. The jutsu itself ends at once; UGA_KamuiExit (the same key) steps out early. */
UCLASS()
class SHINOBICOOP_API UGA_Kamui : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Kamui();

protected:
	virtual void ExecuteJutsu() override;
};

/** The same key while phased: ends Kamui early. Not a jutsu (no seals, cost or cooldown of its own). */
UCLASS()
class SHINOBICOOP_API UGA_KamuiExit : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_KamuiExit();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
