#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "ShinobiDashJutsu.generated.h"

class AShinobiCharacterBase;

/**
 * A jutsu that charges in the hand, then dashes in and strikes (Chidori, Rasengan). The dash goes up to DashDistance,
 * steering to the nearest enemy within SnapHalfAngle of the aim (straight ahead otherwise) and stopping StopShort of it.
 * It's a root-motion force like the dodge's, started from the release on each side independently (see the M2a plan's
 * dash-sync note). At the end, the server strikes the nearest enemy within StrikeRange.
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiDashJutsu : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
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

	/** Server only: the hit on Victim at the end of the dash. */
	virtual void Strike(AShinobiCharacterBase* Victim) PURE_VIRTUAL(UShinobiDashJutsu::Strike, );

private:
	UFUNCTION()
	void OnDashFinished();

	/** Facing when the dash started; the strike uses it, since held move input turns the character mid-dash. */
	FQuat DashRotation = FQuat::Identity; // facing when the dash started (on a wall too)
};
