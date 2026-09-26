#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiJutsuAbility.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitGameplayEvent;

/** What the HUD shows for one jutsu slot. */
struct FShinobiJutsuSlotState
{
	float CooldownRemaining = 0.f;
	float CooldownDuration = 0.f;
	bool bAffordable = true;
};

/**
 * Base for the four jutsu. On activation: PrepareJutsu() (may fail for free), pay the chakra cost, face the
 * aim, then a hand-seal wind-up. A staggering hit (Event.Hit.Stagger, >= 20 damage) during the wind-up
 * cancels the jutsu and refunds half the cost without starting the cooldown. Otherwise the cooldown starts
 * and ExecuteJutsu() runs. Each subclass sets its numbers in its constructor (the spec's UJutsuData, kept in
 * C++ like the M1 abilities).
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiJutsuAbility : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UShinobiJutsuAbility();

	static constexpr float InterruptRefundFraction = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	FText DisplayName;

	/** Kanji shown on the HUD slot. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	FText Glyph;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ChakraCost = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Cooldown = 0.f;

	/** Hand-seal wind-up before the jutsu fires; a staggering hit interrupts it. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "0.01"))
	float WindUp = 0.5f;

	/** For the HUD; safe to call on the class default object. */
	FShinobiJutsuSlotState GetSlotState(const UAbilitySystemComponent& ASC) const;

	virtual const FGameplayTagContainer* GetCooldownTags() const override { return &CooldownTags; }
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Runs before any cost is paid. Return false to fail the activation for free (the HUD flashes the slot). */
	virtual bool PrepareJutsu() { return true; }

	/** Runs after the wind-up, once the cooldown has started. Must end the ability (EndJutsu), directly or from a task. */
	virtual void ExecuteJutsu() PURE_VIRTUAL(UShinobiJutsuAbility::ExecuteJutsu, );

	void EndJutsu();

	/** Players: the camera (the server has the client's from ServerUpdateCamera). AI and tests: the eyes. */
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Sphere-sweeps along the aim on ECC_Pawn (hits characters and world geometry), ignoring the caster.
	 * Range counts from the caster, not from the camera behind it. */
	bool TraceAim(float Range, float Radius, FHitResult& OutHit) const;

	/** Turns the caster toward Target, or to the aim's yaw when Target is null. */
	void FaceAim(const AActor* Target) const;

	/** This jutsu's Cooldown.Jutsu.* tag; set in each subclass constructor. */
	FGameplayTagContainer CooldownTags;

private:
	UFUNCTION()
	void OnWindUpFinished();

	UFUNCTION()
	void OnInterruptedByHit(FGameplayEventData Payload);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InterruptTask;
};
