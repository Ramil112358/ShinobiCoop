#include "Shinobi/Abilities/GA_Dragon.h"
#include "AbilitySystemComponent.h"
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
