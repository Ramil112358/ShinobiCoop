#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_WaterWall.generated.h"

/** Page 3, slot 1: a wall of water rises 350 units ahead (250 HP, 6 s): weaker than the earth wall, cheaper and quicker to recast. */
UCLASS()
class SHINOBICOOP_API UGA_WaterWall : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_WaterWall();

protected:
	virtual void ExecuteJutsu() override;
};
