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
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);

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
	const bool bFinisher = ComboIndex >= ComboDamage.Num() - 1;

	if (HasAuthority(&CurrentActivationInfo))
	{
		const TArray<AShinobiCharacterBase*> Targets = FindHostilesInCone(Range, HalfAngle);
		const FVector Origin = GetAvatarActorFromActorInfo()->GetActorLocation();
		for (AShinobiCharacterBase* Target : Targets)
		{
			ApplyDamage(Target, ComboDamage[ComboIndex]);
			if (bFinisher)
			{
				const FVector Push = (Target->GetActorLocation() - Origin).GetSafeNormal2D() * FinisherKnockback + FVector(0.f, 0.f, 200.f);
				Target->LaunchCharacter(Push, true, true);
			}
		}
		if (Targets.Num() > 0)
		{
			FGameplayEffectSpecHandle Gain = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
			Gain.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraPerHit * Targets.Num());
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Gain);
		}
	}

	WindowTask = UAbilityTask_WaitDelay::WaitDelay(this, ComboWindow);
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
