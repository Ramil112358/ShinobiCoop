#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "GA_Dragon.generated.h"

/** Page 5, slot 1: Summon Dragon. After the Kuchiyose seals a dragon rises under the caster, who rides it for UGE_Mounted::Seconds
 * (UShinobiMovementComponent flies them; Space climbs, Control dives; melee becomes UGA_DragonBreath). Like the other summons, charging
 * does nothing. */
UCLASS()
class SHINOBICOOP_API UGA_SummonDragon : public UShinobiJutsuAbility
{
	GENERATED_BODY()

public:
	UGA_SummonDragon();

protected:
	virtual void ExecuteJutsu() override;
};

/** The same key while mounted: climbs down (page 5 only, like Kamui's exit; UGA_DragonDismountKey works on any page). Not a jutsu (no seals, cost or cooldown of its own). */
UCLASS()
class SHINOBICOOP_API UGA_DragonDismount : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DragonDismount();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The dedicated dismount key (X / D-pad down): off the dragon whatever page is showing. */
UCLASS()
class SHINOBICOOP_API UGA_DragonDismountKey : public UGA_DragonDismount
{
	GENERATED_BODY()

public:
	UGA_DragonDismountKey();
};

/** While mounted, the swing button breathes fire from the dragon's mouth along the camera's aim: PulseCount pulses PulseGap apart, each
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
	static constexpr float Range = 1500.f;
	static constexpr float HalfAngle = 20.f;
	static constexpr float CooldownSeconds = 4.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void Pulse();

	/** A staggering hit puts the fire out, like it interrupts a jutsu. */
	UFUNCTION()
	void OnStaggered(FGameplayEventData Payload);

	void Stop(bool bCancelled);

	int32 Pulses = 0;
	TWeakObjectPtr<class AShinobiDragonFlame> Flame;
	TSet<TWeakObjectPtr<AShinobiCharacterBase>> Ignited;
};
