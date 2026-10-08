#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShinobiEffects.generated.h"

class UTargetTagsGameplayEffectComponent;
class UTargetTagRequirementsGameplayEffectComponent;

/**
 * Base for gameplay effects defined in C++.
 * The helpers create GE components as default subobjects, so call them only from constructors.
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiEffect : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	void AddModifier(const FGameplayAttribute& Attribute, float Magnitude);
	void AddSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag);
	void SetDuration(float Seconds);
	void SetInfinitePeriodic(float PeriodSeconds);
	/** Target gets this tag while the effect is active. */
	void GrantTag(const FGameplayTag& Tag);
	/** Effect is not applied at all if the target has this tag. */
	void IgnoreApplicationIfTargetHas(const FGameplayTag& Tag);
	/** Effect stays applied but does nothing while the target has this tag. */
	void InhibitWhileTargetHas(const FGameplayTag& Tag);
	/** Deal damage through UShinobiDamageExec, which reads SetByCaller Data.Damage. */
	void AddDamageExecution();
	/** For timed effects (SetDuration): execute every PeriodSeconds, the first time one period after application. */
	void SetPeriodic(float PeriodSeconds);
	/** One copy per target from any source: re-applying refreshes the duration instead of stacking a second copy. */
	void UseSingleRefreshingStack();

private:
	UTargetTagRequirementsGameplayEffectComponent& Requirements();

	UPROPERTY()
	TObjectPtr<UTargetTagsGameplayEffectComponent> TargetTagsComponent;

	UPROPERTY()
	TObjectPtr<UTargetTagRequirementsGameplayEffectComponent> RequirementsComponent;
};

/** Instant damage through UShinobiDamageExec (SetByCaller Data.Damage). Invulnerable targets take none; see the exec. */
UCLASS()
class SHINOBICOOP_API UGE_Damage : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Damage();
};

/** Instant chakra gain. Magnitude: SetByCaller Data.ChakraGain. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraGain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraGain();
};

/** Passive chakra regeneration: +2/s. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraRegen();
};

/** Passive stamina regeneration: +20/s, paused while State.StaminaRegenBlocked. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegen();
};

/** Apply after spending stamina; each application is its own instance, so the block lasts 1 s from the latest one. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegenDelay : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegenDelay();
};

/** While sprinting: -10 stamina/s, blocks regen, grants State.Sprinting. */
UCLASS()
class SHINOBICOOP_API UGE_SprintDrain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SprintDrain();
};

/** Dodge cost: -25 stamina (used as the ability's cost, so dodge is blocked below 25). */
UCLASS()
class SHINOBICOOP_API UGE_DodgeCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeCost();
};

/** Dodge invulnerability window: 0.2 s of State.Invulnerable. */
UCLASS()
class SHINOBICOOP_API UGE_DodgeIFrames : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeIFrames();
};

/** While charging chakra: +25 chakra/s. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraCharge : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraCharge();
};

/** Burn: 10 damage per second for 4 s, through the damage exec. Re-applying refreshes the duration. */
UCLASS()
class SHINOBICOOP_API UGE_Burn : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 10.f;
	UGE_Burn();
};

/** Amaterasu's black flame: 15 damage per second for 10 s. Firestorm never removes it. */
UCLASS()
class SHINOBICOOP_API UGE_BlackFlame : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float DamagePerTick = 15.f;
	UGE_BlackFlame();
};

/** Shock stun: 1.5 s of State.Shocked (no abilities, no movement). */
UCLASS()
class SHINOBICOOP_API UGE_Shock : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 1.5f; // scaled per target by ShinobiCombat::ApplyStun
	UGE_Shock();
};

/** Shock bonus: the next direct hit within 10 s deals +25%; the damage exec consumes it. */
UCLASS()
class SHINOBICOOP_API UGE_ShockBonus : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ShockBonus();
};

/** Stagger: 0.5 s of State.Staggered (no abilities, no movement). Substitution staggers the attacker. */
UCLASS()
class SHINOBICOOP_API UGE_Stagger : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Stagger();
};

/** Kamui: 3 s of State.Phased. Hits (and burning) pass through the target, it walks through pawns and the AI loses it. */
UCLASS()
class SHINOBICOOP_API UGE_Kamui : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 3.f;
	UGE_Kamui();
};

/** Susanoo: 20 s of State.Susanoo. UShinobiDamageExec halves the damage its owner takes and raises the damage it deals by
 * 30%; chakra comes back 4 a second faster (a periodic modifier on top of the base regen). Recasting refreshes it. */
