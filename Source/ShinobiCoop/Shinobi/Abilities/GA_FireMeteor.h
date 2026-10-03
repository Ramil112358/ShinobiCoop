#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_FireMeteor.generated.h"

/** Page 3, slot 3: a ring marks the spot under the crosshair (within 3000), and 1.4 s later a flaming meteor lands on it:
 * damage and a burn for every enemy in the radius, and a big patch of ground fire. Charging widens it and hits harder.
 * With nothing to aim at (open sky) the cast fails before any cost is paid. */
UCLASS()
class SHINOBICOOP_API UGA_FireMeteor : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_FireMeteor();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Radius = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 90.f;

	/** From the ring appearing to the landing: the time to run. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FallSeconds = 1.4f;

protected:
	virtual bool PrepareJutsu() override;
	virtual void ExecuteJutsu() override;

private:
	FVector Target = FVector::ZeroVector;
};
