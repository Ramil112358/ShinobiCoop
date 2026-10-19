#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "GA_Dragon.generated.h"

/** Page 5: summon a dragon. After the Kuchiyose seals a dragon rises under the caster, who rides it for UGE_Mounted::Seconds
 * (UShinobiMovementComponent flies them; Space climbs, Control dives; melee becomes UGA_DragonBreath). Like the other summons, charging
 * does nothing. */
UCLASS(Abstract)
class SHINOBICOOP_API UGA_SummonDragonBase : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_SummonDragonBase();

protected:
	virtual void ExecuteJutsu() override;

	EShinobiDragonKind DragonKind = EShinobiDragonKind::Tairn;
};

/** Slot 1: Tairn (Fourth Wing's black Morningstartail, blue-white lightning fire). */
UCLASS()
class SHINOBICOOP_API UGA_SummonDragon : public UGA_SummonDragonBase
{
	GENERATED_BODY()

public:
	UGA_SummonDragon();
};

/** Slot 2: the crimson dragon, orange fire. */
UCLASS()
class SHINOBICOOP_API UGA_SummonCrimsonDragon : public UGA_SummonDragonBase
{
	GENERATED_BODY()

public:
	UGA_SummonCrimsonDragon();
};

/** The summoning key again while mounted: climbs down. Fires from Summon Tairn's loadout slot, only while riding Tairn
 * (UGA_CrimsonDragonDismount is the Crimson Dragon's; UGA_DragonDismountKey works whatever the slots). Not a jutsu. */
UCLASS()
class SHINOBICOOP_API UGA_DragonDismount : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DragonDismount();

	/** Only while riding this kind, unless bAnyKind. */
	EShinobiDragonKind RequiredKind = EShinobiDragonKind::Tairn;
	bool bAnyKind = false;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Summon Crimson Dragon's slot pressed again while riding the Crimson Dragon: climbs down. */
UCLASS()
class SHINOBICOOP_API UGA_CrimsonDragonDismount : public UGA_DragonDismount
{
	GENERATED_BODY()

public:
	UGA_CrimsonDragonDismount();
};

/** The dedicated dismount key (X / D-pad down): off the dragon whatever is in the slots. */
UCLASS()
class SHINOBICOOP_API UGA_DragonDismountKey : public UGA_DragonDismount
{
	GENERATED_BODY()

public:
	UGA_DragonDismountKey();
};

/** While mounted, the swing button breathes fire from the dragon's mouth along the camera's aim: once its jaws are open, PulseCount pulses PulseGap apart, each
 * PulseDamage to every hostile in a narrow cone (3D) up to Range away; the first to hit also sets them alight. 15 chakra, 4 s cooldown. */
UCLASS()
class SHINOBICOOP_API UGA_DragonBreath : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DragonBreath();

	static constexpr float PulseDamage = 25.f;
	static constexpr int32 PulseCount = 5;
	static constexpr float PulseGap = 0.3f;
	static constexpr float Range = AShinobiDragonMount::BreathRange;
	static constexpr float HalfAngle = 20.f;
	static constexpr float CooldownSeconds = 4.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** The jaws are open (AShinobiDragonMount::BreathOpenSeconds in): the fire starts. */
	UFUNCTION()
	void Ignite();

	UFUNCTION()
	void Pulse();

	/** A staggering hit puts the fire out, like it interrupts a jutsu. */
	UFUNCTION()
	void OnStaggered(FGameplayEventData Payload);

	void Stop(bool bCancelled);

	int32 Pulses = 0;
	EShinobiDragonKind BreathKind = EShinobiDragonKind::Tairn;
	FVector LastPatchAt = FVector(TNumericLimits<float>::Max());
	TWeakObjectPtr<class AShinobiDragonFlame> Flame;
	TSet<TWeakObjectPtr<AShinobiCharacterBase>> Ignited;
};
