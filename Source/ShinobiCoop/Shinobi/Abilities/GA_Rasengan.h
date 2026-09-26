#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Rasengan.generated.h"

class AShinobiCharacterBase;

/**
 * Slot 3: a point-blank strike (reach 200): 150 damage and a heavy knockback. On a burning target (normal or
 * black flame) it sets off Firestorm: 200 damage to every enemy within 500 of the target, and the target's
 * normal burn is consumed (black flame stays).
 */
UCLASS()
class SHINOBICOOP_API UGA_Rasengan : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_Rasengan();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Damage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 200.f;

	/** Target cone at point-blank range. Wider than Chidori's 15 degrees: at 200 units that would be only +-52. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float HalfAngle = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Knockback = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormDamage = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float FirestormRadius = 500.f;

protected:
	virtual void ExecuteJutsu() override;

private:
	void Firestorm(AShinobiCharacterBase* Center) const;
};
