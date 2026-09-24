#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Melee.generated.h"

class UAnimMontage;
class UAbilityTask_WaitDelay;

/** Three-hit taijutsu combo. Pressing again inside the combo window continues; each landed hit grants chakra. */
UCLASS()
class SHINOBICOOP_API UGA_Melee : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Melee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSoftObjectPtr<UAnimMontage>> ComboMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<float> ComboDamage;

	/** Seconds from swing start to the damage check. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HitDelay = 0.25f;

	/** Seconds after a hit during which pressing again continues the combo. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float ComboWindow = 0.5f;

	/** Extra seconds added to ComboWindow for the server's copy of a remote player's ability: that
	 * copy's window-close timer is only a safety net (the owning client's own window-close normally
	 * ends the ability first, via ServerEndAbility), so give it slack for a combo press in flight. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float RemoteWindowGrace = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Range = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HalfAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float ChakraPerHit = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FinisherKnockback = 600.f;

private:
	void StartSwing();

	UFUNCTION()
	void OnHitFrame();

	UFUNCTION()
	void OnComboInput(float TimeWaited);

	UFUNCTION()
	void OnComboWindowClosed();

	int32 ComboIndex = 0;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WindowTask;
};
