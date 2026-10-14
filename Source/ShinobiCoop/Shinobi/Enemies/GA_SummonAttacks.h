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
