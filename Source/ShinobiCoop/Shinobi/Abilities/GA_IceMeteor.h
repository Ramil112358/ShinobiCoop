#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_IceMeteor.generated.h"

/** Page 3, slot 4: the first press previews a ground ring under the crosshair (within 2000); the second confirms and starts
 * the cast. Over Duration seconds ShardCount ice shards fall inside it, scattered evenly and in random order. Each hurts
 * and chills the enemies it lands on. Charging widens the circle and strengthens the hits. With nothing to aim at placement
 * fails before any cost is paid. */
UCLASS()
class SHINOBICOOP_API UGA_IceMeteor : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_IceMeteor();

	UPROPERTY(EditDefaultsOnly, Category = "Jutsu")
	float Range = 2000.f;

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
	virtual float GetTargetPreviewRadius() const override { return AreaRadius; }
	virtual FLinearColor GetTargetPreviewTint() const override;
	virtual bool UpdateJutsuTarget(FVector& OutTarget) override;
	virtual void ExecuteJutsu() override;

private:
	FVector Target = FVector::ZeroVector;
};
