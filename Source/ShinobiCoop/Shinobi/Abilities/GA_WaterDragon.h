#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_WaterDragon.generated.h"

/** Page 3, slot 2: a dragon of water flies out along the aim for 1.2 s (about 2200 units), hitting every enemy it passes
 * once, and breaks on the first wall. Charging makes it bigger and harder-hitting. */
UCLASS()
class SHINOBICOOP_API UGA_WaterDragon : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_WaterDragon();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 55.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FlightSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FlightSeconds = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRange = 2200.f;

protected:
	virtual void ExecuteJutsu() override;
};
