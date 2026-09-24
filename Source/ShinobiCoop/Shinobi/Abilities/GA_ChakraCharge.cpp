#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_ChakraCharge::UGA_ChakraCharge()
{
	InputID = EShinobiAbilityInputID::ChakraCharge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_ChakraCharge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
}

bool UGA_ChakraCharge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// Pressing the key at full chakra should do nothing at all: no sprint cancel, no movement lock.
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) < ASC->GetNumericAttribute(UShinobiAttributeSet::GetMaxChakraAttribute());
}

void UGA_ChakraCharge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		ChargeHandle = ApplyEffectToSelf(UGE_ChakraCharge::StaticClass());
	}
	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_ChakraCharge::OnInputReleased);
	Release->ReadyForActivation();

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// MaxChakra is read once at activation; if Max can change mid-charge later, compare against the live value.
	const float MaxChakra = ASC ? ASC->GetNumericAttribute(UShinobiAttributeSet::GetMaxChakraAttribute()) : 0.f;
	// Broadcasts immediately with the current comparison result (false: CanActivateAbility guarantees
	// Chakra < MaxChakra here), then again whenever charging pushes Chakra up to the cap.
	UAbilityTask_WaitAttributeChangeThreshold* Full = UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(
		this, UShinobiAttributeSet::GetChakraAttribute(), EWaitAttributeChangeComparison::GreaterThanOrEqualTo, MaxChakra, false);
	Full->OnChange.AddDynamic(this, &UGA_ChakraCharge::OnChakraFull);
	Full->ReadyForActivation();
}

void UGA_ChakraCharge::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ChakraCharge::OnChakraFull(bool bMatchesComparison, float CurrentValue)
{
	if (bMatchesComparison)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_ChakraCharge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ChargeHandle.IsValid() && HasAuthority(&ActivationInfo))
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffect(ChargeHandle);
		}
		ChargeHandle.Invalidate();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
