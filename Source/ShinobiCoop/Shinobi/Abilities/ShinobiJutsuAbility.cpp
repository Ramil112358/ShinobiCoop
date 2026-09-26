#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UShinobiJutsuAbility::UShinobiJutsuAbility()
{
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Jutsu));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee); // a jutsu can finish a combo
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
}

void UShinobiJutsuAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!PrepareJutsu())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->NotifyAbilityFailed(Handle, this, FGameplayTagContainer(ShinobiTags::Fail_NoTarget));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Cost now, cooldown only when the wind-up completes: an interrupted jutsu refunds half and can be recast at once.
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FaceAim(nullptr);

	if (UAnimMontage* Anim = CastMontage.LoadSynchronous())
	{
		// ponytail: capped speed-up (1-2.5x) so fast wind-ups don't turn the clip into a twitch; per-jutsu CastReleaseFraction tunes the sync
		const float Rate = FMath::Clamp(Anim->GetPlayLength() * CastReleaseFraction / WindUp, 1.f, 2.5f);
		// bStopWhenAbilityEnds = false: let the follow-through finish after the jutsu fires (the interrupt stops it).
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim, Rate, NAME_None, false);
		Play->ReadyForActivation();
	}

	// Event.Hit.Stagger is only sent on the server, so only the server's copy gets interrupted; its cancel
	// replicates to the owning client.
	InterruptTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	InterruptTask->EventReceived.AddDynamic(this, &UShinobiJutsuAbility::OnInterruptedByHit);
	InterruptTask->ReadyForActivation();

	UAbilityTask_WaitDelay* Seals = UAbilityTask_WaitDelay::WaitDelay(this, WindUp);
	Seals->OnFinish.AddDynamic(this, &UShinobiJutsuAbility::OnWindUpFinished);
	Seals->ReadyForActivation();
}

void UShinobiJutsuAbility::OnWindUpFinished()
{
	if (InterruptTask)
	{
		InterruptTask->EndTask();
		InterruptTask = nullptr;
	}
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	ExecuteJutsu();
}

void UShinobiJutsuAbility::OnInterruptedByHit(FGameplayEventData Payload)
{
	FGameplayEffectSpecHandle Refund = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
	Refund.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraCost * InterruptRefundFraction);
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Refund);
	MontageStop(0.2f); // the cast was broken: don't finish the throw
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UShinobiJutsuAbility::EndJutsu()
{
	// Only the server replicates a normal end. The owning client's wind-up starts ~one-way latency earlier, so a
	// client-sent ServerEndAbility can arrive before the server's own wind-up timer fires and kill its copy before it
	// executes (chakra spent, nothing thrown). Cancels (dodge, interrupt) still replicate from either side.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
}

bool UShinobiJutsuAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) >= ChakraCost)
	{
		return true;
	}
	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(CostTag);
	}
	return false;
}

void UShinobiJutsuAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UGE_ChakraGain::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, -ChakraCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UShinobiJutsuAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UGE_JutsuCooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Cooldown, Cooldown);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

FShinobiJutsuSlotState UShinobiJutsuAbility::GetSlotState(const UAbilitySystemComponent& ASC) const
{
	FShinobiJutsuSlotState State;
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	for (const TPair<float, float>& RemainingAndDuration : ASC.GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (RemainingAndDuration.Key > State.CooldownRemaining)
		{
			State.CooldownRemaining = RemainingAndDuration.Key;
			State.CooldownDuration = RemainingAndDuration.Value;
		}
	}
	State.bAffordable = ASC.GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) >= ChakraCost;
	return State;
}

void UShinobiJutsuAbility::GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!Pawn)
	{
		return;
	}
	if (const AController* Controller = Pawn->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	}
	else
	{
		Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
	}
}

bool UShinobiJutsuAbility::TraceAim(float Range, float Radius, FHitResult& OutHit) const
{
	const AShinobiCharacterBase* Self = Cast<AShinobiCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Self)
	{
		return false;
	}
	FVector Start;
	FRotator Rotation;
	GetAimViewPoint(Start, Rotation);
	const FVector Direction = Rotation.Vector();
	// Start level with the caster, not at the camera 450 units behind: anyone standing between them (a partner, or an
	// enemy behind you) would otherwise become the aim point or target.
	Start += Direction * FMath::Max(0.f, FVector::DotProduct(Self->GetActorLocation() - Start, Direction));
	const FVector End = Start + Direction * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiAim), false, Self);

	// A co-op partner can end up right next to the caster (e.g. sharing a spawn point) and would otherwise block this
	// sweep like a wall. Skip non-hostile characters and keep sweeping the remaining distance instead of aiming at a
	// teammate; give up after a handful of skips rather than looping forever against a degenerate stack of actors.
	FVector SegmentStart = Start;
	for (int32 Skips = 0; Skips < 8; ++Skips)
	{
		if (!Self->GetWorld()->SweepSingleByChannel(OutHit, SegmentStart, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params))
		{
			return false;
		}
		const AShinobiCharacterBase* HitCharacter = Cast<AShinobiCharacterBase>(OutHit.GetActor());
		if (!HitCharacter || Self->IsHostileTo(HitCharacter))
		{
			return true;
		}
		Params.AddIgnoredActor(HitCharacter);
		SegmentStart = OutHit.Location;
	}
	return false;
}

void UShinobiJutsuAbility::FaceAim(const AActor* Target) const
{
	AActor* Self = GetAvatarActorFromActorInfo();
	if (!Self)
	{
		return;
	}
	float Yaw = 0.f;
	if (Target)
	{
		Yaw = (Target->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw;
	}
	else
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		Yaw = ViewRotation.Yaw;
	}
	Self->SetActorRotation(FRotator(0.f, Yaw, 0.f));
}
