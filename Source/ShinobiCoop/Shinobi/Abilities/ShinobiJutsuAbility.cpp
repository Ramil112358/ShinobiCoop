#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/ShinobiTelegraph.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

UShinobiJutsuAbility::UShinobiJutsuAbility()
{
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Jutsu));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Phased); // Kamui: no attacking from the other side
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee); // a jutsu can finish a combo
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
}

bool UShinobiJutsuAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// A player's key fires the current page's jutsu. Enemy specs have no input slot and no page.
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	return !Spec || Spec->InputID == INDEX_NONE || ASC->HasMatchingGameplayTag(ShinobiTags::JutsuPage(Page));
}

void UShinobiJutsuAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bTargetPlacementActive = false;
	bCostPaid = false;
	TargetPreview = nullptr;
	TargetingTicker = nullptr;
	TargetPressTask = nullptr;
	const bool bTargetedJutsu = GetTargetPreviewRadius() > 0.f;
	const bool bHasTarget = bTargetedJutsu ? UpdateJutsuTarget(TargetLocation) : PrepareJutsu();
	if (!bHasTarget)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->NotifyAbilityFailed(Handle, this, FGameplayTagContainer(ShinobiTags::Fail_NoTarget));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	InterruptTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	InterruptTask->EventReceived.AddDynamic(this, &UShinobiJutsuAbility::OnInterruptedByHit);
	InterruptTask->ReadyForActivation();

	if (bTargetedJutsu)
	{
		bTargetPlacementActive = true;
		if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(GetShinobiCharacter()))
		{
			Player->BeginJutsuTargetPlacement();
		}
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter(); Self
			&& (Self->IsLocallyControlled() || (!Self->GetController() && Self->GetNetMode() == NM_Standalone)))
		{
			TargetPreview = AShinobiTelegraph::SpawnPreview(Self->GetWorld(), TargetLocation, GetTargetPreviewRadius(), GetTargetPreviewTint());
		}

		UAbilityTask_Repeat* Ticker = UAbilityTask_Repeat::RepeatAction(this, 0.05f, MAX_int32);
		Ticker->OnPerformAction.AddDynamic(this, &UShinobiJutsuAbility::OnTargetingTick);
		TargetingTicker = Ticker;
		Ticker->ReadyForActivation();

		UAbilityTask_WaitInputPress* WaitPress = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
		WaitPress->OnPress.AddDynamic(this, &UShinobiJutsuAbility::OnTargetConfirmed);
		TargetPressTask = WaitPress;
		WaitPress->ReadyForActivation();
		return;
	}

	StartJutsuCast();
}

void UShinobiJutsuAbility::StartJutsuCast()
{
	// Cost now, cooldown only when the wind-up completes: an interrupted jutsu refunds half and can be recast at once.
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	bCostPaid = true;
	FaceAim(nullptr);

	// The ability object is instanced per actor and reused: reset the previous cast's state.
	Phase = EPhase::Seal;
	ChargeLevel = 0.f;
	ChargeTicker = nullptr;
	// Enemies (no input slot) have nothing to hold: they fire uncharged.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	bReleaseRequested = !Spec || Spec->InputID == INDEX_NONE;

	if (const UAnimMontage* Anim = SealMontage.LoadSynchronous())
	{
		// ponytail: the whole seal clip is fitted to WindUp, capped at 0.5-2.5x so it doesn't crawl or twitch
		PlayPhaseMontage(SealMontage, FMath::Clamp(Anim->GetPlayLength() / WindUp, 0.5f, 2.5f));
	}

	if (!bReleaseRequested)
	{
		// bTestAlreadyReleased: a tap's release can arrive before this task exists.
		UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		WaitRelease->OnRelease.AddDynamic(this, &UShinobiJutsuAbility::OnInputReleased);
		WaitRelease->ReadyForActivation();
	}

	UAbilityTask_WaitDelay* Seals = UAbilityTask_WaitDelay::WaitDelay(this, WindUp);
	Seals->OnFinish.AddDynamic(this, &UShinobiJutsuAbility::OnSealsFinished);
	Seals->ReadyForActivation();
}

void UShinobiJutsuAbility::OnTargetConfirmed(float TimeWaited)
{
	if (!bTargetPlacementActive)
	{
		return;
	}
	if (!UpdateTargetPreview())
	{
		CancelTargetPlacement();
		return;
	}
	ClearTargetPreview();
	StartJutsuCast();
}

