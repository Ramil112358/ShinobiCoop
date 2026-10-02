#include "Shinobi/Abilities/GA_Melee.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Animation/AnimMontage.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Melee::UGA_Melee()
{
	InputID = EShinobiAbilityInputID::Melee;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Melee));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Phased);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);

	ComboDamage = { 20.f, 25.f, 40.f };
	ComboMontages = {
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_1.AM_Melee_1"))),
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_2.AM_Melee_2"))),
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_3.AM_Melee_3"))),
	};
}

void UGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Players attack where the camera looks. AI turns itself.
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()); Pawn && Pawn->IsPlayerControlled())
	{
		GetAvatarActorFromActorInfo()->SetActorRotation(FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f));
	}

	ComboIndex = 0;
	StartSwing();
}

void UGA_Melee::StartSwing()
{
	if (ComboMontages.IsValidIndex(ComboIndex))
	{
		if (UAnimMontage* Anim = ComboMontages[ComboIndex].LoadSynchronous())
		{
			UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim);
			Play->ReadyForActivation();
		}
	}
	UAbilityTask_WaitDelay* HitFrame = UAbilityTask_WaitDelay::WaitDelay(this, HitDelay);
	HitFrame->OnFinish.AddDynamic(this, &UGA_Melee::OnHitFrame);
	HitFrame->ReadyForActivation();
}

void UGA_Melee::OnHitFrame()
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!ComboDamage.IsValidIndex(ComboIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const bool bFinisher = ComboIndex >= ComboDamage.Num() - 1;

	if (HasAuthority(&CurrentActivationInfo))
	{
		const TArray<AShinobiCharacterBase*> Targets = FindHostilesInCone(Range, HalfAngle);
		const FVector Origin = Self->GetActorLocation();
		int32 LandedHits = 0;
		for (AShinobiCharacterBase* Target : Targets)
		{
			if (!ApplyDamage(Target, ComboDamage[ComboIndex]))
			{
				// e.g. the target is invulnerable: no knockback, no chakra for a hit that didn't land.
				continue;
			}
			++LandedHits;
			if (bFinisher)
			{
				const FVector Push = (Target->GetActorLocation() - Origin).GetSafeNormal2D() * FinisherKnockback + FVector(0.f, 0.f, 200.f);
				Target->LaunchCharacter(Push, true, true);
			}
		}
		if (LandedHits > 0)
		{
			FGameplayEffectSpecHandle Gain = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
			Gain.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraPerHit * LandedHits);
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Gain);
		}
	}

	// This ability is InstancedPerActor, so on a listen server a remote player's copy here is the
	// server's own separate instance (HasAuthority() true, IsLocallyControlled() false), distinct from
	// the owning client's predicted instance. See RemoteWindowGrace's doc comment for why that copy's
	// timer gets extra time.
	const bool bServerCopyOfRemote = HasAuthority(&CurrentActivationInfo) && !IsLocallyControlled();
	const float Window = bServerCopyOfRemote ? ComboWindow + RemoteWindowGrace : ComboWindow;

	// WindowTask must exist before WaitInputPress is activated below: if the server already received the
	// client's press, WaitInputPress::Activate() fires OnPress synchronously (CallReplicatedEventDelegateIfSet),
	// which calls OnComboInput() and ends WindowTask -- that has to be a real task, not a stale/null one.
	WindowTask = UAbilityTask_WaitDelay::WaitDelay(this, Window);
	WindowTask->OnFinish.AddDynamic(this, &UGA_Melee::OnComboWindowClosed);
	WindowTask->ReadyForActivation();

	if (!bFinisher)
	{
		UAbilityTask_WaitInputPress* Next = UAbilityTask_WaitInputPress::WaitInputPress(this);
		Next->OnPress.AddDynamic(this, &UGA_Melee::OnComboInput);
		Next->ReadyForActivation();
	}
}

void UGA_Melee::OnComboInput(float TimeWaited)
{
	if (WindowTask)
	{
		WindowTask->EndTask();
		WindowTask = nullptr;
	}
	++ComboIndex;
	StartSwing();
}

void UGA_Melee::OnComboWindowClosed()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
