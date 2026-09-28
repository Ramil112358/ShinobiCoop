#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiDashJutsu.h"
#include "GA_Rasengan.generated.h"

class AShinobiCharacterBase;

/**
 * Slot 3: a spinning ball charged in the palm, then a dash of up to 900 (snapping to an enemy like Chidori) and a
 * strike: 150 damage and a heavy knockback, both scaled by the charge. On a burning target (normal or
 * black flame) it sets off Firestorm: 200 damage to every enemy within 500 of the target, and the target's
 * normal burn is consumed (black flame stays).
 */
UCLASS()
class SHINOBICOOP_API UGA_Rasengan : public UShinobiDashJutsu
{
	GENERATED_BODY()

public:
	UGA_Rasengan();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Knockback = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormDamage = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormRadius = 500.f;

protected:
	virtual void Strike(AShinobiCharacterBase* Victim) override;

private:
	void Firestorm(AShinobiCharacterBase* Center) const;
};