void UShinobiJutsuAbility::OnTargetingTick(int32 ActionNumber)
{
	if (bTargetPlacementActive)
	{
		UpdateTargetPreview();
	}
}

bool UShinobiJutsuAbility::UpdateTargetPreview()
{
	const bool bHasTarget = UpdateJutsuTarget(TargetLocation);
	if (TargetPreview)
	{
		TargetPreview->SetPreviewTarget(TargetLocation, bHasTarget);
	}
	return bHasTarget;
}

void UShinobiJutsuAbility::ClearTargetPreview()
{
	bTargetPlacementActive = false;
	if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(GetShinobiCharacter()))
	{
		Player->EndJutsuTargetPlacement();
	}
	if (TargetingTicker)
	{
		TargetingTicker->EndTask();
		TargetingTicker = nullptr;
	}
	if (TargetPreview)
	{
		TargetPreview->Destroy();
		TargetPreview = nullptr;
	}
}

bool UShinobiJutsuAbility::CancelTargetPlacement()
{
	if (!bTargetPlacementActive)
	{
		return false;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	return true;
}

void UShinobiJutsuAbility::OnSealsFinished()
{
	if (bReleaseRequested)
	{
		Release();
		return;
	}
	Phase = EPhase::Charge;
	ChargeStartTime = LastDrainTime = GetWorld()->GetTimeSeconds();
	SetChargingTag(true);
	PlayPhaseMontage(ChargeMontage, 1.f);
	if (!HasAuthority(&CurrentActivationInfo))
	{
		// Running out of chakra is decided on the server, which drains it; it tells the owning client to release.
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const FPredictionKey Key = CurrentActivationInfo.GetActivationPredictionKey();
		ServerReleaseHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericSignalFromServer, CurrentSpecHandle, Key)
			.AddUObject(this, &UShinobiJutsuAbility::Release);
		ASC->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::GenericSignalFromServer, CurrentSpecHandle, Key);
	}
	UAbilityTask_Repeat* Ticker = UAbilityTask_Repeat::RepeatAction(this, 0.1f, MAX_int32);
	Ticker->OnPerformAction.AddDynamic(this, &UShinobiJutsuAbility::OnChargeTick);
	ChargeTicker = Ticker;
	Ticker->ReadyForActivation();
}

void UShinobiJutsuAbility::OnInputReleased(float TimeHeld)
{
	bReleaseRequested = true;
	if (Phase == EPhase::Charge)
	{
		Release();
	}
}

void UShinobiJutsuAbility::OnChargeTick(int32 ActionNumber)
{
	// Both sides stop at the cap on their own clocks. Only the server drains; when chakra runs out it releases and
	// signals a remote owner to release too (polling the replicated chakra would miss it: regen refills it at once).
	if (HasAuthority(&CurrentActivationInfo) && !DrainCharge())
	{
		if (!IsLocallyControlled())
		{
			GetAbilitySystemComponentFromActorInfo()->ClientSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromServer, CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}
		Release();
	}
	else if (GetChargeTime() >= MaxChargeTime)
	{
		Release();
	}
}

bool UShinobiJutsuAbility::DrainCharge()
{
	const float Now = FMath::Min(GetWorld()->GetTimeSeconds(), ChargeStartTime + MaxChargeTime);
	const float Chakra = GetAbilitySystemComponentFromActorInfo()->GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute());
	const float Amount = FMath::Min(ChakraCost * ChargeDrainFraction / MaxChargeTime * FMath::Max(0.f, Now - LastDrainTime), Chakra);
	LastDrainTime = Now;
	if (Amount > 0.f)
	{
		FGameplayEffectSpecHandle Drain = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
		Drain.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, -Amount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Drain);
	}
	return Chakra - Amount > KINDA_SMALL_NUMBER;
}

float UShinobiJutsuAbility::GetChargeTime() const
{
	return Phase == EPhase::Charge ? GetWorld()->GetTimeSeconds() - ChargeStartTime : 0.f;
}

