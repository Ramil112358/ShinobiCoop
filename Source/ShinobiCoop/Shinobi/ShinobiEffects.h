#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShinobiEffects.generated.h"

class UTargetTagsGameplayEffectComponent;
class UTargetTagRequirementsGameplayEffectComponent;

/**
 * Base for gameplay effects defined in C++.
 * The helpers create GE components as default subobjects, so call them only from constructors.
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiEffect : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	void AddModifier(const FGameplayAttribute& Attribute, float Magnitude);
	void AddSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag);
	void SetDuration(float Seconds);
	void SetInfinitePeriodic(float PeriodSeconds);
	/** Target gets this tag while the effect is active. */
	void GrantTag(const FGameplayTag& Tag);
	/** Effect is not applied at all if the target has this tag. */
	void IgnoreApplicationIfTargetHas(const FGameplayTag& Tag);
	/** Effect stays applied but does nothing while the target has this tag. */
	void InhibitWhileTargetHas(const FGameplayTag& Tag);

private:
	UTargetTagRequirementsGameplayEffectComponent& Requirements();

	UPROPERTY()
	TObjectPtr<UTargetTagsGameplayEffectComponent> TargetTagsComponent;

	UPROPERTY()
	TObjectPtr<UTargetTagRequirementsGameplayEffectComponent> RequirementsComponent;
};

/** Instant damage. Magnitude: SetByCaller Data.Damage. Ignored while the target is invulnerable. */
UCLASS()
class SHINOBICOOP_API UGE_Damage : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Damage();
};

/** Instant chakra gain. Magnitude: SetByCaller Data.ChakraGain. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraGain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraGain();
};

/** Passive chakra regeneration: +2/s. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraRegen();
};

/** Passive stamina regeneration: +20/s, paused while State.StaminaRegenBlocked. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegen();
};

/** Apply after spending stamina; each application is its own instance, so the block lasts 1 s from the latest one. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegenDelay : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegenDelay();
};

/** While sprinting: -10 stamina/s, blocks regen, grants State.Sprinting. */
UCLASS()
class SHINOBICOOP_API UGE_SprintDrain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SprintDrain();
};

/** Dodge cost: -25 stamina (used as the ability's cost, so dodge is blocked below 25). */
UCLASS()
class SHINOBICOOP_API UGE_DodgeCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeCost();
};

/** Dodge invulnerability window: 0.2 s of State.Invulnerable. */
UCLASS()
class SHINOBICOOP_API UGE_DodgeIFrames : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeIFrames();
};

/** While charging chakra: +25 chakra/s. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraCharge : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraCharge();
};
