#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiDashJutsu.h"
#include "GA_Chidori.generated.h"

/**
 * Slot 2: lightning charged in the hand, then a dash of up to 1200 that steers to the nearest enemy within 15 degrees
 * of the aim (straight ahead otherwise) and strikes: 120 damage (scaled by the charge), a 1.5 s stun and the Shock
 * bonus.
 */
UCLASS()
class SHINOBICOOP_API UGA_Chidori : public UShinobiDashJutsu
{
	GENERATED_BODY()

public:
	UGA_Chidori();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 120.f;

protected:
	virtual void Strike(AShinobiCharacterBase* Victim) override;
};
