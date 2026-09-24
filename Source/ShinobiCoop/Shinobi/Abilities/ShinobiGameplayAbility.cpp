#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UShinobiGameplayAbility::UShinobiGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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

void UShinobiGameplayAbility::ApplyDamage(AShinobiCharacterBase* Target, float Amount) const
{
	// Predicted (LocalPredicted) abilities run this on the owning client too; without this guard
	// the client would apply damage to Target locally in addition to the server's authoritative copy.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	if (!Source || !Target)
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UGE_Damage::StaticClass(), GetAbilityLevel());
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Amount);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
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
