#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_FireMeteor.generated.h"

/** Page 3, slot 3: the first press previews a ground ring under the crosshair (within 2000); the second confirms and starts
 * the cast. A flaming meteor then damages and burns every enemy in the radius and leaves a patch of ground fire. Charging
 * widens it and hits harder. With nothing to aim at (open sky) placement fails before any cost is paid. */
UCLASS()
class SHINOBICOOP_API UGA_FireMeteor : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_FireMeteor();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Radius = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 90.f;

	/** From the ring appearing to the landing: the time to run. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FallSeconds = 1.4f;

protected:
	virtual bool PrepareJutsu() override;
	virtual float GetTargetPreviewRadius() const override { return Radius; }
	virtual FLinearColor GetTargetPreviewTint() const override;
	virtual bool UpdateJutsuTarget(FVector& OutTarget) override;
	virtual void ExecuteJutsu() override;

private:
	FVector Target = FVector::ZeroVector;
};