void UShinobiJutsuAbility::Release()
{
	if (Phase == EPhase::Released)
	{
		return;
	}
	if (Phase == EPhase::Charge)
	{
		if (HasAuthority(&CurrentActivationInfo))
		{
			DrainCharge(); // the part since the last tick
		}
		ChargeLevel = FMath::Clamp(GetChargeTime() / MaxChargeTime, 0.f, 1.f);
		SetChargingTag(false);
		if (ChargeTicker)
		{
			ChargeTicker->EndTask();
			ChargeTicker = nullptr;
		}
	}
	Phase = EPhase::Released;
	if (InterruptTask)
	{
		InterruptTask->EndTask();
		InterruptTask = nullptr;
	}
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	if (!ReleaseMontage.IsNull())
	{
		PlayPhaseMontage(ReleaseMontage, 1.f);
	}
	else if (!ChargeMontage.IsNull())
	{
		MontageStop(0.2f); // no release clip: don't keep holding the charge pose
	}
	if (ReleaseDelay > 0.f)
	{
		UAbilityTask_WaitDelay* Delay = UAbilityTask_WaitDelay::WaitDelay(this, ReleaseDelay);
		Delay->OnFinish.AddDynamic(this, &UShinobiJutsuAbility::OnReleaseDelayFinished);
		Delay->ReadyForActivation();
	}
	else
	{
		ExecuteJutsu();
	}
}

void UShinobiJutsuAbility::OnReleaseDelayFinished()
{
	ExecuteJutsu();
}

void UShinobiJutsuAbility::PlayPhaseMontage(const TSoftObjectPtr<UAnimMontage>& Montage, float Rate)
{
	if (UAnimMontage* Anim = Montage.LoadSynchronous())
	{
		// bStopWhenAbilityEnds = false: let the follow-through finish after the jutsu fires (the interrupt stops it).
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim, Rate, NAME_None, false);
		Play->ReadyForActivation();
	}
}

void UShinobiJutsuAbility::SetChargingTag(bool bOn)
{
	if (bOn == bChargingTagAdded)
	{
		return;
	}
	bChargingTagAdded = bOn;
	// Same as GAS does for ActivationOwnedTags: every machine running the ability adds it, and the replicated count
	// reaches the other clients.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const EGameplayTagReplicationState Rep = UAbilitySystemGlobals::Get().ShouldReplicateActivationOwnedTags() ? EGameplayTagReplicationState::CountToOwner : EGameplayTagReplicationState::None;
	if (bOn)
	{
		ASC->AddLooseGameplayTag(ShinobiTags::State_Casting_Charging, 1, Rep);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(ShinobiTags::State_Casting_Charging, 1, Rep);
	}
}

void UShinobiJutsuAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearTargetPreview();
	if (IsActive())
	{
		SetChargingTag(false); // cancelled mid-charge (interrupt, dodge)
		if (ServerReleaseHandle.IsValid())
		{
			GetAbilitySystemComponentFromActorInfo()->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericSignalFromServer, CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
				.Remove(ServerReleaseHandle);
			ServerReleaseHandle.Reset();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UShinobiJutsuAbility::OnInterruptedByHit(FGameplayEventData Payload)
{
	if (bCostPaid)
	{
		FGameplayEffectSpecHandle Refund = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
		Refund.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraCost * InterruptRefundFraction);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Refund);
	}
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
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Self);
	const FVector Direction = Player && Player->IsJutsuTargetPlacementActive()
		? Player->GetJutsuTargetAimDirection()
		: Rotation.Vector();
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

bool UShinobiJutsuAbility::FindAimGround(float Range, float AimRadius, FVector& OutPoint) const
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiAimGround), false, Self);
	if (TraceAim(Range, AimRadius, Hit))
	{
		// The crosshair is on a wall or a character, at chest height: land on the floor below that spot (a little off the
		// surface, so the trace does not start inside the wall).
		FHitResult Floor;
		const FVector Above = Hit.ImpactPoint + Hit.ImpactNormal * 10.f + FVector(0.f, 0.f, 50.f);
		OutPoint = Self->GetWorld()->LineTraceSingleByChannel(Floor, Above, Above - FVector(0.f, 0.f, 600.f), ECC_WorldStatic, Params) ? Floor.ImpactPoint : Hit.ImpactPoint;
		return true;
	}
	// Aimed above everything: the ground below the end of the aim, if there is ground within reach.
	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Self);
	const FVector AimDirection = Player && Player->IsJutsuTargetPlacementActive()
		? Player->GetJutsuTargetAimDirection()
		: ViewRotation.Vector();
	const FVector End = Self->GetActorLocation() + AimDirection * Range;
	if (Self->GetWorld()->LineTraceSingleByChannel(Hit, End, End - FVector(0.f, 0.f, Range), ECC_WorldStatic, Params))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
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
	if (Target)
	{
		UShinobiMovementComponent::FaceDirection(Self, Target->GetActorLocation() - Self->GetActorLocation());
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	UShinobiMovementComponent::FaceDirection(Self, ViewRotation.Vector());
}
