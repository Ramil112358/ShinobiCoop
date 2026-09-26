#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_ChakraCharge::UGA_ChakraCharge()
{
	InputID = EShinobiAbilityInputID::ChakraCharge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_ChakraCharge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_ChakraCharge.AM_ChakraCharge")));
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
		// At full chakra the +6.25 ticks are clamped away; the effect still runs so charging looks the same.
		ChargeHandle = ApplyEffectToSelf(UGE_ChakraCharge::StaticClass());
	}
	if (UAnimMontage* Anim = Montage.LoadSynchronous())
	{
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim);
		// Anything else taking over the slot (e.g. a hit reaction) would leave the character charging in the idle
		// pose, movement-locked. OnCancelled is deliberately not bound: it also fires when the montage can't play
		// at all (no anim instance), which must not stop the charge itself.
		Play->OnInterrupted.AddDynamic(this, &UGA_ChakraCharge::OnChargeBroken);
		Play->ReadyForActivation();
		// Loop the montage's only section (AnimMontageFactory always names it "Default") while C is held;
		// PlayMontageAndWait's Activate() -> ASC->PlayMontage() runs synchronously above, so the ASC's
		// current montage should already be ours by the time this call runs. Guard on GetCurrentMontage()
		// still being Anim: if PlayMontage silently failed (e.g. no AnimInstance) the ASC's current montage
		// would be whatever was already playing (e.g. AM_Dodge), and looping *that* forever would be wrong.
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC && ASC->GetCurrentMontage() == Anim)
		{
			static const FName LoopSection(TEXT("Default"));
			ASC->CurrentMontageSetNextSectionName(LoopSection, LoopSection);
		}
	}
	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_ChakraCharge::OnInputReleased);
	Release->ReadyForActivation();

	// Sent only on the server by the damage exec (>= 20 damage); the end replicates to the owner.
	UAbilityTask_WaitGameplayEvent* Hit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	Hit->EventReceived.AddDynamic(this, &UGA_ChakraCharge::OnStaggered);
	Hit->ReadyForActivation();
}

void UGA_ChakraCharge::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ChakraCharge::OnChargeBroken()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_ChakraCharge::OnStaggered(FGameplayEventData Payload)
{
	OnChargeBroken();
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
