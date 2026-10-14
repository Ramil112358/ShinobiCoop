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

	/** Room it needs: a wall nearer than Distance + Clearance pulls it back, so a big creature does not appear inside one. */
	float Clearance = 250.f;
};

/** Page 4, slot 2: a giant toad, slow and tough, that slams the ground. */
UCLASS()
class SHINOBICOOP_API UGA_SummonToad : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonToad();
};

/** Page 4, slot 3: Misha, a giant plush puppy who barks a shockwave from a distance. */
UCLASS()
class SHINOBICOOP_API UGA_SummonMisha : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonMisha();
};

/** Page 4, slot 4: Pabu, a corgi ninken. */
UCLASS()
class SHINOBICOOP_API UGA_SummonPabu : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonPabu();
};
