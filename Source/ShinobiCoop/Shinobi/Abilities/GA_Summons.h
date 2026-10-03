#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "GA_Summons.generated.h"

/** Summoning Jutsu: after the seals, the creature appears in a puff of smoke in front of the caster and fights for
 * AShinobiSummon::GetLifetime seconds. Calling the same one again replaces it. Like Susanoo, charging does nothing. */
UCLASS(Abstract)
class SHINOBICOOP_API UGA_SummonBase : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_SummonBase();

protected:
	virtual void ExecuteJutsu() override;

	EShinobiSummonKind SummonKind = EShinobiSummonKind::Toad;

	/** How far in front of the caster it appears. */
	float Distance = 400.f;
};

/** Page 4, slot 2: a giant toad, slow and tough, that slams the ground. */
UCLASS()
class SHINOBICOOP_API UGA_SummonToad : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonToad();
};

/** Page 4, slot 3: a giant snake that strikes from a distance. */
UCLASS()
class SHINOBICOOP_API UGA_SummonSnake : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonSnake();
};

/** Page 4, slot 4: a pack of four ninken. */
UCLASS()
class SHINOBICOOP_API UGA_SummonDogs : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonDogs();
};
