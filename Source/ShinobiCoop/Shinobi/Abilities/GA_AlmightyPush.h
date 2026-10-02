#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_AlmightyPush.generated.h"

/** Page 2, slot 3: a shockwave from the caster. Every enemy within Radius takes Damage, is staggered and thrown away
 * from the caster. Charging widens the radius and strengthens the throw. */
UCLASS()
class SHINOBICOOP_API UGA_AlmightyPush : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_AlmightyPush();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Radius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 25.f;

	/** Horizontal speed given to each enemy hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float LaunchSpeed = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float LaunchUp = 400.f;

protected:
	virtual void ExecuteJutsu() override;
};
