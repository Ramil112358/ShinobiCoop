#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Amaterasu.generated.h"

class AShinobiCharacterBase;

/** Slot 4: black flame on the enemy under the crosshair (within 3000): 15 damage per second for 10 s. With no enemy
 * there, black flames ignite where the crosshair meets a surface (or the ground below that point) and burn for 5 s,
 * catching enemies who walk in. With nothing to aim at (open sky) the cast fails before any cost is paid (the HUD
 * flashes the slot). */
UCLASS()
class SHINOBICOOP_API UGA_Amaterasu : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Amaterasu();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 3000.f;

	/** Crosshair forgiveness: radius of the aim sweep. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AimRadius = 30.f;

protected:
	virtual bool PrepareJutsu() override;
	virtual void ExecuteJutsu() override;

private:
	TWeakObjectPtr<AShinobiCharacterBase> Target;

	/** Where the flames go when there's no enemy. */
	FVector AimPoint = FVector::ZeroVector;
};
