#include "Shinobi/Abilities/GA_Dodge.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Dodge::UGA_Dodge()
{
	InputID = EShinobiAbilityInputID::Dodge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Dodge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	CostGameplayEffectClass = UGE_DodgeCost::StaticClass();
	Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Dodge.AM_Dodge")));
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ApplyEffectToSelf(UGE_DodgeIFrames::StaticClass());
		ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
	}

	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	FVector Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector().GetSafeNormal2D(); // backstep
	}

	if (UAnimMontage* Anim = Montage.LoadSynchronous())
	{
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim);
		Play->ReadyForActivation();
	}

	UAbilityTask_ApplyRootMotionConstantForce* Push = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Direction, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	Push->OnFinish.AddDynamic(this, &UGA_Dodge::OnDodgeFinished);
	Push->ReadyForActivation();
}

void UGA_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
