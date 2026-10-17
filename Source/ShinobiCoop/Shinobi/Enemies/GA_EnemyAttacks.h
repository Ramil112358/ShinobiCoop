#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_EnemyAttacks.generated.h"

class AShinobiCharacterBase;
class AShinobiEnemyCharacter;
class UAnimMontage;

/**
 * Enemy attacks, activated by AShinobiAIController on the server. Damage comes from the enemy's AttackDamage.
 * The target is the AI's focus, locked at activation: a hit on it while it's invulnerable (a well-timed dodge)
 * still connects, so the dodge triggers Substitution instead of just escaping.
 */
UCLASS(Abstract)
class SHINOBICOOP_API UGA_EnemyAttack : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EnemyAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Telegraph: seconds from activation to the first hit / throw. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float WindUp = 0.45f;

	/** Held still after the last hit before the ability ends. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float Recovery = 0.35f;

	/** Played from activation, slowed so its hit frame meets the end of WindUp. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TSoftObjectPtr<UAnimMontage> WindUpMontage;

	/** Seconds from activation to the end of the attack (a summon's attack clip is fitted to it). */
	virtual float GetTotalSeconds() const { return WindUp + Recovery; }

protected:
	/** Called WindUp after activation. Must end in Finish() (directly or after more timed steps). */
	virtual void Strike() PURE_VIRTUAL(UGA_EnemyAttack::Strike, );

	/** Plays Anim so its hit frame lands Seconds from now. */
	void PlaySwing(const TSoftObjectPtr<UAnimMontage>& Anim, float Seconds);
	void After(float Seconds, void (UGA_EnemyAttack::*Step)());
	void Finish() { After(Recovery, &UGA_EnemyAttack::EndNow); }
	void StrikeAfter(float Seconds) { After(Seconds, &UGA_EnemyAttack::Strike); }
	void EndNow();
	AShinobiEnemyCharacter* GetEnemy() const;
	bool CanReachLockedTarget(float Range) const;

	TWeakObjectPtr<AShinobiCharacterBase> LockedTarget;

	/** Strikes so far this activation. */
	int32 StrikeCount = 0;

private:
	UFUNCTION()
	void RunStep();

	void (UGA_EnemyAttack::*PendingStep)() = nullptr;
};

/** Two swings, AttackDamage each, in a cone in front. */
UCLASS()
class SHINOBICOOP_API UGA_EnemyMelee : public UGA_EnemyAttack
{
	GENERATED_BODY()

public:
	UGA_EnemyMelee();

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	int32 Hits = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float HitGap = 0.5f;

	/** Reach, scaled by the enemy's size. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float Range = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float HalfAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TSoftObjectPtr<UAnimMontage> FollowUpMontage;

protected:
	virtual void Strike() override;
};

/** One kunai (AShinobiKunai) at the target for AttackDamage. */
UCLASS()
class SHINOBICOOP_API UGA_EnemyKunai : public UGA_EnemyAttack
{
	GENERATED_BODY()

public:
	UGA_EnemyKunai();

protected:
	virtual void Strike() override;
};

/** The boss's Fireball: 90 direct damage, no chakra cost, 8 s cooldown, slower (readable) wind-up. */
UCLASS()
class SHINOBICOOP_API UGA_EnemyFireball : public UGA_Fireball
{
	GENERATED_BODY()

public:
	UGA_EnemyFireball();
};
