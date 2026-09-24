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

bool UGA_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// Gating here (rather than in ActivateAbility) means a client at 0 stamina never sends the
	// activation, and a server-only 0 (lag) is rejected via ClientActivateAbilityFailed instead of
	// charging the drain effect and stamina-regen delay for an ability that never really started.
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetStaminaAttribute()) > 0.f;
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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
			// Apply the regen delay before removing the drain: both grant State.StaminaRegenBlocked,
			// and a granted tag is a count, so applying first keeps the count >= 1 the whole time.
			// Doing it the other way briefly drops the count to 0, un-inhibiting UGE_StaminaRegen for a frame.
			ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveActiveGameplayEffect(DrainHandle);
			}
			DrainHandle.Invalidate();
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
