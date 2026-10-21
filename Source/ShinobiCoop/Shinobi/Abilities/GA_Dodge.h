#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Dodge.generated.h"

class UAnimMontage;
class UGameplayEffect;

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

protected:
	/** Invulnerability granted on activation. */
	TSubclassOf<UGameplayEffect> IFramesEffect;
	/** Applied on activation (predicted, so a quick second press finds it); a dodge opens the Lightning Blink window. */
	TSubclassOf<UGameplayEffect> WindowEffect;

	/** Where the dash started (the character's location on activation). */
	FVector DashStart = FVector::ZeroVector;

	UFUNCTION()
	virtual void OnDodgeFinished();
};

/**
 * Lightning Blink: a second dodge press within 1.5 s of a dodge (State.BlinkReady). The body vanishes, streaks 800
 * units in a tenth of a second and reappears; a bolt joins the two spots. Costs 35 stamina; 0.35 s invulnerable (a
 * hit in it still triggers Substitution). Fired by AShinobiPlayerCharacter::AbilityInputPressed ahead of the dodge.
 */
UCLASS()
class SHINOBICOOP_API UGA_Blink : public UGA_Dodge
{
	GENERATED_BODY()

public:
	UGA_Blink();

protected:
	virtual void OnDodgeFinished() override;
};
