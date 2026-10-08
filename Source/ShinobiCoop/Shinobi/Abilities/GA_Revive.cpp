#include "Shinobi/Abilities/GA_Revive.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/World/ShinobiVillager.h"

UGA_Revive::UGA_Revive()
{
	InputID = EShinobiAbilityInputID::Revive;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Revive));
	ActivationOwnedTags.AddTag(ShinobiTags::State_Reviving); // holds the reviver still (AShinobiPlayerCharacter::CanMove)
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
}

void UGA_Revive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Patient = FindDownedAlly();
	if (!Patient.IsValid())
	{
		const AActor* Self = GetAvatarActorFromActorInfo();
		if (AShinobiVillager* Villager = Self ? FindVillagerNear(*Self) : nullptr; Villager && IsLocallyControlled())
		{
			Villager->Talk();
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartTime = GetWorld()->GetTimeSeconds();

	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_Revive::OnReleased);
	Release->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* Hit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ShinobiTags::Event_Hit_Stagger, nullptr, true);
	Hit->EventReceived.AddDynamic(this, &UGA_Revive::OnStaggered);
	Hit->ReadyForActivation();

	UAbilityTask_WaitDelay* Hold = UAbilityTask_WaitDelay::WaitDelay(this, Seconds);
	Hold->OnFinish.AddDynamic(this, &UGA_Revive::OnHeldLongEnough);
	Hold->ReadyForActivation();
}

float UGA_Revive::GetProgress() const
{
	return IsActive() ? FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - StartTime) / Seconds, 0.f, 1.f) : 0.f;
}

AShinobiVillager* UGA_Revive::FindVillagerNear(const AActor& Talker)
{
	AShinobiVillager* Nearest = nullptr;
	float Best = TalkReach;
	for (TActorIterator<AShinobiVillager> It(Talker.GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), Talker.GetActorLocation());
		if (Distance <= Best)
		{
			Best = Distance;
			Nearest = *It;
		}
	}
	return Nearest;
}

AShinobiPlayerCharacter* UGA_Revive::FindDownedAlly() const
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	return Self ? FindDownedAllyFor(*Self) : nullptr;
}

AShinobiPlayerCharacter* UGA_Revive::FindDownedAllyFor(const AShinobiCharacterBase& Self)
{
	UWorld* World = Self.GetWorld();
	const AShinobiGameState* State = World ? World->GetGameState<AShinobiGameState>() : nullptr;
	if (!World || (State && State->IsPvp()))
	{
		return nullptr;
	}
	for (TActorIterator<AShinobiPlayerCharacter> It(World); It; ++It)
	{
		if (*It != &Self && !Self.IsHostileTo(*It) && It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Downed)
			&& FVector::Dist(It->GetActorLocation(), Self.GetActorLocation()) <= Reach)
		{
			return *It;
		}
	}
	return nullptr;
}

void UGA_Revive::OnReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Revive::OnStaggered(FGameplayEventData Payload)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Revive::OnHeldLongEnough()
{
	AShinobiPlayerCharacter* Target = Patient.Get();
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	// A little slack on the distance: the patient may have been knocked a bit while being revived.
	if (HasAuthority(&CurrentActivationInfo) && Target && Self && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) <= Reach * 1.5f)
	{
		Target->Revive();
	}
	// Like EndJutsu: only the server replicates the normal end, so the client's copy can't end the server's before it revives.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HasAuthority(&CurrentActivationInfo), false);
}
