#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "GA_SummonAttacks.generated.h"

/** Pabu's bite: one swing of AttackDamage in front. */
UCLASS()
class SHINOBICOOP_API UGA_PabuBite : public UGA_EnemyMelee
{
	GENERATED_BODY()

public:
	UGA_PabuBite();
};

/** Pusho pounces: crouches for WindUp, leaps onto his locked target (landing StopShort of it, LeapSeconds later), and claws: one
 * swing of AttackDamage in front where he lands. His AttackRange is how far off he pounces from. The clip
 * (Scripts/character/build_pusho.py) spans the crouch, the leap and Recovery. */
UCLASS()
class SHINOBICOOP_API UGA_PushoPounce : public UGA_EnemyMelee
{
	GENERATED_BODY()

public:
	UGA_PushoPounce();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual float GetTotalSeconds() const override { return WindUp + LeapSeconds + Recovery; }

	/** Seconds in the air; it also sets how high the arc goes (about 44 cm at 0.6 s). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float LeapSeconds = 0.6f;

	/** Gap left between his capsule and the target's when he lands. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float StopShort = 30.f;

protected:
	/** First the leap, then (LeapSeconds later) the claw. */
	virtual void Strike() override;

private:
	bool bLeapt = false;
};

/** Misha barks: AttackDamage to everything hostile in a cone in front, out to Range, each staggered and thrown back. His locked
 * target is hit too while it is within Range (as it closes in, a little off the cone). The clip (Scripts/character/build_misha.py)
 * barks 0.5 s in, WindUp. */
UCLASS()
class SHINOBICOOP_API UGA_MishaBark : public UGA_EnemyAttack
{
	GENERATED_BODY()

public:
	UGA_MishaBark();

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float Range = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float HalfAngle = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float LaunchSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float LaunchUp = 350.f;

protected:
	virtual void Strike() override;
};
