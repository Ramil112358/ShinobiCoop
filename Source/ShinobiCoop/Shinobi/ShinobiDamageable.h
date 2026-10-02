#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiDamageable.generated.h"

UINTERFACE(MinimalAPI)
class UShinobiDamageable : public UInterface
{
	GENERATED_BODY()
};

/** Something with no ability system that still takes hits (a jutsu wall). Projectiles call it on the server when they
 * touch an implementer on the other team. */
class SHINOBICOOP_API IShinobiDamageable
{
	GENERATED_BODY()

public:
	virtual void ReceiveShinobiDamage(float Amount, const AActor* Source) = 0;
	virtual EShinobiTeam GetDamageableTeam() const = 0;
};
