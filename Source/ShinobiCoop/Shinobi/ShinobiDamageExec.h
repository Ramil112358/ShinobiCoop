#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ShinobiDamageExec.generated.h"

/**
 * The single place damage is computed. Reads SetByCaller Data.Damage and outputs IncomingDamage.
 * For a direct hit (not a periodic tick):
 * - target has State.Invulnerable: the hit is negated; if another character dealt it and the target has a
 *   Substitution charge, Event.Substitution is sent to the target (GA_Substitution does the rest);
 * - target has State.ShockBonus: damage x ShockBonusMultiplier and the bonus is consumed;
 * - final damage >= StaggerThreshold: Event.Hit.Stagger is sent to the target (interrupts jutsu wind-ups and
 *   chakra charge).
 * Periodic damage (Burn, Black Flame) skips all of that: it ticks through i-frames, never staggers and never
 * uses up the bonus.
 * For any damage: Susanoo and State.AttackBoost scale it first; a target with State.Shield spends its Shield attribute
 * on it before Health (the shield ends when spent), and only what gets through can stagger.
 */
UCLASS()
class SHINOBICOOP_API UShinobiDamageExec : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	static constexpr float StaggerThreshold = 20.f;
	static constexpr float ShockBonusMultiplier = 1.25f;

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
