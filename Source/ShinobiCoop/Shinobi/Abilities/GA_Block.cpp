#include "Shinobi/Abilities/GA_Block.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UGA_Block::UGA_Block()
{
	InputID = EShinobiAbilityInputID::Block;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Block));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Phased);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Mounted);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	// Behind the guard: no attacking, casting, sprinting or charging. A dodge is still allowed and drops the guard (UGA_Dodge).
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
}

bool UGA_Block::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// As UGA_Sprint: gating here means a client at 0 stamina never sends the activation.
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetStaminaAttribute()) > 0.f;
}

void UGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bGuardRaised = true;
	if (UShinobiMovementComponent* Movement = GetShinobiMovement())
	{
		Movement->SetBlocking(true); // on the owning client too: the root is predicted, the guard itself is the server's
	}

	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_Block::OnInputReleased);
	Release->ReadyForActivation();

	if (!HasAuthority(&ActivationInfo))
	{
		return; // the guard itself is the server's (it replicates); the owning client only predicts the ability
	}
	GuardHandle = ApplyEffectToSelf(UGE_Blocking::StaticClass()); // no burst: the ribcage fading in is the whole effect
	// A guard break (UShinobiDamageExec) or going down takes the tag away; the ability follows. Server only: on the client
	// the tag arrives later by replication, and this task fires at once when the tag is missing.
	UAbilityTask_WaitGameplayTagRemoved* Lost = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, ShinobiTags::State_Blocking);
	Lost->Removed.AddDynamic(this, &UGA_Block::OnGuardRemoved);
	Lost->ReadyForActivation();
}

void UGA_Block::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Block::OnGuardRemoved()
{
	if (bGuardRaised) // not when EndAbility itself removed the guard
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bGuardRaised)
	{
		bGuardRaised = false;
		if (UShinobiMovementComponent* Movement = GetShinobiMovement())
		{
			Movement->SetBlocking(false);
		}
		if (HasAuthority(&ActivationInfo))
		{
			// The regen delay first, as in UGA_Sprint::EndAbility: both grant State.StaminaRegenBlocked, so the count never hits 0.
			ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveActiveGameplayEffect(GuardHandle); // already gone after a guard break; harmless
			}
			GuardHandle.Invalidate();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
