#include "Shinobi/Abilities/GA_Kamui.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	/** Kamui's spiral starts at the eye: just in front of and above the head bone (head height off the capsule if there is none). */
	void SpawnEyeSpiral(const AShinobiCharacterBase& Self)
	{
		const USkeletalMeshComponent* Body = Self.GetBodyMesh();
		const FName Head = Self.GetHeadBone();
		const FVector Eye = Body && Body->GetBoneIndex(Head) != INDEX_NONE
			? Body->GetSocketLocation(Head) + Self.GetActorForwardVector() * 15.f + FVector(0.f, 0.f, 8.f)
			: Self.GetActorLocation() + Self.GetActorForwardVector() * 15.f + FVector(0.f, 0.f, 70.f);
		AShinobiBurst::Spawn(Self.GetWorld(), EShinobiBurst::Swirl, Eye);
	}
}

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
		// Phased (and untouchable) at once; the ghost look waits for the spiral (UShinobiPhaseFxComponent::GhostDelay).
		ApplyEffectToSelf(UGE_Kamui::StaticClass());
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
		{
			SpawnEyeSpiral(*Self);
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
			SpawnEyeSpiral(*Self);
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
