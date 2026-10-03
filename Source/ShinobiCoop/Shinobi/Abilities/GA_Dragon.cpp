#include "Shinobi/Abilities/GA_Dragon.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_SummonDragon::UGA_SummonDragon()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	Page = 5;
	DisplayName = INVTEXT("Summon Dragon");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonDragon.T_Jutsu_SummonDragon")));
	ChakraCost = 70.f;
	Cooldown = 60.f;
	WindUp = 0.8f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonDragon);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Mounted); // one dragon at a time
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Summon_Seal.AM_Summon_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Summon_Release.AM_Summon_Release")));
}

void UGA_SummonDragon::ExecuteJutsu()
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		ApplyEffectToSelf(UGE_Mounted::StaticClass());
		AShinobiDragonMount::Spawn(Self);
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::SummonPoof, Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), 2.f);
	}
	EndJutsu();
}

UGA_DragonDismount::UGA_DragonDismount()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationRequiredTags.AddTag(ShinobiTags::State_Mounted);
	ActivationRequiredTags.AddTag(ShinobiTags::JutsuPage(5));
}

void UGA_DragonDismount::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (HasAuthority(&ActivationInfo))
	{
		GetAbilitySystemComponentFromActorInfo()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
		{
			AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::SummonPoof, Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), 2.f);
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGA_DragonBreath::UGA_DragonBreath()
{
	InputID = EShinobiAbilityInputID::Melee;
	ActivationRequiredTags.AddTag(ShinobiTags::State_Mounted);
	CostGameplayEffectClass = UGE_DragonBreathCost::StaticClass();
	CooldownGameplayEffectClass = UGE_DragonBreathCooldown::StaticClass();
}

void UGA_DragonBreath::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Pulses = 0;
	Ignited.Reset();
	Pulse();
}

void UGA_DragonBreath::Pulse()
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self || !Self->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted) || Pulses >= PulseCount)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	++Pulses;
	if (HasAuthority(&CurrentActivationInfo))
	{
		FVector Mouth = Self->GetActorLocation() + Self->GetActorForwardVector() * 400.f;
		for (TActorIterator<AShinobiDragonMount> It(Self->GetWorld()); It; ++It)
		{
			if (It->GetRider() == Self)
			{
				Mouth = It->GetMouthLocation();
			}
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		const FVector Aim = ViewRotation.Vector();
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::FireBreath, Mouth, 3.f, Aim.Rotation());

		const float Cos = FMath::Cos(FMath::DegreesToRadians(HalfAngle));
		for (AShinobiCharacterBase* Target : ShinobiCombat::FindHostilesInRadius(Self, Mouth + Aim * (Range * 0.5f), Range * 0.5f + 150.f))
		{
			const FVector ToTarget = Target->GetActorLocation() - Mouth;
			if (ToTarget.Size() > Range || FVector::DotProduct(ToTarget.GetSafeNormal(), Aim) < Cos)
			{
				continue;
			}
			if (ApplyDamage(Target, PulseDamage) && !Ignited.Contains(Target))
			{
				Ignited.Add(Target);
				ShinobiCombat::ApplyStatus(Self->GetAbilitySystemComponent(), Target, UGE_Burn::StaticClass(), UGE_Burn::DamagePerTick);
			}
		}
	}
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, PulseGap);
	Wait->OnFinish.AddDynamic(this, &UGA_DragonBreath::Pulse);
	Wait->ReadyForActivation();
}
