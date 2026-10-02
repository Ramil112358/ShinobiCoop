#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Teleport.generated.h"

/** Page 2, slot 1 (Flying Raijin): throws three marker kunai in a fan toward the aim point, then ends. For the next 6 s
 * the same key (UGA_TeleportJump) teleports the caster to one of them. */
UCLASS()
class SHINOBICOOP_API UGA_Teleport : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Teleport();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRange = 2500.f;

	/** The side kunai fly this many degrees either way of the middle one. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FanDegrees = 12.f;

protected:
	virtual void ExecuteJutsu() override;
};

/**
 * The same key while State.TeleportMarked: teleports to the stuck kunai nearest the crosshair (within PickDegrees of the
 * aim, else the oldest), hits enemies near the arrival, and uses that kunai up. No cost and no cooldown; the window
 * closes with the last kunai or when it runs out.
 */
UCLASS()
class SHINOBICOOP_API UGA_TeleportJump : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_TeleportJump();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float PickDegrees = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ArrivalRadius = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ArrivalDamage = 20.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
