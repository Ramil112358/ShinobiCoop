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
	/** Deal damage through UShinobiDamageExec, which reads SetByCaller Data.Damage. */
	void AddDamageExecution();
	/** For timed effects (SetDuration): execute every PeriodSeconds, the first time one period after application. */
	void SetPeriodic(float PeriodSeconds);
	/** One copy per target from any source: re-applying refreshes the duration instead of stacking a second copy. */
	void UseSingleRefreshingStack();

private:
	UTargetTagRequirementsGameplayEffectComponent& Requirements();

	UPROPERTY()
	TObjectPtr<UTargetTagsGameplayEffectComponent> TargetTagsComponent;

	UPROPERTY()
	TObjectPtr<UTargetTagRequirementsGameplayEffectComponent> RequirementsComponent;
};

/** Instant damage through UShinobiDamageExec (SetByCaller Data.Damage). Invulnerable targets take none; see the exec. */
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

/** Burn: 10 damage per second for 4 s, through the damage exec. Re-applying refreshes the duration. */
UCLASS()
class SHINOBICOOP_API UGE_Burn : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 10.f;
	UGE_Burn();
};

/** Amaterasu's black flame: 15 damage per second for 10 s. Firestorm never removes it. */
UCLASS()
class SHINOBICOOP_API UGE_BlackFlame : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 15.f;
	UGE_BlackFlame();
};

/** Shock stun: 1.5 s of State.Shocked (no abilities, no movement). */
UCLASS()
class SHINOBICOOP_API UGE_Shock : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Shock();
};

/** Shock bonus: the next direct hit within 10 s deals +25%; the damage exec consumes it. */
UCLASS()
class SHINOBICOOP_API UGE_ShockBonus : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ShockBonus();
};

/** Stagger: 0.5 s of State.Staggered (no abilities, no movement). Substitution staggers the attacker. */
UCLASS()
class SHINOBICOOP_API UGE_Stagger : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Stagger();
};

/** Jutsu cooldown. Duration: SetByCaller Data.Cooldown. The jutsu's Cooldown.Jutsu.* tag is added to each spec as a
 * dynamic granted tag (UShinobiJutsuAbility::ApplyCooldown), so one class serves all four jutsu. */
UCLASS()
class SHINOBICOOP_API UGE_JutsuCooldown : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_JutsuCooldown();
};
