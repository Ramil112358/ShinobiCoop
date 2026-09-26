#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UShinobiGameplayAbility::UShinobiGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Shared blockers: a stunned or staggered character can't act. Subclasses add their own.
	ActivationBlockedTags.AddTag(ShinobiTags::State_Shocked);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Staggered);
}

AShinobiCharacterBase* UShinobiGameplayAbility::GetShinobiCharacter() const
{
	return Cast<AShinobiCharacterBase>(GetAvatarActorFromActorInfo());
}

UShinobiMovementComponent* UShinobiGameplayAbility::GetShinobiMovement() const
{
	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	return Character ? Cast<UShinobiMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

FActiveGameplayEffectHandle UShinobiGameplayAbility::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass) const
{
	if (!ensure(EffectClass))
	{
		return FActiveGameplayEffectHandle();
	}
	return ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, EffectClass->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());
}

bool UShinobiGameplayAbility::ApplyDamage(AShinobiCharacterBase* Target, float Amount) const
{
	// Predicted (LocalPredicted) abilities run this on the owning client too; without this guard
	// the client would apply damage to Target locally in addition to the server's authoritative copy.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}
	return ShinobiCombat::ApplyDamage(GetAbilitySystemComponentFromActorInfo(), Target, Amount);
}

TArray<AShinobiCharacterBase*> UShinobiGameplayAbility::FindHostilesInCone(float Range, float HalfAngleDegrees) const
{
	TArray<AShinobiCharacterBase*> Result;
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		return Result;
	}
	const FVector Origin = Self->GetActorLocation();
	const FVector Forward = Self->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	// ponytail: linear scan over all characters; fine for dozens, switch to an overlap query if counts grow
	for (TActorIterator<AShinobiCharacterBase> It(Self->GetWorld()); It; ++It)
	{
		AShinobiCharacterBase* Other = *It;
		if (!Self->IsHostileTo(Other))
		{
			continue;
		}
		const FVector ToOther = (Other->GetActorLocation() - Origin) * FVector(1.f, 1.f, 0.f);
		const float Distance = ToOther.Size();
		if (Distance > Range)
		{
			continue;
		}
		if (Distance > KINDA_SMALL_NUMBER && FVector::DotProduct(ToOther / Distance, Forward) < MinDot)
		{
			continue;
		}
		Result.Add(Other);
	}
	return Result;
}

AShinobiCharacterBase* UShinobiGameplayAbility::FindNearestHostileInCone(float Range, float HalfAngleDegrees) const
{
	const TArray<AShinobiCharacterBase*> Candidates = FindHostilesInCone(Range, HalfAngleDegrees);
	AShinobiCharacterBase* Nearest = nullptr;
	if (Candidates.Num() == 0)
	{
		return Nearest;
	}
	// FindHostilesInCone only returns candidates when the owner exists.
	const FVector Origin = GetShinobiCharacter()->GetActorLocation();
	float NearestDistance = TNumericLimits<float>::Max();
	for (AShinobiCharacterBase* Candidate : Candidates)
	{
		const float Distance = FVector::Dist2D(Origin, Candidate->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Candidate;
		}
	}
	return Nearest;
}
