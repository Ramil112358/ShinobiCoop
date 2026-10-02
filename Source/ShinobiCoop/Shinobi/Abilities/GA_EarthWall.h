#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_EarthWall.generated.h"

/** Page 2, slot 4: a rock wall rises 350 units ahead (400 HP, 10 s). Enemy projectiles break on it and enemies must go around. */
UCLASS()
class SHINOBICOOP_API UGA_EarthWall : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_EarthWall();

protected:
	virtual void ExecuteJutsu() override;
};
