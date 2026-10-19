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

	virtual bool IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const override;

protected:
	virtual void ExecuteJutsu() override;

	EShinobiSummonKind SummonKind = EShinobiSummonKind::Misha;

	/** How far in front of the caster it appears. */
	float Distance = 400.f;

	/** Room it needs: a wall nearer than Distance + Clearance pulls it back, so a big creature does not appear inside one. */
	float Clearance = 250.f;
};

/** Page 4, slot 1: Pusho, a big long-haired cat who pounces and claws. */
UCLASS()
class SHINOBICOOP_API UGA_SummonPusho : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonPusho();
};

/** Page 4, slot 2: the owl (AShinobiOwl), the plush snowy owl who hovers at the caster's shoulder and calls lightning
 * down from the sky on their foes. She is no ground summon: this calls her straight to the shoulder. */
UCLASS()
class SHINOBICOOP_API UGA_SummonOwl : public UGA_SummonBase
{
	GENERATED_BODY()

public:
	UGA_SummonOwl();
	virtual bool IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const override;

protected:
	virtual void ExecuteJutsu() override;
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
