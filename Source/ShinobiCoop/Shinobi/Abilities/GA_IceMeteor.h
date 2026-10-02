#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_IceMeteor.generated.h"

/** Page 3, slot 4: a ring marks the area under the crosshair (within 3000) and over Duration seconds ShardCount ice shards
 * fall inside it, scattered evenly and in random order. Each hurts and chills the enemies it lands on. Charging widens the
 * circle and strengthens the hits. With nothing to aim at the cast fails before any cost is paid. */
UCLASS()
class SHINOBICOOP_API UGA_IceMeteor : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_IceMeteor();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float AreaRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	int32 ShardCount = 14;

	/** The shards' landings are spread over this long. */
	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Duration = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ShardFall = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ShardDamage = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float ShardRadius = 140.f;

protected:
	virtual bool PrepareJutsu() override;
	virtual void ExecuteJutsu() override;

private:
	FVector Target = FVector::ZeroVector;
};