UCLASS()
class SHINOBICOOP_API UGE_Susanoo : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 20.f;
	static constexpr float DamageTakenScale = 0.5f; // read by UShinobiDamageExec
	static constexpr float DamageDealtScale = 1.3f;
	UGE_Susanoo();
};

/** Riding the summoned dragon: 45 s of State.Mounted. UShinobiMovementComponent flies the rider while it lasts. Ends early when the
 * rider is downed or dies (AShinobiCharacterBase) or dismounts (UGA_DragonDismount). Recasting refreshes it. */
UCLASS()
class SHINOBICOOP_API UGE_Mounted : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 45.f;
	UGE_Mounted();
};

/** The dragon's breath: 15 chakra, then 4 s before the next. */
UCLASS()
class SHINOBICOOP_API UGE_DragonBreathCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DragonBreathCost();
};

UCLASS()
class SHINOBICOOP_API UGE_DragonBreathCooldown : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 4.f;
	UGE_DragonBreathCooldown();
};

/** Chill (Ice Meteor Shower): 3 s of State.Chilled, 40% slower movement. Another hit refreshes it. */
UCLASS()
class SHINOBICOOP_API UGE_Chill : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 3.f;
	static constexpr float SpeedScale = 0.6f; // read by UShinobiMovementComponent::GetMaxSpeed
	UGE_Chill();
};

/** Teleport's jump window: 6 s of State.TeleportMarked, while the thrown kunai stand. */
UCLASS()
class SHINOBICOOP_API UGE_TeleportMarked : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 6.f;
	UGE_TeleportMarked();
};

/** A PvP round's countdown: State.PvpHold (no moving, no abilities) a little past it; AShinobiPvpGameMode removes it at the
 * fight, the margin only covers a late tick. */
UCLASS()
class SHINOBICOOP_API UGE_PvpHold : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 3.5f; // FShinobiPvpMatch::CountdownSeconds + 0.5 (asserted in the .cpp)
	UGE_PvpHold();
};

/** Attack pickup: 15 s of State.AttackBoost; UShinobiDamageExec makes the holder's damage x1.5. Another pickup refreshes it. */
UCLASS()
class SHINOBICOOP_API UGE_AttackBoost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 15.f;
	static constexpr float DamageDealtScale = 1.5f; // read by UShinobiDamageExec
	UGE_AttackBoost();
};

/** Speed pickup: 12 s of State.SpeedBoost, 35% faster movement. Another pickup refreshes it. */
UCLASS()
class SHINOBICOOP_API UGE_SpeedBoost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 12.f;
	static constexpr float SpeedScale = 1.35f; // read by UShinobiMovementComponent::GetMaxSpeed
	UGE_SpeedBoost();
};

/** Shield pickup: 10 s of State.Shield. AShinobiPickup::Apply sets the Shield attribute to Absorb; UShinobiDamageExec spends it
 * before health and removes this effect when it runs out. Another pickup refills it and refreshes the time. */
UCLASS()
class SHINOBICOOP_API UGE_Shield : public UShinobiEffect
{
	GENERATED_BODY()
public:
	static constexpr float Seconds = 10.f;
	static constexpr float Absorb = 60.f;
	UGE_Shield();
};

/** Jutsu cooldown. Duration: SetByCaller Data.Cooldown. The jutsu's Cooldown.Jutsu.* tag is added to each spec as a
 * dynamic granted tag (UShinobiJutsuAbility::ApplyCooldown), so one class serves all four jutsu. */
UCLASS()
class SHINOBICOOP_API UGE_JutsuCooldown : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_JutsuCooldown();
};

/** Substitution cost: -1 charge (used as the ability's cost, so it can't fire at 0). */
UCLASS()
class SHINOBICOOP_API UGE_SubstitutionCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SubstitutionCost();
};

/** +1 Substitution charge every 15 s. Applied at spawn to players. */
UCLASS()
class SHINOBICOOP_API UGE_SubstitutionRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SubstitutionRegen();
};

/** One Substitution per dodge: 0.5 s of Cooldown.Substitution. The dodge's i-frames outlive the cancelled dodge, so
 * without it a second hit in the same window would spend another charge and teleport again. */
UCLASS()
class SHINOBICOOP_API UGE_SubstitutionCooldown : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SubstitutionCooldown();
};

/** Downed (players at 0 HP): State.Downed until revived (which removes it) or it expires: bled out, dead. */
UCLASS()
class SHINOBICOOP_API UGE_Downed : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Downed();
	static constexpr float BleedOutSeconds = 30.f;
};

/** Dead: State.Dead (enemies at 0 HP; players after bleeding out, until the next wave respawns them). */
UCLASS()
class SHINOBICOOP_API UGE_Dead : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Dead();
};