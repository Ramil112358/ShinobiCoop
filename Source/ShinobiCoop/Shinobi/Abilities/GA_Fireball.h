#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Fireball.generated.h"

/** Slot 1: a fireball thrown toward the crosshair. 60 damage to the first enemy it touches (which starts
 * burning), 40 to other enemies within 250 of the impact. */
UCLASS()
class SHINOBICOOP_API UGA_Fireball : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Fireball();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float DirectDamage = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SplashDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SplashRadius = 250.f;

	/** How far the crosshair trace looks for an aim point. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRange = 5000.f;

protected:
	virtual void ExecuteJutsu() override;
};
