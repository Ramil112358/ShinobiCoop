#include "Shinobi/Abilities/GA_Sprint.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UGA_Sprint::UGA_Sprint()
{
	InputID = EShinobiAbilityInputID::Sprint;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Sprint));
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || ASC->GetNumericAttribute(UShinobiAttributeSet::GetStaminaAttribute()) <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSprintStarted = true;
	SetSprinting(true);
	if (HasAuthority(&ActivationInfo))
	{
		DrainHandle = ApplyEffectToSelf(UGE_SprintDrain::StaticClass());
	}

	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_Sprint::OnInputReleased);
	Release->ReadyForActivation();

	// Broadcasts immediately with the current comparison result, then whenever it flips.
	UAbilityTask_WaitAttributeChangeThreshold* Exhausted = UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(
		this, UShinobiAttributeSet::GetStaminaAttribute(), EWaitAttributeChangeComparison::LessThanOrEqualTo, 0.f, false);
	Exhausted->OnChange.AddDynamic(this, &UGA_Sprint::OnStaminaThreshold);
	Exhausted->ReadyForActivation();
}

void UGA_Sprint::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Sprint::OnStaminaThreshold(bool bMatchesComparison, float CurrentValue)
{
	if (bMatchesComparison)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bSprintStarted)
	{
		bSprintStarted = false;
		SetSprinting(false);
		if (HasAuthority(&ActivationInfo))
		{
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveActiveGameplayEffect(DrainHandle);
			}
			DrainHandle.Invalidate();
			ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Sprint::SetSprinting(bool bSprinting) const
{
	if (UShinobiMovementComponent* Movement = GetShinobiMovement())
	{
		Movement->SetSprinting(bSprinting);
	}
}
