#include "Shinobi/Abilities/GA_Kamui.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Kamui::UGA_Kamui()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	Page = 2;
	DisplayName = INVTEXT("Kamui");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_Kamui.T_Jutsu_Kamui")));
	ChakraCost = 30.f;
	Cooldown = 12.f;
	WindUp = 0.3f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Kamui);
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Kamui_Seal.AM_Kamui_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Kamui_Release.AM_Kamui_Release")));
}

void UGA_Kamui::ExecuteJutsu()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyEffectToSelf(UGE_Kamui::StaticClass());
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
		{
			AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::Swirl, Self->GetActorLocation());
		}
	}
	EndJutsu();
}

UGA_KamuiExit::UGA_KamuiExit()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationRequiredTags.AddTag(ShinobiTags::State_Phased);
	ActivationRequiredTags.AddTag(ShinobiTags::JutsuPage(2));
}

void UGA_KamuiExit::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (HasAuthority(&ActivationInfo))
	{
		GetAbilitySystemComponentFromActorInfo()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Phased));
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
		{
			AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::Swirl, Self->GetActorLocation());
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
