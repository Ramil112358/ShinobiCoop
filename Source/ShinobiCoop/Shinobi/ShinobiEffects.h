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
