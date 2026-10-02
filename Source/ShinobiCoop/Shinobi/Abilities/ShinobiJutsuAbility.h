#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiJutsuAbility.generated.h"

class UAbilitySystemComponent;
class UAbilityTask;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UTexture2D;

/** What the HUD shows for one jutsu slot. */
struct FShinobiJutsuSlotState
{
	float CooldownRemaining = 0.f;
	float CooldownDuration = 0.f;
	bool bAffordable = true;
};

/**
 * Base for the four jutsu. On activation: PrepareJutsu() (may fail for free), pay the chakra cost, face the
 * aim, then three phases:
 * - Seal: the hand seals (WindUp seconds), always played in full, even on a tap.
 * - Charge: only if the button is still held when the seals end. Up to MaxChargeTime; chakra drains while held
 *   (ChargeDrainFraction of the cost over a full charge). Ends on release, at the cap, or when chakra runs out.
 * - Release: the cooldown starts and ExecuteJutsu() runs ReleaseDelay later, reading GetChargeLevel().
 * A staggering hit (Event.Hit.Stagger, >= 20 damage) during Seal or Charge cancels the jutsu and refunds half
 * the cost (not the drain) without starting the cooldown. Enemies have no button to hold, so they fire
 * uncharged. Each subclass sets its numbers in its constructor (the spec's UJutsuData, kept in C++ like the M1
 * abilities).
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiJutsuAbility : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UShinobiJutsuAbility();

	static constexpr float InterruptRefundFraction = 0.5f;
	static constexpr float MaxChargeTime = 2.f;
	static constexpr float ChargeDrainFraction = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	FText DisplayName;

	/** Painted icon on the HUD slot (Scripts/setup_hud.py imports it); the slot shows DisplayName while it's missing. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Which page of the player's jutsu this sits on (keys 1-4 fire the current page's). Enemies have no input, so they ignore it. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "1", ClampMax = "5"))
	int32 Page = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ChakraCost = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Cooldown = 0.f;

	/** Length of the seal phase; a staggering hit interrupts it. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "0.01"))
	float WindUp = 0.5f;

	/** From the release to ExecuteJutsu(), so the effect lands on the release clip's key moment (e.g. the blow). */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "0"))
	float ReleaseDelay = 0.f;

	/** Phase animations (built by Scripts/setup_jutsu_anims.py). Seal is rate-fitted to WindUp; any may be unset. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	TSoftObjectPtr<UAnimMontage> SealMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	TSoftObjectPtr<UAnimMontage> ChargeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	TSoftObjectPtr<UAnimMontage> ReleaseMontage;

	/** 0-1: how much of MaxChargeTime was charged; fixed at the release. */
	float GetChargeLevel() const { return ChargeLevel; }
	float GetDamageScale() const { return 1.f + 0.75f * ChargeLevel; }
	float GetVisualScale() const { return 1.f + 0.8f * ChargeLevel; }

	/** For the HUD; safe to call on the class default object. */
	FShinobiJutsuSlotState GetSlotState(const UAbilitySystemComponent& ASC) const;

	virtual const FGameplayTagContainer* GetCooldownTags() const override { return &CooldownTags; }
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Runs before any cost is paid. Return false to fail the activation for free (the HUD flashes the slot). */
	virtual bool PrepareJutsu() { return true; }

	/** Runs after the wind-up, once the cooldown has started. Must end the ability (EndJutsu), directly or from a task. */
	virtual void ExecuteJutsu() PURE_VIRTUAL(UShinobiJutsuAbility::ExecuteJutsu, );

	void EndJutsu();

	/** Sphere-sweeps along the aim on ECC_Pawn (hits characters and world geometry), ignoring the caster.
	 * Range counts from the caster, not from the camera behind it. */
	bool TraceAim(float Range, float Radius, FHitResult& OutHit) const;

	/** Turns the caster toward Target, or to the aim's yaw when Target is null. */
	void FaceAim(const AActor* Target) const;

	/** This jutsu's Cooldown.Jutsu.* tag; set in each subclass constructor. */
	FGameplayTagContainer CooldownTags;

private:
	enum class EPhase : uint8 { Seal, Charge, Released };

	UFUNCTION()
	void OnSealsFinished();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnChargeTick(int32 ActionNumber);

	UFUNCTION()
	void OnInterruptedByHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnReleaseDelayFinished();

	void Release();

	/** Server: drains chakra for the charge time since the last drain. Returns false once chakra has run out. */
	bool DrainCharge();

	float GetChargeTime() const;
	void PlayPhaseMontage(const TSoftObjectPtr<UAnimMontage>& Montage, float Rate);
	void SetChargingTag(bool bOn);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InterruptTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask> ChargeTicker;

	/** Remote owner only: the server's "chakra ran out, release" signal. */
	FDelegateHandle ServerReleaseHandle;

	EPhase Phase = EPhase::Seal;
	bool bReleaseRequested = false;
	bool bChargingTagAdded = false;
	float ChargeStartTime = 0.f;
	float LastDrainTime = 0.f;
	float ChargeLevel = 0.f;
};
