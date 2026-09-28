#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Revive.generated.h"

class AShinobiPlayerCharacter;

/**
 * Interact (E / gamepad Y). Next to a downed teammate: hold for 3 s to revive them at 30% HP (released early, or
 * staggered: cancelled). Otherwise, next to a villager: they say a line (local). A downed ally always takes priority.
 */
UCLASS()
class SHINOBICOOP_API UGA_Revive : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Revive();

	static constexpr float Seconds = 3.f;
	static constexpr float Reach = 200.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	static constexpr float TalkReach = 250.f;

	/** The villager nearest to Talker within TalkReach, or null. */
	static class AShinobiVillager* FindVillagerNear(const AActor& Talker);

	/** 0-1 while reviving, for the HUD; 0 otherwise. */
	float GetProgress() const;

private:
	AShinobiPlayerCharacter* FindDownedAlly() const;

	UFUNCTION()
	void OnReleased(float TimeHeld);

	UFUNCTION()
	void OnStaggered(FGameplayEventData Payload);

	UFUNCTION()
	void OnHeldLongEnough();

	TWeakObjectPtr<AShinobiPlayerCharacter> Patient;
	double StartTime = 0.0;
};
