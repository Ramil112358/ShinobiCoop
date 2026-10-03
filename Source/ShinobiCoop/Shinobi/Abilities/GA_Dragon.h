#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Dragon.generated.h"

/** Page 5, slot 1: Summon Dragon. After the Kuchiyose seals a dragon rises under the caster, who rides it for UGE_Mounted::Seconds
 * (UShinobiMovementComponent flies them; Space climbs, Control dives; melee becomes UGA_DragonBreath). Like the other summons, charging
 * does nothing. */
UCLASS()
class SHINOBICOOP_API UGA_SummonDragon : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_SummonDragon();

protected:
	virtual void ExecuteJutsu() override;
};

/** The same key while mounted: climbs down. Not a jutsu (no seals, cost or cooldown of its own). */
UCLASS()
class SHINOBICOOP_API UGA_DragonDismount : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DragonDismount();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
