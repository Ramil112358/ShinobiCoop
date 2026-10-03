#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "GA_SummonAttacks.generated.h"

/** A ninken's bite: one swing of AttackDamage in front. */
UCLASS()
class SHINOBICOOP_API UGA_DogBite : public UGA_EnemyMelee
{
	GENERATED_BODY()

public:
	UGA_DogBite();
};

/** The toad rears up and slams the ground: AttackDamage to everything hostile within Radius (scaled by its size) of its feet.
 * The clip (Scripts/character/build_toad.py) lands on the ground 0.7 s in, WindUp. */
UCLASS()
class SHINOBICOOP_API UGA_ToadSlam : public UGA_EnemyAttack
{
	GENERATED_BODY()

public:
	UGA_ToadSlam();

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float Radius = 450.f;

protected:
	virtual void Strike() override;
};

/** The snake lunges: AttackDamage to its locked target, from as far as its AttackRange (and a little past, as it closes in). */
UCLASS()
class SHINOBICOOP_API UGA_SnakeStrike : public UGA_EnemyAttack
{
	GENERATED_BODY()

public:
	UGA_SnakeStrike();

protected:
	virtual void Strike() override;
};
