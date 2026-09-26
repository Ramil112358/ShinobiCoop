#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Chidori.generated.h"

/**
 * Slot 2: a lightning dash of up to 1200 that steers to the nearest enemy within 15 degrees of the aim (straight
 * ahead otherwise), then strikes: 120 damage, a 1.5 s stun and the Shock bonus. The dash is a root-motion force
 * like the dodge's, but started from the wind-up timer on each side independently (see the plan's dash-sync note).
 */
UCLASS()
class SHINOBICOOP_API UGA_Chidori : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Chidori();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float DashDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu", meta = (ClampMin = "1"))
	float DashSpeed = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float SnapHalfAngle = 15.f;

	/** How far short of the target the dash stops. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float StopShort = 100.f;

	/** Reach of the strike at the end of the dash. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float StrikeRange = 200.f;

protected:
	virtual void ExecuteJutsu() override;

private:
	UFUNCTION()
	void OnDashFinished();

	/** Facing when the dash started; the strike uses it, since held move input turns the character mid-dash. */
	float DashYaw = 0.f;
};
